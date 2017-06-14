/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O
*/
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "boolType.h"
#include "common.h"
#include "debug.h"
#include "storage.h"
#include "connection.h"
#include "gsm.h"
#include "eth.h"
#include "cli.h"

//foo
void CONNECTION_Protect();
void CONNECTION_Unprotect();
int CONNECTION_InitGsmListenerThread(connection_t * connection);
int CONNECTION_InitManagerThread();
int CONNECTION_InitGsmThread(connection_t * connection);
int CONNECTION_InitEthThread(connection_t * connection);
void * gsm_Do( void * param );
void * gsmListener_Do( void * param );
void * eth_Do( void * param );
void * manager_Do( void * param );

volatile BOOL needToReinit = FALSE ;

//static variables
connection_t connectionProps;
static pthread_mutex_t connectionMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t connectionManageMux = PTHREAD_MUTEX_INITIALIZER;
volatile BOOL gsmListenerExecution = FALSE;
volatile BOOL gsmExecution = FALSE;
volatile BOOL ethExecution = FALSE;
volatile BOOL gsmListenerState = FALSE;
volatile BOOL gsmState = FALSE;
volatile BOOL managerState = FALSE;
volatile BOOL ethState = FALSE;
volatile BOOL gsmListenerPaused = FALSE;
volatile BOOL gsmPaused = FALSE;
volatile BOOL ethPaused = FALSE;
pthread_t gsmThread;
pthread_t managerThread;
pthread_t gsmListenThread ;
pthread_t ethThread;

void CONNECTION_Protect()
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	//COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Protect enter");
	pthread_mutex_lock(&connectionMux);
	//COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Protect exit");
}

//
//
//

void CONNECTION_Unprotect()
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	//COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Unprotect enter");
	pthread_mutex_unlock(&connectionMux);
	//COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Unprotect exit");
}

//
//
//

int CONNECTION_Initialize()
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Initialize start");
	int res = ERROR_GENERAL ;

	needToReinit = FALSE ;

	if ( CONNECTION_RereadConnectionProperties() == OK )
	{
		CONNECTION_Start();
		res = CONNECTION_InitManagerThread();
		if ( res != OK ) return res ;
		res = CONNECTION_InitGsmListenerThread(&connectionProps);
		if ( res != OK ) return res ;
		res = CONNECTION_InitGsmThread(&connectionProps);
		if ( res != OK ) return res ;
		res = CONNECTION_InitEthThread(&connectionProps);
		if ( res != OK ) return res ;
	}

	COMMON_STR_DEBUG (DEBUG_CONNECTION "CONNECTION_Initialize return [%s]", getErrorCode(res));
	return res;
}

//
//
//

int CONNECTION_RereadConnectionProperties()
{
	DEB_TRACE(DEB_IDX_CONNECTION);
	int res = STORAGE_GetConnection( &connectionProps );

	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION: struct connectionProps have next fields:\n"\
							"\tsimTryes=%d\n\tserver=%s\n\tport=%d\n\tapn1=%s\n\tapn1_pass=%s\n\tapn1_user=%s\n\tapn2=%s\n\tapn2_pass=%s\n\tapn2_user=%s\n\t"\
							"allowedIncomingNum1Sim1=%s\n\tallowedIncomingNum2Sim1=%s\n\tallowedIncomingNum1Sim2=%s\n\tallowedIncomingNum2Sim2=%s\n\t"\
							"serverPhoneSim1=%s\n\tserverPhoneSim2=%s\n\tsimPriniciple=%d\n\tmode=%d\n\tprotocol=%d\n\tmyIp=%s\n\tgateway=%s\n\t"\
							"mask=%s\n\tdns1=%s\n\tdns2=%s\n\teth0mode=%d\n\thwaddr=%s\n\tserviceIP=%s\n\tservicePort=%u\n"\
							"\tpin1=%u\n\tpin1_use=%u\n\tpin2=%u\n\tpin2_use=%u" , \
							connectionProps.simTryes , connectionProps.server , connectionProps.port , connectionProps.apn1 ,\
							connectionProps.apn1_pass , connectionProps.apn1_user , connectionProps.apn2 , connectionProps.apn2_pass ,\
							connectionProps.apn2_user , connectionProps.allowedIncomingNum1Sim1 , connectionProps.allowedIncomingNum2Sim1 ,\
							connectionProps.allowedIncomingNum1Sim2 , connectionProps.allowedIncomingNum2Sim2 , \
							connectionProps.serverPhoneSim1 , connectionProps.serverPhoneSim2 , connectionProps.simPriniciple ,\
							connectionProps.mode , connectionProps.protocol , connectionProps.myIp , connectionProps.gateway ,\
							connectionProps.mask , connectionProps.dns1 , connectionProps.dns2 , connectionProps.eth0mode , connectionProps.hwaddress ,\
							connectionProps.serviceIp , connectionProps.servicePort , \
							connectionProps.gsmCpinProperties[0].pin , connectionProps.gsmCpinProperties[0].usage , \
							connectionProps.gsmCpinProperties[1].pin , connectionProps.gsmCpinProperties[1].usage);

	return res ;
}

