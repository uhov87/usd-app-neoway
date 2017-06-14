/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>



#include "common.h"
#include "storage.h"
#include "serial.h"
#include "connection.h"
#include "gsm.h"
#include "pio.h"
#include "cli.h"
#include "debug.h"

//
//
//

enum connectionState_t
{
	GCS_POWERON ,	
	GCS_INIT ,
	GCS_WAIT ,
	GCS_CHECK_TIME_TO_CONNECT ,
	GCS_OPEN_CONNECTION ,
	GCS_PROCESS_CONNECTION ,
	GCS_CLOSE_CONNECTION

};

enum listenerState_t
{
	GLS_WAIT_BYTE ,
	GLSTATE_HANDLE_BUFFER ,
	GLS_CLEAR_BUFFER
};

enum taskStatus_e
{
	GTS_EMPTY,
	GTS_WAIT ,
	GTS_READY
};



static int gsmListenerState = GLS_WAIT_BYTE ;
static int gsmConnectionState = GCS_POWERON ;

static neowayStatus_t neowayStatus ;
static neowayTcpReadBuf_t neowayTcpReadBuf;
static pthread_mutex_t neowayStatusMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t connectionCounterMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tcpReadBufMutex = PTHREAD_MUTEX_INITIALIZER;

unsigned char * answerBankStandart[] = { (unsigned char *)"\r\nOK\r\n",
										(unsigned char *)"\r\nERROR\r\n" ,
										(unsigned char *)"\r\n+CME ERROR",
										(unsigned char *)"\r\n+CMS ERROR",
										NULL } ;

unsigned char * answerBankSendSms[] = { (unsigned char *)">",
										(unsigned char *)"\r\nERROR\r\n" ,
										(unsigned char *)"\r\n+CME ERROR",
										(unsigned char *)"\r\n+CMS ERROR",
										NULL } ;

unsigned char * answerBankCsdCall[] = { (unsigned char *)"\r\nCONNECT 9600",
										(unsigned char *)"\r\nBUSY",
										(unsigned char *)"\r\nNO CARRIER"
										};

unsigned char * answerBankTcpSetup[] = { (unsigned char *)",OK\r\n",
										(unsigned char *)"Listening...",
										(unsigned char *)",FAIL\r\n" ,
										(unsigned char *)",ERROR\r\n" ,
										(unsigned char *)"\r\n+CME ERROR",
										NULL } ;

unsigned char * answerBankCloseClient[] = { (unsigned char *)"+CLOSECLIENT:",
										NULL } ;

void GSM_Protect(pthread_mutex_t * mutex)
{
	pthread_mutex_lock(mutex);
}

//
//
//

void GSM_Unprotect(pthread_mutex_t * mutex)
{
	pthread_mutex_unlock(mutex);
}

//
//
//

void GSM_SetNStatusToDefault()
{
	GSM_Protect( &neowayStatusMutex ) ;

	GSM_Protect( &tcpReadBufMutex ) ;
	neowayTcpReadBuf.size = 0 ;
	memset(neowayTcpReadBuf.buf, 0 , GSM_INPUT_BUF_SIZE);
	GSM_Unprotect( &tcpReadBufMutex ) ;

	neowayStatus.power = FALSE ;
	neowayStatus.currentSim = SIM2 ;
	neowayStatus.sq = -1 ;
	neowayStatus.smsStorSize = 0 ;
	neowayStatus.simPresence = FALSE ;
	neowayStatus.serviceMode = FALSE ;
	neowayStatus.dialState = GDS_OFFLINE ;
	memset(neowayStatus.imei, 0 , GSM_IMEI_LENGTH + 1 );	
	memset(neowayStatus.ip, 0 , LINK_SIZE);

	neowayStatus.currSock = 0 ;
	neowayStatus.hangSock = 0 ;

	memset(&neowayStatus.onlineDts, 0 , sizeof(dateTimeStamp));
	memset(&neowayStatus.lastDataDts, 0 , sizeof(dateTimeStamp));
	memset(&neowayStatus.powerOnDts, 0 , sizeof(dateTimeStamp));

	memset (neowayStatus.neowayBuffer.buf , 0 ,GSM_INPUT_BUF_SIZE ) ;
	neowayStatus.neowayBuffer.size = 0 ;

	// task
	neowayStatus.task.tx = NULL ;
	neowayStatus.task.status = GTS_EMPTY ;
	memset(neowayStatus.task.rx, 0 , GSM_TASK_RX_SIZE ) ;	
	neowayStatus.task.possibleAnswerBank = NULL ;

	GSM_Unprotect( &neowayStatusMutex ) ;
}

//
//
//

void GSM_RebootStates()
{
	DEB_TRACE(DEB_IDX_GSM);

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_RebootStates start");

	gsmConnectionState = GCS_POWERON ;
	gsmListenerState = GLS_WAIT_BYTE ;

	GSM_SetNStatusToDefault();

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_RebootStates stop");
}

//
//
//

void GSM_ConectionDo( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	switch (gsmConnectionState)
	{
		case GCS_POWERON:
		{
			LCD_RemoveIPOCursor();
			if ( GSM_PowerOn(connection) )
				gsmConnectionState = GCS_INIT ;
		}
		break;

		case GCS_INIT:
		{
			GSM_InitCmds();
			if ( GSM_CheckNetworkRegistration(connection) )
				gsmConnectionState = GCS_WAIT ;
		}
		break;

		case GCS_WAIT:
		{			
			//wait a minute
			if ( GSM_WaitAfterStart(connection) )
				gsmConnectionState = GCS_CHECK_TIME_TO_CONNECT ;
		}
		break;

		case GCS_CHECK_TIME_TO_CONNECT:
		{
			if ( GSM_IsTimeToConnect(connection) )
				gsmConnectionState = GCS_OPEN_CONNECTION ;
		}
		break;

		case GCS_OPEN_CONNECTION:
		{
			gsmConnectionState = GCS_POWERON ;
			if ( GSM_Open(connection) )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				BOOL serviceMode;

				gsmConnectionState = GCS_PROCESS_CONNECTION ;

				GSM_Protect( &neowayStatusMutex ) ;
				serviceMode = neowayStatus.serviceMode ;
				GSM_Unprotect( &neowayStatusMutex ) ;

				//save journal event
				COMMMON_CreateDescriptionForConnectEvent(evDesc , connection , serviceMode);
				STORAGE_JournalUspdEvent( EVENT_CONNECTED_TO_SERVER , evDesc );

				//increment conection counter
				GSM_ConnectionCounterIncrement();

				//set up cli
				LCD_SetIPOCursor();
				CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_CLEAR_ERROR);
				CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, CLI_CLEAR_ERROR);

				CONNECTION_SetLastSessionProtoTrFlag(FALSE);
				if ( (connection->mode == CONNECTION_GPRS_SERVER) && (serviceMode == FALSE))
				{
					CONNECTION_SetLastSessionProtoTrFlag(TRUE);
				}
			}
			else
				CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, CLI_SET_ERROR);
		}
		break;

		case GCS_PROCESS_CONNECTION:
		{
			//COMMON_STR_DEBUG(DEBUG_GSM "Process connection");
			gsmConnectionState = GSM_IsTimeToDisconnect(connection);
		}
		break;

		case GCS_CLOSE_CONNECTION:
		{
			LCD_RemoveIPOCursor();
			GSM_Close(connection);
			gsmConnectionState = GCS_CHECK_TIME_TO_CONNECT ;
		}
		break;

	}

	return ;
}

//
//
//

