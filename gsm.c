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
	GSM_CONNECTION_STATE_INIT ,
	GSM_CONNECTION_STATE_WAIT_1_MIN ,
	GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME ,
	GSM_CONNECTION_STATE_CONNECTION_OPEN ,
	GSM_CONNECTION_STATE_CONNECTION_READY ,
	GSM_CONNECTION_STATE_CONNECTION_CLOSE
};

enum listenerState_t
{
	GSM_LISTENER_STATE_WAIT_BYTE ,
	GSM_LISTENER_STATE_HANDLE_BUFFER ,
	GSM_LISTENER_STATE_CLEAR_BUFFER
};

enum readingSmsState_t
{
	GSM_SMS_READER_STATE_CREATE_TASK ,
	GSM_SMS_READER_STATE_ASK_ALL ,
	GSM_SMS_READER_STATE_ASK_ONE ,
	GSM_SMS_READER_STATE_REMOVE_AND_HANDLE ,
	GSM_SMS_READER_STATE_SEND_ANSWER ,
	GSM_SMS_READER_CHECK_SQ
};

//static variables
static pthread_mutex_t gsmTaskMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t lastDataMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cmdMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t connectionCounterMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t smsIndexMutex = PTHREAD_MUTEX_INITIALIZER;

gsmTask_t gsmTask ;
static volatile int gsmListenerState = 0 ;
static volatile int gsmConnectionState = 0 ;
static int smsReadingState = 0 ;

dateTimeStamp lastDataDts;
dateTimeStamp lastConnectionDts;
dateTimeStamp lastClosingDts;

dateTimeStamp waitingIncCallDts;

static volatile BOOL readyModuleFlag = FALSE ;
static volatile BOOL gsmReadyToRead = FALSE ;
static volatile BOOL gsmReadyToWrite = FALSE ;
static volatile BOOL gsmNeedToReboot = TRUE ;
static volatile int currentSim = SIM1 ;
static volatile BOOL gsmLastServiceAttempt = TRUE ;
static volatile BOOL gsmNeedToClose = FALSE ;
static volatile BOOL dataMode = FALSE ;
static volatile int incomingCall = GSM_ICS_NO_CALL ;

unsigned char incomingUartBuffer[GSM_R_BUFFER_LENGTH] = {0} ;
int incomingUartBufferPos = 0 ;
static volatile int StaticSignalQuality=0;

static char imei[GSM_IMEI_LENGTH + 1]={0};
static char gsmOwnIp[LINK_SIZE] = {0};
static char gsmOperatorName[GSM_OPERATOR_NAME_LENGTH]={0};
static int smsNewSmsIndex = GSM_MORE_THEN_ONE_MESSAGE_WAS_RECIEVED ;

smsBuffer_t * smsBuffer = NULL;
static int smsBufferCounter = 0;

static BOOL serviceMode = FALSE ;
static volatile int currSrvPrId = 0 ;
static volatile int srvPrIdToClose = 0 ;
//
//
//

void GSM_TaskProtect()
{
	DEB_TRACE(DEB_IDX_GSM)

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM Try to protect");
	pthread_mutex_lock(&gsmTaskMutex);
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM protected");
}

//
//
//

void GSM_TaskUnprotect()
{
	DEB_TRACE(DEB_IDX_GSM)

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM Try to unprotect");
	pthread_mutex_unlock(&gsmTaskMutex);
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM unprotected");
}

//
//
//

void GSM_RecievedSmsIndexProtect()
{
	DEB_TRACE(DEB_IDX_GSM)

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM RecievedSmsIndex Try to protect");
	pthread_mutex_lock(&smsIndexMutex);
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM RecievedSmsIndex protected");
}

//
//
//

void GSM_RecievedSmsIndexUnprotect()
{
	DEB_TRACE(DEB_IDX_GSM)

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM RecievedSmsIndex Try to unprotect");
	pthread_mutex_unlock(&smsIndexMutex);
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM RecievedSmsIndex unprotected");
}

//
//
//

void GSM_RebootStates()
{
	DEB_TRACE(DEB_IDX_GSM);

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_RebootStates start");

	pthread_mutex_lock(&lastDataMutex);
	memset( &lastDataDts , 0 , sizeof( dateTimeStamp ) );
	pthread_mutex_unlock(&lastDataMutex);

	memset( &lastConnectionDts , 0 , sizeof( dateTimeStamp ) );
	memset( &lastClosingDts , 0 , sizeof(dateTimeStamp));
	memset( gsmOperatorName , 0 , GSM_OPERATOR_NAME_LENGTH);

	gsmReadyToRead = FALSE ;
	gsmReadyToWrite = FALSE ;

	dataMode = FALSE ;
	incomingCall = GSM_ICS_NO_CALL ;

	smsNewSmsIndex = GSM_MORE_THEN_ONE_MESSAGE_WAS_RECIEVED ;

	StaticSignalQuality=0;
	gsmConnectionState = GSM_CONNECTION_STATE_INIT ;
	gsmListenerState = GSM_LISTENER_STATE_WAIT_BYTE ;
	gsmNeedToReboot = TRUE ;

	serviceMode = FALSE ;

	readyModuleFlag = FALSE ;
	currSrvPrId = 0 ;

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_RebootStates stop");
}

//
//
//

int GSM_ConectionDo( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	switch( gsmConnectionState )
	{
		case GSM_CONNECTION_STATE_INIT:
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_INIT");

			LCD_RemoveIPOCursor();

			res = GSM_Init( connection ) ;
			if ( res == OK )
			{
				gsmConnectionState = GSM_CONNECTION_STATE_WAIT_1_MIN;

				//gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME ;
				//CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_CLEAR_ERROR);				
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_ConectionDo gsm was not init. Wait 5 second and try again" );
				COMMON_Sleep( 5 * SECOND );

				gsmNeedToReboot = TRUE ;

				//CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_SET_ERROR);
				//STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_INIT_ERROR , NULL );
			}
		}
		break;

		case GSM_CONNECTION_STATE_WAIT_1_MIN:
		{
			//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_WAIT_1_MIN");

			res = GSM_Check1MinTimeout( connection ) ;
			if ( res == TRUE )
			{
				gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME ;
			}
			else
			{
				if (gsmNeedToReboot)
				{
					gsmConnectionState = GSM_CONNECTION_STATE_INIT;
				}
				else
				{
					GSM_WaitSms();
					COMMON_Sleep( SECOND );
				}
			}
		}
		break;

		case GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME:
		{
			//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME");

			res = GSM_CheckTimeToConnect( connection ) ;
			if ( res == TRUE )
			{
				gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_OPEN ;
			}
			else
			{
				if (gsmNeedToReboot)
				{
					gsmConnectionState = GSM_CONNECTION_STATE_INIT;
				}
				else
				{
					GSM_WaitSms();
					COMMON_Sleep( SECOND );
				}
			}
		}
		break;

		case GSM_CONNECTION_STATE_CONNECTION_OPEN:
		{			
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_CONNECTION_OPEN");
			res = GSM_Open( connection ) ;
			if ( res == OK )
			{
				gsmNeedToClose = FALSE ;
				LCD_SetIPOCursor();
				GSM_ConnectionCounterIncrement();
				CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_CLEAR_ERROR);
				CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, CLI_CLEAR_ERROR);

				unsigned char evDesc[EVENT_DESC_SIZE];
				COMMMON_CreateDescriptionForConnectEvent(evDesc , connection , serviceMode);
				STORAGE_JournalUspdEvent( EVENT_CONNECTED_TO_SERVER , evDesc );

				COMMON_STR_DEBUG(DEBUG_GSM "GSM Save connect event with desc: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
								 evDesc[0], evDesc[1], evDesc[2], evDesc[3], evDesc[4],
								 evDesc[5], evDesc[6], evDesc[7], evDesc[8], evDesc[9]);


				gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_READY ;

				CONNECTION_SetLastSessionProtoTrFlag(FALSE);

				if ( (connection->mode == CONNECTION_GPRS_SERVER) && (serviceMode == FALSE))
				{
					CONNECTION_SetLastSessionProtoTrFlag(TRUE);
				}
			}
			else
			{
				CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, CLI_SET_ERROR);
				gsmConnectionState = GSM_CONNECTION_STATE_INIT ;
			}
		}
		break;

		case GSM_CONNECTION_STATE_CONNECTION_READY:
		{
			//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_CONNECTION_READY");

			res = GSM_CheckConnectionTimeout( connection ) ;
			if ( res == TRUE )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM: connection timeouted, need to close");
				gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_CLOSE ;
			}
			else
			{
				COMMON_Sleep( SECOND );
			}
		}
		break;

		case GSM_CONNECTION_STATE_CONNECTION_CLOSE:
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_STATE_CONNECTION_CLOSE");


			COMMON_GetCurrentDts( &lastClosingDts );
			res = GSM_Close( connection );

			if ( (connection->mode == CONNECTION_GPRS_SERVER) && (serviceMode == FALSE))
			{
				COMMON_GetCurrentDts(&waitingIncCallDts);

				if (gsmNeedToReboot)
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM error was occured. Reboot gsm modile");
					gsmConnectionState = GSM_CONNECTION_STATE_INIT ;
				}
				else
				{
					gsmConnectionState = GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME ;
					COMMON_STR_DEBUG( DEBUG_GSM "GSM do not stop listen port");
				}

			}
			else
			{
				serviceMode = FALSE ;
				gsmNeedToReboot = TRUE ;

				gsmConnectionState = GSM_CONNECTION_STATE_INIT ;
			}
		}
		break;
	}

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CONNECTION_DO EXIT" );
	return res ;
}

//
//
//

void GSM_ListenerDo( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	switch( gsmListenerState )
	{
		case GSM_LISTENER_STATE_WAIT_BYTE:
		{
			if ( dataMode == TRUE )
			{
				COMMON_Sleep(100);
				return ;
			}

			int r = 0 ;
			//COMMON_STR_DEBUG( DEBUG_GSM "GSM_ListenerDo read" );
			if ( SERIAL_Read( PORT_GSM , &incomingUartBuffer[ incomingUartBufferPos ] , 1 , &r ) == OK )
			{
				if ( r == 1 )
				{
					//COMMON_STR_DEBUG( DEBUG_GSM "GSM_ListenerDo readed [%02X]" , incomingUartBuffer[ incomingUartBufferPos ] );
					incomingUartBufferPos++;
					if ( (incomingUartBufferPos == GSM_R_BUFFER_LENGTH) || \
						 ( incomingUartBuffer[ incomingUartBufferPos - 1 ] == '\n') || \
						 (incomingUartBuffer[ incomingUartBufferPos - 1 ] == '>') )
					{
						//COMMON_BUF_DEBUG( DEBUG_GSM "GSM_ListenerDo: " , incomingUartBuffer , incomingUartBufferPos );
						gsmListenerState = GSM_LISTENER_STATE_HANDLE_BUFFER ;
					}
				}
				else
				{
					COMMON_Sleep(50);
				}
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM read error!" );
				COMMON_Sleep( SECOND );
			}
		}
		break;

		case GSM_LISTENER_STATE_HANDLE_BUFFER:
		{
			if ( GSM_HandleIncomingBuffer(connection) == OK )
			{
				gsmListenerState = GSM_LISTENER_STATE_CLEAR_BUFFER ;
			}
			else
			{
				gsmListenerState = GSM_LISTENER_STATE_WAIT_BYTE ;
			}
		}
		break;

		case GSM_LISTENER_STATE_CLEAR_BUFFER:
		{
			gsmListenerState = GSM_LISTENER_STATE_WAIT_BYTE ;
			incomingUartBufferPos = 0 ;
			memset( incomingUartBuffer , 0 , GSM_R_BUFFER_LENGTH );
		}
		break;
	}

	return ;
}

//
//
//

