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
#include "exio.h"
#include "debug.h"

pthread_t exioThread ;

BOOL exioThreadState = FALSE ;
volatile BOOL exioListenerState = FALSE ;
volatile BOOL exioExecut = FALSE ;

void * EXIO_Do(void * unuseArgument);
void * EXIO_DoListener(void * unuseArgument);


static exioTask_t exioTask;
static pthread_mutex_t exioTaskMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t exioMux = PTHREAD_MUTEX_INITIALIZER;

static BOOL exioReadyToCommand = FALSE ;
static BOOL exioNeedToReconfig = FALSE ;

//
//
//

void EXIO_TaskProtect()
{
	DEB_TRACE(DEB_IDX_EXIO)
	pthread_mutex_lock(&exioTaskMux);
}

//
//
//

void EXIO_TaskUnprotect()
{
	DEB_TRACE(DEB_IDX_EXIO)
	pthread_mutex_unlock(&exioTaskMux);
}

//
//
//

void EXIO_Protect()
{
	DEB_TRACE(DEB_IDX_EXIO)
	pthread_mutex_lock(&exioMux);
}

//
//
//

void EXIO_Unprotect()
{
	DEB_TRACE(DEB_IDX_EXIO)
	pthread_mutex_unlock(&exioMux);
}

//
//
//

int EXIO_Initialize()
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	EXIO_ClearTask();

	exioNeedToReconfig = TRUE ;
	exioExecut = TRUE ;

	int exioInitCounter = 0 ;
	if ( pthread_create(&exioThread, NULL, EXIO_DoListener, NULL) == 0 )
	{
		// wait until listener thread start
		do
		{
			++exioInitCounter ;
			COMMON_Sleep( SECOND );
		}
		while( (exioListenerState != TRUE ) && ( exioInitCounter < MAX_COUNT_EXIO_INIT_WAIT ) );

		if ( exioListenerState != TRUE )
		{
			COMMON_STR_DEBUG(DEBUG_EXIO "EXIO Listener Thread was not started");
			COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_Initialize %s", getErrorCode(res));
			return res ;
		}
	}

	exioInitCounter = 0 ;
	if ( pthread_create(&exioThread, NULL, EXIO_Do, NULL) == 0 )
	{
		do
		{
			++exioInitCounter ;
			COMMON_Sleep( SECOND );
		}
		while( (exioThreadState != TRUE ) && ( exioInitCounter < MAX_COUNT_EXIO_INIT_WAIT ) );

		if( exioThreadState != TRUE )
		{
			COMMON_STR_DEBUG(DEBUG_EXIO "EXIO Thread was not started");
			COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_Initialize %s", getErrorCode(res));
			return res ;
		}
	}

	res = OK ;
	//debug
	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_Initialize %s", getErrorCode(res));

	return res ;
}

//
//
//

void EXIO_Stop()
{
	DEB_TRACE(DEB_IDX_EXIO)

	exioExecut = FALSE ;
}

//
//
//