void GSM_ListenerDo( connection_t * connection /*__attribute__((unused))*/)
{
	DEB_TRACE(DEB_IDX_GSM);

	switch( gsmListenerState )
	{
		case GLS_WAIT_BYTE:
		{
			unsigned char byte;
			int r = 0 ;

//			GSM_Protect( &neowayStatusMutex ) ;

//			if (neowayStatus.dialState == GDS_ONLINE)
//			{
//				if ((connection->mode == CONNECTION_CSD_INCOMING) ||
//						(connection->mode == CONNECTION_CSD_OUTGOING))
//				{
//					//
//					GSM_Unprotect( &neowayStatusMutex ) ;
//					return ;
//				}
//			}

//			GSM_Unprotect( &neowayStatusMutex ) ;


			SERIAL_Read(PORT_GSM , &byte , 1 , &r);
			if (!r)
			{
				COMMON_Sleep(50);
				return ;
			}

			GSM_Protect( &neowayStatusMutex ) ;

			if (GSM_IsModuleInCsd(connection))
			{
				//data mode. Do not try to handle buffer, put byte to read buf
				GSM_Protect( &tcpReadBufMutex ) ;

				if (neowayTcpReadBuf.size < GSM_INPUT_BUF_SIZE)
					neowayTcpReadBuf.buf[ neowayTcpReadBuf.size++ ] = byte ;

				GSM_Unprotect( &tcpReadBufMutex ) ;
			}
			else
			{
				neowayStatus.neowayBuffer.buf[ neowayStatus.neowayBuffer.size++ ] = byte ;

				if ( ((byte == '\n') || (byte == '>')) && (neowayStatus.neowayBuffer.size > 2))
					gsmListenerState = GLSTATE_HANDLE_BUFFER ;
				else if (((byte == '\0') || (byte == ' ') )&& (neowayStatus.neowayBuffer.size == 1))
					gsmListenerState = GLS_CLEAR_BUFFER ;
			}

			GSM_Unprotect( &neowayStatusMutex ) ;
		}
		break;

		case GLSTATE_HANDLE_BUFFER:
		{
			BOOL taskAnswer = FALSE ;
			gsmListenerState = GLS_CLEAR_BUFFER ;

			GSM_Protect( &neowayStatusMutex ) ;

			//COMMON_STR_DEBUG( DEBUG_GSM "GSM handle message[%d]: [%s]", neowayStatus.neowayBuffer.size, neowayStatus.neowayBuffer.buf );
			//COMMON_BUF_DEBUG( DEBUG_GSM "GSM", neowayStatus.neowayBuffer.buf, neowayStatus.neowayBuffer.size );

			if ( neowayStatus.task.status == GTS_WAIT )
			{
				//COMMON_STR_DEBUG( DEBUG_GSM "GSM Task waiting for cmd [%s]", neowayStatus.task.tx );
				if (!neowayStatus.task.tx)
				{
					taskAnswer = TRUE ;
				}
				else if ( !strncmp((char *)neowayStatus.task.tx, (char *)neowayStatus.neowayBuffer.buf, strlen((char *)neowayStatus.task.tx) ) )
				{
					taskAnswer = TRUE ;
				}
				//COMMON_STR_DEBUG(DEBUG_GSM "GSM: task tx [%s], neowayBuffer [%s]", neowayStatus.task.tx, neowayStatus.neowayBuffer.buf);
			}

			if (taskAnswer)
			{
				BOOL fullAnswer = COMMON_CheckPhrasePresence(neowayStatus.task.possibleAnswerBank,
															neowayStatus.neowayBuffer.buf,
															strlen((char *)neowayStatus.neowayBuffer.buf),
															NULL);
				if (fullAnswer)
				{
					memcpy( neowayStatus.task.rx, neowayStatus.neowayBuffer.buf, GSM_TASK_RX_SIZE );
					neowayStatus.task.status = GTS_READY ;
				}
				else
				{
					gsmListenerState = GLS_WAIT_BYTE ;
				}
			}
			else
			{
				//async message
				if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+EIND: 1\r\n", 12) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM neoway start");
					neowayStatus.power = TRUE ;
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+EIND", 7) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM EIND message found");
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+EUSIM:", 9) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Sim is available");
					neowayStatus.simPresence = TRUE ;
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+STKPCI:", 10) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM STKPCI async was detected");
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+TCPCLOSE:", 12) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM connection was closed");
					neowayStatus.dialState = GDS_OFFLINE ;
				}				
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+TCPRECV:", 11) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM [%s]", (char *)neowayStatus.neowayBuffer.buf);
					GSM_PutIncMessageToReadBuf();
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "+TCPRECV(S):", 12) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM [%s]", (char *)neowayStatus.neowayBuffer.buf);
					GSM_PutIncMessageToReadBuf();
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\nConnect AcceptSocket", 22) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM [%s]", (char *)neowayStatus.neowayBuffer.buf);
					char ip[LINK_SIZE] = {0};
					unsigned int port = 0, sock = 0 ;

					sscanf((char*)neowayStatus.neowayBuffer.buf, "\r\nConnect AcceptSocket=%u,ClientAddr=%" DEF_TO_XSTR(LINK_SIZE) "[0123456789.],ClientPort=%u"  , &sock, ip, &port);
					COMMON_STR_DEBUG( DEBUG_GSM "GSM accepted connection [%u] from [%s] : [%u]", sock, ip, port);

					if ( neowayStatus.dialState == GDS_WAIT )
					{
						neowayStatus.dialState = GDS_NEED_TO_ANSW ;
						neowayStatus.currSock = sock ;

						connection->clientPort = port ;
						strncpy( (char *)connection->clientIp, ip, LINK_SIZE );
					}
					else if (neowayStatus.dialState == GDS_ONLINE)
					{
						neowayStatus.dialState = GDS_NEED_TO_HANG ;
						neowayStatus.hangSock = sock ;
					}

				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+CLOSECLIENT:", 15) )
				{
					unsigned int sock = 0 ;
					sscanf((char*)neowayStatus.neowayBuffer.buf, "\r\n+CLOSECLIENT:%u", &sock);

					if ( neowayStatus.currSock == sock )
					{
						neowayStatus.dialState = GDS_WAIT ;
					}
				}
				else if ( !strncmp((char *)neowayStatus.neowayBuffer.buf , "\r\n+CLIP", 7) )
				{
					char incPhone[PHONE_MAX_SIZE] = {0};
					unsigned char * allowedPhone1 = NULL ;
					unsigned char * allowedPhone2 = NULL ;
					incPhone[0] = '+' ;

					sscanf((char*)neowayStatus.neowayBuffer.buf, "\r\n+CLIP: \"%" DEF_TO_XSTR(PHONE_MAX_SIZE) "[0-9]\"", &incPhone[1]);

					COMMON_STR_DEBUG( DEBUG_GSM "GSM Incoming call from phone [%s]", incPhone );

					if ( connection->mode == CONNECTION_CSD_INCOMING && neowayStatus.dialState == GDS_WAIT)
					{
						if (neowayStatus.currentSim == SIM1)
						{
							allowedPhone1 = connection->allowedIncomingNum1Sim1 ;
							allowedPhone2 = connection->allowedIncomingNum2Sim1 ;
						}
						else
						{
							allowedPhone1 = connection->allowedIncomingNum1Sim2 ;
							allowedPhone2 = connection->allowedIncomingNum2Sim2 ;
						}

						if (!strlen((char*)allowedPhone1) && !strlen((char*)allowedPhone2))
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM Both allowed numbers for current sim are empty. Answer the call");
							neowayStatus.dialState = GDS_NEED_TO_ANSW ;
						}
						else if ( !strcmp((char *)allowedPhone1, incPhone) )
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM it is all from allowed phone 1. Answer the call");
							neowayStatus.dialState = GDS_NEED_TO_ANSW ;
						}
						else if ( !strcmp((char *)allowedPhone2, incPhone) )
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM it is all from allowed phone 2. Answer the call");
							neowayStatus.dialState = GDS_NEED_TO_ANSW ;
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM there is no such number in white list of phone numbers. Hang the call");
							neowayStatus.dialState = GDS_NEED_TO_HANG ;
						}
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Unknown async message[%d]: [%s]", neowayStatus.neowayBuffer.size, neowayStatus.neowayBuffer.buf );
				}
			}





			GSM_Unprotect( &neowayStatusMutex ) ;

		}
		break;

		case GLS_CLEAR_BUFFER:
		{
			GSM_Protect( &neowayStatusMutex ) ;

			memset (neowayStatus.neowayBuffer.buf , 0 ,GSM_INPUT_BUF_SIZE ) ;
			neowayStatus.neowayBuffer.size = 0 ;
			gsmListenerState = GLS_WAIT_BYTE ;

			GSM_Unprotect( &neowayStatusMutex ) ;
		}
		break;

	}

	return ;
}

//
//
//

BOOL GSM_PowerOn(connection_t * connection)
{	
	DEB_TRACE(DEB_IDX_GSM);

	int poweronTimeout = GSM_POWERON_TIMEOUT ;
	int currentSim ;
	BOOL simPresence = FALSE, power = FALSE, pinUsage = FALSE ;
	unsigned char pinCmd[32] = {0};
	unsigned int pin ;
	unsigned char evDesc[EVENT_DESC_SIZE] = {0};

	GSM_ChoiceSim(connection);

	GSM_Protect( &neowayStatusMutex ) ;

	memset(&neowayStatus.onlineDts, 0 , sizeof(dateTimeStamp));
	memset(&neowayStatus.lastDataDts, 0 , sizeof(dateTimeStamp));
	neowayStatus.power = FALSE ;
	neowayStatus.dialState = GDS_OFFLINE ;
	neowayStatus.simPresence = FALSE ;
	currentSim = neowayStatus.currentSim ;
	//neowayStatus.sq = -1 ;

	pin = connection->gsmCpinProperties[0].pin ;
	pinUsage = connection->gsmCpinProperties[0].usage ;
	if (currentSim == SIM2)
	{
		pin = connection->gsmCpinProperties[1].pin ;
		pinUsage = connection->gsmCpinProperties[1].usage ;
		evDesc[0] = 0x01 ;
	}

	GSM_Unprotect( &neowayStatusMutex ) ;

	//process power off

	if ( PIO_SetValue(PIO_GSM_NOT_OE, 1) != OK )
		return FALSE ;

	if ( PIO_SetValue(PIO_GSM_PWR, 0) != OK )
		return FALSE ;

	if ( PIO_SetValue(PIO_GSM_POKIN, 0) != OK )
		return FALSE ;

	COMMON_Sleep(SECOND * 7);

	//process power on
	if ( PIO_SetValue(PIO_GSM_SIM_SEL, currentSim ) != OK )
		return FALSE ;

	if ( PIO_SetValue(PIO_GSM_NOT_OE, 0) != OK )
		return FALSE ;

	if ( PIO_SetValue(PIO_GSM_PWR, 1) != OK )
		return FALSE ;

	COMMON_Sleep(SECOND);

	if ( PIO_SetValue(PIO_GSM_POKIN, 1) != OK )
		return FALSE ;

	//wait sisstart received
	while ( (poweronTimeout--) > 0 )
	{
		GSM_Protect( &neowayStatusMutex ) ;

		power = neowayStatus.power ;
		simPresence = neowayStatus.simPresence ;

		GSM_Unprotect( &neowayStatusMutex ) ;



		if (power)
		{
			if (!simPresence)
				return FALSE ;

			COMMON_STR_DEBUG( DEBUG_GSM "GSM Power on" );
			CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_CLEAR_ERROR);
			return TRUE ;
		}

		COMMON_Sleep(SECOND);
	}



	// enter pin code
	if (simPresence && !power)
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM check pin is needed");
		neowayCmdWA_t askPin = {(unsigned char *)"AT+CPIN?\r", (unsigned char *)"+CPIN: SIM PIN", answerBankStandart , 5};
		neowayCmdWA_t setPin = { pinCmd, answerBankStandart[0], answerBankStandart , 5};
		snprintf((char *)pinCmd, 32, "AT+CPIN=\"%u\"\r", pin);

		if ( GSM_CmdByWaitAnswer(&askPin, NULL) )
		{
			if (!pinUsage)
			{
				STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , evDesc );
				return FALSE ;
			}

			if ( !GSM_CmdByWaitAnswer(&setPin, NULL) )
			{
				STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , evDesc );
				return FALSE ;
			}

			//wait EIND:1 indication
			poweronTimeout = GSM_POWERON_TIMEOUT ;
			while ( (poweronTimeout--) > 0 )
			{
				GSM_Protect( &neowayStatusMutex ) ;
				power = neowayStatus.power ;
				GSM_Unprotect( &neowayStatusMutex ) ;

				if (power)
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Power on" );
					CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_CLEAR_ERROR);
					return TRUE ;
				}

				COMMON_Sleep(SECOND);
			} //while ( (poweronTimeout--) > 0 )

		}
		else
		{
			STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , evDesc );
		}
	}
	else if (!simPresence && !power)
	{
		CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_SET_ERROR);
		STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_INIT_ERROR , NULL );
	}

	return FALSE ;
}

//
//
//

void GSM_ChoiceSim(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	GSM_Protect( &neowayStatusMutex ) ;

	switch (connection->simPriniciple)
	{
		case SIM_FIRST :
			neowayStatus.currentSim = SIM1 ;
			break ;

		case SIM_SECOND :
			neowayStatus.currentSim = SIM2 ;
			break ;

		case SIM_FIRST_THEN_SECOND:
		case SIM_SECOND_THEN_FIRST:
		{
			if (neowayStatus.currentSim == SIM1)
				neowayStatus.currentSim = SIM2 ;
			else
				neowayStatus.currentSim = SIM1 ;
		}
		break;
	}

	GSM_Unprotect( &neowayStatusMutex ) ;

	return ;
}