void GSM_WaitSms()
{
	DEB_TRACE(DEB_IDX_GSM);

	switch( smsReadingState )
	{
		case GSM_SMS_READER_STATE_CREATE_TASK:
		{
			GSM_RecievedSmsIndexProtect();

			if( smsNewSmsIndex >= 0 )
			{
				smsReadingState = GSM_SMS_READER_STATE_ASK_ONE ;
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_WaitSms try to read sms [%d] freom storage" , smsNewSmsIndex);
			}
			else if( smsNewSmsIndex == GSM_SMS_INDEX_NO_NEW_MESSAGES )
			{
				//COMMON_STR_DEBUG( DEBUG_GSM "GSM_WaitSms there is no new sms");
				static int longTimeOutSmsRequest = 0 ;
				if ( (++longTimeOutSmsRequest) >= 600)
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM_WaitSms there is no new sms too long. Try to read all sms storage");
					longTimeOutSmsRequest = 0 ;
					smsReadingState = GSM_SMS_READER_STATE_ASK_ALL;
				}
				else
				{
					//COMMON_STR_DEBUG( DEBUG_GSM "GSM_WaitSms asking for SQ");
					++longTimeOutSmsRequest;
					smsReadingState = GSM_SMS_READER_CHECK_SQ ;
				}
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_WaitSms smsNewSmsIndex=[%d]. Try to read all sms storage" , smsNewSmsIndex);
				smsReadingState = GSM_SMS_READER_STATE_ASK_ALL ;
			}

			GSM_RecievedSmsIndexUnprotect();
		}
		break;

		case GSM_SMS_READER_STATE_ASK_ALL :
		{
			GSM_RecievedSmsIndexProtect();

			smsNewSmsIndex = GSM_SMS_INDEX_NO_NEW_MESSAGES ;
			smsReadingState = GSM_SMS_READER_STATE_CREATE_TASK ;

			GSM_RecievedSmsIndexUnprotect();

			unsigned char * answer = NULL;
			int answerLength = 0 ;
			unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

			if ( GSM_CmdWrite((unsigned char *)"AT+CMGL=\"ALL\"\r" ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						unsigned char * searchingPos = answer;
						smsBuffer_t tempSmsBuffer;
						while(1)
						{
							memset(&tempSmsBuffer , 0 , sizeof(smsBuffer_t));
							searchingPos = COMMON_FindNextSms( searchingPos , &tempSmsBuffer );
							if (searchingPos)
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM WaitSMS(): more one SMS was founded");
								smsBuffer=realloc( smsBuffer, (++smsBufferCounter)*sizeof(smsBuffer_t));
								if( !smsBuffer )
								{
									fprintf( stderr , "Can not allocate [%d] bytes\n" , smsBufferCounter * sizeof( smsBuffer_t ) );
									EXIT_PATTERN;
								}

								memcpy(&smsBuffer[smsBufferCounter-1] , &tempSmsBuffer , sizeof(smsBuffer_t));
								++searchingPos;
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM WaitSMS(): no more SMS was founded. Free buffer");
								break;
							}
						}

						if ( smsBufferCounter > 0 )
						{
							smsReadingState = GSM_SMS_READER_STATE_REMOVE_AND_HANDLE ;
						}
					}
					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		break;

		case GSM_SMS_READER_STATE_ASK_ONE :
		{
			int smsIndex = 0 ;
			GSM_RecievedSmsIndexProtect();

			if( smsNewSmsIndex >= 0 )
			{
				smsIndex = smsNewSmsIndex ;
				smsNewSmsIndex = GSM_SMS_INDEX_NO_NEW_MESSAGES ;
			}
			else if ( smsNewSmsIndex == GSM_SMS_INDEX_NO_NEW_MESSAGES )
			{
				smsReadingState = GSM_SMS_READER_CHECK_SQ ;
			}
			else
			{
				smsReadingState = GSM_SMS_READER_STATE_ASK_ALL ;
			}

			GSM_RecievedSmsIndexUnprotect();

			if ( smsReadingState == GSM_SMS_READER_STATE_ASK_ONE )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM WaitSMS(): asking only for [%d] sms in storage" , smsIndex);
				smsBufferCounter = 1 ;
				smsBuffer = malloc( smsBufferCounter * sizeof( smsBuffer_t ) ) ;
				if ( !smsBuffer )
				{
					fprintf( stderr , "Can not allocate [%d] bytes\n" , smsBufferCounter * sizeof( smsBuffer_t ) );
					EXIT_PATTERN;
				}

				memset(&smsBuffer[0] , 0  , sizeof( smsBuffer_t ) );
				smsBuffer[0].smsStorageNumber = smsIndex ;

				char cmd[32];
				memset( cmd , 0 , 32 );
				snprintf( cmd , 32 , "AT+CMGR=%d\r" , smsIndex);


				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
						{
							if ( COMMON_FindSmsIn_CMGR_Output( answer , answerLength , &smsBuffer[0] ) == OK )
							{
								smsReadingState = GSM_SMS_READER_STATE_REMOVE_AND_HANDLE ;
							}
							else
							{
								free( smsBuffer );
								smsBuffer = NULL ;
								smsBufferCounter = 0 ;
								smsReadingState = GSM_SMS_READER_STATE_ASK_ALL ;
							}
						}
						free( answer ) ;
						answer = NULL;
						answerLength = 0 ;
					}
				}
				else
				{
					smsReadingState = GSM_SMS_READER_STATE_ASK_ALL ;
				}
			}
		}
		break;

		case GSM_SMS_READER_STATE_REMOVE_AND_HANDLE :
		{
			if ( smsBufferCounter > 0 )
			{
				smsReadingState = GSM_SMS_READER_STATE_SEND_ANSWER ;
				int smsIndex = 0 ;
				for ( ; smsIndex < smsBufferCounter ; ++smsIndex )
				{
					unsigned char * answer = NULL;
					int answerLength = 0 ;
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

					char cmd[32];
					memset( cmd , 0 , 32 );
					snprintf( cmd , 32 , "AT+CMGD=%d\r" , smsBuffer[ smsIndex ].smsStorageNumber );

					if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM sms [%d] deleting OK. Handle it" , smsBuffer[ smsIndex ].smsStorageNumber );
//								int messageLength = strlen((char *)smsBuffer[smsIndex].incomingText) ;
//								COMMON_ShiftTextDown( (char *)smsBuffer[smsIndex].incomingText , messageLength );
//								if( !memcmp( &smsBuffer[smsIndex].incomingText[0] , "uspd" , strlen("uspd") ))
//								{
//									if ( COMMON_TranslateSpecialSmsEncodingToAscii( smsBuffer[smsIndex].incomingText , &messageLength ) == OK )
//									{
//										STORAGE_SmsNewConfig(smsBuffer[smsIndex].incomingText , smsBuffer[smsIndex].textMessage);
//									}
//									else
//									{
//										snprintf( (char *)smsBuffer[smsIndex].textMessage , SMS_TEXT_LENGTH , "ERROR: can not parse special symbol" );
//									}
//								}
								STORAGE_SmsNewConfig((char *)smsBuffer[smsIndex].incomingText , (char *)smsBuffer[smsIndex].textMessage);
							}

							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}
				}
			}
			else
			{
				smsReadingState = GSM_SMS_READER_STATE_CREATE_TASK ;
			}
		}
		break;

		case GSM_SMS_READER_STATE_SEND_ANSWER :
		{
			//debug
//			smsBuffer_t smsBuffer ;
//			memset( &smsBuffer , 0 , sizeof(smsBuffer_t) );
//			sprintf( (char *)smsBuffer.phoneNumber , "+79506280447" );
//			sprintf( (char *)smsBuffer.textMessage , "Taxi olen'. Ne bud' olenem, pozvoni skoree" );
//			GSM_SendSms( &smsBuffer );

			smsReadingState = GSM_SMS_READER_STATE_CREATE_TASK ;
			if ( smsBufferCounter > 0 )
			{
				int smsIndex = 0 ;
				for( ; smsIndex < smsBufferCounter ; smsIndex++)
				{
					if( strlen((char *)smsBuffer[smsIndex].textMessage) > 0)
					{
						GSM_SendSms(&smsBuffer[ smsIndex ]) ;
					}
				}

				free ( smsBuffer );
				smsBuffer = NULL;
				smsBufferCounter = 0 ;
			}
		}
		break;

		case GSM_SMS_READER_CHECK_SQ:
		{
			smsReadingState = GSM_SMS_READER_STATE_CREATE_TASK ;

			static int readingSqCounter = 0 ;
			if ( readingSqCounter > 500 )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM WaitSMS(): asking for SQ");
				readingSqCounter = 0 ;
				GSM_RememberSQ();
			}
			else
			{
				++readingSqCounter ;
			}
		}
		break;
	}

	return ;
}

//
//
//

BOOL GSM_CheckIncCallTimeout()
{
	if ( COMMON_CheckDtsForValid(&waitingIncCallDts) == OK)
	{
		//calc time before reset gsm module
		dateTimeStamp currentDts;
		COMMON_GetCurrentDts(&currentDts);

		int diff = COMMON_GetDtsDiff__Alt(&waitingIncCallDts, &currentDts, BY_SEC);
		if ( (diff > TIMEOUT3) || (diff < 0))
			return TRUE;
	}
	else
		COMMON_GetCurrentDts(&waitingIncCallDts);

	return FALSE;
}

//
//
//

BOOL GSM_CheckTimeToConnect( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckTimeToConnect start" );

	BOOL res = FALSE ;

	switch( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		{
			if (serviceMode == FALSE)
			{
				switch(incomingCall)
				{
					case GSM_ICS_NO_CALL:
					default:
						gsmNeedToReboot = GSM_CheckIncCallTimeout();
						break;

					case GSM_ICS_NEED_TO_ANSWER:
						COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckTimeToConnectServer SERVER ACC" );
						res = TRUE;
						break;
				} //switch(incomingCall)

			}
			else
			{
				//do not listen port. Try to touch service
				res = TRUE ;
			}
		}
		break;

		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_OUTGOING:
		case CONNECTION_GPRS_ALWAYS:
		{
			//if ( COMMON_CheckDtsForValid( &lastClosingDts ) == OK )
			//{
			//	dateTimeStamp currentDts;
			//	memset( &currentDts , 0 , sizeof(dateTimeStamp) );
			//	COMMON_GetCurrentDts( &currentDts );
			//	int secondDiff = COMMON_GetDtsDiff__Alt( &lastClosingDts , &currentDts , BY_SEC ) ;
			//	if ( (secondDiff > TIMEOUT2) || (secondDiff < 0))
			//	{

					res = CONNECTION_CheckTimeToConnect(&serviceMode) ;
					//res = TRUE ;
			//	}
			//}
			//else
			//{
			//	COMMON_GetCurrentDts( &lastClosingDts );
			//	COMMON_STR_DEBUG( DEBUG_GSM "GSM It is first time gsm is switched on. Wait a minute for reading sms");
			//}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		{
			switch(incomingCall)
			{
				case GSM_ICS_NO_CALL:
					gsmNeedToReboot = GSM_CheckIncCallTimeout();
					break;

				case GSM_ICS_NEED_TO_ANSWER:
					res = TRUE;
					break;

				case GSM_ICS_NEED_TO_HANG:
				{
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
					unsigned char * answer = NULL ;
					int answerLength = 0 ;
					if ( GSM_CmdWrite((unsigned char *)"ATH\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							incomingCall = GSM_ICS_NO_CALL ;
							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}
				}
					break;
			} //switch(incomingCall)
		}
		break;

		default:
			break;
	}

	if ( res == TRUE )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckTimeToConnect return TRUE" );
	}

	return res ;
}

//
//
//

BOOL GSM_Check1MinTimeout( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_Check1MinTimeout start" );

	BOOL res = FALSE ;

	switch( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_CSD_OUTGOING:
		{
			if ( COMMON_CheckDtsForValid( &lastClosingDts ) == OK )
			{
				dateTimeStamp currentDts;
				memset( &currentDts , 0 , sizeof(dateTimeStamp) );
				COMMON_GetCurrentDts( &currentDts );
				int secondDiff = COMMON_GetDtsDiff__Alt( &lastClosingDts , &currentDts , BY_SEC ) ;
				if ( (secondDiff > TIMEOUT2) || (secondDiff < 0))
				{
					res = TRUE ;
				}
			}
			else
			{
				COMMON_GetCurrentDts( &lastClosingDts );
				COMMON_STR_DEBUG( DEBUG_GSM "GSM It is first time gsm is switched on. Wait a minute for reading sms");
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		{
			res = TRUE;
		}
		break;


		default:
			break;
	}

	if ( res == TRUE )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM 1 minute timeout is over");

		if (connection->mode == CONNECTION_GPRS_SERVER)
		{
			CONNECTION_CheckTimeToConnect(&serviceMode) ;
			if (serviceMode == FALSE)
			{
				incomingCall = GSM_ICS_NO_CALL ;
				if( GSM_ConnectAsGprsServer( connection ) != OK)
				{
					res = FALSE;
				}
			}

		}
	}

	return res ;

}

//
//
//

BOOL GSM_CheckConnectionTimeout( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	BOOL res = FALSE ;

	switch( connection->mode )
	{
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_OUTGOING:
		{
			if ( gsmNeedToClose == TRUE )
			{
				res = TRUE ;
				gsmNeedToClose = FALSE ;

				STORAGE_EVENT_DISCONNECTED(0x02);
			}
			else
			{
				pthread_mutex_lock(&lastDataMutex);

				if ( COMMON_CheckDtsForValid( &lastDataDts ) == OK )
				{
					dateTimeStamp currentDts ;
					memset( &currentDts , 0 , sizeof(dateTimeStamp));
					COMMON_GetCurrentDts(&currentDts);
					int secondDiff = COMMON_GetDtsDiff__Alt( &lastDataDts , &currentDts , BY_SEC );

					if ( (secondDiff < 0 ) || (secondDiff > TIMEOUT1) )
					{
						res = TRUE ;
					}
				}
				else
				{
					res = TRUE ;
				}

				pthread_mutex_unlock(&lastDataMutex);

				if ( (res == FALSE) && ( connection->calendarGsm.ifstoptime == TRUE ))
				{
					dateTimeStamp dtsToStop;
					memcpy( &dtsToStop , &connection->calendarGsm.lastBingoDts, sizeof(dateTimeStamp) );

					if ( (connection->calendarGsm.stop_hour >= 0 ) && (connection->calendarGsm.stop_min >= 0) )
					{
						if ( ( connection->calendarGsm.stop_hour + connection->calendarGsm.stop_min ) > 0 )
						{
							int i;
							for( i = 0 ; i<(connection->calendarGsm.stop_hour) ; i++)
							{
								COMMON_ChangeDts(&dtsToStop , INC , BY_HOUR);
							}
							for( i = 0 ; i<(connection->calendarGsm.stop_min) ; i++)
							{
								COMMON_ChangeDts(&dtsToStop , INC , BY_MIN);
							}

							dateTimeStamp currentDts;
							COMMON_GetCurrentDts( &currentDts );

							res = COMMON_CheckDts(&dtsToStop , &currentDts) ;
						}
					}
				}

				if ( res == TRUE )
				{
					STORAGE_EVENT_DISCONNECTED(0x01);
				}

			}
		}
		break;

		default:
		{
			if ( connection->mode == CONNECTION_GPRS_SERVER )				
			{
				if ( srvPrIdToClose > 0 )
				{
					int srvPrIdToClose_ = srvPrIdToClose ;
					srvPrIdToClose = 0 ;

					unsigned char * answer = NULL;
					int answerLength = 0 ;
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

					char cmd[64] = {0};
					snprintf(cmd , 64, "AT^SISC=%d\r", srvPrIdToClose_);
					if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM srv profile id [%d] closed OK ", srvPrIdToClose_ );
								res = OK ;
							}
							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}
				}
			}

			pthread_mutex_lock(&lastDataMutex);

			if ( gsmNeedToClose == TRUE )
			{
				res = TRUE ;
				gsmNeedToClose = FALSE ;

				STORAGE_EVENT_DISCONNECTED(0x02);
			}
			else if ( gsmNeedToReboot == TRUE )
			{
				res = TRUE ;
				STORAGE_EVENT_DISCONNECTED(0x02);
			}
			else
			{

				if ( COMMON_CheckDtsForValid( &lastDataDts ) == OK )
				{
					dateTimeStamp currentDts ;
					memset( &currentDts , 0 , sizeof(dateTimeStamp));
					COMMON_GetCurrentDts(&currentDts);
					int secondDiff = COMMON_GetDtsDiff__Alt( &lastDataDts , &currentDts , BY_SEC );

					if ( (secondDiff < 0 ) || (secondDiff > TIMEOUT1) )
					{
						STORAGE_EVENT_DISCONNECTED(0x01);
						res = TRUE ;
					}
				}
				else
				{
					res = TRUE ;
				}
			}

			pthread_mutex_unlock(&lastDataMutex);
		}
		break;
	}


	return res;
}

