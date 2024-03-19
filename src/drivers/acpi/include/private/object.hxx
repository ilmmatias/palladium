/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OBJECT_HXX_
#define _OBJECT_HXX_

#include <value.hxx>

struct AcpipObject {
    char Name[4];
    AcpipValue Value;
};

#endif /* _OBJECT_HXX_ */