//
//
//

void GSM_Cmd(unsigned char * cmd, unsigned char * answ, unsigned char ** answerBank, int timeout)
{
	DEB_TRACE(DEB_IDX_GSM);

	const int checkInterval = 500 ; //ms
	int w, status, timer = timeout * SECOND / checkInterval;

	//set task
	GSM_Protect( &neowayStatusMutex ) ;	
	neowayStatus.task.possibleAnswerBank = answerBank ;
	neowayStatus.task.status = GTS_WAIT ;
	neowayStatus.task.tx = cmd ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	SERIAL_Write(PORT_GSM, cmd , strlen((char *)cmd), &w);
	//wait task ready
	while ( timer )
	{
		GSM_Protect( &neowayStatusMutex ) ;
		status = neowayStatus.task.status;
		GSM_Unprotect( &neowayStatusMutex ) ;

		if ( status == GTS_READY )
		{
			break;
		}

		COMMON_Sleep(checkInterval);
		timer-- ;
	}

	GSM_Protect( &neowayStatusMutex ) ;

	//handle task result
	memcpy(answ, neowayStatus.task.rx, GSM_TASK_RX_SIZE) ;

	// clear task
	neowayStatus.task.status = GTS_EMPTY;
	neowayStatus.task.tx = NULL ;	
	memset(neowayStatus.task.rx, 0 , GSM_TASK_RX_SIZE ) ;
	neowayStatus.task.possibleAnswerBank = NULL ;

	GSM_Unprotect( &neowayStatusMutex ) ;
}

//
//
//

BOOL GSM_CmdByWaitAnswer(neowayCmdWA_t * cmd, unsigned char * answer )
{
	DEB_TRACE(DEB_IDX_GSM);

	unsigned char ansBuf[GSM_TASK_RX_SIZE] ;
	memset(ansBuf, 0 , GSM_TASK_RX_SIZE);

	GSM_Cmd(cmd->cmd, ansBuf, cmd->answerBank, cmd->timeout);

	COMMON_STR_DEBUG(DEBUG_GSM "%s", ansBuf);

	if ( strstr((char *)ansBuf, (char *)cmd->waitAnsw) )
	{
		if (answer)
			memcpy(answer, ansBuf, GSM_TASK_RX_SIZE);
		return TRUE ;
	}

	return FALSE ;
}

//
//
//

void GSM_GetOpNameAndSq(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	unsigned char answer[GSM_TASK_RX_SIZE];
	neowayCmdWA_t opNameStr = {(unsigned char *)"AT+COPS?\r", answerBankStandart[0], answerBankStandart , 5};
	neowayCmdWA_t getSqStr = {(unsigned char *)"AT+CSQ\r", answerBankStandart[0], answerBankStandart , 5};
	neowayCmdWA_t getImsiStr = {(unsigned char *)"AT+CIMI\r", answerBankStandart[0], answerBankStandart , 5};

	unsigned char opName[GSM_OPERATOR_NAME_LENGTH] = {0};
	unsigned int sq = 0 ;
	char imsi[32] = {0};
	BOOL needToSendImsiSms = FALSE ;
	int sim = 1;


	memset(answer, 0 , GSM_TASK_RX_SIZE );
	if ( !GSM_CmdByWaitAnswer(&opNameStr, answer) )
		return ;

	//parse op name
	sscanf((char *)answer, "%*s\r\r\n+COPS: %*d,%*d,\"%" DEF_TO_XSTR(GSM_OPERATOR_NAME_LENGTH) "[A-Z,a-z,0-9, ]\"\r\n", opName);
	if ( opName[GSM_OPERATOR_NAME_LENGTH - 1] != '\0' )
		opName[GSM_OPERATOR_NAME_LENGTH - 1] = '\0';



	memset(answer, 0 , GSM_TASK_RX_SIZE );
	if ( !GSM_CmdByWaitAnswer(&getSqStr, answer) )
		return ;

	//parse sq
	sscanf((char *)answer, "%*s\r\r\n+CSQ: %u, %*d", &sq);
	sq = sq * 100 / 31 ;



	memset(answer, 0 , GSM_TASK_RX_SIZE );
	if ( !GSM_CmdByWaitAnswer(&getImsiStr, answer) )
		return ;

	//parse imsi
	sscanf((char *)answer, "%*s\r\r\n%31[0-9]", imsi);


	GSM_Protect( &neowayStatusMutex ) ;

	if ( neowayStatus.currentSim == SIM2)
		sim = 2;

	strcpy( neowayStatus.opName, (char *)opName );
	neowayStatus.sq = sq ;

	GSM_Unprotect( &neowayStatusMutex ) ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM parsed op name [%s], sq [%u%%], imsi [%s]", opName, sq, imsi);

	STORAGE_UpdateImsi( sim , imsi , &needToSendImsiSms);
	if (needToSendImsiSms)
	{
		smsBuffer_t smsBuffer;
		char sn[32];

		memset(sn , 0 , 32);
		memset(&smsBuffer, 0, sizeof(smsBuffer_t));

		COMMON_GET_USPD_SN((unsigned char *)sn);

		snprintf( (char *)smsBuffer.textMessage, SMS_TEXT_LENGTH, "USPD %s sim %d imsi %s" , sn , sim , imsi );
		snprintf( (char *)smsBuffer.phoneNumber, PHONE_MAX_SIZE , "%s" , connection->smsReportPhone );

		GSM_SendSms(&smsBuffer);
	}
}

//
//
//

BOOL GSM_CheckNetworkRegistration(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	neowayCmdWA_t cmd = {(unsigned char *)"AT+CREG?\r", (unsigned char *)"CREG: 0,1", answerBankStandart , 5};
	const int checkInterval = 3000 ; //ms
	int timer = GSM_NETWORK_REG_TIMEOUT * SECOND / checkInterval;

	while(timer)
	{
		if ( GSM_CmdByWaitAnswer(&cmd, NULL) )
		{
			COMMON_STR_DEBUG(DEBUG_GSM "GSM Network registration success");
			GSM_GetOpNameAndSq(connection);
			return TRUE ;
		}
		timer--;
		COMMON_Sleep(checkInterval);
	}

	COMMON_STR_DEBUG(DEBUG_GSM "GSM Network registration fail");
	return FALSE ;
}

//
//
//

BOOL GSM_InitCmds()
{
	DEB_TRACE(DEB_IDX_GSM);

	char ansBuf[GSM_TASK_RX_SIZE];
	char * ansBufPtr = NULL ;
	neowayCmdWA_t cmdArr[] = {{(unsigned char *)"AT+CGSN\r", answerBankStandart[0] , answerBankStandart , 5},
							{(unsigned char *)"AT+CMEE=2\r", answerBankStandart[0] , answerBankStandart , 5},
							{(unsigned char *)"AT+CMGF=0\r", answerBankStandart[0] , answerBankStandart , 5},
							{(unsigned char *)"AT+CBST=71,,1\r", answerBankStandart[0] , answerBankStandart , 5},							
							{(unsigned char *)"AT+CPMS=\"SM\",\"SM\",\"SM\"\r", answerBankStandart[0] , answerBankStandart , 5}	};

	int i, cmdSize = sizeof(cmdArr) / sizeof(neowayCmdWA_t) ;

	enum parsingState_e
	{
		PS_NONE,
		PS_IMEI,		
		PS_SMS_STOR
	};
	int parsingState = PS_NONE ;
	char imei[GSM_IMEI_LENGTH + 1] = {0};	
	unsigned int smsStorSize = 0 ;


	for ( i = 0 ; i < cmdSize; i++)
	{
		ansBufPtr = NULL ;
		parsingState = PS_NONE ;
		if ( !strcmp( (char *)cmdArr[i].cmd, (char *)"AT+CGSN\r") )
		{
			ansBufPtr = ansBuf ;
			memset(ansBuf, 0 , GSM_TASK_RX_SIZE );
			parsingState = PS_IMEI ;
		}
		else if ( !strncmp( (char *)cmdArr[i].cmd, (char *)"AT+CPMS", 7) )
		{
			ansBufPtr = ansBuf ;
			memset(ansBuf, 0 , GSM_TASK_RX_SIZE );
			parsingState = PS_SMS_STOR ;
		}	

		if ( !GSM_CmdByWaitAnswer( &cmdArr[i], (unsigned char *)ansBufPtr ) )
		{
			COMMON_STR_DEBUG(DEBUG_GSM "GSM Error entering init cmd [%s]", cmdArr[i].cmd);
			return FALSE ;
		}

		switch(parsingState)
		{
			case PS_IMEI:
				sscanf(ansBuf, "%*s\r\r\n%" DEF_TO_XSTR(GSM_IMEI_LENGTH) "[0-9]", imei);
				break;

			case PS_SMS_STOR:
				sscanf(ansBuf, "%*s\r\r\n+CPMS: %*d, %u", &smsStorSize);
				break;

			default:
			case PS_NONE:
				break;
		}

	}

	GSM_Protect( &neowayStatusMutex ) ;

	strcpy( neowayStatus.imei, imei );	
	neowayStatus.smsStorSize = smsStorSize ;
	COMMON_GetCurrentDts( &neowayStatus.powerOnDts );

	GSM_Unprotect( &neowayStatusMutex ) ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM Parsed imei [%s], sms storage size [%u]", imei, smsStorSize );

	return TRUE ;
}

//
//
//
BOOL GSM_WaitAfterStart(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	static dateTimeStamp startDts = {{0,0,0}, {0,0,0}};
	dateTimeStamp currDts ;
	int dtsDiff;
	BOOL res = FALSE, serviceMode = FALSE ;

	if (COMMON_CheckDtsForValid(&startDts) == OK)
	{
		COMMON_GetCurrentDts(&currDts);
		dtsDiff = COMMON_GetDtsDiff( &startDts, &currDts, BY_SEC );
		if ( dtsDiff > 60 )
			res = TRUE ;
	}
	else
		COMMON_GetCurrentDts(&startDts);

	if (res)
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM waiting minute after start is over");
		memset(&startDts, 0 , sizeof(dateTimeStamp));

		if (connection->mode == CONNECTION_GPRS_SERVER)
		{
			CONNECTION_CheckTimeToConnect(&serviceMode);

			GSM_Protect( &neowayStatusMutex ) ;
			neowayStatus.serviceMode = serviceMode ;
			GSM_Unprotect( &neowayStatusMutex ) ;

			if (!serviceMode)
			{
				//set up listen port
				if ( !GSM_SetListenPort(connection) )
				{
					res = FALSE ;
					gsmConnectionState = GCS_POWERON ;
				}
			}
		}
		else if (connection->mode == CONNECTION_CSD_INCOMING )
		{
			GSM_Protect( &neowayStatusMutex ) ;
			neowayStatus.dialState = GDS_WAIT ;
			GSM_Unprotect( &neowayStatusMutex ) ;
		}

	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM Read sms from line %d", __LINE__);
		GSM_SmsHandler();
		COMMON_Sleep(500);
	}

	return res ;
}

