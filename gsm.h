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
void GSM_RebootStates();
int GSM_ConectionDo(connection_t * connection);
void GSM_ListenerDo(connection_t * connection);
void GSM_WaitSms();
BOOL GSM_CheckTimeToConnect( connection_t * connection );
BOOL GSM_CheckConnectionTimeout( connection_t * connection );
int GSM_Open( connection_t * connection ) ;
int GSM_Init( connection_t * connection );
void GSM_ChoiceSim( connection_t * connection );
int GSM_PowerOnModule();
int GSM_CheckNetworkRegistration( connection_t * connection );
int GSM_CastInitiateAtCommands(connection_t * connection);
BOOL GSM_Check1MinTimeout( connection_t * connection );
int GSM_ConnectAsGprsServer( connection_t * connection );
int GSM_ConnectAsGprsClient( connection_t * connection );

int GSM_Close(connection_t * connection);
int GSM_Write(connection_t * connection, unsigned char * buf, int size, unsigned long * written);
int GSM_Read(connection_t * connection, unsigned char * buf, int size, unsigned long * readed);
int GSM_ReadAvailable(connection_t * connection);
int GSM_SendSms( smsBuffer_t * sms );



int GSM_HandleIncomingBuffer(connection_t * connection);
void GSM_AsyncMessageHandler(unsigned char * foundedPhrase, connection_t * connection);
int GSM_CreateTask( unsigned char * cmd , BOOL emptyCmd , BOOL avalancheOutput , unsigned char ** possibleAnswerBank ) ;
int GSM_WaitTaskAnswer( int timeout ,  unsigned char ** answer , int * answerLength );
void GSM_ClearTask();
int GSM_CmdWrite( unsigned char * cmd , BOOL emptyCmd , int timeout ,unsigned char ** possibleAnswerBank , unsigned char ** answer , int * answerLength);
int GSM_RememberSQ();



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
void GSM_GetDefaultOperatorApn( char * str , const int length , int part );
void GSM_ParseSisoOutput(char * sisoOutput , int sockIdx , char * ownIp , int * ownPort , char * remoteIp, int * remotePort);
#ifdef __cplusplus
}
#endif

#endif

//EOF