void * EXIO_DoListener(void * unuseArgument __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_EXIO)

	COMMON_STR_DEBUG( DEBUG_EXIO "EXIO_DoListener start");
	exioListenerState = TRUE ;

	unsigned char incomingBuffer[ EXIO_INCOMING_BUF_LENGTH ] ;
	memset( incomingBuffer , 0 , EXIO_INCOMING_BUF_LENGTH );
	int incomingBufferPos = 0 ;

	int messageLength = 0 ;
	int startMessagePosition = 0 ;

	enum { 	EXIO_LISTENER_STATE_TRY_TO_READ_BYTE ,
			EXIO_STATE_CHECK_BUFFER ,
			EXIO_STATE_EXECUT_TASK ,
			EXIO_STATE_ASYNC_MESSAGE_HANDLER,
			EXIO_STATE_CUT_MESSAGE_FROM_BUFFER} ;

	int doListenerState = EXIO_LISTENER_STATE_TRY_TO_READ_BYTE ;
	while( exioExecut == TRUE )
	{
		SVISOR_THREAD_SEND(THREAD_EXIO_LISTENER);
		switch( doListenerState )
		{
			case EXIO_LISTENER_STATE_TRY_TO_READ_BYTE :
			{
				int r = 0 ;
				if ( SERIAL_Read( PORT_EXIO , &incomingBuffer[ incomingBufferPos ] , 1 , &r ) == OK )
				{
					if( r == 1 )
					{
						COMMON_STR_DEBUG( DEBUG_EXIO "EXIO LISTENER. Readed byte [%02X] OK", incomingBuffer[ incomingBufferPos ] );
						incomingBufferPos++ ;

						if ( incomingBufferPos == EXIO_INCOMING_BUF_LENGTH )
						{
							COMMON_STR_DEBUG( DEBUG_EXIO "EXIO Error incoming buffer overflow!!!" );
							memset( incomingBuffer , 0 , EXIO_INCOMING_BUF_LENGTH );
							incomingBufferPos = 0 ;
						}
						else
						{
							doListenerState = EXIO_STATE_CHECK_BUFFER ;

							messageLength = 0 ;
							startMessagePosition = 0 ;

						}
					}
					else
					{
						COMMON_Sleep(10);
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_EXIO "EXIO Error reading from serial!!!!" );
					COMMON_Sleep(5000);
				}

			}
			break;

			//-------

			case EXIO_STATE_CHECK_BUFFER :
			{
				COMMON_STR_DEBUG( DEBUG_EXIO "EXIO_DoListener() catch a byte [%02X]. Checking...", incomingBuffer[ incomingBufferPos - 1 ] );
				if ( EXIO_CheckIncomingBufferForValidAnswer( incomingBuffer , incomingBufferPos , &startMessagePosition , &messageLength) == OK )
				{
					doListenerState = EXIO_STATE_EXECUT_TASK ;
				}
				else
				{
					doListenerState = EXIO_LISTENER_STATE_TRY_TO_READ_BYTE ;
				}
			}
			break;

			//-------

			case EXIO_STATE_EXECUT_TASK :
			{
				if ( EXIO_ExecutTask( &incomingBuffer[ startMessagePosition ] , messageLength ) == OK )
				{
					doListenerState = EXIO_STATE_CUT_MESSAGE_FROM_BUFFER ;
				}
				else
				{
					doListenerState = EXIO_STATE_ASYNC_MESSAGE_HANDLER ;
				}
			}
			break;

			//-------

			case EXIO_STATE_ASYNC_MESSAGE_HANDLER:
			{
				EXIO_HandleAsyncMessage( &incomingBuffer[ startMessagePosition ] , messageLength );
				doListenerState = EXIO_STATE_CUT_MESSAGE_FROM_BUFFER ;
			}
			break;

			//------

			case EXIO_STATE_CUT_MESSAGE_FROM_BUFFER:
			{
				memmove( incomingBuffer , &incomingBuffer[ startMessagePosition + messageLength ] , EXIO_INCOMING_BUF_LENGTH - startMessagePosition - messageLength );
				int bufAreaToMakeEmpty = startMessagePosition + messageLength ;
				startMessagePosition = 0;
				messageLength = 0 ;
				memset( &incomingBuffer[ EXIO_INCOMING_BUF_LENGTH - startMessagePosition - messageLength ] , 0 , bufAreaToMakeEmpty );
				incomingBufferPos -= startMessagePosition + messageLength ;
				doListenerState = EXIO_STATE_CHECK_BUFFER ;
			}
			break;

			default:
				break;
		}
	}

	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_DoListener was stopped");
	return NULL ;
}