BOOL GSM_IsTimeToConnect(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	BOOL res = FALSE ;
	dateTimeStamp currentDts;
	int dtsDiff ;

	switch(connection->mode)
	{
		case CONNECTION_GPRS_SERVER:
		{
			res = TRUE ;
			GSM_Protect( &neowayStatusMutex ) ;
			if ( !neowayStatus.serviceMode )
			{				
				if (neowayStatus.dialState == GDS_NEED_TO_ANSW)
				{
					COMMON_GetCurrentDts( &neowayStatus.onlineDts);
					COMMON_GetCurrentDts( &neowayStatus.lastDataDts);
				}
				else
				{
					res = FALSE ;

					COMMON_STR_DEBUG(DEBUG_GSM "GSM Read sms from line %d", __LINE__);
					GSM_SmsHandler();

					//check timeout to reboot module
					COMMON_GetCurrentDts(&currentDts);
					dtsDiff = COMMON_GetDtsDiff__Alt( &neowayStatus.powerOnDts, &currentDts, BY_SEC );

					if (dtsDiff > TIMEOUT3)
						gsmConnectionState = GCS_POWERON ;
				}

			}
			GSM_Unprotect( &neowayStatusMutex ) ;
		}
			break;

		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_OUTGOING:
		{			
			GSM_Protect( &neowayStatusMutex ) ;
			res = CONNECTION_CheckTimeToConnect( &neowayStatus.serviceMode );
			GSM_Unprotect( &neowayStatusMutex ) ;

			if (!res)
			{
				COMMON_STR_DEBUG(DEBUG_GSM "GSM Read sms from line %d", __LINE__);
				GSM_SmsHandler();
			}
		}
			break;

		case CONNECTION_CSD_INCOMING:
		{
			int dialState;
			GSM_Protect( &neowayStatusMutex ) ;

			dialState = neowayStatus.dialState ;

			GSM_Unprotect( &neowayStatusMutex ) ;

			switch(dialState)
			{
				case GDS_NEED_TO_ANSW:
					res = TRUE ;
					break;

				case GDS_NEED_TO_HANG:
				{
					neowayCmdWA_t cmdArr = { (unsigned char *)"ATH\r", answerBankStandart[0] , answerBankStandart, 5 };
					GSM_CmdByWaitAnswer(&cmdArr, NULL);
				}
				break;

				default:
				{
					COMMON_STR_DEBUG(DEBUG_GSM "GSM Read sms from line %d", __LINE__);
					GSM_SmsHandler();
				}
					break;

			} //switch(dialState)
		}
			break;

		default:
			return FALSE ;
	}

	if ( res )
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM It's time to connect...");
	}

	return res ;
}

//
//
//

int GSM_IsTimeToDisconnect(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int dialState, dtsDiff, i ;
	BOOL serviceMode ;
	dateTimeStamp currentDts, lastDataDts, stopTimeDts ;
	GSM_Protect( &neowayStatusMutex ) ;
	dialState = neowayStatus.dialState ;
	serviceMode = neowayStatus.serviceMode ;
	memcpy(&lastDataDts, &neowayStatus.lastDataDts, sizeof(dateTimeStamp));
	memcpy(&stopTimeDts, &neowayStatus.onlineDts, sizeof(dateTimeStamp));
	GSM_Unprotect( &neowayStatusMutex ) ;

	if (dialState == GDS_WAIT)
	{
		if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
		{
			//connection was closed
			STORAGE_EVENT_DISCONNECTED(0x02);
			return GCS_CHECK_TIME_TO_CONNECT ; //wait next connection accept
		}
	}

//	if (dialState == GDS_NEED_TO_HANG)
//	{
//		if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
//		{
//			//TODO: close connection to hang
//			return GCS_PROCESS_CONNECTION ;
//		}
//	}

	//check connection was closed by server
	if (dialState == GDS_OFFLINE)
	{
		STORAGE_EVENT_DISCONNECTED(0x02);
		if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
			return GCS_CLOSE_CONNECTION ; //close connection and resume listening port
		else if (connection->mode == CONNECTION_CSD_INCOMING)
			return GCS_CLOSE_CONNECTION ; //close connection and resume waiting call
		else
			return GCS_POWERON ; //reboot module
	}

	//check connection was timeouted by data timeout
	if ( COMMON_CheckDtsForValid(&lastDataDts) )
	{
		STORAGE_EVENT_DISCONNECTED(0x01);
		if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
			return GCS_CLOSE_CONNECTION ; //close connection and resume listening port
		else if (connection->mode == CONNECTION_CSD_INCOMING)
			return GCS_CLOSE_CONNECTION ; //close connection and resume waiting call
		else
			return GCS_POWERON ; //reboot module
	}

	COMMON_GetCurrentDts(&currentDts);
	dtsDiff = COMMON_GetDtsDiff__Alt( &lastDataDts, &currentDts, BY_SEC );
	if ( (dtsDiff < 0) || (dtsDiff > TIMEOUT1))
	{
		STORAGE_EVENT_DISCONNECTED(0x01);
		if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
			return GCS_CLOSE_CONNECTION ; //close connection and resume listening port
		else if (connection->mode == CONNECTION_CSD_INCOMING)
			return GCS_CLOSE_CONNECTION ; //close connection and resume waiting call
		else
			return GCS_POWERON ; //reboot module
	}

	//check connection was timeouted by shedule
	if ( (connection->mode == CONNECTION_CSD_OUTGOING) || (connection->mode == CONNECTION_GPRS_CALENDAR))
	{
		if ( (connection->calendarGsm.ifstoptime) &&
			 ((connection->calendarGsm.stop_hour > 0) ||
			  (connection->calendarGsm.stop_min > 0)) )
		{
			//calc stoptime
			for (i=0; i < connection->calendarGsm.stop_hour; i++)
				COMMON_ChangeDts(&stopTimeDts, INC, BY_HOUR);
			for (i=0; i < connection->calendarGsm.stop_min; i++)
				COMMON_ChangeDts(&stopTimeDts, INC, BY_MIN);

			//check if stoptime is less or equal to current time
			if ( COMMON_CheckDts( &stopTimeDts, &currentDts) )
			{
				STORAGE_EVENT_DISCONNECTED(0x01);
				return GCS_POWERON ; //reboot module
			}
		}
	}

	COMMON_Sleep(SECOND);
	return GCS_PROCESS_CONNECTION ;
}

//
//
//

BOOL GSM_IsModuleInCsd(connection_t * connection)
{
	if ((connection->mode == CONNECTION_CSD_INCOMING) || (connection->mode == CONNECTION_CSD_OUTGOING))
	{
		if (neowayStatus.dialState == GDS_ONLINE)
		{
			return TRUE ;
		}
	}

	return FALSE ;
}

//
//
//

void GSM_Close(connection_t * connection)
{
	BOOL serviceMode ;
	unsigned int currSock;
	int w;
	GSM_Protect( &neowayStatusMutex ) ;
	serviceMode = neowayStatus.serviceMode ;
	currSock = neowayStatus.currSock ;
	neowayStatus.dialState = GDS_WAIT ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
	{
		unsigned char closeClientChr[64] = {0};
		neowayCmdWA_t closeClientStr = {closeClientChr, answerBankCloseClient[0], answerBankCloseClient , 5};
		snprintf((char *)closeClientChr, 64, "AT+CLOSECLIENT=%u\r", currSock);

		GSM_CmdByWaitAnswer(&closeClientStr, NULL);

		COMMON_STR_DEBUG(DEBUG_GSM "GSM Close connection [%u]", currSock);
	}
	else if (connection->mode == CONNECTION_CSD_INCOMING)
	{
		neowayCmdWA_t closeClientStr = {(unsigned char *)"ATH\r", answerBankStandart[0], answerBankStandart , 5};


		SERIAL_Write(PORT_GSM, (unsigned char *)"+++", 3, &w);
		COMMON_Sleep(SECOND);

		GSM_CmdByWaitAnswer(&closeClientStr, NULL);

		COMMON_STR_DEBUG(DEBUG_GSM "GSM Close call was closed");
	}
}

BOOL GSM_Open(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	BOOL serviceMode;

	GSM_Protect( &neowayStatusMutex ) ;
	serviceMode = neowayStatus.serviceMode ;
	GSM_Unprotect( &neowayStatusMutex ) ;


	CONNECTION_SetLastSessionProtoTrFlag(FALSE);

	switch(connection->mode)
	{
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_GPRS_ALWAYS:
			return GSM_OpenByClient (connection);

		case CONNECTION_GPRS_SERVER:		
			if (serviceMode)
				return GSM_OpenByClient (connection);


			GSM_Protect( &neowayStatusMutex ) ;
			neowayStatus.dialState = GDS_ONLINE ;
			GSM_Unprotect( &neowayStatusMutex ) ;
			return TRUE ;

		case CONNECTION_CSD_OUTGOING:
			return GSM_OpenByCsdOutgoing(connection);

		case CONNECTION_CSD_INCOMING:
			return GSM_OpenByCsdIncoming();

		default:
			return FALSE ;
	}
}

