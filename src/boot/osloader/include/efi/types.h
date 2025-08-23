// clang-format off

/** @file
  Processor or Compiler specific defines and types.

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _EFI_TYPES_H_
#define _EFI_TYPES_H_

//
// Make sure we are using the correct packing rules per EFI specification
//
#pragma pack()

//
// Modifiers to abstract standard types to aid in debug of problems
//

///
/// Datum is read-only.
///
#define CONST  const

///
/// Datum is scoped to the current file or function.
///
#define STATIC  static

///
/// Undeclared type.
///
#define VOID  void

//
// Modifiers for Data Types used to self document code.
// This concept is borrowed for UEFI specification.
//

///
/// Datum is passed to the function.
///
#define IN

///
/// Datum is returned from the function.
///
#define OUT

///
/// Passing the datum to the function is optional, and a NULL
/// is passed if the value is not supplied.
///
#define OPTIONAL

//
//  UEFI specification claims 1 and 0. We are concerned about the
//  compiler portability so we did it this way.
//

///
/// Boolean true value.  UEFI Specification defines this value to be 1,
/// but this form is more portable.
///
#ifndef TRUE
#define TRUE  ((BOOLEAN)(1==1))
#endif

///
/// Boolean false value.  UEFI Specification defines this value to be 0,
/// but this form is more portable.
///
#ifndef FALSE
#define FALSE  ((BOOLEAN)(0==1))
#endif

///
/// NULL pointer (VOID *)
///
#ifndef NULL
#if defined (__cplusplus)
#define NULL  nullptr
#else
#define NULL  ((VOID *) 0)
#endif
#endif

//
// Null character
//
#define CHAR_NULL  0x0000

///
/// Maximum values for common UEFI Data Types
///
#define MAX_INT8    ((INT8)0x7F)
#define MAX_UINT8   ((UINT8)0xFF)
#define MAX_INT16   ((INT16)0x7FFF)
#define MAX_UINT16  ((UINT16)0xFFFF)
#define MAX_INT32   ((INT32)0x7FFFFFFF)
#define MAX_UINT32  ((UINT32)0xFFFFFFFF)
#define MAX_INT64   ((INT64)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINT64  ((UINT64)0xFFFFFFFFFFFFFFFFULL)

///
/// Minimum values for the signed UEFI Data Types
///
#define MIN_INT8   (((INT8)  -127) - 1)
#define MIN_INT16  (((INT16) -32767) - 1)
#define MIN_INT32  (((INT32) -2147483647) - 1)
#define MIN_INT64  (((INT64) -9223372036854775807LL) - 1)

#define  BIT0   0x00000001
#define  BIT1   0x00000002
#define  BIT2   0x00000004
#define  BIT3   0x00000008
#define  BIT4   0x00000010
#define  BIT5   0x00000020
#define  BIT6   0x00000040
#define  BIT7   0x00000080
#define  BIT8   0x00000100
#define  BIT9   0x00000200
#define  BIT10  0x00000400
#define  BIT11  0x00000800
#define  BIT12  0x00001000
#define  BIT13  0x00002000
#define  BIT14  0x00004000
#define  BIT15  0x00008000
#define  BIT16  0x00010000
#define  BIT17  0x00020000
#define  BIT18  0x00040000
#define  BIT19  0x00080000
#define  BIT20  0x00100000
#define  BIT21  0x00200000
#define  BIT22  0x00400000
#define  BIT23  0x00800000
#define  BIT24  0x01000000
#define  BIT25  0x02000000
#define  BIT26  0x04000000
#define  BIT27  0x08000000
#define  BIT28  0x10000000
#define  BIT29  0x20000000
#define  BIT30  0x40000000
#define  BIT31  0x80000000
#define  BIT32  0x0000000100000000ULL
#define  BIT33  0x0000000200000000ULL
#define  BIT34  0x0000000400000000ULL
#define  BIT35  0x0000000800000000ULL
#define  BIT36  0x0000001000000000ULL
#define  BIT37  0x0000002000000000ULL
#define  BIT38  0x0000004000000000ULL
#define  BIT39  0x0000008000000000ULL
#define  BIT40  0x0000010000000000ULL
#define  BIT41  0x0000020000000000ULL
#define  BIT42  0x0000040000000000ULL
#define  BIT43  0x0000080000000000ULL
#define  BIT44  0x0000100000000000ULL
#define  BIT45  0x0000200000000000ULL
#define  BIT46  0x0000400000000000ULL
#define  BIT47  0x0000800000000000ULL
#define  BIT48  0x0001000000000000ULL
#define  BIT49  0x0002000000000000ULL
#define  BIT50  0x0004000000000000ULL
#define  BIT51  0x0008000000000000ULL
#define  BIT52  0x0010000000000000ULL
#define  BIT53  0x0020000000000000ULL
#define  BIT54  0x0040000000000000ULL
#define  BIT55  0x0080000000000000ULL
#define  BIT56  0x0100000000000000ULL
#define  BIT57  0x0200000000000000ULL
#define  BIT58  0x0400000000000000ULL
#define  BIT59  0x0800000000000000ULL
#define  BIT60  0x1000000000000000ULL
#define  BIT61  0x2000000000000000ULL
#define  BIT62  0x4000000000000000ULL
#define  BIT63  0x8000000000000000ULL

#define  SIZE_1KB    0x00000400
#define  SIZE_2KB    0x00000800
#define  SIZE_4KB    0x00001000
#define  SIZE_8KB    0x00002000
#define  SIZE_16KB   0x00004000
#define  SIZE_32KB   0x00008000
#define  SIZE_64KB   0x00010000
#define  SIZE_128KB  0x00020000
#define  SIZE_256KB  0x00040000
#define  SIZE_512KB  0x00080000
#define  SIZE_1MB    0x00100000
#define  SIZE_2MB    0x00200000
#define  SIZE_4MB    0x00400000
#define  SIZE_8MB    0x00800000
#define  SIZE_16MB   0x01000000
#define  SIZE_32MB   0x02000000
#define  SIZE_64MB   0x04000000
#define  SIZE_128MB  0x08000000
#define  SIZE_256MB  0x10000000
#define  SIZE_512MB  0x20000000
#define  SIZE_1GB    0x40000000
#define  SIZE_2GB    0x80000000
#define  SIZE_4GB    0x0000000100000000ULL
#define  SIZE_8GB    0x0000000200000000ULL
#define  SIZE_16GB   0x0000000400000000ULL
#define  SIZE_32GB   0x0000000800000000ULL
#define  SIZE_64GB   0x0000001000000000ULL
#define  SIZE_128GB  0x0000002000000000ULL
#define  SIZE_256GB  0x0000004000000000ULL
#define  SIZE_512GB  0x0000008000000000ULL
#define  SIZE_1TB    0x0000010000000000ULL
#define  SIZE_2TB    0x0000020000000000ULL
#define  SIZE_4TB    0x0000040000000000ULL
#define  SIZE_8TB    0x0000080000000000ULL
#define  SIZE_16TB   0x0000100000000000ULL
#define  SIZE_32TB   0x0000200000000000ULL
#define  SIZE_64TB   0x0000400000000000ULL
#define  SIZE_128TB  0x0000800000000000ULL
#define  SIZE_256TB  0x0001000000000000ULL
#define  SIZE_512TB  0x0002000000000000ULL
#define  SIZE_1PB    0x0004000000000000ULL
#define  SIZE_2PB    0x0008000000000000ULL
#define  SIZE_4PB    0x0010000000000000ULL
#define  SIZE_8PB    0x0020000000000000ULL
#define  SIZE_16PB   0x0040000000000000ULL
#define  SIZE_32PB   0x0080000000000000ULL
#define  SIZE_64PB   0x0100000000000000ULL
#define  SIZE_128PB  0x0200000000000000ULL
#define  SIZE_256PB  0x0400000000000000ULL
#define  SIZE_512PB  0x0800000000000000ULL
#define  SIZE_1EB    0x1000000000000000ULL
#define  SIZE_2EB    0x2000000000000000ULL
#define  SIZE_4EB    0x4000000000000000ULL
#define  SIZE_8EB    0x8000000000000000ULL

#define  BASE_1KB    0x00000400
#define  BASE_2KB    0x00000800
#define  BASE_4KB    0x00001000
#define  BASE_8KB    0x00002000
#define  BASE_16KB   0x00004000
#define  BASE_32KB   0x00008000
#define  BASE_64KB   0x00010000
#define  BASE_128KB  0x00020000
#define  BASE_256KB  0x00040000
#define  BASE_512KB  0x00080000
#define  BASE_1MB    0x00100000
#define  BASE_2MB    0x00200000
#define  BASE_4MB    0x00400000
#define  BASE_8MB    0x00800000
#define  BASE_16MB   0x01000000
#define  BASE_32MB   0x02000000
#define  BASE_64MB   0x04000000
#define  BASE_128MB  0x08000000
#define  BASE_256MB  0x10000000
#define  BASE_512MB  0x20000000
#define  BASE_1GB    0x40000000
#define  BASE_2GB    0x80000000
#define  BASE_4GB    0x0000000100000000ULL
#define  BASE_8GB    0x0000000200000000ULL
#define  BASE_16GB   0x0000000400000000ULL
#define  BASE_32GB   0x0000000800000000ULL
#define  BASE_64GB   0x0000001000000000ULL
#define  BASE_128GB  0x0000002000000000ULL
#define  BASE_256GB  0x0000004000000000ULL
#define  BASE_512GB  0x0000008000000000ULL
#define  BASE_1TB    0x0000010000000000ULL
#define  BASE_2TB    0x0000020000000000ULL
#define  BASE_4TB    0x0000040000000000ULL
#define  BASE_8TB    0x0000080000000000ULL
#define  BASE_16TB   0x0000100000000000ULL
#define  BASE_32TB   0x0000200000000000ULL
#define  BASE_64TB   0x0000400000000000ULL
#define  BASE_128TB  0x0000800000000000ULL
#define  BASE_256TB  0x0001000000000000ULL
#define  BASE_512TB  0x0002000000000000ULL
#define  BASE_1PB    0x0004000000000000ULL
#define  BASE_2PB    0x0008000000000000ULL
#define  BASE_4PB    0x0010000000000000ULL
#define  BASE_8PB    0x0020000000000000ULL
#define  BASE_16PB   0x0040000000000000ULL
#define  BASE_32PB   0x0080000000000000ULL
#define  BASE_64PB   0x0100000000000000ULL
#define  BASE_128PB  0x0200000000000000ULL
#define  BASE_256PB  0x0400000000000000ULL
#define  BASE_512PB  0x0800000000000000ULL
#define  BASE_1EB    0x1000000000000000ULL
#define  BASE_2EB    0x2000000000000000ULL
#define  BASE_4EB    0x4000000000000000ULL
#define  BASE_8EB    0x8000000000000000ULL

/**
  Produces a RETURN_STATUS code with the highest bit set.

  @param  StatusCode    The status code value to convert into a warning code.
                        StatusCode must be in the range 0x00000000..0x7FFFFFFF.

  @return The value specified by StatusCode with the highest bit set.

**/
#define ENCODE_ERROR(StatusCode)  ((RETURN_STATUS)(MAX_BIT | (StatusCode)))

