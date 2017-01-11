/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_ETH_H
#define __ZIF_ETH_H

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
int ETH_ConectionDo(connection_t * connection);
int ETH_SetListener(connection_t * connection);
int ETH_Open(connection_t * connection);
/*int ETH_OpenListener(connection_t * connection);*/
int ETH_Close(connection_t * connection);
int ETH_CheckTimings(connection_t * connection);
int ETH_Write(connection_t * connection, unsigned char * buf, int size, unsigned long * written);
int ETH_Read(connection_t * connection, unsigned  char * buf, int size, unsigned long * readed);
int ETH_ReadAvailable(connection_t * connection);

//int ETH_UpdateConfigFiles(connection_t * connection);
//int ETH_CheckConfigFiles(connection_t * connection);
int ETH_ReadAvailable(connection_t * connection);

int ETH_EthSettings();

int ETH_ConnectionState();
int ETH_GetCurrentConnectionTime(void);
unsigned int ETH_GetConnectionsCounter(void);

void ETH_RebootStates();

int ETH_CheckResolv(connection_t * connection);
int ETH_UpdateResolv(connection_t * connection);
int ETH_CheckInterfaces(connection_t * connection);
int ETH_UpdateInterfaces(connection_t * connection);

BOOL ETH_GetServiceModeStat();

#ifdef __cplusplus
}
#endif

#endif

//EOF