BOOL GSM_OpenByCsdOutgoing(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	char cmd[128] = {0};
	neowayCmdWA_t cmdArr = { (unsigned char *)cmd, answerBankCsdCall[0] , answerBankCsdCall, 30 };

	GSM_Protect( &neowayStatusMutex ) ;

	if (neowayStatus.serviceMode && strlen((char *)connection->servicePhone))
		snprintf(cmd, 128, "ATD%s\r", connection->servicePhone);
	else if ( neowayStatus.currentSim == SIM1 )
		snprintf(cmd, 128, "ATD%s\r", connection->serverPhoneSim1);
	else
		snprintf(cmd, 128, "ATD%s\r", connection->serverPhoneSim2);

	GSM_Unprotect( &neowayStatusMutex ) ;


	if (!GSM_CmdByWaitAnswer(&cmdArr, NULL))
	{
		GSM_Protect( &neowayStatusMutex ) ;

		if (neowayStatus.serviceMode)
			CONNECTION_ReverseLastServiceAttemptFlag();

		GSM_Unprotect( &neowayStatusMutex ) ;

		COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by CSD fail");
		return FALSE ;
	}

	GSM_Protect( &neowayStatusMutex ) ;
	neowayStatus.dialState = GDS_ONLINE ;
	neowayStatus.currSock = 0 ;
	COMMON_GetCurrentDts( &neowayStatus.onlineDts);
	COMMON_GetCurrentDts( &neowayStatus.lastDataDts);
	GSM_Unprotect( &neowayStatusMutex ) ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by CSD OK");

	return TRUE ;
}

BOOL GSM_OpenByCsdIncoming()
{
	neowayCmdWA_t cmdArr = { (unsigned char *)"ATA\r", answerBankCsdCall[0] , answerBankCsdCall, 10 };

	if (!GSM_CmdByWaitAnswer(&cmdArr, NULL))
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by CSD fail");
		return FALSE ;
	}

	GSM_Protect( &neowayStatusMutex ) ;
	neowayStatus.dialState = GDS_ONLINE ;
	neowayStatus.currSock = 0 ;
	COMMON_GetCurrentDts( &neowayStatus.onlineDts);
	COMMON_GetCurrentDts( &neowayStatus.lastDataDts);
	GSM_Unprotect( &neowayStatusMutex ) ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by CSD OK");

	return TRUE ;
}

BOOL GSM_OpenByClient(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	unsigned char apnStr[128] = {0};
	unsigned char servStr[128] = {0};
	unsigned char ansBuf[GSM_TASK_RX_SIZE];
	char ip[LINK_SIZE];
	unsigned char * ansBufPtr = NULL ;

	neowayCmdWA_t cmdArr[] = {{apnStr, answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+XIIC=1\r", answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+ASCII=0\r", answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+XIIC?\r", answerBankStandart[0] , answerBankStandart , 5},
							 {servStr, answerBankTcpSetup[0] , answerBankTcpSetup , 30} };
	int i, cmdSize = sizeof(cmdArr) / sizeof(neowayCmdWA_t) ;
	BOOL parsingIp = FALSE ;


	GSM_SetNetApnStr(connection, apnStr, 128);
	GSM_SetTcpSetupStr(connection, servStr, 128);


	for ( i = 0 ; i < cmdSize ; i++)
	{		
		ansBufPtr = NULL ;
		parsingIp = FALSE ;
		//need to parse answer for command AT+XIIC? to get own IP
		if ( !strcmp( (char *)cmdArr[i].cmd, (char *)"AT+XIIC?\r") )
		{
			memset(ansBuf, 0 , GSM_TASK_RX_SIZE);
			memset(ip, 0 , LINK_SIZE);
			ansBufPtr = ansBuf ;
			parsingIp = TRUE ;
			COMMON_Sleep(SECOND*2);
		}

		if ( !GSM_CmdByWaitAnswer( &cmdArr[i], ansBufPtr ) )
		{
			COMMON_STR_DEBUG(DEBUG_GSM "GSM Error entering init cmd [%s]", cmdArr[i].cmd);

			GSM_Protect( &neowayStatusMutex ) ;
			if (neowayStatus.serviceMode)
				CONNECTION_ReverseLastServiceAttemptFlag();
			GSM_Unprotect( &neowayStatusMutex ) ;

			return FALSE ;
		}

		if (parsingIp)
		{
			sscanf((char*)ansBuf, "%*s\r\r\n+XIIC:    %*d,%" DEF_TO_XSTR(LINK_SIZE) "[0123456789.]", ip);

			COMMON_STR_DEBUG(DEBUG_GSM "GSM parsed own ip [%s]", ip ) ;

			GSM_Protect( &neowayStatusMutex ) ;
			strncpy( neowayStatus.ip, ip, (LINK_SIZE - 1) );
			GSM_Unprotect( &neowayStatusMutex ) ;
		}

		COMMON_Sleep(50);
	}

	COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by client success. Setting timers");

	GSM_Protect( &neowayStatusMutex ) ;
	neowayStatus.dialState = GDS_ONLINE ;
	neowayStatus.currSock = 0 ;
	COMMON_GetCurrentDts( &neowayStatus.onlineDts);
	COMMON_GetCurrentDts( &neowayStatus.lastDataDts);
	GSM_Unprotect( &neowayStatusMutex ) ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM opening by client OK");

	return TRUE ;
}

//
//
//

BOOL GSM_SetListenPort(connection_t * connection)
{
	BOOL res = FALSE ;
	unsigned char apnStr[128] = {0};
	unsigned char listenStr[128] = {0};

	neowayCmdWA_t cmdArr[] = {{apnStr, answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+XIIC=1\r", answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+ASCII=0\r", answerBankStandart[0] , answerBankStandart , 5},
							 {(unsigned char *)"AT+XIIC?\r", answerBankStandart[0] , answerBankStandart , 5},
							 {listenStr, answerBankTcpSetup[0] , answerBankTcpSetup , 30},};

	int i, cmdSize = sizeof(cmdArr) / sizeof(neowayCmdWA_t) ;
	unsigned char * ansBufPtr = NULL ;
	unsigned char ansBuf[GSM_TASK_RX_SIZE];
	char ip[LINK_SIZE];
	BOOL parsingIp = FALSE ;

	GSM_SetNetApnStr(connection, apnStr, 128);
	snprintf((char *)listenStr, 128, "AT+TCPLISTEN=%u\r", connection->port);

	for ( i = 0 ; i < cmdSize ; i++)
	{
		ansBufPtr = NULL ;
		parsingIp = FALSE ;
		//need to parse answer for command AT+XIIC? to get own IP
		if ( !strcmp( (char *)cmdArr[i].cmd, (char *)"AT+XIIC?\r") )
		{
			memset(ansBuf, 0 , GSM_TASK_RX_SIZE);
			memset(ip, 0 , LINK_SIZE);
			ansBufPtr = ansBuf ;
			parsingIp = TRUE ;
			COMMON_Sleep(SECOND*2);
		}

		if ( !GSM_CmdByWaitAnswer( &cmdArr[i], ansBufPtr ) )
		{
			COMMON_STR_DEBUG(DEBUG_GSM "GSM Error entering init cmd [%s]", cmdArr[i].cmd);

			GSM_Protect( &neowayStatusMutex ) ;
			if (neowayStatus.serviceMode)
				CONNECTION_ReverseLastServiceAttemptFlag();
			GSM_Unprotect( &neowayStatusMutex ) ;

			return res ;
		}

		if (parsingIp)
		{
			sscanf((char*)ansBuf, "%*s\r\r\n+XIIC:    %*d,%" DEF_TO_XSTR(LINK_SIZE) "[0123456789.]", ip);

			COMMON_STR_DEBUG(DEBUG_GSM "GSM parsed own ip [%s]", ip ) ;

			GSM_Protect( &neowayStatusMutex ) ;
			strncpy( neowayStatus.ip, ip, (LINK_SIZE - 1) );
			GSM_Unprotect( &neowayStatusMutex ) ;
		}

		COMMON_Sleep(50);
	}

	CONNECTION_SetLastSessionProtoTrFlag(TRUE);

	GSM_Protect( &neowayStatusMutex ) ;
	neowayStatus.dialState = GDS_WAIT ;

	GSM_Unprotect( &neowayStatusMutex ) ;

	res = TRUE;
	return res ;
}

//
//
//

void GSM_SetNetApnStr(connection_t * connection, unsigned char * str, int length)
{
	DEB_TRACE(DEB_IDX_GSM);

	unsigned char * apn = connection->apn1;
	unsigned char * user = connection->apn1_user;
	unsigned char * pass = connection->apn1_pass;
	unsigned char opName[GSM_OPERATOR_NAME_LENGTH] = {0};
	unsigned char currentSim;

	GSM_Protect( &neowayStatusMutex ) ;

	currentSim = neowayStatus.currentSim ;
	snprintf( (char *)opName, GSM_OPERATOR_NAME_LENGTH , "%s", neowayStatus.opName );

	GSM_Unprotect( &neowayStatusMutex ) ;

	if ( currentSim == SIM2 )
	{
		apn = connection->apn2;
		user = connection->apn2_user;
		pass = connection->apn2_pass;
	}

	if ( !strlen((char *)apn) && !strlen((char *)user) && !strlen((char *)pass) )
	{
		//set default apn settings by op name
		COMMON_ShiftTextDown( (char *)opName , strlen((char *)opName) );
		if ( strstr( (char *)opName , "mts" ) )
		{
			apn = (unsigned char *)"internet.mts.ru";
			user =(unsigned char *) "mts" ;
			pass = (unsigned char *)"mts" ;
		}
		else if ( strstr( (char *)opName , "beeline" ) )
		{
			apn = (unsigned char *)"internet.beeline.ru";
			user = (unsigned char *)"beeline" ;
			pass = (unsigned char *)"beeline" ;
		}
		else if ( strstr( (char *)opName , "megafon" ) )
		{
			apn = (unsigned char *)"internet";
			user = (unsigned char *)"gdata" ;
			pass = (unsigned char *)"gdata" ;
		}
		else if ( strstr( (char *)opName , "tele2" ) )
		{
			apn = (unsigned char *)"internet.tele2.ru";
			user = (unsigned char *)"" ;
			pass = user ;
		}
		else if ( strstr( (char *)opName , "yota" ) )
		{
			apn = (unsigned char *)"internet.yota";
			user = (unsigned char *)"" ;
			pass = user ;
		}
		else
		{
			apn = (unsigned char *)"internet";
			user = (unsigned char *)"" ;
			pass = user ;
		}
	}

	snprintf((char *)str, length, "AT+NETAPN=\"%s\",\"%s\",\"%s\"\r", apn, user, pass);
	return ;
}

//
//
//

void GSM_SetTcpSetupStr(connection_t * connection, unsigned char * str, int length)
{
	DEB_TRACE(DEB_IDX_GSM);

	BOOL serviceMode ;
	unsigned char * server = connection->server;
	unsigned int port = connection->port;

	GSM_Protect( &neowayStatusMutex ) ;

	serviceMode = neowayStatus.serviceMode ;

	GSM_Unprotect( &neowayStatusMutex ) ;

	if (serviceMode)
	{
		server = connection->serviceIp ;
		port = connection->servicePort ;

		#if REV_COMPILE_STASH_NZIF_IP
		if ( !strcmp((char *)server, "212.67.9.43") && (port == 10100) )
		{
			server = (unsigned char *)"212.67.9.44" ;
			port = 58198 ;
		}
		#endif
	}
	#if REV_COMPILE_STASH_NZIF_IP
	else
	{
		if ( !strcmp((char *)server, "212.67.9.43") && (port == 10101) )
		{
			server = (unsigned char *)"212.67.9.44" ;
			port = 58198 ;
		}
	}
	#endif


	snprintf((char *)str, length , "AT+TCPSETUP=0,%s,%u\r", server, port);

}

//
//
//

void GSM_PutIncMessageToReadBuf()
{
	GSM_Protect( &tcpReadBufMutex ) ;

	//sscanf( neowayStatus.neowayBuffer.buf, "%*[A-Z,(,)]: " )

	//\r\n+TCPRECV: 0,18,7e7e7e7e7e7e7e7e01000200010001090009
	//+TCPRECV(S):0,4,3334350a


	int i = 0, commaCounter = 0 ;
	unsigned char * buf = neowayStatus.neowayBuffer.buf ;
	unsigned int scnfed;
	char interBuf[3] = {0} ;
	int interBufSize = 0 ;
	unsigned int sockNumb = 0 ;
	unsigned int oldBufSize = neowayTcpReadBuf.size ;

	for ( ;i < neowayStatus.neowayBuffer.size ; i++)
	{
		if (neowayTcpReadBuf.size == GSM_INPUT_BUF_SIZE)
			continue;

		if ( !isprint(buf[i]) )
			continue;

		if (buf[i] == ',')
		{
			commaCounter++ ;
			if (commaCounter == 1)
			{
				if (i < 1)
					break ;

				interBuf[interBufSize++] = buf[i - 1];
				sscanf(interBuf, "%u", &sockNumb);

				if (sockNumb != neowayStatus.currSock)
				{
					//message from unexpected sock. do nothing
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Wait current socket is [%u], but message from sock [%u]", neowayStatus.currSock, sockNumb );
					break;
				}

				interBufSize = 0 ;
				memset(interBuf, 0 , 3);
			}
			continue ;
		}


		if (commaCounter == 2)
		{

			interBuf[interBufSize++] = buf[i];
			if (interBufSize == 2)
			{
				sscanf(interBuf, "%02x", &scnfed);
				neowayTcpReadBuf.buf[ neowayTcpReadBuf.size++ ] = scnfed;
				interBufSize = 0 ;
				memset(interBuf, 0 , 3);
			}
		}
	}

	if (oldBufSize != neowayTcpReadBuf.size)
	{
		COMMON_GetCurrentDts(&neowayStatus.lastDataDts);
		COMMON_BUF_DEBUG(DEBUG_GSM "GSM read buf:", neowayTcpReadBuf.buf, neowayTcpReadBuf.size);
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM Parsing incoming buffer is unsuccess!!!");
	}

	GSM_Unprotect( &tcpReadBufMutex ) ;
}




int GSM_WriteTcp(connection_t * connection, unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_GSM);

	const int checkInterval = 500 ; //ms
	int res = ERROR_GENERAL ;
	int w, status;
	int timer1 = 5 * SECOND / checkInterval;
	int timer2 = 20 * SECOND / checkInterval;
	int breakFlag = 0 ;
	unsigned char cmd[64];
	unsigned char * answerBankPrompt[] = { (unsigned char *)">",
										   (unsigned char *)"\r\n+TCPSEND",
										   NULL};
	unsigned char * answerBankSend[] = { (unsigned char *)"\r\n+TCPSEND", NULL};
	memset(cmd, 0 , 64);

	//set task
	GSM_Protect( &neowayStatusMutex ) ;

	if ((connection->mode == CONNECTION_GPRS_SERVER) && !neowayStatus.serviceMode )
	{
		//TODO: add socket to write instead 0
		snprintf((char *)cmd, 64 , "AT+TCPSENDS=%u,%d\r", neowayStatus.currSock, size);
	}
	else
	{
		snprintf((char *)cmd, 64 , "AT+TCPSEND=%u,%d\r", neowayStatus.currSock, size);
	}

	neowayStatus.task.possibleAnswerBank = answerBankPrompt ;
	neowayStatus.task.status = GTS_WAIT ;
	neowayStatus.task.tx = cmd ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	SERIAL_Write(PORT_GSM, cmd , strlen((char *)cmd), &w);

	//wait task ready
	while ( timer1 )
	{
		GSM_Protect( &neowayStatusMutex ) ;
		if ( neowayStatus.task.status == GTS_READY )
		{
			breakFlag = 2 ;
			neowayStatus.task.status = GTS_EMPTY;
			neowayStatus.task.possibleAnswerBank = NULL ;
			if (strstr((char *)neowayStatus.task.rx, ">"))
			{
				//prompt was recieved. Prepare task to next write iteration
				breakFlag = 1 ;
				neowayStatus.task.status = GTS_WAIT;
				neowayStatus.task.possibleAnswerBank = answerBankSend ;
			}

			memset(neowayStatus.task.rx, 0 , GSM_TASK_RX_SIZE ) ;
			neowayStatus.task.tx = NULL ;
		}
		GSM_Unprotect( &neowayStatusMutex ) ;

		if (breakFlag)
			break;

		COMMON_Sleep(checkInterval);
		timer1-- ;
	}

	if (breakFlag == 2)
	{
		COMMON_STR_DEBUG(DEBUG_GSM "GSM_WriteTcp was not found prompt");
		return ERROR_GENERAL ;
	}

	//write data
	SERIAL_Write(PORT_GSM, buf , size, &w);

	while ( timer2 )
	{
		GSM_Protect( &neowayStatusMutex ) ;
		status = neowayStatus.task.status ;
		GSM_Unprotect( &neowayStatusMutex ) ;

		if ( status == GTS_READY )
			break;

		COMMON_Sleep(checkInterval);
		timer2-- ;
	}



	GSM_Protect( &neowayStatusMutex ) ;

	if ( neowayStatus.task.status == GTS_READY )
	{
		COMMON_STR_DEBUG(DEBUG_GSM "%s", neowayStatus.task.rx ) ;
		if ( strstr((char *)neowayStatus.task.rx, "\r\nOK\r\n"))
		{
			//TODO: parse bytes were written
			*written = size ;
			COMMON_STR_DEBUG(DEBUG_GSM "GSM_WriteTcp OK");
			COMMON_GetCurrentDts(&neowayStatus.lastDataDts);
			res = OK ;
		}
	}

	// clear task
	neowayStatus.task.status = GTS_EMPTY;
	neowayStatus.task.tx = NULL ;
	memset(neowayStatus.task.rx, 0 , GSM_TASK_RX_SIZE ) ;
	neowayStatus.task.possibleAnswerBank = NULL ;

	GSM_Unprotect( &neowayStatusMutex ) ;


	return res ;
}

