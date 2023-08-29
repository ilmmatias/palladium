/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KE_H_
#define _KE_H_

void KiRunBootStartDrivers(void *LoaderData);

[[noreturn]] void KeFatalError(int Message);

#endif /* _KE_H_ */