/**
  Produces a RETURN_STATUS code with the highest bit clear.

  @param  StatusCode    The status code value to convert into a warning code.
                        StatusCode must be in the range 0x00000000..0x7FFFFFFF.

  @return The value specified by StatusCode with the highest bit clear.

**/
#define ENCODE_WARNING(StatusCode)  ((RETURN_STATUS)(StatusCode))

/**
  Returns TRUE if a specified RETURN_STATUS code is an error code.

  This function returns TRUE if StatusCode has the high bit set.  Otherwise, FALSE is returned.

  @param  StatusCode    The status code value to evaluate.

  @retval TRUE          The high bit of StatusCode is set.
  @retval FALSE         The high bit of StatusCode is clear.

**/
#define RETURN_ERROR(StatusCode)  (((INTN)(RETURN_STATUS)(StatusCode)) < 0)

///
/// The operation completed successfully.
///
#define RETURN_SUCCESS  (RETURN_STATUS)(0)

///
/// The image failed to load.
///
#define RETURN_LOAD_ERROR  ENCODE_ERROR (1)

///
/// The parameter was incorrect.
///
#define RETURN_INVALID_PARAMETER  ENCODE_ERROR (2)

///
/// The operation is not supported.
///
#define RETURN_UNSUPPORTED  ENCODE_ERROR (3)