void * EXIO_Do(void * unuseArgument __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_EXIO)

	COMMON_STR_DEBUG( DEBUG_EXIO "EXIO_Do start");

	exioThreadState = TRUE ;

	enum { 	EXIO_DO_STATE_GET_CURRENT_MODE ,
			EXIO_DO_STATE_WRITE_DEFAULT ,
			EXIO_DO_STATE_STAND_BY} ;

	int exioState = EXIO_DO_STATE_STAND_BY ;

	while( exioExecut == TRUE )
	{
		SVISOR_THREAD_SEND(THREAD_EXIO);
		switch( exioState )
		{
			case EXIO_DO_STATE_GET_CURRENT_MODE :
			{
				COMMON_STR_DEBUG( DEBUG_EXIO "EXIO_Do try get current discret state");

				unsigned char answer[ EXIO_DISCRET_OUTPUT_NUMB ];
				memset( answer , 0 , EXIO_DISCRET_OUTPUT_NUMB );
				if (EXIO_CommandGetState( answer ) == OK)
				{
					//check config
					if ( EXIO_CheckConfig( answer ) == TRUE)
					{
						exioState = EXIO_DO_STATE_WRITE_DEFAULT ;
					}
					else
					{
						exioReadyToCommand = TRUE ;
						exioState = EXIO_DO_STATE_STAND_BY ;
					}


				}
				else
				{
					COMMON_Sleep( 10000 );
				}

			}
			break;



			case EXIO_DO_STATE_WRITE_DEFAULT :
			{
				COMMON_STR_DEBUG( DEBUG_EXIO "EXIO_Do try to write default config");
				EXIO_Protect();

				exioCfg_t * exioCfg = NULL;
				if ( STORAGE_GetExioCfg( &exioCfg ) == OK )
				{
					if( exioCfg )
					{
						unsigned char param[ EXIO_DISCRET_OUTPUT_NUMB ];
						memset( param , 0 , EXIO_DISCRET_OUTPUT_NUMB );
						int discretIndex = 0;
						for( ; discretIndex < EXIO_DISCRET_OUTPUT_NUMB ; ++discretIndex )
						{
							param[ discretIndex ] = (( exioCfg->discretCfg[ discretIndex ].index << 4 ) & 0xF0 ) | \
													( exioCfg->discretCfg[ discretIndex ].mode ) | \
													( exioCfg->discretCfg[ discretIndex ].defaultValue ) ;
						}

						EXIO_SortParamArray( param , EXIO_DISCRET_OUTPUT_NUMB );

						if ( EXIO_AddTask( EXIO_CMD_DISCRET_SET_MODE , param , EXIO_DISCRET_OUTPUT_NUMB  ) == OK )
						{
							unsigned char answer[ EXIO_DISCRET_OUTPUT_NUMB ];
							memset( answer , 0 , EXIO_DISCRET_OUTPUT_NUMB);
							if ( EXIO_WaitTaskAnswer( EXIO_TIMEOUT , answer , EXIO_DISCRET_OUTPUT_NUMB ) == OK )
							{
								exioReadyToCommand = TRUE ;
								exioState = EXIO_DO_STATE_STAND_BY ;
							}
							else
							{
								COMMON_Sleep(SECOND * 10);
							}

						}
						else
						{
							COMMON_Sleep(SECOND * 10);
						}


					}
					else
					{
						exioState = EXIO_DO_STATE_GET_CURRENT_MODE ;
					}
				}
				else
				{
					exioState = EXIO_DO_STATE_GET_CURRENT_MODE ;
				}


				EXIO_Unprotect();
			}
			break;

			case EXIO_DO_STATE_STAND_BY :
			{
				EXIO_Protect();

				if ( exioNeedToReconfig == TRUE )
				{
					exioNeedToReconfig = FALSE ;
					exioState = EXIO_DO_STATE_GET_CURRENT_MODE ;
					exioReadyToCommand = FALSE ;
				}

				EXIO_Unprotect();

				if( exioState == EXIO_DO_STATE_STAND_BY )
				{
					COMMON_Sleep(1000);
				}

			}
			break;

			default:
				break;

		}

	}


	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_Do was stopped");
	return NULL ;
}

//
//
//

int EXIO_RewriteDefault()
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	if ( exioReadyToCommand == TRUE )
	{
		//EXIO_Protect();

		exioNeedToReconfig = TRUE ;
		res = TRUE ;

		//EXIO_Unprotect();
	}

	return res ;
}

//
//
//

BOOL EXIO_CheckExioReadyToCommand()
{
	DEB_TRACE(DEB_IDX_EXIO)

	return exioReadyToCommand ;
}

//
//
//

int EXIO_CommandSetMode( int discretOutputIndex , unsigned char mode , unsigned char defaultValue )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	if ( exioReadyToCommand == TRUE )
	{
		EXIO_Protect();

		unsigned char param = discretOutputIndex | mode | defaultValue ;

		if ( EXIO_AddTask( EXIO_CMD_DISCRET_SET_MODE , &param , 1 ) == OK )
		{
			unsigned char answer = 0 ;
			if ( EXIO_WaitTaskAnswer( EXIO_TIMEOUT , &answer , 1 ) == OK )
			{
				if ( (( answer & discretOutputIndex ) != 0 ) && ( (answer & EXIO_DISCRET_ERRORCODE_ERROR) == 0) )
				{
					res = OK ;
				}
			}
		}

		EXIO_Unprotect();
	}

	return res ;
}

//
//
//

int EXIO_CommandSetState( int discretOutputIndex , unsigned char state )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	if ( exioReadyToCommand == TRUE )
	{
		EXIO_Protect();

		unsigned char param = discretOutputIndex | state ;

		if ( EXIO_AddTask( EXIO_CMD_DISCRET_SET_STATE , &param , 1 ) == OK )
		{
			unsigned char answer = 0 ;
			if ( EXIO_WaitTaskAnswer( EXIO_TIMEOUT , &answer , 1 ) == OK )
			{
				if ( (( answer & discretOutputIndex ) != 0 ) && ( (answer & EXIO_DISCRET_ERRORCODE_ERROR) == 0) )
				{
					res = OK ;
				}
			}
		}

		EXIO_Unprotect();
	}

	return res ;
}