int GSM_WriteCsd(connection_t * connection __attribute__((unused)), unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_GSM);

	return SERIAL_Write(PORT_GSM, buf, size, (int *)written);
}


int GSM_Write(connection_t * connection, unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_GSM);

	switch(connection->mode)
	{
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_SERVER:
			return GSM_WriteTcp(connection, buf, size, written);

		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
			return GSM_WriteCsd(connection, buf, size, written);
	}
	return ERROR_GENERAL;
}

int GSM_Read(connection_t * connection __attribute__((unused)), unsigned char * buf, int size, unsigned long * readed)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;
	uint bytesReaded;

	GSM_Protect( &tcpReadBufMutex ) ;

	bytesReaded = size ;
	if ((unsigned int)size > neowayTcpReadBuf.size)
		bytesReaded = neowayTcpReadBuf.size ;

	memcpy(buf, neowayTcpReadBuf.buf, bytesReaded);
	memmove( &neowayTcpReadBuf.buf[0], &neowayTcpReadBuf.buf[bytesReaded], neowayTcpReadBuf.size - bytesReaded );
	neowayTcpReadBuf.size -= bytesReaded ;
	memset( &neowayTcpReadBuf.buf[neowayTcpReadBuf.size], 0 , GSM_INPUT_BUF_SIZE - neowayTcpReadBuf.size );
	*readed = bytesReaded ;
	res = OK ;

	GSM_Unprotect( &tcpReadBufMutex ) ;


	COMMON_BUF_DEBUG(DEBUG_GSM "GSM_Read", buf, *readed);

	return res ;
}

int GSM_ReadAvailable(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	GSM_Protect( &neowayStatusMutex ) ;
	BOOL serviceMode = neowayStatus.serviceMode ;
	unsigned int hangSock = neowayStatus.hangSock ;
	int dialState = neowayStatus.dialState ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	switch(dialState)
	{
		case GDS_OFFLINE:
		case GDS_WAIT:
		case GDS_NEED_TO_ANSW:
		{
			GSM_Protect( &tcpReadBufMutex ) ;

			neowayTcpReadBuf.size = 0;
			memset( neowayTcpReadBuf.buf, 0 , GSM_INPUT_BUF_SIZE);

			GSM_Unprotect( &tcpReadBufMutex ) ;
		}
			return ERROR_GENERAL ;

		case GDS_NEED_TO_HANG:
		{
			if ( (connection->mode == CONNECTION_GPRS_SERVER) && (!serviceMode) )
			{
				unsigned char closeClientChr[64] = {0};
				neowayCmdWA_t closeClientStr = {closeClientChr, answerBankCloseClient[0], answerBankCloseClient , 5};
				snprintf((char *)closeClientChr, 64, "AT+CLOSECLIENT=%u\r", hangSock);

				GSM_CmdByWaitAnswer(&closeClientStr, NULL);

				COMMON_STR_DEBUG(DEBUG_GSM "GSM Hang connection [%u]", hangSock);

				GSM_Protect( &neowayStatusMutex ) ;
				neowayStatus.hangSock = 0 ;
				neowayStatus.dialState = GDS_ONLINE ;
				GSM_Unprotect( &neowayStatusMutex ) ;
			}
		}
		break;

		default:
			break;

	}

	GSM_Protect( &tcpReadBufMutex ) ;

	res = neowayTcpReadBuf.size ;

	GSM_Unprotect( &tcpReadBufMutex ) ;

	static int gsmEmptyReadAvailCounter = 0 ;

	if ( (res == 0) && (connection->mode != CONNECTION_CSD_INCOMING) && (connection->mode != CONNECTION_CSD_OUTGOING))
	{
		gsmEmptyReadAvailCounter++;
		if ( gsmEmptyReadAvailCounter > 10 )
		{
			gsmEmptyReadAvailCounter = 0 ;
			COMMON_STR_DEBUG(DEBUG_GSM "GSM Read sms from line %d", __LINE__);
			GSM_SmsHandler();
		}
	}

	return res ;
}