///
/// The buffer was not the proper size for the request.
///
#define RETURN_BAD_BUFFER_SIZE  ENCODE_ERROR (4)

///
/// The buffer was not large enough to hold the requested data.
/// The required buffer size is returned in the appropriate
/// parameter when this error occurs.
///
#define RETURN_BUFFER_TOO_SMALL  ENCODE_ERROR (5)

///
/// There is no data pending upon return.
///
#define RETURN_NOT_READY  ENCODE_ERROR (6)

///
/// The physical device reported an error while attempting the
/// operation.
///
#define RETURN_DEVICE_ERROR  ENCODE_ERROR (7)

///
/// The device can not be written to.
///
#define RETURN_WRITE_PROTECTED  ENCODE_ERROR (8)

///
/// The resource has run out.
///
#define RETURN_OUT_OF_RESOURCES  ENCODE_ERROR (9)

///
/// An inconsistency was detected on the file system causing the
/// operation to fail.
///
#define RETURN_VOLUME_CORRUPTED  ENCODE_ERROR (10)

///
/// There is no more space on the file system.
///
#define RETURN_VOLUME_FULL  ENCODE_ERROR (11)

///
/// The device does not contain any medium to perform the
/// operation.
///
#define RETURN_NO_MEDIA  ENCODE_ERROR (12)

