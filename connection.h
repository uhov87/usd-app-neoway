/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_CONNECTION_H
#define __ZIF_CONNECTION_H

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
int CONNECTION_Initialize();
int CONNECTION_RereadConnectionProperties();
void CONNECTION_Stop(unsigned char disconnectReason);
void CONNECTION_Start();

int CONNECTION_ReadAvailable(void);
int CONNECTION_Read(unsigned char * buf, int size, unsigned long * readed);
int CONNECTION_Write(unsigned char * buf, int size, unsigned long * written);
int CONNECTION_ConnectionState();

void CONNECTION_RestartAndLoadNewParametres();

BOOL CONNECTION_CheckTimeToConnect( BOOL * service );
void CONNECTION_ReverseLastServiceAttemptFlag();
void CONNECTION_SetLastSessionProtoTrFlag(BOOL value);

#ifdef __cplusplus
}
#endif

#endif

//EOF