//
//
//

int EXIO_CheckIncomingBufferForValidAnswer( unsigned char * buffer , int bufferLength , int * startMessagePosition , int * messageLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	if ( (messageLength == NULL) || ( startMessagePosition == NULL ) )
		return res;

	int startByteIndex = -1 ;
	int endByteIndex = -1 ;
	int idx = 0 ;
	for( ;idx < bufferLength ; ++idx )
	{
		if ( buffer[ idx ] == 0xFF )
		{
			startByteIndex = idx ;
		}

		else if ( buffer[ idx ] == 0xFE)
		{
			endByteIndex = idx ;
		}

		if( (startByteIndex > 0) && (endByteIndex>0) )
		{
			break;
		}
	}


	if ( (startByteIndex >= 0) && (endByteIndex > 0) && ( startByteIndex < endByteIndex ) )
	{
		if ( ( endByteIndex - startByteIndex ) > 1 )
		{

			if ( buffer[ startByteIndex + 1 ] & 0x80 )
			{
				(*startMessagePosition) = startByteIndex ;
				(*messageLength) = endByteIndex - startByteIndex + 1 ;
				res = OK ;
			}

		}
	}

	return res;
}

//
//
//

int EXIO_ExecutTask( unsigned char * buffer , int bufferLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res  = ERROR_GENERAL ;

	EXIO_TaskProtect();

	if ( exioTask.state == EXIO_TASK_STATE_WAIT_ANSWER )
	{
		if ( (exioTask.cmd & 0x80) == buffer[ 1 ] )
		{
			exioTask.state = EXIO_TASK_STATE_READY ;

			int answerLength = ( bufferLength - 1 ) - 2 ;
			if (answerLength > EXIO_DISCRET_OUTPUT_NUMB)
			{
				answerLength = EXIO_DISCRET_OUTPUT_NUMB ;
			}

			memcpy( exioTask.answer , &buffer[2] , answerLength );
			res = OK ;
		}

	}

	EXIO_TaskUnprotect();

	return res ;
}

//
//
//

int EXIO_HandleAsyncMessage( unsigned char * buffer , int bufferLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	if ( buffer[1] == EXIO_MES_DISCRET_CHANGE )
	{
		unsigned char evDesc[EVENT_DESC_SIZE];
		int descriptionLength = ( bufferLength - 1 ) - 2 ;
		if( descriptionLength > EVENT_DESC_SIZE )
		{
			descriptionLength = EVENT_DESC_SIZE ;
		}
		memcpy( evDesc ,&buffer[2] , descriptionLength );
		STORAGE_JournalUspdEvent(EVENT_EXIO_STATE_CHANGED , evDesc );

		res = OK ;
	}

	return res ;
}

//
//
//

int EXIO_AddTask( unsigned char cmd , unsigned char * param , int paramLength )
{
	DEB_TRACE(DEB_IDX_EXIO)
	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_AddTask start");

	int res = ERROR_GENERAL ;

	EXIO_TaskProtect();

	if ( exioTask.state == EXIO_TASK_STATE_EMPTY )
	{
		exioTask.state = EXIO_TASK_STATE_WAIT_ANSWER ;
		exioTask.cmd = cmd ;

		if ( paramLength > EXIO_DISCRET_OUTPUT_NUMB )
		{
			paramLength = EXIO_DISCRET_OUTPUT_NUMB ;
		}

		if ( paramLength  > 0 )
		{
			memcpy( exioTask.param , param , paramLength );
		}

		//write command to uart
		int bufferToWriteLength = 2 /*start and stop bytes*/ + 1 /*cmd*/ + paramLength ;
		unsigned char * bufferToWrite = malloc( bufferToWriteLength * sizeof( unsigned char ) );
		if ( bufferToWrite )
		{
			int bufferToWritePos = 0 ;

			bufferToWrite[ bufferToWritePos++ ] = 0xFF ;
			bufferToWrite[ bufferToWritePos++ ] = cmd ;

			if ( paramLength > 0 )
			{
				int paramIndex = 0 ;
				for( ; paramIndex < paramLength ; ++paramIndex )
				{
					bufferToWrite[ bufferToWritePos++ ] = param[ paramIndex ] ;
				}
			}

			bufferToWrite[ bufferToWritePos++ ] = 0xFE ;

			COMMON_BUF_DEBUG( DEBUG_EXIO "EXIO_AddTask buffer to write: ", bufferToWrite , bufferToWriteLength  );


			int w = 0 ;
			if ( SERIAL_Write( PORT_EXIO , bufferToWrite , bufferToWriteLength , &w) == OK )
			{
				if ( w == bufferToWriteLength )
				{
					res = OK ;
				}
			}

			free( bufferToWrite );
			bufferToWrite = NULL ;
		}
	}

	EXIO_TaskUnprotect();

	if ( res != OK )
	{
		EXIO_ClearTask() ;
	}

	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_AddTask return [%s] " , getErrorCode(res));

	return res ;
}