//
//
//

void CONNECTION_Stop(unsigned char disconnectReason)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	pthread_mutex_lock(&connectionManageMux);

	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION_Stop start");



//	gsmListenerExecution = FALSE;
//	gsmExecution = FALSE;
//	ethExecution = FALSE;

//	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION WAITING WHILE THREADS ARE PAUSED");
//	do
//	{
//		COMMON_Sleep( SECOND );
//		COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION eth = [%d] , gsm = [%d] , gsmListener = [%d]" , ethPaused , gsmPaused , gsmListenerPaused);

//	}
//	while( (ethPaused == FALSE) || (gsmPaused == FALSE) || (gsmListenerPaused == FALSE) ) ;

	//pausing eth thread
	ETH_RebootStates();
	ethExecution = FALSE;
	do
	{
		COMMON_Sleep( SECOND );
		COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION eth = [%d]" , ethPaused );

	}
	while((ethPaused == FALSE) ) ;
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION ETH THREAD IS PAUSED");

	//pausing gsm thread
	gsmExecution = FALSE;
	do
	{
		COMMON_Sleep( SECOND );
		COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION gsm = [%d]" , gsmPaused );

	}
	while((gsmPaused == FALSE) ) ;
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION GSM THREAD IS PAUSED");

	//pausing gsm-listener thread
	gsmListenerExecution = FALSE;
	do
	{
		COMMON_Sleep( SECOND );
		COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION gsmListener = [%d]" , gsmListenerPaused );

	}
	while((gsmListenerPaused == FALSE) ) ;
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION GSM LISTENER THREAD IS PAUSED");

	if (CONNECTION_ConnectionState() == OK)
	{
		STORAGE_EVENT_DISCONNECTED(disconnectReason);
	}

	GSM_RebootStates();	

	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION_Stop exit");

	pthread_mutex_unlock(&connectionManageMux);
}

//
//
//

void CONNECTION_Start()
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	pthread_mutex_lock(&connectionManageMux);

	gsmListenerExecution = TRUE;
	gsmExecution = TRUE;
	ethExecution = TRUE;

	pthread_mutex_unlock(&connectionManageMux);
}

//
//
//

int CONNECTION_Write(unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	CONNECTION_Protect();
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION WRITE");

	int res = ERROR_GENERAL;

	LCD_SetActCursor();

	switch (connectionProps.mode)
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
			res = GSM_Write(&connectionProps, buf, size, written);
			break;

		case CONNECTION_ETH_ALWAYS:
		case CONNECTION_ETH_CALENDAR:
		case CONNECTION_ETH_SERVER:
			res = ETH_Write(&connectionProps, buf, size, written);
			break;

		default:
			res = ERROR_NO_CONNECTION_PARAMS ;
			break;
	}

	LCD_RemoveActCursor();

	//debug
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION WRITE %d bytes %s", (*written),  getErrorCode(res));

	CONNECTION_Unprotect();
	return res;
}

//
//
//

int CONNECTION_Read(unsigned char * buf, int size, unsigned long * readed)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	CONNECTION_Protect();

	int res = ERROR_GENERAL;

	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION READ");

	LCD_SetActCursor();

	switch (connectionProps.mode)
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
			res = GSM_Read(&connectionProps, buf, size, readed);
			break;

		case CONNECTION_ETH_ALWAYS:
		case CONNECTION_ETH_CALENDAR:
		case CONNECTION_ETH_SERVER:
			res = ETH_Read(&connectionProps, buf, size, readed);
			break;

		default:
			res = ERROR_NO_CONNECTION_PARAMS ;
			break;
	}
	LCD_RemoveActCursor();

	CONNECTION_Unprotect();
	//debug
	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION READ %d bytes %s", (*readed),  getErrorCode(res));

	return res;
}

