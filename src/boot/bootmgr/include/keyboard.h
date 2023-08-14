/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#define KEY_UNKNOWN 0x0000
#define KEY_ESC 0x1000
#define KEY_LEFT 0x1001
#define KEY_RIGHT 0x1002
#define KEY_UP 0x1003
#define KEY_DOWN 0x1004
#define KEY_INS 0x1005
#define KEY_DEL 0x1006
#define KEY_HOME 0x1007
#define KEY_END 0x1008
#define KEY_PGUP 0x1009
#define KEY_PGDOWN 0x100A
#define KEY_F1 0x1010
#define KEY_F2 0x1011
#define KEY_F3 0x1012
#define KEY_F4 0x1013
#define KEY_F5 0x1014
#define KEY_F6 0x1015
#define KEY_F7 0x1016
#define KEY_F8 0x1017
#define KEY_F9 0x1018
#define KEY_F10 0x1019
#define KEY_F11 0x101A
#define KEY_F12 0x101B

void BmInitKeyboard(void);
int BmPollKey(void);

#endif /* _KEYBOARD_H_ */