//
//
//

void EXIO_ClearTask()
{
	DEB_TRACE(DEB_IDX_EXIO)

	EXIO_TaskProtect();

	exioTask.state = EXIO_TASK_STATE_EMPTY ;
	exioTask.cmd = 0x00 ;
	memset( exioTask.param , 0 , EXIO_DISCRET_OUTPUT_NUMB );
	memset( exioTask.answer , 0 , EXIO_DISCRET_OUTPUT_NUMB );

	EXIO_TaskUnprotect();

	return ;
}

//
//
//

int EXIO_WaitTaskAnswer( int timeout , unsigned char * answer , int maxAnswerLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_WaitTaskAnswer start");

	int res = ERROR_TIME_IS_OUT ;

	if( !answer )
	{
		return res ;
	}

	BOOL loopExitFlag = FALSE ;
	int taskExamineCounter = timeout ;
	do
	{
		COMMON_Sleep( SECOND );
		--taskExamineCounter ;

		EXIO_TaskProtect();

		if ( exioTask.state == EXIO_TASK_STATE_READY )
		{
			if ( maxAnswerLength > EXIO_DISCRET_OUTPUT_NUMB )
				maxAnswerLength = EXIO_DISCRET_OUTPUT_NUMB ;

			memcpy( answer , exioTask.answer , maxAnswerLength );

			loopExitFlag = TRUE ;
			res = OK ;
		}

		EXIO_TaskUnprotect();
	}
	while( (taskExamineCounter > 0) && ( loopExitFlag == FALSE ));

	EXIO_ClearTask();

	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_WaitTaskAnswer return [%s] " , getErrorCode(res));
	return res ;
}

//
//
//

BOOL EXIO_CheckConfig( unsigned char * currentState )
{
	DEB_TRACE(DEB_IDX_EXIO)

	BOOL res = TRUE ;
	if( currentState == NULL )
		return FALSE ;


	exioCfg_t * exioCfg = NULL;
	if ( STORAGE_GetExioCfg( &exioCfg ) == OK )
	{
		if( exioCfg )
		{
			if ( EXIO_SortParamArray( currentState , EXIO_DISCRET_OUTPUT_NUMB ) == OK )
			{
				int discretIndex = 0 ;
				for( ; discretIndex < EXIO_DISCRET_OUTPUT_NUMB ; ++discretIndex )
				{
					if ( exioCfg->discretCfg[ discretIndex ].index == ((currentState[discretIndex] >> 4) & 0x0F)  )
					{
						if ( exioCfg->discretCfg[ discretIndex ].mode != ( currentState[discretIndex] & EXIO_DISCRET_MODE_INPUT ) )
						{
							res = FALSE;
							break;
						}

						if ( exioCfg->discretCfg[ discretIndex ].defaultValue != ( currentState[discretIndex] & EXIO_DISCRET_DEFAULT_ON ) )
						{
							res = FALSE;
							break;
						}

						if ( exioCfg->discretCfg[ discretIndex ].defaultValue != ( currentState[discretIndex] & EXIO_DISCRET_CURRENT_STATE_ON ) )
						{
							res = FALSE;
							break;
						}

					}
					else
					{
						res = FALSE ;
						break;
					}
				}

			}
			else
			{
				res = FALSE ;
			}



		}

	}

	return res ;
}

//
//
//

int EXIO_SortParamArray( unsigned char * paramArray , int paramArrayLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;
	if( (paramArray == NULL) || ( paramArrayLength == 0 ) )
	{
		return res ;
	}

	while(1)
	{
		int exitFlag = 1 ;

		int idx = 0 ;
		for( ; idx < (paramArrayLength - 1) ; ++idx )
		{
			unsigned int firstDiscretNumber = (paramArray[ idx ] >> 4) & 0x0F ;
			unsigned int secondDiscretNumber = (paramArray[ idx + 1] >> 4) & 0x0F ;

			if ( firstDiscretNumber > secondDiscretNumber )
			{
				exitFlag = 0 ;
				unsigned char temp = paramArray[ idx ] ;
				paramArray[ idx ] = paramArray[ idx + 1 ] ;
				paramArray[ idx + 1 ] = temp ;
			}
		}

		if ( exitFlag == 1 )
		{
			break ;
		}
	}

	res = OK ;


	return res ;
}