//
//
//

void GSM_GetOwnIp(char * ip)
{
	DEB_TRACE(DEB_IDX_GSM);

	GSM_Protect( &neowayStatusMutex ) ;

	if ( strlen(neowayStatus.ip) )
		snprintf(ip, LINK_SIZE, "%s", neowayStatus.ip);
	else
		snprintf(ip, LINK_SIZE, "0.0.0.0");

	GSM_Unprotect( &neowayStatusMutex ) ;
}

int GSM_GetIMEI(char * imeiString, const unsigned int imeiStringMaxLength)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	GSM_Protect( &neowayStatusMutex ) ;

	if ( strlen(neowayStatus.imei) )
	{
		res = OK ;
		snprintf(imeiString, imeiStringMaxLength, "%s", neowayStatus.imei);
	}

	GSM_Unprotect( &neowayStatusMutex ) ;

	return  res;
}

int GSM_GetSignalQuality()
{
	DEB_TRACE(DEB_IDX_GSM);

	int sq ;
	GSM_Protect( &neowayStatusMutex ) ;

	sq = neowayStatus.sq ;

	GSM_Unprotect( &neowayStatusMutex ) ;
	return sq ;
}

int GSM_ConnectionState()
{
	DEB_TRACE(DEB_IDX_GSM);

	if ( gsmConnectionState == GCS_PROCESS_CONNECTION )
		return OK ;

	return ERROR_GENERAL ;
}

int GSM_GetGsmOperatorName(char * gsmOpName, const unsigned int gsmOpNameMaxLength)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;
	GSM_Protect( &neowayStatusMutex ) ;

	if (strlen( neowayStatus.opName ))
	{
		res = OK ;
		snprintf(gsmOpName, gsmOpNameMaxLength, "%s", neowayStatus.opName);
	}

	GSM_Unprotect( &neowayStatusMutex ) ;
	return res ;
}

int GSM_GetCurrentConnectionTime()
{
	DEB_TRACE(DEB_IDX_GSM);

	int connectionTime = 0 ;
	dateTimeStamp currDts, onlineDts ;

	if ( GSM_ConnectionState() == OK)
	{
		COMMON_GetCurrentDts(&currDts);

		GSM_Protect( &neowayStatusMutex ) ;
		memcpy(&onlineDts, &neowayStatus.onlineDts, sizeof(dateTimeStamp));
		GSM_Unprotect( &neowayStatusMutex ) ;

		connectionTime = COMMON_GetDtsDiff__Alt( &onlineDts, &currDts, BY_SEC );
	}
	return connectionTime ;
}

unsigned int GSM_GetConnectionsCounter()
{
	DEB_TRACE(DEB_IDX_GSM)

	GSM_Protect(&connectionCounterMutex);

	unsigned int res=0;

	int fd = open( GSM_CONNECTION_COUNTER, O_RDONLY );
	if ( fd > 0 )
	{
		unsigned int ccounter = 0 ;
		if( read(fd, &ccounter, sizeof(unsigned int)) == sizeof(unsigned int) )
		{
			res = ccounter ;
		}

		close(fd);
	}

	GSM_Unprotect(&connectionCounterMutex);

	return res;
}

int GSM_GetCurrentSimNumber()
{
	DEB_TRACE(DEB_IDX_GSM);

	unsigned char currentSim;
	GSM_Protect( &neowayStatusMutex ) ;
	currentSim = neowayStatus.currentSim ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	return currentSim ;
}

int GSM_ConnectionCounterIncrement()
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	GSM_Protect(&connectionCounterMutex);

	int fd = open( GSM_CONNECTION_COUNTER, O_RDWR | O_CREAT );
	if ( fd > 0 )
	{
		unsigned int ccounter = 0 ;
		read(fd, &ccounter, sizeof(unsigned int));
		++ccounter;

		lseek( fd , 0 , SEEK_SET);
		ftruncate( fd , 0 );

		if ( write( fd , &ccounter , sizeof(unsigned int) ) == sizeof(unsigned int) )
		{
			res = OK ;
		}

		close(fd);
	}

	GSM_Unprotect(&connectionCounterMutex);

	return res;
}

BOOL GSM_GetServiceModeStat()
{
	DEB_TRACE(DEB_IDX_GSM);

	BOOL serviceMode;
	GSM_Protect( &neowayStatusMutex ) ;
	serviceMode = neowayStatus.serviceMode ;
	GSM_Unprotect( &neowayStatusMutex ) ;

	return serviceMode ;
}

//
// SMS part
//

void GSM_ConvertByteToPduBuf(unsigned char byte, char * buf, int * iterator)
{
	char ibuf[3];
	memset((char*)ibuf, 0 , 3);
	snprintf(ibuf, 3 ,"%02X", byte);

	buf[ *iterator ] = ibuf[0];
	buf[ *iterator + 1] = ibuf[1];

	*iterator += 2 ;
}

void GSM_ConvertHalfByteToPduBuf(unsigned char hbyte, char * buf, int * iterator)
{
	char ibuf[2];
	memset((char*)ibuf, 0 , 2);
	snprintf(ibuf, 2 ,"%1X", hbyte & 0x0F);

	buf[ *iterator ] = ibuf[0];
	*iterator += 1 ;
}

void GSM_ConvertPhoneNumerToPduBuf(char * phone, char * buf, int * iterator)
{
	int i = 0, phoneLengthPos = *iterator, phoneLength = 0 ;

	//set length
	GSM_ConvertByteToPduBuf( 0xFF , buf, iterator );

	//set destination type
	GSM_ConvertByteToPduBuf( 0x91 , buf, iterator );

	for ( i = 0 ; i < (int)strlen(phone); i++)
	{
		if (isdigit(phone[i]))
		{
			GSM_ConvertHalfByteToPduBuf( phone[i], buf, iterator );
			phoneLength ++;
		}
	}

	if (phoneLength % 2)
		GSM_ConvertHalfByteToPduBuf( 0x0F, buf, iterator );

	//set length again
	GSM_ConvertByteToPduBuf( phoneLength , buf, &phoneLengthPos );


	i = phoneLengthPos + 2 ;
	while(i < *iterator)
	{
		char swap = 0;
		swap = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = swap ;
		i += 2 ;
	}

}

//recieving sms part

BOOL GSM_ConvertPduToByte(char * buf, int * iterator, unsigned char * byte)
{
	unsigned int ibyte;

	if ( (int)strlen(buf) <= (*iterator + 1) )
		return FALSE;

	if ( !isxdigit( buf[*iterator]) || !isxdigit( buf[*iterator + 1]) )
		return FALSE;

	sscanf( &buf[*iterator], "%02X", &ibyte);
	*byte = ibyte ;

	*iterator += 2 ;

	return TRUE ;
}

void GSM_ConvertAsciiToGsmAlphabet(unsigned char * ascii, unsigned char * gsm, int * gsmLength)
{
	int i = 0 ;
	int asciiLength = strlen((char *)ascii);

	*gsmLength = 0 ;

	for (; i < asciiLength; i++)
	{
		switch( ascii[i] )
		{
			case '@':
				gsm[ (*gsmLength)++ ] = 0x00 ;
				break;

			case '_':
				gsm[ (*gsmLength)++ ] = 0x11 ;
				break;

			case '[':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x3C ;
				break;

			case '\\':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x2F ;
				break;

			case ']':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x3E ;
				break;

			case '^':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x14 ;
				break;

			case '`':
				gsm[ (*gsmLength)++ ] = '\'' ;
				break;

			case '{':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x28 ;
				break;

			case '|':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x40 ;
				break;

			case '}':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x29 ;
				break;

			case '~':
				gsm[ (*gsmLength)++ ] = 0x1B ;
				gsm[ (*gsmLength)++ ] = 0x3D ;
				break;

			case '$':
				gsm[ (*gsmLength)++ ] = 0x02 ;
				break;

			default:
				gsm[ (*gsmLength)++ ] = ascii[i] ;
				break;

		}
	}
}

int GSM_AnsiTo7bit(unsigned char * src, unsigned char * dst )
{
	unsigned char buf[9];
	unsigned char bufToPdu[16];
	int srcIdx = 0 ;
	int bytesToCopy = 0 ;
	int i ;
	int dstSize = 0 ;
	int bufToPduSize = 0 ;
	unsigned char gsmAlphabet[256];
	int gsmAlphabetLength = 0 ;

	memset(gsmAlphabet, 0 , 256);
	GSM_ConvertAsciiToGsmAlphabet(src, gsmAlphabet, &gsmAlphabetLength);
	GSM_ConvertByteToPduBuf(gsmAlphabetLength, (char *)dst, &dstSize);

	while (srcIdx < gsmAlphabetLength)
	{
		bytesToCopy = gsmAlphabetLength - srcIdx ;
		if (  bytesToCopy > 8)
			bytesToCopy = 8 ;

		memset(buf, 0 , 9);
		memset(bufToPdu, 0 , 16);

		bufToPduSize = 0 ;

		memcpy(buf, &gsmAlphabet[srcIdx], bytesToCopy);
		srcIdx+=8;

		bufToPdu[ bufToPduSize++ ] = buf[ 0 ] & 0x7F;

		for(i=1 ; i < bytesToCopy ; i++)
		{
			int mskWidth = i % 8 ;
			unsigned char msk = 0xFF >> (8 - mskWidth) ;

			bufToPdu[ bufToPduSize ] = buf[i] >> mskWidth;
			bufToPdu[ bufToPduSize - 1 ] |= ( buf[i] & msk ) << (8 - mskWidth) ;

			if (i != 7)
				bufToPduSize++;
		}

		//conver 7bit to pdu
		for (i=0; i<bufToPduSize; i++)
		{
			GSM_ConvertByteToPduBuf(bufToPdu[i], (char *)dst, &dstSize);
		}
	}

	return dstSize ;
}

