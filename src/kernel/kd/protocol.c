/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/hal.h>
#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ki.h>
#include <kernel/mm.h>
#include <os/intrin.h>
#include <rt/except.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern bool KdpDebugConnected;
extern uint32_t KdpDebugTransport;
extern uint32_t KdpDebugDisconnectPolicy;
extern uint32_t KdpDebugDisconnectTimeoutMilliseconds;
extern uint8_t KdpDebuggerHardwareAddress[6];
extern uint8_t KdpDebuggerProtocolAddress[4];
extern uint16_t KdpDebuggerPort;
extern uint16_t KdpDebuggeePort;

static uint64_t SessionId;
static uint64_t ClientNonce;
static uint64_t ReceiveSequence;
static uint64_t SendSequence;
static uint8_t LastRequest[KDP_PROTOCOL_MAX_DATAGRAM];
static size_t LastRequestSize;
static uint8_t LastResponse[KDP_PROTOCOL_MAX_DATAGRAM];
static size_t LastResponseSize;
static bool CacheResponse;
static uint64_t LastActivityTicks;

static const uint64_t Capabilities =
    KDP_PROTOCOL_CAP_OUTPUT | KDP_PROTOCOL_CAP_READ_PHYSICAL | KDP_PROTOCOL_CAP_READ_VIRTUAL |
    KDP_PROTOCOL_CAP_READ_PORT | KDP_PROTOCOL_CAP_STOP | KDP_PROTOCOL_CAP_CONTEXT |
    KDP_PROTOCOL_CAP_CONTINUE | KDP_PROTOCOL_CAP_STEP | KDP_PROTOCOL_CAP_SOFTWARE_BREAKPOINT |
    KDP_PROTOCOL_CAP_SMP_CONTEXT | KDP_PROTOCOL_CAP_RECONNECT;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enforces the configured heartbeat timeout from the runtime poll worker.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpCheckHeartbeat(void) {
    if (!KdpDebugConnected || !KdpDebugDisconnectTimeoutMilliseconds) {
        return;
    }
    if (!LastActivityTicks) {
        LastActivityTicks = HalGetTimerTicks();
        return;
    }
    uint64_t TimeoutTicks =
        (__uint128_t)KdpDebugDisconnectTimeoutMilliseconds * HalGetTimerFrequency() / 1000;
    if (HalGetTimerTicks() - LastActivityTicks < TimeoutTicks) {
        return;
    }

    KdpDebugConnected = false;
    SessionId = 0;
    LastActivityTicks = 0;
    bool Continue = KdpDebugDisconnectPolicy == KI_LOADER_DEBUG_DISCONNECT_CONTINUE;
    if (KdpGetTargetState() == KDP_TARGET_STOPPED) {
        KdpHandleStoppedDisconnect(Continue);
    } else {
        KdpRequestDisconnect(Continue);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends a complete protocol datagram through the active transport.
 *
 * PARAMETERS:
 *     Buffer - Complete protocol datagram.
 *     Size - Datagram size.
 *
 * RETURN VALUE:
 *     true if the transport accepted the datagram, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool SendDatagram(const void *Buffer, size_t Size) {
    if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_PC16550_PIO) {
        return KdpSendSerialDatagram(Buffer, Size);
    }
    if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_KDNET) {
        return !KdpSendUdpPacket(
            KdpDebuggerHardwareAddress,
            KdpDebuggerProtocolAddress,
            KdpDebuggeePort,
            KdpDebuggerPort,
            (void *)Buffer,
            Size);
    }
    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function builds and sends one protocol packet.
 *
 * PARAMETERS:
 *     Type - Protocol message type.
 *     Flags - Request/response/event flags.
 *     Status - Protocol status for responses.
 *     RequestId - Correlated request identifier, or zero for events.
 *     Payload - Optional packet payload.
 *     PayloadSize - Payload size.
 *
 * RETURN VALUE:
 *     true if the packet was accepted by the transport, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool KdpSendProtocolPacket(
    uint16_t Type,
    uint16_t Flags,
    uint32_t Status,
    uint64_t RequestId,
    const void *Payload,
    size_t PayloadSize) {
    if (PayloadSize > KDP_PROTOCOL_MAX_PAYLOAD || (PayloadSize && !Payload)) {
        return false;
    }

    uint8_t Datagram[KDP_PROTOCOL_MAX_DATAGRAM];
    KdpProtocolHeader *Header = (KdpProtocolHeader *)Datagram;
    memcpy(Header->Magic, KDP_PROTOCOL_MAGIC, sizeof(Header->Magic));
    Header->Major = KDP_PROTOCOL_MAJOR;
    Header->Minor = KDP_PROTOCOL_MINOR;
    Header->HeaderSize = sizeof(*Header);
    Header->Type = Type;
    Header->Flags = Flags;
    Header->TotalSize = sizeof(*Header) + PayloadSize;
    Header->Status = Status;
    Header->Reserved = 0;
    Header->SessionId = SessionId;
    Header->Sequence = ++SendSequence;
    Header->RequestId = RequestId;
    if (PayloadSize) {
        memcpy(Header + 1, Payload, PayloadSize);
    }

    if (CacheResponse && (Flags & KDP_PROTOCOL_FLAG_RESPONSE)) {
        memcpy(LastResponse, Datagram, Header->TotalSize);
        LastResponseSize = Header->TotalSize;
        CacheResponse = false;
    }
    return SendDatagram(Datagram, Header->TotalSize);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns a correlated status-only response.
 *
 * PARAMETERS:
 *     Header - Request header.
 *     Status - Result status.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SendStatusResponse(KdpProtocolHeader *Header, uint32_t Status) {
    KdpSendProtocolPacket(
        Header->Type, KDP_PROTOCOL_FLAG_RESPONSE, Status, Header->RequestId, NULL, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks whether a retry exactly matches the previously accepted request.
 *
 * PARAMETERS:
 *     Header - Incoming retry header.
 *     Datagram - Complete incoming datagram.
 *
 * RETURN VALUE:
 *     true if the retry can safely reuse the cached response, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool IsExactRetry(KdpProtocolHeader *Header, const uint8_t *Datagram) {
    if (!(Header->Flags & KDP_PROTOCOL_FLAG_RETRY) || Header->Sequence != ReceiveSequence ||
        Header->TotalSize != LastRequestSize || !LastResponseSize) {
        return false;
    }

    KdpProtocolHeader *Previous = (KdpProtocolHeader *)LastRequest;
    return Header->Type == Previous->Type && Header->RequestId == Previous->RequestId &&
           Header->SessionId == Previous->SessionId &&
           !memcmp(
               Datagram + sizeof(*Header),
               LastRequest + sizeof(*Header),
               Header->TotalSize - sizeof(*Header));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a physical or virtual memory-read request.
 *
 * PARAMETERS:
 *     Header - Request header.
 *     Payload - Request payload.
 *     PayloadSize - Request payload size.
 *     Physical - true for physical memory, false for virtual memory.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void
HandleReadMemory(KdpProtocolHeader *Header, void *Payload, size_t PayloadSize, bool Physical) {
    if (PayloadSize != sizeof(KdpProtocolReadMemoryRequest)) {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
        return;
    }
    if (KdpGetTargetState() != KDP_TARGET_STOPPED) {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_STATE);
        return;
    }

    KdpProtocolReadMemoryRequest *Request = Payload;
    if ((Request->ItemSize != 1 && Request->ItemSize != 2 && Request->ItemSize != 4 &&
         Request->ItemSize != 8) ||
        !Request->ItemCount || Request->ItemCount > UINT32_MAX / Request->ItemSize) {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
        return;
    }
    uint32_t Length = Request->ItemSize * Request->ItemCount;
    if (Length > KDP_PROTOCOL_MAX_PAYLOAD - sizeof(KdpProtocolReadMemoryResponse) ||
        Request->Address > UINT64_MAX - Length) {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_TOO_LARGE);
        return;
    }

    uint8_t ResponseBuffer[KDP_PROTOCOL_MAX_PAYLOAD];
    KdpProtocolReadMemoryResponse *Response = (void *)ResponseBuffer;
    *Response = (KdpProtocolReadMemoryResponse){
        .Address = Request->Address,
        .ItemSize = Request->ItemSize,
        .ItemCount = Request->ItemCount,
        .Length = Length,
    };

    uint32_t Status = KDP_PROTOCOL_STATUS_SUCCESS;
    if (Physical) {
        void *Mapping = HalpMapDebuggerMemory(Request->Address, Length, 0);
        if (!Mapping) {
            Status = KDP_PROTOCOL_STATUS_ACCESS_VIOLATION;
        } else {
            __try {
                memcpy(Response + 1, Mapping, Length);
            } __except (RT_EXC_EXECUTE_HANDLER) {
                Status = KDP_PROTOCOL_STATUS_ACCESS_VIOLATION;
            }
            HalpUnmapDebuggerMemory(Mapping, Length);
        }
    } else {
        __try {
            memcpy(Response + 1, (void *)Request->Address, Length);
        } __except (RT_EXC_EXECUTE_HANDLER) {
            Status = KDP_PROTOCOL_STATUS_ACCESS_VIOLATION;
        }
    }

    if (Status != KDP_PROTOCOL_STATUS_SUCCESS) {
        SendStatusResponse(Header, Status);
    } else {
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            Status,
            Header->RequestId,
            Response,
            sizeof(*Response) + Length);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles one validated in-session request.
 *
 * PARAMETERS:
 *     Header - Request header.
 *     Payload - Request payload.
 *     PayloadSize - Request payload size.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void HandleRequest(KdpProtocolHeader *Header, void *Payload, size_t PayloadSize) {
    if (Header->Type == KDP_PROTOCOL_PING) {
        if (PayloadSize) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
        } else {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_SUCCESS);
        }
    } else if (Header->Type == KDP_PROTOCOL_QUERY_STATUS) {
        KdpProtocolStatus Status = {
            .TargetState = KdpGetTargetState(),
            .OwnerProcessor = KdpGetStopOwner(),
            .Generation = KdpGetStopGeneration(),
            .ProcessorCount = HalpOnlineProcessorCount ? HalpOnlineProcessorCount : 1,
            .BreakpointCount = KdpGetBreakpointCount(),
            .Capabilities = Capabilities,
            .DisconnectPolicy = KdpDebugDisconnectPolicy,
            .DisconnectTimeoutMilliseconds = KdpDebugDisconnectTimeoutMilliseconds,
        };
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            KDP_PROTOCOL_STATUS_SUCCESS,
            Header->RequestId,
            &Status,
            sizeof(Status));
    } else if (Header->Type == KDP_PROTOCOL_LIST_CPUS) {
        if (PayloadSize != sizeof(KdpProtocolListRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolListRequest *Request = Payload;
        uint32_t Total = HalpOnlineProcessorCount ? HalpOnlineProcessorCount : 1;
        uint32_t Maximum = (KDP_PROTOCOL_MAX_PAYLOAD - sizeof(KdpProtocolListResponse)) /
                           sizeof(KdpProtocolProcessorEntry);
        if (Request->StartIndex > Total || Request->MaximumEntries > Maximum) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
            return;
        }
        uint8_t Buffer[KDP_PROTOCOL_MAX_PAYLOAD];
        KdpProtocolListResponse *Response = (void *)Buffer;
        Response->TotalCount = Total;
        Response->ReturnedCount = Total - Request->StartIndex;
        if (Response->ReturnedCount > Request->MaximumEntries) {
            Response->ReturnedCount = Request->MaximumEntries;
        }
        KdpProtocolProcessorEntry *Entries = (void *)(Response + 1);
        for (uint32_t i = 0; i < Response->ReturnedCount; i++) {
            Entries[i] = (KdpProtocolProcessorEntry){
                .Processor = Request->StartIndex + i,
                .Flags = Request->StartIndex + i == KdpGetStopOwner() ? 1u : 0,
                .Generation = KdpGetStopGeneration(),
            };
        }
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            KDP_PROTOCOL_STATUS_SUCCESS,
            Header->RequestId,
            Buffer,
            sizeof(*Response) + Response->ReturnedCount * sizeof(*Entries));
    } else if (Header->Type == KDP_PROTOCOL_READ_CONTEXT) {
        if (PayloadSize != sizeof(KdpProtocolContextRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolContextRequest *Request = Payload;
        KdpProtocolContextResponse Response = {.Processor = Request->Processor};
        uint64_t Generation = 0;
        if (Request->Reserved ||
            !KdpGetContext(Request->Processor, &Generation, &Response.Context)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
            return;
        }
        Response.Generation = Generation;
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            KDP_PROTOCOL_STATUS_SUCCESS,
            Header->RequestId,
            &Response,
            sizeof(Response));
    } else if (
        Header->Type == KDP_PROTOCOL_READ_PHYSICAL || Header->Type == KDP_PROTOCOL_READ_VIRTUAL) {
        HandleReadMemory(Header, Payload, PayloadSize, Header->Type == KDP_PROTOCOL_READ_PHYSICAL);
    } else if (Header->Type == KDP_PROTOCOL_READ_PORT) {
        if (PayloadSize != sizeof(KdpProtocolReadPortRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolReadPortRequest *Request = Payload;
        if (KdpGetTargetState() != KDP_TARGET_STOPPED ||
            (Request->Size != 1 && Request->Size != 2 && Request->Size != 4)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
            return;
        }
        KdpProtocolReadPortResponse Response = {
            .Port = Request->Port,
            .Size = Request->Size,
        };
        if (Request->Size == 1) {
            Response.Value = ReadPortByte(Request->Port);
        } else if (Request->Size == 2) {
            Response.Value = ReadPortWord(Request->Port);
        } else {
            Response.Value = ReadPortDWord(Request->Port);
        }
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            KDP_PROTOCOL_STATUS_SUCCESS,
            Header->RequestId,
            &Response,
            sizeof(Response));
    } else if (Header->Type == KDP_PROTOCOL_REQUEST_STOP) {
        if (PayloadSize || KdpGetTargetState() != KDP_TARGET_RUNNING) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_STATE);
            return;
        }
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_SUCCESS);
        KdpRequestStop();
    } else if (Header->Type == KDP_PROTOCOL_CONTINUE || Header->Type == KDP_PROTOCOL_SINGLE_STEP) {
        if (PayloadSize) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        uint32_t Status = KdpResume(Header->Type == KDP_PROTOCOL_SINGLE_STEP);
        SendStatusResponse(Header, Status);
    } else if (Header->Type == KDP_PROTOCOL_BREAKPOINT_ADD) {
        if (PayloadSize != sizeof(KdpProtocolBreakpointAddRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolBreakpointEntry Entry;
        uint32_t Status =
            KdpAddBreakpoint(((KdpProtocolBreakpointAddRequest *)Payload)->Address, &Entry);
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            Status,
            Header->RequestId,
            Status == KDP_PROTOCOL_STATUS_SUCCESS ? &Entry : NULL,
            Status == KDP_PROTOCOL_STATUS_SUCCESS ? sizeof(Entry) : 0);
    } else if (
        Header->Type == KDP_PROTOCOL_BREAKPOINT_REMOVE ||
        Header->Type == KDP_PROTOCOL_BREAKPOINT_ENABLE ||
        Header->Type == KDP_PROTOCOL_BREAKPOINT_DISABLE) {
        if (PayloadSize != sizeof(KdpProtocolBreakpointRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolBreakpointRequest *Request = Payload;
        if (Request->Reserved) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
            return;
        }
        int Operation = Header->Type == KDP_PROTOCOL_BREAKPOINT_REMOVE
                            ? 0
                            : (Header->Type == KDP_PROTOCOL_BREAKPOINT_ENABLE ? 1 : 2);
        SendStatusResponse(Header, KdpChangeBreakpoint(Request->Identifier, Operation));
    } else if (Header->Type == KDP_PROTOCOL_BREAKPOINT_LIST) {
        if (PayloadSize != sizeof(KdpProtocolListRequest)) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_MALFORMED_MESSAGE);
            return;
        }
        KdpProtocolListRequest *Request = Payload;
        uint32_t Maximum = (KDP_PROTOCOL_MAX_PAYLOAD - sizeof(KdpProtocolListResponse)) /
                           sizeof(KdpProtocolBreakpointEntry);
        if (Request->MaximumEntries > Maximum) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_ARGUMENT);
            return;
        }
        uint8_t Buffer[KDP_PROTOCOL_MAX_PAYLOAD];
        KdpProtocolListResponse *Response = (void *)Buffer;
        uint32_t TotalCount = 0;
        uint32_t ReturnedCount = 0;
        uint32_t Status = KdpListBreakpoints(
            Request->StartIndex,
            Request->MaximumEntries,
            (void *)(Response + 1),
            &TotalCount,
            &ReturnedCount);
        Response->TotalCount = TotalCount;
        Response->ReturnedCount = ReturnedCount;
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            Status,
            Header->RequestId,
            Status == KDP_PROTOCOL_STATUS_SUCCESS ? Buffer : NULL,
            Status == KDP_PROTOCOL_STATUS_SUCCESS
                ? sizeof(*Response) + Response->ReturnedCount * sizeof(KdpProtocolBreakpointEntry)
                : 0);
    } else if (Header->Type == KDP_PROTOCOL_DETACH) {
        if (PayloadSize || KdpGetTargetState() == KDP_TARGET_PANIC) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_NONRESUMABLE);
            return;
        }
        if (KdpGetTargetState() != KDP_TARGET_STOPPED && KdpGetBreakpointCount()) {
            SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_STATE);
            return;
        }
        if (KdpGetTargetState() == KDP_TARGET_STOPPED) {
            for (uint32_t i = 1; i < UINT32_MAX && KdpGetBreakpointCount(); i++) {
                KdpChangeBreakpoint(i, 0);
            }
        }
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_SUCCESS);
        if (KdpGetTargetState() == KDP_TARGET_STOPPED) {
            KdpResume(false);
        }
        KdpDebugConnected = false;
        SessionId = 0;
    } else {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_UNSUPPORTED);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses one protocol datagram received from UDP or serial.
 *
 * PARAMETERS:
 *     State - Early-handshake or runtime receive state.
 *     SourceHardwareAddress - UDP source MAC, or zero for serial.
 *     SourceProtocolAddress - UDP source IPv4 address, or zero for serial.
 *     SourcePort - UDP source port, or zero for serial.
 *     Packet - Complete protocol datagram.
 *     Length - Datagram size.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseDebugPacket(
    int State,
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    void *Packet,
    uint32_t Length) {
    (void)State;
    if (!Packet || Length < sizeof(KdpProtocolHeader) || Length > KDP_PROTOCOL_MAX_DATAGRAM) {
        return;
    }
    KdpProtocolHeader *Header = Packet;
    if (memcmp(Header->Magic, KDP_PROTOCOL_MAGIC, 4) || Header->HeaderSize != sizeof(*Header) ||
        Header->TotalSize != Length || Header->Reserved || Header->Status ||
        (Header->Flags != KDP_PROTOCOL_FLAG_REQUEST &&
         Header->Flags != (KDP_PROTOCOL_FLAG_REQUEST | KDP_PROTOCOL_FLAG_RETRY))) {
        return;
    }
    if (Header->Major != KDP_PROTOCOL_MAJOR || Header->Minor > KDP_PROTOCOL_MINOR) {
        return;
    }

    uint8_t *Payload = (uint8_t *)(Header + 1);
    size_t PayloadSize = Length - sizeof(*Header);
    if (Header->Type == KDP_PROTOCOL_HELLO) {
        if (Header->SessionId || Header->Sequence != 1 ||
            PayloadSize != sizeof(KdpProtocolHelloRequest)) {
            return;
        }
        KdpProtocolHelloRequest *Hello = (void *)Payload;
        if (!Hello->Nonce || Hello->Reserved ||
            Hello->MaximumDatagram < sizeof(KdpProtocolHeader)) {
            return;
        }
        if (KdpDebugConnected && Hello->Nonce != ClientNonce) {
            return;
        }

        memcpy(KdpDebuggerHardwareAddress, SourceHardwareAddress, 6);
        memcpy(KdpDebuggerProtocolAddress, SourceProtocolAddress, 4);
        KdpDebuggerPort = SourcePort;
        ClientNonce = Hello->Nonce;
        SessionId = Hello->Nonce ^ HalGetTimerTicks() ^ (uint64_t)(uintptr_t)Packet;
        if (!SessionId) {
            SessionId = 1;
        }
        ReceiveSequence = 1;
        SendSequence = 0;
        KdpDebugConnected = true;
        /* The HAL replaces its provisional early timer during boot. Start heartbeat accounting
         * after runtime polling observes the final timer source. */
        LastActivityTicks = 0;
        KdpProtocolHelloResponse Response = {
            .Capabilities = Capabilities & Hello->Capabilities,
            .MaximumDatagram = KDP_PROTOCOL_MAX_DATAGRAM,
            .Architecture = KDP_PROTOCOL_ARCH_AMD64,
            .ContextVersion = KDP_PROTOCOL_CONTEXT_VERSION,
            .ProcessorCount = HalpOnlineProcessorCount ? HalpOnlineProcessorCount : 1,
            .TargetState = KdpGetTargetState(),
            .Transport = KdpDebugTransport,
            .DisconnectPolicy = KdpDebugDisconnectPolicy,
            .DisconnectTimeoutMilliseconds = KdpDebugDisconnectTimeoutMilliseconds,
        };
        KdpSendProtocolPacket(
            Header->Type,
            KDP_PROTOCOL_FLAG_RESPONSE,
            KDP_PROTOCOL_STATUS_SUCCESS,
            Header->RequestId,
            &Response,
            sizeof(Response));
        if (KdpGetTargetState() == KDP_TARGET_STOPPED) {
            KdpSendStopEvent(KdpGetStopReason(), true);
        }
        return;
    }

    if (!KdpDebugConnected || Header->SessionId != SessionId) {
        return;
    }
    if (KdpDebugTransport == KI_LOADER_DEBUG_TRANSPORT_KDNET &&
        (memcmp(SourceHardwareAddress, KdpDebuggerHardwareAddress, 6) ||
         memcmp(SourceProtocolAddress, KdpDebuggerProtocolAddress, 4) ||
         SourcePort != KdpDebuggerPort)) {
        return;
    }
    if (IsExactRetry(Header, Packet)) {
        LastActivityTicks = HalGetTimerTicks();
        SendDatagram(LastResponse, LastResponseSize);
        return;
    }
    if (Header->Sequence != ReceiveSequence + 1) {
        SendStatusResponse(Header, KDP_PROTOCOL_STATUS_INVALID_SEQUENCE);
        return;
    }

    ReceiveSequence = Header->Sequence;
    LastActivityTicks = HalGetTimerTicks();
    memcpy(LastRequest, Packet, Length);
    LastRequestSize = Length;
    LastResponseSize = 0;
    CacheResponse = true;
    HandleRequest(Header, Payload, PayloadSize);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function emits the current owner stop as an asynchronous protocol event.
 *
 * PARAMETERS:
 *     Reason - Stop reason.
 *     Resumable - Whether continue/step are valid.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpSendStopEvent(uint32_t Reason, bool Resumable) {
    KdpProtocolStopEvent Event = {
        .Reason = Reason,
        .Flags = Resumable ? KDP_STOP_FLAG_RESUMABLE : 0,
        .Generation = KdpGetStopGeneration(),
        .Processor = KdpGetStopOwner(),
        .ProcessorCount = HalpOnlineProcessorCount ? HalpOnlineProcessorCount : 1,
    };
    uint64_t Generation = 0;
    KdpGetContext(Event.Processor, &Generation, &Event.Context);
    Event.Address = Event.Context.Rip;
    Event.ExceptionCode = Reason == KDP_STOP_REASON_SOFTWARE_BREAKPOINT
                              ? RT_EXC_BREAKPOINT
                              : (Reason == KDP_STOP_REASON_SINGLE_STEP ? RT_EXC_SINGLE_STEP : 0);
    KdpSendProtocolPacket(
        KDP_PROTOCOL_STOP,
        KDP_PROTOCOL_FLAG_EVENT,
        KDP_PROTOCOL_STATUS_SUCCESS,
        0,
        &Event,
        sizeof(Event));
}