///
/// The medium in the device has changed since the last
/// access.
///
#define RETURN_MEDIA_CHANGED  ENCODE_ERROR (13)

///
/// The item was not found.
///
#define RETURN_NOT_FOUND  ENCODE_ERROR (14)

///
/// Access was denied.
///
#define RETURN_ACCESS_DENIED  ENCODE_ERROR (15)

///
/// The server was not found or did not respond to the request.
///
#define RETURN_NO_RESPONSE  ENCODE_ERROR (16)

///
/// A mapping to the device does not exist.
///
#define RETURN_NO_MAPPING  ENCODE_ERROR (17)

///
/// A timeout time expired.
///
#define RETURN_TIMEOUT  ENCODE_ERROR (18)

///
/// The protocol has not been started.
///
#define RETURN_NOT_STARTED  ENCODE_ERROR (19)

///
/// The protocol has already been started.
///
#define RETURN_ALREADY_STARTED  ENCODE_ERROR (20)

///
/// The operation was aborted.
///
#define RETURN_ABORTED  ENCODE_ERROR (21)

///
/// An ICMP error occurred during the network operation.
///
#define RETURN_ICMP_ERROR  ENCODE_ERROR (22)

///
/// A TFTP error occurred during the network operation.
///
#define RETURN_TFTP_ERROR  ENCODE_ERROR (23)

///
/// A protocol error occurred during the network operation.
///
#define RETURN_PROTOCOL_ERROR  ENCODE_ERROR (24)

///
/// A function encountered an internal version that was
/// incompatible with a version requested by the caller.
///
#define RETURN_INCOMPATIBLE_VERSION  ENCODE_ERROR (25)

///
/// The function was not performed due to a security violation.
///
#define RETURN_SECURITY_VIOLATION  ENCODE_ERROR (26)

///
/// A CRC error was detected.
///
#define RETURN_CRC_ERROR  ENCODE_ERROR (27)

///
/// The beginning or end of media was reached.
///
#define RETURN_END_OF_MEDIA  ENCODE_ERROR (28)