//
//
//

int GSM_ConnectAsGprsClient( connection_t * connection )
{

	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	unsigned char * answer = NULL;
	int answerLength = 0 ;

	currSrvPrId = 0 ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM_ConnectAsGprsClient() creating connection profile for grps connection");
	unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
	if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,conType,GPRS0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: conType OK");
				res = OK ;
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: conType ERROR");
				gsmNeedToReboot = TRUE ;
			}

			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,alphabet,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: alphabet OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: alphabet ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		 res = ERROR_GENERAL ;
		 if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,inactTO,60\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		 {
			 if ( answer )
			 {
				 if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				 {
					 COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: inactTO OK");
					 res = OK ;
				 }
				 else
				 {
					 COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: inactTO ERROR");
					 gsmNeedToReboot = TRUE ;
				 }

				 free( answer ) ;
				 answer = NULL;
				 answerLength = 0 ;
			 }
		 }
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		char username[FIELD_SIZE];
		memset(username , 0 , FIELD_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( username , FIELD_SIZE , GSM_PART_USER );
			}
			else
			{
				snprintf( username , FIELD_SIZE , "%s" , (char *)connection->apn1_user );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( username , FIELD_SIZE , GSM_PART_USER );
			}
			else
			{
				snprintf( username , FIELD_SIZE , "%s" , (char *)connection->apn2_user );
			}
		}

		if ( strlen(username) > 0 )
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,user,%s\r" , username);

			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username [%s] OK", username);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username [%s] ERROR", username);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}

		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username is empty");
			res = OK ;
		}
	}


	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		char password[FIELD_SIZE];
		memset(password , 0 , FIELD_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( password , FIELD_SIZE , GSM_PART_PASS );
			}
			else
			{
				snprintf( password , FIELD_SIZE , "%s" , (char *)connection->apn1_pass );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( password , FIELD_SIZE , GSM_PART_PASS );
			}
			else
			{
				snprintf( password , FIELD_SIZE , "%s" , (char *)connection->apn2_pass );
			}
		}

		if ( strlen( password ) > 0)
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,passwd,%s\r", password);

			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd [%s] OK", password);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd [%s] ERROR", password);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd is empty");
			res = OK ;
		}
	}

	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		char apn[LINK_SIZE];
		memset(apn , 0 , LINK_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( apn , LINK_SIZE , GSM_PART_APN );
			}
			else
			{
				snprintf( apn , LINK_SIZE , "%s" , (char *)connection->apn1 );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( apn , LINK_SIZE , GSM_PART_APN );
			}
			else
			{
				snprintf( apn , LINK_SIZE , "%s" , (char *)connection->apn2 );
			}
		}

		if ( strlen( apn ) > 0 )
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,apn,%s\r", apn);
			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					//COMMON_STR_DEBUG( DEBUG_GSM ">>%s" , answer);
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn [%s] OK" , apn);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn [%s] ERROR", apn);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn is empty. Error");
			CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_SET_ERROR);
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,srvType,Socket\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: service type OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: service type ERROR");
					gsmNeedToReboot = TRUE ;
				}


				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,conId,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: conId OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: conId ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,alphabet,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: alphabet OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: alphabet ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;

		unsigned char cmd[256];
		memset(cmd , 0 , 256);

		char ip[IP_SIZE];
		memset(ip , 0 , IP_SIZE);
		unsigned int port = 0 ;

		if ( serviceMode == TRUE )
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM It is time for service connect" );

			#if REV_COMPILE_STASH_NZIF_IP
				//to do change if 212.67.9.43:10100 to 212.67.9.44:58197
				char stashIp[ IP_SIZE ];
				memset( stashIp , 0 , IP_SIZE );
				snprintf( stashIp , IP_SIZE , "%d.%d.%d.%d" , 212 , 67 , 9 , 43 );

				if ( !strcmp( (char *)connection->serviceIp , stashIp ) && (connection->servicePort == 10100) )
				{
					snprintf( ip , IP_SIZE , "%d.%d.%d.%d" , 212 , 67 , 9 , 44  );
					port = 58198; //was 58197
				}
				else
				{
					snprintf( ip , IP_SIZE , "%s" , connection->serviceIp );
					port = connection->servicePort;
				}
			#else
				snprintf( ip , IP_SIZE , "%s" , connection->serviceIp );
				port = connection->servicePort;
			#endif
		}
		else
		{
			#if REV_COMPILE_STASH_NZIF_IP
				//change 212.67.9.43:10101 to 212.67.9.44:58198
				char stashIp[ IP_SIZE ];
				memset( stashIp , 0 , IP_SIZE );
				snprintf( stashIp , IP_SIZE , "%d.%d.%d.%d" , 212 , 67 , 9 , 43 );

				if ( !strcmp( (char *)connection->server , stashIp ) && (connection->port == 10101) )
				{
					snprintf( ip , IP_SIZE , "%d.%d.%d.%d" , 212 , 67 , 9 , 44  );
					port = 58198;
				}
				else
				{
					snprintf( ip , IP_SIZE , "%s" , connection->server );
					port = connection->port;
				}
			#else
				snprintf( ip , IP_SIZE , "%s" , connection->server );
				port = connection->port ;
			#endif
		}

		snprintf((char *)cmd , 256 , "AT^SISS=0,address,\"socktcp://%s:%u\"\r", ip , port);

		if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: set addres [%s] : [%u] OK" , ip , port);
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: set addres ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,tcpOT,60\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: tcpOT OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: tcpOT ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Try to open connection");
		res = ERROR_GENERAL ;

		GSM_RecievedSmsIndexProtect();

		smsNewSmsIndex = GSM_SMS_INDEX_CHECK_AFTER_OPEN ;

		GSM_RecievedSmsIndexUnprotect();

		if ( GSM_CmdWrite((unsigned char *)"AT^SISO=0\r" ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					if ( (serviceMode == FALSE) && ( connection->mode == CONNECTION_GPRS_CALENDAR ) )
					{
						COMMON_GetCurrentDts( &connection->calendarGsm.lastBingoDts );
					}

					pthread_mutex_lock(&lastDataMutex);
					COMMON_GetCurrentDts(&lastDataDts);
					pthread_mutex_unlock(&lastDataMutex);

					COMMON_GetCurrentDts(&lastConnectionDts);
					COMMON_STR_DEBUG( DEBUG_GSM "GSM open connection OK");
					res = OK ;
				}
				else
				{
					gsmNeedToReboot = TRUE ;
					COMMON_STR_DEBUG( DEBUG_GSM "GSM open connection ERROR");
					if ( serviceMode == TRUE )
					{
						CONNECTION_ReverseLastServiceAttemptFlag();
					}
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
		else
		{
			gsmNeedToReboot = TRUE ;

			COMMON_STR_DEBUG( DEBUG_GSM "GSM open connection timeout");
			if ( serviceMode == TRUE )
			{
				CONNECTION_ReverseLastServiceAttemptFlag();
			}
		}
	}

	if ( res == OK )
	{
		if ( GSM_CmdWrite((unsigned char *)"AT^SISO?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				//TODO: get client ip and port
				COMMON_STR_DEBUG( DEBUG_GSM " %s", answer);

				/*
				AT^SISO?
				^SISO: 0,"Socket","3","3","0","0","46.250.52.54:58198","0.0.0.0:0"
				^SISO: 1,"Socket","5","4","0","0","46.250.52.54:58198","95.79.55.25:48077"
				^SISO: 2,"Socket","2","4","0","0","46.250.52.54:58198","95.79.55.25:48079"
				^SISO: 3,"Socket","2","4","0","0","46.250.52.54:58198","89.113.4.190:3166"
				^SISO: 4,""
				^SISO: 5,""
				^SISO: 6,""
				^SISO: 7,""
				^SISO: 8,""
				^SISO: 9,""

				OK
				*/

				char ownIp[LINK_SIZE] = {0};
				GSM_ParseSisoOutput((char *)answer, 0, ownIp, NULL, NULL, NULL);
				strncpy( gsmOwnIp, ownIp , LINK_SIZE );

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	return res;

}

//
//
//

int GSM_ConnectAsGprsServer( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	unsigned char * answer = NULL;
	int answerLength = 0 ;

	currSrvPrId = 0 ;

	COMMON_STR_DEBUG(DEBUG_GSM "GSM_ConnectAsGprsServer() creating connection profile for grps connection");
	unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
	if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,conType,GPRS0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: conType OK");
				res = OK ;
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: conType ERROR");
				gsmNeedToReboot = TRUE ;
			}

			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,alphabet,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: alphabet OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: alphabet ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		 res = ERROR_GENERAL ;
		 if ( GSM_CmdWrite((unsigned char *)"AT^SICS=0,inactTO,60\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		 {
			 if ( answer )
			 {
				 if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				 {
					 COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: inactTO OK");
					 res = OK ;
				 }
				 else
				 {
					 COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: inactTO ERROR");
					 gsmNeedToReboot = TRUE ;
				 }

				 free( answer ) ;
				 answer = NULL;
				 answerLength = 0 ;
			 }
		 }
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		char username[FIELD_SIZE];
		memset(username , 0 , FIELD_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( username , FIELD_SIZE , GSM_PART_USER );
			}
			else
			{
				snprintf( username , FIELD_SIZE , "%s" , (char *)connection->apn1_user );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( username , FIELD_SIZE , GSM_PART_USER );
			}
			else
			{
				snprintf( username , FIELD_SIZE , "%s" , (char *)connection->apn2_user );
			}
		}

		if ( strlen(username) > 0 )
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,user,%s\r" , username);

			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username [%s] OK", username);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username [%s] ERROR", username);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}

		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: username is empty");
			res = OK ;
		}
	}


	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		char password[FIELD_SIZE];
		memset(password , 0 , FIELD_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( password , FIELD_SIZE , GSM_PART_PASS );
			}
			else
			{
				snprintf( password , FIELD_SIZE , "%s" , (char *)connection->apn1_pass );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( password , FIELD_SIZE , GSM_PART_PASS );
			}
			else
			{
				snprintf( password , FIELD_SIZE , "%s" , (char *)connection->apn2_pass );
			}
		}

		if ( strlen( password ) > 0)
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,passwd,%s\r", password);

			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd [%s] OK", password);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd [%s] ERROR", password);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: passwd is empty");
			res = OK ;
		}
	}

	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		char apn[LINK_SIZE];
		memset(apn , 0 , LINK_SIZE * sizeof(char));

		if ( currentSim == SIM1 )
		{
			if ( (strlen((char *)connection->apn1_user)==0) && (strlen((char *)connection->apn1_pass)==0) && (strlen((char *)connection->apn1)==0) )
			{
				GSM_GetDefaultOperatorApn( apn , LINK_SIZE , GSM_PART_APN );
			}
			else
			{
				snprintf( apn , LINK_SIZE , "%s" , (char *)connection->apn1 );
			}
		}
		else
		{
			if ( (strlen((char *)connection->apn2_user)==0) && (strlen((char *)connection->apn2_pass)==0) && (strlen((char *)connection->apn2)==0) )
			{
				GSM_GetDefaultOperatorApn( apn , LINK_SIZE , GSM_PART_APN );
			}
			else
			{
				snprintf( apn , LINK_SIZE , "%s" , (char *)connection->apn2 );
			}
		}

		if ( strlen( apn ) > 0 )
		{
			unsigned char cmd[256];
			memset(cmd , 0 , 256);
			snprintf( (char *)cmd , 256 , "AT^SICS=0,apn,%s\r", apn);
			if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					//COMMON_STR_DEBUG( DEBUG_GSM ">>%s" , answer);
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn [%s] OK" , apn);
						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn [%s] ERROR", apn);
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM creating connection profile: apn is empty. Error");
			CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_SET_ERROR);
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,srvType,Socket\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: service type OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: service type ERROR");
					gsmNeedToReboot = TRUE ;
				}


				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,conId,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: conId OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: conId ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,alphabet,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: alphabet OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: alphabet ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}


	if ( res == OK)
	{
		unsigned char cmd[256];
		memset(cmd , 0 , 256);
		snprintf( (char *)cmd , 256 , "AT^SISS=0,address,\"socktcp://listener:%u\"\r", connection->port);
		res = ERROR_GENERAL ;

		if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: set listener at [%u] OK", connection->port);
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: set listener ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}


	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISS=0,tcpOT,60\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: tcpOT OK");
					res = OK ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM creating service profile: tcpOT ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK)
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT^SISO=0\r" ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM open connection listener OK");					
					res = OK ;

					CONNECTION_SetLastSessionProtoTrFlag(TRUE);
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM open connection listener ERROR");
					gsmNeedToReboot = TRUE ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	//debug
	if ( GSM_CmdWrite((unsigned char *)"AT^SISO?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			COMMON_STR_DEBUG( DEBUG_GSM " %s", answer);

			char ownIp[LINK_SIZE] = {0};
			GSM_ParseSisoOutput((char *)answer, 0, ownIp, NULL, NULL, NULL);
			strncpy( gsmOwnIp, ownIp , LINK_SIZE );

			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}


	return res ;
}

