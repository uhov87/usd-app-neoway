/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_POOL_H
#define __ZIF_POOL_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

//exported definitions
//all in "common.h"
#define POOL_ID_485 0
#define POOL_ID_PLC 1

//exported types
//all in "common.h"

//exported variables
extern int appendTimeout;

//exported functions
int POOL_Initialize();
void POOL_StopPlc();
void POOL_StartPlc();
BOOL POOL_IsWorkPlc();
BOOL POOL_IsRunPlc();

void POOL_Stop485();
void POOL_Start485();
BOOL POOL_IsWork485();
BOOL POOL_IsRun485();

void POOL_Pause();
void POOL_Continue();

int POOL_ProcessCountersOn485();
int POOL_Create485Task();
int POOL_Run485Task();
int POOL_Save485TaskResults();
int POOL_Free485Task();

int POOL_ProcessCountersOnPlc(int * lastWasTimeouted);
int POOL_CreatePlcTask();
int POOL_RunPlcTask();
int POOL_WaitPlcTaskComplete(int * lastWasTimeouted);
int POOL_SavePlcTaskResults();
int POOL_ResetBeforeFreePlcTask();
int POOL_BeforeFreePlcTask();
int POOL_FreePlcTask();

int POOL_ErrorHandle(int ErrorCode, int poolId);

void POOL_GetStatusOfPlc(char * p);
void POOL_GetStatusOf485(char * p);

int POOL_TurnTransparentOn(unsigned long counterDbId);
void POOL_TurnTransparentOff(); //by direct command
void POOL_TurnTransparentOffConnDie(); //by connection closed
void POOL_TurnTransparentOffByTimeout(); //by poller after calculating timeout
BOOL POOL_GetTransparent(unsigned long * counterDbId); //if TRUE counterDbId contains counter cfg DbId value
BOOL POOL_GetTransparentPlc(); //used for plc stack
BOOL POOL_GetTransparent485();
BOOL POOL_InTransparent();
int POOL_WriteTransparent(unsigned char * sendBuf, int size);

void POOL_WaitPlcLoopEnds();
void POOL_ResumePlcLoop();

void POOL_Wait485LoopEnds();
void POOL_Resume485Loop();

void POOL_SetTransparentPlcAnswer(unsigned long remoteId, unsigned char * plcPayload, unsigned int plcPayloadSize);

void POOL_LockAlternativeAtom();
void POOL_UnLockAlternativeAtom();

#ifdef __cplusplus
}
#endif

#endif

//EOF