///
/// The end of the file was reached.
///
#define RETURN_END_OF_FILE  ENCODE_ERROR (31)

///
/// The language specified was invalid.
///
#define RETURN_INVALID_LANGUAGE  ENCODE_ERROR (32)

///
/// The security status of the data is unknown or compromised
/// and the data must be updated or replaced to restore a valid
/// security status.
///
#define RETURN_COMPROMISED_DATA  ENCODE_ERROR (33)

///
/// There is an address conflict address allocation.
///
#define RETURN_IP_ADDRESS_CONFLICT  ENCODE_ERROR (34)

///
/// A HTTP error occurred during the network operation.
///
#define RETURN_HTTP_ERROR  ENCODE_ERROR (35)

///
/// The string contained one or more characters that
/// the device could not render and were skipped.
///
#define RETURN_WARN_UNKNOWN_GLYPH  ENCODE_WARNING (1)

///
/// The handle was closed, but the file was not deleted.
///
#define RETURN_WARN_DELETE_FAILURE  ENCODE_WARNING (2)

///
/// The handle was closed, but the data to the file was not
/// flushed properly.
///
#define RETURN_WARN_WRITE_FAILURE  ENCODE_WARNING (3)

///
/// The resulting buffer was too small, and the data was
/// truncated to the buffer size.
///
#define RETURN_WARN_BUFFER_TOO_SMALL  ENCODE_WARNING (4)

///
/// The data has not been updated within the timeframe set by
/// local policy for this type of data.
///
#define RETURN_WARN_STALE_DATA  ENCODE_WARNING (5)

///
/// The resulting buffer contains UEFI-compliant file system.
///
#define RETURN_WARN_FILE_SYSTEM  ENCODE_WARNING (6)

///
/// The operation will be processed across a system reset.
///
#define RETURN_WARN_RESET_REQUIRED  ENCODE_WARNING (7)

///
/// 8-byte unsigned value
///
typedef unsigned __int64 UINT64;
///
/// 8-byte signed value
///
typedef __int64 INT64;
///
/// 4-byte unsigned value
///
typedef unsigned __int32 UINT32;
///
/// 4-byte signed value
///
typedef __int32 INT32;
///
/// 2-byte unsigned value
///
typedef unsigned short UINT16;
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef unsigned short CHAR16;
///
/// 2-byte signed value
///
typedef short INT16;
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char BOOLEAN;
///
/// 1-byte unsigned value
///
typedef unsigned char UINT8;
///
/// 1-byte Character
///
typedef char CHAR8;
///
/// 1-byte signed value
///
typedef signed char INT8;

///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef UINT64 UINTN;
///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef INT64 INTN;

///
/// A value of native width with the highest bit set.
///
#define MAX_BIT  0x8000000000000000ULL
///
/// A value of native width with the two highest bits set.
///
#define MAX_2_BITS  0xC000000000000000ULL

///
/// Maximum legal address
///
#define MAX_ADDRESS  0xFFFFFFFFFFFFFFFFULL

///
/// Maximum usable address at boot time
///
#define MAX_ALLOC_ADDRESS  MAX_ADDRESS

///
/// Maximum legal INTN and UINTN values.
///
#define MAX_INTN   ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN  ((UINTN)0xFFFFFFFFFFFFFFFFULL)

///
/// Minimum legal INTN value.
///
#define MIN_INTN  (((INTN)-9223372036854775807LL) - 1)

///
/// The stack alignment required for ARCH_*
///
#ifdef ARCH_amd64
#define CPU_STACK_ALIGNMENT  16
#else
#error "Undefined ARCH for the osloader module!"
#endif

///
/// Page allocation granularity for ARCH_*
///
#ifdef ARCH_amd64
#define DEFAULT_PAGE_ALLOCATION_GRANULARITY  (0x1000)
#define RUNTIME_PAGE_ALLOCATION_GRANULARITY  (0x1000)
#else
#error "Undefined ARCH for the osloader module!"
#endif