//
//
//

int GSM_Open( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;
	unsigned char * answer = NULL;
	int answerLength = 0 ;


	COMMON_STR_DEBUG(DEBUG_GSM "GSM_Open() start. Current connection mode is [%d]" , connection->mode );

	switch( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		{
			if (serviceMode == TRUE)
			{
				res = GSM_ConnectAsGprsClient(connection);
			}
			else
			{
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				char cmd[64]  = {0};
				snprintf(cmd , 64 , "AT^SISO=%d\r", currSrvPrId);

				if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{						
						if (strstr((char *)answer, "OK"))
							res = OK ;

						free( answer ) ;
						answer = NULL;
						answerLength = 0 ;
					}
				}

				if ( GSM_CmdWrite((unsigned char *)"AT^SISO?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						//TODO: get client ip and port
						COMMON_STR_DEBUG( DEBUG_GSM " %s", answer);

						/*
						AT^SISO?
						^SISO: 0,"Socket","3","3","0","0","46.250.52.54:58198","0.0.0.0:0"
						^SISO: 1,"Socket","5","4","0","0","46.250.52.54:58198","95.79.55.25:48077"
						^SISO: 2,"Socket","2","4","0","0","46.250.52.54:58198","95.79.55.25:48079"
						^SISO: 3,"Socket","2","4","0","0","46.250.52.54:58198","89.113.4.190:3166"
						^SISO: 4,""
						^SISO: 5,""
						^SISO: 6,""
						^SISO: 7,""
						^SISO: 8,""
						^SISO: 9,""

						OK
						*/

						char clientIp[LINK_SIZE] = {0};
						int clientPort = 0 ;
						GSM_ParseSisoOutput((char *)answer, currSrvPrId, NULL, NULL, clientIp, &clientPort);

						COMMON_STR_DEBUG(DEBUG_GSM "GSM Parsed client ip [%s]:[%u]", clientIp, clientPort);

						strcpy( (char *)connection->clientIp, clientIp );
						connection->clientPort = clientPort ;

						free( answer ) ;
						answer = NULL;
						answerLength = 0 ;
					}
				}

				incomingCall = GSM_ICS_NO_CALL ;

				pthread_mutex_lock(&lastDataMutex);
				COMMON_GetCurrentDts(&lastDataDts);
				pthread_mutex_unlock(&lastDataMutex);

				COMMON_GetCurrentDts(&lastConnectionDts);
			}
		}
		break;

		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		{
			res = GSM_ConnectAsGprsClient(connection);

			#if 0
			static int gprsUnsuccessfullConnectCounter = 0 ;
			if ( res == OK )
			{
				gprsUnsuccessfullConnectCounter = 0 ;
			}
			else
			{
				if ( (++gprsUnsuccessfullConnectCounter) > 360 )
				{
					gprsUnsuccessfullConnectCounter = 0 ;
					gsmNeedToReboot = TRUE ;
				}
			}
			#endif
		}
		break;

		case CONNECTION_CSD_OUTGOING:
		{
			unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_DIALER ;
			char cmd[ 64 ] = { 0 } ;

			if ( serviceMode == TRUE )
			{
				snprintf( cmd , 64 , "ATD%s\r", (char *)connection->servicePhone);
			}
			else
			{
				if ( currentSim == SIM1 )
					snprintf( cmd , 64 , "ATD%s\r", (char *)connection->serverPhoneSim1);
				else
					snprintf( cmd , 64 , "ATD%s\r", (char *)connection->serverPhoneSim2);
			}

			COMMON_STR_DEBUG( DEBUG_GSM "GSM >> %s", cmd);

			if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nCONNECT" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM CSD connection OK");

						if ( serviceMode == FALSE )
						{
							COMMON_GetCurrentDts( &connection->calendarGsm.lastBingoDts );
						}

						pthread_mutex_lock(&lastDataMutex);
						COMMON_GetCurrentDts(&lastDataDts);
						pthread_mutex_unlock(&lastDataMutex);

						COMMON_GetCurrentDts(&lastConnectionDts);

						dataMode = TRUE ;

						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM CSD connection ERROR");
						gsmNeedToReboot = TRUE ;

						if ( serviceMode == TRUE )
						{
							CONNECTION_ReverseLastServiceAttemptFlag();
						}
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM CSD connection timeout");

				gsmNeedToReboot = TRUE ;

				if ( serviceMode == TRUE )
				{
					CONNECTION_ReverseLastServiceAttemptFlag();
				}
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		{

			unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_DIALER ;
			if ( GSM_CmdWrite((unsigned char *)"ATA\r" ,FALSE, GSM_LARGE_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nCONNECT" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM CSD connection OK");

						pthread_mutex_lock(&lastDataMutex);
						COMMON_GetCurrentDts(&lastDataDts);
						pthread_mutex_unlock(&lastDataMutex);

						COMMON_GetCurrentDts(&lastConnectionDts);

						dataMode = TRUE ;

						res = OK ;
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM CSD connection ERROR");
						gsmNeedToReboot = TRUE ;
					}

					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}

			incomingCall = GSM_ICS_NO_CALL ;
		}
		break;

		default:
			break;
	}

	return res;
}

//
//
//

int GSM_Init( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	if(strlen(gsmOwnIp) == 0)
		snprintf( gsmOwnIp, LINK_SIZE,  "0.0.0.0" );

	memset(&waitingIncCallDts, 0 , sizeof(dateTimeStamp));

	GSM_ChoiceSim(connection);

	if ( (gsmNeedToReboot == TRUE) || (StaticSignalQuality == 0) )
	{
		res = GSM_PowerOnModule();
		if ( res != OK ) { return res ; }

		res = GSM_CheckNetworkRegistration( connection );
		if ( res != OK ) { return res ; }

		res = GSM_CastInitiateAtCommands( connection );
		if ( res != OK ) { return res ; }

		res = GSM_RememberSQ();
	}
	else
	{
		res = GSM_RememberSQ();
		if ( StaticSignalQuality == 0 )
		{
			gsmNeedToReboot = TRUE ;
		}
	}

	return res;
}

//
//
//

void GSM_ChoiceSim( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim start");

	switch( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_OUTGOING:
		case CONNECTION_CSD_INCOMING:
		{
			switch( connection->simPriniciple )
			{
				default:
				case SIM_FIRST:
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim SIM_FIRST");
					currentSim = SIM1;
				}
				break;

				case SIM_SECOND:
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim SIM_SECOND");
					currentSim = SIM2;
				}
				break;

				case SIM_FIRST_THEN_SECOND:
				case SIM_SECOND_THEN_FIRST:
				{
					gsmNeedToReboot = TRUE ;
					memset( gsmOperatorName , 0 , GSM_OPERATOR_NAME_LENGTH );
					COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim SIM_FIRST_THEN_SECOND or SIM_SECOND_THEN_FIRST. change sim");
					if ( currentSim == SIM1 )
					{
						STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_SIM_CHANGED , (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
						currentSim = SIM2;
					}
					else
					{
						STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_SIM_CHANGED , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
						currentSim = SIM1;
					}
				}
				break;
			}
		}
		break;

		default:
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim current connection type is [%d]. change sim to [%u]" , connection->mode , connection->simPriniciple);
			if ( currentSim == SIM1 )
			{
				currentSim = SIM2;
			}
			else
			{
				currentSim = SIM1;
			}
		}
		break;
	}

	if ( currentSim == SIM1 )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim sim is 1");
	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_ChoiceSim sim is 2");
	}

	return ;
}

//
//
//

int GSM_PowerOnModule()
{
	DEB_TRACE(DEB_IDX_GSM);

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_PowerOnModule() start");

	int res = ERROR_GENERAL ;


	readyModuleFlag = FALSE ;

	//select SIM card
	res = PIO_SetValue(PIO_GSM_SIM_SEL, currentSim );
	if (res!=OK) return res;
	//
	// process power off
	//
	res = PIO_SetValue(PIO_GSM_POKIN, 1);
	if (res!=OK) return res;
	usleep(500);
	res = PIO_SetValue(PIO_GSM_NOT_OE, 1);
	if (res!=OK) return res;
	usleep(1000);
	res = PIO_SetValue(PIO_GSM_PWR, 0);
	if (res!=OK) return res;

	COMMON_Sleep( SECOND * 60 );

	//
	// process power on
	//
	res = PIO_SetValue(PIO_GSM_PWR, 1);
	if (res!=OK) return res;
	usleep(1000);


	res = PIO_SetValue(PIO_GSM_POKIN, 0);
	if (res!=OK) return res;
	usleep(3000);
	res = PIO_SetValue(PIO_GSM_NOT_OE, 0);
	if (res!=OK) return res;
	usleep(3000);
	res = PIO_SetValue(PIO_GSM_POKIN, 1);
	if (res!=OK) return res;
	usleep(1000);

	//waiting for SYSSTART message

	res = ERROR_GENERAL ;

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_PowerOnModule waiting for readyModuleFlag");

	int attemptCounter = 600 ;
	while( attemptCounter > 0 )
	{
		if( readyModuleFlag == TRUE )
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_PowerOnModule readyModuleFlag OK");
			res = OK ;
			break;
		}
		else
		{
			--attemptCounter;
			COMMON_Sleep(100);
		}
	}

	if( res != OK )
	{
		CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_SET_ERROR);
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_PowerOnModule can not find sysstart async message! error!");
		STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_INIT_ERROR , NULL );
	}
	else
	{
		CLI_SetErrorCode(CLI_ERROR_INIT_GSM_MODULE, CLI_CLEAR_ERROR);
		gsmNeedToReboot = FALSE ;
		memset(&lastClosingDts , 0 , sizeof(dateTimeStamp) );

		COMMON_Sleep( 10 * SECOND );
	}


	COMMON_STR_DEBUG( DEBUG_GSM "GSM_PowerOnModule() return %s", getErrorCode(res) );

	return res ;
}

//
//
//