//
//
//

int EXIO_CommandGetState( unsigned char * answerBuffer )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	EXIO_Protect();

	if ( !answerBuffer )
	{
		EXIO_Unprotect();
		return res ;
	}

	res = EXIO_AddTask( EXIO_CMD_DISCRET_GET_STATE , NULL , 0 );
	if ( res != OK )
	{
		EXIO_Unprotect();
		return res ;
	}

	res =  EXIO_WaitTaskAnswer( EXIO_TIMEOUT , answerBuffer , EXIO_DISCRET_OUTPUT_NUMB) ;

	EXIO_Unprotect();
	return res ;
}






#if 0
void EXIO_ClearTask()
{
	DEB_TRACE(DEB_IDX_EXIO)

	EXIO_TaskProtect();

	exioTask.cmd = 0 ;
	memset ( exioTask.param , 0 , EXIO_DISCRET_OUTPUT_NUMB );
	memset ( exioTask.answer , 0 , EXIO_DISCRET_OUTPUT_NUMB ) ;
	exioTask.result = EXIO_RESULT_UNKNOWN ;
	exioTask.queueFlag = 0 ;

	EXIO_TaskUnprotect();
}

//
//
//

int EXIO_AddTask( unsigned char cmd , unsigned char * param , int paramLength )
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	EXIO_ClearTask();

	EXIO_TaskProtect();

	exioTask.cmd = cmd ;
	memcpy( exioTask.param , param , paramLength * sizeof( unsigned char ) );
	exioTask.result = EXIO_RESULT_UNKNOWN ;
	exioTask.queueFlag = 1 ;

	EXIO_TaskUnprotect();

	int tryCounter = 0 ;
	while ( tryCounter < 50)
	{
		EXIO_TaskProtect();

		int taskResult = exioTask.result ;

		EXIO_TaskUnprotect();

		if ( taskResult == EXIO_RESULT_OK )
		{
			res = OK ;
			break;
		}
		else if ( taskResult == EXIO_RESULT_ERROR )
		{
			break;
		}

		COMMON_Sleep( 100 );
	}

	return res ;
}

//
//
//

int EXIO_CmdSetDiscretMode()
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	//TODO

	return res ;
}


//
//
//

int EXIO_CmdSetDiscretState()
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	//TODO

	return res ;
}

//
//
//

int EXIO_CmdGetDiscretState()
{
	DEB_TRACE(DEB_IDX_EXIO)

	int res = ERROR_GENERAL ;

	//TODO

	return res ;
}

//
//
//

