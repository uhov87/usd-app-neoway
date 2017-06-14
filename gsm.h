/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_GSM_H
#define __ZIF_GSM_H

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
void GSM_SetNStatusToDefault();
void GSM_RebootStates();
void GSM_ConectionDo(connection_t * connection);
void GSM_ListenerDo(connection_t * connection);

void GSM_Cmd(unsigned char * cmd, unsigned char * answ, unsigned char ** answerBank, int timeout);
BOOL GSM_CmdByWaitAnswer(neowayCmdWA_t * cmd , unsigned char * answer);

BOOL GSM_CheckNetworkRegistration(connection_t * connection);
BOOL GSM_PowerOn(connection_t * connection);
void GSM_ChoiceSim(connection_t * connection);
BOOL GSM_InitCmds();
BOOL GSM_WaitAfterStart(connection_t * connection);
BOOL GSM_Open(connection_t * connection);
void GSM_Close(connection_t * connection);

BOOL GSM_OpenByCsdOutgoing(connection_t * connection);
BOOL GSM_OpenByCsdIncoming();

BOOL GSM_SetListenPort(connection_t * connection);
BOOL GSM_OpenByClient(connection_t * connection);
BOOL GSM_IsTimeToConnect(connection_t * connection);
int GSM_IsTimeToDisconnect(connection_t * connection);
BOOL GSM_IsModuleInCsd(connection_t * connection);

void GSM_SetNetApnStr(connection_t * connection, unsigned char * str, int length);
void GSM_SetTcpSetupStr(connection_t * connection, unsigned char * str, int length);

void GSM_PutIncMessageToReadBuf();

int GSM_Write(connection_t * connection, unsigned char * buf, int size, unsigned long * written);
int GSM_Read(connection_t * connection, unsigned char * buf, int size, unsigned long * readed);
int GSM_ReadAvailable(connection_t * connection);

void GSM_GetOwnIp(char * ip);
int GSM_GetIMEI(char * imeiString, const unsigned int imeiStringMaxLength);
int GSM_GetSignalQuality();
int GSM_ConnectionState();
int GSM_GetGsmOperatorName(char * gsmOpName, const unsigned int gsmOpNameMaxLength);
int GSM_GetCurrentConnectionTime();
unsigned int GSM_GetConnectionsCounter();
int GSM_GetCurrentSimNumber();
int GSM_ConnectionCounterIncrement();
BOOL GSM_GetServiceModeStat();

void GSM_GetPlain(char * buf, char * phone, char * message);
void GSM_GetPdu(char * message, char * phone, char * result, int * length);
void GSM_SendSms(smsBuffer_t * smsBuffer);
void GSM_SmsHandler();
#ifdef __cplusplus
}
#endif

#endif

//EOF