int GSM_CheckNetworkRegistration( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_GSM);

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration start");

	int res = ERROR_GENERAL ;

	enum{
		CHECKING_NETWORK_REGISTRATION_STATE_CHECK_AT ,
		CHECKING_NETWORK_REGISTRATION_STATE_CHECK_PIN ,
		CHECKING_NETWORK_REGISTRATION_STATE_ENTER_PIN ,
		CHECKING_NETWORK_REGISTRATION_STATE_GET_IMEI ,
		CHECKING_NETWORK_REGISTRATION_STATE_GET_CREG ,
		CHECKING_NETWORK_REGISTRATION_STATE_GET_OP_NAME ,
		CHECKING_NETWORK_REGISTRATION_STATE_STK_MODE ,
		CHECKING_NETWORK_REGISTRATION_STATE_SETTING_STK_MODE ,
		CHECKING_NETWORK_REGISTRATION_STATE_RESTART ,
		CHECKOUT_NETWORK_REGISTRATION_STATE_SET_STGI ,
		CHECKOUT_NETWORK_REGISTRATION_STATE_GET_IMSI ,
		CHECKING_NETWORK_REGISTRATION_STATE_DONE
	};

	BOOL checkingCpinFirstTime = TRUE ;
	int networkRegistrationAttemptCounter = 30 ;

	int networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_CHECK_AT ;
	while( networkRegistrationState != CHECKING_NETWORK_REGISTRATION_STATE_DONE )
	{
		switch( networkRegistrationState )
		{
			case CHECKING_NETWORK_REGISTRATION_STATE_CHECK_AT:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking AT");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite((unsigned char *)"AT\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM module ready for AT commands");
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_CHECK_PIN ;
						}

						free( answer ) ;
						answer = NULL;
					}
					else
					{
						networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
					}
				}
				else
				{
					networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_CHECK_PIN:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking PIN");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite((unsigned char *)"AT+CPIN?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM >>%s" , answer );
						if ( checkingCpinFirstTime == TRUE )
						{
							checkingCpinFirstTime = FALSE ;
							if ( strstr((char *)answer , "READY") )
							{
								if ( currentSim == SIM1 )
								{
									if ( connection->gsmCpinProperties[0].usage == TRUE )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM expected pin code request, but sim1 is ready");
										STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM PIN OK");
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_GET_IMEI ;
									}
								}
								else
								{
									if ( connection->gsmCpinProperties[1].usage == TRUE )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM expected pin code request, but sim2 is ready");
										STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM PIN OK");
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_GET_IMEI ;
									}
								}
							}
							else if ( strstr((char *)answer , "SIM PIN") )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM EXPECT PIN ENTERING");
								if ( currentSim == SIM1 )
								{
									if ( connection->gsmCpinProperties[0].usage == TRUE )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM there is pin in the configuration");
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_ENTER_PIN ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM there is no pin in the configuration. Error");
										STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
									}
								}
								else
								{
									if ( connection->gsmCpinProperties[1].usage == TRUE )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM there is pin in the configuration");
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_ENTER_PIN ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM there is no pin in the configuration. Error");
										STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
									}
								}

							}
							else if ( strstr((char *)answer , "ERROR") )
							{
								//there is no sim in the slot
//								if ( currentSim == SIM1 )
//								{
//									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_NETWORK_REGISTER_ERR , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
//								}
//								else
//								{
//									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_NETWORK_REGISTER_ERR , (unsigned char *)"\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00" );
//								}
								networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
							}
							else
							{
								if ( currentSim == SIM1 )
								{
									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
								}
								else
								{
									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
								}
								networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
							}
						}
						else
						{
							if ( strstr((char *)answer , "READY") )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM PIN OK");
								networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_GET_IMEI ;
							}
							else
							{
								if ( currentSim == SIM1 )
								{
									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
								}
								else
								{
									STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_PIN_INVALID , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
								}
								networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
							}
						}
						free( answer ) ;
						answer = NULL;
					}
					else
					{
						networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
					}
				}
				else
				{
					networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_ENTER_PIN:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration enter pin code");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char cmd[64];
				memset( cmd , 0 , 64);
				if ( currentSim == SIM1 )
				{
					snprintf( (char *)cmd , 64 , "AT+CPIN=\"%04u\"\r" , connection->gsmCpinProperties[0].pin );
				}
				else
				{
					snprintf( (char *)cmd , 64 , "AT+CPIN=\"%04u\"\r" , connection->gsmCpinProperties[1].pin );
				}

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM pin code entering OK");
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_CHECK_PIN ;
						}
						else
						{
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
						}

						free( answer ) ;
						answer = NULL;
					}
					else
					{
						networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
					}
				}
				else
				{
					networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				}
			}
			break;


			case CHECKING_NETWORK_REGISTRATION_STATE_GET_IMEI:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking IMEI");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite((unsigned char *)"AT+GSN\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							int imeiPos = 0 ;
							int inx = 0;
							for( inx = 0 ; inx < answerLength ; ++inx )
							{
								if ( ( answer[ inx ] >= 0x30 ) && ( answer[ inx ] <= 0x39 ) )
								{
									imei[ imeiPos++ ] = answer[ inx ] ;
									if ( imeiPos == GSM_IMEI_LENGTH )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM IMEI parsed [%s]" , imei );
										break;
									}
								}
							}
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_GET_CREG ;
						}
						else
						{
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
						}
						free( answer ) ;
						answer = NULL;
					}
					else
					{
						networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
					}
				}
				else
				{
					networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				}

			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_GET_CREG:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking CREG");
				if ( networkRegistrationAttemptCounter > 0 )
				{
					unsigned char * answer = NULL;
					int answerLength = 0 ;
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
					if ( GSM_CmdWrite((unsigned char *)"AT+CREG?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							if ( strstr((char *)answer , "\r\nOK\r\n") )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM >>%s" , answer );

								if((strstr((char *)answer, "CREG: 0,1") ) || (strstr((char *)answer, "CREG: 0,5")) )
								{
									networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_GET_OP_NAME ;
								}
								else if((strstr((char *)answer, "CREG: 0,0") ) || (strstr((char *)answer, "CREG: 0,3")) || (strstr((char *)answer, "CREG: 0,4")))
								{
									COMMON_STR_DEBUG( DEBUG_GSM "GSM Module is not registering now. May be there is no sim");
									networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
								}
								else //if ( strstr( (char *)answer , "CREG: 0,2" ) )
								{
									COMMON_STR_DEBUG( DEBUG_GSM "GSM Module is regisring now. Waiting");
									--networkRegistrationAttemptCounter;
									COMMON_Sleep( 4 * SECOND);
								}
							}
							else
							{
								networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
							}
							free( answer ) ;
							answer = NULL;
						}
						else
						{
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
						}
					}
					else
					{
						networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
					}
				}
				else
				{
					if ( currentSim == SIM1 )
					{
						STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_NETWORK_REGISTER_ERR , (unsigned char *)"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" );
					}
					else
					{
						STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_NETWORK_REGISTER_ERR , (unsigned char *)"\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00" );
					}
					networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_GET_OP_NAME:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking OP");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
				if ( GSM_CmdWrite((unsigned char *)"AT+COPS?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							//parsing gsm operator name
							memset(gsmOperatorName , 0 , GSM_OPERATOR_NAME_LENGTH);
							char * startNamePtr = strstr( (char *)answer , "\"" );
							if( startNamePtr )
							{
								++startNamePtr;
								char * endNamePtr = strstr( startNamePtr , "\"" );
								if( endNamePtr )
								{
									int nameLength = endNamePtr - startNamePtr ;

									if( nameLength >= GSM_OPERATOR_NAME_LENGTH )
									{
										memcpy( gsmOperatorName , startNamePtr , GSM_OPERATOR_NAME_LENGTH - 1 );
									}
									else
									{
										memcpy( gsmOperatorName , startNamePtr , nameLength );
									}

									COMMON_STR_DEBUG( DEBUG_GSM "GSM operator name parsed [%s]" , gsmOperatorName );

									networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_STK_MODE ;

									//res = OK ;
								}
							}
						}

						free( answer ) ;
						answer = NULL;
					}
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_STK_MODE :
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration checking STK mode");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				if ( GSM_CmdWrite((unsigned char *)"AT^STSF?\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							COMMON_STR_DEBUG(DEBUG_GSM ">>%s" , answer );

							unsigned char * startPos = (unsigned char *)strstr( (char *)answer , "AT^STSF?\r" );
							if ( startPos )
							{
								const int tokenNumber = 10 ;
								int tokenCounter = 0 ;
								char * token[tokenNumber];
								char * pch;

								char * lastPtr = NULL ;
								pch=strtok_r((char *)startPos, " :\r\n" , &lastPtr);

								while (pch!=NULL)
								{
									token[ tokenCounter++ ] = pch;
									pch=strtok_r(NULL, " :\r\n" , &lastPtr);
									if ( tokenCounter == tokenNumber )
										break;
								}

								if ( tokenCounter == 4 )
								{
									int stkMode = atoi( token[2] );
									COMMON_STR_DEBUG(DEBUG_GSM "GSM STK mode is [%d]" , stkMode );

									if ( stkMode == 1 )
									{
										networkRegistrationState = CHECKOUT_NETWORK_REGISTRATION_STATE_SET_STGI ;
									}
									else
									{
										networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_SETTING_STK_MODE ;
									}
								}
							}
						}
						free( answer ) ;
						answer = NULL;
					}
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_SETTING_STK_MODE :
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration set STK mode");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				if ( GSM_CmdWrite((unsigned char *)"AT^STSF=1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						COMMON_STR_DEBUG(DEBUG_GSM ">>%s" , answer );
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_RESTART ;
						}

						free( answer ) ;
						answer = NULL;
					}
				}
			}
			break;

			case CHECKING_NETWORK_REGISTRATION_STATE_RESTART:
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration restart");
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				if ( GSM_CmdWrite((unsigned char *)"AT+CFUN=1,1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						COMMON_STR_DEBUG(DEBUG_GSM ">>%s" , answer );
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{
							readyModuleFlag = FALSE ;
							COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration waiting for readyModuleFlag");
							int attemptCounter = 600 ;
							while( attemptCounter > 0 )
							{
								if( readyModuleFlag == TRUE )
								{
									COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration readyModuleFlag OK");
									networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_CHECK_AT ;
									break;
								}
								else
								{
									--attemptCounter;
									COMMON_Sleep(100);
								}
							}
						}

						free( answer ) ;
						answer = NULL;
					}
				}
			}
			break;

			case CHECKOUT_NETWORK_REGISTRATION_STATE_SET_STGI:
			{
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration set stgi");

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				if ( GSM_CmdWrite((unsigned char *)"AT^STGI=0,0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
				{
					if ( answer )
					{
						COMMON_STR_DEBUG(DEBUG_GSM ">>%s" , answer );
						if ( strstr((char *)answer , "\r\nOK\r\n") )
						{							
							networkRegistrationState = CHECKOUT_NETWORK_REGISTRATION_STATE_GET_IMSI ;
						}

						free( answer ) ;
						answer = NULL;
					}
				}
			}
			break;

			case CHECKOUT_NETWORK_REGISTRATION_STATE_GET_IMSI:
			{
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration get imsi");

				BOOL needToSendSms = FALSE ;
				int sim = 1 ;
				if ( currentSim == SIM2)
					sim = 2;

				char imsi[32];
				memset( imsi , 0 , 32 );

				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				if ( (strlen((char *)connection->smsReportPhone) > 0) && (connection->smsReportAllow == TRUE) )
				{
					if ( GSM_CmdWrite((unsigned char *)"AT+CIMI\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							COMMON_STR_DEBUG(DEBUG_GSM ">>%s" , answer );
							unsigned char * startPos = (unsigned char *)strstr( (char *)answer , "AT+CIMI\r" );
							if ( startPos )
							{
								const int tokenNumber = 10 ;
								int tokenCounter = 0 ;
								char * token[tokenNumber];
								char * pch;
								char * lastPtr = NULL ;

								pch=strtok_r((char *)startPos, " \r\n" , &lastPtr);

								while (pch!=NULL)
								{
									token[ tokenCounter++ ] = pch;
									pch=strtok_r(NULL, " \r\n" , &lastPtr);
									if ( tokenCounter == tokenNumber )
										break;
								}

								if ( tokenCounter == 3 )
								{									
									snprintf( imsi , 32 , "%s" , token[1] );
									STORAGE_UpdateImsi( sim , imsi , &needToSendSms);
									if ( needToSendSms == FALSE )
										res = OK ;
								}
							}


							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Do not ask imsi" );
					res = OK ;
				}

				if ( needToSendSms == TRUE )
				{
					//set sms text format
					if ( GSM_CmdWrite((unsigned char *)"AT+CMGF=1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting the the short messages text format OK" );
								res = OK ;
							}

							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}

					if ( res == OK )
					{
						char sn[32];
						memset(sn , 0 , 32);
						COMMON_GET_USPD_SN((unsigned char *)sn);

						smsBuffer_t tempSmsBuffer;
						memset( &tempSmsBuffer , 0 , sizeof( smsBuffer_t ) );
						snprintf( (char *)tempSmsBuffer.textMessage , SMS_TEXT_LENGTH , "USPD %s sim %d imsi %s" , sn , sim , imsi );
						snprintf( (char *)tempSmsBuffer.phoneNumber , PHONE_MAX_SIZE , "%s" , connection->smsReportPhone );

						GSM_SendSms( &tempSmsBuffer );
					}
				}
			}
			break;

			default:
			{
				networkRegistrationState = CHECKING_NETWORK_REGISTRATION_STATE_DONE ;
			}
			break;
		}
	}

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_CheckNetworkRegistration() return %s", getErrorCode(res) );

	return res;
}

//
//
//

