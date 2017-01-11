/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_CLI_H
#define __ZIF_CLI_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

//exported definitions
//all in "common.h"

//exported types
//all in "common.h"

//exported variables

//exported functions

int CLI_Initialize();
int CLI_InitializeBadNotice();
int CLI_SetErrorCode(int errorCode, int setNotUnset);

//int CLI_LCDMenu(void);

int LCD_SetActCursor();
int LCD_RemoveActCursor();

int LCD_SetWorkCursor();
int LCD_RemoveWorkCursor();

int LCD_SetIPOCursor();
int LCD_RemoveIPOCursor();

int CLI_GetButtonEvent();
int CLI_InitButtons();

#ifdef __cplusplus
}
#endif

#endif

//EOF