void GSM_7BitToAnsi(unsigned char * src, unsigned char * dst)
{
	unsigned char buf[8];
	unsigned char length = 0 ;
	int i = 0, srcIdx = 0 ;
	int dstSize = 0 ;
	int moveWidth = 0 ;
	unsigned char byteToConvert;

	GSM_ConvertPduToByte((char *)src, &srcIdx, &length);

	while ( (dstSize < length) && (srcIdx < (int)strlen((char *)src)))
	{
		int bytesToCopy = (strlen((char *)src) - srcIdx) / 2 ;
		if ( bytesToCopy > 7 )
			bytesToCopy = 7 ;


		memset(buf, 0 , 8);
		for (i=0; i < bytesToCopy; i++)
		{
			GSM_ConvertPduToByte((char *)src, &srcIdx, &buf[i]);
		}

		for (i=7; i > 0 ; i--)
		{
			int mskWidth = i % 8;
			unsigned char msk = 0xFF << (8 - mskWidth);
			buf[i] = ((buf[i] << mskWidth) & 0x7F) | ((buf[i-1] & msk) >> (8 - mskWidth)) ;
		}
		buf[0] &= 0x7F;

		if (bytesToCopy == 7)
			bytesToCopy = 8 ;
		if (bytesToCopy > (length - dstSize) )
			bytesToCopy = length - dstSize ;

		memcpy(&dst[dstSize], buf, bytesToCopy);
		dstSize += bytesToCopy ;

		if (!bytesToCopy)
			break;
	}

	//convert GSM alphabet to ASCII
	for (i=0; i < dstSize; i++)
	{
		switch(dst[i])
		{
			case 0x00:
				moveWidth = 0 ;
				byteToConvert = '@' ;
				break;

			case 0x02:
				moveWidth = 0 ;
				byteToConvert = '$' ;
				break;

			case 0x11:
				moveWidth = 0 ;
				byteToConvert = '_' ;
				break;

			case 0x1B:
			{
				moveWidth = 1 ;
				switch(dst[i+1])
				{
					case 0x3C:
						byteToConvert = '[' ;
						break;

					case 0x2F:
						byteToConvert = '\\' ;
						break;

					case 0x3E:
						byteToConvert = ']' ;
						break;

					case 0x14:
						byteToConvert = '^' ;
						break;

					case 0x28:
						byteToConvert = '{' ;
						break;

					case 0x40:
						byteToConvert = '|' ;
						break;

					case 0x29:
						byteToConvert = '}' ;
						break;

					case 0x3D:
						byteToConvert = '~' ;
						break;

					default:
						byteToConvert = '!' ;
						break;
				}
			}
			break;


			default:
				moveWidth = 0 ;
				byteToConvert = dst[i];
				break;

		}

		if (moveWidth > 0)
		{
			memmove( &dst[i], &dst[i+moveWidth], dstSize - i + moveWidth );
			dstSize -= moveWidth ;
		}
		dst[i] = byteToConvert ;
	}

}

void GSM_GetPdu(char * message, char * phone, char * result, int * length)
{
	int i = 0;

	//set sms center number
	GSM_ConvertByteToPduBuf( 0x00, result, &i );

	//set pdu type
	GSM_ConvertByteToPduBuf( 0x01, result, &i );

	//set mr
	GSM_ConvertByteToPduBuf( 0x00, result, &i );

	//phone number
	GSM_ConvertPhoneNumerToPduBuf(phone, result, &i);

	//set pid
	GSM_ConvertByteToPduBuf( 0x00, result, &i );

	//set data coding scheme
	//00 - 7bit coding
	//08 - UCS2
	//10 - flash 7bit coding
	//18 - flash UCS2
	GSM_ConvertByteToPduBuf( 0x00, result, &i );

	//set validity period by default
	//GSM_ConvertByteToPduBuf( 0x00, result, &i );

	i += GSM_AnsiTo7bit((unsigned char *)message, (unsigned char *)&result[i]) ;
	*length = (i - 2 ) / 2 ;
}

void GSM_GetPlain(char * buf, char * phone, char * message)
{
	int i = 0, j = 0 ;
	unsigned char byte = 0;

	int phoneNumberLength = 0 ;
	char phoneNumberBuf[64] = {0};

	//get sms center phone number]
	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;

	//skip sms center phone
	i += (byte * 2) ;


	//pdu type
	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;
	if (byte & 0x01)
		return ;

	//sender phone number
	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;
	phoneNumberLength = byte;
	if (phoneNumberLength % 2)
		phoneNumberLength++ ;
	phoneNumberLength /= 2 ; //in bytes

	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;
	if (( byte== 0x91) || (byte == 0x81))
	{
		snprintf(phoneNumberBuf, 64, "+" );


		for (j=0; j < phoneNumberLength; j++)
		{
			unsigned char reverseByte;
			if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
				return ;

			reverseByte = ((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4) ;
			snprintf( &phoneNumberBuf[ strlen(phoneNumberBuf) ], 64 - strlen(phoneNumberBuf), "%02X", reverseByte );
		}

		if ( phoneNumberBuf[  strlen(phoneNumberBuf) - 1] == 'F' )
			phoneNumberBuf[  strlen(phoneNumberBuf) - 1] = '\0';
	}
	else
	{
		//there is alpabet-numeric format. Do not parse
		i += (phoneNumberLength * 2) ;
	}
	snprintf(phone, PHONE_MAX_SIZE, "%s", phoneNumberBuf);

	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;

	if ( !GSM_ConvertPduToByte(buf, &i, &byte) )
		return ;

	//skip 7 bytes date time stamp
	i+=14;

	if ( (byte & 0x0F) == 0x00)
	{
		//7 bit coding
		GSM_7BitToAnsi((unsigned char *)&buf[i], (unsigned char *)message);
	}
	else if ( (byte & 0x0F) == 0x08)
	{
		;//UCS2 coding
	}
}

void GSM_SendSms(smsBuffer_t * smsBuffer)
{
	char pdu[MAX_MESSAGE_LENGTH] = {0};
	int length = 0 ;
	char cmd[64] = {0};
	neowayCmdWA_t sendSmsStr = {(unsigned char *)cmd, answerBankSendSms[0], answerBankSendSms , 5};
	neowayCmdWA_t sendPduStr = {(unsigned char *)pdu, answerBankStandart[0], answerBankStandart , 10};

	GSM_GetPdu((char *)smsBuffer->textMessage, (char *)smsBuffer->phoneNumber, pdu, &length);
	snprintf(cmd, 64, "AT+CMGS=%d\r", length);

	if ( GSM_CmdByWaitAnswer(&sendSmsStr, NULL))
	{
		//Add CTRL+Z to the and of PDU buffer
		pdu[strlen(pdu)] = 0x1A;
		GSM_CmdByWaitAnswer(&sendPduStr, NULL);
	}
}


//
//
//

void GSM_SmsHandler()
{
	enum GSH_State_e
	{
		GSH_ReadSms,
		GSH_DeleteSms,
		GSH_HandleSms,
		GSH_Answer,
		GSH_GetSQ
	};

	char cmd[128] = {0};
	unsigned char answer[GSM_TASK_RX_SIZE]= {0};
	unsigned char pdu[GSM_TASK_RX_SIZE] = {0};
	neowayCmdWA_t cmdStr = {(unsigned char *)cmd, answerBankStandart[0], answerBankStandart , 10};
	static int neowaySmsHandlerState = GSH_ReadSms ;
	static smsBuffer_t smsStaticBuffer = { {0}, {0}, {0}, 1 };

	switch(neowaySmsHandlerState)
	{
		case GSH_ReadSms:
		{
			GSM_Protect( &neowayStatusMutex ) ;
			if ( (smsStaticBuffer.smsStorageNumber > (int)neowayStatus.smsStorSize) || (smsStaticBuffer.smsStorageNumber < 1) )
			{
				smsStaticBuffer.smsStorageNumber = 1 ;
				neowaySmsHandlerState = GSH_GetSQ ;
			}
			GSM_Unprotect( &neowayStatusMutex ) ;

			if ( neowaySmsHandlerState != GSH_GetSQ )
			{
				memset(smsStaticBuffer.incomingText, 0, SMS_TEXT_LENGTH);
				memset(smsStaticBuffer.phoneNumber, 0, PHONE_MAX_SIZE);
				memset(smsStaticBuffer.textMessage, 0, SMS_TEXT_LENGTH);

				snprintf(cmd, 128, "AT+CMGR=%d\r", smsStaticBuffer.smsStorageNumber);
				if ( GSM_CmdByWaitAnswer(&cmdStr, answer) )
				{
					neowaySmsHandlerState = GSH_DeleteSms ;
					sscanf( (char *)answer, "%*s\r\r\n+CMGR: %*s %*s\r\n%" DEF_TO_XSTR(GSM_TASK_RX_SIZE) "s", pdu );

					GSM_GetPlain((char *)pdu, (char *)smsStaticBuffer.phoneNumber, (char *)smsStaticBuffer.incomingText);

					COMMON_STR_DEBUG( DEBUG_GSM "GSM Parsed SMS in cell [%d] from phone [%s] with text [%s]",
									  smsStaticBuffer.smsStorageNumber,
									  smsStaticBuffer.phoneNumber,
									  smsStaticBuffer.incomingText);

				}
				else
					smsStaticBuffer.smsStorageNumber++;
			}
		}
			break;

		case GSH_DeleteSms:
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM Delete sms from cell [%d]", smsStaticBuffer.smsStorageNumber);

			//TODO: delete
			snprintf(cmd, 128, "AT+CMGD=%d\r", smsStaticBuffer.smsStorageNumber);

			GSM_CmdByWaitAnswer(&cmdStr, NULL);

			neowaySmsHandlerState = GSH_HandleSms ;
			if ( !strlen((char *)smsStaticBuffer.phoneNumber) || !strlen((char *)smsStaticBuffer.incomingText) )
				neowaySmsHandlerState = GSH_ReadSms ;

		}
			break;

		case GSH_HandleSms:
		{
			STORAGE_SmsNewConfig((char *)smsStaticBuffer.incomingText , (char *)smsStaticBuffer.textMessage);
			neowaySmsHandlerState = GSH_Answer ;
		}
			break;

		case GSH_Answer:
		{
			GSM_SendSms(&smsStaticBuffer);
			neowaySmsHandlerState = GSH_ReadSms ;
		}
			break;

		case GSH_GetSQ:
		{
			neowaySmsHandlerState = GSH_ReadSms ;
			snprintf(cmd, 128, "AT+CSQ\r");

			if ( GSM_CmdByWaitAnswer(&cmdStr, answer) )
			{
				unsigned int sq;
				sscanf((char *)answer, "%*s\r\r\n+CSQ: %u, %*d", &sq);

				GSM_Protect( &neowayStatusMutex ) ;
				neowayStatus.sq = sq * 100 / 31 ;
				GSM_Unprotect( &neowayStatusMutex ) ;
			}

		}
			break;
	}
}

//EOF
