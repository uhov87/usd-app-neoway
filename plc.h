/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_PLC_H
#define __ZIF_PLC_H

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
int PLC_Initialize();
int PLC_InitializePlcBase();
void PLC_TerminatePlcBase();
void PLC_Stop();
void PLC_Start();

void PLC_BreakTask();

BOOL PLC_IsWork(void);
BOOL PLC_IsRun(void);
BOOL PLC_IsStop(void);
BOOL PLC_IsPaused(void);

BOOL PLC_IsNeedToReset(void);
BOOL PLC_IsExternalReset(void);
void PLC_SetNeedToReset(BOOL setVal);
int PLC_GetSNTableTimeout(void);
void PLC_UpdateSNTableTimeout(void);

void PLC_ErrorIncrement();
void PLC_ErrorReset();
void PLC_ProcessTable();
int PLC_AddCounterTask(counterTask_t ** counterTask);
int PLC_RunTask();
int PLC_NextStepOfTask();
BOOL PLC_IsTaskComplete(int * lastWasTimeouted);
int PLC_FreeTask();
int PLC_GetCounterTasksCount();
int PLC_GetCounterTaskByIndex(int taskIndex, counterTask_t ** plcTask);
int PLC_GetTaskIndexByTagNoAtom(int taskTag, int * foundTaskIndex);

void PLC_ProcessIncomingPacket(unsigned char plcPacketType, unsigned char  plcPacketCode, unsigned char * plcPayload, unsigned int plcPacketLen);

int PLC_SendToRemote(unsigned char * serial, unsigned char * buf, unsigned char toSend, int * taskTagAssigned);
int PLC_SendToRemote2(unsigned char * remoteSN, unsigned char * buf, unsigned char size, unsigned char port, int hiSnPart, int * assignedTaskTag);

int PLC_SetupAnswer(unsigned long remoteId, unsigned char * plcPayload, unsigned int plcPayloadSize);
void PLC_SetupResponce(char responceIndex);

int PLC_INFO_TABLE_GetRemotesCount();
int PLC_INFO_TABLE_GetRemoteIdBySn(unsigned char * serial, unsigned int * remoteId);
int PLC_INFO_TABLE_GetRemoteSnById(unsigned int remoteId, unsigned char ** serial);
int PLC_INFO_TABLE_GetRemoteNodeByIndex(unsigned int remoteIndex, plcNode_t * node);
int PLC_INFO_TABLE_GetRemoteNodeBySn(unsigned char * serial, plcNode_t * node);
int PLC_INFO_TABLE_GetRemoteSerialByIndex(unsigned int remoteIndex, unsigned char ** serial);
int PLC_INFO_TABLE_DeleteNode(unsigned int nodeId);
int PLC_INFO_TABLE_Erase();
int PLC_INFO_TABLE_UpdateRemoteNode(plcNode_t * node, int * agedCounters);

int PLC_GetNetowrkId();
unsigned char PLC_GetNetworkMode();

//plc shamaning part
int PLC_LeaveNetworkIndividual(counter_t * counter);

//hal IT 700 functions
unsigned char IT700_GetCrc(unsigned char * buf, int size);
int IT700_WaitAnswer(unsigned char opCode, unsigned char responce);
int IT700_WaitAnswerInt(unsigned char opCode, unsigned char responce, int * value);
int IT700_WaitAnswerLong(unsigned char opCode, unsigned char responce, unsigned char * value);
int IT700_WaitAnswerNode(unsigned char opCode, unsigned char responce, plcNode_t ** node);
int IT700_WaitAnswerVariable(unsigned char opCode, unsigned char responce, unsigned char * value, int waitSize);
int IT700_CheckNodeKey();
int IT700_CheckSerial();
int IT700_GetParam(int paramIndex, int paramTable, int * value);
int IT700_SetParam(int value, int paramIndex, int paramTable);
int IT700_SaveParameters (int paramTable);
int IT700_SetConfig();

int IT700_GetTableEntry(int idx, plcNode_t ** node);
int IT700_GetTable();
int IT700_UpdateTable(int * agedCounters);
int IT700_DeleteNode(unsigned int nodeId);
int IT700_EraseAddressDb();
int IT700_EraseTable();
int IT700_LeaveNetwork() ;

int IT700_Init();
int IT700_GoOnline();
void IT700_Admission(unsigned char * admissionData);
int IT700_SendAdmissionResponce(int result, unsigned char * admissionData);



#ifdef __cplusplus
}
#endif

#endif

//EOF