int GSM_CastInitiateAtCommands(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	unsigned char * answer = NULL;
	int answerLength = 0 ;
	unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
	if ( GSM_CmdWrite((unsigned char *)"AT+CMEE=2\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting terminal error reporting mode OK" );
				res = OK ;
			}

			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}


	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT+CMGF=1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting the the short messages text format OK" );
					res = OK ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT+CNMI=1,1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting delivering new short message indication OK " );
					res = OK ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		if ( GSM_CmdWrite((unsigned char *)"AT+CPMS=\"SM\",\"SM\",\"SM\"\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
		{
			if ( answer )
			{
				if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting short message storage to sm OK " );
					res = OK ;
				}

				free( answer ) ;
				answer = NULL;
				answerLength = 0 ;
			}
		}
	}

	if ( res == OK )
	{
		res = ERROR_GENERAL ;
		if ( connection->mode == CONNECTION_CSD_INCOMING )
		{
			if ( GSM_CmdWrite((unsigned char *)"AT+CLIP=1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting incoming call indication OK" );
						res = OK ;
					}


					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		else
		{
			if ( GSM_CmdWrite((unsigned char *)"AT+CLIP=0\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting incoming call indication OK" );
						res = OK ;
					}


					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
	}

	if ( res == OK )
	{
		if ( (connection->mode == CONNECTION_CSD_INCOMING ) || (connection->mode == CONNECTION_CSD_OUTGOING ) )
		{
			if ( GSM_CmdWrite((unsigned char *)"AT+CBST=71,,1\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Setting bearer service type OK" );
						res = OK ;
					}


					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
	}

	return res;
}

//
//
//

int GSM_Close(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	switch ( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		{
			unsigned char * answer = NULL;
			int answerLength = 0 ;
			unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

			char cmd [64] = {0};
			snprintf(cmd , 64 , "AT^SISC=%d\r", currSrvPrId);

			if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
			{
				if ( answer )
				{
					if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM close OK " );
						res = OK ;
					}
					free( answer ) ;
					answer = NULL;
					answerLength = 0 ;
				}
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
		{
			dataMode = FALSE ;
			gsmNeedToReboot = TRUE ;
			//TODO
		}

		default:
			break;
	}


	return res ;
}

//
//
//

int GSM_Write(connection_t * connection, unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	switch ( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		{
			if ( GSM_ConnectionState() == OK )
			{
				int attemptCounter = 30 ;
				{
					while( (attemptCounter > 0) && (gsmReadyToWrite == FALSE))
					{
						--attemptCounter;
						COMMON_Sleep(SECOND);
					}
				}

				if ( gsmReadyToWrite == TRUE )
				{
					unsigned char * answer = NULL;
					int answerLength = 0 ;
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_SISW ;

					unsigned char cmd[64];
					memset( cmd , 0 , 64);
					snprintf( (char *)cmd , 64 , "AT^SISW=%d,%d\r" , currSrvPrId,  size );

					BOOL promptFlag = FALSE ;

					if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							if( strstr( (char *)answer , "\r\n^SISW:" ) )
							{
								promptFlag = TRUE ;
							}

							free(answer);
							answer = NULL ;
							answerLength = 0 ;
						}
					}

					if ( promptFlag == TRUE )
					{
						if ( GSM_CreateTask( NULL , TRUE , FALSE , possibleAnswerBank) == OK )
						{
							if ( SERIAL_Write( PORT_GSM , buf , size , (int *)written ) == OK )
							{
								if ( GSM_WaitTaskAnswer( GSM_SMALL_ANSWER_TIMEOUT , &answer , &answerLength ) == OK )
								{
									if ( answer )
									{
										if ( strstr( (char *)answer , "\r\nOK\r\n") )
										{
											COMMON_BUF_DEBUG( DEBUG_GSM "GSM Tx: " , buf , size );
											pthread_mutex_lock(&lastDataMutex);
											COMMON_GetCurrentDts(&lastDataDts);
											pthread_mutex_unlock(&lastDataMutex);
											res = OK ;
										}

										free(answer);
										answer = NULL ;
										answerLength = 0 ;
									}
								}
								else
								{
									COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write can not write buffer. exit" );
								}

							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write can not write buffer. exit" );
							}

							//COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write clearing task");
							GSM_ClearTask();
							//COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write clearing task OK");
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write can not create task. exit" );
						}
					}
				}

				if ( res != OK )
				{
					gsmNeedToReboot = TRUE ;
				}
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
		{
			COMMON_BUF_DEBUG( DEBUG_GSM "GSM Tx: " , buf , size );
			res = SERIAL_Write( PORT_GSM , buf , size , (int *)written);

			pthread_mutex_lock(&lastDataMutex);
			COMMON_GetCurrentDts(&lastDataDts);
			pthread_mutex_unlock(&lastDataMutex);
		}
		break;

		default:
			break;
	}

	return res ;
}

//
//
//

int GSM_Read(connection_t * connection, unsigned char * buf, int size, unsigned long * readed)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	switch ( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		{
			if ( GSM_ConnectionState() == OK )
			{
				COMMON_STR_DEBUG(DEBUG_GSM "GSM Read rom srv pr id [%d]", currSrvPrId );
				unsigned char * answer = NULL;
				int answerLength = 0 ;
				unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

				unsigned char cmd[64];
				memset( cmd , 0 , 64);
				snprintf( (char *)cmd , 64 , "AT^SISR=%d,%d\r" , currSrvPrId, size);
				int cmdLen = strlen((char *)cmd);

				if ( GSM_CmdWrite( cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength ) == OK )
				{
					if ( answer )
					{
						//AT^SISR=0,16\r      //+12 (12)
						//\r\n^SISR: 0,16\r\n //+15 (27)
						//BYTES               //+16 (43)
						//\r\n\r\nOK\r\n      //+8  (51)

						//parse answer
						if( answerLength >= (int)(strlen("\r\n\r\nOK\r\n")+ cmdLen ))
						{
							int answerOffset=0;
							for (;answerOffset<(answerLength-cmdLen); answerOffset++){
								if ( memcmp( &answer[answerOffset], cmd, cmdLen) == 0){
									COMMON_STR_DEBUG(DEBUG_GSM "GSM Read: answerOffset [%d]", answerOffset);
									COMMON_BUF_DEBUG(DEBUG_GSM "GSM Read:", answer, answerLength);
									break;
								}
							}

							if( memcmp( &answer[ answerLength - strlen("\r\n\r\nOK\r\n") ] , "\r\n\r\nOK\r\n" , strlen("\r\n\r\nOK\r\n")  ) == 0 )
							{
								int nIndex = 0 ;
								int nCounter = 0;

								do{
									if( answer[ answerOffset+nIndex ] == '\n'){
										nCounter++;
									}
									nIndex++;

								}while( (nCounter < 2) && (nIndex < answerLength) );

								(*readed) = answerLength - strlen("\r\n\r\nOK\r\n") - nIndex ;

								if( (*readed) > 0 )
								{
									if ( (int)(*readed) <= size )
									{
										memcpy(buf , &answer[ answerOffset+nIndex ] , (*readed) );
										pthread_mutex_lock(&lastDataMutex);
										COMMON_GetCurrentDts(&lastDataDts);
										pthread_mutex_unlock(&lastDataMutex);
										COMMON_BUF_DEBUG( DEBUG_GSM "GSM RX : " , buf , (*readed) );
										gsmReadyToRead = FALSE ;
										res = OK;
									}
									else
									{
										COMMON_STR_DEBUG(DEBUG_GSM "!!! GSM Read error: readed [%d] , but asked [%d] bytes" , (*readed) , size);
										res = ERROR_SIZE_READ;
									}
								}
							}

						}
						free( answer ) ;
						answer = NULL;
						answerLength = 0 ;
					}
				}
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
		{
			char noCarrStr[] = "\r\nNO CARRIER\r\n" ;

			res = SERIAL_Read(  PORT_GSM , buf , size , (int *)readed) ;
			COMMON_BUF_DEBUG( DEBUG_GSM "GSM Rx: " , buf , size );

			if ( size >= (int)strlen(noCarrStr) )
			{

				//checking for closed connection

				pthread_mutex_lock(&lastDataMutex);
				COMMON_GetCurrentDts(&lastDataDts);
				pthread_mutex_unlock(&lastDataMutex);

				if ( !memcmp( &buf[ size - strlen(noCarrStr)] , noCarrStr , strlen(noCarrStr) ) )
				{
					COMMON_STR_DEBUG(DEBUG_GSM "GSM NO CARRIER was catch");
					dataMode = FALSE ;
					gsmNeedToClose = TRUE ;
				}
			}
		}
		break;

		default:
			break;
	}

	return res ;
}

//
//
//

int GSM_SendSms( smsBuffer_t * sms )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms start" );

	unsigned char * answer = NULL;
	int answerLength = 0 ;
	unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_CMGS ;

	unsigned char cmd[64];
	memset(cmd , 0 , 64 );
	snprintf( (char *)cmd , 64 , "AT+CMGS=\"%s\"\r" , sms->phoneNumber );

	BOOL promptFlag = FALSE ;

	if ( GSM_CmdWrite(cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			if ( strstr( (char *)answer , ">" ) )
			{
				COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms prompt was recieved" );

				promptFlag = TRUE ;
			}

			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}

	if ( promptFlag == TRUE )
	{
		if ( GSM_CreateTask( NULL , TRUE , FALSE , possibleAnswerBank) == OK )
		{
			unsigned int written = 0 ;
			COMMON_STR_DEBUG( DEBUG_GSM "Sending sms: %s", (char *)sms->textMessage );
			if ( SERIAL_Write( PORT_GSM , sms->textMessage , strlen( (char *)sms->textMessage ) , (int *)&written ) == OK )
			{
				if ( written == strlen( (char *)sms->textMessage ) )
				{
					written = 0 ;
					COMMON_Sleep(10);
					if ( SERIAL_Write( PORT_GSM , (unsigned char *)"\x1A" , 1 , (int *)&written ) == OK )
					{
						if( written == 1 )
						{
							if ( GSM_WaitTaskAnswer( GSM_LARGE_ANSWER_TIMEOUT , &answer , &answerLength ) == OK )
							{
								if ( answer )
								{
									res = OK ;
									if ( strstr( (char *)answer , "\r\nOK\r\n") )
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms sms sended OK");
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms sms sended error");
									}

									free(answer);
									answer = NULL ;
									answerLength = 0 ;
								}
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms sms sended timeout");
							}
						}
					}
				}
			}

			GSM_ClearTask();
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_Write can not create task. exit" );
		}
	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_SendSms promt was not recieved" );
	}
	return res ;
}

//
//
//


int GSM_ReadAvailable(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	static int readsTries = 0;
	switch ( connection->mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		{
			if ( GSM_ConnectionState() == OK )
			{
				readsTries++;

				//COMMON_STR_DEBUG( DEBUG_GSM "GSM_ReadAvailable() gsmReadyToRead=[%d] readsTries=[%d]" , gsmReadyToRead , readsTries );

				if(( gsmReadyToRead == TRUE ) || (readsTries == 10))
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM_ReadAvailable() asking for bytes available to read from srv pr id [%d]", currSrvPrId);

					readsTries = 0;

					unsigned char * answer = NULL;
					int answerLength = 0 ;
					unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;

					char cmd[64] = {0};
					snprintf(cmd , 64 , "AT^SISR=%d,0\r", currSrvPrId);

					if ( GSM_CmdWrite((unsigned char *)cmd ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
					{
						if ( answer )
						{
							COMMON_STR_DEBUG( DEBUG_GSM "%s" , answer );
							if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
							{
								//parsing
								unsigned char * startPos = (unsigned char *)strstr((char *)answer , cmd);
								if( startPos )
								{
									const int tokenNumber = 10 ;
									int tokenCounter = 0 ;
									char * token[tokenNumber];
									char * lastPtr = NULL ;
									char * pch;

									pch=strtok_r((char *)startPos, " ,:\r\n" , &lastPtr);

									while (pch!=NULL)
									{
										token[ tokenCounter++ ] = pch;
										pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
										if ( tokenCounter == tokenNumber )
											break;
									}

									if ( tokenCounter > 5 )
									{
										res = atoi(token[4]);
										COMMON_STR_DEBUG( DEBUG_GSM "GSM_ReadAvailable() available to read [%d] bytes", res);
									}
								}
							}
							else
							{
								gsmNeedToReboot = TRUE ;
							}
							free( answer ) ;
							answer = NULL;
							answerLength = 0 ;
						}
					}
				}
				else
				{
					//COMMON_STR_DEBUG( DEBUG_GSM "GSM_ReadAvailable() reading for sms");
					GSM_WaitSms();
					res = 0 ;
				}
			}
		}
		break;

		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
		{
			if ( (GSM_ConnectionState() == OK) && (dataMode == TRUE) )
			{
				SERIAL_Check( PORT_GSM , &res ) ;
			}
		}
		break;

		default:
			break;
	}

	return res ;
}

//
//
//

int GSM_RememberSQ()
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL;

	unsigned char * answer = NULL;
	int answerLength = 0 ;
	unsigned char * possibleAnswerBank[] = GSM_CMD_ANSWER_BANK_STANDART ;
	if ( GSM_CmdWrite((unsigned char *)"AT+CSQ\r" ,FALSE, GSM_SMALL_ANSWER_TIMEOUT , possibleAnswerBank , &answer , &answerLength) == OK )
	{
		if ( answer )
		{
			if ( strstr( (char *)answer , "\r\nOK\r\n" ) )
			{
				unsigned char * startAtPos = (unsigned char *)strstr( (char *)answer , "AT+CSQ" );
				if( startAtPos )
				{
					int tokenCounter = 0 ;
					char * token[5];
					char * pch;
					char * lastPtr = NULL ;

					pch=strtok_r((char *)startAtPos, " ,:\r\n" , &lastPtr);
					while (pch!=NULL)
					{
						token[ tokenCounter++ ] = pch;
						pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
						if ( tokenCounter >= 5 )
							break;
					}

					if( tokenCounter > 3 )
					{
						int sq = atoi(token[2]);
						if( sq != 99 )
						{
							res = OK ;
							StaticSignalQuality = ( sq * 100 ) / 31;
						}
						else
						{
							StaticSignalQuality = 0;
						}
						COMMON_STR_DEBUG( DEBUG_GSM "GSM SignalQ [ %d%% ]", StaticSignalQuality );
					}

				}
			}
			free( answer ) ;
			answer = NULL;
			answerLength = 0 ;
		}
	}

	return res ;
}

//
//
//

int GSM_HandleIncomingBuffer(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	GSM_TaskProtect();

	//COMMON_BUF_DEBUG( DEBUG_GSM "GSM_HandleIncomingBuffer incBuf: " , incomingUartBuffer , incomingUartBufferPos );

	if ( gsmTask.status == GSM_TASK_WAIT )
	{
		//COMMON_STR_DEBUG( DEBUG_GSM "GSM_HandleIncomingBuffer task is now waiting for answer ");
		BOOL echoPresenseFlag = TRUE ;
		if ( gsmTask.cmd )
		{
			if ( gsmTask.avalancheOutput == FALSE )
			{
				if ( memcmp( &incomingUartBuffer[0] , gsmTask.cmd , strlen((char *)gsmTask.cmd) ) != 0 )
				{
					echoPresenseFlag = FALSE ;
				}
			}
			else
			{
				if ( gsmTask.answerLength == 0 )
				{
					if ( memcmp( &incomingUartBuffer[0] , gsmTask.cmd , strlen((char *)gsmTask.cmd) ) != 0 )
					{
						COMMON_STR_DEBUG(DEBUG_GSM "set echoPresenseFlag FALSE");
						echoPresenseFlag = FALSE ;
					} else {
						COMMON_STR_DEBUG(DEBUG_GSM "set echoPresenseFlag TRUE");
					}
				}
			}
		}

		if ( echoPresenseFlag == TRUE )
		{
			if ( COMMON_CheckPhrasePresence( gsmTask.possibleAnswerBank , incomingUartBuffer , incomingUartBufferPos , NULL )== OK )
			{
				int lastAnswerBufferPos = gsmTask.answerLength ;
				gsmTask.answerLength += incomingUartBufferPos;
				gsmTask.answer = realloc( gsmTask.answer , gsmTask.answerLength * sizeof(unsigned char) );
				if ( !gsmTask.answer )
				{
					fprintf(stderr , "Can not allocate [%d] bytes\n" , gsmTask.answerLength );
					EXIT_PATTERN;
				}
				memcpy( &gsmTask.answer[ lastAnswerBufferPos ] , incomingUartBuffer , incomingUartBufferPos );
				gsmTask.status = GSM_TASK_READY;

				//we are found something
				res = OK ;
			}
			else
			{
				if( incomingUartBufferPos == GSM_R_BUFFER_LENGTH )
				{
					if ( gsmTask.avalancheOutput == TRUE ){
						int lastAnswerBufferPos = gsmTask.answerLength ;
						gsmTask.answerLength += incomingUartBufferPos;
						gsmTask.answer = realloc( gsmTask.answer , gsmTask.answerLength * sizeof(unsigned char) );
						if ( !gsmTask.answer )
						{
							fprintf(stderr , "Can not allocate [%d] bytes\n" , gsmTask.answerLength );
							EXIT_PATTERN;
						}
						memcpy( &gsmTask.answer[ lastAnswerBufferPos ] , incomingUartBuffer , incomingUartBufferPos );
					}

					//we are get full buffer
					res = OK;
				}
			}
		}
		else
		{
			//there is no echo. it is async message.
			unsigned char * asyncPhrases[] = GSM_CMD_ASYNC_MESSAGES ;
			unsigned char foundedPhrase[64];
			if ( COMMON_CheckPhrasePresence( asyncPhrases , incomingUartBuffer , incomingUartBufferPos , foundedPhrase )== OK )
			{
				GSM_AsyncMessageHandler( foundedPhrase , connection);
				res = OK;

			} else {

				//unexpected answer by too much long length
				if( incomingUartBufferPos == GSM_R_BUFFER_LENGTH )
				{
					res = OK;
				}
			}

//			else
//			{
//				res = OK ;
//				//COMMON_STR_DEBUG( DEBUG_GSM "GSM Unknown async message was found [%s]" , incomingUartBuffer);
//				//COMMON_BUF_DEBUG( DEBUG_GSM "GSM unknown async message: " , incomingUartBuffer , incomingUartBufferPos );
//			}
		}
	}
	else
	{
		//COMMON_STR_DEBUG( DEBUG_GSM "GSM_HandleIncomingBuffer task isn't waiting for answer. It is async message");

		//COMMON_STR_DEBUG( DEBUG_GSM "GSM Unknown async message is [%s]" , incomingUartBuffer);

		//we do not wait for command answer. it is async message
		unsigned char * asyncPhrases[] = GSM_CMD_ASYNC_MESSAGES ;
		unsigned char foundedPhrase[64];
		if ( COMMON_CheckPhrasePresence( asyncPhrases , incomingUartBuffer , incomingUartBufferPos , foundedPhrase )== OK )
		{
			GSM_AsyncMessageHandler( foundedPhrase , connection);
			res = OK;

		} else {

			//unexpected answer by too much long length
			if( incomingUartBufferPos == GSM_R_BUFFER_LENGTH )
			{
				res = OK;
			}
		}

//		else
//		{
//			COMMON_STR_DEBUG( DEBUG_GSM "GSM Unknown async message was found [%s]" , incomingUartBuffer);
//			COMMON_BUF_DEBUG( DEBUG_GSM "GSM unknown async message: " , incomingUartBuffer , incomingUartBufferPos );
//		}
	}

	GSM_TaskUnprotect();

	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_HandleIncomingBuffer return res [%s]" , getErrorCode(res) );

	return res ;
}

//
//
//

void GSM_AsyncMessageHandler(unsigned char * foundedPhrase, connection_t * connection)
{
	DEB_TRACE(DEB_IDX_GSM);

	if ( !strcmp( (char *)foundedPhrase , "\r\n^SYSSTART\r\n" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ^SYSSTART was found" , foundedPhrase );

		if ( gsmConnectionState == GSM_CONNECTION_STATE_INIT )
			readyModuleFlag = TRUE ;
		else
		{
			STORAGE_JournalUspdEvent( EVENT_GSM_MODULE_INIT_ERROR , NULL );

			if ( gsmConnectionState == GSM_CONNECTION_STATE_CONNECTION_READY )
				STORAGE_EVENT_DISCONNECTED(0x02);

			gsmConnectionState = GSM_CONNECTION_STATE_INIT ;
		}
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\n^SISR:" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ^SISR was found" , foundedPhrase );
		unsigned char * startAtPos = (unsigned char *)strstr( (char *)incomingUartBuffer , "^SISR" );
		if( startAtPos )
		{
			int tokenCounter = 0 ;
			char * token[5];
			char * pch;

			char * lastPtr = NULL ;
			pch=strtok_r((char *)startAtPos, " ,:\r\n" , &lastPtr);

			while (pch!=NULL)
			{
				token[ tokenCounter++ ] = pch;
				pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
				if ( tokenCounter >= 5 )
					break;
			}
			if( tokenCounter > 2 )
			{
				if( atoi(token[2]) >= 1 )
				{
					//Module available to get some data
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Module available to read some data");

					gsmReadyToRead = TRUE ;
				}
			}
		}
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\n^SISW:" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ^SISW was found" , foundedPhrase );
		unsigned char * startAtPos = (unsigned char *)strstr( (char *)incomingUartBuffer , "^SISW" );
		if( startAtPos )
		{
			int tokenCounter = 0 ;
			char * token[5];
			char * pch;
			char * lastPtr = NULL ;

			pch=strtok_r((char *)startAtPos, " ,:\r\n" , &lastPtr);

			while (pch!=NULL)
			{
				token[ tokenCounter++ ] = pch;
				pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
				if ( tokenCounter >= 5 )
					break;
			}
			if( tokenCounter > 2 )
			{
				if( atoi(token[2]) != 1 )
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM Module is not ready to transmit data");

					// Module is not ready to get data
					gsmReadyToWrite = FALSE ;

				}
				else
				{
					if ( tokenCounter > 3 )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Module ready to transmit [%d] bytes" , atoi(token[3]));
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Module ready to transmit some bytes" );
					}

					gsmReadyToWrite = TRUE ;
				}
			}
		}
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\n^SIS:" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ^SIS was found: %s", incomingUartBuffer );
		unsigned char * startAtPos = (unsigned char *)strstr( (char *)incomingUartBuffer , "^SIS" );
		if( startAtPos )
		{
			int tokenCounter = 0 ;
			char * token[5];
			char * pch;
			char * lastPtr = NULL ;
			pch=strtok_r((char *)startAtPos, " ,:\r\n" , &lastPtr);
			while (pch!=NULL)
			{
				token[ tokenCounter++ ] = pch;
				pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
				if ( tokenCounter >= 5 )
					break;
			}

			if( tokenCounter >= 4 )
			{
				int urcCause = atoi( token[2] );
				int urcInfold = atoi( token[3] );

				//handle urcCause

				if ( ( urcCause == 0 ) || ( urcCause == 2 ) )
				{
					if ( urcInfold == 0 )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM The service works properly");
					}
					else if ( ( urcInfold >= 1 ) && ( urcInfold <= 2000 ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Error, the service is interrupted");

//						pthread_mutex_lock(&lastDataMutex);
//						memset( &lastDataDts , 0 , sizeof(dateTimeStamp) );
//						pthread_mutex_unlock(&lastDataMutex);

//						if ( (connection->mode == CONNECTION_GPRS_SERVER) && (serviceMode == FALSE))
//						{
//							COMMON_STR_DEBUG( DEBUG_GSM "GSM srv pr id [%d] was interrupted", atoi(token[1]));

//							//TODO close
//						}
//						else
							gsmNeedToClose = TRUE ;
					}
					else if ( ( urcInfold >= 4001 ) && ( urcInfold <= 6000 ) )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM Error, the service is not interrupted");

						gsmNeedToReboot = TRUE ;
					}
				}
				else if (urcCause == 1) //for server connection type
				{
					if ( gsmConnectionState == GSM_CONNECTION_STATE_CONNECTION_CHECK_TIME )
					{
						COMMON_STR_DEBUG( DEBUG_GSM "GSM new incoming connection at srv profile id [%d]", urcInfold);
						currSrvPrId = urcInfold ;
						srvPrIdToClose = 0 ;
						incomingCall = GSM_ICS_NEED_TO_ANSWER ;
					}
					else
					{
						srvPrIdToClose = urcInfold ;
					}
				}
			}
		}
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\n+CMTI:" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ^CMTI was found" , foundedPhrase );
		unsigned char * startAtPos = (unsigned char *)strstr( (char *)incomingUartBuffer , "+CMTI:" );
		if( startAtPos )
		{
			int tokenCounter = 0 ;
			char * token[5];
			char * pch;
			char * lastPtr = NULL ;

			pch=strtok_r((char *)startAtPos, " ,:\r\n" , &lastPtr);
			while (pch!=NULL)
			{
				token[ tokenCounter++ ] = pch;
				pch=strtok_r(NULL, " ,:\r\n" , &lastPtr);
				if ( tokenCounter >= 5 )
					break;
			}

			if( tokenCounter >= 3 )
			{
				GSM_RecievedSmsIndexProtect();

				if ( smsNewSmsIndex == GSM_SMS_INDEX_NO_NEW_MESSAGES )
				{
					smsNewSmsIndex = atoi(token[2]) ;
					COMMON_STR_DEBUG( DEBUG_GSM "GSM new sms was delivered. it's index in storage is [%d]" , atoi(token[2]) );
				}
				else if ( smsNewSmsIndex >= 0 )
				{
					smsNewSmsIndex = GSM_MORE_THEN_ONE_MESSAGE_WAS_RECIEVED ;
					COMMON_STR_DEBUG( DEBUG_GSM "GSM new sms was delivered. it's index in storage is [%d]. Now more then one sms are in storage" , atoi(token[2]) );
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_GSM "GSM new sms was delivered. it's index in storage is [%d]. Check all storage " , atoi(token[2]) );
				}

				GSM_RecievedSmsIndexUnprotect();
			}
		}
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\nRING\r\n" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message RING was found" , foundedPhrase );
	}
	else if ( !strncmp( (char *)foundedPhrase , "\r\n+CLIP", 7 ) )
	{
		char phone[PHONE_MAX_SIZE] = {0} ;
		char * firstQuote = strstr( (char *)incomingUartBuffer , "\"" );
		char * secondQuote = NULL ;
		int phoneLength = 0 ;
		BOOL allowToAnswer = FALSE ;
		char * allowedPhoneNumb1 = NULL;
		char * allowedPhoneNumb2 = NULL;

		//COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message +CLIP was found" , foundedPhrase );

		if ( firstQuote )
			secondQuote = strstr( firstQuote + 1 , "\"" );

		if ( secondQuote )
			phoneLength = secondQuote - firstQuote - 1;

		if ( phoneLength > 0 )
		{
			if ( phoneLength > PHONE_MAX_SIZE )
				phoneLength = PHONE_MAX_SIZE - 1 ;

			strncpy( phone , firstQuote + 1 , phoneLength ) ;
		}

		COMMON_STR_DEBUG( DEBUG_GSM "Incoming call from [%s] was cathed" , phone );

		//checking phone number
		if ( currentSim == SIM1 )
		{
			allowedPhoneNumb1 = (char *)connection->allowedIncomingNum1Sim1 ;
			allowedPhoneNumb2 = (char *)connection->allowedIncomingNum2Sim1 ;
		}
		else
		{
			allowedPhoneNumb1 = (char *)connection->allowedIncomingNum1Sim2 ;
			allowedPhoneNumb2 = (char *)connection->allowedIncomingNum2Sim2 ;
		}

		if ( !strlen( allowedPhoneNumb1 ) && !strlen( allowedPhoneNumb2 ) )
			allowToAnswer = TRUE ;
		else if ( !strcmp( phone, allowedPhoneNumb1 ) || !strcmp( phone, allowedPhoneNumb2 ) )
			allowToAnswer = TRUE ;


		if ( allowToAnswer )
			incomingCall = GSM_ICS_NEED_TO_ANSWER ;
		else
			incomingCall = GSM_ICS_NEED_TO_HANG ;
	}
	else if ( !strcmp( (char *)foundedPhrase , "\r\nERROR" ) )
	{
		static int errorCounter = 0 ;
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Async message ERROR was found" , foundedPhrase );
		if ( (++errorCounter) > 3 )
		{
			errorCounter = 0 ;
			gsmNeedToReboot = TRUE ;
		}
	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM There is no handler for async message [%s]. Do nothing" , foundedPhrase );
		COMMON_STR_DEBUG( DEBUG_GSM "Output buffer: [%s] " , incomingUartBuffer);
	}
	return ;
}