//
//
//

int CONNECTION_ReadAvailable(void)
{
	DEB_TRACE(DEB_IDX_CONNECTION)

	int res=ERROR_GENERAL;

	COMMON_STR_DEBUG(DEBUG_CONNECTION "CONNECTION CONNECTION_ReadAvailable");

	CONNECTION_Protect();

	switch (connectionProps.mode)
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
			res = GSM_ReadAvailable(&connectionProps);
			break;

		case CONNECTION_ETH_ALWAYS:
		case CONNECTION_ETH_CALENDAR:
		case CONNECTION_ETH_SERVER:
			res = ETH_ReadAvailable(&connectionProps);
			break;

		default:
			res = ERROR_NO_CONNECTION_PARAMS ;
			break;
	}

	CONNECTION_Unprotect();
	return res;
}

//
//
//

int CONNECTION_ConnectionState()
{
	DEB_TRACE(DEB_IDX_CONNECTION)


	CONNECTION_Protect();

	int res=ERROR_GENERAL;

	switch (connectionProps.mode)
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
			res = GSM_ConnectionState();
			break;

		case CONNECTION_ETH_ALWAYS:
		case CONNECTION_ETH_CALENDAR:
		case CONNECTION_ETH_SERVER:
			res = ETH_ConnectionState();
			break;
	}

	CONNECTION_Unprotect();

	return res;
}

//
//
//

int CONNECTION_InitGsmListenerThread(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	int res = ERROR_GENERAL ;

	GSM_RebootStates();

	gsmListenerState = FALSE;
	if ( !pthread_create(&gsmListenThread, NULL, gsmListener_Do, connection) )
	{
		int connectionInitCounter = MAX_COUNT_CONNECTION_INIT_WAIT ;
		do
		{
			--connectionInitCounter;
			COMMON_Sleep(DELAY_CONNECTION_INIT);
		}
		while( (gsmListenerState == FALSE) && (connectionInitCounter > 0) );

		if ( gsmListenerState == TRUE)
		{
			res = OK ;
		}
	}

	return res ;
}

//
//
//

int CONNECTION_InitManagerThread()
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	int res = ERROR_GENERAL ;

	managerState = FALSE;
	if ( !pthread_create(&managerThread, NULL, manager_Do, NULL) )
	{
		int connectionInitCounter = MAX_COUNT_CONNECTION_INIT_WAIT ;
		do
		{
			--connectionInitCounter;
			COMMON_Sleep(DELAY_CONNECTION_INIT);
		}
		while( (managerState == FALSE) && (connectionInitCounter > 0) );

		if ( managerState == TRUE)
		{
			res = OK ;
		}
	}

	return res ;
}

//
//
//

int CONNECTION_InitGsmThread(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	int res = ERROR_GENERAL ;

	gsmState = FALSE;
	if ( !pthread_create(&gsmThread, NULL, gsm_Do, connection) )
	{
		int connectionInitCounter = MAX_COUNT_CONNECTION_INIT_WAIT ;
		do
		{
			--connectionInitCounter;
			COMMON_Sleep(DELAY_CONNECTION_INIT);
		}
		while( (gsmState == FALSE) && (connectionInitCounter > 0) );

		if ( gsmState == TRUE)
		{
			res = OK ;
		}
	}

	return res ;
}

//
//
//

int CONNECTION_InitEthThread(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	int res = ERROR_GENERAL ;

	ethState = FALSE;
	if ( !pthread_create(&ethThread, NULL, eth_Do, connection) )
	{
		int connectionInitCounter = MAX_COUNT_CONNECTION_INIT_WAIT ;
		do
		{
			--connectionInitCounter;
			COMMON_Sleep(DELAY_CONNECTION_INIT);
		}
		while( (ethState == FALSE) && (connectionInitCounter > 0) );

		if ( ethState == TRUE)
		{
			res = OK ;
		}
	}

	return res ;
}

//
//
//