//
// Modifier to ensure that all protocol member functions and EFI intrinsics
// use the correct C calling convention. All protocol member functions and
// EFI intrinsics are required to modify their member functions with EFIAPI.
//
#ifdef EFIAPI
///
/// If EFIAPI is already defined, then we use that definition.
///
#else
///
/// Microsoft* compiler specific method for EFIAPI calling convention.
///
#define EFIAPI  __cdecl
#endif

///
/// 128-bit buffer containing a unique identifier value.
///
typedef struct {
    UINT32 Data0;
    UINT16 Data1;
    UINT16 Data2;
    UINT8 Data3[8];
} EFI_GUID;
//
// Status codes common to all execution phases
//
typedef UINTN RETURN_STATUS;
///
/// Function return status for EFI API.
///
typedef RETURN_STATUS EFI_STATUS;
///
/// A collection of related interfaces.
///
typedef VOID *EFI_HANDLE;
///
/// Handle to an event structure.
///
typedef VOID *EFI_EVENT;
///
/// Task priority level.
///
typedef UINTN EFI_TPL;
///
/// Logical block address.
///
typedef UINT64 EFI_LBA;

///
/// 64-bit physical memory address.
///
typedef UINT64 EFI_PHYSICAL_ADDRESS;

///
/// 64-bit virtual memory address.
///
typedef UINT64 EFI_VIRTUAL_ADDRESS;

///
/// EFI Time Abstraction:
///  Year:       1900 - 9999
///  Month:      1 - 12
///  Day:        1 - 31
///  Hour:       0 - 23
///  Minute:     0 - 59
///  Second:     0 - 59
///  Nanosecond: 0 - 999,999,999
///  TimeZone:   -1440 to 1440 or 2047
///
typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

//
// Networking Definitions
//
typedef struct {
  UINT8 Addr[4];
} EFI_IPv4_ADDRESS;

typedef struct {
  UINT8 Addr[16];
} EFI_IPv6_ADDRESS;

typedef struct {
  UINT8 Addr[32];
} EFI_MAC_ADDRESS;

typedef union {
  UINT32            Addr[4];
  EFI_IPv4_ADDRESS  v4;
  EFI_IPv6_ADDRESS  v6;
} EFI_IP_ADDRESS;

//
/// BLUETOOTH_ADDRESS
///
typedef struct {
  ///
  /// 48bit Bluetooth device address.
  ///
  UINT8    Address[6];
} BLUETOOTH_ADDRESS;

///
/// BLUETOOTH_CLASS_OF_DEVICE. See Bluetooth specification for detail.
///
typedef struct {
  UINT8     FormatType        : 2;
  UINT8     MinorDeviceClass  : 6;
  UINT16    MajorDeviceClass  : 5;
  UINT16    MajorServiceClass : 11;
} BLUETOOTH_CLASS_OF_DEVICE;

///
/// BLUETOOTH_LE_ADDRESS
///
typedef struct {
  ///
  /// 48-bit Bluetooth device address
  ///
  UINT8    Address[6];
  ///
  /// 0x00 - Public Device Address
  /// 0x01 - Random Device Address
  ///
  UINT8    Type;
} BLUETOOTH_LE_ADDRESS;