//
//
//

int GSM_CreateTask( unsigned char * cmd , BOOL emptyCmd , BOOL avalancheOutput , unsigned char ** possibleAnswerBank )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;

	int attemptIdx = 10 ;

	for ( ; attemptIdx > 0 ; attemptIdx--)
	{
		GSM_TaskProtect();

		if ( gsmTask.status == GSM_TASK_EMPTY )
		{
			res = OK ;
			if( (emptyCmd == FALSE) && (cmd != NULL))
			{
				snprintf( (char *)gsmTask.cmd , GSM_R_BUFFER_LENGTH , "%s" , cmd );
			}
			gsmTask.status = GSM_TASK_WAIT ;
			gsmTask.avalancheOutput = avalancheOutput ;
			gsmTask.possibleAnswerBank = possibleAnswerBank;
			GSM_TaskUnprotect();
			break;
		}
		else
		{
			GSM_TaskUnprotect();
			COMMON_Sleep( 200 );
		}

	}

	return res ;
}

//
//
//

void GSM_ClearTask()
{
	DEB_TRACE(DEB_IDX_GSM);

	GSM_TaskProtect();

	gsmTask.status = GSM_TASK_EMPTY ;
	memset( gsmTask.cmd , 0 , GSM_R_BUFFER_LENGTH ) ;
	if ( gsmTask.answer)
	{
		free(gsmTask.answer);
		gsmTask.answer = NULL ;
	}
	gsmTask.answerLength = 0 ;
	gsmTask.possibleAnswerBank = NULL ;

	GSM_TaskUnprotect();
}