void * gsm_Do( void * param )
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	gsmState = TRUE;

	connection_t * connection = (connection_t *)param;

	while( 1 )
	{
		SVISOR_THREAD_SEND(THREAD_GSM);
		if ( gsmExecution == TRUE )
		{
			gsmPaused = FALSE ;
			GSM_ConectionDo(connection);
		}
		else
		{
			gsmPaused = TRUE ;
		}
		COMMON_Sleep(100);
	}

}

//
//
//

void * manager_Do( void * param __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	managerState = TRUE;
	while( 1 )
	{
		SVISOR_THREAD_SEND(THREAD_CONN_MANAGER);
		if ( needToReinit == TRUE )
		{
			CONNECTION_Stop(0x04);

			STORAGE_LoadConnections();

			CONNECTION_RereadConnectionProperties() ;

			CONNECTION_Start();

			needToReinit = FALSE ;
		}
		else
		{
			COMMON_Sleep(100);
		}

	}

}

//
//
//

void * gsmListener_Do( void * param )
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	gsmListenerState = TRUE;

	connection_t * connection = (connection_t *)param;

	GSM_SetNStatusToDefault();

	while( 1 )
	{
		SVISOR_THREAD_SEND(THREAD_GSM_LISTENER);
		if ( gsmListenerExecution == TRUE )
		{
			gsmListenerPaused = FALSE ;
			GSM_ListenerDo(connection);
		}
		else
		{
			gsmListenerPaused = TRUE ;
			COMMON_Sleep(100);
		}
	}

}

//
//
//

void * eth_Do( void * param )
{
	DEB_TRACE(DEB_IDX_CONNECTION);

	ethState = TRUE;

	connection_t * connection = (connection_t *)param;

	while( 1 )
	{
		SVISOR_THREAD_SEND(THREAD_ETH);
		if ( ethExecution == TRUE )
		{
			ethPaused = FALSE ;
			ETH_ConectionDo(connection);
		}
		else
		{
			ethPaused = TRUE ;
		}
		COMMON_Sleep(100);
	}

}

//
//
//

void CONNECTION_RestartAndLoadNewParametres()
{
	needToReinit = TRUE ;
	return ;
}

//
//
//
static BOOL lastServiceAttemptFlag = TRUE ;
//static BOOL lastSessionProtoTrFlag = FALSE ;
static BOOL lastSessionProtoTrFlag = TRUE ;