///
/// Enumeration of EFI_STATUS.
///@{
#define EFI_SUCCESS               RETURN_SUCCESS
#define EFI_LOAD_ERROR            RETURN_LOAD_ERROR
#define EFI_INVALID_PARAMETER     RETURN_INVALID_PARAMETER
#define EFI_UNSUPPORTED           RETURN_UNSUPPORTED
#define EFI_BAD_BUFFER_SIZE       RETURN_BAD_BUFFER_SIZE
#define EFI_BUFFER_TOO_SMALL      RETURN_BUFFER_TOO_SMALL
#define EFI_NOT_READY             RETURN_NOT_READY
#define EFI_DEVICE_ERROR          RETURN_DEVICE_ERROR
#define EFI_WRITE_PROTECTED       RETURN_WRITE_PROTECTED
#define EFI_OUT_OF_RESOURCES      RETURN_OUT_OF_RESOURCES
#define EFI_VOLUME_CORRUPTED      RETURN_VOLUME_CORRUPTED
#define EFI_VOLUME_FULL           RETURN_VOLUME_FULL
#define EFI_NO_MEDIA              RETURN_NO_MEDIA
#define EFI_MEDIA_CHANGED         RETURN_MEDIA_CHANGED
#define EFI_NOT_FOUND             RETURN_NOT_FOUND
#define EFI_ACCESS_DENIED         RETURN_ACCESS_DENIED
#define EFI_NO_RESPONSE           RETURN_NO_RESPONSE
#define EFI_NO_MAPPING            RETURN_NO_MAPPING
#define EFI_TIMEOUT               RETURN_TIMEOUT
#define EFI_NOT_STARTED           RETURN_NOT_STARTED
#define EFI_ALREADY_STARTED       RETURN_ALREADY_STARTED
#define EFI_ABORTED               RETURN_ABORTED
#define EFI_ICMP_ERROR            RETURN_ICMP_ERROR
#define EFI_TFTP_ERROR            RETURN_TFTP_ERROR
#define EFI_PROTOCOL_ERROR        RETURN_PROTOCOL_ERROR
#define EFI_INCOMPATIBLE_VERSION  RETURN_INCOMPATIBLE_VERSION
#define EFI_SECURITY_VIOLATION    RETURN_SECURITY_VIOLATION
#define EFI_CRC_ERROR             RETURN_CRC_ERROR
#define EFI_END_OF_MEDIA          RETURN_END_OF_MEDIA
#define EFI_END_OF_FILE           RETURN_END_OF_FILE
#define EFI_INVALID_LANGUAGE      RETURN_INVALID_LANGUAGE
#define EFI_COMPROMISED_DATA      RETURN_COMPROMISED_DATA
#define EFI_IP_ADDRESS_CONFLICT   RETURN_IP_ADDRESS_CONFLICT
#define EFI_HTTP_ERROR            RETURN_HTTP_ERROR

#define EFI_WARN_UNKNOWN_GLYPH     RETURN_WARN_UNKNOWN_GLYPH
#define EFI_WARN_DELETE_FAILURE    RETURN_WARN_DELETE_FAILURE
#define EFI_WARN_WRITE_FAILURE     RETURN_WARN_WRITE_FAILURE
#define EFI_WARN_BUFFER_TOO_SMALL  RETURN_WARN_BUFFER_TOO_SMALL
#define EFI_WARN_STALE_DATA        RETURN_WARN_STALE_DATA
#define EFI_WARN_FILE_SYSTEM       RETURN_WARN_FILE_SYSTEM
#define EFI_WARN_RESET_REQUIRED    RETURN_WARN_RESET_REQUIRED
///@}

///
/// Define macro to encode the status code.
///
#define EFIERR(_a)  ENCODE_ERROR(_a)

#define EFI_ERROR(A)  RETURN_ERROR(A)

///
/// ICMP error definitions
///@{
#define EFI_NETWORK_UNREACHABLE   EFIERR(100)
#define EFI_HOST_UNREACHABLE      EFIERR(101)
#define EFI_PROTOCOL_UNREACHABLE  EFIERR(102)
#define EFI_PORT_UNREACHABLE      EFIERR(103)
///@}

///
/// Tcp connection status definitions
///@{
#define EFI_CONNECTION_FIN      EFIERR(104)
#define EFI_CONNECTION_RESET    EFIERR(105)
#define EFI_CONNECTION_REFUSED  EFIERR(106)
///@}

//
// The EFI memory allocation functions work in units of EFI_PAGEs that are
// 4KB. This should in no way be confused with the page size of the processor.
// An EFI_PAGE is just the quanta of memory in EFI.
//
#define EFI_PAGE_SIZE   SIZE_4KB
#define EFI_PAGE_MASK   0xFFF
#define EFI_PAGE_SHIFT  12

#endif /* _EFI_TYPES_H_ */