//
//
//

int GSM_WaitTaskAnswer( int timeout ,  unsigned char ** answer , int * answerLength )
{
	DEB_TRACE(DEB_IDX_GSM);

	int res = ERROR_GENERAL ;
	int attemptCounter = timeout ;
	while( attemptCounter > 0 )
	{
		GSM_TaskProtect();

		if ( gsmTask.status == GSM_TASK_READY )
		{
			(*answerLength) = gsmTask.answerLength ;
			(*answer) = malloc( (gsmTask.answerLength + 1) * sizeof(unsigned char) );
			memset( (*answer) , 0 , (gsmTask.answerLength + 1) * sizeof(unsigned char) );
			memcpy( (*answer) , gsmTask.answer , gsmTask.answerLength );
			res = OK ;
			GSM_TaskUnprotect();
			break;
		}
		else
		{
			GSM_TaskUnprotect();
			--attemptCounter;
			COMMON_Sleep( SECOND );
		}
	}

	if ( res != OK )
	{
		gsmNeedToReboot = TRUE ;
	}

	return res ;
}

//
//
//

int GSM_CmdWrite( unsigned char * cmd , BOOL emptyCmd , int timeout ,unsigned char ** possibleAnswerBank , unsigned char ** answer , int * answerLength)
{
	DEB_TRACE(DEB_IDX_GSM);

	pthread_mutex_lock(&cmdMutex);
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite start" );

	int res = ERROR_GENERAL ;

	BOOL avalancheOutput = FALSE ;
	unsigned char * avalancheCommands[] = GSM_AVALANCHE_COMMANDS ;
	if ( cmd )
	{
		if ( COMMON_CheckPhrasePresence( avalancheCommands , cmd , strlen((char *)cmd) , NULL ) == OK )
		{
			avalancheOutput = TRUE ;
		}
	}

	if ( GSM_CreateTask( cmd , emptyCmd , avalancheOutput , possibleAnswerBank) != OK )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM Task state:\nCMD: %s\nANSWER: %s\nANSWERLEN: %d\nSTATUS: %d\n" , gsmTask.cmd , gsmTask.answer , gsmTask.answerLength , gsmTask.status );
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite can not create task. exit" );
		pthread_mutex_unlock(&cmdMutex);
		return res ;
	}

	int w = 0 ;
	if ( cmd )
	{
		if ( SERIAL_Write( PORT_GSM , cmd , strlen((char *)cmd) , &w) != OK )
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite can not write. exit" );
			GSM_ClearTask();
			pthread_mutex_unlock(&cmdMutex);
			return res ;
		}
		if ( w != (int)strlen((char *)cmd) )
		{
			COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite number write is not equal. exit" );
			GSM_ClearTask();
			pthread_mutex_unlock(&cmdMutex);
			return res ;
		}
	}


	if ( GSM_WaitTaskAnswer( timeout , answer , answerLength ) == OK )
	{
		res = OK ;
	}

	GSM_ClearTask();

	pthread_mutex_unlock(&cmdMutex);

	if ( res != OK )
	{
		gsmNeedToReboot = TRUE ;
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite waiting answer timeouted" );
	}
	//COMMON_STR_DEBUG( DEBUG_GSM "GSM_CmdWrite return %s", getErrorCode(res) );
	return res ;
}

//
//
//

int GSM_GetIMEI(char * imeiString, const unsigned int imeiStringMaxLength)
{
	DEB_TRACE(DEB_IDX_GSM);

	snprintf( imeiString , imeiStringMaxLength , "%s" , imei  );

	return OK;
}
//
//
//

int GSM_GetSignalQuality()
{
	DEB_TRACE(DEB_IDX_GSM);

	return StaticSignalQuality;
}

//
//
//

void GSM_GetOwnIp(char * ip)
{
	if ( strlen(gsmOwnIp) != 0 )
		strncpy(ip, gsmOwnIp, LINK_SIZE);
	else
		snprintf(ip, LINK_SIZE, "0.0.0.0");
}

//
//
//

int GSM_ConnectionState()
{
	DEB_TRACE(DEB_IDX_GSM);

	int res=ERROR_CONNECTION;
	if(gsmConnectionState == GSM_CONNECTION_STATE_CONNECTION_READY)
	{
		res=OK;
	}
	return res;
}

//
//
//

int GSM_GetGsmOperatorName(char * gsmOpName, const unsigned int gsmOpNameMaxLength)
{
	DEB_TRACE(DEB_IDX_GSM)

	snprintf( gsmOpName , gsmOpNameMaxLength , "%s", gsmOperatorName );
	return OK;

}

//
//
//

int GSM_GetCurrentConnectionTime()
{
	DEB_TRACE(DEB_IDX_GSM);

	if(GSM_ConnectionState() == OK)
	{
		if ( COMMON_CheckDtsForValid( &lastConnectionDts ) == OK )
		{
			dateTimeStamp currentDts;
			COMMON_GetCurrentDts(&currentDts);

			int secondDiff = COMMON_GetDtsDiff__Alt( &lastConnectionDts , &currentDts , BY_SEC);
			if ( secondDiff > 0 )
			{
				return secondDiff ;
			}
			else
			{
				return 0 ;
			}
		}
	}
	return 0 ;

}

//
//
//

unsigned int GSM_GetConnectionsCounter()
{
	DEB_TRACE(DEB_IDX_GSM)

	pthread_mutex_lock(&connectionCounterMutex);

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

	pthread_mutex_unlock(&connectionCounterMutex);

	return res;
}

//
//
//

int GSM_ConnectionCounterIncrement()
{
	int res = ERROR_GENERAL ;

	pthread_mutex_lock(&connectionCounterMutex);

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

	pthread_mutex_unlock(&connectionCounterMutex);

	return res;
}

//
//
//

int GSM_GetCurrentSimNumber()
{
	return currentSim ;
}

//
//
//

BOOL GSM_GetServiceModeStat()
{
	return serviceMode ;
}

//
//
//

void GSM_GetDefaultOperatorApn(char * str , const int length , int part )
{
	char opName[GSM_OPERATOR_NAME_LENGTH];
	memset( opName , 0 , GSM_OPERATOR_NAME_LENGTH * sizeof(char) );
	memcpy( opName , gsmOperatorName , strlen(gsmOperatorName) );

	COMMON_ShiftTextDown( opName , strlen(opName) );
	if ( strstr( opName , "mts" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn finded op name MTS" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet.mts.ru");
				break;
			case GSM_PART_USER:
				snprintf(str , length , "mts");
				break;
			case GSM_PART_PASS:
				snprintf(str , length , "mts");
				break;
		}
	}
	else if ( strstr( opName , "beeline" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn finded op name Beeline" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet.beeline.ru");
				break;
			case GSM_PART_USER:
				snprintf(str , length , "beeline");
				break;
			case GSM_PART_PASS:
				snprintf(str , length , "beeline");
				break;
		}
	}
	else if ( strstr( opName , "megafon" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn finded op name Megafon" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet");
				break;
			case GSM_PART_USER:
				snprintf(str , length , "gdata");
				break;
			case GSM_PART_PASS:
				snprintf(str , length , "gdata");
				break;
		}
	}
	else if ( strstr( opName , "tele2" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn finded op name Tele2" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet.tele2.ru");
				break;
			case GSM_PART_USER:
				memset(str , 0 , length);
				break;
			case GSM_PART_PASS:
				memset(str , 0 , length);
				break;
		}
	}
	else if ( strstr( opName , "yota" ) )
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn finded op name Yota" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet.yota");
				break;
			case GSM_PART_USER:
				memset(str , 0 , length);
				break;
			case GSM_PART_PASS:
				memset(str , 0 , length);
				break;
		}
	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_GSM "GSM_GetDefaultOperatorApn op name was not founded. Try to use default" );
		switch(part)
		{
			case GSM_PART_APN:
				snprintf(str , length , "internet");
				break;
			case GSM_PART_USER:
				memset(str , 0 , length);
				break;
			case GSM_PART_PASS:
				memset(str , 0 , length);
				break;
		}
	}

	return;
}

//
//
//

void GSM_ParseSisoOutput(char * sisoOutput ,
						 int sockIdx ,
						 char * ownIp ,
						 int * ownPort ,
						 char * remoteIp,
						 int * remotePort)
{
	const int searchStrLength = 128 ;
	char searchStr[searchStrLength];
	char * stringIdx = NULL ;
	int idx = 0 ;
	int bracketCounter = 0 ;

	char * ownIpPtr = NULL ;
	char * remoteIpPtr = NULL ;
	char * ownDotPtr = NULL ;
	char * remDotPtr = NULL ;

	char ownIp_[searchStrLength];
	char remIp_[searchStrLength];

	memset(ownIp_, 0 , searchStrLength);
	memset(remIp_, 0 , searchStrLength);
	memset(searchStr, 0 , searchStrLength);

	//set return values by default
	if (ownIp)		snprintf(ownIp, LINK_SIZE, "0.0.0.0");
	if (remoteIp)	snprintf(remoteIp, LINK_SIZE, "0.0.0.0");
	if (ownPort)	*ownPort = 0 ;
	if (remotePort)	*remotePort = 0 ;


	snprintf( searchStr, searchStrLength , "^SISO: %d," , sockIdx );

	if ( (stringIdx = strstr(sisoOutput, searchStr)) == NULL )
		return ;

	memset(searchStr, 0 , searchStrLength);
	for (idx = 0 ; (stringIdx[idx] != '\r') && (idx < searchStrLength); idx++)
		searchStr[idx] = stringIdx[idx];

	if (!strstr(searchStr, "Socket"))
		return ;

	//^SISO: 1,"Socket","5","4","0","0","46.250.52.54:58198","95.79.55.25:48077"
	//									^					  ^
	//									|					  |
	//									11					  13
	for (idx = 0 ; idx < (int)strlen(searchStr) ; idx++)
	{
		if (searchStr[idx] == '\"')
		{
			bracketCounter++;

			if (bracketCounter == 11)
				ownIpPtr = &searchStr[idx];

			if (bracketCounter == 13)
				remoteIpPtr = &searchStr[idx];
		}

	}

	for (idx = 1 ; (ownIpPtr[idx] != '\"') && (idx < searchStrLength); idx++ )
		ownIp_[idx - 1] = ownIpPtr[idx];

	for (idx = 1 ; (remoteIpPtr[idx] != '\"') && (idx < searchStrLength); idx++ )
		remIp_[idx - 1] = remoteIpPtr[idx];

	ownDotPtr = strstr(ownIp_, ":");
	remDotPtr = strstr(remIp_, ":");

	if (ownDotPtr)
	{
		if (ownIp)
		{
			for (idx = 0 ; (&ownIp_[idx] < ownDotPtr) && (idx < LINK_SIZE); idx++)
				ownIp[idx] = ownIp_[idx];
		}

		if (ownPort)
			*ownPort = atoi(ownDotPtr + 1);
	}

	if (remDotPtr)
	{
		if (remoteIp)
		{
			for (idx = 0 ; (&remIp_[idx] < remDotPtr) && (idx < LINK_SIZE); idx++)
				remoteIp[idx] = remIp_[idx];
		}

		if (remotePort)
			*remotePort = atoi(remDotPtr + 1);
	}

	return ;
}
//EOF
