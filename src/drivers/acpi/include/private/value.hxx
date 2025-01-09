/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _VALUE_HXX_
#define _VALUE_HXX_

#include <cxx/array.hxx>
#include <cxx/list.hxx>
#include <cxx/ptr.hxx>

struct AcpipObject;

enum class AcpipValueType : uint64_t {
    Uninitialized = 0,
    Integer,
    String,
    Buffer,
    Package,
    FieldUnit,
    Device,
    Event,
    Method,
    Mutex,
    OperationRegion,
    PowerResource,
    Processor,
    ThermalZone,
    BufferField,
    DebugObject = 16,
    Scope,
};

struct AcpipValue {
    AcpipValueType Type;
    uint64_t Integer;
    ScopeArray<char> String;
    ScopeArray<uint8_t> Buffer;
    SList<AutoPtr<AcpipObject>> Children{"Acpi"};
};

#endif /* _VALUE_HXX_ */
