/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_MICRON_V1_H
#define __ZIF_MICRON_V1_H

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
int MICRON_V1();


int MICRON_CheckMD5( unsigned char * hashControl , unsigned char * str , unsigned long strLength);



int MICRON_AllocateMemory(unsigned char ** pointer, int new_size);
int MICRON_FreeMemory(unsigned char ** pointer);

int MICRON_CheckReceivedData(unsigned char * type , int * crcFlag, unsigned int * packageNumber, \
					 int * remainingPackages, int * firstPayloadBytePos, int * packageLength);
int MICRON_SendShortMessage(unsigned int PartNumber, const unsigned char mask);

int MICRON_OutgoingBufferProtect();
int MICRON_OutgoingBufferUnprotect();

void MICRON_GetMeterages(unsigned char mask, unsigned long counterDbId);
void MICRON_GetEnergies( unsigned long counterDbId );
void MICRON_GetJournal( unsigned int mask , unsigned long counterDbId );
void MICRON_GetPQP( unsigned int mask , unsigned long counterDbId );
void MICRON_GetTariffState();
void MICRON_GetPSMState();
void MICRON_GetDiscretOutputState();
void MICRON_GetFirmwareState();
void MICRON_GetTransactionStatistic();
void MICRON_GetPlcState();
void MICRON_GetRfState();
void MICRON_GetSyncStateWord();

int MICRON_Stuffing( unsigned char ** buffer , int * bufferLength );
int MICRON_Gniffuts( unsigned char ** buffer , int * bufferLength );

void MICRON_SendTransparent(unsigned char * sendBuf, int sendSize);
#ifdef __cplusplus
}
#endif

#endif

//EOF