void * EXIO_Do(void * param)
{
	DEB_TRACE(DEB_IDX_EXIO)

	exioThreadState = TRUE ;



	enum { EXIO_STATE_GET_CFG ,
		   EXIO_STATE_CHECK_CFG ,
		   EXIO_STATE_WAIT_INCOMING_PACKAGE ,
		   EXIO_STATE_READ ,
		   EXIO_STATE_CHECK_INCOMING_PACKAGE ,
		   EXIO_STATE_HANDLE ,
		   EXIO_STATE_WRITE_CMD} ;

	int bytesAvailableToRead = 0 ;

	unsigned char incomingBuffer[ EXIO_INCOMING_BUF_LENGTH ] ;
	memset( incomingBuffer , 0 , EXIO_INCOMING_BUF_LENGTH );
	int incomingBufferPos = 0 ;
	int tailPos = -1 ;

	int exioState = EXIO_STATE_GET_CFG ;

	while( exioExecut == TRUE )
	{
		switch( exioState )
		{
			case EXIO_STATE_GET_CFG:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_GET_CFG");

				EXIO_TaskProtect();

				exioTask.cmd = EXIO_CMD_DISCRET_GET_STATE ;
				exioTask.queueFlag = 1 ;
				exioTask.intitiateCheckingFlag = 1 ;

				EXIO_TaskUnprotect();

				exioState = EXIO_STATE_CHECK_CFG ;

			}
			break;

			case EXIO_STATE_CHECK_CFG:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_CHECK_CFG");

				exioCfg_t * exioCfg = NULL;

				STORAGE_GetExioCfg( &exioCfg );



				//making byte version of configuration
				unsigned char etalonConfing[ EXIO_DISCRET_OUTPUT_NUMB ];
				memset( etalonConfing , 0 , EXIO_DISCRET_OUTPUT_NUMB );

				int index = 0 ;
				for ( ; index < EXIO_DISCRET_OUTPUT_NUMB ; ++index  )
				{
					etalonConfing[ index ] = ( (exioCfg->discretCfg[index].index & 0x0f ) << 4 ) + \
							( (exioCfg->discretCfg[index].mode & 0x01) << 3 ) + \
							( (exioCfg->discretCfg[index].defaultValue & 0x01) << 1) ;
				}

				//making current byte version of configuration

				unsigned char currentConfig[ EXIO_DISCRET_OUTPUT_NUMB ] ;
				memset( currentConfig , 0, EXIO_DISCRET_OUTPUT_NUMB );

				for(index = 0 ; index < EXIO_DISCRET_OUTPUT_NUMB ; ++index)
				{
					int discretIndex;
					int i = 2 ;
					for( ; i < tailPos ; ++i )
					{
						discretIndex = ( incomingBuffer[i] & 0xF0 ) >> 4 ;
						if ( discretIndex == (index + 1) )
							break;
					}

					//masking state and error bits
					currentConfig[index] = incomingBuffer[i] & 0xfa ;

				}

				if ( memcmp( etalonConfing , currentConfig , EXIO_DISCRET_OUTPUT_NUMB ) == 0 )
				{
					//config is valid
					exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
				}
				else
				{
					exioState = EXIO_STATE_WRITE_CMD ;

					exioTask.queueFlag = 1 ;
					memcpy ( exioTask.param , etalonConfing , EXIO_INCOMING_BUF_LENGTH ) ;
					exioTask.cmd = EXIO_CMD_DISCRET_SET_MODE ;
				}

				//clear incoming buffer
				memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
				incomingBufferPos = 0 ;
				tailPos = -1 ;
			}
			break;

			case EXIO_STATE_WRITE_CMD:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_WRITE_CMD");
				//prepare outgoing buffer
				unsigned char outgoingBuffer[32];
				memset(outgoingBuffer , 0 , 32);


				int outgoingBufferPos = 0 ;

				outgoingBuffer[ outgoingBufferPos++ ] = 0xFF ;
				outgoingBuffer[ outgoingBufferPos++ ] = exioTask.cmd ;

				int paramIndex = 0 ;

				for( ; paramIndex < EXIO_DISCRET_OUTPUT_NUMB ; ++paramIndex )
				{
					if ( exioTask.param[ paramIndex ] == 0 )
					{
						break;
					}
					else
					{
						outgoingBuffer[ outgoingBufferPos++ ] = exioTask.param[ paramIndex ] ;
					}
				}

				outgoingBuffer[ outgoingBufferPos++ ] = 0xFE ;

				//send buffer

				int wr = 0 ;
				if ( SERIAL_Write(PORT_EXIO , outgoingBuffer , outgoingBufferPos , &wr) == OK )
				{
					//fflush queue flag
					exioTask.queueFlag = 0 ;
				}
				exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;


			}
			break;

			case EXIO_STATE_WAIT_INCOMING_PACKAGE:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				bytesAvailableToRead = 0 ;
				if ( SERIAL_Check( PORT_EXIO , &bytesAvailableToRead ) == OK )
				{
					if ( bytesAvailableToRead > 0  )
					{
						exioState = EXIO_STATE_READ ;
						COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE , some data detected for reed");
					}
					else
					{
						//Check if we have some data to send

						EXIO_TaskProtect();

						if ( exioTask.queueFlag != 0 )
						{
							COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE , have some data to send");
							exioState = EXIO_STATE_WRITE_CMD ;
						}

						EXIO_TaskUnprotect();
					}

					if ( exioState == EXIO_STATE_WAIT_INCOMING_PACKAGE )
						COMMON_Sleep(10);

				}
				else
				{
					COMMON_Sleep( SECOND );
				}
			}
			break;

			case EXIO_STATE_READ:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_READ");
				int rd = 0 ;
				if ( SERIAL_Read( PORT_EXIO ,\
								 &incomingBuffer[ incomingBufferPos ] ,\
								 ( incomingBufferPos + bytesAvailableToRead < EXIO_INCOMING_BUF_LENGTH ) ? bytesAvailableToRead : EXIO_INCOMING_BUF_LENGTH - incomingBufferPos ,\
								 &rd ) == OK )
				{
					incomingBufferPos += rd ;

					exioState = EXIO_STATE_CHECK_INCOMING_PACKAGE ;
				}
				else
				{
					//clear incoming buffer
					//clear incoming buffer
					memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
					incomingBufferPos = 0 ;
					tailPos = -1 ;

					exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
				}
			}
			break;

			case EXIO_STATE_CHECK_INCOMING_PACKAGE:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_CHECK_INCOMING_PACKAGE");
				if ( incomingBuffer[ 0 ] == 0xFF )
				{
					tailPos = -1 ;
					int bufferIndex = 0 ;
					//searching for tail byte
					for ( ; bufferIndex < incomingBufferPos ; ++bufferIndex)
					{
						if ( incomingBuffer[ bufferIndex ] == 0xFE )
						{
							tailPos = bufferIndex ;
							break;
						}
					}

					if ( tailPos > 0 )
					{
						exioState = EXIO_STATE_HANDLE ;
					}
					else
					{
						exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;

						if ( incomingBufferPos == EXIO_INCOMING_BUF_LENGTH )
						{
							//clear incoming buffer
							memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
							incomingBufferPos = 0 ;
							tailPos = -1 ;
						}
					}

				}
				else
				{
					memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
					incomingBufferPos = 0 ;
					tailPos = -1 ;

					exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
				}

			}
			break;

			case EXIO_STATE_HANDLE:
			{
				DEB_TRACE(DEB_IDX_EXIO)

				COMMON_STR_DEBUG(DEBUG_EXIO "EXIO exioState = EXIO_STATE_HANDLE");
				unsigned char cmd = incomingBuffer[1] ;

				EXIO_TaskProtect();

				switch( cmd )
				{
					case (EXIO_CMD_DISCRET_GET_STATE | 0x80) :
					{
						COMMON_STR_DEBUG(DEBUG_EXIO "EXIO EXIO_CMD_DISCRET_GET_STATE catched ");
						exioTask.result = EXIO_RESULT_OK ;
						memcpy( exioTask.answer , &incomingBuffer[2] , tailPos - 2 - 1);

						if ( exioTask.intitiateCheckingFlag == 1 )
						{
							exioTask.intitiateCheckingFlag = 0 ;
							exioState = EXIO_STATE_CHECK_CFG ;
						}

					}
					break;

					case (EXIO_CMD_DISCRET_SET_MODE | 0x80) :
					{
						COMMON_STR_DEBUG(DEBUG_EXIO "EXIO EXIO_CMD_DISCRET_SET_MODE catched ");
						exioTask.result = EXIO_RESULT_OK ;
						memcpy( exioTask.answer , &incomingBuffer[2] , tailPos - 2 - 1);

						//clear incoming buffer
						memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
						incomingBufferPos = 0 ;
						tailPos = -1 ;

						exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
					}
					break;

					case (EXIO_CMD_DISCRET_SET_STATE | 0x80) :
					{
						COMMON_STR_DEBUG(DEBUG_EXIO "EXIO EXIO_CMD_DISCRET_SET_STATE catched ");
						exioTask.result = EXIO_RESULT_OK ;
						memcpy( exioTask.answer , &incomingBuffer[2] , tailPos - 2 - 1);

						//clear incoming buffer
						memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
						incomingBufferPos = 0 ;
						tailPos = -1 ;

						exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
					}
					break;

					case EXIO_MES_DISCRET_CHANGE :
					{
						COMMON_STR_DEBUG(DEBUG_EXIO "EXIO EXIO_MES_DISCRET_CHANGE catched ");

						int descriptionLength = tailPos - 2 ;
						unsigned char evDesc[ EVENT_DESC_SIZE ];
						memset( evDesc , 0 , EVENT_DESC_SIZE );

						memcpy( evDesc ,&incomingBuffer[2] , descriptionLength );

						STORAGE_JournalUspdEvent(EVENT_EXIO_STATE_CHANGED , evDesc );


						//clear incoming buffer
						memset ( incomingBuffer , 0, EXIO_INCOMING_BUF_LENGTH ) ;
						incomingBufferPos = 0 ;
						tailPos = -1 ;



						exioState = EXIO_STATE_WAIT_INCOMING_PACKAGE ;
					}
					break;

					default:
						exioTask.result = EXIO_RESULT_ERROR ;
						memcpy( exioTask.answer , &incomingBuffer[2] , tailPos - 2 - 1);
						break;
				}

				EXIO_TaskUnprotect();
			}
			break;


			default:
				break;

		}
	}

	exioThreadState = FALSE ;

	COMMON_STR_DEBUG(DEBUG_EXIO "EXIO_Do() thread quit");

	return NULL;
}
#endif
//EOF