BOOL CONNECTION_CheckTimeToConnect( BOOL * service )
{
	BOOL res = FALSE ;

	(*service) = FALSE ;
	if ( STORAGE_CheckTimeToServiceConnect() == TRUE )
	{
		COMMON_STR_DEBUG(DEBUG_GSM "___It is time to service connect");
		COMMON_STR_DEBUG(DEBUG_GSM "___lastServiceAttemptFlag [%d], lastSessionProtoTrFlag [%d]", lastServiceAttemptFlag , lastSessionProtoTrFlag);
		if ( (lastServiceAttemptFlag == TRUE) && (lastSessionProtoTrFlag == TRUE))
		{			
			(*service) = TRUE;
		}
		else
		{
			lastServiceAttemptFlag = TRUE ;
		}
	}
	else
	{
		lastServiceAttemptFlag = TRUE ;
	}

	if ( (*service) == TRUE)
	{
		//check service params
		switch ( connectionProps.mode )
		{
			case CONNECTION_GPRS_SERVER:
			case CONNECTION_GPRS_ALWAYS:
			case CONNECTION_GPRS_CALENDAR:
			case CONNECTION_ETH_ALWAYS:
			case CONNECTION_ETH_CALENDAR:
			case CONNECTION_ETH_SERVER:
			{
				if ( (strlen ( (char *)connectionProps.serviceIp ) > 0) && (connectionProps.servicePort > 0))
				{
					return TRUE ;
				}
				else
				{
					(*service) = FALSE ;
				}
			}
			break;

			case CONNECTION_CSD_INCOMING:
			case CONNECTION_CSD_OUTGOING:
			{
				if ( (strlen ( (char *)connectionProps.servicePhone ) > 0) )
				{
					return TRUE ;
				}
				else
				{
					(*service) = FALSE ;
				}
			}
			break;

			default:
			{
				(*service) = FALSE ;
			}
			break;
		}
	}

	switch( connectionProps.mode )
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_ETH_ALWAYS:
		case CONNECTION_ETH_SERVER:
		{
			res = TRUE ;
		}
		break;

		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_OUTGOING:
		case CONNECTION_ETH_CALENDAR:
		{
			calendar_t * calendar = NULL ;
			if ( connectionProps.mode == CONNECTION_ETH_CALENDAR )
			{
				calendar = &connectionProps.calendarEth ;
			}
			else
			{
				calendar = &connectionProps.calendarGsm ;
			}

			//check calendar

			dateTimeStamp currentDts ;
			COMMON_GetCurrentDts( &currentDts );

			dateTimeStamp startDts;
			startDts.t.s = 0 ;
			startDts.t.m = calendar->start_min ;
			startDts.t.h = calendar->start_hour ;

			startDts.d.d = calendar->start_mday ;
			startDts.d.m = calendar->start_month ;
			startDts.d.y = calendar->start_year ;

			if ( COMMON_CheckDts( &startDts , &currentDts ) == TRUE )
			{
				if ( COMMON_GetDtsDiff__Alt(&startDts, &currentDts, BY_MONTH) > 0 )
				{
					startDts.d.d = 1 ;
					startDts.d.m = currentDts.d.m ;
					startDts.d.y = currentDts.d.y ;
				}

				if ( (calendar->interval > 0) && ( (calendar->ratio == CONNECTION_INTERVAL_DAY) || (calendar->ratio == CONNECTION_INTERVAL_HOUR) || (calendar->ratio == CONNECTION_INTERVAL_WEEK) ))
				{
					dateTimeStamp beginConnectionWindowDts;
					memcpy(&beginConnectionWindowDts , &startDts , sizeof(dateTimeStamp) );

					BOOL loopExitFlag = FALSE ;
					while( loopExitFlag == FALSE )
					{
						dateTimeStamp endConnectionWindowDts;
						memcpy(&endConnectionWindowDts , &beginConnectionWindowDts , sizeof(dateTimeStamp) );

						int i = 0;
						for( ; i< CONNECTION_TIME_INTERVAL ; i++)
						{
							COMMON_ChangeDts( &endConnectionWindowDts , INC , BY_SEC );
						}

						if (COMMON_DtsInRange( &currentDts , &beginConnectionWindowDts , &endConnectionWindowDts ) == TRUE)
						{
							res = TRUE ;
							loopExitFlag = TRUE ;
						}
						else if ( COMMON_CheckDts( &currentDts , &beginConnectionWindowDts ) == TRUE )
						{
							//it is not time
							loopExitFlag = TRUE ;
						}

						//increment beginConnectionWindowDts
						switch( calendar->ratio )
						{
							case CONNECTION_INTERVAL_HOUR:
							{
								int incrementIndex = 0 ;
								for ( ; incrementIndex < (int)calendar->interval ; ++incrementIndex )
								{
									COMMON_ChangeDts( &beginConnectionWindowDts , INC , BY_HOUR );
								}
							}
							break;

							case CONNECTION_INTERVAL_DAY:
							{
								int incrementIndex = 0 ;
								for ( ; incrementIndex < (int)calendar->interval ; ++incrementIndex )
								{
									COMMON_ChangeDts( &beginConnectionWindowDts , INC , BY_DAY );
								}
							}
							break;

							case CONNECTION_INTERVAL_WEEK:
							{
								int incrementIndex = 0 ;
								for ( ; incrementIndex < ( (int)calendar->interval * 7 ) ; ++incrementIndex )
								{
									COMMON_ChangeDts( &beginConnectionWindowDts , INC , BY_DAY );
								}
							}
							break;

							default:
							{
								loopExitFlag = TRUE ;
							}
							break;
						}
					} //while()

				}
				else
				{
					res = TRUE ;
				}
			}
		}
		break;

		default:
			break;
	}

	return res ;
}


void CONNECTION_ReverseLastServiceAttemptFlag()
{
	if ( lastServiceAttemptFlag == TRUE )
	{
		lastServiceAttemptFlag = FALSE ;
	}
	else
	{
		lastServiceAttemptFlag = TRUE ;
	}
}

void CONNECTION_SetLastSessionProtoTrFlag(BOOL value)
{
	lastSessionProtoTrFlag = value ;
}

//EOF
