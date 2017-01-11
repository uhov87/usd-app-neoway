/*
		application code v.1
		for uspd micron 2.0x

		2012 - January
		NPO Frunze
		RUSSIA NIGNY NOVGOROD

		OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "common.h"
#include "connection.h"
#include "cli.h"
#include "pio.h"
#include "pool.h"
#include "plc.h"
#include "storage.h"
#include "boolType.h"
#include "gsm.h"
#include "ujuk.h"
#include "eth.h"
#include "debug.h"
#include "pio.h"
#include "exio.h"

#define UD_INPUT_SIZE 32
#define UD_OUTPUT_SIZE 2048

//local variables
BOOL storageUdThreadOk = FALSE;
BOOL storageUdExecution = FALSE;

//variables for plc it 700 config
it700params_t plcBaseSettings;

//connection params
connection_t connectionParams;
connection_t connectionParams_copy;
static BOOL writingSmsNewParams = FALSE ;

seasonProperties_t seasonProperties = { STORAGE_SEASON_WINTER , STORAGE_SEASON_CHANGE_DISABLE} ;
countersArray_t countersArray485 = {NULL, 0, 0};
countersArray_t countersArrayPlc = {NULL, 0, 0};
statisticMap_t counterStatistic = {NULL, 0};

exioCfg_t exioCfg;

pthread_t storageUdThread;
pthread_t storageUdClient;
void * STORAGE_DoUdListener(void * param);
void * STORAGE_udClient(void * param);

static pthread_mutex_t storageMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t storageMux2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t storageCounterStatMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t journalMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t counterSyncFlagMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t storageMuxStatistic = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t storageHardwareCtrlMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t storageSeasonFlgMux = PTHREAD_MUTEX_INITIALIZER;

time_t lastPoolReinit;
int pollQueReinitCount = 0;

//
//
//

void STORAGE_Protect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE try to protect");
	pthread_mutex_lock(&storageMux);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE protected");
}

//
//
//

void STORAGE_Unprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE try to unprotect");
	pthread_mutex_unlock(&storageMux);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE unprotected");
}

//
//
//

void STORAGE_CountersProtect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE 2 try to protect");
	pthread_mutex_lock(&storageMux2);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE 2 protected");
}

//
//
//

void STORAGE_CountersUnprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE 2 try to unprotect");
	pthread_mutex_unlock(&storageMux2);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE 2 unprotected");
}

//
//
//

void STORAGE_CounterStatusProtect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter status try to protect");
	pthread_mutex_lock(&storageCounterStatMux);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter status protected");
}

//
//
//

void STORAGE_CounterStatusUnprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter status try to unprotect");
	pthread_mutex_unlock(&storageCounterStatMux);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter status unprotected");
}

//
//
//

void STORAGE_StatisticProtect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE STATISTIC try to protect");
	pthread_mutex_lock(&storageMuxStatistic);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE STATISTIC protected");
}

//
//
//

void STORAGE_StatisticUnprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE STATISTIC try to unprotect");
	pthread_mutex_unlock(&storageMuxStatistic);
	//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE STATISTIC unprotected");
}


//
//
//

void STORAGE_HardwareWritingCtrl_Protect() {
	DEB_TRACE(DEB_IDX_STORAGE) ;

	pthread_mutex_lock(&storageHardwareCtrlMux);
}

//
//
//

void STORAGE_HardwareWritingCtrl_Unprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	pthread_mutex_unlock(&storageHardwareCtrlMux);

}

//
//
//

void STORAGE_SeasonProtect() {
	DEB_TRACE(DEB_IDX_STORAGE) ;

	pthread_mutex_lock(&storageSeasonFlgMux);
}

//
//
//

void STORAGE_SeasonUnprotect() {
	DEB_TRACE(DEB_IDX_STORAGE)

	pthread_mutex_unlock(&storageSeasonFlgMux);

}
//
// Storage part initialization
//

int STORAGE_Initialize() {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;

	unsigned char sn[32];
	memset( sn , 0 , 32 );
	COMMON_GET_USPD_SN(sn);

	if ( STORAGE_LoadSeasonAllowFlag() != OK )
	{
		STORAGE_UpdateSeasonAllowFlag( STORAGE_SEASON_CHANGE_DISABLE ) ;
	}

	if ( STORAGE_LoadCurrentSeason() != OK )
	{
		STORAGE_UpdateCurrentSeason( STORAGE_SEASON_WINTER ) ;
	}

	// reset structures
	memset(&connectionParams, 0, sizeof (connection_t));

	// load connection parrams
	if (res == OK)
		res = STORAGE_LoadConnections();

	//load parameters for RF part (for V1)
	if (res == OK)
		res = STORAGE_LoadPlcSettings();

	// load counters 485 data from configuration file
	res = STORAGE_LoadCounters485(&countersArray485);

	// load counters rf data from configuration file
	res = STORAGE_LoadCountersPlc(&countersArrayPlc);

	//create map for counters rf by rf serials
	if (res == OK)
		res = STORAGE_CreateStatisticMap();


	//refactor profile archives to new version
	if (res == OK)
		res = STORAGE_RefactoreProfileWithId();


	#if REV_COMPILE_EXIO == 1
	STORAGE_LoadExio();
	#endif

//	//save power on event
//	if ( STORAGE_CheckUspdStartFlag() == TRUE )
//		STORAGE_JournalUspdEvent(EVENT_USPD_POWER_ON , NULL);

//	//save application start event
//	STORAGE_JournalUspdEvent( EVENT_APP_POWER_ON , NULL );

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_Initialize() %s", getErrorCode(res));

	//start thread for accepting incoming connections from cgi-bin part of web
	storageUdExecution = TRUE;
	if ( pthread_create(&storageUdThread, NULL, STORAGE_DoUdListener, 0) == 0 )
		res = OK ;

	//fix last poll time reinit
	time(&lastPoolReinit);
	
	return res;
}

//
// Stop rf execution
//
void STORAGE_UdStop()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	storageUdExecution = FALSE;
}

//
// Start rf execution
//
void STORAGE_UdStart()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	storageUdExecution = TRUE;
}

//
// Check rf execution
//
BOOL STORAGE_UdIsWork()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	return (storageUdExecution && storageUdThreadOk);
}

//
// Check storage ud thread state
//
BOOL STORAGE_UdIsRun()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	return storageUdThreadOk;
}

//
//
//
void * STORAGE_DoUdListener(void * param __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int udsockfd, newsockfd;
	struct sockaddr_un serv_addr;

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread started");

	if ((udsockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: Error on creating unix domain socket");
		storageUdThreadOk = FALSE;
		return NULL;
	}

	memset(&serv_addr, 0, sizeof (serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, STORAGE_UNIX_DOMAIN_LISTENER, strlen(STORAGE_UNIX_DOMAIN_LISTENER));

	unlink(STORAGE_UNIX_DOMAIN_LISTENER);

	if (bind(udsockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: Error on bind unix domain socket");
		storageUdThreadOk = FALSE;
		return NULL;
	}

	if (chmod(STORAGE_UNIX_DOMAIN_LISTENER, S_IRWXU | S_IRWXG | S_IRWXO) < 0){
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: Error on chmod socket file");
	}

	if(listen(udsockfd, 10) < 0){
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: Error on listen unix domain socket");
		storageUdThreadOk = FALSE;
		return NULL;
	}
	storageUdThreadOk = TRUE;

	while (storageUdExecution)
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: IN ACCEPTION");
		newsockfd = accept(udsockfd, NULL, NULL);
		//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: ACCEPED");
		if (newsockfd > 0)
		{
			COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: new connection %d", newsockfd);
			pthread_create(&storageUdClient, NULL, STORAGE_udClient, (void *)&newsockfd);

		}
		else
		{
			break;
		}

		COMMON_Sleep(THREAD_SLEEPS_DELAY);
	}

	close(udsockfd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD therad quit");

	storageUdThreadOk = FALSE;

	return NULL;
}

//
//
//
void * STORAGE_udClient(void * param)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res;
	int sockFd = *(int *)param;
	int readBytes = 0;
	int writeBytes = 0;
	int reqIndex = -1;
	int iterator485 = 0;
	int iteratorPlc = 0;
	int iteratorPlcMap = 0;
	int iteratorStatistic = 0;
	int eventSize = 0;
	char * inputBuff = NULL;
	char * outputBuff = NULL;

	counterStat_t * counterStatisticCopy = NULL ;
	int counterStatisticCopyLength = 0 ;

	int loop = 1;

	dateTimeStamp lastDtsDay;
	dateTimeStamp lastDtsMonth;
	dateTimeStamp lastDtsProfile;

	memset(&lastDtsDay, 0, sizeof(dateTimeStamp));
	memset(&lastDtsMonth, 0, sizeof(dateTimeStamp));
	memset(&lastDtsProfile, 0, sizeof(dateTimeStamp));

	plcNode_t * node = NULL;
	meteregesArray_t * mArray = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD thread: new ud thread for client %d", sockFd);

	//create buffers
	inputBuff = (char *)malloc(UD_INPUT_SIZE);
	outputBuff = (char *)malloc(UD_OUTPUT_SIZE);

	//check input creation
	if (inputBuff == NULL){
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: error creating input buf, exit");
		loop = 0;
	} else
		memset(inputBuff, 0, UD_INPUT_SIZE);

	//check output creation
	if (inputBuff == NULL){
		COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: error creating output buf, exit");
		loop = 0;
	} else
		memset(outputBuff, 0, UD_INPUT_SIZE);

	while (loop){

		//read input
		readBytes = read(sockFd, inputBuff, UD_INPUT_SIZE);

		if (readBytes < 0) {
			loop = 0;

		} else {

			//debug
			if (readBytes > 0){
				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: read %d [%s]", readBytes, inputBuff);
			}

			//
			// WEB HAVE UPDATE CONFIGURATION FILE
			//

			if (strstr(inputBuff, "update||") != NULL) {
				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: UPDATE CONFIG");
				loop = 0;

				char * ptr = strstr(inputBuff, "update||");
				if (ptr != NULL){
					char cfgName[32];
					unsigned char evDesc[EVENT_DESC_SIZE];

					memset(cfgName, 0, 32);
					strcpy(cfgName, ptr+8);


					int offset = 0;
					int localFlag = 0 ;
					char * localPtr = strstr( cfgName , "local||" );
					if ( localPtr )
					{
						COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: new local configuration was detected");
						offset = strlen("local||");
						localFlag = 1 ;
					}
					COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: UPDATE CONFIG %s", cfgName + offset);

					//process update by storage
					STORAGE_PROCESS_UPDATE(cfgName);

					if ( strstr(cfgName + offset , "gsm.cfg")!=NULL ||
						 strstr(cfgName + offset , "cpin.cfg")!=NULL)
					{
						if ( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_GSM , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_GSM , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}

						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( (strstr(cfgName + offset , "eth.cfg")!=NULL) || (strstr(cfgName + offset , "ethernet.cfg")!=NULL ) )
					{
						if ( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_ETH , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_ETH , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}

						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( strstr(cfgName + offset , "connection.cfg")!=NULL )
					{
						if ( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_CONN , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_CONN , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}
						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( strstr(cfgName + offset , "plc.cfg")!=NULL )
					{
						if ( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_DEV_PLC , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_DEV_PLC , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}
						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( strstr(cfgName + offset , "rs485.cfg")!=NULL )
					{
						if( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_DEV_RS , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_DEV_RS , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}
						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( strstr(cfgName + offset , "plcNet.cfg")!=NULL )
					{
						if( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_COORD_PLC , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_CFG_COORD_PLC , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}
						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}
					else if ( strstr(cfgName + offset , ".xls") )
					{
						if( !localFlag )
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_COUNTERS_BILL , DESC_EVENT_CFG_UPDATE_BY_WUI, evDesc);
						}
						else
						{
							COMMON_CharArrayDisjunction( DESC_EVENT_UPDATE_COUNTERS_BILL , DESC_EVENT_CFG_UPDATE_BY_LOCAL, evDesc);
						}
						STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
					}


					//STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , )
				}

			//
			// CLOSE CONNECTION WITH HTTP
			//

			} else if (strstr(inputBuff, "close") != NULL) {
				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: CLOSE");
				loop = 0;

			//
			// SEND NEXT PORTION OF DATA
			//

			} else if (strstr(inputBuff, "next") != NULL) {

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: GET next");

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				switch (reqIndex){

					//meterages
					case 1:
						//get meterage by iterator
						res = ERROR_GENERAL;

						memset(&lastDtsDay, 0, sizeof(dateTimeStamp));
						memset(&lastDtsMonth, 0, sizeof(dateTimeStamp));
						memset(&lastDtsProfile, 0, sizeof(dateTimeStamp));

						if ((countersArray485.countersSize > 0) && (iterator485 < countersArray485.countersSize)){
							res = STORAGE_GetMeteragesForDb(STORAGE_CURRENT, countersArray485.counter[iterator485]->dbId, &mArray);

							STORAGE_GetLastMeterageDts(STORAGE_DAY, countersArray485.counter[iterator485]->dbId, &lastDtsDay);
							STORAGE_GetLastMeterageDts(STORAGE_MONTH, countersArray485.counter[iterator485]->dbId, &lastDtsMonth);
							STORAGE_GetLastProfileDts(countersArray485.counter[iterator485]->dbId, &lastDtsProfile);

						} else if((countersArrayPlc.countersSize > 0) && (iteratorPlc < countersArrayPlc.countersSize)){
							res = STORAGE_GetMeteragesForDb(STORAGE_CURRENT, countersArrayPlc.counter[iteratorPlc]->dbId, &mArray);

							STORAGE_GetLastMeterageDts(STORAGE_DAY, countersArrayPlc.counter[iteratorPlc]->dbId, &lastDtsDay);
							STORAGE_GetLastMeterageDts(STORAGE_MONTH, countersArrayPlc.counter[iteratorPlc]->dbId, &lastDtsMonth);
							STORAGE_GetLastProfileDts(countersArrayPlc.counter[iteratorPlc]->dbId, &lastDtsProfile);
						}

						COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: METERAGES / get next / res=%s iter485=%d iterRf=%d", getErrorCode(res),  iterator485, iteratorPlc);

						if (res == OK){
							int realRatio = 1;
							double scaler = (double)0;

							//create scaler by ration and delimeter
							switch (mArray->meterages[0]->ratio){
								case 0:
								case 1:
								case 2:
								case 3:
								case 4:
								case 5:
								case 6:
									realRatio = COMMON_GetRatioValueUjuk(mArray->meterages[0]->ratio);
									scaler = (double)(1)/(double)(2 * realRatio);
									break;

								case 0xFA:
								case 0xFB:
								case 0xFC:
								case 0xFD:
									realRatio = COMMON_GetRatioValueMayak(mArray->meterages[0]->ratio);
									scaler = (double)(realRatio)/(double)(1000*1000);
									break;

								case GUBANOV_METERAGES_RATIO_VALUE:
									scaler = (double)1/(double)1000;
									break;
							}


							if ((countersArray485.countersSize > 0) && (iterator485 < countersArray485.countersSize)){
								memset(outputBuff, 0, UD_OUTPUT_SIZE);
								sprintf(outputBuff, "#%lu||%s||" DTS_PATTERN "||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f@@%hhu@@" DTS_PATTERN "@@" DTS_PATTERN "@@" DTS_PATTERN ,
										countersArray485.counter[iterator485]->serial,
										"rs-485",
										DTS_GET_BY_VAL(mArray->meterages[0]->dts),

										//t1
										(double)((double)mArray->meterages[0]->t[0].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[3] * scaler),

										//t2
										(double)((double)mArray->meterages[0]->t[1].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[3] * scaler),

										//t3
										(double)((double)mArray->meterages[0]->t[2].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[3] * scaler),

										//t4
										(double)((double)mArray->meterages[0]->t[3].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[3] * scaler),

										//addon data
										countersArray485.counter[iterator485]->type,
										DTS_GET_BY_VAL(lastDtsDay),
										DTS_GET_BY_VAL(lastDtsMonth),
										DTS_GET_BY_VAL(lastDtsProfile)

									);

								COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "SEND TO WEB (485): %s\r\n", outputBuff);

								writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
								if (writeBytes< 0) loop = 0;
								iterator485++;

							} else if ((countersArrayPlc.countersSize > 0) && (iteratorPlc < countersArrayPlc.countersSize)){
								memset(outputBuff, 0, UD_OUTPUT_SIZE);
								sprintf(outputBuff, "#%lu||%s||" DTS_PATTERN "||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f||%.2f@@%hhu@@" DTS_PATTERN "@@" DTS_PATTERN "@@" DTS_PATTERN ,
										countersArrayPlc.counter[iteratorPlc]->serial,
										countersArrayPlc.counter[iteratorPlc]->serialRemote,
										DTS_GET_BY_VAL(mArray->meterages[0]->dts),

										//t1
										(double)((double)mArray->meterages[0]->t[0].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[0].e[3] * scaler),

										//t2
										(double)((double)mArray->meterages[0]->t[1].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[1].e[3] * scaler),

										//t3
										(double)((double)mArray->meterages[0]->t[2].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[2].e[3] * scaler),

										//t4
										(double)((double)mArray->meterages[0]->t[3].e[0] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[1] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[2] * scaler),
										(double)((double)mArray->meterages[0]->t[3].e[3] * scaler),

										//addon data
										countersArrayPlc.counter[iteratorPlc]->type,
										DTS_GET_BY_VAL(lastDtsDay),
										DTS_GET_BY_VAL(lastDtsMonth),
										DTS_GET_BY_VAL(lastDtsProfile)

									);

								COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "SEND TO WEB (PLC): %s\r\n", outputBuff);

								writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
								if (writeBytes< 0) loop = 0;
								iteratorPlc++;
							}

						} else {

							COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: METERAGES / get storage error");
							if ((countersArray485.countersSize > 0) && (iterator485 < countersArray485.countersSize)){
								memset(outputBuff, 0, UD_OUTPUT_SIZE);
								sprintf(outputBuff, "#%lu||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s@@%hhu@@" DTS_PATTERN "@@" DTS_PATTERN "@@" DTS_PATTERN ,
										countersArray485.counter[iterator485]->serial,
										"rs-485",
										"нет показаний",

										//t1
										"---", "---", "---", "---",
										//t2
										"---", "---", "---", "---",
										//t3
										"---", "---", "---", "---",
										//t4
										"---", "---", "---", "---",

										//addon data
										countersArray485.counter[iterator485]->type,
										DTS_GET_BY_VAL(lastDtsDay),
										DTS_GET_BY_VAL(lastDtsMonth),
										DTS_GET_BY_VAL(lastDtsProfile)
									);

								COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "SEND TO WEB (RS): %s\r\n", outputBuff);

								writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
								if (writeBytes< 0) loop = 0;
								iterator485++;

							} else if ((countersArrayPlc.countersSize > 0) && (iteratorPlc < countersArrayPlc.countersSize)){
								memset(outputBuff, 0, UD_OUTPUT_SIZE);
								sprintf(outputBuff, "#%lu||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s@@%hhu@@" DTS_PATTERN "@@" DTS_PATTERN "@@" DTS_PATTERN ,
										countersArrayPlc.counter[iteratorPlc]->serial,
										countersArrayPlc.counter[iteratorPlc]->serialRemote,
										"нет показаний",

										//t1
										"---", "---", "---", "---",
										//t2
										"---", "---", "---", "---",
										//t3
										"---", "---", "---", "---",
										//t4
										"---", "---", "---", "---",

										//addon data
										countersArrayPlc.counter[iteratorPlc]->type,
										DTS_GET_BY_VAL(lastDtsDay),
										DTS_GET_BY_VAL(lastDtsMonth),
										DTS_GET_BY_VAL(lastDtsProfile)

									);

								COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "SEND TO WEB (RF): %s\r\n", outputBuff);

								writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
								if (writeBytes< 0) loop = 0;
								iteratorPlc++;
							}



						}
						break;

					//statistic
					case 2:
						if (iteratorStatistic < counterStatisticCopyLength){
							memset(outputBuff, 0, UD_OUTPUT_SIZE);
							sprintf(outputBuff, "#%lu||%s||%02d-%02d-%02d %02d:%02d:%02d||%02d-%02d-%02d %02d:%02d:%02d||%d||%d||%d",
									counterStatisticCopy[iteratorStatistic].serial,
									counterStatisticCopy[iteratorStatistic].serialRemote,

									counterStatisticCopy[iteratorStatistic].poolRun.d.d,
									counterStatisticCopy[iteratorStatistic].poolRun.d.m,
									counterStatisticCopy[iteratorStatistic].poolRun.d.y,
									counterStatisticCopy[iteratorStatistic].poolRun.t.h,
									counterStatisticCopy[iteratorStatistic].poolRun.t.m,
									counterStatisticCopy[iteratorStatistic].poolRun.t.s,

									counterStatisticCopy[iteratorStatistic].poolEnd.d.d,
									counterStatisticCopy[iteratorStatistic].poolEnd.d.m,
									counterStatisticCopy[iteratorStatistic].poolEnd.d.y,
									counterStatisticCopy[iteratorStatistic].poolEnd.t.h,
									counterStatisticCopy[iteratorStatistic].poolEnd.t.m,
									counterStatisticCopy[iteratorStatistic].poolEnd.t.s,
									counterStatisticCopy[iteratorStatistic].poolStatus,
									counterStatisticCopy[iteratorStatistic].pollErrorTransactionIndex,
									counterStatisticCopy[iteratorStatistic].pollErrorTransactionResult
								);

							COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "SEND TO WEB (STAT): %s\r\n", outputBuff);

							writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
							iteratorStatistic++;
							if (writeBytes< 0) loop = 0;
						}
						break;

					case 3:
						break;


					//plc map
					case 4:{

						int remotesCount = PLC_INFO_TABLE_GetRemotesCount() ;

						if ((iteratorPlcMap < remotesCount ) && (remotesCount > 0)){

							node = (plcNode_t *)malloc (sizeof(plcNode_t));
							if (node != NULL){
								memset(node, 0, sizeof(plcNode_t));
								res = PLC_INFO_TABLE_GetRemoteNodeByIndex(iteratorPlcMap, node);

								COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: PLC MAP / iteratorPlcMap %d getPlcNode = %s", iteratorPlcMap, getErrorCode(res));

								if (res == OK) {
									memset(outputBuff, 0, UD_OUTPUT_SIZE);
									sprintf(outputBuff, "#%d||%d||%s||%d||%d",
											node->did,
											node->pid,
											node->serial,
											node->age,
											0);

									COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: RF MAP / output: %s", outputBuff);

									writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
									if (writeBytes< 0) loop = 0;
									iteratorPlcMap++;
								}

								free (node);
								node = NULL;
							}
						}
					}
						break;

					//general
					case 100:
						{
						COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: USPD STATUS");

						dateTimeStamp dts;
						char uspdFirmwareVersion[ STORAGE_VERSION_LENGTH ];
						memset(uspdFirmwareVersion , 0, STORAGE_VERSION_LENGTH);
						char uspdSerial[128];
						char uspdEthIp[16];
						char uspdEthMac[32];
						char uspdGsmImei[32];
						char uspdTime[32];
						char uspdSyncTimeLast[64];
						char uspdRs485State[128];
						char uspdPlcState[128];
						char uspdConnectionState[128];
						char uspdGsmSq[64];
						char uspdGsmNetName[64];
						int uspdGsmLags;
						int upTimeGsmCnt;
						char uspdGsmUpTime[64];
						int uspdEthLags;
						int upTimeEthCnt;
						char uspdEthUpTime[64];
						char uspdPlcNetworkId[16];
						char uspdPlcNetworkMode[64];
						char gsmIp[LINK_SIZE];
						connection_t connection;
						memset(&connection , 0 , sizeof(connection_t));
						int result;

						//get serial number
						char sn[32];
						memset(sn, 0, 32);
						COMMON_GET_USPD_SN((unsigned char *)&sn[0]);

						//get firmware version
						if ( STORAGE_GetFirmwareVersion( uspdFirmwareVersion ) != OK )
						{
							snprintf( uspdFirmwareVersion , STORAGE_VERSION_LENGTH , "неизвестно" );
						}

						//glue serial stroke
						memset(uspdSerial, 0, 128);
						snprintf(uspdSerial, 128, "%s / версия ПО: %s", sn, uspdFirmwareVersion );
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get sn %s", uspdSerial);

						//get eth ip and mac
						memset(uspdEthIp, 0, 16);
						memset(uspdEthMac, 0, 32);
						COMMON_GET_USPD_IP(&uspdEthIp[0]);
						COMMON_GET_USPD_MAC(&uspdEthMac[0]);

						//get gsm ip
						memset(gsmIp, 0 , LINK_SIZE);
						GSM_GetOwnIp(gsmIp);

						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get ip %s", uspdEthIp);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get mac %s", uspdEthMac);

						//get gsm imei
						memset(uspdGsmImei, 0, 32);
						result = GSM_GetIMEI(uspdGsmImei, 32);
						if(result != OK)
							sprintf (uspdGsmImei, "GSM модуль занят");
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get imei %s", uspdGsmImei);
						//get uspd time
						memset(uspdTime, 0, 32);
						COMMON_GetCurrentDts(&dts);
						sprintf (uspdTime, DTS_PATTERN, DTS_GET_BY_VAL(dts));
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get time %s", uspdTime);
						//get last sync time
						memset(uspdSyncTimeLast, 0, 64);
						unsigned char syncTimeCmdFrom = 0 ;
						if (STORAGE_GetLastSyncDts(&dts , &syncTimeCmdFrom ) == OK )
						{
							sprintf (uspdSyncTimeLast, DTS_PATTERN, DTS_GET_BY_VAL(dts));
							if ( syncTimeCmdFrom == DESC_EVENT_TIME_SYNCED_BY_UI )
							{
								strcat( uspdSyncTimeLast , " / через WEB" );
							}
							else if ( syncTimeCmdFrom == DESC_EVENT_TIME_SYNCED_BY_PROTO )
							{
								strcat( uspdSyncTimeLast , " / через ПОВУ" );
							}
							else if ( syncTimeCmdFrom == DESC_EVENT_TIME_SYNCED_BY_SEASON )
							{								
								strcat( uspdSyncTimeLast , " / сезонный перевод" );
							}
						}
						else
						{
							sprintf (uspdSyncTimeLast, "неизвестно" );
						}

						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: get las sync time %s", uspdSyncTimeLast);

						//get rs 485 state
						memset(uspdRs485State, 0, 128);
						POOL_GetStatusOf485(&uspdRs485State[0]);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: 485 state %s", uspdRs485State);
						//get rf state
						memset(uspdPlcState, 0, 128);
						POOL_GetStatusOfPlc(&uspdPlcState[0]);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: RF state %s", uspdRfState);
						//get connection state
						memset(uspdConnectionState, 0, 128);
						if(STORAGE_GetConnection(&connection) == OK){
							switch(connection.mode){
								case CONNECTION_GPRS_ALWAYS:
								case CONNECTION_GPRS_CALENDAR:
								case CONNECTION_GPRS_SERVER:
									if(GSM_ConnectionState() == 0)
										sprintf(uspdConnectionState, "подключен по GPRS");
									else
										sprintf(uspdConnectionState, "ожидание подключения по GPRS");
									break;

								case CONNECTION_CSD_OUTGOING:
								case CONNECTION_CSD_INCOMING:
									if(GSM_ConnectionState() == 0)
										sprintf(uspdConnectionState, "подключен по CSD");
									else
										sprintf(uspdConnectionState, "ожидание подключения по CSD");
									break;

								case CONNECTION_ETH_ALWAYS:
								case CONNECTION_ETH_CALENDAR:
								case CONNECTION_ETH_SERVER:
									if ( ETH_ConnectionState(&connection) == OK )
									{
										sprintf(uspdConnectionState, "подключен по ETH");
									}
									else
									{
										sprintf(uspdConnectionState, "ожидание подключения по ETH");
									}
									break;

								default:
									sprintf(uspdConnectionState, "неопределенное состояние");
									break;
							}
						} else {
							sprintf(uspdConnectionState, "конфигурация не определена");
						}
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: CONN CONF %s", uspdConnectionState);
						//get gsm name
						memset(uspdGsmNetName, 0, 64);
						result = GSM_GetGsmOperatorName(uspdGsmNetName, 64);
						if (result!=OK) {
							sprintf(uspdGsmNetName, "не зарегистрирован");
							sprintf(uspdGsmSq, "не зарегистрирован");
						} else {
							//get gsm sq
							memset(uspdGsmSq, 0, 64);
							int quality = GSM_GetSignalQuality();
							if (quality <= 0)
								sprintf(uspdGsmSq, "GSM модуль занят");
							else
							{
								if ( GSM_GetCurrentSimNumber() == SIM1 )
								{
									sprintf(uspdGsmSq, "sim1 / %d%%", quality);
								}
								else
								{
									sprintf(uspdGsmSq, "sim2 / %d%%", quality);
								}


							}

						}
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: GSM net name %s", uspdGsmNetName);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: GSM sq %s", uspdGsmSq);

						//get gsm lags
						uspdGsmLags = GSM_GetConnectionsCounter();
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: GSM lags %u", uspdGsmLags);

						//get gsm uptime
						upTimeGsmCnt = GSM_GetCurrentConnectionTime();
						memset(uspdGsmUpTime, 0, 64);
						sprintf (uspdGsmUpTime, "%u сек", upTimeGsmCnt);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: GSM UPtime %s", uspdGsmUpTime);

						//get eth lags
						uspdEthLags = ETH_GetConnectionsCounter();
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: ETH lags %u", uspdEthLags);

						//get eth uptime
						upTimeEthCnt = ETH_GetCurrentConnectionTime();
						memset(uspdEthUpTime, 0, 64);
						sprintf (uspdEthUpTime, "%u сек", upTimeEthCnt);
						//COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STATUS: ETH UPtime %s", uspdEthUpTime);

						//get plc network Id
						memset(uspdPlcNetworkId, 0, 16);
						if (PLC_GetNetowrkId() == 0){
							sprintf (uspdPlcNetworkId, "---");
						} else {
							sprintf (uspdPlcNetworkId, "%u", PLC_GetNetowrkId());
						}

						//get plc mode
						memset(uspdPlcNetworkMode, 0, 64);
						sprintf (uspdPlcNetworkMode, "%s", getPlcModeDesc());

						//glue
						memset(outputBuff, 0, UD_OUTPUT_SIZE);
						sprintf(outputBuff, "#%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%s||%u||%s||%u||%s||%s||%s",
									uspdSerial,
									uspdEthIp,
									uspdEthMac,
									uspdGsmImei,
									uspdTime,
									uspdSyncTimeLast,
									uspdRs485State,
									uspdPlcState,
									uspdConnectionState,
									uspdGsmSq,
									uspdGsmNetName,
									gsmIp,
									uspdGsmLags,
									uspdGsmUpTime,
									uspdEthLags,
									uspdEthUpTime, uspdPlcNetworkId, uspdPlcNetworkMode);

						COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client send %s", outputBuff);

						writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
						if (writeBytes< 0) loop = 0;
						}
						break;
				}

			//
			// CHECK REQUESTED DATA TO PREPEARE
			//

			} else if (strstr(inputBuff, "getStation") != NULL){

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP ask USPD STATUS");

				reqIndex = 100;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				sprintf(outputBuff, "1");
				writeBytes = write(sockFd, outputBuff, strlen(outputBuff));
				if (writeBytes < 0) loop = 0;

			}else if (strstr(inputBuff, "getMetrages") != NULL){

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP ask METERAGES");

				reqIndex = 1;
				iterator485 = 0;
				iteratorPlc = 0;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);
				sprintf(outputBuff, "%d", countersArray485.countersSize + countersArrayPlc.countersSize);
				writeBytes = write(sockFd, outputBuff, strlen(outputBuff));
				if (writeBytes < 0) loop = 0;

			} else if (strstr(inputBuff, "getStatistic") != NULL) {

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP ask STATISTIC");

				reqIndex = 2;
				iteratorStatistic = 0;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				//get statistic struct copy
				STORAGE_GetCounterStatistic(&counterStatisticCopy, &counterStatisticCopyLength);

				sprintf(outputBuff, "%d", counterStatisticCopyLength);
				writeBytes = write(sockFd, outputBuff, strlen(outputBuff));
				if (writeBytes < 0) loop = 0;

			} else if (strstr(inputBuff, "getPlcMap") != NULL) {

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP ask PLC MAP");

				reqIndex = 4;
				iteratorPlcMap = 0;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				sprintf(outputBuff, "%d", PLC_INFO_TABLE_GetRemotesCount());

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client send [%s]", outputBuff);

				writeBytes = write(sockFd, outputBuff, strlen(outputBuff));
				if (writeBytes < 0) loop = 0;

			} else if (strstr(inputBuff, "testPlcModem") != NULL) {

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP ask TEST RF MODEM");

				reqIndex = 5;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);
				sprintf(outputBuff, "1");
				writeBytes = write(sockFd, outputBuff, strlen(outputBuff));
				if (writeBytes < 0) loop = 0;

			} else if (strstr(inputBuff, "testPlcStop") != NULL) {

				reqIndex = 7;

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				//start poller
				break;

			} else if (strstr(inputBuff, "updateRs") != NULL) {

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				//stop rs
				//renwe config
				//renwe status
				break;

			} else if (strstr(inputBuff, "updateRf") != NULL) {

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				//stop rf
				//renwe config
				//renwe status
				break;

			} else if (strstr(inputBuff, "updateConnections") != NULL) {

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

				//close connections
				//renwe config of connections
				break;

			} else if (strstr(inputBuff, "event||") != NULL) {

				COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: HTTP raise EVENT");

				char * ptr = strstr(inputBuff, "event||");
				if (ptr != NULL){
					eventSize=strlen(ptr+7)-2;

					COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "EVENT SIZE %d", eventSize);
					COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "EVENT DATA %s", ptr+7);
					memset(outputBuff, 0, UD_OUTPUT_SIZE);

					char * syncTimePtr = NULL;

					if (eventSize > 0){

						//parse event
						if (strstr(ptr, "anystring")!=NULL){

							//do something

						//erase remotes table
						} else if ( strstr(ptr, "rferasetable") !=NULL){

							//
							// no additional parsing
							// ptr = 'rferasetable||no'
							//

							if (IT700_EraseTable() == OK)
							{
								//need to restart plc coordinator
								STORAGE_UPDATE_PLC_NET();

								sprintf(outputBuff,"eventAnswer||1"); //done ok
							} else {
								sprintf(outputBuff,"eventAnswer||-2"); //error occured
							}

						//erase on node
						}else if ( strstr(ptr, "rfleavenetwork") !=NULL)
						{
							// leaving network
							if ( IT700_LeaveNetwork() == OK )
							{
								sprintf( outputBuff , "eventAnswer||1");
							}
							else
							{
								sprintf( outputBuff , "eventAnswer||-1");
							}
						}

						else if ( strstr(ptr, "rferasenode") !=NULL){

							//
							// parse node id to be deleted
							// ptr = 'rferasenode||node_id'
							//
							unsigned int nodeId = (unsigned int)atoi(ptr + 7 + strlen("rferasenode||"));
							if (nodeId != 0){
								if (IT700_DeleteNode(nodeId) == OK){
									sprintf(outputBuff,"eventAnswer||1"); //done ok
								} else {
									sprintf(outputBuff,"eventAnswer||-2"); //error occured
								}
							} else {
								sprintf(outputBuff,"eventAnswer||-2"); //error occured
							}

						//sync time
						} else if ( (syncTimePtr = strstr(ptr, "synctime")) !=NULL){

							//
							// parse time passed, sync, return answer
							// ptr = 'synctime||DTS in format hhmmssddmmyyyy as str'
							//
							syncTimePtr+=strlen("synctime||");
							dateTimeStamp dtsToSync;
							memset(&dtsToSync, 0, sizeof(dateTimeStamp));
							if ( COMMON_GetDtsFromUIString(syncTimePtr, &dtsToSync) == OK ){
								int diff = 0;
								if (COMMON_SetNewTimeStep1(&dtsToSync, &diff) == OK){
									COMMON_SetNewTimeStep2(DESC_EVENT_TIME_SYNCED_BY_UI);

									sprintf(outputBuff,"eventAnswer||1"); //done ok
								}
								else{
									sprintf(outputBuff,"eventAnswer||-2"); //error occured
								}
							}
							else {
								sprintf(outputBuff,"eventAnswer||-2"); //error occured
							}


						//all other events traited as unknown
						}
						else if( strstr( ptr , "loginWeb" ) )
						{
							if ( strstr( ptr , "ok" ) )
							{
								STORAGE_JournalUspdEvent( EVENT_WEB_UI_LOGIN , (unsigned char *)DESC_EVENT_LOGIN_SUCCESS );
							}
							else
							{
								STORAGE_JournalUspdEvent( EVENT_WEB_UI_LOGIN , (unsigned char *)DESC_EVENT_LOGIN_FAILURE );
							}
						}						
						else if ( strstr( ptr , "plcNetConf" ) )
						{
							int firstValue = 0 ;
							int secValue = 0 ;
							if ( sscanf( ptr , "event||plcNetConf||%d||%d" , &firstValue , &secValue ) == 2 )
							{
								unsigned char desc[EVENT_DESC_SIZE] = { 0 };
								desc[0] = 0x02;
								desc[1] = (firstValue >> 24 ) & 0xFF ;
								desc[2] = (firstValue >> 16 ) & 0xFF ;
								desc[3] = (firstValue >> 8 ) & 0xFF ;
								desc[4] = firstValue  & 0xFF ;

								desc[5] = (secValue >> 24 ) & 0xFF ;
								desc[6] = (secValue >> 16 ) & 0xFF ;
								desc[7] = (secValue >> 8 ) & 0xFF ;
								desc[8] = secValue  & 0xFF ;
								STORAGE_JournalUspdEvent( EVENT_ERROR_CONFIG_PlC_COORD , desc );
							}
						}
						else {
							sprintf(outputBuff,"eventAnswer||-3"); //unknown event
						}


					//error parse event command from web
					} else {
						sprintf(outputBuff,"eventAnswer||-4"); //bad event sintacsis
					}

					COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client send [%s]", outputBuff);

					//writeBytes = write(sockFd, outputBuff, strlen(outputBuff)+1);
					writeBytes = send(sockFd, outputBuff, strlen(outputBuff)+1 , MSG_NOSIGNAL);
					if (writeBytes< 0) loop = 0;
				}

				memset(inputBuff, 0, UD_INPUT_SIZE);
				memset(outputBuff, 0, UD_OUTPUT_SIZE);

			} else {
				loop = 0;
			}
		}

		//fix non reading times
		//
		//TO DO - if no reads during some time -> exit loop, release thread
		//

		COMMON_Sleep(THREAD_SLEEPS_DELAY);
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_WEB "STORAGE UD client: quit, loop ends");
	close (sockFd);

	free(outputBuff);
	free(inputBuff);
	COMMON_FreeMeteragesArray(&mArray);

	if (counterStatisticCopy != NULL){
		free (counterStatisticCopy);
		counterStatisticCopy = NULL;
		counterStatisticCopyLength = 0;
	}

	return NULL ;
}

//
//
//

void STORAGE_SetSyncFlagForCounters(int globalDiff)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	STORAGE_Protect();

	int counterIndex = 0;

	//update RF counter array

	pthread_mutex_lock(&counterSyncFlagMutex);

	#if REV_COMPILE_PLC
		for( counterIndex = 0; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
		{
			if ( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_WORT_ACQ )
			{
				if ( !( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_PROCESS ) )
				{
					countersArrayPlc.counter[ counterIndex ]->syncWorth += globalDiff;
					if ( abs( countersArrayPlc.counter[ counterIndex ]->syncWorth ) > 1 )
					{
						STORAGE_SwitchSyncFlagTo_NeedToSync( &countersArrayPlc.counter[ counterIndex ] );
					}
				}
				else
				{
					if ( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
						countersArrayPlc.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
					if ( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
						countersArrayPlc.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
				}
			}
			else
			{
				if ( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
					countersArrayPlc.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
				if ( countersArrayPlc.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
					countersArrayPlc.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
			}


		}
	#endif

	#if REV_COMPILE_485
		//update 485 counters array
		for( counterIndex = 0; counterIndex < countersArray485.countersSize ; ++counterIndex )
		{
			if ( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_WORT_ACQ )
			{
				if ( !( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_PROCESS ) )
				{
					countersArray485.counter[ counterIndex ]->syncWorth += globalDiff;
					if ( abs (countersArray485.counter[ counterIndex ]->syncWorth) > 1 )
					{
						STORAGE_SwitchSyncFlagTo_NeedToSync( &countersArray485.counter[ counterIndex ] );
					}
				}
				else
				{
					if ( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
						countersArray485.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
					if ( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
						countersArray485.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
				}
			}
			else
			{
				if ( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
					countersArray485.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
				if ( countersArray485.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
					countersArray485.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
			}
		}
	#endif


	#if REV_COMPILE_RF
		for( counterIndex = 0; counterIndex < countersArrayRf.countersSize ; ++counterIndex )
		{
			if ( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_WORT_ACQ )
			{
				if ( !( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_PROCESS ) )
				{
					countersArrayRf.counter[ counterIndex ]->syncWorth += globalDiff;
					if ( abs (countersArrayRf.counter[ counterIndex ]->syncWorth) > 1 )
					{
						STORAGE_SwitchSyncFlagTo_NeedToSync( &countersArrayRf.counter[ counterIndex ] );
					}
				}
				else
				{
					if ( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
						countersArrayRf.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
					if ( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
						countersArrayRf.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
				}
			}
			else
			{
				if ( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_DONE )
					countersArrayRf.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_DONE;
				if ( countersArrayRf.counter[ counterIndex ]->syncFlag & STORAGE_SYNC_FAIL )
					countersArrayRf.counter[ counterIndex ]->syncFlag -= STORAGE_SYNC_FAIL;
			}
		}
	#endif

	pthread_mutex_unlock(&counterSyncFlagMutex);

	STORAGE_Unprotect();

	return ;
}

//
//
//

BOOL STORAGE_CheckCounterDbIdPresenceInConfiguration( unsigned long counterDbId )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	BOOL res = FALSE ;

	STORAGE_CountersProtect();

	int counterIndex = 0 ;

#if REV_COMPILE_PLC
	for( counterIndex = 0; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
	{
		if (countersArrayPlc.counter[ counterIndex ]->dbId == counterDbId)
		{
			STORAGE_CountersUnprotect();
			return TRUE ;
		}
	}
#endif

#if REV_COMPILE_485
	for( counterIndex = 0; counterIndex < countersArray485.countersSize ; ++counterIndex )
	{
		if (countersArray485.counter[ counterIndex ]->dbId == counterDbId)
		{
			STORAGE_CountersUnprotect();
			return TRUE ;
		}
	}
#endif

	//
	// TODO for RF
	//
	STORAGE_CountersUnprotect();

	return res ;
}

//
//
//

int STORAGE_GetSyncStateWord( syncStateWord_t ** syncStateWord , int * syncStateWordSize)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	STORAGE_CountersProtect();

	(*syncStateWordSize) = countersArray485.countersSize + countersArrayPlc.countersSize ;
	(*syncStateWord) = (syncStateWord_t *)malloc( (*syncStateWordSize) * sizeof( syncStateWord_t ) );

	if ( (*syncStateWord) )
	{
		int counterIndex = 0 ;
		int syncStateWordIndex = 0 ;

		res = OK ;

		// for RS counters
		for( counterIndex = 0 ; counterIndex < countersArray485.countersSize ; ++counterIndex )
		{
			(*syncStateWord)[ syncStateWordIndex ].counterDbId = countersArray485.counter[ counterIndex ]->dbId ;
			(*syncStateWord)[ syncStateWordIndex ].transactionTime = countersArray485.counter[ counterIndex ]->transactionTime ;

			pthread_mutex_lock( &counterSyncFlagMutex );
			(*syncStateWord)[ syncStateWordIndex ].status = countersArray485.counter[ counterIndex ]->syncFlag ;
			pthread_mutex_unlock( &counterSyncFlagMutex );

			(*syncStateWord)[ syncStateWordIndex++ ].worth = countersArray485.counter[ counterIndex ]->syncWorth ;
		}


		// for PLC counters
		for( counterIndex = 0 ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
		{
			(*syncStateWord)[ syncStateWordIndex ].counterDbId = countersArrayPlc.counter[ counterIndex ]->dbId ;
			(*syncStateWord)[ syncStateWordIndex ].transactionTime = countersArrayPlc.counter[ counterIndex ]->transactionTime ;

			pthread_mutex_lock( &counterSyncFlagMutex );
			(*syncStateWord)[ syncStateWordIndex ].status = countersArrayPlc.counter[ counterIndex ]->syncFlag ;
			pthread_mutex_unlock( &counterSyncFlagMutex );

			(*syncStateWord)[ syncStateWordIndex++ ].worth = countersArrayPlc.counter[ counterIndex ]->syncWorth ;
		}

		// TO DO FOR RF
		//
		//
	}


	STORAGE_CountersUnprotect();

	return res ;
}

//
//
//

void STORAGE_SetTransactionTime(unsigned long counterDbId, double transactionTime){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL;
	counter_t * counter = NULL;

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if ((res == OK) && (counter != NULL)){
		counter->transactionTime = (int)transactionTime;
	}

	STORAGE_CountersUnprotect();
}

//
//
//

int STORAGE_GetConnection(connection_t * connection) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL;
	if ( !connection )
		return res ;

	STORAGE_Protect();

	//*connection = &connectionParams;
	res = OK ;
	memcpy(connection , &connectionParams , sizeof(connection_t));

	STORAGE_Unprotect();

	return res;
}

//
//
//

int STORAGE_GetMaxPossibleTaskTimeFor485(){
	DEB_TRACE(DEB_IDX_STORAGE)

	int maxPossibleTime = POOL_MAX_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 ;

	int nodesNumber = countersArray485.countersSize;
	if ( nodesNumber > 0 )
	{
		maxPossibleTime = (12 * 60) / nodesNumber ;
		if (  maxPossibleTime > POOL_MAX_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 )
		{
			maxPossibleTime = POOL_MAX_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 ;
		}
		else if (  maxPossibleTime < POOL_MIN_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 )
		{
			maxPossibleTime = POOL_MIN_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 ;
		}
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_GetMaxPossibleTaskTimeForPlc() return maxPossibleTime = %d minutes", maxPossibleTime);

	return maxPossibleTime;
}

//
//
//
int STORAGE_GetMaxPossibleTaskTimeForPLC(){
	int maxTime = 0;

	if (countersArrayPlc.countersSize > 0){
		maxTime = ((24 * 60 * 60) / countersArrayPlc.countersSize / MAX_POOLS_PER_DAY);

		if (maxTime > MAX_PLC_POLL_TIME_FOR_SINGLR_COUNTER )
			maxTime = MAX_PLC_POLL_TIME_FOR_SINGLR_COUNTER;
	}

	return maxTime;
}

//
//
//

BOOL STORAGE_CheckPollingCounterAgainNoAtom(counter_t * counter, int maxPossibleTime){
	DEB_TRACE(DEB_IDX_STORAGE)

	BOOL res = FALSE;

	if (counter->lastTaskRepeateFlag == 1)
	{
		dateTimeStamp currentDts ;
		COMMON_GetCurrentDts( &currentDts );

		int timeDiff = COMMON_GetDtsDiff__Alt( &counter->lastCounterTaskRequest , &currentDts , BY_SEC ) ;
		if(( counter->pollAlternativeAlgo != POOL_ALTER_NOT_SET ) || 
			( counter->profileReadingState == STORAGE_PROFILE_STATE_WAITING_SEARCH_RES ) || 
			 ( (timeDiff >= 0) && (timeDiff < maxPossibleTime) ) )
		{
			res = TRUE;
		}		
	}

	return res;
}


//
//POOL QUE
//

int currentIndexFromQue = -1;
int queOffset = 0;
int plcQue[MAX_DEVICE_COUNT_TOTAL];
double countersWeight[MAX_DEVICE_COUNT_TOTAL];


int STORAGE_PlcQue_ReInitialize(){
	int res = ERROR_NO_COUNTERS;

	queOffset = 0;
	currentIndexFromQue = -1; 
	
	if (countersArrayPlc.countersSize != 0) {

		int i = 0;
		int j = 0;
		int k = 0;
		double maxWeight = POLL_WEIGHT_UNKNOWN;

		//reset weight
		for ( i = 0 ; i < MAX_DEVICE_COUNT_TOTAL ; ++i ){
			countersWeight[i] = POLL_WEIGHT_UNKNOWN;
		}

		//calc weight
		for ( i = 0 ; i < countersArrayPlc.countersSize ; ++i ){
			STORAGE_CalcEffectivePollWeight(countersArrayPlc.counter[i]->dbId, &countersWeight[ i ]);
		}

		for ( j = 0 ; j < countersArrayPlc.countersSize ; ++j)
		{
			k = -1 ;
			maxWeight = (double)0.0;
			for ( i = 0 ; i < countersArrayPlc.countersSize; ++i)
			{
				if ( (k == -1) && ( countersWeight[i] > POLL_WEIGHT_MIN ) )
				{
					k = i ;
				}
				if ( (countersWeight[i] > maxWeight) && ( countersWeight[i] > POLL_WEIGHT_MIN )  )
				{
					maxWeight = countersWeight[i] ;
					k = i ;
				}
			}

			if ( k < 0) //?????????
				k = 0 ;

			plcQue[j] = k ;
			countersWeight[k] = POLL_WEIGHT_UNKNOWN;
			
		}

		//calc pool round time
		time_t tNow;
		time(&tNow);
		int tDiff = tNow - lastPoolReinit;
		time(&lastPoolReinit);
		
		COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC, QUE REINIT [%d], prev round " HMS_PATTERN, pollQueReinitCount, HMS_FROM_S(tDiff));

		pollQueReinitCount++;
		currentIndexFromQue = plcQue[0];
		res = OK;
	}
	
	return res;
}
	
int STORAGE_PlcQue_NextCounter(){
	int res = ERROR_NO_COUNTERS;

	if (countersArrayPlc.countersSize != 0) {
		if ((queOffset + 1) < countersArrayPlc.countersSize){
			queOffset++;
			currentIndexFromQue = plcQue[queOffset];
			res = OK;
			
		} else {
			res = STORAGE_PlcQue_ReInitialize();
		}
	}

	return res;
}

int STORAGE_GetNextCounterPlc(counter_t ** counter){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_NO_COUNTERS;
	int prevCounterIndex = 0;
	BOOL again = FALSE;
	
	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_GetNextCounterPlc() enter");

	STORAGE_CountersProtect();

	if (countersArrayPlc.countersSize != 0) {

		if (currentIndexFromQue != -1){
			prevCounterIndex = currentIndexFromQue;
			again = STORAGE_CheckPollingCounterAgainNoAtom(countersArrayPlc.counter[prevCounterIndex], (STORAGE_GetMaxPossibleTaskTimeForPLC() + appendTimeout));

			if( again == TRUE ){
				//return the same counter
				res = OK;
			}
			else
			{
				//reset repeat flag on previous counter if was set
				if (countersArrayPlc.counter[prevCounterIndex]->lastTaskRepeateFlag == 1){
					countersArrayPlc.counter[prevCounterIndex]->lastTaskRepeateFlag = 0;
				}

				//and step to next counter
				res = STORAGE_PlcQue_NextCounter();
			}	
		}
		else
		{
			res = STORAGE_PlcQue_ReInitialize();
		}

		if (res == OK){
			countersArrayPlc.currentCounterIndex = currentIndexFromQue;
			*counter = countersArrayPlc.counter[countersArrayPlc.currentCounterIndex];
			COMMON_GetCurrentDts( &countersArrayPlc.counter[countersArrayPlc.currentCounterIndex]->lastCounterTaskRequest );
		}
		
	}

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL QUE %d / %d counter index [%d]", queOffset, countersArrayPlc.countersSize, currentIndexFromQue);

	STORAGE_CountersUnprotect();


	if ((*counter) != NULL){

		COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_GetNextCounterPlc( returned counter SN %lu, dbid %lu) exit", (*counter)->serial, (*counter)->dbId);

	} else {
		COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "No counters on PLC");
	}

	return res;
}







//
//
//

int STORAGE_GetNextCounter485(counter_t ** counter) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_NO_COUNTERS;
	int prevCounterIndex = 0;

	COMMON_STR_DEBUG(DEBUG_STORAGE_485 "STORAGE_GetNextCounter485() enter");

	STORAGE_CountersProtect();

	if (countersArray485.countersSize != 0) {

		if (countersArray485.currentCounterIndex == 0){
			prevCounterIndex = countersArray485.countersSize - 1;
		} else {
			prevCounterIndex = countersArray485.currentCounterIndex - 1;
		}

		//check we can return the same counter again
		BOOL again = STORAGE_CheckPollingCounterAgainNoAtom(countersArray485.counter[prevCounterIndex], STORAGE_GetMaxPossibleTaskTimeFor485());
		if ( again == TRUE )
		{
			//if we can - return the same
			*counter = countersArray485.counter[prevCounterIndex];
			countersArray485.currentCounterIndex = prevCounterIndex;
		}
		else
		{
			//if not, reset repeatflag if was set
			if (countersArray485.counter[prevCounterIndex]->lastTaskRepeateFlag == 1){
				countersArray485.counter[prevCounterIndex]->lastTaskRepeateFlag = 0;
			}

			//and return next counter
			*counter = countersArray485.counter[countersArray485.currentCounterIndex];
			COMMON_GetCurrentDts( &countersArray485.counter[countersArray485.currentCounterIndex]->lastCounterTaskRequest );
		}

		res = OK;

		if ((countersArray485.currentCounterIndex + 1) < countersArray485.countersSize)
			countersArray485.currentCounterIndex++;
		else
			countersArray485.currentCounterIndex = 0;

	}

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_485 "STORAGE_GetNextCounter485() exit");

	return res;
}

//
//
//

int STORAGE_CountOfCountersPlc() {
	DEB_TRACE(DEB_IDX_STORAGE)
		
	int retVal  = 0;

	STORAGE_CountersProtect();
	retVal = countersArrayPlc.countersSize;
	STORAGE_CountersUnprotect();

	return retVal;
}

//
//
//

int STORAGE_CountOfCounters485() {
	DEB_TRACE(DEB_IDX_STORAGE)

	int retVal  = 0;

	STORAGE_CountersProtect();
	retVal = countersArray485.countersSize;
	STORAGE_CountersUnprotect();

	return retVal;
}


//
//
//

int STORAGE_GetQuizStatistic(quizStatistic_t ** quizStatistic , int * quizStatisticLength)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	if( !quizStatistic || !quizStatisticLength )
		return res;

	(*quizStatisticLength) = 0 ;

	int quizStatisticPos = 0 ;
	int cIndex = 0;

	STORAGE_CountersProtect();

	(*quizStatisticLength) = countersArray485.countersSize + countersArrayPlc.countersSize;
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetQuizStatistic stuff array for config. new array length is [%d]", (*quizStatisticLength));

	if ((*quizStatisticLength) > 0){

		(*quizStatistic) = (quizStatistic_t *)realloc( (*quizStatistic) , (*quizStatisticLength) * sizeof( quizStatistic_t ) );
		if ( !(*quizStatistic) )
		{
			fprintf( stderr , "ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}

		cIndex = 0;
		for (;cIndex < countersArray485.countersSize; cIndex++){
			(*quizStatistic)[ quizStatisticPos ].counterDbId = countersArray485.counter[ cIndex ]-> dbId ;
			(*quizStatistic)[ quizStatisticPos ].counterSerial = countersArray485.counter[ cIndex ]->serial ;
			(*quizStatistic)[ quizStatisticPos ].counterType = countersArray485.counter[ cIndex ]->type ;
			quizStatisticPos++;
		}

		cIndex = 0;
		for (;cIndex < countersArrayPlc.countersSize; cIndex++){
			(*quizStatistic)[ quizStatisticPos ].counterDbId = countersArrayPlc.counter[ cIndex ]-> dbId ;
			(*quizStatistic)[ quizStatisticPos ].counterSerial = countersArrayPlc.counter[ cIndex ]->serial ;
			(*quizStatistic)[ quizStatisticPos ].counterType = countersArrayPlc.counter[ cIndex ]->type ;
			quizStatisticPos++;
		}
	}

	STORAGE_CountersUnprotect();



	cIndex = 0;
	for (;cIndex < (*quizStatisticLength); cIndex++){

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetQuizStatistic copy statisctic for index [%d]", cIndex);
		dateTimeStamp tempDts;

		memset( &tempDts , (unsigned char)0xFF , sizeof( dateTimeStamp ) );
		STORAGE_GetLastMeterageDts( STORAGE_CURRENT , (*quizStatistic)[ cIndex ].counterDbId , &tempDts);
		memcpy( &(*quizStatistic)[ cIndex ].lastCurrentDts , &tempDts , sizeof( dateTimeStamp ) );

		memset( &tempDts , (unsigned char)0xFF , sizeof( dateTimeStamp ) );
		STORAGE_GetLastMeterageDts( STORAGE_DAY , (*quizStatistic)[ cIndex ].counterDbId , &tempDts);
		memcpy( &(*quizStatistic)[ cIndex ].lastDayDts , &tempDts , sizeof( dateTimeStamp ) );

		memset( &tempDts , (unsigned char)0xFF , sizeof( dateTimeStamp ) );
		STORAGE_GetLastMeterageDts( STORAGE_MONTH , (*quizStatistic)[ cIndex ].counterDbId , &tempDts) ;
		memcpy( &(*quizStatistic)[ cIndex ].lastMonthDts , &tempDts , sizeof( dateTimeStamp ) );

		memset( &tempDts , (unsigned char)0xFF , sizeof( dateTimeStamp ) );
		STORAGE_GetLastProfileDts( (*quizStatistic)[ cIndex ].counterDbId , &tempDts);
		memcpy( &(*quizStatistic)[ cIndex ].lastProfileDts , &tempDts , sizeof( dateTimeStamp ) );
	}


	return res = 0 ;
}

//
//
//

int STORAGE_GetCounterAutodetectionStatistic(counterAutodetectionStatistic_t ** counterAutodetectionStatistic , int * counterAutodetectionStatisticLength)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	if( !counterAutodetectionStatistic || !counterAutodetectionStatisticLength ){
		return res;
	}

	(*counterAutodetectionStatisticLength) = 0 ;
	int counterIndex = 0 ;
	int counterAutodetectionStatisticPos = 0 ;

	STORAGE_CountersProtect();

	#if REV_COMPILE_485
	//search how many counters are with autodetection flag
	int counters485NumberWithAutoDetection = 0 ;
	if ( countersArray485.countersSize > 0 )
	{
		for( counterIndex = 0 ; counterIndex < countersArray485.countersSize ; ++counterIndex )
		{
			if ( countersArray485.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				counters485NumberWithAutoDetection++ ;
			}
		}
	}
	//fill the array
	if ( counters485NumberWithAutoDetection > 0 )
	{
		(*counterAutodetectionStatisticLength) += counters485NumberWithAutoDetection ;
		(*counterAutodetectionStatistic) = ( counterAutodetectionStatistic_t * )realloc( (*counterAutodetectionStatistic) , (*counterAutodetectionStatisticLength)* sizeof(counterAutodetectionStatistic_t) );
		if ( !(*counterAutodetectionStatistic) )
		{
			STORAGE_CountersUnprotect();
			
			fprintf( stderr , "ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}
		for( counterIndex = 0 ; counterIndex < countersArray485.countersSize ; ++counterIndex )
		{
			if ( countersArray485.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].dbId = countersArray485.counter[ counterIndex ]->dbId ;
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].type = countersArray485.counter[ counterIndex ]->type ;
				counterAutodetectionStatisticPos++;
			}
		}
	}
	#endif

	#if REV_COMPILE_PLC
	//search how many counters are with autodetection flag
	int countersPlcNumberWithAutoDetection = 0 ;
	if ( countersArrayPlc.countersSize > 0 )
	{
		for( counterIndex = 0 ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
		{
			if ( countersArrayPlc.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				countersPlcNumberWithAutoDetection++ ;
			}
		}
	}
	//fill the array
	if ( countersPlcNumberWithAutoDetection > 0 )
	{
		(*counterAutodetectionStatisticLength) += countersPlcNumberWithAutoDetection ;
		(*counterAutodetectionStatistic) = ( counterAutodetectionStatistic_t * )realloc( (*counterAutodetectionStatistic) , (*counterAutodetectionStatisticLength) * sizeof(counterAutodetectionStatistic_t) );
		if ( !(*counterAutodetectionStatistic) )
		{
			STORAGE_CountersUnprotect();
			
			fprintf( stderr , "ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}
		for( counterIndex = 0 ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
		{
			if ( countersArrayPlc.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].dbId = countersArrayPlc.counter[ counterIndex ]->dbId ;
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].type = countersArrayPlc.counter[ counterIndex ]->type ;
				counterAutodetectionStatisticPos++;

			}
		}
	}
	#endif

	#if REV_COMPILE_RF
	//search how many counters are with autodetection flag
	int countersRfNumberWithAutoDetection = 0 ;
	if ( countersArrayRf.countersSize > 0 )
	{
		for( counterIndex = 0 ; counterIndex < countersArrayRf.countersSize ; ++counterIndex )
		{
			if ( countersArrayRf.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				countersRfNumberWithAutoDetection++ ;
			}
		}
	}
	//fill the array
	if ( countersRfNumberWithAutoDetection > 0 )
	{
		(*counterAutodetectionStatisticLength) += countersRfNumberWithAutoDetection ;
		(*counterAutodetectionStatistic) = ( counterAutodetectionStatistic_t * )realloc( (*counterAutodetectionStatistic) , (*counterAutodetectionStatisticLength)* sizeof(counterAutodetectionStatistic_t) );
		if ( !(*counterAutodetectionStatistic) )
		{
			STORAGE_CountersUnprotect();
			
			fprintf( stderr , "ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}
		for( counterIndex = 0 ; counterIndex < countersArrayRf.countersSize ; ++counterIndex )
		{
			if ( countersArrayRf.counter[ counterIndex ]->autodetectionType == TRUE )
			{
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].dbId = countersArrayRf.counter[ counterIndex ]->dbId ;
				(*counterAutodetectionStatistic)[ counterAutodetectionStatisticPos ].type = countersArrayRf.counter[ counterIndex ]->type ;
				counterAutodetectionStatisticPos++;
			}
		}
	}
	#endif


	STORAGE_CountersUnprotect();

	return OK ;
}

//
//
//

int STORAGE_UpdateConnectionConfigFiles( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL;


	enum
	{
		CCF_UPDATING_STATE_EXIT = 0 ,
		CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE,
		CCF_UPDATING_STATE_CREATING_ETH_CFG ,
		CCF_UPDATING_STATE_CREATING_GSM_CFG ,
		CCF_UPDATING_STATE_CREATING_CONN_CFG ,
		CCF_UPDATING_STATE_FFLUSH_GSM_BUFFER ,
		CCF_UPDATING_STATE_FFLUSH_ETH_BUFFER ,
		CCF_UPDATING_STATE_FFLUSH_CONN_BUFFER
	};



	if ( connection != NULL )
	{

		unsigned char connBuf[MAX_MESSAGE_LENGTH];
		unsigned char ethBuf[MAX_MESSAGE_LENGTH];
		unsigned char gsmBuf[MAX_MESSAGE_LENGTH];

		memset( connBuf , 0 , MAX_MESSAGE_LENGTH);
		memset( ethBuf , 0 , MAX_MESSAGE_LENGTH);
		memset( gsmBuf , 0 , MAX_MESSAGE_LENGTH);

		int updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
		while (updatingState != CCF_UPDATING_STATE_EXIT)
		{



			switch(updatingState)
			{
				case CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE:
				{
					if ( (connection->ethUpdateFlag) == 1 )
					{
						updatingState = CCF_UPDATING_STATE_CREATING_ETH_CFG ;
					}
					else if ( (connection->gsmUpdateFlag) == 1 )
					{
						updatingState = CCF_UPDATING_STATE_CREATING_GSM_CFG ;
					}
					else if ( (connection->connUpdateFlag) == 1 )
					{
						updatingState = CCF_UPDATING_STATE_CREATING_CONN_CFG ;
					}
					else
					{
						updatingState = CCF_UPDATING_STATE_EXIT ;
					}
				}
				break;

				case CCF_UPDATING_STATE_CREATING_CONN_CFG:
				{
					// Creating buffer for "connection.cfg"
					unsigned char connectId[16];
					memset(connectId , 0 , 16);

					switch (connection->mode)
					{
						case CONNECTION_GPRS_SERVER:
						case CONNECTION_GPRS_ALWAYS:
						case CONNECTION_GPRS_CALENDAR:
						case CONNECTION_CSD_INCOMING:
						case CONNECTION_CSD_OUTGOING:
						{
							sprintf((char *)connectId , "GSM");
						}
						break;

						default:
							sprintf((char *)connectId , "Ethernet");
							break;
					}

					unsigned char objName[STORAGE_PLACE_NAME_LENGTH];
					unsigned char objPlace[STORAGE_PLACE_NAME_LENGTH];

					memset(objName , 0 , STORAGE_PLACE_NAME_LENGTH);
					memset(objPlace , 0 , STORAGE_PLACE_NAME_LENGTH);

					COMMON_GET_USPD_OBJ_NAME(objName);
					COMMON_GET_USPD_LOCATION(objPlace);

					snprintf((char *)connBuf, MAX_MESSAGE_LENGTH , \
										"[connect_id]\n" \
										"%s\n" \
										"[server_link]\n" \
										"%s\n" \
										"[port]\n" \
										"%u\n" \
										"[obj_name]\n" \
										"%s\n" \
										"[obj_place]\n" \
										"%s\n" ,  connectId , connection->server , connection->port , objName , objPlace);

					 updatingState = CCF_UPDATING_STATE_FFLUSH_CONN_BUFFER ;

				}
				break;

				case CCF_UPDATING_STATE_CREATING_ETH_CFG:
				{
					// Creating buffer for "eth.cfg"

					unsigned char getIP_id[16];
					memset( getIP_id , 0 , 16 );


					if ( connection ->eth0mode == ETH_STATIC)
						sprintf((char *)getIP_id , "getIP_manual");
					else
						sprintf( (char *)getIP_id , "getIP_auto");

					unsigned char ethConnectId[16];
					memset(ethConnectId , 0 , 16);
					if ( connection->mode == CONNECTION_ETH_CALENDAR )
						sprintf((char *)ethConnectId , "schedule");
					else
						sprintf((char *)ethConnectId , "always") ;

					int ethCalendarType = 0;
					switch(connection->calendarEth.interval)
					{
						case CONNECTION_INTERVAL_WEEK:
							ethCalendarType = 2 ;
							break;

						case CONNECTION_INTERVAL_DAY:
							ethCalendarType = 1 ;
							break;

						default:
							ethCalendarType = 0 ;
							break;
					}

					int ethIfStopTime;
					if (connection->calendarEth.ifstoptime == TRUE)
						ethIfStopTime = 1 ;
					else
						ethIfStopTime = 0 ;


					snprintf( (char *)ethBuf , \
							  MAX_MESSAGE_LENGTH,\
								"[getIP_id]\n" \
								"%s\n" \
								"[ip_address]\n" \
								"%s\n" \
								"[ip_mask]\n" \
								"%s\n" \
								"[getway]\n" \
								"%s\n" \
								"[dns_1]\n" \
								"%s\n" \
								"[dns_2]\n" \
								"%s\n" \
								"[connection_id]\n" \
								"%s\n" \
								"[startDate]\n" \
								"%02d-%02d-%02d\n" \
								"[startTimeHour]\n" \
								"%d\n" \
								"[startTimeMin]\n" \
								"%d\n" \
								"[intevalOffset]\n" \
								"%ld\n" \
								"[intervalType]\n" \
								"%d\n" \
								"[finishedOn]\n" \
								"%d\n" \
								"[stopTimeHour]\n" \
								"%d\n" \
								"[stopTimeMin]\n" \
								"%d\n" ,    (char*)getIP_id , (char*)connection->myIp , (char*)connection->mask , (char*)connection->gateway , (char*)connection->dns1, \
											(char*)connection->dns2 , (char*)ethConnectId , (connection->calendarEth.start_year+2000) , \
											connection->calendarEth.start_month , connection->calendarEth.start_mday , \
											connection->calendarEth.start_hour , connection->calendarEth.start_min , \
											connection->calendarEth.interval , ethCalendarType , ethIfStopTime , \
											connection->calendarEth.stop_hour , connection->calendarEth.stop_min);
					updatingState = CCF_UPDATING_STATE_FFLUSH_ETH_BUFFER ;
				}
				break;

				case CCF_UPDATING_STATE_CREATING_GSM_CFG:
				{
					// Creating buffer for "gsm.cfg"

					unsigned char gsmConnectId[16];
					memset(gsmConnectId , 0 , 16);

					switch(connection->mode)
					{
						case CONNECTION_GPRS_CALENDAR:
							sprintf((char *)gsmConnectId , "schedule_gprs");
							break;
						case CONNECTION_CSD_INCOMING:
							sprintf((char *)gsmConnectId , "incamming_call_csd");
							break;
						case CONNECTION_CSD_OUTGOING:
							sprintf((char *)gsmConnectId , "schedule_csd");
							break;
						default:
							sprintf((char *)gsmConnectId , "always_gprs");
							break;
					}

					int gsmCalendarType = 0;
					switch(connection->calendarGsm.interval)
					{
						case CONNECTION_INTERVAL_WEEK:
							gsmCalendarType = 2 ;
							break;

						case CONNECTION_INTERVAL_DAY:
							gsmCalendarType = 1 ;
							break;

						default:
							gsmCalendarType = 0 ;
							break;
					}
					int gsmIfStopTime;
					if (connection->calendarEth.ifstoptime == TRUE)
						gsmIfStopTime = 1 ;
					else
						gsmIfStopTime = 0 ;

					int simPrinciple = 1 ;
					switch( connection->simPriniciple )
					{
						case SIM_FIRST:
							simPrinciple = 1 ;
							break;

						case SIM_SECOND:
							simPrinciple = 2 ;
							break;

						case SIM_SECOND_THEN_FIRST:
							simPrinciple = 21 ;
							break;

						default:
							simPrinciple = 12 ;
							break;
					}


					snprintf( (char *)gsmBuf , \
							  MAX_MESSAGE_LENGTH , \
							"[sim1_apn]\n" \
							"%s\n" \
							"[sim1_username]\n" \
							"%s\n" \
							"[sim1_password]\n" \
							"%s\n" \
							"[sim2_apn]\n" \
							"%s\n" \
							"[sim2_username]\n" \
							"%s\n" \
							"[sim2_password]\n" \
							"%s\n" \
							"[sim_1_allowed_csd_number_1]\n" \
							"%s\n" \
							"[sim_1_allowed_csd_number_2]\n" \
							"%s\n" \
							"[sim_2_allowed_csd_number_1]\n" \
							"%s\n" \
							"[sim_2_allowed_csd_number_2]\n" \
							"%s\n" \
							"[sim_1_allowed_csd_number_3]\n" \
							"%s\n" \
							"[sim_2_allowed_csd_number_3]\n" \
							"%s\n" \
							"[sim_change_tries]\n" \
							"%d\n" \
							"[connection_id]\n" \
							"%s\n" \
							"[startDate]\n" \
							"%02d-%02d-%02d\n" \
							"[startTimeHour]\n" \
							"%d\n" \
							"[startTimeMin]\n" \
							"%d\n" \
							"[intervalOffset]\n" \
							"%ld\n" \
							"[intervalType]\n" \
							"%d\n" \
							"[finishedOn]\n" \
							"%d\n" \
							"[stopTimeHour]\n" \
							"%d\n" \
							"[stopTimeMin]\n" \
							"%d\n" \
							"[simPriority]\n" \
							"%d\n",    connection->apn1 , connection->apn1_user , connection->apn1_pass , \
										connection->apn2 , connection->apn2_user , connection->apn2_pass , \
										connection->allowedIncomingNum1Sim1 , connection->allowedIncomingNum2Sim1, \
										connection->allowedIncomingNum2Sim1 , connection->allowedIncomingNum2Sim2, \
										connection->serverPhoneSim1 , connection->serverPhoneSim2, \
										connection->simTryes , gsmConnectId , \
										(connection->calendarGsm.start_year+2000) , connection->calendarGsm.start_month , connection->calendarGsm.start_mday, \
										connection->calendarGsm.start_hour , connection->calendarGsm.start_min , \
										connection->calendarGsm.interval , gsmCalendarType , gsmIfStopTime , connection->calendarGsm.stop_hour, \
										connection->calendarGsm.stop_min , simPrinciple);
					updatingState = CCF_UPDATING_STATE_FFLUSH_GSM_BUFFER ;
				}
				break;

				case CCF_UPDATING_STATE_FFLUSH_GSM_BUFFER:
				{
#if 0
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					// Write buffer to the files

					//debug
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "GSMBUF:\n%s\n",gsmBuf);


					int gsm_fd = open(CONFIG_PATH_GSM , O_RDWR | O_TRUNC );

					if ( gsm_fd >= 0 )
					{
						//debug
						//printf("configuration files opening OK\n");
						int gsm_bak_fd = open(CONFIG_PATH_GSM ".bak", O_RDWR | O_TRUNC | O_CREAT , S_IRWXU);

						if ( gsm_bak_fd>=0 )
						{
							//printf("backup files opening OK\n");
							ftruncate(gsm_bak_fd , 0);

							//printf("backup files truncate OK\n");

							unsigned char buf[MAX_MESSAGE_LENGTH];
							int bytes = 0;
							memset( buf , 0 , MAX_MESSAGE_LENGTH );


							while( (bytes = read(gsm_fd , buf , MAX_MESSAGE_LENGTH)) > 0 )
							{
								write( gsm_bak_fd , buf , bytes);
								memset( buf , 0 , MAX_MESSAGE_LENGTH );
							}


							close(gsm_bak_fd);

							//writing new config files

							lseek( gsm_fd , 0 , SEEK_SET );

							ftruncate(gsm_fd , 0);

							if ( write( gsm_fd , gsmBuf , strlen((char *)gsmBuf) ) == strlen((char *)gsmBuf) )
							{
								res = OK ;
							}
							else
							{
								res = ERROR_GENERAL;
								updatingState = CCF_UPDATING_STATE_EXIT ;
							}

						}

						close(gsm_fd);
					}

					connection->gsmUpdateFlag = 2 ;
#else
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					res = STORAGE_SaveNewConfig( MICRON_PROP_GSM , gsmBuf , strlen( (char *)gsmBuf ) , DESC_EVENT_CFG_UPDATE_BY_SMS );
					if( res == OK )
					{
						connection->gsmUpdateFlag = 2 ;
					}
					else
					{
						updatingState = CCF_UPDATING_STATE_EXIT ;
					}
#endif
				}
				break;

				case CCF_UPDATING_STATE_FFLUSH_ETH_BUFFER:
				{
#if 0
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					// Write buffers to the files

					//debug
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "ETHBUF:\n%s\n",ethBuf);


					int eth_fd = open(CONFIG_PATH_ETH , O_RDWR | O_TRUNC);

					if ( eth_fd >= 0 )
					{
						//debug
						//printf("configuration files opening OK\n");
						int eth_bak_fd = open(CONFIG_PATH_ETH ".bak", O_RDWR | O_TRUNC | O_CREAT , S_IRWXU);

						if ( eth_bak_fd >=0 )
						{
							//printf("backup files opening OK\n");
							ftruncate(eth_bak_fd , 0);

							//printf("backup files truncate OK\n");

							unsigned char buf[MAX_MESSAGE_LENGTH];
							int bytes = 0;
							memset( buf , 0 , MAX_MESSAGE_LENGTH );


							//printf("backuping file connection.cfg\n");


							while( (bytes = read(eth_fd , buf , MAX_MESSAGE_LENGTH)) > 0 )
							{
								write( eth_bak_fd , buf , bytes);
								memset( buf , 0 , MAX_MESSAGE_LENGTH );
							}

							close(eth_bak_fd);

							//writing new config files

							lseek( eth_fd , 0 , SEEK_SET );

							ftruncate(eth_fd , 0);

							if ( write( eth_fd , ethBuf , strlen((char *)ethBuf) ) == strlen((char *)ethBuf)  )
							{
								res = OK ;
							}
							else
							{
								res = ERROR_GENERAL;
								updatingState = CCF_UPDATING_STATE_EXIT ;
							}

						}

						close(eth_fd);
					}

					connection->ethUpdateFlag = 2 ;

#else
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					res = STORAGE_SaveNewConfig( MICRON_PROP_ETH , ethBuf , strlen( (char *)ethBuf ) , DESC_EVENT_CFG_UPDATE_BY_SMS );
					if( res == OK )
					{
						connection->ethUpdateFlag = 2 ;
					}
					else
					{
						updatingState = CCF_UPDATING_STATE_EXIT ;
					}
#endif
				}
				break;

				case CCF_UPDATING_STATE_FFLUSH_CONN_BUFFER:
				{
#if 0
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					// Write buffers to the files

					//debug
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "CONNBUF:\n%s\n",connBuf);

					int conn_fd = open( CONFIG_PATH_CONNECTION , O_RDWR | O_TRUNC);

					if ( conn_fd >= 0 )
					{
						//debug
						//printf("configuration files opening OK\n");
						int conn_bak_fd = open( CONFIG_PATH_CONNECTION ".bak", O_RDWR | O_TRUNC | O_CREAT , S_IRWXU);

						if ( conn_bak_fd >= 0 )
						{
							//printf("backup files opening OK\n");
							ftruncate(conn_bak_fd , 0);

							//printf("backup files truncate OK\n");

							unsigned char buf[MAX_MESSAGE_LENGTH];
							int bytes = 0;
							memset( buf , 0 , MAX_MESSAGE_LENGTH );


							//printf("backuping file connection.cfg\n");
							while( (bytes = read(conn_fd , buf , MAX_MESSAGE_LENGTH)) > 0 )
							{
								write( conn_bak_fd , buf , bytes) ;
								memset( buf , 0 , MAX_MESSAGE_LENGTH );
							}

							close(conn_bak_fd);

							//writing new config files

							lseek( conn_fd , 0 , SEEK_SET );

							ftruncate(conn_fd , 0);

							if ( write( conn_fd , connBuf , strlen((char *)connBuf) ) == strlen((char *)connBuf) )
							{
								res = OK ;
							}
							else
							{
								res = ERROR_GENERAL;
								updatingState = CCF_UPDATING_STATE_EXIT ;
							}

						}

						close(conn_fd);
					}

					connection->connUpdateFlag = 2 ;

#else
					updatingState = CCF_UPDATING_STATE_CHOICE_FILE_TO_UPDATE ;
					res = STORAGE_SaveNewConfig( MICRON_PROP_USD , connBuf , strlen( (char *)connBuf ), DESC_EVENT_CFG_UPDATE_BY_SMS );
					if( res == OK )
					{
						connection->connUpdateFlag = 2 ;
					}
					else
					{
						updatingState = CCF_UPDATING_STATE_EXIT ;
					}

#endif

				}
				break;



				default:

					break;

			}
		}

	}

	if ( connection->servUpdateFlag == 1 )
	{
		res = STORAGE_SaveServiceProperties( connection ) ;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "UpdatingConfigFiles() exit\n");
	return res;

}

//
//
//

void STORAGE_SmsNewConfig ( char * str , char * answer )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL getStrRemain = FALSE ;
	BOOL finalBlank = FALSE ;
	int partIdx = 0 ;
	int keywordBankIdx = 0 ;
	char * cp  = NULL, *cp_ptr = NULL;
	enum keywordBankEnum
	{
		KBIDX_NEWCFG = 0, KBIDX_CONN, KBIDX_IPP, KBIDX_TOUCH, KBIDX_APN1,
		KBIDX_APN2, KBIDX_UP1, KBIDX_UP2,	KBIDX_CSDN1, KBIDX_CSDN2,
		KBIDX_SERVIPP, KBIDX_SERVPHONE, KBIDX_SERVSMS, KBIDX_SERVINTTYPE, KBIDX_SERVINTVAL,
		KBIDX_SERVSTART, KBIDX_SIMPR, KBIDX_APPLY, KBIDX_CANCEL, KBIDX_ETHMOD,
		KBIDX_ETHIP, KBIDX_ETHGW, KBIDX_ETHNET, KBIDX_ETHDNS, KBIDX_STARTDTS,
		KBIDX_INTVAL, KBIDX_INTTYPE, KBIDX_STOPFLG, KBIDX_WORKTIME, KBIDX_SETPWD,
		KBIDX_REBOOT, KBIDX_GETSNSQ, KBIDX_GETVER,
		KBIDX_NOKEYWORD
	};

	char * keywordBank[] = {
		"NEWCFG", "CONN", "IPP", "TOUCH", "APN1",
		"APN2", "UP1", "UP2", "CSDN1", "CSDN2",
		"SERVIPP", "SERVPHONE", "SERVSMS", "SERVINTTYPE", "SERVINTVAL",
		"SERVSTART", "SIMPR", "APPLY", "CANCEL", "ETHMOD",
		"ETHIP", "ETHGW", "ETHNET", "ETHDNS", "STARTDTS",
		"INTVAL", "INTTYPE", "STOPFLG", "WORKTIME", "SETPWD",
		"REBOOT", "GETSNSQ", "GETVER",
		NULL
	} ;

	enum answerBankEnum
	{
		ABE_OK, ABE_ERROR, ABE_FORBIDDEN
	};

	char * answerBank[] = {
		"OK", "ERROR" , "FORBIDDEN", NULL
	};

	enum messagePartEnum
	{
		MPIDX_PREXIX = 0,
		MPIDX_KEYWORD,
		MPIDX_PRM1,
		MPIDX_PRM2
	};

	char * messagePart[ 4 ] = { NULL };


	cp = strdup(str);
	if (!cp)
		return ;

	// pointer CP will be changed during STRSEP execution. Need to save pointer value for FREE
	cp_ptr = cp ;

	COMMON_RemoveExcessBlanks( cp );
	if ( (strlen(cp) > 0 ) && (cp[ strlen(cp) - 1 ] == ' ') )
		finalBlank = TRUE ;

	#define S_SNC_ANSWER_OK		snprintf( answer , SMS_TEXT_LENGTH , "%s %s", answerBank[ABE_OK] , keywordBank[keywordBankIdx] )
	#define S_SNC_ANSWER_ERROR	snprintf( answer , SMS_TEXT_LENGTH , "%s %s", answerBank[ABE_ERROR] , keywordBank[keywordBankIdx] )
	#define S_SNC_ANSWER_FORBID snprintf( answer , SMS_TEXT_LENGTH , "%s %s", answerBank[ABE_FORBIDDEN] , keywordBank[keywordBankIdx] )

	while(!getStrRemain)
	{
		messagePart[ partIdx ] = strsep( &cp , " " );

		switch (partIdx )
		{
			case MPIDX_PREXIX:
				if ( !messagePart[ partIdx ] || strcasecmp(messagePart[ partIdx ] , "USPD"))
					goto S_SNC_EXIT;
				break;

			case MPIDX_KEYWORD:
				if ( !messagePart[ partIdx ] || !strlen( messagePart[ partIdx ] ) )
				{
					strcpy(answer , answerBank[ABE_ERROR] );
					goto S_SNC_EXIT;
				}

				//checking the keyword
				for( keywordBankIdx = 0 ; keywordBankIdx < KBIDX_NOKEYWORD ; keywordBankIdx++ )
				{
					if ( !strcasecmp( keywordBank[ keywordBankIdx ], messagePart[ partIdx ] ) )
						break;
				}

				if ( keywordBankIdx == KBIDX_NOKEYWORD )
				{
					strcpy(answer , answerBank[ABE_ERROR]);
					goto S_SNC_EXIT;
				}

				//only keywords TOUCH and SETWPD have 4 parametres
				if ( keywordBankIdx != KBIDX_TOUCH && keywordBankIdx != KBIDX_SETPWD )
					getStrRemain = TRUE ;

				break;

			default:
				if ( !messagePart[ partIdx ] || !strlen( messagePart[ partIdx ] ) )
				{
					S_SNC_ANSWER_ERROR;
					goto S_SNC_EXIT;
				}

				getStrRemain = TRUE ;
				break;
		}//switch (partIdx )

		partIdx++;
	}

	messagePart[ partIdx ] = cp ;
	if ( !messagePart[ partIdx ] )
		messagePart[ partIdx ] = "";


	switch( keywordBankIdx )
	{
		case KBIDX_NEWCFG:
		{
			char pass[128] = {0};
			COMMON_GetWebUIPass((unsigned char *)pass);
			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );

			if (!strcmp( pass , messagePart[MPIDX_PRM1] ) || !strcmp( messagePart[MPIDX_PRM1] , STORAGE_DEFPSWD))
			{
				writingSmsNewParams = TRUE ;
				memcpy(&connectionParams_copy , &connectionParams , sizeof(connection_t));

				S_SNC_ANSWER_OK ;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_CONN:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if ( !strcasecmp(messagePart[MPIDX_PRM1], "GPRS ALL") )
					{
						connectionParams_copy.mode = CONNECTION_GPRS_ALWAYS ;

						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							default:
								break;
						}

						S_SNC_ANSWER_OK ;
					}
					if ( !strcasecmp(messagePart[MPIDX_PRM1], "GPRS SERV") )
					{
						connectionParams_copy.mode = CONNECTION_GPRS_SERVER ;

						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							default:
								break;
						}

						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "GPRS CAL") )
					{
						connectionParams_copy.mode = CONNECTION_GPRS_CALENDAR ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "CSD IN") )
					{
						connectionParams_copy.mode = CONNECTION_CSD_INCOMING ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "CSD OUT CAL") )
					{
						connectionParams_copy.mode = CONNECTION_CSD_OUTGOING ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.gsmUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "ETH ALL") )
					{
						connectionParams_copy.mode = CONNECTION_ETH_ALWAYS ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "ETH CAL") )
					{
						connectionParams_copy.mode = CONNECTION_ETH_CALENDAR ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_SERVER:
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else if ( !strcasecmp(messagePart[MPIDX_PRM1], "ETH SERV") )
					{
						connectionParams_copy.mode = CONNECTION_ETH_SERVER ;
						switch( connectionParams.mode )
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							case CONNECTION_CSD_INCOMING:
							case CONNECTION_CSD_OUTGOING:
								connectionParams_copy.connUpdateFlag = 1 ;
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
								connectionParams_copy.ethUpdateFlag = 1 ;
								break;

							default:
								break;
						}
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;

				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				//current connection state
				strncpy(answer, keywordBank[keywordBankIdx] , SMS_TEXT_LENGTH);
				switch( connectionParams.mode )
				{
					case CONNECTION_ETH_ALWAYS:
						strncat(answer, ": ETH ALL to ", strlen(answer) - SMS_TEXT_LENGTH);
						goto S_SNC_CONN_ETH;

					case CONNECTION_ETH_SERVER:
						strncat(answer, ": ETH SERV ", strlen(answer) - SMS_TEXT_LENGTH);
						goto S_SNC_CONN_ETH;


					case CONNECTION_ETH_CALENDAR:
						strncat(answer, ": ETH CAL to ", strlen(answer) - SMS_TEXT_LENGTH);
						S_SNC_CONN_ETH:
						if ( ETH_GetServiceModeStat() == TRUE)
							strncat(answer, "SERVICE ", strlen(answer) - SMS_TEXT_LENGTH);
						else
							snprintf( &answer[ strlen(answer) ] , strlen(answer) - SMS_TEXT_LENGTH , \
									"%s:%u ", (char *)connectionParams.server , connectionParams.port);
						if ( ETH_ConnectionState() == OK )
							snprintf( &answer[ strlen(answer) ] , strlen(answer) - SMS_TEXT_LENGTH , \
									"ON %d sec", ETH_GetCurrentConnectionTime() );
						else
							strncat(answer, "OFF", strlen(answer) - SMS_TEXT_LENGTH);

						break;


					case CONNECTION_GPRS_SERVER:
						strncat(answer, ": GPRS SERV ", strlen(answer) - SMS_TEXT_LENGTH);
						break;

					case CONNECTION_GPRS_ALWAYS:
						strncat(answer, ": GPRS ALL to ", strlen(answer) - SMS_TEXT_LENGTH);
						goto S_SNC_CONN_GRPS;


					case CONNECTION_GPRS_CALENDAR:
					{
						strncat(answer, ": GPRS OUT CAL to ", strlen(answer) - SMS_TEXT_LENGTH);

						S_SNC_CONN_GRPS:
						if ( GSM_GetServiceModeStat() == TRUE)
							strncat(answer, "SERVICE ", strlen(answer) - SMS_TEXT_LENGTH);
						else
							snprintf( &answer[ strlen(answer) ] , strlen(answer) - SMS_TEXT_LENGTH , \
									"%s:%u ", (char *)connectionParams.server , connectionParams.port);

						if ( GSM_ConnectionState() == OK )
							snprintf( &answer[ strlen(answer) ] , strlen(answer) - SMS_TEXT_LENGTH , \
									"ON %d sec", GSM_GetCurrentConnectionTime() );
						else
							strncat(answer, "OFF", strlen(answer) - SMS_TEXT_LENGTH);

					}
						break;

					case CONNECTION_CSD_INCOMING:
					{
						char * allowedPhoneNumb1 = NULL;
						char * allowedPhoneNumb2 = NULL;
						strncat(answer, ": CSD IN from ", strlen(answer) - SMS_TEXT_LENGTH);
						if ( GSM_GetCurrentSimNumber() == SIM1 )
						{
							allowedPhoneNumb1 = (char *)connectionParams.allowedIncomingNum1Sim1 ;
							allowedPhoneNumb2 = (char *)connectionParams.allowedIncomingNum2Sim1 ;
						}
						else
						{
							allowedPhoneNumb1 = (char *)connectionParams.allowedIncomingNum1Sim2 ;
							allowedPhoneNumb2 = (char *)connectionParams.allowedIncomingNum2Sim2 ;
						}

						if ( !strlen(allowedPhoneNumb1) && !strlen(allowedPhoneNumb2))
							strncat(answer, "any number", strlen(answer) - SMS_TEXT_LENGTH);
						else
						{
							BOOL orFlag = FALSE ;
							if (strlen(allowedPhoneNumb1))
							{
								strncat(answer, (char *)allowedPhoneNumb1, strlen(answer) - SMS_TEXT_LENGTH);
								orFlag = TRUE ;
							}

							if (strlen(allowedPhoneNumb2))
							{
								if ( orFlag )
									strncat(answer, " or ", strlen(answer) - SMS_TEXT_LENGTH);

								strncat(answer, (char *)allowedPhoneNumb2, strlen(answer) - SMS_TEXT_LENGTH);
							}

							strncat(answer, " OFF", strlen(answer) - SMS_TEXT_LENGTH);
						}

					}
						break;

					case CONNECTION_CSD_OUTGOING:
					{
						strncat(answer, ": CSD CAL to ", strlen(answer) - SMS_TEXT_LENGTH);

						if ( GSM_GetCurrentSimNumber() == SIM1 )
							strncat(answer, (char *)connectionParams.serverPhoneSim1, strlen(answer) - SMS_TEXT_LENGTH);
						else
							strncat(answer, (char *)connectionParams.serverPhoneSim2, strlen(answer) - SMS_TEXT_LENGTH);

						strncat(answer, " OFF", strlen(answer) - SMS_TEXT_LENGTH);
					}
						break;
				}
			}
		}
			break;

		case KBIDX_IPP:
		{
			if ( !strlen( messagePart[MPIDX_PRM1] ) )
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s:%u", keywordBank[keywordBankIdx], connectionParams.server, connectionParams.port );
			else
			{
				if ( writingSmsNewParams )
				{
					char * ip = NULL ;
					char * port = NULL ;
					int port_int = 0 ;
					struct sockaddr_in sa;

					ip = strsep( &messagePart[MPIDX_PRM1] , ":" );
					port = strsep( &messagePart[MPIDX_PRM1] , ":" );

					if ( port && strlen(port) )
						port_int = atoi(port);

					if ( (port_int > 0) && ( inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1 ) )
					{
						connectionParams_copy.port = port_int ;
						strncpy( (char *)connectionParams_copy.server, ip , LINK_SIZE );
						connectionParams_copy.connUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
		}
			break;

		case KBIDX_TOUCH:
		{
			char pass[128] = {0};
			COMMON_GetWebUIPass((unsigned char *)pass);
			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM2] );

			if (!strcmp( pass , messagePart[MPIDX_PRM2] ) || !strcmp( messagePart[MPIDX_PRM2] , STORAGE_DEFPSWD))
			{
				char * ip = NULL ;
				char * port = NULL ;
				int port_int = 0 ;
				struct sockaddr_in sa;

				ip = strsep( &messagePart[MPIDX_PRM1] , ":" );
				port = strsep( &messagePart[MPIDX_PRM1] , ":" );

				if ( port && strlen(port) )
					port_int = atoi(port);

				if ( (port_int > 0) && ( inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1 ) )
				{
					connection_t connectionParams_touch;
					memcpy( &connectionParams_touch , &connectionParams , sizeof(connection_t) );

					connectionParams_touch.port = port_int ;
					strncpy( (char *)connectionParams_touch.server, ip , LINK_SIZE ) ;
					connectionParams_touch.connUpdateFlag = 1 ;

					if ( STORAGE_UpdateConnectionConfigFiles( &connectionParams_touch ) == OK )
					{
						snprintf( answer , SMS_TEXT_LENGTH , "%s %s PREV %s:%d", answerBank[ABE_OK] , keywordBank[keywordBankIdx] , \
								  (char *)connectionParams.server, connectionParams.port ) ;
						STORAGE_PROCESS_UPDATE("connection.cfg");
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_ERROR ;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_APN1:
		case KBIDX_APN2:
		{
			char * apn = NULL ;
			if ( strlen( messagePart[MPIDX_PRM1] ) || finalBlank )
			{
				apn = (char *)connectionParams_copy.apn2 ;
				if ( keywordBankIdx == KBIDX_APN1 )
					apn = (char *)connectionParams_copy.apn1 ;

				COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );

				//setting new apn
				if ( writingSmsNewParams )
				{
					memset(apn , 0 , LINK_SIZE);
					strncpy( apn , messagePart[MPIDX_PRM1] , LINK_SIZE );
					connectionParams_copy.gsmUpdateFlag = 1 ;
					S_SNC_ANSWER_OK ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				//send current apn
				apn = (char *)connectionParams.apn2 ;
				if ( keywordBankIdx == KBIDX_APN1 )
					apn = (char *)connectionParams.apn1 ;

				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], apn );
			}
		}
			break;

		case KBIDX_UP1:
		case KBIDX_UP2:
		{
			char * username = NULL ;
			char * password = NULL ;

			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					username = strsep( &messagePart[MPIDX_PRM1] , ":" );
					password = strsep( &messagePart[MPIDX_PRM1] , ":" );

					if ( username && password )
					{
						COMMON_DecodeSmsSpecialChars( username );
						COMMON_DecodeSmsSpecialChars( password );

						if (keywordBankIdx == KBIDX_UP1)
						{
							memset( connectionParams_copy.apn1_user , 0 , FIELD_SIZE );
							memset( connectionParams_copy.apn1_pass , 0 , FIELD_SIZE );
							strncpy( (char *)connectionParams_copy.apn1_user , username , FIELD_SIZE );
							strncpy( (char *)connectionParams_copy.apn1_pass , password , FIELD_SIZE );
						}
						else
						{
							memset( connectionParams_copy.apn2_user , 0 , FIELD_SIZE );
							memset( connectionParams_copy.apn2_pass , 0 , FIELD_SIZE );
							strncpy( (char *)connectionParams_copy.apn2_user , username , FIELD_SIZE );
							strncpy( (char *)connectionParams_copy.apn2_pass , password , FIELD_SIZE );
						}
						connectionParams_copy.gsmUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				username = (char *)connectionParams.apn1_user ;
				password = (char *)connectionParams.apn1_pass ;
				if ( keywordBankIdx == KBIDX_UP2 )
				{
					username = (char *)connectionParams.apn2_user ;
					password = (char *)connectionParams.apn2_pass ;
				}
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s:%s", keywordBank[keywordBankIdx], username , password );
			}
		}
			break;

		case KBIDX_CSDN1:
		case KBIDX_CSDN2:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) || finalBlank )
			{
				if (writingSmsNewParams)
				{
					COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );
					//setting up new number
					if ( connectionParams_copy.mode == CONNECTION_CSD_INCOMING )
					{
						if ( keywordBankIdx == KBIDX_CSDN1 )
						{
							memset( connectionParams_copy.allowedIncomingNum1Sim1 , 0 , PHONE_MAX_SIZE ) ;
							memset( connectionParams_copy.allowedIncomingNum1Sim2 , 0 , PHONE_MAX_SIZE ) ;
							if ( strlen( messagePart[MPIDX_PRM1] ) )
							{
								snprintf( (char *)connectionParams_copy.allowedIncomingNum1Sim1 , PHONE_MAX_SIZE , \
										"+7%s", messagePart[MPIDX_PRM1] );
								snprintf( (char *)connectionParams_copy.allowedIncomingNum1Sim2 , PHONE_MAX_SIZE , \
										"+7%s", messagePart[MPIDX_PRM1] );
							}
						}
						else
						{
							memset( connectionParams_copy.allowedIncomingNum2Sim1 , 0 , PHONE_MAX_SIZE ) ;
							memset( connectionParams_copy.allowedIncomingNum2Sim2 , 0 , PHONE_MAX_SIZE ) ;
							if ( strlen( messagePart[MPIDX_PRM1] ) )
							{
								snprintf( (char *)connectionParams_copy.allowedIncomingNum2Sim1 , PHONE_MAX_SIZE , \
										"+7%s", messagePart[MPIDX_PRM1] );
								snprintf( (char *)connectionParams_copy.allowedIncomingNum2Sim2 , PHONE_MAX_SIZE , \
										"+7%s", messagePart[MPIDX_PRM1] );
							}
						}

						connectionParams_copy.gsmUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else if ( connectionParams_copy.mode == CONNECTION_CSD_OUTGOING )
					{
						memset( connectionParams_copy.serverPhoneSim1 , 0 , PHONE_MAX_SIZE ) ;
						memset( connectionParams_copy.serverPhoneSim2 , 0 , PHONE_MAX_SIZE ) ;

						if ( strlen( messagePart[MPIDX_PRM1] ) )
						{
							snprintf( (char *)connectionParams_copy.serverPhoneSim1 , PHONE_MAX_SIZE , \
									"+7%s", messagePart[MPIDX_PRM1] );
							snprintf( (char *)connectionParams_copy.serverPhoneSim2 , PHONE_MAX_SIZE , \
									"+7%s", messagePart[MPIDX_PRM1] );
						}
						connectionParams_copy.gsmUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;

			}
			else
			{
				//setting up new number
				if ( connectionParams.mode == CONNECTION_CSD_INCOMING )
				{
					if ( keywordBankIdx == KBIDX_CSDN1 )
						snprintf( answer , SMS_TEXT_LENGTH , "%s IN FROM S1:%s S2:%s", keywordBank[keywordBankIdx], \
								  (char *)connectionParams.allowedIncomingNum1Sim1 , (char *)connectionParams.allowedIncomingNum1Sim2 );
					else
						snprintf( answer , SMS_TEXT_LENGTH , "%s IN FROM S1:%s S2:%s", keywordBank[keywordBankIdx], \
								  (char *)connectionParams.allowedIncomingNum2Sim1 , (char *)connectionParams.allowedIncomingNum2Sim2 );
				}
				else if ( connectionParams.mode == CONNECTION_CSD_OUTGOING )
				{
					snprintf( answer , SMS_TEXT_LENGTH , "%s OUT S1:%s S2:%s", keywordBank[keywordBankIdx], \
							  (char *)connectionParams.serverPhoneSim1, (char *)connectionParams.serverPhoneSim2 );
				}
				else
					S_SNC_ANSWER_ERROR ;
			}
		}
			break;

		case KBIDX_SERVIPP:
		{
			if ( !strlen( messagePart[MPIDX_PRM1] ) )
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s:%u", keywordBank[keywordBankIdx], connectionParams.serviceIp, connectionParams.servicePort );
			else
			{
				if ( writingSmsNewParams )
				{
					char * ip = NULL ;
					char * port = NULL ;
					int port_int = 0 ;
					struct sockaddr_in sa;

					ip = strsep( &messagePart[MPIDX_PRM1] , ":" );
					port = strsep( &messagePart[MPIDX_PRM1] , ":" );

					if ( port && strlen(port) )
						port_int = atoi(port);

					if ( (port_int > 0) && ( inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1 ) )
					{
						connectionParams_copy.servicePort = port_int ;
						strncpy( (char *)connectionParams_copy.serviceIp, ip , IP_SIZE );
						connectionParams_copy.servUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
		}
			break;

		case KBIDX_SERVPHONE:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) || finalBlank)
			{
				if (writingSmsNewParams)
				{
					COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );
					memset( connectionParams_copy.servicePhone , 0 , PHONE_MAX_SIZE);
					if ( strlen( messagePart[MPIDX_PRM1] ) )
						snprintf( (char *)connectionParams_copy.servicePhone , PHONE_MAX_SIZE ,"+7%s", messagePart[MPIDX_PRM1] );
					connectionParams_copy.servUpdateFlag = 1 ;
					S_SNC_ANSWER_OK ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], connectionParams.servicePhone);
		}
			break;

		case KBIDX_SERVSMS:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) || finalBlank)
			{
				if (writingSmsNewParams)
				{
					COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );
					memset( connectionParams_copy.smsReportPhone , 0 , PHONE_MAX_SIZE);
					if ( strlen( messagePart[MPIDX_PRM1] ) )
						snprintf( (char *)connectionParams_copy.smsReportPhone , PHONE_MAX_SIZE ,"+7%s", messagePart[MPIDX_PRM1] );
					connectionParams_copy.servUpdateFlag = 1 ;
					connectionParams_copy.servSmsUpdateFlag = 1 ;
					S_SNC_ANSWER_OK ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], connectionParams.smsReportPhone);
		}
			break;

		case KBIDX_SERVINTTYPE:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					int serviceInterval = atoi( messagePart[MPIDX_PRM1] );
					switch( serviceInterval )
					{
						case -1:
						case 0:
						case 1:
						case 2:
							connectionParams_copy.serviceInterval = serviceInterval ;
							connectionParams_copy.servUpdateFlag = 1 ;
							S_SNC_ANSWER_OK;
							break;

						default:
							S_SNC_ANSWER_ERROR;
							break;
					}
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], connectionParams.serviceInterval);
		}
			break;

		case KBIDX_SERVINTVAL:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					int serviceRatio = atoi(messagePart[MPIDX_PRM1]);
					if ( serviceRatio > 0 )
					{
						connectionParams_copy.serviceRatio = serviceRatio ;
						connectionParams_copy.servUpdateFlag = 1 ;
						S_SNC_ANSWER_OK;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], connectionParams.serviceRatio);
		}
			break;

		case KBIDX_SERVSTART:
		{
			char pass[128] = {0};
			COMMON_GetWebUIPass((unsigned char *)pass);

			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );
			if (!strcmp( pass , messagePart[MPIDX_PRM1] ) || !strcmp( messagePart[MPIDX_PRM1] , STORAGE_DEFPSWD) )
			{
				unlink( STORAGE_PATH_SERVICE );
				S_SNC_ANSWER_OK;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_SIMPR:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					int symPr = atoi( messagePart[MPIDX_PRM1] );
					switch(symPr)
					{
						case 1:
							connectionParams_copy.simPriniciple = SIM_FIRST ;
							goto S_SNC_SIMPR_OK;

						case 2:
							connectionParams_copy.simPriniciple = SIM_SECOND ;
							goto S_SNC_SIMPR_OK;

						case 12:
							connectionParams_copy.simPriniciple = SIM_FIRST_THEN_SECOND ;
							goto S_SNC_SIMPR_OK;

						case 21:
							connectionParams_copy.simPriniciple = SIM_SECOND_THEN_FIRST ;
							S_SNC_SIMPR_OK:
							connectionParams_copy.gsmUpdateFlag = 1 ;
							S_SNC_ANSWER_OK ;
							break;

						default:
							S_SNC_ANSWER_ERROR ;
							break;
					}
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				snprintf( answer , SMS_TEXT_LENGTH , "%s ", keywordBank[keywordBankIdx]);
				switch( connectionParams.simPriniciple )
				{
					case SIM_FIRST:				strncat( answer , "1", SMS_TEXT_LENGTH - strlen(answer) );	break;
					case SIM_SECOND:			strncat( answer , "2", SMS_TEXT_LENGTH - strlen(answer) );	break;
					case SIM_FIRST_THEN_SECOND:	strncat( answer , "12", SMS_TEXT_LENGTH - strlen(answer) ); break;
					case SIM_SECOND_THEN_FIRST:	strncat( answer , "21", SMS_TEXT_LENGTH - strlen(answer) ); break;
					default:			 strncat( answer , "INCORRECT", SMS_TEXT_LENGTH - strlen(answer) );	break;
				}
			}
		}
			break;

		case KBIDX_APPLY:
		{
			if ( writingSmsNewParams )
			{
				writingSmsNewParams = 0 ;
				if ( STORAGE_UpdateConnectionConfigFiles( &connectionParams_copy ) == OK )
				{
					int sq = GSM_GetSignalQuality();
					unsigned char sn[32] = {0};
					COMMON_GET_USPD_SN(sn);

					if ( connectionParams_copy.connUpdateFlag || connectionParams_copy.servUpdateFlag )
					{
						if ( connectionParams_copy.servSmsUpdateFlag )
						{
							unlink(STORAGE_PATH_IMSI "_1");
							unlink(STORAGE_PATH_IMSI "_2");
						}
						STORAGE_PROCESS_UPDATE("connection.cfg");
					}

					if ( connectionParams_copy.ethUpdateFlag )
						STORAGE_PROCESS_UPDATE("ethernet.cfg");

					if ( connectionParams_copy.gsmUpdateFlag )
						STORAGE_PROCESS_UPDATE("gsm.cfg");

					snprintf( answer , SMS_TEXT_LENGTH , "%s %s %s SQ %d%%", answerBank[ABE_OK] , keywordBank[keywordBankIdx] , sn , sq );
				}
				else
					S_SNC_ANSWER_ERROR ;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_CANCEL:			
			if ( writingSmsNewParams )
			{
				writingSmsNewParams = FALSE ;
				S_SNC_ANSWER_OK ;
			}
			else
				S_SNC_ANSWER_ERROR ;

			break;

		case KBIDX_ETHMOD:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if (strcmp( messagePart[MPIDX_PRM1] , "STATIC" ) )
					{
						connectionParams_copy.eth0mode = ETH_STATIC ;
						connectionParams_copy.ethUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else if (strcmp( messagePart[MPIDX_PRM1] , "DHCP" ) )
					{
						connectionParams_copy.eth0mode = ETH_DHCP ;
						connectionParams_copy.ethUpdateFlag = 1 ;
						S_SNC_ANSWER_OK ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				char * currMode = "STATIC" ;
				if ( connectionParams.eth0mode == ETH_DHCP )
					currMode = "DHCP";

				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], currMode );
			}
		}
			break;

		case KBIDX_ETHIP:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if ( connectionParams_copy.eth0mode == ETH_STATIC )
					{
						//checking for valid ip address
						struct sockaddr_in sa;
						if ( inet_pton(AF_INET, messagePart[MPIDX_PRM1], &(sa.sin_addr)) == 1 )
						{
							//ip addres is valid
							strncpy( (char *)connectionParams_copy.myIp , messagePart[MPIDX_PRM1], IP_SIZE );
							connectionParams_copy.ethUpdateFlag = 1 ;
						}
						else
							S_SNC_ANSWER_ERROR ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				char ip[IP_SIZE];
				COMMON_GET_USPD_IP(ip);
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], ip );
			}
		}
			break;

		case KBIDX_ETHGW:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if ( connectionParams_copy.eth0mode == ETH_STATIC )
					{
						//checking for valid ip address
						struct sockaddr_in sa;
						if ( inet_pton(AF_INET, messagePart[MPIDX_PRM1], &(sa.sin_addr)) == 1 )
						{
							//ip addres is valid
							strncpy( (char *)connectionParams_copy.gateway , messagePart[MPIDX_PRM1], IP_SIZE );
							connectionParams_copy.ethUpdateFlag = 1 ;
						}
						else
							S_SNC_ANSWER_ERROR ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], connectionParams.gateway );
			}
		}
			break;

		case KBIDX_ETHNET:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if ( connectionParams_copy.eth0mode == ETH_STATIC )
					{
						//checking for valid ip address
						struct sockaddr_in sa;
						if ( inet_pton(AF_INET, messagePart[MPIDX_PRM1], &(sa.sin_addr)) == 1 )
						{
							//ip addres is valid
							strncpy( (char *)connectionParams_copy.mask , messagePart[MPIDX_PRM1], IP_SIZE );
							connectionParams_copy.ethUpdateFlag = 1 ;
						}
						else
							S_SNC_ANSWER_ERROR ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], connectionParams.mask );
		}
			break;

		case KBIDX_ETHDNS:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if ( writingSmsNewParams )
				{
					if ( connectionParams_copy.eth0mode == ETH_STATIC )
					{
						//checking for valid ip address
						struct sockaddr_in sa;
						if ( inet_pton(AF_INET, messagePart[MPIDX_PRM1], &(sa.sin_addr)) == 1 )
						{
							//ip addres is valid
							strncpy( (char *)connectionParams_copy.dns1 , messagePart[MPIDX_PRM1], IP_SIZE );
							connectionParams_copy.ethUpdateFlag = 1 ;
						}
						else
							S_SNC_ANSWER_ERROR ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], connectionParams.dns1 );

		}
			break;

		case KBIDX_STARTDTS:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if (writingSmsNewParams)
				{
					int dts_dd  = 0;
					int dts_mm  = 0;
					int dts_yyyy = 0;
					int dts_hh = 0;
					int dts_min = 0;

					if ( sscanf(messagePart[MPIDX_PRM1], "%d-%d-%d %d:%d", &dts_dd , &dts_mm , &dts_yyyy , &dts_hh , &dts_min) == 5 )
					{
						if ( COMMON_CheckDtsForCorrect(dts_dd,dts_mm,dts_yyyy,dts_hh,dts_min) == OK )
						{
							switch(connectionParams_copy.mode)
							{
								case CONNECTION_CSD_OUTGOING:
								case CONNECTION_GPRS_CALENDAR:
									connectionParams_copy.calendarGsm.start_year = (dts_yyyy - 1900) % 100 ;
									connectionParams_copy.calendarGsm.start_month = dts_mm ;
									connectionParams_copy.calendarGsm.start_mday = dts_dd ;
									connectionParams_copy.calendarGsm.start_hour = dts_hh ;
									connectionParams_copy.calendarGsm.start_min = dts_min ;
									connectionParams_copy.gsmUpdateFlag = 1;
									S_SNC_ANSWER_OK ;
									break;
								case CONNECTION_ETH_CALENDAR:
									connectionParams_copy.calendarEth.start_year = (dts_yyyy - 1900) % 100 ;
									connectionParams_copy.calendarEth.start_month = dts_mm ;
									connectionParams_copy.calendarEth.start_mday = dts_dd ;
									connectionParams_copy.calendarEth.start_hour = dts_hh ;
									connectionParams_copy.calendarEth.start_min = dts_min ;
									connectionParams_copy.ethUpdateFlag = 1;
									S_SNC_ANSWER_OK ;
									break;
								default:
									S_SNC_ANSWER_ERROR ;
									break;
							}
						}
						else
							S_SNC_ANSWER_ERROR ;
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				switch( connectionParams.mode )
				{
					case CONNECTION_CSD_OUTGOING:
					case CONNECTION_GPRS_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d-%d-%d %d:%d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarGsm.start_mday, \
								  connectionParams.calendarGsm.start_month, \
								  connectionParams.calendarGsm.start_year + 2000, \
								  connectionParams.calendarGsm.start_hour,
								  connectionParams.calendarGsm.start_min ) ;
						break;

					case CONNECTION_ETH_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d-%d-%d %d:%d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarEth.start_mday, \
								  connectionParams.calendarEth.start_month, \
								  connectionParams.calendarEth.start_year + 2000, \
								  connectionParams.calendarEth.start_hour,
								  connectionParams.calendarEth.start_min ) ;
						break;

					default:
						S_SNC_ANSWER_ERROR ;
						break;
				}
			}
		}
			break;

		case KBIDX_INTVAL:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if (writingSmsNewParams)
				{
					int interval = atoi( messagePart[MPIDX_PRM1] );
					if ( interval > 0 )
					{
						switch( connectionParams_copy.mode )
						{
							case CONNECTION_CSD_OUTGOING:
							case CONNECTION_GPRS_CALENDAR:
								connectionParams_copy.calendarGsm.interval = interval ;
								connectionParams_copy.gsmUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							case CONNECTION_ETH_CALENDAR:
								connectionParams_copy.calendarGsm.interval = interval ;
								connectionParams_copy.ethUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							default:
								S_SNC_ANSWER_ERROR ;
								break;
						}
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				switch( connectionParams.mode )
				{
					case CONNECTION_CSD_OUTGOING:
					case CONNECTION_GPRS_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %ld", keywordBank[keywordBankIdx], \
								  connectionParams.calendarGsm.interval ) ;
						break;

					case CONNECTION_ETH_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %ld", keywordBank[keywordBankIdx], \
								  connectionParams.calendarEth.interval ) ;
						break;

					default:
						S_SNC_ANSWER_ERROR ;
						break;
				}
			}
		}
			break;

		case KBIDX_INTTYPE:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if (writingSmsNewParams)
				{
					int ratio = atoi(messagePart[MPIDX_PRM1]);
					if ( (ratio >= 0) && (ratio <=2) )
					{
						switch( connectionParams_copy.mode )
						{
							case CONNECTION_CSD_OUTGOING:
							case CONNECTION_GPRS_CALENDAR:
								connectionParams_copy.calendarGsm.ratio = ratio ;
								connectionParams_copy.gsmUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							case CONNECTION_ETH_CALENDAR:
								connectionParams_copy.calendarGsm.interval = ratio ;
								connectionParams_copy.ethUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							default:
								S_SNC_ANSWER_ERROR ;
								break;
						}
					}
					else
						S_SNC_ANSWER_ERROR ;
				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				switch( connectionParams.mode )
				{
					case CONNECTION_CSD_OUTGOING:
					case CONNECTION_GPRS_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarGsm.ratio ) ;
						break;

					case CONNECTION_ETH_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarEth.ratio ) ;
						break;

					default:
						S_SNC_ANSWER_ERROR ;
						break;
				}
			}
		}
			break;

		case KBIDX_STOPFLG:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if (writingSmsNewParams)
				{
					int ifstoptime = atoi(messagePart[MPIDX_PRM1]);
					if ( (ifstoptime == 1) && (ifstoptime == 0) )
					{
						switch( connectionParams_copy.mode )
						{
							case CONNECTION_CSD_OUTGOING:
							case CONNECTION_GPRS_CALENDAR:
								connectionParams_copy.calendarGsm.ifstoptime = ifstoptime ;
								connectionParams_copy.gsmUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							case CONNECTION_ETH_CALENDAR:
								connectionParams_copy.calendarGsm.ifstoptime = ifstoptime ;
								connectionParams_copy.ethUpdateFlag = 1;
								S_SNC_ANSWER_OK ;
								break;

							default:
								S_SNC_ANSWER_ERROR ;
								break;
						}
					}

				}
				else
					S_SNC_ANSWER_FORBID ;
			}
			else
			{
				switch( connectionParams.mode )
				{
					case CONNECTION_CSD_OUTGOING:
					case CONNECTION_GPRS_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarGsm.ifstoptime ) ;
						break;

					case CONNECTION_ETH_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarEth.ifstoptime ) ;
						break;

					default:
						S_SNC_ANSWER_ERROR ;
						break;
				}
			}
		}
			break;

		case KBIDX_WORKTIME:
		{
			if ( strlen( messagePart[MPIDX_PRM1] ) )
			{
				if (writingSmsNewParams)
				{
					int hh = 0 , mm = 0 ;
					if ( sscanf(messagePart[MPIDX_PRM1], "%d:%d" , &hh , &mm ) == 2 )
					{
						if (((( 60 * hh ) + mm) != 0) && (mm>=0) && (mm<60) && (hh>=0))
						{
							switch( connectionParams_copy.mode )
							{
								case CONNECTION_CSD_OUTGOING:
								case CONNECTION_GPRS_CALENDAR:
									connectionParams_copy.calendarGsm.stop_hour = hh ;
									connectionParams_copy.calendarGsm.stop_min = mm ;
									connectionParams_copy.gsmUpdateFlag = 1;
									S_SNC_ANSWER_OK ;
									break;

								case CONNECTION_ETH_CALENDAR:
									connectionParams_copy.calendarEth.stop_hour = hh ;
									connectionParams_copy.calendarEth.stop_min = mm ;
									connectionParams_copy.ethUpdateFlag = 1;
									S_SNC_ANSWER_OK ;
									break;

								default:
									S_SNC_ANSWER_ERROR ;
									break;
							}
						}
						else
							S_SNC_ANSWER_ERROR;
					}
					else
						S_SNC_ANSWER_ERROR;
				}
				else
					S_SNC_ANSWER_FORBID;
			}
			else
			{
				switch( connectionParams.mode )
				{
					case CONNECTION_CSD_OUTGOING:
					case CONNECTION_GPRS_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d:%d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarGsm.stop_hour , connectionParams.calendarGsm.stop_min ) ;
						break;

					case CONNECTION_ETH_CALENDAR:
						snprintf( answer , SMS_TEXT_LENGTH , "%s %d:%d", keywordBank[keywordBankIdx], \
								  connectionParams.calendarEth.stop_hour , connectionParams.calendarEth.stop_min ) ;
						break;

					default:
						S_SNC_ANSWER_ERROR ;
						break;
				}
			}
		}
			break;

		case KBIDX_SETPWD:
		{
			char pass[128] = {0};
			COMMON_GetWebUIPass((unsigned char *)pass);

			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );
			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM2] );

			if (!strcmp( pass , messagePart[MPIDX_PRM2] ) || !strcmp( messagePart[MPIDX_PRM2] , STORAGE_DEFPSWD))
			{
				if (COMMON_SetWebUIPass((unsigned char *)messagePart[MPIDX_PRM1] ) == OK )					
					S_SNC_ANSWER_OK ;
				else
					S_SNC_ANSWER_ERROR ;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_REBOOT:
		{			
			char pass[128] = {0};
			COMMON_GetWebUIPass((unsigned char *)pass);
			COMMON_DecodeSmsSpecialChars( messagePart[MPIDX_PRM1] );

			if (!strcmp( pass , messagePart[MPIDX_PRM1] ) || !strcmp( messagePart[MPIDX_PRM1] , STORAGE_DEFPSWD))
			{
				STORAGE_JournalUspdEvent( EVENT_USPD_REBOOT , (unsigned char *)DESC_EVENT_REBOOT_BY_SMS );
				COMMON_Sleep(SECOND * 5);
				PIO_RebootUspd();
				SHELL_CMD_REBOOT;
				S_SNC_ANSWER_ERROR ;
			}
			else
				S_SNC_ANSWER_FORBID ;
		}
			break;

		case KBIDX_GETSNSQ:
		{
			if ( !strlen( messagePart[MPIDX_PRM1] ) )
			{
				int sq = GSM_GetSignalQuality();
				unsigned char sn[32] = {0};
				COMMON_GET_USPD_SN(sn);
				snprintf( answer , SMS_TEXT_LENGTH , "%s %s SQ %d%%", keywordBank[keywordBankIdx] , sn , sq );
			}
			else
				S_SNC_ANSWER_ERROR ;
		}
			break;

		case KBIDX_GETVER:
		{
			if ( !strlen( messagePart[MPIDX_PRM1] ) )
			{
				char ver[STORAGE_VERSION_LENGTH] = {0};
				if ( STORAGE_GetFirmwareVersion( ver ) == OK )
				{
					snprintf( answer , SMS_TEXT_LENGTH , "%s %s", keywordBank[keywordBankIdx], ver );
				}
				else
					S_SNC_ANSWER_ERROR ;
			}
			else
				S_SNC_ANSWER_ERROR ;

		}
			break;

		case KBIDX_NOKEYWORD:
			break;

		default:
			break;
	}


//#undef S_SNC_ANSWER_OK
//#undef S_SNC_ANSWER_ERROR
//#undef S_SNC_ANSWER_FORBID

	S_SNC_EXIT:

	free(cp_ptr);
	return ;
}

//
//
//

int STORAGE_GetFirmwareVersion( char * version )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_GetFirmwareVersion start" );

	int res = ERROR_GENERAL ;

	if ( !version )
	{
		return res ;
	}

	STORAGE_Protect();

	int fd = open( STORAGE_PATH_USPD_VERSION , O_RDONLY );
	if ( fd > 0 )
	{
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);

		if( fd_stat.st_size > 0 )
		{
			int bufferLength = fd_stat.st_size ;
			char * buffer = malloc( bufferLength + 1 );
			if ( buffer )
			{
				memset(buffer , 0 , bufferLength + 1 );

				if ( read( fd , &buffer[0] , bufferLength) == bufferLength )
				{
					//try to parse buffer
					char * token[ STORAGE_MAX_TOKEN_NUMB ];
					int tokenCounter = 0;

					char * pch;
					char * lastPtr = NULL ;
					pch=strtok_r(buffer, " :\r\n\t\v" , &lastPtr);
					while (pch!=NULL)
					{
						token[tokenCounter++]=pch;
						pch=strtok_r(NULL, " :\r\n\t\v" , &lastPtr);
						if (tokenCounter == STORAGE_MAX_TOKEN_NUMB)
						{
							break;
						}
					}

					int tokenIndex = 0 ;
					for(tokenIndex = 0 ; tokenIndex < (tokenCounter - 1) ; tokenIndex++ )
					{
						if ( !strcmp( token[tokenIndex] , "version" ) )
						{
							snprintf( version , STORAGE_VERSION_LENGTH , "%s" , token[tokenIndex + 1]);
							res = OK;
							break;
						}
					}
				}
				free(buffer);
				buffer = NULL ;
			}
		}
		close( fd );
	}

	STORAGE_Unprotect();

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_GetFirmwareVersion return %s" , getErrorCode(res) );
	return res ;
}

//
//
//

int STORAGE_UpdateImsi( int sim , char * imsi , BOOL * needToSendSms)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	(*needToSendSms) = TRUE ;

	const int strTempLength = 64 ;
	char strTemp[ strTempLength ] ;
	memset( strTemp , 0 , strTempLength );

	int r = 0 ;

	char path[64];
	memset( path , 0 , 64 );
	snprintf( path , 64 , "%s_%d" , STORAGE_PATH_IMSI , sim );

	int fd = open( path , O_RDWR | O_CREAT , 0777 );
	if ( fd > 0 )
	{

		if ( COMMON_ReadLn(fd, (char *)strTemp, strTempLength, &r) == OK )
		{
			if ( !strcmp( strTemp , imsi ) )
			{
				(*needToSendSms) = FALSE ;
			}
		}

		if ( (*needToSendSms) == TRUE )
		{
			lseek(fd , 0 , SEEK_SET);
			ftruncate(fd , 0);
			memset( strTemp , 0 , strTempLength );
			snprintf( strTemp , strTempLength , "%s\n" , imsi );

			write( fd , strTemp , strlen(strTemp) );
		}

		res = OK ;
		close(fd);
	}

	return res ;

}

//
//
//

int STORAGE_TranslateFirmwareVersion(char * version, unsigned int * ver_)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	if ( version && ver_ )
	{
		if ( version[0] == 'v' )
		{
			char * dotPosition = strstr( version , "." );
			if ( dotPosition )
			{
				dotPosition[0] = 0;
				unsigned char firstByte = (unsigned char)atoi( &version[1] ) ;
				unsigned char secByte = (unsigned char)atoi( &dotPosition[1] ) ;
				(*ver_) = (( firstByte << 8 ) & 0xFF00 ) | ( secByte & 0xFF );
				res = OK ;
			}
		}
	}
	return res;
}

//
//
//

int STORAGE_LoadConnections() {
	DEB_TRACE(DEB_IDX_STORAGE)

/*
/home/root/cfg/connection.cfg

[connect_id]
Ethernet
[server_link]
192.168.0.225
[port]
1111
[obj_name]
Название объекта
[obj_place]
установки


/home/root/cfg/ethernet.cfg

[getIP_id]
getIP_manual
[ip_address]
IPaddress
[ip_mask]
IPmask
[getway]
Gateway
[dns_1]
DNS1
[dns_2]
DNS2
[connection_id]
schedule
[startDate]
2012-01-06
[startTimeHour]
4
[startTimeMin]
8
[intevalOffset]
4
[intervalType]
1
[finishedOn]

[stopTimeHour]

[stopTimeMin]


/home/root/cfg/gsm.cfg
[sim1_apn]
mts.internet.ru
[sim1_username]
mts
[sim1_password]
mts
[sim2_apn]
beeline.internet.ru
[sim2_username]
beeline
[sim2_password]
beeline
[sim_1_allowed_csd_number_1]
123456789
[sim_1_allowed_csd_number_2]
123456789
[sim_2_allowed_csd_number_1]
123456789
[sim_2_allowed_csd_number_2]
1234567888
[sim_1_allowed_csd_number_3]
1111111
[sim_2_allowed_csd_number_3]
2222222
[sim_change_tries]
5
[connection_id]
2
[startDate]
2012-02-04
[startTimeHour]
11
[startTimeMin]
12
[intevalOffset]
6
[intervalType]
0
[finishedOn]
1
[stopTimeHour]
14
[stopTimeMin]
3

*/

	int res = ERROR_FILE_OPEN_ERROR;
	STORAGE_Protect();

	memset(&connectionParams, 0, sizeof (connection_t));

	memset(&objNameUndPlace, 0 , sizeof(objNameUndPlace_t) );
	//read connection parameters from fileS
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE : Try to open config files...");


	int connPr_fd , ethPr_fd , gsmPr_fd , hwPr_fd;

	connPr_fd = open( CONFIG_PATH_CONNECTION , O_RDONLY) ;
	if (connPr_fd < 0)
		connPr_fd = open( CONFIG_PATH_CONNECTION ".bak", O_RDONLY) ;

	ethPr_fd = open( CONFIG_PATH_ETH , O_RDONLY) ;
	if(ethPr_fd < 0)
		ethPr_fd = open( CONFIG_PATH_ETH ".bak" , O_RDONLY) ;

	gsmPr_fd = open( CONFIG_PATH_GSM , O_RDONLY) ;
	if(gsmPr_fd < 0)
		gsmPr_fd = open( CONFIG_PATH_GSM ".bak", O_RDONLY) ;

	hwPr_fd = open ( ETH_MAC_CFG_PATH , O_RDONLY );
	if( hwPr_fd < 0 )
		hwPr_fd = open ( ETH_MAC_CFG_PATH ".bak", O_RDONLY );

	BOOL macReadingSuccessFlag = FALSE ;
	if ( hwPr_fd >=0 )
	{
		CLI_SetErrorCode(CLI_NO_CONFIG_MAC, CLI_CLEAR_ERROR);
		unsigned char * macBuf = NULL;
		struct stat macStat;
		memset(&macStat , 0 , sizeof(stat));
		fstat(hwPr_fd , &macStat);

		int macBufSize = macStat.st_size ;

		macBuf = malloc( macBufSize*sizeof(unsigned char) );
		if (macBuf != NULL)
		{
			if ( read(hwPr_fd , macBuf , macBufSize) == macBufSize )
			{

				unsigned char * pch;
				char * lastPtr = NULL ;
				unsigned char * macToken[STORAGE_MAX_TOKEN_NUMB];
				int macTokenCounter = 0;

				pch = (unsigned char *)strtok_r( (char *)macBuf , " \r\n" , &lastPtr);
				while(pch!=NULL)
				{
					macToken[macTokenCounter++] = pch ;
					pch = (unsigned char *)strtok_r(NULL , " \r\n" , &lastPtr);
					if(macTokenCounter == STORAGE_MAX_TOKEN_NUMB)
						break;
				}

				int doublePointCounter = 0;
				int i;
				for( i=0 ; i < (int)strlen((char *)macToken[0]); i++)
				{
					if( (macToken[0])[i] == ':' )
						doublePointCounter++;
				}

				if((doublePointCounter == 5) && (strlen((char *)macToken[0]) == ETH_HW_SIZE ))
				{
					memcpy( connectionParams.hwaddress , macToken[0] , strlen((char *)macToken[0])  );
					macReadingSuccessFlag = TRUE;
				}


			}

			free(macBuf);
		}

		close(hwPr_fd);
	}
	else
		CLI_SetErrorCode(CLI_NO_CONFIG_MAC, CLI_SET_ERROR);

	if ( macReadingSuccessFlag == FALSE)
	{
		// using current hwaddress
		COMMON_GET_USPD_MAC( (char *)connectionParams.hwaddress );
	}


	if ( (connPr_fd >= 0) && (ethPr_fd >= 0) && (gsmPr_fd >= 0))
	{
		CLI_SetErrorCode(CLI_NO_CONFIG_CONN, CLI_CLEAR_ERROR);
		CLI_SetErrorCode(CLI_NO_CONFIG_ETH, CLI_CLEAR_ERROR);
		CLI_SetErrorCode(CLI_NO_CONFIG_GSM, CLI_CLEAR_ERROR);


		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: config file are opening. Reading them to the buffers");
		unsigned char connBuf[MAX_MESSAGE_LENGTH];
		unsigned char ethBuf[MAX_MESSAGE_LENGTH];
		unsigned char gsmBuf[MAX_MESSAGE_LENGTH];
		memset(connBuf , 0 , MAX_MESSAGE_LENGTH);
		memset(ethBuf , 0 , MAX_MESSAGE_LENGTH);
		memset(gsmBuf , 0 , MAX_MESSAGE_LENGTH);

		if (    (read( connPr_fd , connBuf , MAX_MESSAGE_LENGTH ) > 0 ) && \
				(read( ethPr_fd , ethBuf , MAX_MESSAGE_LENGTH ) > 0 ) && \
				(read( gsmPr_fd , gsmBuf , MAX_MESSAGE_LENGTH ) > 0 )  )
		{
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: Start parsing buffers" );
			unsigned char * connToken[STORAGE_MAX_TOKEN_NUMB];
			unsigned char * ethToken[STORAGE_MAX_TOKEN_NUMB];
			unsigned char * gsmToken[STORAGE_MAX_TOKEN_NUMB];

			int connTokenCounter = 0;
			int ethTokenCounter = 0 ;
			int gsmTokenCounter = 0 ;

			unsigned char * pch;
			char * lastPtr = NULL ;

			pch=(unsigned char *)strtok_r((char *)connBuf, "\r\n" , &lastPtr);
			while (pch!=NULL)
			{
				connToken[connTokenCounter]=pch;
				pch=(unsigned char *)strtok_r(NULL, "\r\n" , &lastPtr);
				connTokenCounter++;
				if(connTokenCounter == STORAGE_MAX_TOKEN_NUMB)
					break;
			}

			pch=(unsigned char *)strtok_r((char *)ethBuf, "\r\n" , &lastPtr);
			while (pch!=NULL)
			{
				ethToken[ethTokenCounter]=pch;
				pch=(unsigned char *)strtok_r(NULL, "\r\n" , &lastPtr);
				ethTokenCounter++;
				if(ethTokenCounter == STORAGE_MAX_TOKEN_NUMB)
					break;
			}

			pch=(unsigned char *)strtok_r((char *)gsmBuf, "\r\n" , &lastPtr);
			while (pch!=NULL)
			{
				gsmToken[gsmTokenCounter]=pch;
				pch=(unsigned char *)strtok_r(NULL, "\r\n" , &lastPtr);
				gsmTokenCounter++;
				if(gsmTokenCounter == STORAGE_MAX_TOKEN_NUMB)
					break;
			}

			int idx;

			//start parsing buffers
			//#define CONNECT_ID_ETHERNET 0
			//#define CONNECT_ID_GSM 1

			enum
			{
				CONNECT_ID_ETHERNET = 0 ,
				CONNECT_ID_GSM
			};

			int connectId = 0 ;


			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: Parsing connection.cfg " );
			// parsing connection.cfg
			for( idx = 0 ; idx < connTokenCounter  ; idx++ )
			{
				if ( (strstr( (char *)connToken[idx] , "[connect_id]" ) !=NULL) && (idx != (connTokenCounter - 1) ) )
				{
					if ( strstr( (char *)connToken[idx + 1] , "[" ) != (char*)connToken[idx + 1]  )
					{
						if (strstr( (char *)connToken[idx + 1] , "Ethernet") != NULL)
							connectId = CONNECT_ID_ETHERNET ;
						else
							connectId = CONNECT_ID_GSM ;
					}
				}

				else if ( (strstr( (char *)connToken[idx] , "[server_link]" ) !=NULL) && (idx != (connTokenCounter - 1) ) )
				{
					if ( strstr( (char *)connToken[idx + 1] , "[" ) != (char*)connToken[idx + 1]  )
					{
						memcpy( connectionParams.server , connToken[idx + 1 ] , strlen((char *)connToken[idx + 1 ]) );
					}
				}

				else if ( (strstr( (char *)connToken[idx] , "[port]" ) !=NULL) && (idx != (connTokenCounter - 1) ) )
				{
					if ( strstr( (char *)connToken[idx + 1] , "[" ) != (char*)connToken[idx + 1]  )
					{
						connectionParams.port = atoi((char *)connToken[idx + 1]);
					}
				}

				else if ( (strstr( (char *)connToken[idx] , "[obj_name]" ) !=NULL) && (idx != (connTokenCounter - 1) ) )
				{
					if ( strstr( (char *)connToken[idx + 1] , "[" ) != (char*)connToken[idx + 1]  )
					{
						memcpy( objNameUndPlace.objName , connToken[idx + 1 ] , strlen((char *)connToken[idx + 1 ]) );
					}
				}

				else if ( (strstr( (char *)connToken[idx] , "[obj_place]" ) !=NULL) && (idx != (connTokenCounter - 1) ) )
				{
					if ( strstr( (char *)connToken[idx + 1] , "[" ) != (char*)connToken[idx + 1]  )
					{
						memcpy( objNameUndPlace.objPlace , connToken[idx + 1 ] , strlen((char *)connToken[idx + 1 ]) );
					}
				}



			}

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: Parsing ethernet.cfg " );
			//parsing ethernet.cfg

			for( idx = 0 ; idx < ethTokenCounter ; idx++ )
			{

				if ( (strstr( (char *)ethToken[idx] , "[getIP_id]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						if (strstr((char *)ethToken[idx + 1] , "manual") != NULL)
							connectionParams.eth0mode = ETH_STATIC ;
						else
							connectionParams.eth0mode = ETH_DHCP ;
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[ip_address]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						memcpy(connectionParams.myIp , ethToken[idx + 1] , strlen((char *)ethToken[idx + 1]));
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[ip_mask]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						memcpy(connectionParams.mask , ethToken[idx + 1] , strlen((char *)ethToken[idx + 1]));
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[getway]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						memcpy(connectionParams.gateway , ethToken[idx + 1] , strlen((char *)ethToken[idx + 1]));
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[dns_1]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						memcpy(connectionParams.dns1 , ethToken[idx + 1] , strlen((char *)ethToken[idx + 1]));
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[dns_2]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						memcpy(connectionParams.dns2 , ethToken[idx + 1] , strlen((char *)ethToken[idx + 1]));
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[connection_id]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						if ( connectId == CONNECT_ID_ETHERNET)
						{
							if ( strstr((char *)ethToken[idx + 1] , "always")!=NULL )
								connectionParams.mode = CONNECTION_ETH_ALWAYS;
							else if ( strstr((char *)ethToken[idx + 1] , "serv")!=NULL )
								connectionParams.mode = CONNECTION_ETH_SERVER;
							else
								connectionParams.mode = CONNECTION_ETH_CALENDAR;
						}
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[startDate]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						sscanf( (char *)ethToken[idx+1], "%d-%d-%d", &connectionParams.calendarEth.start_year , \
															   &connectionParams.calendarEth.start_month ,\
															   &connectionParams.calendarEth.start_mday );
						connectionParams.calendarEth.start_year=(connectionParams.calendarEth.start_year - 1900) % 100 ;
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[startTimeHour]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						connectionParams.calendarEth.start_hour=atoi((char *)ethToken[idx+1]);
						if (connectionParams.calendarEth.start_hour <0)
							connectionParams.calendarEth.start_hour = 0;
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[startTimeMin]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						connectionParams.calendarEth.start_min=atoi((char *)ethToken[idx+1]);
						if (connectionParams.calendarEth.start_min < 0)
							connectionParams.calendarEth.start_min = 0;
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "Offset]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						connectionParams.calendarEth.interval=atoi((char *)ethToken[idx+1]);
						if (connectionParams.calendarEth.interval <0)
							connectionParams.calendarEth.interval = 0;
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "Type]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						switch( atoi((char *)ethToken[idx+1]))
						{
							case 1:
								connectionParams.calendarEth.ratio = CONNECTION_INTERVAL_DAY;
								break;

							case 2:
								connectionParams.calendarEth.ratio = CONNECTION_INTERVAL_WEEK;
								break;

							default:
								connectionParams.calendarEth.ratio = CONNECTION_INTERVAL_HOUR;
								break;
						}
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[finishedOn]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						switch( atoi((char *)ethToken[idx+1]))
						{
							case 0:
								connectionParams.calendarEth.ifstoptime = FALSE;
								break;
							default:
								connectionParams.calendarEth.ifstoptime = TRUE;
								break;
						}
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[stopTimeHour]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						connectionParams.calendarEth.stop_hour = atoi((char *)ethToken[idx + 1]);
					}
				}

				else if ( (strstr( (char *)ethToken[idx] , "[stopTimeMin]" ) !=NULL) && (idx != (ethTokenCounter - 1) ) )
				{
					if ( strstr( (char *)ethToken[idx + 1] , "[" ) != (char*)ethToken[idx + 1]  )
					{
						connectionParams.calendarEth.stop_min = atoi((char *)ethToken[idx + 1]);
					}
				}


			}

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: Parsing gsm.cfg");
			for( idx = 0 ; idx < gsmTokenCounter ; idx++ )
			{
				if ( (strstr( (char *)gsmToken[idx] , "[sim1_apn]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn1 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim1_username]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn1_user , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim1_password]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn1_pass , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim2_apn]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn2 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim2_username]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn2_user , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim2_password]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.apn2_pass , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_1_allowed_csd_number_1]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.allowedIncomingNum1Sim1 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_1_allowed_csd_number_2]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.allowedIncomingNum2Sim1 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_2_allowed_csd_number_1]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.allowedIncomingNum1Sim2 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_2_allowed_csd_number_2]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.allowedIncomingNum2Sim2 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_1_allowed_csd_number_3]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.serverPhoneSim1 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_2_allowed_csd_number_3]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						memcpy( connectionParams.serverPhoneSim2 , gsmToken[idx+1] , strlen((char *)gsmToken[idx+1]) );
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[sim_change_tries]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.simTryes = atoi( (char *)gsmToken[idx+1] );
						if ( connectionParams.simTryes <= 0 )
							connectionParams.simTryes = 1;
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[connection_id]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						if ( connectId == CONNECT_ID_GSM )
						{
//							if ( strstr( (char *)gsmToken[idx + 1] , "always_gprs" ) != NULL)
//							{
//								connectionParams.mode = CONNECTION_GPRS_ALWAYS ;
//							}
//							else if( strstr( (char *)gsmToken[idx + 1] , "ing_call_csd" ) != NULL)
//							{
//								connectionParams.mode = CONNECTION_CSD_INCOMING ;
//							}
//							else if( strstr( (char *)gsmToken[idx + 1] , "schedule_gprs" ) != NULL)
//							{
//								connectionParams.mode = CONNECTION_GPRS_CALENDAR ;
//							}
//							else if( strstr( (char *)gsmToken[idx + 1] , "schedule_csd" ) != NULL)
//							{
//								connectionParams.mode = CONNECTION_CSD_OUTGOING ;
//							}
//							else
//							{
								switch(atoi((char *)gsmToken[idx + 1]))
								{
									case 1:
										connectionParams.mode = CONNECTION_CSD_INCOMING ;
										break;

									case 2:
										connectionParams.mode = CONNECTION_GPRS_CALENDAR ;
										break;

									case 3:
										connectionParams.mode = CONNECTION_CSD_OUTGOING ;
										break;

									case 4:
										connectionParams.mode = CONNECTION_GPRS_SERVER;
										break;

									default:
										connectionParams.mode = CONNECTION_GPRS_ALWAYS ;
										break;
								}
							//}

						}

					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[startDate]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						sscanf( (char *)gsmToken[idx + 1] , "%d-%d-%d",   &connectionParams.calendarGsm.start_year , \
																	&connectionParams.calendarGsm.start_month , \
																	&connectionParams.calendarGsm.start_mday );
						connectionParams.calendarGsm.start_year = ( connectionParams.calendarGsm.start_year - 1900 ) % 100 ;

					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[startTimeHour]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.calendarGsm.start_hour = atoi((char *)gsmToken[idx+1]);
						if ( connectionParams.calendarGsm.start_hour < 0 )
							connectionParams.calendarGsm.start_hour = 0 ;
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[startTimeMin]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.calendarGsm.start_min = atoi((char *)gsmToken[idx+1]);
						if ( connectionParams.calendarGsm.start_min < 0 )
							connectionParams.calendarGsm.start_min = 0 ;
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "Offset]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.calendarGsm.interval = atoi((char *)gsmToken[idx+1]);
						if(connectionParams.calendarGsm.interval < 0)
							connectionParams.calendarGsm.interval = 0;
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "Type]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						switch(atoi((char *)gsmToken[idx+1]))
						{
							case 1:
								connectionParams.calendarGsm.ratio = CONNECTION_INTERVAL_DAY ;
								break;

							case 2:
								connectionParams.calendarGsm.ratio = CONNECTION_INTERVAL_WEEK ;
								break;

							default:
								connectionParams.calendarGsm.ratio = CONNECTION_INTERVAL_HOUR ;
								break;
						}
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[finishedOn]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
					   switch( atoi((char *)gsmToken[idx+1]) )
					   {
						   case 1:
							   connectionParams.calendarGsm.ifstoptime = TRUE;
							   break;

						   default:
							   connectionParams.calendarGsm.ifstoptime = FALSE;
							   break;
					   }
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[stopTimeHour]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.calendarGsm.stop_hour = atoi((char *)gsmToken[idx+1]);
						if(connectionParams.calendarGsm.stop_hour < 0)
							connectionParams.calendarGsm.stop_hour = 0;
					}
				}

				else if ( (strstr( (char *)gsmToken[idx] , "[stopTimeMin]" ) !=NULL) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						connectionParams.calendarGsm.stop_min = atoi((char *)gsmToken[idx+1]);
						if(connectionParams.calendarGsm.stop_min < 0)
							connectionParams.calendarGsm.stop_min = 0;
					}
				}

				else if ( ( (strstr( (char *)gsmToken[idx] , "rity]" ) !=NULL) || (strstr( (char *)gsmToken[idx] , "ciple]" ) !=NULL)  ) && (idx != (gsmTokenCounter - 1) ) )
				{
					if ( strstr( (char *)gsmToken[idx + 1] , "[" ) != (char*)gsmToken[idx + 1]  )
					{
						switch( atoi((char *)gsmToken[idx + 1]) )
						{
							case 1:
								connectionParams.simPriniciple = SIM_FIRST ;
								break;
							case 2:
								connectionParams.simPriniciple = SIM_SECOND ;
								break;
							case 21:
								connectionParams.simPriniciple = SIM_SECOND_THEN_FIRST ;
								break;
							default:
								connectionParams.simPriniciple = SIM_FIRST_THEN_SECOND ;
								break;
						}

					}
				}


			}

			res = OK;

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: struct connectionParams have next fields:\n"\
									"\tsimTryes=%d\n\tserver=%s\n\tport=%d\n\tapn1=%s\n\tapn1_pass=%s\n\tapn1_user=%s\n\tapn2=%s\n\tapn2_pass=%s\n\tapn2_user=%s\n\t"\
									"allowedIncomingNum1Sim1=%s\n\tallowedIncomingNum2Sim1=%s\n\tallowedIncomingNum1Sim2=%s\n\tallowedIncomingNum2Sim2=%s\n\t"\
									"serverPhoneSim1=%s\n\tserverPhoneSim2=%s\n\tsimPriniciple=%d\n\tmode=%d\n\tprotocol=%d\n\tmyIp=%s\n\tgateway=%s\n\t"\
									"mask=%s\n\tdns1=%s\n\tdns2=%s\n\teth0mode=%d\n\thwaddr=%s",\
									connectionParams.simTryes , connectionParams.server , connectionParams.port , connectionParams.apn1 ,\
									connectionParams.apn1_pass , connectionParams.apn1_user , connectionParams.apn2 , connectionParams.apn2_pass ,\
									connectionParams.apn2_user , connectionParams.allowedIncomingNum1Sim1 , connectionParams.allowedIncomingNum2Sim1 ,\
									connectionParams.allowedIncomingNum1Sim2 , connectionParams.allowedIncomingNum2Sim2 , \
									connectionParams.serverPhoneSim1 , connectionParams.serverPhoneSim2 , connectionParams.simPriniciple ,\
									connectionParams.mode , connectionParams.protocol , connectionParams.myIp , connectionParams.gateway ,\
									connectionParams.mask , connectionParams.dns1 , connectionParams.dns2 , connectionParams.eth0mode , connectionParams.hwaddress);
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE: struct objNameUndPlace have next fields:\n\t"\
											"objName = %s\n\tobjPlace = %s", objNameUndPlace.objName , objNameUndPlace.objPlace );
		}

		//debug
		if ( connectionParams.simPriniciple == 0 )
			connectionParams.simPriniciple = SIM_FIRST_THEN_SECOND ;

		close(connPr_fd);
		close(gsmPr_fd);
		close(ethPr_fd);
	}
	else if (connPr_fd < 0)
	{
		CLI_SetErrorCode(CLI_NO_CONFIG_CONN, CLI_SET_ERROR);
	}
	else if (ethPr_fd < 0 )
	{
		CLI_SetErrorCode(CLI_NO_CONFIG_ETH, CLI_SET_ERROR);
	}
	else if (gsmPr_fd < 0)
	{
		CLI_SetErrorCode(CLI_NO_CONFIG_GSM, CLI_SET_ERROR);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadConnections() %s", getErrorCode(res));

	STORAGE_Unprotect();
	STORAGE_LoadServiceConnection();
	STORAGE_LoadCpin();

	return res;
}

//
//
//

int STORAGE_LoadCpin()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	STORAGE_Protect();

	const unsigned char sim1_pin = 0x01;
	const unsigned char sim1_use = 0x02;
	const unsigned char sim2_pin = 0x04;
	const unsigned char sim2_use = 0x08;
	unsigned char readyMask = 0;

	unsigned char * buffer = NULL ;
	int bufferLength = 0 ;
	if ( COMMON_GetFileFromFS( &buffer , &bufferLength , (unsigned char *)CONFIG_PATH_CPIN) == OK )
	{
		if ( buffer )
		{
			COMMON_RemoveComments((char *)buffer , &bufferLength , '#');

			blockParsingResult_t * searchResult = NULL ;
			int searchResultLength = 0 ;
			if ( COMMON_SearchBlock( (char *)buffer , bufferLength , "sim1" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "pin") )
						{
							readyMask |= sim1_pin ;
							connectionParams.gsmCpinProperties[0].pin = atoi( searchResult[ searchResultIndex ].value );
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "use") )
						{
							readyMask |= sim1_use ;
							if ( atoi( searchResult[ searchResultIndex ].value) == 0  )
							{
								 connectionParams.gsmCpinProperties[0].usage = FALSE ;
							}
							else
							{
								connectionParams.gsmCpinProperties[0].usage = TRUE ;
							}
						}
					}
					free( searchResult );
					searchResult = NULL ;
					searchResultLength = 0 ;
				}
			}
			if ( COMMON_SearchBlock( (char *)buffer , bufferLength , "sim2" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "pin") )
						{
							readyMask |= sim2_pin ;
							connectionParams.gsmCpinProperties[1].pin = atoi( searchResult[ searchResultIndex ].value );
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "use") )
						{
							readyMask |= sim2_use ;
							if ( atoi( searchResult[ searchResultIndex ].value ) == 0 )
							{
								 connectionParams.gsmCpinProperties[1].usage = FALSE ;
							}
							else
							{
								connectionParams.gsmCpinProperties[1].usage = TRUE ;
							}
						}
					}
					free( searchResult );
					searchResult = NULL ;
					searchResultLength = 0 ;
				}
			}


		}
	}

	if ( ( readyMask & sim1_pin ) && ( readyMask & sim1_use ) && ( readyMask & sim2_pin ) && ( readyMask & sim2_use ) )
	{
		res = OK ;
	}

	STORAGE_Unprotect();

	return res ;
}

//
//
//

int STORAGE_LoadServiceConnection()
{
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() start");

	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	//set service field by default values
	memset( connectionParams.serviceIp , 0 , IP_SIZE ) ;
	memset( connectionParams.servicePhone , 0 ,PHONE_MAX_SIZE );
	memset( connectionParams.smsReportPhone , 0 ,PHONE_MAX_SIZE );
	connectionParams.smsReportAllow = FALSE ;
	connectionParams.servicePort = 0 ;
	connectionParams.serviceInterval = 0 ;
	connectionParams.serviceRatio = 1 ;

	STORAGE_Protect();

	unsigned char * buffer = NULL ;
	int bufferLength = 0 ;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() try to get data from " CONFIG_PATH_SERVICE );
	if ( COMMON_GetFileFromFS(&buffer , &bufferLength , (unsigned char *)CONFIG_PATH_SERVICE ) == OK )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() getting data success ");
		if ( buffer )
		{
			blockParsingResult_t * searchResult = NULL ;
			int searchResultLength = 0 ;

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() removing comments");
			COMMON_RemoveComments((char *)buffer , &bufferLength , '#');

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() searching for server part");
			if ( COMMON_SearchBlock( (char *)buffer , bufferLength , "server" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "ip") )
						{
							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() ip is [%s]" , searchResult[ searchResultIndex ].value);
							snprintf( (char *)connectionParams.serviceIp , IP_SIZE ,"%s" , searchResult[ searchResultIndex ].value );
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "phone") )
						{
							snprintf( (char *)connectionParams.servicePhone , PHONE_MAX_SIZE ,"%s" , searchResult[ searchResultIndex ].value );
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "port") )
						{
							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() port is [%u]" , atoi(searchResult[ searchResultIndex ].value) );
							connectionParams.servicePort = atoi( searchResult[ searchResultIndex ].value );
						}
					}

					free( searchResult );
					searchResult = NULL ;
					searchResultLength = 0 ;
				}
			}

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() searching for connection properties part");
			if ( COMMON_SearchBlock( (char *)buffer , bufferLength , "connection_properties" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "interval") )
						{
							connectionParams.serviceInterval = atoi( searchResult[ searchResultIndex ].value );
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "ratio") )
						{
							connectionParams.serviceRatio = atoi( searchResult[ searchResultIndex ].value );
						}
					}

					free( searchResult );
					searchResult = NULL ;
					searchResultLength = 0 ;
				}
			}

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() searching for sms-report properties part");
			if ( COMMON_SearchBlock( (char *)buffer , bufferLength , "sms_report" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "allow_report") )
						{
							if ( atoi ( searchResult[ searchResultIndex ].value ) == 1 )
							{
								connectionParams.smsReportAllow = TRUE ;
							}
						}
						else if ( strstr(searchResult[ searchResultIndex ].variable , "phone") )
						{
							snprintf( (char *)connectionParams.smsReportPhone , PHONE_MAX_SIZE ,"%s" , searchResult[ searchResultIndex ].value );
						}
					}

					free( searchResult );
					searchResult = NULL ;
					searchResultLength = 0 ;
				}
			}

			free( buffer );

			if ( (strlen( (char *)connectionParams.serviceIp ) > 0) && (connectionParams.servicePort != 0) )
			{
				res = OK ;
			}
		}
	}

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadServiceConnection() return %s" , getErrorCode(res) );

	return res ;
}

//
//
//

//
//
//

BOOL STORAGE_CheckTimeToServiceConnect()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = TRUE ;

	STORAGE_Protect();

	int fd = open( STORAGE_PATH_SERVICE , O_RDONLY );
	if ( fd )
	{
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);

		if( fd_stat.st_size == sizeof(uspdService_t) )
		{
			uspdService_t tempUspdService;
			memset( &tempUspdService , 0 , sizeof(uspdService_t));

			if ( read( fd , &tempUspdService , sizeof(uspdService_t) ) == sizeof(uspdService_t) )
			{
				unsigned int crc = 0 ;
				if ( STORAGE_GetServiceCRC( &tempUspdService , &crc) == OK )
				{
					if ( crc == tempUspdService.crc )
					{
						dateTimeStamp currentDts;
						COMMON_GetCurrentDts( &currentDts );

						unsigned int needMinuteDiff = 0 ;
						switch( connectionParams.serviceInterval )
						{
							case 0:
							{
								 needMinuteDiff = connectionParams.serviceRatio * 60;
							}
							break;

							case 1:
							{
								needMinuteDiff = connectionParams.serviceRatio * 60 * 24;
							}
							break;

							case 2:
							{
								needMinuteDiff = connectionParams.serviceRatio * 60 * 24 * 7;
							}
							break;

							case (-1):
							{
								close(fd);
								STORAGE_Unprotect();
								return FALSE ;
							}

							default:
							{
								needMinuteDiff = connectionParams.serviceRatio ;
							}
							break;
						}

						if ( COMMON_CheckDts( &tempUspdService.lastServiceConnectDts , &currentDts ) == TRUE )
						{
							if ( (unsigned int)COMMON_GetDtsDiff__Alt( &tempUspdService.lastServiceConnectDts , &currentDts , BY_MIN ) < needMinuteDiff )
							{
								res = FALSE ;
							}
						}
					}
				}
			}
		}
		close(fd);
	}

	STORAGE_Unprotect();

	return res ;
}

//
//
//

int STORAGE_GetServiceCRC( uspdService_t * uspdService , unsigned int * crc )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	if ( uspdService && crc)
	{
		unsigned char * buf = (unsigned char *)malloc( sizeof(uspdService_t) );
		if ( !buf )
		{
			fprintf( stderr , "Can not allocate [%d] bytes memory" , sizeof(uspdService_t) );
			EXIT_PATTERN;
		}
		memset( buf , 0 , sizeof(uspdService_t) );

		int bufPos = 0 ;
		memcpy(&buf[bufPos] , &uspdService->lastServiceConnectDts , sizeof(dateTimeStamp));
		bufPos += sizeof(unsigned int) ;

		unsigned char m256 = 0 ;
		unsigned char mXor = 0 ;

		int idx = 0 ;
		for( ; idx < bufPos ; ++idx)
		{
			m256 += buf[idx];
			mXor ^= buf[idx] ^ m256 ;
		}
		free(buf);

		(*crc) = (mXor<<8) + m256;
		return OK ;
	}
	return res ;
}

//
//
//

int STORAGE_LoadPlcSettings(){
	DEB_TRACE(DEB_IDX_STORAGE)

/*
[id_interview] 0 - linear, 2 - parallel by map, 1 - parallel by config
0
[size_physical]
10
[size_logical]
10
[id_mode] 0 - auto 1 - app aproval, 2-with type detect
0
[node key] - 16 symbols of hex string
00 00 00 00 00 00 00 00

*/

	int res = ERROR_FILE_OPEN_ERROR;
	FILE * f = NULL;
	unsigned char strTemp[20];

	STORAGE_Protect();

	//reset previously loaded settings
	memset(&plcBaseSettings, 0, sizeof(it700params_t));
	plcBaseSettings.netSizeLogical = 10;
	plcBaseSettings.netSizePhisycal = 10;

	//try to open file
	f = fopen(CONFIG_PATH_NET_PLC, "r");
	if (f == NULL){
		COMMON_STR_DEBUG(DEBUG_PLC "OUCH! F IS NULL");
		fopen(CONFIG_PATH_NET_PLC ".bak" , "r");
	}

	if (f != NULL) {

		//reset error of absent file
		CLI_SetErrorCode(CLI_NO_CONFIG_NETPLC, CLI_CLEAR_ERROR);

		//STRATEGY: section + data
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		if (strlen((char *)strTemp) > 0) {
			plcBaseSettings.strategy = atoi((char *)strTemp);

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC"STORAGE PLC strategy = %s",  getPlcStrategyDesc());
		}

		//SIZE P: section + data
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		if (strlen((char *)strTemp) > 0){
			plcBaseSettings.netSizePhisycal = atoi((char *)strTemp);

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "STORAGE PLC netSizePhisycal = %d",  plcBaseSettings.netSizePhisycal);
		}

		//SIZE L: section + data
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		if (strlen((char *)strTemp) > 0){
			plcBaseSettings.netSizeLogical = atoi((char *)strTemp);

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "STORAGE PLC netSizeLogical = %d",  plcBaseSettings.netSizeLogical);
		}

		//MODE: section + data
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		if (strlen((char *)strTemp) > 0){
			plcBaseSettings.admissionMode = atoi((char *)strTemp);

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "STORAGE PLC admissionMode = %s",  getPlcModeDesc());
		}

		//NODE KEY: section + data
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		memset(strTemp, 0, 20);
		fscanf(f, "%s", strTemp);
		if (strlen((char *)strTemp) > 0){
			int idx;
			for (idx=0; idx<8; idx++){
				unsigned char keyPartStr[3];
				memset(keyPartStr, 0, 3);
				memcpy(keyPartStr, &strTemp[idx*2], 2);
				plcBaseSettings.nodeKey[idx] = (strtoul((char *)keyPartStr, NULL, 16) & 0xFF);
			}

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "STORAGE PLC node key = %02x %02x %02x %02x %02x %02x %02x %02x",  plcBaseSettings.nodeKey[0], plcBaseSettings.nodeKey[1], plcBaseSettings.nodeKey[2], plcBaseSettings.nodeKey[3], plcBaseSettings.nodeKey[4], plcBaseSettings.nodeKey[5], plcBaseSettings.nodeKey[6], plcBaseSettings.nodeKey[7]);
		}

		fclose(f);

		res = OK;
	}
	else {
		COMMON_STR_DEBUG(DEBUG_PLC "OUCH! F 2 IS NULL");
		CLI_SetErrorCode(CLI_NO_CONFIG_NETPLC, CLI_SET_ERROR);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "STORAGE_LoadPlcSettings %s", getErrorCode(res));

	STORAGE_Unprotect();

	return res;
}

//
//
//

unsigned int STORAGE_GetPlcUniqRemotesNumber( countersArray_t * countersArray )
{
	DEB_TRACE(DEB_IDX_STORAGE);
	unsigned int res = 0 ;

	if ( countersArray == NULL )
		return res ;

	int counterIndex = 0 ;
	for ( ; counterIndex < countersArray->countersSize ; ++counterIndex )
	{
		BOOL dublicatedFlag = FALSE ;
		int dublicatedCounterIndex = 0 ;
		for ( ; dublicatedCounterIndex < counterIndex ; ++dublicatedCounterIndex )
		{
			if ( !strcmp( (char*)countersArray->counter[ counterIndex ]->serialRemote, (char*)countersArray->counter[ dublicatedCounterIndex ]->serialRemote ) )
			{
				dublicatedFlag = TRUE ;
				break;
			}
		}
		if ( dublicatedFlag == FALSE )
		{
			res++;
		}
	}

	return res ;
}

//
//
//

int STORAGE_LoadCountersPlc(countersArray_t * countersArray) {
	DEB_TRACE(DEB_IDX_STORAGE)

	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_LoadCountersRf started");
	int res = ERROR_FILE_OPEN_ERROR;
	int counterIndex;
	int f = 0;
	int r = 0;
	unsigned char strTemp[20];

	//STORAGE_Protect();

	if ( countersArray485.countersSize >= MAX_DEVICE_COUNT_TOTAL )
	{
		//STORAGE_Unprotect();
		return OK;
	}

	f = open (CONFIG_PATH_PLC , O_RDONLY);
	if (f < 0)
		f=open(CONFIG_PATH_PLC ".bak", O_RDONLY);
	if (f > 0) {

		CLI_SetErrorCode(CLI_NO_CONFIG_PLC, CLI_CLEAR_ERROR);
		/*
		rf.cfg structure
		[counter]
		1       type
		0554    address
		Z012932 remote address
		3       tarifs mask   //1-15 according bits: t4 | t3 | t2 | t1
		1       energy mask   //1-15 according bits: r- | r+ | a- | a+
		000000  password 1
		000000  password 2
		30      profile interval - no
		0       read profile or not
		12      dbase id
		ddmmyy  mount date
		0       clear flag
		*/

		do {
			res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
			if (res != OK) break;

			if (strstr((char *)strTemp, STORAGE_COUNTER_STRING) != NULL)// find config str for new device
			{
				COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "-=[COUNTER PLC]=-");

				countersArray->counter = (counter_t **) realloc(countersArray->counter, (++(countersArray->countersSize)) * sizeof (counter_t *));
				if (countersArray->counter != NULL) {
					counterIndex = countersArray->countersSize - 1;
					countersArray->counter[counterIndex] = (counter_t *)malloc(sizeof (counter_t));
					if (countersArray->counter[counterIndex] != NULL) {

						int parsed = 0;
						int pwdSize = 0;

						//type
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0){
							countersArray->counter[counterIndex]->type = atoi((char *)strTemp) & 0xFF;
						}
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Type parsed t = %d srt is[%s]", parsed, strTemp);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Type is [%hhu]" , countersArray->counter[counterIndex]->type);

						if ( countersArray->counter[counterIndex]->type == TYPE_COUNTER_UNKNOWN )
						{
							countersArray->counter[counterIndex]->autodetectionType = TRUE ;
						}
						else
						{
							countersArray->counter[counterIndex]->autodetectionType = FALSE ;
						}

						//serial
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0){
							countersArray->counter[counterIndex]->serial = strtoul((char *)strTemp , NULL , 10);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Serial parsed s = %d srt is[%s]", parsed, strTemp);
						}

						//remote
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						memset(countersArray->counter[counterIndex]->serialRemote, 0, SIZE_OF_SERIAL + 1);
						if ((strlen((char *)strTemp) > 0) && (strlen((char *)strTemp) < (SIZE_OF_SERIAL + 1)))
							memcpy(countersArray->counter[counterIndex]->serialRemote, strTemp, strlen((char *)strTemp));
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Remote parsed r = %d srt is[%s]", parsed, strTemp);

						//tarifs mask
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						#if REV_COMPILE_FULL_QUIZ_MASK
							countersArray->counter[counterIndex]->maskTarifs = (unsigned char) 0x0F ;
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Triff mask is always 0x0F by define");
						#else
							countersArray->counter[counterIndex]->maskTarifs = (unsigned char) (atoi((char *)strTemp) & 0xFF);
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Triff mask parsed t = %d srt is[%s]", parsed, strTemp);
						#endif

						//energy mask
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						#if REV_COMPILE_FULL_QUIZ_MASK
							countersArray->counter[counterIndex]->maskEnergy = (unsigned char) 0x0F ;
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Energy mask is always 0x0F by define");
						#else
							countersArray->counter[counterIndex]->maskEnergy = (unsigned char) (atoi((char *)strTemp) & 0xFF);
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Energy mask parsed e = %d srt is[%s]", parsed, strTemp);
						#endif

						//passwrod1
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						memset(countersArray->counter[counterIndex]->password1, 0, PASSWORD_SIZE);
						pwdSize = strlen((char *)strTemp);
						if ((pwdSize > 0) && (pwdSize <= PASSWORD_SIZE))
						{
							memcpy(countersArray->counter[counterIndex]->password1, strTemp, pwdSize);
						}
						else if ( pwdSize > PASSWORD_SIZE )
						{
							COMMON_TranslateHexPwdToAnsi( strTemp , countersArray->counter[counterIndex]->password1 );
						}

						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Pass1 parsed 1 = %d srt is[%s]", parsed, strTemp);
						COMMON_BUF_DEBUG (DEBUG_STORAGE_PLC "Pass1 : " , countersArray->counter[counterIndex]->password1 , PASSWORD_SIZE);

						//passwrod2
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						memset(countersArray->counter[counterIndex]->password2, 0, PASSWORD_SIZE);
						pwdSize = strlen((char *)strTemp);
						if ((pwdSize > 0) && (pwdSize <= PASSWORD_SIZE))
						{
							memcpy(countersArray->counter[counterIndex]->password2, strTemp, pwdSize);
						}
						else if ( pwdSize > PASSWORD_SIZE )
						{
							COMMON_TranslateHexPwdToAnsi( strTemp , countersArray->counter[counterIndex]->password2 );
						}
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Pass2 parsed 2 = %d srt is[%s]", parsed, strTemp);
						COMMON_BUF_DEBUG (DEBUG_STORAGE_PLC "Pass2 : " , countersArray->counter[counterIndex]->password2 , PASSWORD_SIZE);

						//profile interval
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->profileInterval = (unsigned char) (atoi((char *)strTemp) & 0xFF);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Profile Interval parsed i = %d srt is[%s]", parsed, strTemp);

						//profile read ?
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->profile = (unsigned char) (atoi((char *)strTemp) & 0xFF);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Profile Read parsed p = %d srt is[%s]", parsed, strTemp);

						//db id
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						countersArray->counter[counterIndex]->dbId = strtoul((char *)strTemp , NULL , 10);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "DbID parsed d = %d srt is[%s]", parsed, strTemp);

						//mount date
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						int d,m,y;
						parsed = sscanf((char *)strTemp, "%d-%d-%d", &d, &m, &y);
						countersArray->counter[counterIndex]->mountD.d = d;
						countersArray->counter[counterIndex]->mountD.m = m;
						countersArray->counter[counterIndex]->mountD.y = y;
						COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "Mount Day parsed m = %d [%u-%u-%u]", parsed, d, m, y);

						//clear flag
						res = COMMON_ReadLn(f, (char *)strTemp, 20, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->clear = (unsigned char) (atoi((char *)strTemp) & 0xFF);

						//to do set clear flag
						//printf ("parsed c = %d srt is[%s]\r\n", parsed, strTemp);

						countersArray->counter[counterIndex]->useDefaultPass = FALSE ;

						//reading meterages properties
						memset( &countersArray->counter[counterIndex]->dayMetaragesLastRequestDts , 0 , sizeof(dateStamp));
						memset( &countersArray->counter[counterIndex]->monthMetaragesLastRequestDts , 0 , sizeof(dateStamp));
						memset( &countersArray->counter[counterIndex]->profileLastRequestDts , 0 , sizeof(dateTimeStamp));

						//properties of sync time
						countersArray->counter[counterIndex]->syncFlag = STORAGE_FLAG_EMPTY ; //debug was STORAGE_FLAG_EMPTY
						countersArray->counter[counterIndex]->syncWorth = -1;

						//other properties
						countersArray->counter[counterIndex]->lastTaskRepeateFlag = 0 ;

						//setup profile pointer to empty value
						countersArray->counter[counterIndex]->profilePointer = POINTER_EMPTY;
						countersArray->counter[counterIndex]->profileCurrPtr = POINTER_EMPTY;
						countersArray->counter[counterIndex]->profilePtrCurrOverflow = 0 ;
						countersArray->counter[counterIndex]->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;

						//setup journal reading properties
						countersArray->counter[counterIndex]->journalNumber = 0;
						countersArray->counter[counterIndex]->eventNumber = 0;
						countersArray->counter[counterIndex]->eventsTotal = 0;
						countersArray->counter[counterIndex]->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER ;

						//countersArray->counter[counterIndex]->tariffMapWritingFlag = FALSE ; //debug writing tasriff map. was FALSE

						//transaction time
						countersArray->counter[counterIndex]->transactionTime = 0 ;
						countersArray->counter[counterIndex]->stampReread = 0;

						//ratio
						countersArray->counter[counterIndex]->ratioIndex = 0xFF;
						countersArray->counter[counterIndex]->transformationRatios = 0x00;
						countersArray->counter[counterIndex]->lastProfileId = 0;

						//setup swver
						countersArray->counter[counterIndex]->swver1= 0;
						countersArray->counter[counterIndex]->swver2= 0;

						//setup counter's dateTimeStamp
						memset(&(countersArray->counter[counterIndex]->currentDts) , 0 , sizeof(dateTimeStamp));

						memset(&(countersArray->counter[counterIndex]->lastCounterTaskRequest) , 0 , sizeof(dateTimeStamp));

						//init last poll time for shamaning
						time(&(countersArray->counter[counterIndex]->lastPollTime));
						time(&(countersArray->counter[counterIndex]->lastLeaveCmd));
						countersArray->counter[counterIndex]->pollAlternativeAlgo = 0;
						countersArray->counter[counterIndex]->shamaningTryes = 0;
						countersArray->counter[counterIndex]->wasAged = FALSE;
						
						//countersArray->counter[counterIndex]->tariffWritingAttemptsCounter = 0 ;
						countersArray->counter[counterIndex]->unknownId = 0 ;

						//try to get something from status
						counterStatus_t counterStatus;
						memset( &counterStatus , 0 , sizeof(counterStatus_t) );
						if ( STORAGE_LoadCounterStatus( countersArray->counter[counterIndex]->dbId , &counterStatus ) == OK )
						{
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "status loading ok");
#if 0
							//get ratio
							if ( counterStatus.ratio != 0xFF )
								countersArray->counter[counterIndex]->ratio = counterStatus.ratio;
#endif
							countersArray->counter[counterIndex]->profilePointer = counterStatus.lastProfilePtr & 0xFFFF ;

							// try to get profileInterval and type from status if it need
							if ( countersArray->counter[counterIndex]->type == TYPE_COUNTER_UNKNOWN )
							{
								COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "get type from status");
								countersArray->counter[counterIndex]->type = counterStatus.type;
							}

							if ( countersArray->counter[counterIndex]->profileInterval == STORAGE_UNKNOWN_PROFILE_INTERVAL )
							{
								COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "get profileInterval from status");
								countersArray->counter[counterIndex]->profileInterval = counterStatus.profileInterval;
							}
						}
						else
						{
							COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "status loading error");
						}

						//done load if config iz oversized
						if ( (countersArray->countersSize + countersArray485.countersSize) >= MAX_DEVICE_COUNT_TOTAL)
							break;
					} else
						SET_RES_AND_BREAK_WITH_MEM_ERROR
				} else
					SET_RES_AND_BREAK_WITH_MEM_ERROR
			} else {
				res = ERROR_CONFIG_FORMAT_ERROR;

				COMMON_STR_DEBUG ( DEBUG_STORAGE_PLC "storage load get format error for PLC config file");

				break;
			}

		} while (1);

		close(f);

		if (res != ERROR_MEM_ERROR)
			res = OK;
	}
	else
		CLI_SetErrorCode(CLI_NO_CONFIG_PLC, CLI_SET_ERROR);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_LoadCountersRf(counters count %d) %s", countersArray->countersSize, getErrorCode(res));

	//STORAGE_Unprotect();

	return res;
}

//
//
//

int STORAGE_CheckConfigPlcSettings(char * buf, unsigned int bufLength, int * firstRetValue , int * secRetValue)
{
	int res = ERROR_GENERAL ;

	if ( (buf == NULL) || (bufLength == 0) )
		return res ;

	char * tmpBuf = malloc( bufLength + 1 );
	if ( !tmpBuf )
	{
		EXIT_PATTERN;
	}

	memcpy( tmpBuf , buf , bufLength );
	tmpBuf[ bufLength ] = '\0' ;

	char * physicalSizePtr = strstr( tmpBuf , "[size_physical]" );
	char * logicalSizePtr = strstr( tmpBuf , "[size_logical]" );

	free( tmpBuf );
	tmpBuf = NULL ;

	STORAGE_CountersProtect();

	//get number of uniq plc modems
	int modemsNumber = (int)STORAGE_GetPlcUniqRemotesNumber( &countersArrayPlc );
	(*secRetValue) = modemsNumber ;

	STORAGE_CountersUnprotect();

	if ( ( physicalSizePtr == NULL ) || ( logicalSizePtr == NULL ) )
		return res ;

	int physicalSize = (int)strtoul( physicalSizePtr + strlen( "[size_physical]\n" ) , NULL , 10);
	int logicalSize = (int)strtoul( logicalSizePtr + strlen( "[size_logical]\n" ) , NULL , 10);

	if ( (physicalSize < 10) || (logicalSize < 10) )
		return res ;


	if ( (physicalSize > modemsNumber) && (logicalSize > modemsNumber) )
	{
		res = OK ;
	}
	else
	{
		(*firstRetValue) = logicalSize ;
		if ( physicalSize < (*firstRetValue) )
		{
			(*firstRetValue) = physicalSize ;
		}
	}


	return res ;
}

//
//
//

int STORAGE_GetCountersNumberInRawConfig(char * buf, unsigned int bufLength)
{
	int retVal = 0 ;
	char * tmpBuf = malloc( bufLength + 1 );
	char * ptr ;
	if ( !tmpBuf )
	{
		EXIT_PATTERN;
	}

	memcpy( tmpBuf , buf , bufLength );
	tmpBuf[ bufLength ] = '\0' ;

	ptr = tmpBuf;

	while ( (ptr = strstr( ptr, STORAGE_COUNTER_STRING )) != NULL )
	{
		ptr++;
		retVal++;
	}


	free(tmpBuf);
	return retVal ;
}

//
//
//

int STORAGE_CheckConfigCountersPlc(char * buf, unsigned int bufLength , int * firstRetValue , int * secRetValue)
{
	int res = ERROR_GENERAL ;

	if ( buf == NULL )
	{
		return res ;
	}

	if ( bufLength == 0 )
	{
		return OK ;
	}

	char * tmpBuf = malloc( bufLength + 1 );
	if ( !tmpBuf )
	{
		EXIT_PATTERN;
	}

	memcpy( tmpBuf , buf , bufLength );
	tmpBuf[ bufLength ] = '\0' ;

	int logicalSize = plcBaseSettings.netSizeLogical;
	int physicalSize = plcBaseSettings.netSizePhisycal;

	//tokenize buffer
	int remoteCounter = 0 ;
	const int tokenNumber = MAX_DEVICE_COUNT_TOTAL * 13 ;
	char * lastPtr = NULL ;
	int tokenCounter = 0 ;
	char * token[ tokenNumber ];
	char * pch = strtok_r( tmpBuf, "\r\n" , &lastPtr);
	while (pch!=NULL)
	{
		token[ tokenCounter++ ] = pch;
		pch=strtok_r(NULL, "\r\n" , &lastPtr);
		if ( tokenCounter == tokenNumber )
			break;
	}

	//getting uniq number of remotes
	char * remotePtr[ MAX_DEVICE_COUNT_TOTAL ];
	int tokenIndex = 0 ;
	for ( tokenIndex = 0 ; tokenIndex < (tokenCounter-3) ; ++tokenIndex )
	{
		if( !strcmp( token[tokenIndex], "[counter]") )
		{
			BOOL dublicatedFlag = FALSE ;
			int dublicatedIndex = 0 ;
			for ( ; dublicatedIndex < remoteCounter ; ++dublicatedIndex )
			{
				if ( !strcmp( remotePtr[ dublicatedIndex ], token[tokenIndex + 3] ) )
				{
					dublicatedFlag = TRUE ;
					break;
				}
			}

			if ( dublicatedFlag == FALSE )
				remotePtr[ remoteCounter++ ] = token[tokenIndex + 3];
		}
	}

	free(tmpBuf);
	tmpBuf = NULL ;

	if ( (logicalSize > remoteCounter) && ( physicalSize > remoteCounter) )
	{
		res = OK ;
	}
	else
	{
		(*firstRetValue) = remoteCounter ;
		(*secRetValue) = logicalSize;
		if ( physicalSize < (*secRetValue) )
		{
			(*secRetValue) = physicalSize ;
		}
	}

	return res ;
}

//
//
//

void STORAGE_ClearPlcCounterConfiguration()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	STORAGE_CountersProtect();

	//clear config file
	int fd = open( CONFIG_PATH_PLC , O_RDWR );
	if ( fd > 0 )
	{
		ftruncate( fd , 0);
		close (fd);
	}

	//clear current config
	if ( (countersArrayPlc.countersSize > 0) && (countersArrayPlc.counter) )
	{
		int counterIndex = 0 ;
		for( ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex)
		{
			if ( countersArrayPlc.counter[ counterIndex ] )
			{
				STORAGE_ClearMeterages( countersArrayPlc.counter[ counterIndex ]->dbId );

				free( countersArrayPlc.counter[ counterIndex ] );
				countersArrayPlc.counter[ counterIndex ] = NULL ;
			}
		}

		free ( countersArrayPlc.counter );
		countersArrayPlc.counter = NULL ;

		countersArrayPlc.countersSize = 0 ;
		countersArrayPlc.currentCounterIndex = 0;
	}


	STORAGE_CountersUnprotect();

	return ;
}

//
//
//

int STORAGE_AddPlcCounterToConfiguration(unsigned char * serialRemote)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;


	STORAGE_CountersProtect();

	unsigned long serial = strtoul((char *)serialRemote , NULL, 10);
	if ( STORAGE_IsRemotePresentInConfiguration( serialRemote ) == TRUE )
	{
		STORAGE_CountersUnprotect();
		return OK ;
	}

	#if 0 //uhov forgot, osipov forgot, anybody known answer - for what this was needed?!
	if ( STORAGE_IsCounterPresentInConfiguration( serial ) == TRUE )
	{
		STORAGE_CountersUnprotect();
		return OK ;
	}ppppp
	#endif
	
	//
	// fill the struct counter_t
	//

	counter_t newCounter;
	memset(&newCounter , 0 , sizeof(counter_t) );

	STORAGE_CounterTypeDetection( serial , &newCounter );
	newCounter.serial = serial ;

	if ((strlen((char *)serialRemote) > 0) && (strlen((char *)serialRemote) < (SIZE_OF_SERIAL + 1)))
		memcpy(newCounter.serialRemote, serialRemote, strlen((char *)serialRemote));

	newCounter.maskTarifs = 0x0F ;
	newCounter.maskEnergy = 0x0F ;
	newCounter.useDefaultPass = TRUE ;
	newCounter.profileInterval = STORAGE_UNKNOWN_PROFILE_INTERVAL ;
	newCounter.profile = 1 ;
	newCounter.ratioIndex = 0xFF;
	newCounter.stampReread = 0;
	newCounter.dbId = STORAGE_GetNextFreeDbId();
	
	//clear storage just in case
	STORAGE_ClearMeterages( newCounter.dbId );

	//mount date
	dateTimeStamp currentDts ;
	COMMON_GetCurrentDts( &currentDts );
	memcpy( &newCounter.mountD , &currentDts.d , sizeof(dateStamp) );

	//properties of sync time
	newCounter.syncFlag = STORAGE_FLAG_EMPTY ; //debug was STORAGE_FLAG_EMPTY
	newCounter.syncWorth = -1;

	//setup profile pointer to empty value
	newCounter.profilePointer = POINTER_EMPTY;
	newCounter.profileCurrPtr = POINTER_EMPTY;
	newCounter.profilePtrCurrOverflow = 0 ;
	newCounter.profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;
	
	//init ulutchaizings for plc poll
	time(&(newCounter.lastLeaveCmd));
	time(&(newCounter.lastPollTime));

	newCounter.pollAlternativeAlgo = POOL_ALTER_NOT_SET;
	newCounter.shamaningTryes = 0;
	newCounter.wasAged = FALSE;

	//
	// add counter to PLC counters array (no PLC poll at this moment)
	//

	//osipov: potential problem
	//reallocation of counter struct will affect previously setted pointers in statistic map
	countersArrayPlc.counter = (counter_t **) realloc(countersArrayPlc.counter, (++(countersArrayPlc.countersSize)) * sizeof (counter_t *));
	if ( countersArrayPlc.counter )
	{
		int counterIndex = countersArrayPlc.countersSize - 1;

		countersArrayPlc.counter[counterIndex] = (counter_t *)malloc(sizeof (counter_t));
		if (countersArrayPlc.counter[counterIndex] != NULL)
		{
			counter_t * tmpPtr = countersArrayPlc.counter[counterIndex] ;
			// move elements of array
			for ( ; counterIndex > 0 ; --counterIndex )
			{
				countersArrayPlc.counter[counterIndex] = countersArrayPlc.counter[counterIndex - 1] ;
				countersArrayPlc.counter[counterIndex]->lastTaskRepeateFlag = 0 ;
			}

			//set the new counter to the 0 position of array
			countersArrayPlc.counter[ 0 ] = tmpPtr ;
			memcpy( countersArrayPlc.counter[ 0 ] , &newCounter , sizeof(counter_t) );

			countersArrayPlc.currentCounterIndex = 0 ;
			res = OK ;

			//direct save if repeater
			//other will be detected from unknown type later and then saved
			if ( newCounter.type == TYPE_COUNTER_REPEATER )
			{
				STORAGE_SavePlcCounterToConfiguration( &newCounter );
			}

			//append to statistic at this place: how ?!
			STORAGE_AppendToStatisticMap(&newCounter);

		}
		else
		{
			res = ERROR_MEM_ERROR ;
		}
	}
	else
	{
		res = ERROR_MEM_ERROR ;
	}

	STORAGE_CountersUnprotect();
	
	return res ;
}

//
//
//

int STORAGE_SavePlcCounterToConfiguration( counter_t * counter )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

			//there is no counter in the configuration. save it
			int i = 0 ;
			char pass1[ PASSWORD_SIZE * 2+1 ];
			memset(pass1 , 0 , PASSWORD_SIZE * 2+1);

			for( i=0 ; i<PASSWORD_SIZE ; ++i)
				sprintf( &pass1[ strlen(pass1) ] , "%02X" , counter->password1[i] );

			char pass2[ PASSWORD_SIZE * 2+1 ];
			memset(pass2 , 0 , PASSWORD_SIZE * 2+1);
			for( i=0 ; i<PASSWORD_SIZE ; ++i)
				sprintf( &pass2[ strlen(pass2) ] , "%02X" , counter->password1[i] );

			char buf[ MAX_MESSAGE_LENGTH ];
			memset( buf , 0 , MAX_MESSAGE_LENGTH );

			snprintf( buf , MAX_MESSAGE_LENGTH ,
					  "[counter]\n"
					  "%hhu\n"
					  "%lu\n"
					  "%s\n"
					  "%u\n"
					  "%u\n"
					  "%s\n"
					  "%s\n"
					  "%hhu\n"
					  "%hhu\n"
					  "%lu\n"
					  "%02hhu-%02hhu-%02hhu\n"
					  "%hhu\n",
					  counter->type , counter->serial , counter->serialRemote ,
					  counter->maskTarifs, counter->maskEnergy , pass1 ,pass2 ,
					  counter->profileInterval , counter->profile , counter->dbId , counter->mountD.d ,
					  counter->mountD.m , counter->mountD.y , counter->clear );

			//STORAGE_Protect();

			int fd = open( CONFIG_PATH_PLC , O_RDWR );
			if ( fd < 0 )
			{
				fd = open( CONFIG_PATH_PLC ".bak", O_RDWR );
			}
			if ( fd < 0 )
			{
				fd = open( CONFIG_PATH_PLC , O_CREAT | O_RDWR , 0777);
			}
			if ( fd >= 0 )
			{
				struct stat st;
				fstat(fd, &st);
				int fileSize = st.st_size ;

				char * cfgContents = NULL ;
				if ( fileSize > 0 )
				{
					cfgContents = (char *)malloc( fileSize ) ;
					if ( cfgContents )
					{
						read( fd , cfgContents , fileSize);
						ftruncate(fd , 0);
						lseek(fd , 0 , SEEK_SET);

						write(fd , buf , strlen(buf));
						write(fd , cfgContents , fileSize);
						free( cfgContents ) ;
						res = OK ;
					}
					else
					{
						res = ERROR_MEM_ERROR ;
					}
				}
				else
				{
					write(fd , buf , strlen(buf));
					res = OK ;
				}
				close(fd);
			}
			
			//STORAGE_Unprotect();
			

	if( res == OK )
	{
		//save journal event
		unsigned char evDesc[EVENT_DESC_SIZE];
		memset( evDesc , 0 , EVENT_DESC_SIZE );

		evDesc[0] = ( counter->dbId >> 24 ) & 0xFF ;
		evDesc[1] = ( counter->dbId >> 16 ) & 0xFF ;
		evDesc[2] = ( counter->dbId >> 8 ) & 0xFF ;
		evDesc[3] =   counter->dbId & 0xFF ;

		evDesc[4] = ( counter->serial >> 24 ) & 0xFF ;
		evDesc[5] = ( counter->serial >> 16 ) & 0xFF ;
		evDesc[6] = ( counter->serial >> 8 ) & 0xFF ;
		evDesc[7] =   counter->serial & 0xFF ;

		evDesc[8] = counter->type ;
		STORAGE_JournalUspdEvent( EVENT_COUNTER_ADDED_TO_PLC_CONFIG , evDesc );
	}

	return res;
}

//
//
//

BOOL STORAGE_IsCounterPresentInConfiguration(unsigned long serial)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = FALSE ;

	int counterIndex = 0 ;
	for( ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex)
	{
		if ( serial == countersArrayPlc.counter[ counterIndex ]->serial )
			return TRUE ;		
	}

	return res ;
}

//
//
//

BOOL STORAGE_IsRemotePresentInConfiguration( unsigned char * serialRemote )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = FALSE ;

	int counterIndex = 0 ;
	for( ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex)
	{
		if ( !strcmp( (char *)countersArrayPlc.counter[ counterIndex ]->serialRemote , (char *)serialRemote ) )
			return TRUE ;
	}

	return res ;
}

//
//
//

void STORAGE_CounterTypeDetection( unsigned long serial , counter_t * counter )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	char buf[11];
	memset(buf , 0 , 11);

	snprintf(buf , 11 , "%lu" , serial);

	counter->type = TYPE_COUNTER_UNKNOWN;

	if ( (strlen(buf) == 10)  )
	{
		if (buf[0] == '4'){
			counter ->type = TYPE_COUNTER_REPEATER;
		} else {
			counter->unknownId = UNKNOWN_ID_GUBANOV ; //to try detect ujuk
		}
	}
}

//
//
//

unsigned long STORAGE_GetNextFreeDbId()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	unsigned long dbidIndex = 1 ;

	for( ; dbidIndex < 0xFFFFFFFF ; ++dbidIndex )
	{
		int counterIndex = 0 ;
		BOOL crossFlag = FALSE ;

		#if REV_COMPILE_485
			if ( crossFlag == FALSE )
			{
				if ( countersArray485.countersSize )
				{
					for( counterIndex = 0 ; counterIndex < countersArray485.countersSize ; ++counterIndex )
					{
						if ( countersArray485.counter[ counterIndex ]->dbId == dbidIndex )
						{
							crossFlag = TRUE ;
							break;
						}
					}
				}
			}
		#endif

		#if REV_COMPILE_PLC
			if ( crossFlag == FALSE )
			{
				if ( countersArrayPlc.countersSize )
				{
					for( counterIndex = 0 ; counterIndex < countersArrayPlc.countersSize ; ++counterIndex )
					{
						if ( countersArrayPlc.counter[ counterIndex ]->dbId == dbidIndex )
						{
							crossFlag = TRUE ;
							break;
						}
					}
				}
			}
		#endif

		#if REV_COMPILE_RF
			if ( crossFlag == FALSE )
			{
				if ( countersArrayRf.countersSize )
				{
					for( counterIndex = 0 ; counterIndex < countersArrayRf.countersSize ; ++counterIndex )
					{
						if ( countersArrayRf.counter[ counterIndex ]->dbId == dbidIndex )
						{
							crossFlag = TRUE ;
							break;
						}
					}
				}
			}
		#endif

		if ( crossFlag == FALSE )
		{
			return dbidIndex ;
		}
	}

	return 0;
}

//
//
//

int STORAGE_LoadCounters485(countersArray_t * countersArray) {
	DEB_TRACE(DEB_IDX_STORAGE)

	COMMON_STR_DEBUG(DEBUG_STORAGE_485 "STORAGE_LoadCounters485 started ");
	int res = ERROR_FILE_OPEN_ERROR;
	int counterIndex = 0;
	int f = 0;
	int r = 0;
	const int strTempLength = 64 ;
	unsigned char strTemp[ strTempLength ];

	STORAGE_Protect();

	if ( countersArrayPlc.countersSize >= MAX_DEVICE_COUNT_TOTAL )
	{
		STORAGE_Unprotect();
		return OK;
	}

	f = open (CONFIG_PATH_485 , O_RDONLY);
	if ( f < 0 )
		f = open (CONFIG_PATH_485 ".bak", O_RDONLY);
	if (f > 0) {

		CLI_SetErrorCode(CLI_NO_CONFIG_RS, CLI_CLEAR_ERROR);
		/*
		rf.cfg structure
		[counter]
		1       type
		111     addres
		3       tarifs mask   //1-15 according bits: t4 | t3 | t2 | t1
		1       energy mask   //1-15 according bits: r- | r+ | a- | a+
		000000  password 1
		000000  password 2
		30      profile interval
		0       read profile or not
		12      dbase id
		ddmmyy  mount date
		0       clear flag

		 */

		do {
			res = ERROR_MEM_ERROR;

			res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
			if (res != OK) break;

			if (strstr((char *)strTemp, STORAGE_COUNTER_STRING) != NULL)// find config str for new device
			{
				COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "-=[COUNTER 485]=-");

				countersArray->counter = (counter_t **) realloc(countersArray->counter, (++(countersArray->countersSize) * sizeof (counter_t *)));
				if (countersArray->counter != NULL) {
					counterIndex = countersArray->countersSize - 1;
					countersArray->counter[counterIndex] = (counter_t *) malloc(sizeof (counter_t));
					if (countersArray->counter[counterIndex] != NULL) {
						//type
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0){
							int typeFromWebConfig = (unsigned char) (atoi((char *)strTemp) & 0xFF);
							countersArray->counter[counterIndex]->type = typeFromWebConfig;
						}
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Type parsed t = %d srt is[%s]", r, strTemp);

						if ( countersArray->counter[counterIndex]->type == TYPE_COUNTER_UNKNOWN )
						{
							countersArray->counter[counterIndex]->autodetectionType = TRUE ;
						}
						else
						{
							countersArray->counter[counterIndex]->autodetectionType = FALSE ;
						}

						//serial attributes
						countersArray->counter[counterIndex]->serial485Attributes.stopBits = SERIAL_RS_ONCE_STOP_BIT ;
						countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_NONE ;
						countersArray->counter[counterIndex]->serial485Attributes.bits = SERIAL_RS_BITS_8 ;
						countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_9600 ;
						char * serialAttributesChPtr = NULL ;

						//serial
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						countersArray->counter[counterIndex]->serial = strtoul((char *)strTemp , &serialAttributesChPtr , 10);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Serial parsed s = %d srt is[%s]", r, strTemp);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Serial is [%lu]" , countersArray->counter[counterIndex]->serial );
						if ( serialAttributesChPtr )
						{
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Parsing serial attributes");
							unsigned int speed = 0 ;
							unsigned char bits = 0 ;
							char parity = 0 ;
							unsigned char stopBit = 0 ;

							int scannedBlocks = sscanf( serialAttributesChPtr , "|%u-%hhu%c%hhu" , &speed , &bits , &parity , &stopBit );
							if ( scannedBlocks == 4 )
							{
								COMMON_STR_DEBUG( DEBUG_STORAGE_485 "speed=[%u] bits=[%hhu] parity=[%c] stopBit=[%hhu]" , speed , bits , parity , stopBit );

								switch (speed)
								{
									case 300:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 300" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_300 ;
										break;

									case 600:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 600" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_600 ;
										break;

									case 1200:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 1200" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_1200 ;
										break;

									case 1800:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 1800" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_1800 ;
										break;

									case 2400:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 2400" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_2400 ;
										break;

									case 4800:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 4800" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_4800 ;
										break;

									default:
									case 9600:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 9600" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_9600 ;
										break;

									case 19200:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 19200" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_19200 ;
										break;
									case 38400:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 38400" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_38400 ;
										break;
									case 57600:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 57600" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_57600 ;
										break;
									case 115200:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes speed 115200" );
										countersArray->counter[counterIndex]->serial485Attributes.speed = SERIAL_RS_BAUD_115200 ;
										break;
								}

								switch (bits)
								{
									default:
									case 8:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes bits 8" );
										countersArray->counter[counterIndex]->serial485Attributes.bits = SERIAL_RS_BITS_8 ;
										break;

									case 7:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes bits 7" );
										countersArray->counter[counterIndex]->serial485Attributes.bits = SERIAL_RS_BITS_7 ;
										break;
								}

								switch (parity)
								{
									default:
									case 'N':
									case 'n':
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes parity none" );
										countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_NONE ;
										break;

									case 'e':
									case 'E':
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes parity even" );
										countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_EVEN ;
										break;

									case 'o':
									case 'O':
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes parity odd" );
										countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_ODD ;
										break;

									case 'm':
									case 'M':
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes parity odd" );
										countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_MARK ;
										break;

									case 's':
									case 'S':
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes parity odd" );
										countersArray->counter[counterIndex]->serial485Attributes.parity = SERIAL_RS_PARITY_SPACE ;
										break;
								}

								switch(stopBit)
								{
									default:
									case 1:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes stop bit once" );
										countersArray->counter[counterIndex]->serial485Attributes.stopBits = SERIAL_RS_ONCE_STOP_BIT ;
										break;

									case 2:
										COMMON_STR_DEBUG( DEBUG_STORAGE_485 "serial attributes stop bit double" );
										countersArray->counter[counterIndex]->serial485Attributes.stopBits = SERIAL_RS_DOUBLE_STOP_BIT ;
										break;
								}
							}
							else
							{
								COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Serial attributes format error");
							}
						}
						//remote
						memset(countersArray->counter[counterIndex]->serialRemote, 0, SIZE_OF_SERIAL + 1);

						//tarifs mask
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						#if REV_COMPILE_FULL_QUIZ_MASK
							countersArray->counter[counterIndex]->maskTarifs = (unsigned char) 0x0F;
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Triff mask is always 0x0f by define");
						#else
							if (strlen((char *)strTemp) > 0)
								countersArray->counter[counterIndex]->maskTarifs = (unsigned char) (atoi((char *)strTemp) & 0xFF);
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Triff mask parsed t = %d srt is[%s]", r, strTemp);
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Tariff mask parsed [%02X]", countersArray->counter[counterIndex]->maskTarifs ) ;
						#endif

						//energy mask
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						#if REV_COMPILE_FULL_QUIZ_MASK
							countersArray->counter[counterIndex]->maskEnergy = (unsigned char) 0x0F;
							COMMON_STR_DEBUG (DEBUG_STORAGE_485 "Energy mask is always 0x0F by define");
						#else
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->maskEnergy = (unsigned char) (atoi((char *)strTemp) & 0xFF);
						COMMON_STR_DEBUG (DEBUG_STORAGE_485 "Energy mask parsed e = %d srt is[%s]", r, strTemp);
						#endif

						//passwrod1
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						memset(countersArray->counter[counterIndex]->password1, 0, PASSWORD_SIZE);
						if ((strlen((char *)strTemp) > 0) && (strlen((char *)strTemp) <= PASSWORD_SIZE))
						{
							memcpy(countersArray->counter[counterIndex]->password1, strTemp, strlen((char *)strTemp));
						}
						else if ( strlen((char *)strTemp) > PASSWORD_SIZE )
						{
							COMMON_TranslateHexPwdToAnsi( strTemp , countersArray->counter[counterIndex]->password1 );
						}
						COMMON_STR_DEBUG (DEBUG_STORAGE_485 "Pass1 parsed 1 = %d srt is[%s]", r, strTemp);
						COMMON_BUF_DEBUG (DEBUG_STORAGE_485 "Pass2 : " , countersArray->counter[counterIndex]->password1 , PASSWORD_SIZE);

						//passwrod2
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						memset(countersArray->counter[counterIndex]->password2, 0, PASSWORD_SIZE);
						if ((strlen((char *)strTemp) > 0) && (strlen((char *)strTemp) <= PASSWORD_SIZE))
						{
							memcpy(countersArray->counter[counterIndex]->password2, strTemp, strlen((char *)strTemp));
						}
						else if ( strlen((char *)strTemp) > PASSWORD_SIZE )
						{
							COMMON_TranslateHexPwdToAnsi( strTemp , countersArray->counter[counterIndex]->password2 );
						}
						COMMON_STR_DEBUG (DEBUG_STORAGE_485 "Pass2 parsed 2 = %d srt is[%s]", r, strTemp);
						COMMON_BUF_DEBUG (DEBUG_STORAGE_485 "Pass2 : " , countersArray->counter[counterIndex]->password2 , PASSWORD_SIZE);

						//profile interval
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->profileInterval = (unsigned char) (atoi((char *)strTemp) & 0xFF);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Profile Interval parsed i = %d srt is[%s]", r, strTemp);

						//profile
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->profile = (unsigned char) (atoi((char *)strTemp) & 0xFF);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Profile Read parsed p = %d srt is[%s]", r, strTemp);

						//db id
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						countersArray->counter[counterIndex]->dbId = strtoul((char *)strTemp , NULL , 10);
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "DbID parsed d = %d srt is[%s]", r, strTemp);

						//mount date
						int d,m,y;
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						r = sscanf((char *)strTemp, "%d-%d-%d", &d, &m, &y);
						countersArray->counter[counterIndex]->mountD.d = d;
						countersArray->counter[counterIndex]->mountD.m = m;
						countersArray->counter[counterIndex]->mountD.y = y;
						COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "Mount Day parsed m = %d [%u-%u-%u]", r, d, m, y);

						//clear
						res = COMMON_ReadLn(f, (char *)strTemp, strTempLength, &r);
						if (res != OK) break;
						if (strlen((char *)strTemp) > 0)
							countersArray->counter[counterIndex]->clear = (unsigned char) (atoi((char *)strTemp) & 0xFF);

						countersArray->counter[counterIndex]->useDefaultPass = FALSE ;

						//reading meterages properties
						memset( &countersArray->counter[counterIndex]->dayMetaragesLastRequestDts , 0 , sizeof(dateStamp));
						memset( &countersArray->counter[counterIndex]->monthMetaragesLastRequestDts , 0 , sizeof(dateStamp));
						memset( &countersArray->counter[counterIndex]->profileLastRequestDts , 0 , sizeof(dateTimeStamp));

						//properties of sync time
						countersArray->counter[counterIndex]->syncFlag = STORAGE_FLAG_EMPTY ;
						countersArray->counter[counterIndex]->syncWorth = -1;

						//other properties
						countersArray->counter[counterIndex]->lastTaskRepeateFlag = 0 ;

						//setup profile pointer to empty value
						countersArray->counter[counterIndex]->profilePointer = POINTER_EMPTY;
						countersArray->counter[counterIndex]->profileCurrPtr = POINTER_EMPTY;
						countersArray->counter[counterIndex]->profilePtrCurrOverflow = 0 ;
						countersArray->counter[counterIndex]->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;

						//setup journal reading properties
						countersArray->counter[counterIndex]->journalNumber = 0;
						countersArray->counter[counterIndex]->eventNumber = 0;
						countersArray->counter[counterIndex]->eventsTotal = 0;
						countersArray->counter[counterIndex]->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER ;

						//countersArray->counter[counterIndex]->tariffMapWritingFlag =FALSE ;

						//transaction time set to initial
						countersArray->counter[counterIndex]->transactionTime = 0 ;
						countersArray->counter[counterIndex]->stampReread = 0;

						//ratio
						countersArray->counter[counterIndex]->ratioIndex = 0xFF;
						countersArray->counter[counterIndex]->transformationRatios = 0x00;
						countersArray->counter[counterIndex]->lastProfileId = 0;

						//setup swver
						countersArray->counter[counterIndex]->swver1= 0;
						countersArray->counter[counterIndex]->swver2= 0;

						//setup counter's dateTimeStamp
						memset(&(countersArray->counter[counterIndex]->currentDts) , 0 , sizeof(dateTimeStamp));
						memset(&(countersArray->counter[counterIndex]->lastCounterTaskRequest) , 0 , sizeof(dateTimeStamp));

						//countersArray->counter[counterIndex]->tariffWritingAttemptsCounter = 0 ;
						countersArray->counter[counterIndex]->unknownId = 0 ;

						//try to get counterType from status
						counterStatus_t counterStatus;
						memset( &counterStatus , 0 , sizeof(counterStatus_t) );
						if ( STORAGE_LoadCounterStatus( countersArray->counter[counterIndex]->dbId , &counterStatus ) == OK )
						{
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "status loading ok");

							countersArray->counter[counterIndex]->profilePointer = counterStatus.lastProfilePtr & 0xFFFF ;

							// try to get profileInterval and type from status if it need
							if ( countersArray->counter[counterIndex]->type == TYPE_COUNTER_UNKNOWN )
							{
								COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "get type from status");
								countersArray->counter[counterIndex]->type = counterStatus.type;
							}

							if ( countersArray->counter[counterIndex]->profileInterval == STORAGE_UNKNOWN_PROFILE_INTERVAL )
							{
								COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "get profileInterval from status");
								countersArray->counter[counterIndex]->profileInterval = counterStatus.profileInterval;
							}

						}
						else
						{
							COMMON_STR_DEBUG ( DEBUG_STORAGE_485 "status loading error");
						}

						//check loaded count
						if ((countersArray->countersSize + countersArrayPlc.countersSize) >= MAX_DEVICE_COUNT_TOTAL)
							break;

					} else
						SET_RES_AND_BREAK_WITH_MEM_ERROR
				} else
					SET_RES_AND_BREAK_WITH_MEM_ERROR
			} else {
				res = ERROR_CONFIG_FORMAT_ERROR;
				break;
			}

		} while (1);

		close(f);

		if (res != ERROR_MEM_ERROR)
			res = OK;
	}
	else
		CLI_SetErrorCode(CLI_NO_CONFIG_RS, CLI_SET_ERROR);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_485 "STORAGE_LoadCounters485(counters count %d) %s", countersArray->countersSize, getErrorCode(res));

	STORAGE_Unprotect();

	return res;
}

//
//
//

int STORAGE_LoadCounterStatus( unsigned long counterDbId , counterStatus_t * counterStatus )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	if ( !counterStatus )
		return res ;

	char fileName[SFNL];
	GET_STORAGE_FILE_NAME(fileName, STORAGE_PATH_STATUS, counterDbId);

	STORAGE_CounterStatusProtect();

	int fd = open( fileName , O_RDONLY );

	if( fd > 0 )
	{		
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);

		if ( fd_stat.st_size == sizeof(counterStatus_t) )
		{
			counterStatus_t tempCounterStatus;
			memset(&tempCounterStatus , 0 , sizeof(counterStatus_t) );

			if ( read(fd , &tempCounterStatus , sizeof(counterStatus_t)) == sizeof(counterStatus_t) )
			{
				if ( tempCounterStatus.crc == STORAGE_GetCounterStatusCrc(&tempCounterStatus) )
				{
					memcpy( counterStatus , &tempCounterStatus , sizeof(counterStatus_t) );
					res = OK;
				}
			}
		}

		close( fd );
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}


	STORAGE_CounterStatusUnprotect();

	return res ;
}

//
//
//

int STORAGE_SaveCounterStatus( unsigned long counterDbId , counterStatus_t * counterStatus )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	if ( !counterStatus )
		return res ;

	char fileName[SFNL];
	GET_STORAGE_FILE_NAME(fileName, STORAGE_PATH_STATUS, counterDbId);

	if ( counterStatus->profileInterval == 0 )
	{
		counterStatus->profileInterval = STORAGE_UNKNOWN_PROFILE_INTERVAL ;
	}

	//if ( counterStatus->ratio == 0 )
	//{
	//	counterStatus->ratio = 0xFF ;
	//}

	STORAGE_CounterStatusProtect();

	int fd = open( fileName , O_WRONLY | O_TRUNC | O_CREAT , 0777 );
	if ( fd > 0 )
	{
		counterStatus_t tempCounterStatus;
		memcpy(&tempCounterStatus , counterStatus , sizeof(counterStatus_t) );
		tempCounterStatus.crc = STORAGE_GetCounterStatusCrc(&tempCounterStatus);

		if ( write(fd , &tempCounterStatus , sizeof(counterStatus_t)) == sizeof(counterStatus_t) )
		{
			STORAGE_IncrementHardwareWritingCtrlCounter();
			res = OK ;
		}

		close( fd );
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	STORAGE_CounterStatusUnprotect();

	return res ;
}

//
//
//

unsigned int STORAGE_GetCounterStatusCrc( counterStatus_t * counterStatus __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_STORAGE)

	//TODO

	return 0x0ABCDEF0 ;
}

//
//
//

int STORAGE_UpdateCounterStatus_LastProfilePtr( counter_t * counter , unsigned int newLastProfilePtr )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		counterStatus.lastProfilePtr = (counterStatus.lastProfilePtr & 0xFF000000) | newLastProfilePtr ;
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		counterStatus.lastProfilePtr = newLastProfilePtr ;

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

int STORAGE_UpdateCounterStatus_CounterType( counter_t * counter , unsigned char newCounterType )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		counterStatus.type = newCounterType ;
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		counterStatus.type = newCounterType ;

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

int STORAGE_UpdateCounterStatus_ProfileInterval( counter_t * counter , unsigned char newProfileInterval )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		counterStatus.profileInterval = newProfileInterval ;
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		counterStatus.profileInterval = newProfileInterval ;

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

int STORAGE_UpdateCounterStatus_Ratio( counter_t * counter , unsigned char newRatio )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		counterStatus.ratio = newRatio ;
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		counterStatus.ratio = newRatio ;

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

int STORAGE_UpdateCounterStatus_StateWord( counter_t * counter , counterStateWord_t * counterStateWord )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		memcpy( &counterStatus.counterStateWord , counterStateWord , sizeof(counterStateWord_t) );
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		memcpy( &counterStatus.counterStateWord , counterStateWord , sizeof(counterStateWord_t) );

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

int STORAGE_UpdateCounterStatus_AllowSeasonChangeFlg( counter_t * counter , unsigned char allowSeasonChangeFlg )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	counterStatus_t counterStatus;
	memset(&counterStatus , 0 , sizeof(counterStatus_t) );

	res = STORAGE_LoadCounterStatus( counter->dbId , &counterStatus );
	if ( res == OK )
	{
		counterStatus.allowSeasonChange = allowSeasonChangeFlg ;
		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}
	else
	{
		STORAGE_StuffCounterStatusByDefault( &counterStatus );
		counterStatus.allowSeasonChange = allowSeasonChangeFlg ;

		res = STORAGE_SaveCounterStatus( counter->dbId , &counterStatus );
	}

	return res ;
}

//
//
//

void STORAGE_StuffCounterStatusByDefault( counterStatus_t * counterStatus )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	if ( counterStatus )
	{
		memset( counterStatus->counterStateWord.word , 0xFF , sizeof(counterStateWord_t) );
		counterStatus->lastProfilePtr = POINTER_EMPTY ;
		counterStatus->type = TYPE_COUNTER_UNKNOWN ;
		counterStatus->profileInterval = STORAGE_UNKNOWN_PROFILE_INTERVAL ;
		counterStatus->ratio = 0xFF ;
		counterStatus->allowSeasonChange = STORAGE_SEASON_CHANGE_DISABLE;
		counterStatus->reserved1 = 0xFF;
		counterStatus->reserved2 = 0xFF;
		counterStatus->reserved3 = 0xFF;
		counterStatus->reserved4 = 0xFF;
		counterStatus->reserved5 = 0xFF;
		counterStatus->reserved6 = 0xFF;
		counterStatus->reserved7 = 0xFF;
		counterStatus->reserved8 = 0xFF;
		counterStatus->reserved9 = 0xFF;
		counterStatus->reserved10 = 0xFF;
		counterStatus->crc = STORAGE_GetCounterStatusCrc( counterStatus );
	}
}

//
//
//

int STORAGE_LoadSeasonAllowFlag()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	STORAGE_SeasonProtect();

	int fd = open( CONFIG_PATH_SEASON , O_RDONLY );
	if ( fd > 0 )
	{
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);

		if( fd_stat.st_size > 0 )
		{
			char * buf = (char *)malloc( fd_stat.st_size * sizeof(char) ) ;
			if ( !buf )
			{
				STORAGE_SeasonUnprotect();
				EXIT_PATTERN ;
			}

			memset( buf , 0 , fd_stat.st_size * sizeof(char) );

			read( fd , buf , fd_stat.st_size * sizeof(char) );

			blockParsingResult_t * searchResult = NULL ;
			int searchResultLength = 0 ;
			if ( COMMON_SearchBlock( (char *)buf , fd_stat.st_size , "SeasonProps" , &searchResult , &searchResultLength) == OK )
			{
				if ( searchResult )
				{
					int searchResultIndex = 0 ;
					for ( ; searchResultIndex < searchResultLength ; ++searchResultIndex )
					{
						if ( strstr(searchResult[ searchResultIndex ].variable , "allowSeasonChange") )
						{
							unsigned char allowSeasonChange = atoi( searchResult[ searchResultIndex ].value );
							if ( (allowSeasonChange == STORAGE_SEASON_CHANGE_DISABLE) || (allowSeasonChange == STORAGE_SEASON_CHANGE_ENABLE) )
							{
								seasonProperties.allowChanging = allowSeasonChange ;
								res = OK ;
							}
							break;
						}
					}
				}
			}
			free(buf);
		}
		close(fd);
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	STORAGE_SeasonUnprotect();

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_LoadSeasonAllowFlag() res=[%s] seasonFlag=[%hhu]", getErrorCode(res) , seasonProperties.allowChanging);

	return res ;
}

//
//
//

int STORAGE_LoadCurrentSeason()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	STORAGE_SeasonProtect();

	int fd = open( STORAGE_PATH_CURRENT_SEASON , O_RDONLY );
	if ( fd > 0 )
	{
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);

		if( fd_stat.st_size > 0 )
		{
			char seasonAskii[8] ;
			memset( seasonAskii , 0 , 8 );

			if ( read( fd , &seasonAskii , 1 ) == 1 )
			{
				unsigned char season=(unsigned char)atoi( seasonAskii );

				if ( (season == STORAGE_SEASON_SUMMER) || (season == STORAGE_SEASON_WINTER) )
				{
					seasonProperties.currentSeason = season ;
					res = OK ;
				}
			}
		}

		close(fd);
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	STORAGE_SeasonUnprotect();

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_LoadCurrentSeason() res=[%s] currentSeason=[%hhu]", getErrorCode(res) , seasonProperties.currentSeason);

	return res ;
}

//
//
//

int STORAGE_UpdateSeasonAllowFlag( unsigned char allow )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;
	if ( (allow != STORAGE_SEASON_CHANGE_DISABLE) && (allow != STORAGE_SEASON_CHANGE_ENABLE) )
	{
		return res ;
	}

	STORAGE_SeasonProtect();

	seasonProperties.allowChanging = allow ;

	char buf[128];
	memset( buf , 0 , 128 );
	sprintf( buf , "[SeasonProps]\nallowSeasonChange=%hhu\n", allow ) ;

	int fd = open( CONFIG_PATH_SEASON , O_CREAT | O_WRONLY | O_TRUNC , 0777);
	if ( fd > 0 )
	{
		if ( write( fd , buf , strlen(buf) ) == (int)strlen(buf) )
		{
			STORAGE_IncrementHardwareWritingCtrlCounter();
			res = OK ;
		}
		close(fd);
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	STORAGE_SeasonUnprotect();

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_UpdateSeasonAllowFlag() res=[%s]", getErrorCode(res) );

	return res ;
}

//
//
//

int STORAGE_UpdateCurrentSeason( unsigned char season )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;
	STORAGE_SeasonProtect();

	seasonProperties.currentSeason = season ;

	char buf[8];
	memset( buf , 0 , 8 );
	sprintf( buf , "%hhu\n", season ) ;

	int fd = open( STORAGE_PATH_CURRENT_SEASON , O_CREAT | O_WRONLY | O_TRUNC , 0777);
	if ( fd > 0 )
	{
		if ( write( fd , buf , strlen(buf) ) == (int)strlen(buf) )
		{
			STORAGE_IncrementHardwareWritingCtrlCounter();
			res = OK ;
		}
		close(fd);
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	STORAGE_SeasonUnprotect();

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_UpdateCurrentSeason() res=[%s]", getErrorCode(res) );

	return res ;
}

//
//
//

unsigned char STORAGE_GetSeasonAllowFlag()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	STORAGE_SeasonProtect();
	unsigned char res = seasonProperties.allowChanging ;
	STORAGE_SeasonUnprotect();

	return res ;
}

//
//
//

unsigned char STORAGE_GetCurrentSeason()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	STORAGE_SeasonProtect();
	unsigned char res = seasonProperties.currentSeason ;
	STORAGE_SeasonUnprotect();

	return res ;
}

//
//
//

int STORAGE_LoadExio()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;
	struct stat st;

	STORAGE_Protect();

	int fd = open( EXIO_CFG_PATH , O_RDONLY);
	if ( fd < 0 )
	{
		fd = open( EXIO_CFG_PATH ".bak" , O_RDONLY );
	}

	if ( fd > 0 )
	{
		fstat(fd, &st);
		if (st.st_size != ZERO)
		{
			unsigned char * buf = malloc( st.st_size );
			if ( buf )
			{
				if ( read(fd , buf , st.st_size ) == st.st_size )
				{
					int discretIndex = 0 ;

					for( ; discretIndex < EXIO_DISCRET_OUTPUT_NUMB ; ++discretIndex )
					{
						blockParsingResult_t * blockParsingResult = NULL;
						int blockParsingResultSize = 0 ;

						unsigned char blockName[64];
						memset(blockName , 0 , 64);
						sprintf( (char *)blockName , "discret_output_%d" , (discretIndex + 1) );

						if (COMMON_SearchBlock((char *)buf , st.st_size ,(char *)blockName , &blockParsingResult , &blockParsingResultSize ) == OK )
						{

							int defaultPos = -1;
							int modePos = -1;

							int blockElementIndex = 0;
							for( ; blockElementIndex < blockParsingResultSize ; ++blockElementIndex )
							{
								if ( strcmp( blockParsingResult[ blockElementIndex ].variable , "mode" ) == 0 )
								{
									modePos = blockElementIndex ;
								}
								if ( strcmp( blockParsingResult[ blockElementIndex ].variable , "default" ) == 0 )
								{
									defaultPos = blockElementIndex ;
								}
							}

							if ( (defaultPos >= 0) && (modePos >= 0 ) )
							{
								res = OK;

								exioCfg.discretCfg[ discretIndex ].index = discretIndex + 1 ;

								if (  strcmp( blockParsingResult[ modePos ].value , "in") == 0 )
								{
									exioCfg.discretCfg[ discretIndex ].mode = EXIO_DISCRET_MODE_INPUT;
								}
								else
								{
									exioCfg.discretCfg[ discretIndex ].mode = EXIO_DISCRET_MODE_OUTPUT ;
								}

								if (  strcmp( blockParsingResult[ defaultPos ].value , "0") == 0 )
								{
									exioCfg.discretCfg[ discretIndex ].defaultValue = EXIO_DISCRET_DEFAULT_OFF;
								}
								else
								{
									exioCfg.discretCfg[ discretIndex ].defaultValue = EXIO_DISCRET_DEFAULT_ON ;
								}

							}
							else
							{
								res = ERROR_GENERAL;
							}

							free(blockParsingResult);
						}
						else
						{
							res = ERROR_GENERAL;
							break;
						}
					}

				}

				free(buf);
			}
			else
				res = ERROR_MEM_ERROR;
		}
		close(fd);
	}
	if ( res == OK )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadExio() exioCfg:");
		int i = 0 ;
		for( ; i < EXIO_DISCRET_OUTPUT_NUMB ; ++i)
		{
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "\tdiscret output N[%d], mode = [%d] , defaultValue = [%d]" , exioCfg.discretCfg[i].index , exioCfg.discretCfg[i].mode >> 3, exioCfg.discretCfg[i].defaultValue >> 1 );
		}
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_LoadExio() exit with errorCode [%s]", getErrorCode(res));

	STORAGE_Unprotect();

	return res ;
}

//
//
//

int STORAGE_GetExioCfg( exioCfg_t ** exioConf )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK ;

	STORAGE_Protect();

	//check exio config for valid
	int i = 0 ;
	for( ; i < EXIO_DISCRET_OUTPUT_NUMB ; ++ i )
	{
		if ( exioCfg.discretCfg[ i ].index != ( i + 1 ) )
		{
			res = ERROR_GENERAL ;
			break;
		}
	}

	if( res == OK )
	{
		*exioConf = &exioCfg;
	}

	STORAGE_Unprotect();

	return res ;
}




//
//
//

int STORAGE_GetPlcSettings(it700params_t * params){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;

	STORAGE_Protect();

	if (plcBaseSettings.netSizeLogical < 10){
		plcBaseSettings.netSizeLogical = 10;
	}

	if (plcBaseSettings.netSizePhisycal < 10){
		plcBaseSettings.netSizePhisycal = 10;
	}

	if (plcBaseSettings.admissionMode > PLC_ADMISSION_WITH_DETECT) /*|| ((*params)->admissionMode <PLC_ADMISSION_AUTO)*/{
		plcBaseSettings.admissionMode = PLC_ADMISSION_WITH_DETECT;
	}

	memcpy(params, &plcBaseSettings, sizeof(it700params_t));

	STORAGE_Unprotect();

	return res;
}

//
//
//

int STORAGE_CreateStatisticMap() {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL;
	int statIndex = 0;
	int counterIndex = 0;

	//STORAGE_Protect();
	STORAGE_StatisticProtect();

	//process plc counters
	for (; counterIndex < countersArrayPlc.countersSize; counterIndex++) {
		counterStatistic.countersStat = (counterStat_t **) realloc(counterStatistic.countersStat, (++(counterStatistic.countersStatCount) * sizeof (counterStat_t *)));
		if (counterStatistic.countersStat != NULL) {
			statIndex = counterStatistic.countersStatCount - 1;
			counterStatistic.countersStat[statIndex] = (counterStat_t *) malloc(sizeof (counterStat_t));
			if (counterStatistic.countersStat[statIndex] != NULL) {
				counterStatistic.countersStat[statIndex]->dbid = countersArrayPlc.counter[counterIndex]->dbId;
				counterStatistic.countersStat[statIndex]->serial = countersArrayPlc.counter[counterIndex]->serial;
				memcpy(counterStatistic.countersStat[statIndex]->serialRemote, countersArrayPlc.counter[counterIndex]->serialRemote, SIZE_OF_SERIAL+1);
				counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_NOT_IN_POOL_YET;
			} else
				SET_RES_AND_BREAK_WITH_MEM_ERROR

			} else
				SET_RES_AND_BREAK_WITH_MEM_ERROR
		}

	//process 485 counters
	counterIndex = 0;
	for (; counterIndex < countersArray485.countersSize; counterIndex++) {
		counterStatistic.countersStat = (counterStat_t **) realloc(counterStatistic.countersStat, (++(counterStatistic.countersStatCount) * sizeof (counterStat_t *)));
		if (counterStatistic.countersStat != NULL) {
			statIndex = counterStatistic.countersStatCount - 1;
			counterStatistic.countersStat[statIndex] = (counterStat_t *) malloc(sizeof (counterStat_t));
			if (counterStatistic.countersStat[statIndex] != NULL) {
				counterStatistic.countersStat[statIndex]->dbid = countersArray485.counter[counterIndex]->dbId;
				counterStatistic.countersStat[statIndex]->serial = countersArray485.counter[counterIndex]->serial;
				memset(counterStatistic.countersStat[statIndex]->serialRemote, 0 , SIZE_OF_SERIAL+1 );
				memcpy(counterStatistic.countersStat[statIndex]->serialRemote, "rs-485", strlen("rs-485"));
				counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_NOT_IN_POOL_YET;
			} else
				SET_RES_AND_BREAK_WITH_MEM_ERROR

			} else
				SET_RES_AND_BREAK_WITH_MEM_ERROR
		}

	//check error
	if (res != ERROR_MEM_ERROR)
		res = OK;

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_CreateStatisticMap(counters count %d) %s", counterStatistic.countersStatCount, getErrorCode(res));

	STORAGE_StatisticUnprotect();
	//STORAGE_Unprotect();

	return res;
}

//osipov, add get start of stats
int STORAGE_GetStatisticStartDts(unsigned long dbid, dateTimeStamp * dts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int statIndex = 0;

	STORAGE_StatisticProtect();

	//check counter presence in statistic array
	for (; statIndex < (int)counterStatistic.countersStatCount; statIndex++ ){
		if (counterStatistic.countersStat[statIndex]->dbid == dbid){
			//copy dts
			memcpy(dts, &counterStatistic.countersStat[statIndex]->poolRun, sizeof (dateTimeStamp));
		
			//return if present
			STORAGE_StatisticUnprotect();
			
			return OK;
		}
	}

	STORAGE_StatisticUnprotect();
	
	return ERROR_IS_NOT_FOUND;	
}


void STORAGE_AppendToStatisticMap(counter_t * counter) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int statIndex = 0;
	int counterIndex = 0;

	STORAGE_StatisticProtect();

	//check counter presence in statistic array
	for (; counterIndex < (int)counterStatistic.countersStatCount; counterIndex++ ){

		if (counterStatistic.countersStat[counterIndex]->dbid == counter->dbId){
			//return if present
			STORAGE_StatisticUnprotect();
			return;
		}
	}

	//append to stat in all other cases
	counterStatistic.countersStat = (counterStat_t **) realloc(counterStatistic.countersStat, (++(counterStatistic.countersStatCount) * sizeof (counterStat_t *)));
	if (counterStatistic.countersStat != NULL) {
		statIndex = counterStatistic.countersStatCount - 1;
		counterStatistic.countersStat[statIndex] = (counterStat_t *) malloc(sizeof (counterStat_t));
		if (counterStatistic.countersStat[statIndex] != NULL) {
			counterStatistic.countersStat[statIndex]->dbid = counter->dbId;
			counterStatistic.countersStat[statIndex]->serial = counter->serial;
			memcpy(counterStatistic.countersStat[statIndex]->serialRemote, counter->serialRemote, SIZE_OF_SERIAL+1);
			counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_NOT_IN_POOL_YET;
		}
	}

	STORAGE_StatisticUnprotect();
}

//
//
//

void STORAGE_DeleteStatisticMap() {
	DEB_TRACE(DEB_IDX_STORAGE)

	STORAGE_StatisticProtect();

	int statIndex;

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_DeleteStatisticMap(counters count %d) start", counterStatistic.countersStatCount);

	if (counterStatistic.countersStatCount > 0){
		for (statIndex=counterStatistic.countersStatCount-1; statIndex>=0; statIndex--){
			if (counterStatistic.countersStat[statIndex]!=NULL){
				free (counterStatistic.countersStat[statIndex]);
				counterStatistic.countersStat[statIndex] = NULL;
			}
		}

		counterStatistic.countersStatCount = 0;

		free (counterStatistic.countersStat);
		counterStatistic.countersStat = NULL;
	}

	STORAGE_StatisticUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_DeleteStatisticMap(counters count %d) stop", counterStatistic.countersStatCount);

}

//
//
//

int STORAGE_SetCounterStatistic(unsigned long counterDbId, BOOL inPool, unsigned int transactionIndex, unsigned char transactionResult) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	unsigned int statIndex = 0;
	BOOL wasFound = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetCounterStatus() enter");

	//STORAGE_Protect(); //?
	STORAGE_StatisticProtect();

	//found index in statistic of counter
	for (; statIndex < counterStatistic.countersStatCount; statIndex++) {
		if (counterStatistic.countersStat[statIndex]->dbid == counterDbId) {
			wasFound = TRUE;
			break;
		}
	}

	//check if we are really found counter
	if (wasFound == TRUE) {
		if (inPool == TRUE) {
			//fix time of poller start
			COMMON_GetCurrentDts(&counterStatistic.countersStat[statIndex]->poolRun);

			//setup counter in pool
			counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_IN_POOL;

		} else {

			//fix time of poller start
			COMMON_GetCurrentDts(&counterStatistic.countersStat[statIndex]->poolEnd);

			//setup transaction status if error
			if (transactionResult != TRANSACTION_COUNTER_POOL_DONE) {
				counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_NOT_IN_POOL_ERROR;
				counterStatistic.countersStat[statIndex]->pollErrorTransactionIndex = (unsigned char) (transactionIndex & 0xFF);
				counterStatistic.countersStat[statIndex]->pollErrorTransactionResult = transactionResult;
			} else {
				counterStatistic.countersStat[statIndex]->poolStatus = STATISTIC_NOT_IN_POOL;
			}
		}

		res = OK;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetCounterStatus(dbid %ld, ltr: %d) %s", counterDbId, transactionResult, getErrorCode(res));

	STORAGE_StatisticUnprotect();
	//STORAGE_Unprotect(); //?

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetCounterStatus() exit");

	return res;
}


//
//
//

int STORAGE_GetUnknownId (unsigned long counterDbId, int * unknownId){
	int res = ERROR_IS_NOT_FOUND;

	STORAGE_CountersProtect();

	counter_t * counter = NULL;
	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK){
		(*unknownId) = counter->unknownId;
	}

	STORAGE_CountersUnprotect();

	return res;
}

//
//
//

int STORAGE_FoundCounter(unsigned long counterDbId, counter_t ** counter) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounter() enter");

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &(*counter));

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounter() exit with res %s" , getErrorCode(res));

	return res;
}

//
//
//

int STORAGE_FoundCounterNoAtom(unsigned long counterDbId, counter_t ** counter) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	int sIndex;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounterNoAtom() enter");

	//found counter in 485 by dbid index
	sIndex = 0;
	for (; sIndex < countersArray485.countersSize; sIndex++) {
		if (countersArray485.counter[sIndex]->dbId == counterDbId) {
			(* counter) = countersArray485.counter[sIndex];

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounterNoAtom (found in 485) exit");

			return OK;
		}
	}

	//found counter in RF by dbid index
	sIndex = 0;
	for (; sIndex < countersArrayPlc.countersSize; sIndex++) {
		if (countersArrayPlc.counter[sIndex]->dbId == counterDbId) {
			(* counter) = countersArrayPlc.counter[sIndex];

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounterNoAtom (found in PLC) exit");

			return OK;
		}
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundCounterNoAtom () exit with res %s" , getErrorCode(res));

	return res;
}


//
//
//
int STORAGE_FoundPlcCounter(unsigned char * remoteSn, counter_t ** counter){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	int sIndex = 0;
	unsigned long sn1 = strtoul((char*)remoteSn, NULL, 10);
	unsigned long sn2 = 0;
	
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundPlcCounter() enter");

	STORAGE_CountersProtect();

	for (; sIndex < countersArrayPlc.countersSize; sIndex++) {
	
		sn2 =  strtoul((char *)countersArrayPlc.counter[sIndex]->serialRemote, NULL, 10);
		if (sn1 == sn2) {
			(* counter) = countersArrayPlc.counter[sIndex];

			STORAGE_CountersUnprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundPlcCounter() exit");

			return OK;
		}
	}

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_FoundPlcCounter() exit with res %s" , getErrorCode(res));

	return res;

}

//
//
//

void STORAGE_SetPlcCounterAged(unsigned char *remoteSn){
	DEB_TRACE(DEB_IDX_STORAGE)

	int sIndex = 0;
	unsigned long sn1 = strtoul((char*)remoteSn, NULL, 10);
	unsigned long sn2 = 0;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetPlcCounterAged() enter");

	STORAGE_CountersProtect();

	for (; sIndex < countersArrayPlc.countersSize; sIndex++) {
		sn2 =  strtoul((char *)countersArrayPlc.counter[sIndex]->serialRemote, NULL, 10);
		if (sn1 == sn2) {
			countersArrayPlc.counter[sIndex]->wasAged = TRUE;
			break;
		}
	}

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetPlcCounterAged()");
}

//
//
//

static int lastAgedIndex = -1;

int STORAGE_GetNextAgedPlcCounter(counter_t ** counter){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	int sIndex = lastAgedIndex + 1;

	if (sIndex >= countersArrayPlc.countersSize){
		sIndex = 0;
	}
	
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetNextAgedPlcCounter() enter");

	STORAGE_CountersProtect();

	for (; sIndex < countersArrayPlc.countersSize; sIndex++) {
		if (countersArrayPlc.counter[sIndex]->wasAged == TRUE) {
            countersArrayPlc.counter[sIndex]->wasAged = FALSE;
			(* counter) = countersArrayPlc.counter[sIndex];
			lastAgedIndex = sIndex;
			
			STORAGE_CountersUnprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetNextAgedPlcCounter() exit");

			return OK;
		}
	}
	
	sIndex = 0;
	for (; sIndex < lastAgedIndex; sIndex++) {
		if (countersArrayPlc.counter[sIndex]->wasAged == TRUE) {
			countersArrayPlc.counter[sIndex]->wasAged = FALSE;
			(* counter) = countersArrayPlc.counter[sIndex];
			lastAgedIndex = sIndex;
			
			STORAGE_CountersUnprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetNextAgedPlcCounter() exit");

			return OK;
		}
	}

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetNextAgedPlcCounter() exit with res %s" , getErrorCode(res));

	return res;

}

//
//
//

int STORAGE_SetupRatio(unsigned long counterDbId, unsigned int ratio) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	counter_t * counter = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupRatio() enter");

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK) {
		counter->ratioIndex = ratio;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupRatio(dbid %ld) %s", counterDbId, getErrorCode(res));

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupRatio() exit");

	return res;
}

//
//
//

int STORAGE_UpdateLastCounterProfileId(unsigned long counterDbId, unsigned int profileId) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	counter_t * counter = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_UpdateLastCounterProfileId() enter");

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK) {
		counter->lastProfileId = profileId;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_UpdateLastCounterProfileId(dbid %ld) %s", counterDbId, getErrorCode(res));

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_UpdateLastCounterProfileId() exit");

	return res;

}

//
//
//

unsigned int STORAGE_GetLastCounterProfileId(unsigned long counterDbId) {
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned int lastProfileId = 0;
	counter_t * counter = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastCounterProfileId() enter");

	STORAGE_CountersProtect();

	int res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK) {
		lastProfileId = counter->lastProfileId;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastCounterProfileId(dbid %ld) %s", counterDbId, getErrorCode(res));

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastCounterProfileId() exit");

	return lastProfileId;

}

//
//
//

int STORAGE_SetupProfileInterval(unsigned long counterDbId, unsigned char interval) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	counter_t * counter = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupProfileInterval() enter");

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK) {
		counter->profileInterval = interval;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupProfileInterval(dbid %ld) %s", counterDbId, getErrorCode(res));

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SetupProfileInterval() exit");

	return res;
}

//
//
//

void STORAGE_ResetRepeatFlag(unsigned long counterDbId){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	counter_t * counter = NULL;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ResetRepeatFlag() enter");

	STORAGE_CountersProtect();

	res = STORAGE_FoundCounterNoAtom(counterDbId, &counter);
	if (res == OK){
		if (counter != NULL){
			if(counter->lastTaskRepeateFlag == 1){
				counter->lastTaskRepeateFlag = 0;
				counter->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS;
			}
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ResetRepeatFlag(dbid %ld)", counterDbId);

	STORAGE_CountersUnprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ResetRepeatFlag() exit");
}

//
//
//
//void STORAGE_ResetWriteTarrifsFlag (unsigned long counterDbId){
//	counter_t * counter = NULL ;
//	if (STORAGE_FoundCounter(counterDbId , &counter) == OK){
//		if(counter){
//			counter->tariffMapWritingFlag = FALSE ;
//		}
//	}
//}

//
//
//
unsigned int STORAGE_CalcCrcForMeterage(meterage_t * data __attribute__((unused))){
	DEB_TRACE(DEB_IDX_STORAGE)


	/*
	typedef struct
	{
		unsigned int crc;
		dateTimeStamp dts;
		energy_t t[4];
		unsigned char status;
		unsigned int ratio;
		unsigned char delimeter;
	} meterage_t;
	*/

	//LSB mod 256
	//TODO
	//MSB xor


	/*
	int crc;
	int size = (4*4*4) + 6 + 4 + 1;

	unsigned char * l;
	unsigned char m256 = 0;
	unsigned char mXor = 0;

	l = (unsigned char * )malloc(size);
	if (l != NULL){
		memset(l, 0, size);
		l[0] = m->delimeter;
		memcpy(&l[1], &m->ratio, 4);
		memcpy(&l[5], &m->dts, 6);
		memcpy(&l[11], m->t, (4*4*4));


		printf ("L: ");
		for (int k=0; k<size; k++){
			m256+=l[k];
			mXor^=l[k]^m256;

			printf ("%02x ", l[k]);

		}
		printf ("\r\n");
	}

	crc = (mXor<<8) + m256;

	free (l);
	l = NULL;

	return crc;
	*/

	return -1;
}

//
//
//
unsigned int STORAGE_CalcCrcForProfile(profile_t * data){
	DEB_TRACE(DEB_IDX_STORAGE);

	unsigned int crc = 0 ;

	//int bufSize = sizeof(dateTimeStamp) + (sizeof(unsigned int) * 2) + sizeof( energy_t ) + sizeof(unsigned char) ;

	unsigned char buf[ 128 ];
	//unsigned char * buf = (unsigned char *)malloc( bufSize );
	//if ( buf )
	//{
		memset(buf , 0 , 128);

		int bufPos = 0 ;
		memcpy( &buf[ bufPos ] , &data->dts , sizeof( dateTimeStamp ) );
		bufPos += sizeof( dateTimeStamp ) ;

		memcpy( &buf[ bufPos ] , &data->id , sizeof( unsigned int ) );
		bufPos += sizeof( unsigned int ) ;

		memcpy( &buf[ bufPos ] , &data->p , sizeof( energy_t ) );
		bufPos += sizeof( energy_t ) ;

		memcpy( &buf[ bufPos ] , &data->status , sizeof( unsigned char ) ) ;
		bufPos += sizeof( unsigned char ) ;

		memcpy( &buf[ bufPos ] , &data->ratio , sizeof( unsigned int ) );
		bufPos += sizeof( unsigned int ) ;

		unsigned char m256 = 0 ;
		unsigned char mXor = 0 ;

		int idx = 0 ;
		for( ; idx < bufPos ; ++idx)
		{
			m256 += buf[idx];
			mXor ^= buf[idx] ^ m256 ;
		}

		crc = (mXor<<8) + m256;

		//free(buf);
		//buf = NULL ;
	//}
	return crc;
}

//
//
//
unsigned int STORAGE_CalcCrcForJournal(uspdEvent_t * event){
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned int res = 0xFF ;

	/*
	typedef struct
	{
		unsigned int crc;
		unsigned char evType;
		unsigned char evDesc[EVENT_DESC_SIZE];
		unsigned char status;
		dateTimeStamp dts;
	}uspdEvent_t;
	*/

	unsigned char m256 = 0;
	unsigned char mXor = 0;

	//int size = sizeof(unsigned char) + (sizeof(unsigned char) * EVENT_DESC_SIZE) + sizeof(dateTimeStamp);
	//unsigned char * buf = (unsigned char *)malloc( size * sizeof(unsigned char) );

	unsigned char buf[128];

	//if ( buf != NULL )
	//{
		memset( buf, 0 , 128 );

		int copyPos = 0;
		memcpy( &buf[copyPos] , &event->evType , sizeof(unsigned char) );
		copyPos+=sizeof(unsigned char) ;

		memcpy( &buf[copyPos] , &event->evDesc , EVENT_DESC_SIZE * sizeof(unsigned char) );
		copyPos+=sizeof(unsigned char) * EVENT_DESC_SIZE ;

		memcpy( &buf[copyPos] , &event->dts , sizeof(dateTimeStamp) );
		copyPos+=sizeof(dateTimeStamp) ;

		int idx;
		for(idx = 0 ; idx < copyPos ; idx++)
		{
			m256 += buf[idx] ;
			mXor ^= buf[idx] ^ m256 ;
		}

		res = (mXor<<8) + m256;
		//free(buf) ;
	//}



	return res;
}

//
//
//

int STORAGE_CalcCrcForHardwareWritingCtrlCounter(hardwareWritingCtrl_t * hardwareWritingCtrl , unsigned int * crc)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	if ( hardwareWritingCtrl && crc )
	{
		(*crc) = 0 ;

		//allocate temp buffer
		//unsigned char * buf = (unsigned char *)malloc( sizeof( hardwareWritingCtrl_t ) ) ;
		unsigned char buf[128];
		//if ( buf )
		//{
			//memset( buf , 0 , sizeof(hardwareWritingCtrl_t) );
			memset( buf , 0 , 128 );

			int bufPos = 0 ;

			memcpy( &buf[ bufPos ] , &hardwareWritingCtrl->counter , sizeof( unsigned int ) );
			bufPos += sizeof( unsigned int ) ;

			memcpy( &buf[ bufPos ] , &hardwareWritingCtrl->initDts , sizeof( dateTimeStamp ) );
			bufPos += sizeof( dateTimeStamp ) ;

			memcpy( &buf[ bufPos ] , &hardwareWritingCtrl->lastWritingDts , sizeof( dateTimeStamp ) );
			bufPos += sizeof( dateTimeStamp ) ;

			unsigned char m256 = 0 ;
			unsigned char mXor = 0 ;

			int idx = 0 ;
			for( ; idx < bufPos ; ++idx)
			{
				m256 += buf[idx];
				mXor ^= buf[idx] ^ m256 ;
			}

			(*crc) = (mXor<<8) + m256;

			res = OK ;
			//free( buf );
//		}
//		else
//		{
//			res = ERROR_MEM_ERROR ;
//		}
	}

	return res;
}

void STORAGE_IncrementHardwareWritingCtrlCounter()
{
	DEB_TRACE(DEB_IDX_STORAGE);

	hardwareWritingCtrl_t hardwareWritingCtrl ;

	//init the struct
	hardwareWritingCtrl.counter = 0 ;
	hardwareWritingCtrl.crc = 0 ;
	memset( &hardwareWritingCtrl.initDts , 0 , sizeof( dateTimeStamp ) );
	memset( &hardwareWritingCtrl.lastWritingDts , 0 , sizeof( dateTimeStamp ) );

	STORAGE_HardwareWritingCtrl_Protect();

	int fd = open( STORAGE_PATH_HARDWARE_WRITING_CTRL , O_RDWR | O_CREAT, S_IRWXU );
	if ( fd > 0 )
	{
		BOOL fillTheStructFlag = FALSE ;
		struct stat st;
		fstat(fd, &st);
		if (st.st_size == sizeof( hardwareWritingCtrl_t ) )
		{
			int bytesReaded = read( fd , &hardwareWritingCtrl , sizeof( hardwareWritingCtrl_t ) ) ;

			if ( bytesReaded == sizeof( hardwareWritingCtrl_t ) )
			{
				unsigned int crc = 0 ;
				if ( STORAGE_CalcCrcForHardwareWritingCtrlCounter( &hardwareWritingCtrl , &crc ) == OK )
				{
					if( crc == hardwareWritingCtrl.crc )
					{
						fillTheStructFlag = TRUE ;
					}
				}
			}
		}
		else
		{
			if ( st.st_size == 0 )
			{
				//there is first time we use the counter
				COMMON_GetCurrentDts( &hardwareWritingCtrl.initDts );
				fillTheStructFlag = TRUE ;
			}
		}

		if ( fillTheStructFlag == TRUE )
		{
			//fill the struct
			COMMON_GetCurrentDts( &hardwareWritingCtrl.lastWritingDts );
			hardwareWritingCtrl.counter += 2 ;
			if ( STORAGE_CalcCrcForHardwareWritingCtrlCounter( &hardwareWritingCtrl , &hardwareWritingCtrl.crc ) == OK )
			{
				lseek( fd , 0, SEEK_SET );
				write( fd , &hardwareWritingCtrl , sizeof( hardwareWritingCtrl_t ) );
			}
		}

		close( fd );
	}

	STORAGE_HardwareWritingCtrl_Unprotect();

	return ;
}

//
//
//

int STORAGE_SaveMeterages(int storage, unsigned long counterDbId, meterage_t *data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd;
	struct stat st;
	char file_name[SFNL];
	//counter_t * counter = NULL;
	meterage_t * data_in_file;
	int blocks = 0;
	int size_of_block = sizeof (meterage_t);
	int max_blocks = 0;
	int result;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() enter");

	if ( COMMON_CheckDtsForValid(&data->dts) != OK )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() datestamp is not valid");
		return ERROR_GENERAL ;
	}

	STORAGE_Protect();

	//check input pointer
	if (data == NULL) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages ERROR with NULL ptr to meterage");

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() exit");

		return ERROR_GENERAL;
	}

	//osipov 2051-03-13
	//not needed more

	//get counter by dbid
	//res = STORAGE_FoundCounter(counterDbId, &counter);
	//if (res == OK) {

		//get crc
		data->crc = STORAGE_CalcCrcForMeterage(data);

		//create file name and set file size limits
		switch (storage){
			case STORAGE_CURRENT:
				GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);

				max_blocks = 1;
				break;

			case STORAGE_DAY:
				GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);

				//if (strlen((char *)counter->serialRemote) == 0) {
				//	max_blocks = (STORAGE_MAX_DAY_RS485);
				//} else {
				//	max_blocks = (STORAGE_MAX_DAY_PLC);
				//}
				max_blocks = STORAGE_MAX_DAY;
				break;

			case STORAGE_MONTH:
				GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);

				//if (strlen((char *)counter->serialRemote) == 0) {
				//	max_blocks = (STORAGE_MAX_MONTH_RS485);
				//} else {
				//	max_blocks = (STORAGE_MAX_MONTH_PLC);
				//}

				max_blocks = STORAGE_MAX_MONTH;
				break;
		}

		//open link to file
		fd = open(file_name, O_RDWR | O_CREAT, S_IRWXU);
		if (fd == -1) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages ERROR with open path(%s)", file_name);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() exit");

			return ERROR_FILE_OPEN_ERROR;
		}

		//get file size
		fstat(fd, &st);

		//check file size
		if (st.st_size == ZERO) {

			//simple write first portion of data
			write(fd, data, size_of_block);
			STORAGE_IncrementHardwareWritingCtrlCounter();

			//setup return code
			res = OK;

		} else {
			//allocate mem according file size
			blocks = st.st_size / size_of_block;
			data_in_file = (meterage_t *)malloc((blocks+1) * size_of_block);
			if (data_in_file == NULL) {
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_Unprotect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() exit");

				return ERROR_MEM_ERROR;
			}

			//read file content
			result = read(fd, &data_in_file[1], blocks*size_of_block);

			//check read result
			if (result == (blocks*size_of_block)){

				//append new data
				memcpy(&data_in_file[0], data, size_of_block);
				blocks++;

				//seek fd to start
				if (lseek(fd, 0L, SEEK_SET) != 0) {
					//debug
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages ERROR with file update(%s)", file_name);

					//close file
					close(fd);

					STORAGE_Unprotect();

					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() exit");

					return ERROR_FILE_OPEN_ERROR;
				}

				//new
				if (blocks > max_blocks)
					blocks = max_blocks;

				write(fd, data_in_file, (blocks * size_of_block));
				STORAGE_IncrementHardwareWritingCtrlCounter();

			} else {
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages READ FILE error ");

				res = ERROR_FILE_OPEN_ERROR;
			}

			//free mem
			free(data_in_file);
			data_in_file = NULL;
		}

		//close file
		close(fd);
		
	//}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages (%s) %s", file_name, getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveMeterages() exit");

	return res;
}


//
//
//

int STORAGE_ProfileReconvert(unsigned long counterDbId){
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_old_t * data_in_file = NULL;
	profile_t * data_to_file = NULL;
	int result;
	int size_of_block = sizeof (profile_old_t);
	int size_of_new_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	//glue file name
	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ProfileReconvert ERROR with open path(%s)", file_name);

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ProfileReconvert() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = (profile_old_t *)malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ProfileReconvert ERROR with alloc mem()");

			//close file
			close(fd);

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){

			//allocate new array with new struct used
			data_to_file = (profile_t *)malloc (blocks * size_of_new_block);
			if (!data_to_file){
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ProfileReconvert ERROR with alloc mem() for new blocks");

				//close file
				close(fd);

				return ERROR_MEM_ERROR;
			}

			//zero mem
			memset(data_to_file, 0, blocks * size_of_new_block);

			int bId = 0;

			//loop on blocks
			for (blockIndex=(blocks-1); blockIndex >=0; blockIndex--){
				memcpy(&data_to_file[blockIndex].dts, &data_in_file[blockIndex].dts, sizeof(dateTimeStamp));
				data_to_file[blockIndex].ratio = data_in_file[blockIndex].ratio;
				data_to_file[blockIndex].status = data_in_file[blockIndex].status;
				memcpy(&data_to_file[blockIndex].p, &data_in_file[blockIndex].p, sizeof(energy_t));
				data_to_file[blockIndex].id = bId;
				data_to_file[blockIndex].crc = STORAGE_CalcCrcForProfile(&data_to_file[blockIndex]);

				bId++;
			}

			//set to 0
			lseek( fd , 0 , SEEK_SET );
			ftruncate( fd , 0 );

			//write new data
			write(fd, data_to_file, (blocks * size_of_new_block));
			STORAGE_IncrementHardwareWritingCtrlCounter();

			//free mem
			free(data_to_file);
			data_to_file = NULL;

			//setup res
			res = OK;

		} else {
			res = ERROR_FILE_FORMAT;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_ProfileReconvert() exit");

	return res;
}


//
//
//

int STORAGE_RefactoreProfileWithId(){
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = OK;
	int cIndex = 0;

	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	BOOL needConvert = FALSE;


#if REV_COMPILE_485
	//loop on 485
	cIndex = 0;
	for (;cIndex<countersArray485.countersSize; cIndex++){

		//reset flag
		needConvert = FALSE;

		//glue file name
		GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, countersArray485.counter[cIndex]->dbId);

		//try to open file
		fd = open(file_name, O_RDONLY);
		if (fd > 0) {

			//get file size
			fstat(fd, &st);

			//check file size
			if (st.st_size > 0){

				//try read 1 block
				result = read(fd, &data_in_file, size_of_block);

				//check fread result
				if (result == size_of_block){

					//check CRC
					unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file);
					if (blockCrc != data_in_file.crc){
						needConvert = TRUE;
					}


				} else {
					needConvert = TRUE;
				}

				//check flag
				if (needConvert == TRUE){
					COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "dbid %ld need to convert profile", countersArray485.counter[cIndex]->dbId );
					close(fd);
					res = STORAGE_ProfileReconvert(countersArray485.counter[cIndex]->dbId);
				}

			} else {
				close(fd);
			}
		}
	}
#endif

#if REV_COMPILE_PLC
	//loop on plc
	cIndex = 0;
	for (;cIndex<countersArrayPlc.countersSize; cIndex++){
		//reset flag
		needConvert = FALSE;

		//glue file name
		GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, countersArrayPlc.counter[cIndex]->dbId);

		//try to open file
		fd = open(file_name, O_RDONLY);
		if (fd > 0) {

			//get file size
			fstat(fd, &st);

			//check file size
			if (st.st_size > 0){

				//try read 1 block
				result = read(fd, &data_in_file, size_of_block);

				//check fread result
				if (result == size_of_block){

					//check CRC
					unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file);
					if (blockCrc != data_in_file.crc){
						needConvert = TRUE;
					}


				} else {
					needConvert = TRUE;
				}

				//check flag
				if (needConvert == TRUE){
					close(fd);
					COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "dbid %ld need to convert profile", countersArrayPlc.counter[cIndex]->dbId );
					res = STORAGE_ProfileReconvert(countersArrayPlc.counter[cIndex]->dbId);
				}

			} else {
				close(fd);
			}
		}
	}

#endif

	return res;
}

//
//
//

int STORAGE_SaveProfile(unsigned long counterDbId, profile_t *data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	//counter_t * counter = NULL;
	profile_t * data_in_file = NULL;
	int size_of_file = 0;
	int size_of_block = sizeof (profile_t);
	int blocks = 0;
	int max_blocks = STORAGE_MAX_PROFILE;
	int result;
	BOOL profileIsPresent = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() enter");

	if ( COMMON_CheckDtsForValid(&data->dts) != OK )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() dateStamp is not valid");
		return ERROR_GENERAL ;
	}

	profileIsPresent = STORAGE_IsProfilePresenceInStorage(counterDbId, &data->dts);
	if (profileIsPresent == TRUE){
		data->status |= METERAGE_DUBLICATED;
	}


	STORAGE_Protect();

	//check input pointer
	if (data == NULL) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile ERROR with NULL ptr to meterage");

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() exit");

		return ERROR_GENERAL;
	}

	//osipov 2051-03-13
	//not needed more
	
	//get counter by dbid
	//res = STORAGE_FoundCounter(counterDbId, &counter);
	//if (res == OK) {

		//create file name and set file size limits
		GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

		//if (strlen((char *)counter->serialRemote) == 0) {
		//	max_blocks = (STORAGE_MAX_PROFILE_RS485);
		//} else {
		//	max_blocks = (STORAGE_MAX_PROFILE_PLC);
		//}

		//open link to file
		fd = open(file_name, O_RDWR | O_CREAT, S_IRWXU);
		if (fd == -1) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile ERROR with open path(%s)", file_name);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() exit");

			return ERROR_FILE_OPEN_ERROR;
		}

		//get file size
		fstat(fd, &st);
		size_of_file = st.st_size;

		//check file size
		if (size_of_file == ZERO) {

			//set to nole
			data->id = ZERO;

			//get crc
			data->crc = STORAGE_CalcCrcForProfile(data);

			//simple write first portion of data
			write(fd, data, size_of_block);
			STORAGE_IncrementHardwareWritingCtrlCounter();

			//setup return code
			res = OK;

		} else {
			//allocate mem according file size
			blocks = st.st_size / size_of_block;
			data_in_file = (profile_t *)malloc((blocks+1)*size_of_block);
			if (data_in_file == NULL) {
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "SSTORAGE_SaveProfile ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_Unprotect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() exit");

				return ERROR_MEM_ERROR;
			}

			//read file content
			result = read(fd, &data_in_file[1], blocks*size_of_block);

			//check fread result
			if (result == (blocks * size_of_block)){

				//get id
				int nextId = data_in_file[1].id + 1;
				data->id = nextId;

				//get crc
				data->crc = STORAGE_CalcCrcForProfile(data);

				memcpy(&data_in_file[0], data, size_of_block);
				blocks++;

				//seek fd to start
				if (lseek(fd, 0L, SEEK_SET) != 0) {
					//debug
					COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile ERROR with file update(%s)", file_name);

					//close file
					close(fd);

					STORAGE_Unprotect();

					COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() exit");

					return ERROR_FILE_OPEN_ERROR;
				}

				//write back changes
				if (blocks > max_blocks)
					blocks = max_blocks;

				write(fd, data_in_file, (blocks * size_of_block));
				STORAGE_IncrementHardwareWritingCtrlCounter();

			} else {
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile READ FILE error ");

				res = ERROR_FILE_OPEN_ERROR;
			}

			//free mem
			free(data_in_file);
			data_in_file = NULL;

		}

		//close file
		close(fd);
	//}

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile (%s) with DTS = [" DTS_PATTERN "] %s ", file_name, DTS_GET_BY_VAL(data->dts) , getErrorCode(res));

	//printf( "\n\nSTORAGE_SaveProfile (%s) with DTS = [" DTS_PATTERN "] with crc [%08X] %s\n\n", file_name, DTS_GET_BY_VAL(data->dts) , data->crc , getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_PROFILE "STORAGE_SaveProfile() exit");

	return res;
}

//
//
//

int STORAGE_GetMeterages(int storage, unsigned long counterDbId, dateTimeStamp date_from, dateTimeStamp date_to, meteregesArray_t ** data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_NO_DATA_FOUND;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	meterage_t * data_in_file = NULL;
	int result;
	int size_of_block = sizeof (meterage_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() enter");

	STORAGE_Protect();

	switch (storage){
		case STORAGE_CURRENT:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);
			break;

		case STORAGE_DAY:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);
			break;

		case STORAGE_MONTH:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);
			break;
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() try open file %s", file_name);

	//open link to file
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = (meterage_t *)malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() blocks presetn %d", blocks);

		if (result == (blocks * size_of_block)){

			//allocate array
			(*data) = (meteregesArray_t *) malloc(sizeof(meteregesArray_t));

			if ((*data) == NULL){
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_Unprotect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() exit");

				return ERROR_MEM_ERROR;
			}

			//setup meterages pointers
			(*data)->meteragesCount = 0;
			(*data)->meterages = NULL;

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {
				BOOL dateInRange = FALSE;

				//check block crc
				unsigned int blockCrc = STORAGE_CalcCrcForMeterage(&data_in_file[blockIndex]);
				if (blockCrc == data_in_file[blockIndex].crc){

					//check storage values dts in requested range
					dateInRange = COMMON_DtsInRange(&data_in_file[blockIndex].dts, &date_from, &date_to);
					if (dateInRange == TRUE) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() dates in range TRUE");

						//append storage value
						(*data)->meterages = (meterage_t **) realloc((*data)->meterages, (++(*data)->meteragesCount) * sizeof(meterage_t *));
						if ((*data)->meterages != NULL) {
							(*data)->meterages[(*data)->meteragesCount - 1] = (meterage_t *) malloc(size_of_block);
							if (((*data)->meterages[(*data)->meteragesCount - 1]) != NULL) {
								memcpy((*data)->meterages[(*data)->meteragesCount - 1], &data_in_file[blockIndex], size_of_block);
								res = OK;
							} else
								SET_RES_AND_BREAK_WITH_MEM_ERROR
						} else
							SET_RES_AND_BREAK_WITH_MEM_ERROR

					} else {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() dates [" DTS_PATTERN "] is not in range from",DTS_GET_BY_VAL(data_in_file[blockIndex].dts)  );
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "\t[" DTS_PATTERN "] to", DTS_GET_BY_VAL(date_from));
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "\t[" DTS_PATTERN "]", DTS_GET_BY_VAL(date_to));

					}
				} else {

					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() block with wrong crc skipped");
				}
			}
		} else {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages READ FILE error ");

			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = ERROR_NO_DATA_YET;
	}

	//close file
	close(fd);

	//debug
	if ( (*data) )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages (total: %d) %s", (*data)->meteragesCount, getErrorCode(res));
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages (total: 0) %s", getErrorCode(res));
	}

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeterages() exit");

	//
	//debug
	//
//	if(res == ERROR_NO_DATA_FOUND)
//		EXIT_PATTERN;

	return res;
}

//
//
//

int STORAGE_GetMeteragesForDb(int storage, unsigned long counterDbId, meteregesArray_t ** data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_NO_DATA_FOUND;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	meterage_t * data_in_file = NULL;
	int result;
	int size_of_block = sizeof (meterage_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() enter");

	STORAGE_Protect();

	switch (storage){
		case STORAGE_CURRENT:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);
			break;

		case STORAGE_DAY:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);
			break;

		case STORAGE_MONTH:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);
			break;
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() try open file %s", file_name);

	//open link to file
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = (meterage_t *)malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() blocks presetn %d", blocks);

		if (result == (blocks * size_of_block)){

			//allocate array
			(*data) = (meteregesArray_t *) malloc(sizeof(meteregesArray_t));

			if ((*data) == NULL){
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_Unprotect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() exit");

				return ERROR_MEM_ERROR;
			}

			//setup meterages pointers
			(*data)->meteragesCount = 0;
			(*data)->meterages = NULL;

			//loop on saved values
			for (blockIndex = (blocks-1); blockIndex >=0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForMeterage(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if ( !( data_in_file[blockIndex].status & METERAGE_IN_DB_MASK) ) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() not in DB, so place to send");

						//append storage value
						(*data)->meterages = (meterage_t **) realloc((*data)->meterages, (++(*data)->meteragesCount) * sizeof(meterage_t *));
						if ((*data)->meterages != NULL) {
							(*data)->meterages[(*data)->meteragesCount - 1] = (meterage_t *) malloc(size_of_block);
							if (((*data)->meterages[(*data)->meteragesCount - 1]) != NULL) {
								memcpy((*data)->meterages[(*data)->meteragesCount - 1], &data_in_file[blockIndex], size_of_block);
								res = OK;
							} else
								SET_RES_AND_BREAK_WITH_MEM_ERROR
						} else
							SET_RES_AND_BREAK_WITH_MEM_ERROR

					} else {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() dates in range FALSE");
					}
				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() block with wrong crc skipped");
				//}
			}
		} else {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb READ FILE error ");

			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = ERROR_NO_DATA_YET;
	}

	//close file
	close(fd);

	//debug
	if ( (*data) )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb (total: %d) %s", (*data)->meteragesCount, getErrorCode(res));
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb (total: 0) %s", getErrorCode(res));
	}

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetMeteragesForDb() exit");

	return res;
}

//
//
//

int STORAGE_MarkMeteragesAsPlacedToDb(int storage, unsigned long counterDbId, dateTimeStamp lastDts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	//int res = ERROR_NO_DATA_FOUND;
	//ukhov
	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	meterage_t * data_in_file = NULL;
	int result;
	int size_of_block = sizeof (meterage_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() enter");

	STORAGE_Protect();

	switch (storage){
		case STORAGE_CURRENT:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);
			break;

		case STORAGE_DAY:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);
			break;

		case STORAGE_MONTH:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);
			break;
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() try open file %s", file_name);

	//open link to file
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = (meterage_t *)malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() blocks presetn %d", blocks);

		if (result == (blocks * size_of_block)){

			//loop on saved values
			for (blockIndex = (blocks-1); blockIndex >=0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForMeterage(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (!(data_in_file[blockIndex].status & METERAGE_IN_DB_MASK) ) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() not in DB, so place to send");

						//check storage values dts in requested range
						dateInRange = COMMON_DtsIsLower(&data_in_file[blockIndex].dts, &lastDts);
						if (dateInRange == TRUE) {

							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() dates in range TRUE");

							data_in_file[blockIndex].status += METERAGE_IN_DB_MASK;
							needUpdate = TRUE;

						} else {

							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() dates in range FALSE");
						}
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() block with wrong crc skipped");
				//}
			}

			//update
			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();
				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() error lseek");
				}
			}

		} else {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb READ FILE error ");

			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		//res = ERROR_NO_DATA_YET;
		//ukhov
		res = OK ;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb %s", /*(*data)->meteragesCount,*/ getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkMeteragesAsPlacedToDb() exit");

	return res;
}

//
//
//

int STORAGE_RecoveryMetragesPlasedToDbFlag(int storage, unsigned long counterDbId, dateTimeStamp olderDts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	meterage_t * data_in_file = NULL;
	int result;
	int size_of_block = sizeof (meterage_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() enter");

	STORAGE_Protect();

	switch (storage){
		case STORAGE_CURRENT:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);
			break;

		case STORAGE_DAY:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);
			break;

		case STORAGE_MONTH:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);
			break;
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() try open file %s", file_name);

	//open link to file
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = (meterage_t *)malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() blocks presetn %d", blocks);

		if (result == (blocks * size_of_block)){

			//loop on saved values
			for (blockIndex = (blocks-1); blockIndex >=0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForMeterage(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (data_in_file[blockIndex].status & METERAGE_IN_DB_MASK) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() marked \"in DB\", so place to send");

						//check storage values dts in requested range
						dateInRange = COMMON_DtsIsLower( &olderDts , &data_in_file[blockIndex].dts);
						if (dateInRange == TRUE) {

							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() dates in range TRUE");

							data_in_file[blockIndex].status -= METERAGE_IN_DB_MASK;
							needUpdate = TRUE;

						} else {

							COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() dates in range FALSE");
						}
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() block with wrong crc skipped");
				//}
			}

			//update
			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();
				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() error lseek");
				}
			}

		} else {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag READ FILE error ");

			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag %s", /*(*data)->meteragesCount,*/ getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryMetragesPlasedToDbFlag() exit");

	return res;
}

//
//
//

int STORAGE_GetProfile(unsigned long counterDbId, dateTimeStamp date_from, dateTimeStamp date_to, energiesArray_t ** data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t * data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() enter");

	STORAGE_Protect();

	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, &data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {
				BOOL dateInRange = FALSE;

				//check block crc
				unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
				if (blockCrc == data_in_file[blockIndex].crc){

					//check storage values dts in requested range
					dateInRange = COMMON_DtsInRange(&(data_in_file[blockIndex].dts), &date_from, &date_to);
					if (dateInRange == TRUE) {
						//append storage value
						(*data)->energies = (profile_t **) realloc((*data)->energies, (++(*data)->energiesCount) * sizeof(profile_t *));
						if ((*data)->energies != NULL) {
							(*data)->energies[(*data)->energiesCount - 1] = (profile_t *) malloc(size_of_block);
							if (((*data)->energies[(*data)->energiesCount - 1]) != NULL)
								memcpy(&(*data)->energies[(*data)->energiesCount - 1], &data_in_file[blockIndex], size_of_block);
							else
								SET_RES_AND_BREAK_WITH_MEM_ERROR
						} else
							SET_RES_AND_BREAK_WITH_MEM_ERROR
					}

				} else {

					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() block with wrong crc skipped");
				}
			}
		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	if ( (*data) )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile (total: %d) %s", (*data)->energiesCount, getErrorCode(res));
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile (total: 0) %s", getErrorCode(res));
	}

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfile() exit");

	return res;
}

//
//
//

int STORAGE_GetProfileForDb(unsigned long counterDbId, energiesArray_t ** data) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t * data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	unsigned int lastGettedId = ZERO;
	unsigned char lastGettedIdReady = 0;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() enter");

	STORAGE_Protect();

	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){

			//
			//------------------------
			//

			(*data) =  (energiesArray_t *)malloc( sizeof( energiesArray_t ) );
			if ( (*data) == NULL )
			{
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_Unprotect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() exit");

				return ERROR_MEM_ERROR;
			}


			//
			//-------------------------
			//

			(*data)->energiesCount = 0 ;
			(*data)->energies = NULL ;

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >=0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (!(data_in_file[blockIndex].status & METERAGE_IN_DB_MASK)) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() not in DB, so place to send");

						//append storage value
						(*data)->energies = (profile_t **) realloc((*data)->energies, (++((*data)->energiesCount)) * sizeof(profile_t *));

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() energiesCount = [%d]" , (*data)->energiesCount );

						if ((*data)->energies != NULL) {
							(*data)->energies[(*data)->energiesCount - 1] = (profile_t *) malloc(size_of_block);
							if (((*data)->energies[(*data)->energiesCount - 1]) != NULL)
							{
								//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE try copy block with date %u-%u-%u" , data_in_file[blockIndex].dts.d.d , data_in_file[blockIndex].dts.d.m , data_in_file[blockIndex].dts.d.y );

								memcpy((*data)->energies[(*data)->energiesCount - 1], &data_in_file[blockIndex], size_of_block);
								//COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE copy block with date %u-%u-%u" , (*data)->energies[ (*data)->energiesCount - 1 ]->dts.d.d , (*data)->energies[ (*data)->energiesCount - 1 ]->dts.d.m , (*data)->energies[ (*data)->energiesCount - 1 ]->dts.d.y);

								lastGettedId = data_in_file[blockIndex].id;
								lastGettedIdReady = 1;
							}
							else
								SET_RES_AND_BREAK_WITH_MEM_ERROR
						} else
							SET_RES_AND_BREAK_WITH_MEM_ERROR
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() block with wrong crc skipped");
				//}
			}

		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	if ( (*data) )
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb (total: %d) %s", (*data)->energiesCount, getErrorCode(res));
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb (total: %0) %s", getErrorCode(res));
	}

	STORAGE_Unprotect();

	if (lastGettedIdReady == 1)
		STORAGE_UpdateLastCounterProfileId(counterDbId, lastGettedId);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetProfileForDb() exit");

	return res;
}

//
//
//

BOOL STORAGE_IsProfilePresenceInStorage(unsigned long counterDbId, dateTimeStamp * checkDts)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	BOOL res = FALSE;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t * data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() enter");

	STORAGE_Protect();

	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() exit");

		return res;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() exit");

			EXIT_PATTERN;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){			

			//loop on saved values			
			for (blockIndex=0; blockIndex < blocks ; ++blockIndex) {
				//check block crc
				unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
				if (blockCrc == data_in_file[blockIndex].crc){

					if ( data_in_file[blockIndex].status == METERAGE_OUT_OF_ARCHIVE_BOUNDS )
					{
						break;
					}
					if ( COMMON_DtsAreEqual( &data_in_file[blockIndex].dts , checkDts ))
					{
						res = TRUE ;
						break ;
					}

				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() block with wrong crc skipped");
				}
			}
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	}

	//close file
	close(fd);	

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsProfilePresenceInStorage() exit");

	return res;
}

//
//
//

int STORAGE_MarkProfileAsPlacedToDb(unsigned long counterDbId, dateTimeStamp dtsLast) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t * data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;
	unsigned int lastProfileId = 0;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() enter");

	STORAGE_Protect();

	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){

			//get lastProfileId
			lastProfileId = STORAGE_GetLastCounterProfileId(counterDbId);

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {

				//check block crc
				unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
				if (blockCrc == data_in_file[blockIndex].crc){

					if (data_in_file[blockIndex].id <= lastProfileId){
						if (!(data_in_file[blockIndex].status & METERAGE_IN_DB_MASK)) {

							//check storage values dts in requested range
							dateInRange = COMMON_DtsIsLower(&(data_in_file[blockIndex].dts), &dtsLast);
							if (dateInRange == TRUE) {
								data_in_file[blockIndex].status += METERAGE_IN_DB_MASK;
								data_in_file[blockIndex].crc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
								needUpdate = TRUE;
							}
						}
					}

				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() block with wrong crc skipped");
				}
			}

			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();
				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() error lseek");
				}
			}

		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb %s", getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkProfileAsPlacedToDb() exit");

	return res;
}

//
//
//

int STORAGE_RecoveryProfilePlacedToDbFlag(unsigned long counterDbId, dateTimeStamp olderDts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	char file_name[SFNL];
	profile_t * data_in_file;
	int result;
	int size_of_block = sizeof (profile_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() enter");

	STORAGE_Protect();

	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_Unprotect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == blocks * size_of_block){

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {

				//check block crc
				
				unsigned int blockCrc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
				if (blockCrc == data_in_file[blockIndex].crc){

					if (data_in_file[blockIndex].status & METERAGE_IN_DB_MASK) {

						//check storage values dts in requested range
						dateInRange = COMMON_DtsIsLower(&olderDts , &(data_in_file[blockIndex].dts));
						if (dateInRange == TRUE) {
							data_in_file[blockIndex].status -= METERAGE_IN_DB_MASK;
							data_in_file[blockIndex].crc = STORAGE_CalcCrcForProfile(&data_in_file[blockIndex]);
							needUpdate = TRUE;
						}
					}

				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() block with wrong crc skipped");
				}
			}

			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();

				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() error lseek");
				}
			}

		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag %s", getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryProfilePlacedToDbFlag() exit");

	return res;
}

//
//
//
int STORAGE_GetLastMeterageDts(int storage, unsigned long counterDbId, dateTimeStamp * dts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	int fd;
	struct stat st;
	char file_name[SFNL];
	meterage_t block;
	const int blockSize =  sizeof (meterage_t);

	memset(&block, 0 , blockSize );
	memset(&st, 0 , sizeof(struct stat));

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastMeterageDts() enter");

	STORAGE_Protect();

	switch (storage){
		case STORAGE_CURRENT:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_CURRENT, counterDbId);
			break;

		case STORAGE_DAY:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_DAY, counterDbId);
			break;

		case STORAGE_MONTH:
			GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_MONTH, counterDbId);
			break;
	}

	//open link to file
	if ( (fd = open(file_name, O_RDONLY)) < 0)
	{
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastMeterageDts ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastMeterageDts() exit");

		return res;
	}

	//get file size
	fstat(fd, &st);
	if (
			(st.st_size >= blockSize) && \
			(read(fd, &block, blockSize) == blockSize) && \
			(STORAGE_CalcCrcForMeterage(&block) == block.crc)
		)
	{
		memcpy(dts, &block.dts, sizeof(dateTimeStamp));
		res = OK ;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastMeterageDts (dbid: %08ld storage: %d last %02d.%02d.%02d) %s", counterDbId, storage, dts->d.d, dts->d.m, dts->d.y, getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastMeterageDts() exit");

	return res;
}

//
// need to be optimized with fread loop
//

int STORAGE_GetLastProfileDts(unsigned long counterDbId, dateTimeStamp * dts) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_IS_NOT_FOUND;
	int fd;
	struct stat st;
	char file_name[SFNL];
	profile_t block;
	const int blockSize =  sizeof (profile_t);

	memset(&block, 0 , blockSize );
	memset(&st, 0 , sizeof(struct stat));


	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastProfileDts() enter");

	STORAGE_Protect();

	//create file name
	GET_STORAGE_FILE_NAME(file_name, STORAGE_PATH_PROFILE, counterDbId);

	//open link to file
	if ( (fd = open(file_name, O_RDONLY)) < 0 )
	{
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastProfileDts ERROR with open path(%s)", file_name);

		STORAGE_Unprotect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastProfileDts() exit");

		return res;
	}

	//get file size
	fstat(fd, &st);

	if (
			(st.st_size >= blockSize) && \
			(read(fd, &block, blockSize) == blockSize) && \
			(STORAGE_CalcCrcForProfile(&block) == block.crc)
		)
	{
		memcpy(dts, &block.dts, sizeof(dateTimeStamp));
		res = OK ;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastProfileDts (dbid: %08ld last %02d.%02d.%02d) %s", counterDbId, dts->d.d, dts->d.m, dts->d.y, getErrorCode(res));

	STORAGE_Unprotect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetLastProfileDts() exit");

	return res;
}

//
//
//

BOOL STORAGE_CheckTimeToRequestProfile( counter_t * counter, unsigned int * ptr, dateTimeStamp * dtsToRequest, int * canHoursRead )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = FALSE ;
	int cttrpState ;
	int stampsDiff = 0 ;

	enum cttrp_e
	{
		CTTRP_CHECK_MY_PTR,
		CTTRP_CHECK_STORAGE,
		CTTRP_FILL_MOUNT,
		CTTRP_CHECK_MAX_DEEP,
		CTTRP_CHECK_DIFFS,
		CTTRP_CHECK_PTRS,
		CTTRP_END
	};

	if ( COMMON_CheckProfileIntervalForValid( counter->profileInterval ) != OK )
	{
		return FALSE ;
	}

	cttrpState = CTTRP_CHECK_MY_PTR ;

	while (cttrpState != CTTRP_END) {
		switch( cttrpState )
		{
			case CTTRP_CHECK_MY_PTR:
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MY PTR"); //new profile debug
				if ( *ptr == POINTER_EMPTY ){
					cttrpState = CTTRP_CHECK_STORAGE ;
				}
				else
				{
					cttrpState = CTTRP_CHECK_PTRS ;
				}
				break;

			case CTTRP_CHECK_STORAGE:
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK STORE"); //new profile debug
				if ( STORAGE_GetLastProfileDts( counter->dbId , dtsToRequest ) == OK )
				{
					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK STORE OK dts  in store " DTS_PATTERN, DTS_GET_BY_PTR(dtsToRequest)); //new profile debug

					int i = 0 ;
					for ( ; i < counter->profileInterval ; ++i){
						COMMON_ChangeDts( dtsToRequest , INC , BY_MIN );
					}

					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : NEED dts will be " DTS_PATTERN, DTS_GET_BY_PTR(dtsToRequest)); //new profile debug

					cttrpState = CTTRP_CHECK_MAX_DEEP ;
				}
				else
				{
					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK STORE ERR"); //new profile debug
					cttrpState = CTTRP_FILL_MOUNT ;
				}
				break;

			case CTTRP_FILL_MOUNT:
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : FILL MOUNT"); //new profile debug
				memcpy( &dtsToRequest->d, &counter->mountD, sizeof(dateStamp) );
				memset( &dtsToRequest->t , 0 , sizeof(timeStamp));
				cttrpState = CTTRP_CHECK_MAX_DEEP ;
				break;

			case CTTRP_CHECK_MAX_DEEP:
			{
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MAXDEEP"); //new profile debug
				int maxDeep = 0 ;
				stampsDiff = COMMON_GetDtsDiff__Alt( dtsToRequest, &counter->currentDts , BY_HOUR );

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MAXDEEP REQ DTS" DTS_PATTERN,  DTS_GET_BY_PTR(dtsToRequest)); //new profile debug
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MAXDEEP CUR DTS" DTS_PATTERN,  DTS_GET_BY_VAL(counter->currentDts)); //new profile debug
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MAXDEEP stampsdiff %d", stampsDiff);

				if ( !counter->profilePtrCurrOverflow ){
					maxDeep = counter->profileCurrPtr / ( ((60 / counter->profileInterval) + 1) * 8 ) ;
				}else{
					maxDeep = COMMON_GetStorageDepth(counter, STORAGE_PROFILE) ;
				}

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK MAXDEEP maxdeep %d", maxDeep);

				if ( stampsDiff > maxDeep ){
					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : FIX DEEP"); //new profile debug

					while ( stampsDiff > maxDeep )
					{
						COMMON_ChangeDts( dtsToRequest , INC , BY_HOUR );
						stampsDiff--;
					}

					dtsToRequest->t.h = 0;
				}

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : FIXED REQ DTS" DTS_PATTERN,  DTS_GET_BY_PTR(dtsToRequest)); //new profile debug

				cttrpState = CTTRP_CHECK_DIFFS ;
			}
				break;

			case CTTRP_CHECK_PTRS:
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK PTRS"); //new profile debug
				if ( !counter->profilePtrCurrOverflow && (*ptr > counter->profileCurrPtr ) ){
					cttrpState = CTTRP_CHECK_STORAGE ;
				}
				else
				{
					cttrpState = CTTRP_END ;

					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : INC POINTER"); //new profile debug

					*ptr = UJUK_GetNextProfileHeadPtr( counter , *ptr );
					if ( *ptr != counter->profileCurrPtr ){
						res = TRUE ;
					}

					if (canHoursRead)
					{
						//check max hours can read
						int maxPtrToRead = counter->profileCurrPtr;
						if ((*ptr) > counter->profileCurrPtr )
						{
							maxPtrToRead = UJUK_GetProfileMaxPtr(counter);
						}

						*canHoursRead = (maxPtrToRead - (*ptr)) / (8 * ((60 / counter->profileInterval) + 1 ));
					}
				}
				break;

			case CTTRP_CHECK_DIFFS:
				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE ITTRP : CHECK DIFFS %u", stampsDiff); //new profile debug

				cttrpState = CTTRP_END ;
				if ( stampsDiff > 1 )
				{
					res = TRUE ;
					*ptr = POINTER_EMPTY ;
				}
				break;
		}
	}

	return res ;
}

//
//
//

BOOL STORAGE_CheckTimeToRequestMeterage ( counter_t * counter , int storage , dateTimeStamp * dtsToRequest)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = FALSE ;

	if ( (counter == NULL )|| ( dtsToRequest == NULL ) )
	{
		return res ;
	}

	dateTimeStamp currentCounterDts , lastMetrageDts;
	memset(&lastMetrageDts , 0 , sizeof(dateTimeStamp) );
	memcpy(&currentCounterDts , &counter->currentDts , sizeof(dateTimeStamp));

	int mountDflag = 0;
	if ( COMMON_CheckDtsForValid(&currentCounterDts) == OK )
	{
		if  ( (( currentCounterDts.t.h == 23 ) && (currentCounterDts.t.m > 55)) || \
			  (( currentCounterDts.t.h == 0 ) && (currentCounterDts.t.m < 5)) )
		{
			return res ;
		}

		switch( storage )
		{
			case STORAGE_CURRENT:
			{
				if ( STORAGE_GetLastMeterageDts( STORAGE_CURRENT , counter->dbId, &lastMetrageDts) != OK ) {
					res = TRUE;

				} else {

					int diff = COMMON_GetDtsDiff__Alt(&lastMetrageDts , &currentCounterDts , BY_HOUR);
					if ((diff < 0) || (diff > 12))
					{
						res = TRUE;
					}
				}
			}
			break;

			case STORAGE_DAY:
			{
				if ( STORAGE_GetLastMeterageDts( STORAGE_DAY , counter->dbId, &lastMetrageDts) != OK )
				{
					memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
					mountDflag = 1;
				}
				else
				{
					dateTimeStamp mountDts;
					memset(&mountDts , 0 , sizeof(dateTimeStamp));
					memcpy(&mountDts.d , &counter->mountD , sizeof(dateStamp));
					if( COMMON_CheckDts( &mountDts , &lastMetrageDts ) == FALSE )
					{
						memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
						mountDflag = 1;
					}
				}

				if ( COMMON_CheckDtsForValid(&lastMetrageDts) == OK  )
				{
					//calculating differense between last meterage dts and current counter dts
					currentCounterDts.t.s= 0;
					currentCounterDts.t.m= 0;
					currentCounterDts.t.h= 0;

					lastMetrageDts.t.s = 0 ;
					lastMetrageDts.t.m = 0 ;
					lastMetrageDts.t.h = 0 ;

					int diff = COMMON_GetDtsDiff__Alt(&lastMetrageDts , &currentCounterDts , BY_DAY);
					//if counter have time in past
					if (diff < 0)
					{
						//debug
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter DTS is in the past [" DTS_PATTERN "]" , DTS_GET_BY_VAL(currentCounterDts));

						if (mountDflag == 1)
							mountDflag = 0;
					}
					if ((diff > 0) || (mountDflag == 1))
					{
						res = TRUE ;
						if (mountDflag == 0)
						{
							COMMON_ChangeDts(&lastMetrageDts , INC , BY_DAY);
						}

						memcpy(dtsToRequest , &lastMetrageDts , sizeof(dateTimeStamp) );
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE It is time to ask day meterage from [" DTS_PATTERN "]", DTS_GET_BY_PTR(dtsToRequest) );
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE last day meterage DTS is invalid" );
				}
			}
			break;

			case STORAGE_MONTH:
			{
				if ( STORAGE_GetLastMeterageDts( STORAGE_MONTH , counter->dbId, &lastMetrageDts) != OK )
				{
					//printf("\tSTORAGE_GetLastMeterageDts returned ERROR.");
					memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
					lastMetrageDts.d.d = 1 ;
					mountDflag = 1;
				}
				else
				{
					//printf("\tSTORAGE_GetLastMeterageDts returned OK.");
					dateTimeStamp mountDts;
					memset(&mountDts , 0 , sizeof(dateTimeStamp));
					memcpy(&mountDts.d , &counter->mountD , sizeof(dateStamp));
					mountDts.d.d = 1 ;
					if( COMMON_CheckDts( &mountDts , &lastMetrageDts ) == FALSE )
					{
						//printf("\tCOMMON_CheckDts returned FALSE\n");
						//printf("\tmountDts" DTS_PATTERN "\n" , DTS_GET_BY_VAL(mountDts) );
						//printf("lastMetrageDts" DTS_PATTERN "\n" , DTS_GET_BY_VAL(lastMetrageDts) );
						memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
						lastMetrageDts.d.d = 1 ;
						mountDflag = 1;
					}
				}

				if ( COMMON_CheckDtsForValid(&lastMetrageDts) == OK  )
				{
					//calculating differense between last meterage dts and current counter dts
					currentCounterDts.t.s= 0;
					currentCounterDts.t.m= 0;
					currentCounterDts.t.h= 0;
					currentCounterDts.d.d= 1;

					if ( mountDflag == 0 )
					{
						lastMetrageDts.t.s = 0 ;
						lastMetrageDts.t.m = 0 ;
						lastMetrageDts.t.h = 0 ;
						lastMetrageDts.d.d = 1 ;
					}

					int diff = COMMON_GetDtsDiff__Alt(&lastMetrageDts , &currentCounterDts , BY_MONTH);
					if (diff < 0)
					{
						//debug
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter DTS is in the past [" DTS_PATTERN "]" , DTS_GET_BY_VAL(currentCounterDts));

						if (mountDflag == 1)
							mountDflag = 0;
					}

					if ((diff > 0) || (mountDflag == 1))
					{
						//printf("\n----------\nmonth diff = [%d], mountDFlag = [%d]\n----------\n", diff , mountDflag);
						res = TRUE ;
						if (mountDflag == 0)
						{
							COMMON_ChangeDts(&lastMetrageDts , INC , BY_MONTH);
						}

						memcpy(dtsToRequest , &lastMetrageDts , sizeof(dateTimeStamp) );
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE It is time to ask month meterage from [" DTS_PATTERN "]", DTS_GET_BY_PTR(dtsToRequest) );
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE last month meterage DTS is invalid" );
				}
			}
			break;

			case STORAGE_PROFILE:
			{
				COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE search for last profile dts");
				//do not read profile if season changed until 4:00 was happend
				if ( STORAGE_GetSeasonAllowFlag() == STORAGE_SEASON_CHANGE_ENABLE )
				{
					dateTimeStamp springDts , autumnDts ;
					memset( &springDts , 0 , sizeof(dateTimeStamp) );
					memset( &autumnDts , 0 , sizeof(dateTimeStamp) );
					COMMON_GetSeasonChangingDts( currentCounterDts.d.y , STORAGE_SEASON_WINTER , &springDts , &autumnDts );

					if ( ((currentCounterDts.d.y == springDts.d.y) && (currentCounterDts.d.m == springDts.d.m) && (currentCounterDts.d.d == springDts.d.d)) ||
						 ((currentCounterDts.d.y == autumnDts.d.y) && (currentCounterDts.d.m == autumnDts.d.m) && (currentCounterDts.d.d == autumnDts.d.d)) )
					{
						if ( ( currentCounterDts.t.h <= 3 ) && ( currentCounterDts.t.m < 35 ))
						{
							COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "Spring dts [" DTS_PATTERN  "]", DTS_GET_BY_VAL(springDts) );
							COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "Autumn dts [" DTS_PATTERN  "]", DTS_GET_BY_VAL(autumnDts) );
							COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "Current counter dts [" DTS_PATTERN  "]", DTS_GET_BY_VAL(currentCounterDts) );
							COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "It is spring ir autumn season change time. Do not ask profile" );
							return res ;
						}
					}
				}

				if ( STORAGE_GetLastProfileDts( counter->dbId, &lastMetrageDts) != OK )
				{
					memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
					mountDflag = 1;
				}
				else
				{
					dateTimeStamp mountDts;
					memset(&mountDts , 0 , sizeof(dateTimeStamp));
					memcpy(&mountDts.d , &counter->mountD , sizeof(dateStamp));
					if( COMMON_CheckDts( &mountDts , &lastMetrageDts ) == FALSE )
					{
						memcpy(&lastMetrageDts.d , &counter->mountD , sizeof(dateStamp));
						mountDflag = 1;
					}
				}

				if ( COMMON_CheckDtsForValid(&lastMetrageDts) == OK  )
				{
					//calculating differense between last meterage dts and current counter dts
					int diff = 0;
					if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
					{
						int minuteDiff = COMMON_GetDtsDiff__Alt(&lastMetrageDts , &currentCounterDts , BY_MIN);
						diff = minuteDiff / ( counter->profileInterval ) ;
					}
					else
					{
						return FALSE ;
					}

					if (diff < 0)
					{
						//debug
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE counter DTS is in the past [" DTS_PATTERN "]" , DTS_GET_BY_VAL(currentCounterDts));

						if (mountDflag == 1)
							mountDflag = 0;
					}

					if ((diff > 1) || (mountDflag == 1))
					{
						res = TRUE ;
						if (mountDflag == 0)
						{
							if ( counter->profileInterval == 30 )
							{
								COMMON_ChangeDts(&lastMetrageDts , INC , BY_HALF_HOUR);
							}
							else
							{
								int changingDtsIndex = counter->profileInterval ;
								for( ; changingDtsIndex > 0 ; --changingDtsIndex )
								{
									COMMON_ChangeDts(&lastMetrageDts , INC , BY_MIN);
								}
							}
						}

						memcpy(dtsToRequest , &lastMetrageDts , sizeof(dateTimeStamp) );
						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE It is time to ask profile from [" DTS_PATTERN "]", DTS_GET_BY_PTR(dtsToRequest) );
						//printf("printf STORAGE It is time to ask profile from [" DTS_PATTERN "]\n\n\n\n", DTS_GET_BY_PTR(dtsToRequest) );
					}

				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE last profile DTS is invalid" );
				}

			}
			break;

			default:
				break;
		}


	}



	return res ;
}

//
//
//
#if 0
int STORAGE_JournalUspdEvent( int evtype, int evdesc)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	pthread_mutex_lock(&journalMux);
	int fd = open(STORAGE_PATH_EVENTS, O_RDWR | O_CREAT , 00655 );
	if (fd < 0)
	{
		res =  ERROR_GENERAL;
	}
	else
	{
		//stuff the struct
		uspdEvent_t event;
		memset(&event, 0 , sizeof(uspdEvent_t) );
		event.evType = evtype;
		event.evDesc = evdesc;
		COMMON_GetCurrentDts(&(event.dts));

		struct stat st;
		fstat(fd, &st);
		int eventsNumber = ( st.st_size / sizeof(uspdEvent_t) );
		if ( eventsNumber > STORAGE_MAX_EVENTS_NUMBER )
		{
			//remove the first event in file by overwriting
			uspdEvent_t * data_in_file = NULL;
			data_in_file = (uspdEvent_t*)malloc(st.st_size);
			if (data_in_file == NULL)
			{
				// achtung!!! achtung!!! achtung!!!
				// cann't allocate memory
				close(fd);
				pthread_mutex_unlock(&journalMux);
				EXIT_PATTERN;
			}
			int bytes_numb = read(fd , data_in_file , st.st_size);
			close(fd);
			fd = 0;
			if ( bytes_numb <= 0)
			{
				free(data_in_file);
				pthread_mutex_unlock(&journalMux);
				return ERROR_GENERAL;
			}
			else
			{
				//lseek(fd, 0 , SEEK_SET);
				fd = open(STORAGE_PATH_EVENTS, O_WRONLY | O_TRUNC);
				if ( fd < 0)
				{
					close(fd);
					free(data_in_file);
					pthread_mutex_unlock(&journalMux);
					return ERROR_FILE_OPEN_ERROR;
				}
				if ( write(fd , &data_in_file[1] , (bytes_numb - sizeof( uspdEvent_t ) ) ) != \
					(bytes_numb - sizeof( uspdEvent_t ) ) )
				{
					free(data_in_file);
					pthread_mutex_unlock(&journalMux);
					return ERROR_FILE_OPEN_ERROR;
				}
				free(data_in_file);
			}

		}
		//write last event
		if ( write(fd , &event , sizeof(uspdEvent_t)) != sizeof(uspdEvent_t) )
			res = ERROR_FILE_OPEN_ERROR;;

		close(fd);

	}
	pthread_mutex_unlock(&journalMux);
}
#endif

//
//
//

BOOL STORAGE_CheckUspdStartFlag()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	BOOL res = FALSE;

	int fd = open(USPD_BOOT_FLAG , O_RDWR);

	if( fd >= 0 )
	{
		unsigned char ch;
		if ( read( fd , &ch , 1 ) == 1 )
		{
			if ( ch == '1' )
			{
				// write zero to flag
				lseek(fd , 0 , SEEK_SET);
				write( fd , "0" , 1);
				res = TRUE ;
			}
		}
		close(fd);
	}

	return res;
}

//
//
//
void STORAGE_JournalUspdEvent(unsigned char evtype, unsigned char * evdesc){
	DEB_TRACE(DEB_IDX_STORAGE)

	uspdEvent_t event;
	dateTimeStamp dts;

	//fill event
	memset(&dts, 0, sizeof(dateTimeStamp));
	memset(&event, 0, sizeof(uspdEvent_t));

	//get dts
	COMMON_GetCurrentDts(&dts);
	memcpy(&event.dts , &dts , sizeof(dateTimeStamp));

	//set type
	event.evType = evtype;

	//set description
	if ( evdesc == NULL )
		memset( event.evDesc, 0, EVENT_DESC_SIZE );
	else
		memcpy( event.evDesc, evdesc, EVENT_DESC_SIZE);

	//printf("DESCRIPTION: %02x %02x %02x %02x %02x %02x \n", event.evDesc[0], event.evDesc[1], event.evDesc[2], event.evDesc[3], event.evDesc[4], event.evDesc[5] );

	//calculate crc
	event.crc = STORAGE_CalcCrcForJournal(&event);

	//save event
	STORAGE_SaveEvent(&event  , STORAGE_USPD_DBID );
}

//
//
//

int STORAGE_JournalCounterEvent(unsigned long counterDbId ,dateTimeStamp dts , int eventType , unsigned char * evdesc )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	uspdEvent_t event;
	memset(&event , 0 , sizeof(uspdEvent_t));

	memcpy(&event.dts , &dts , sizeof(dateTimeStamp) );
	event.evType = eventType ;

	if (evdesc == NULL)
		memset(event.evDesc, 0, EVENT_DESC_SIZE);
	else
		memcpy( event.evDesc, evdesc, EVENT_DESC_SIZE);

	event.crc = STORAGE_CalcCrcForJournal(&event);

	STORAGE_SaveEvent(&event  , counterDbId );

	return res;
}

//
//
//

int STORAGE_JournalProtect()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	return pthread_mutex_lock(&journalMux);
}

//
//
//

int STORAGE_JournalUnProtect()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	return pthread_mutex_unlock(&journalMux);
}

//
//
//

int STORAGE_GetJournalNumberByEvent(uspdEvent_t * event , int * maxEventsNumber)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = EVENT_COUNTER_JOURNAL_NUMBER_UNKNOWN_JOURNAL;

	if ( event && maxEventsNumber )
	{
		(*maxEventsNumber) = STORAGE_MIN_EVENTS_NUMBER ;

		switch(event->evType)
		{
		case EVENT_OPENING_BOX:
		case EVENT_CLOSING_BOX:
			res = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_BOX;
			break;

		case EVENT_OPENING_CONTACT:
		case EVENT_CLOSING_CONTACT:
			res = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_CONTACT;
			break;

		case EVENT_COUNTER_SWITCH_ON:
		case EVENT_COUNTER_SWITCH_OFF:
			res = EVENT_COUNTER_JOURNAL_NUMBER_DEVICE_SWITCH;
			break;

		case EVENT_SYNC_TIME_HARD:
			res = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_HARD;
			break;

		case EVENT_SYNC_TIME_SOFT:
			res = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_SOFT;
			break;

		case EVENT_CHANGE_TARIF_MAP:
			res = EVENT_COUNTER_JOURNAL_NUMBER_SET_TARIFF ;
			break;

		case EVENT_SWITCH_POWER_ON:
		case EVENT_SWITCH_POWER_OFF:
			res = EVENT_COUNTER_JOURNAL_NUMBER_POWER_SWITCH ;
			break;

		case EVENT_SYNC_STATE_WORD_CH:
			res = EVENT_COUNTER_JOURNAL_NUMBER_STATE_WORD;
			break;

		case EVENT_PQP_VOLTAGE_INCREASE_F1:
		case EVENT_PQP_VOLTAGE_I_NORMALISE_F1:
		case EVENT_PQP_VOLTAGE_INCREASE_F2:
		case EVENT_PQP_VOLTAGE_I_NORMALISE_F2:
		case EVENT_PQP_VOLTAGE_INCREASE_F3:
		case EVENT_PQP_VOLTAGE_I_NORMALISE_F3:

		case EVENT_PQP_AVE_VOLTAGE_INCREASE_F1:
		case EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F1:
		case EVENT_PQP_AVE_VOLTAGE_INCREASE_F2:
		case EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F2:
		case EVENT_PQP_AVE_VOLTAGE_INCREASE_F3:
		case EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F4:

		case EVENT_PQP_VOLTAGE_DECREASE_F1:
		case EVENT_PQP_VOLTAGE_D_NORMALISE_F1:
		case EVENT_PQP_VOLTAGE_DECREASE_F2:
		case EVENT_PQP_VOLTAGE_D_NORMALISE_F2:
		case EVENT_PQP_VOLTAGE_DECREASE_F3:
		case EVENT_PQP_VOLTAGE_D_NORMALISE_F3:

		case EVENT_PQP_AVE_VOLTAGE_DECREASE_F1:
		case EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F1:
		case EVENT_PQP_AVE_VOLTAGE_DECREASE_F2:
		case EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F2:
		case EVENT_PQP_AVE_VOLTAGE_DECREASE_F3:
		case EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F4:

		case EVENT_PQP_FREQ_DEVIATION_02:
		case EVENT_PQP_FREQ_NORMALISE_02:
		case EVENT_PQP_FREQ_DEVIATION_04:
		case EVENT_PQP_FREQ_NORMALISE_04:

		case EVENT_PQP_KNOP_INCREASE_2:
		case EVENT_PQP_KNOP_NORMALISE_2:
		case EVENT_PQP_KNOP_INCREASE_4:
		case EVENT_PQP_KNOP_NORMALISE_4:

		case EVENT_PQP_KNNP_INCREASE_2:
		case EVENT_PQP_KNNP_NORMALISE_2:
		case EVENT_PQP_KNNP_INCREASE_4:
		case EVENT_PQP_KNNP_NORMALISE_4:

		case EVENT_PQP_KDF_INCREASE_F1:
		case EVENT_PQP_KDF_NORMALISE_F1:
		case EVENT_PQP_KDF_INCREASE_F2:
		case EVENT_PQP_KDF_NORMALISE_F2:
		case EVENT_PQP_KDF_INCREASE_F3:
		case EVENT_PQP_KDF_NORMALISE_F3:

		case EVENT_PQP_DDF_INCREASE_F1:
		case EVENT_PQP_DDF_NORMALISE_F1:
		case EVENT_PQP_DDF_INCREASE_F2:
		case EVENT_PQP_DDF_NORMALISE_F2:
		case EVENT_PQP_DDF_INCREASE_F3:
		case EVENT_PQP_DDF_NORMALISE_F3:
			res = EVENT_COUNTER_JOURNAL_NUMBER_PQP;
			break;

		default:
			break;
		}
	}
	return res;

}

//
//
//

int STORAGE_GetUspdJournalByEvent( uspdEvent_t * event , int * maxEventsNumber)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = 0;
	if (event)
	{
		switch(event->evType)
		{
			case EVENT_TARIFF_MAP_RECIEVED:
			case EVENT_TARIFF_MAP_WRITING:
			{
				res = EVENT_USPD_JOURNAL_NUMBER_TARIFF;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			case EVENT_PSM_RULES_RECIEVED :
			case EVENT_PSM_RULES_WRITING :
			{
				res = EVENT_USPD_JOURNAL_NUMBER_POWER_SWITCH_MODULE;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			case EVENT_USR_CMNDS_RECIEVED:
			case EVENT_USR_CMNDS_WRITING:
			case EVENT_FIRMAWARE_RECIEVED :
			case EVENT_FIRMAWARE_WRITING :
			{
				res = EVENT_USPD_JOURNAL_NUMBER_FIRMWARE;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			case EVENT_EXIO_STATE_CHANGED:
			{
				res = EVENT_USPD_JOURNAL_NUMBER_EXTENDED_IO_CARD;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			case EVENT_RF_TABLE_DEL_REMOTE:
			case EVENT_RF_INITIALIZE_ERROR:
			case EVENT_RF_PIM_START_CODE:
			case EVENT_RF_TABLE_NEW_REMOTE:
			case EVENT_RF_TABLE_START_REMOVING_REMOTE:
			case EVENT_RF_INIT_ERROR:
			{
				res = EVENT_USPD_JOURNAL_NUMBER_RF_EVENTS;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			case EVENT_PLC_TABLE_START_CLEARING:
			case EVENT_PLC_TABLE_STOP_CLEARING:
			case EVENT_PLC_TABLE_NEW_REMOTE:
			case EVENT_PLC_TABLE_DEL_REMOTE:
			case EVENT_PLC_ISM_START_CODE:
			case EVENT_PLC_ISM_ERROR_CODE:
			case EVENT_PIM_INITIALIZE_ERROR:
			case EVENT_PLC_TABLE_ALL_LEAVE_NET:
			case EVENT_PLC_PIM_CONFIG:
			{
				res = EVENT_USPD_JOURNAL_NUMBER_PLC_EVENTS;
				if ( maxEventsNumber )
				{
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;

			default:
			{
				res = EVENT_USPD_JOURNAL_NUMBER_GENERAL ;
				if ( maxEventsNumber )
				{
					//(*maxEventsNumber) = STORAGE_MIN_EVENTS_NUMBER ;
					(*maxEventsNumber) = STORAGE_MAX_EVENTS_NUMBER ;
				}
			}
			break;
		}
	}

	return res ;
}

//
//
//

BOOL STORAGE_IsEventInJournal(uspdEvent_t * event , unsigned long counterDbId)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	BOOL res = FALSE ;

	STORAGE_JournalProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsEventInJournal() start");
	if (event)
	{
		 char fileName[SFNL];
		 int maxBlocks;
		 int journalNumber = STORAGE_GetJournalNumberByEvent(event , &maxBlocks);

		 GET_JOURNAL_FILE_NAME(fileName , STORAGE_PATH_JOURNAL , counterDbId ,journalNumber );

		 int fd = 0 ;
		 fd = open(fileName, O_RDONLY);
		 if (fd >= 0)
		 {
			 struct stat st;
			 //get file size
			 fstat(fd, &st);

			 if (st.st_size != ZERO)
			 {
				 uspdEvent_t tempEvent;

				 int size_of_block = sizeof(uspdEvent_t);
				 int blocks = st.st_size / size_of_block ;

				 int idx = 0 ;
				 for(idx = 0 ; idx < blocks ; idx++)
				 {
					 memset(&tempEvent , 0 ,sizeof(uspdEvent_t));
					 if (read(fd , &tempEvent , sizeof(uspdEvent_t)) == sizeof(uspdEvent_t) )
					 {
						 if ( (tempEvent.evType == event->evType) && \
							  (memcmp(&tempEvent.evDesc , &event->evDesc , EVENT_DESC_SIZE) == 0) && \
							  (memcmp(&tempEvent.dts , &event->dts , sizeof(dateTimeStamp)) == 0)  )
						 {
							 res = TRUE ;
							 break;
						 }
					 }
					 else
						 break;
				 }
			 }
			 close(fd);
		 }

	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsEventInJournal() ERROR with NULL ptr to event");
	}

	STORAGE_JournalUnProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_IsEventInJournal() exit with res = [%d]" , res);

	return res;
}

//
//
//

int STORAGE_SaveEvent(uspdEvent_t * event , unsigned long counterDbId ){
	DEB_TRACE(DEB_IDX_STORAGE)


	int res = OK;
	int fd = 0;
	struct stat st;
	uspdEvent_t * data_in_file;
	int blocks = 0;
	int size_of_block = sizeof(uspdEvent_t);
	int max_blocks = 0 ;
	int result;

	//check input pointer
	if (event == NULL) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveEvent ERROR with NULL ptr to event");

		return ERROR_GENERAL;
	}


	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveEvent() enter");

	char fileName[SFNL];
	if (counterDbId == STORAGE_USPD_DBID)
	{

		#if 0
		memset(fileName , 0 , SFNL);
		memcpy(fileName , STORAGE_PATH_EVENTS , strlen(STORAGE_PATH_EVENTS) );
		#else
		int journalNumber = STORAGE_GetUspdJournalByEvent(event , &max_blocks);

		GET_USPD_JOURNAL_FILE_NAME(fileName,journalNumber);
		#endif
	}
	else
	{

		int journalNumber = STORAGE_GetJournalNumberByEvent(event , &max_blocks);

		GET_JOURNAL_FILE_NAME(fileName , STORAGE_PATH_JOURNAL , counterDbId ,journalNumber );
	}

	//lock journal mutex
	STORAGE_JournalProtect();

	//open link to file
	fd = open(fileName, O_RDWR | O_CREAT, S_IRWXU);
	if (fd == -1) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveEvent ERROR with open path(%s)", STORAGE_PATH_EVENTS);

		STORAGE_JournalUnProtect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_SaveEvent() exit");

		return ERROR_FILE_OPEN_ERROR;
	}


	//get file size
	fstat(fd, &st);

	//check file size
	if (st.st_size == ZERO) {

		//simple write first portion of data
		write(fd, event, size_of_block);
		STORAGE_IncrementHardwareWritingCtrlCounter();

		//setup return code
		res = OK;

	} else {

		//allocate mem according file size
		blocks = st.st_size / size_of_block;
		data_in_file = (uspdEvent_t *)malloc((blocks+1)*size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_JournalUnProtect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, &data_in_file[1], blocks*size_of_block);

		//check read result
		if (result == (blocks*size_of_block)){

			//append new data
			memcpy(&data_in_file[0], event, size_of_block);
			blocks++;

			//seek fd to start
			if (lseek(fd, 0L, SEEK_SET) != 0) {
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent ERROR with file update(%s)", STORAGE_PATH_EVENTS);

				//close file
				close(fd);

				STORAGE_JournalUnProtect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent() exit");

				return ERROR_FILE_OPEN_ERROR;
			}

			//new
			if (blocks > max_blocks)
				blocks = max_blocks;
			write(fd, data_in_file, (blocks * size_of_block));
			STORAGE_IncrementHardwareWritingCtrlCounter();

		} else {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent READ FILE error ");

			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent (%s) %s", STORAGE_PATH_EVENTS, getErrorCode(res));

	STORAGE_JournalUnProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_JournalUspdEvent() exit");

	return res;
}

//
//
//

int STORAGE_GetJournalForDb(uspdEventsArray_t ** data , unsigned long counterDbId , int journalNumber) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	uspdEvent_t * data_in_file;
	int result;
	int size_of_block = sizeof (uspdEvent_t);
	int blocks = ZERO;
	int blockIndex = ZERO;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() enter");

	STORAGE_JournalProtect();

	char fileName[SFNL];
	if (counterDbId == STORAGE_USPD_DBID)
	{
		#if 0
		memset(fileName , 0 , SFNL);
		memcpy(fileName , STORAGE_PATH_EVENTS , strlen(STORAGE_PATH_EVENTS) );
		#else

		GET_USPD_JOURNAL_FILE_NAME(fileName,journalNumber);
		#endif
	}
	else
	{
	   GET_JOURNAL_FILE_NAME(fileName , STORAGE_PATH_JOURNAL , counterDbId ,journalNumber );
	}

	//open link to file
	fd = open(fileName, O_RDONLY);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb ERROR with open path(%s)", fileName);

		STORAGE_JournalUnProtect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(blocks * size_of_block);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_JournalUnProtect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, blocks * size_of_block);

		//check fread result
		if (result == (blocks * size_of_block)){

			//
			//-----------------------
			//
			(*data) = (uspdEventsArray_t *)malloc( sizeof(uspdEventsArray_t) );
			if ( (*data) == NULL)
			{
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb ERROR with alloc mem()");

				//close file
				close(fd);

				STORAGE_JournalUnProtect();

				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() exit");

				return ERROR_MEM_ERROR;
			}

			//
			//-----------------------
			//

			(*data)->events = NULL ;
			(*data)->eventsCount = 0 ;

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >=0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForJournal(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (!(data_in_file[blockIndex].status & METERAGE_IN_DB_MASK)) {

						COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() not in DB, so place to send");

						//append storage value
						(*data)->events = (uspdEvent_t **) realloc((*data)->events, (++(*data)->eventsCount) * sizeof(uspdEvent_t *));
						if ((*data)->events != NULL) {
							(*data)->events[(*data)->eventsCount - 1] = (uspdEvent_t *) malloc(size_of_block);
							if (((*data)->events[(*data)->eventsCount - 1]) != NULL)
								memcpy((*data)->events[(*data)->eventsCount - 1], &data_in_file[blockIndex], size_of_block);
							else
								SET_RES_AND_BREAK_WITH_MEM_ERROR
						} else
							SET_RES_AND_BREAK_WITH_MEM_ERROR
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() block with wrong crc skipped");
				//}
			}
		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	if ((*data))
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb (total: %d) %s", (*data)->eventsCount, getErrorCode(res));
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb (total: 0) %s", getErrorCode(res));
	}

	STORAGE_JournalUnProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_GetJournalForDb() exit");

	return res;
}

//
//
//

int STORAGE_MarkJournalAsPlacedToDb(dateTimeStamp dtsLast , unsigned long counterDbId,int journalNumber) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	uspdEvent_t * data_in_file;
	int result;
	int size_of_block = sizeof (uspdEvent_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() enter");

	STORAGE_JournalProtect();

	char fileName[SFNL];
	if (counterDbId == STORAGE_USPD_DBID)
	{
		#if 0
		memset(fileName , 0 , SFNL);
		memcpy(fileName , STORAGE_PATH_EVENTS , strlen(STORAGE_PATH_EVENTS) );
		#else
		GET_USPD_JOURNAL_FILE_NAME(fileName,journalNumber);
		#endif
	}
	else
	{
		GET_JOURNAL_FILE_NAME(fileName , STORAGE_PATH_JOURNAL , counterDbId ,journalNumber );
	}

	//open link to file
	fd = open(fileName, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb ERROR with open path(%s)", STORAGE_PATH_EVENTS);

		STORAGE_JournalUnProtect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(size_of_block * blocks);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_JournalUnProtect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, size_of_block * blocks);

		//check fread result
		if (result == (size_of_block * blocks)){

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForJournal(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (!(data_in_file[blockIndex].status & METERAGE_IN_DB_MASK)) {

						//check storage values dts in requested range
						dateInRange = COMMON_DtsIsLower(&(data_in_file[blockIndex].dts), &dtsLast);
						if (dateInRange == TRUE) {
							data_in_file[blockIndex].status += METERAGE_IN_DB_MASK;
							needUpdate = TRUE;
						}
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() block with wrong crc skipped");
				//}
			}

			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();

				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() error lseek");
				}
			}

		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb %s", getErrorCode(res));

	STORAGE_JournalUnProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_MarkJournalAsPlacedToDb() exit");

	return res;
}

//
//
//

int STORAGE_RecoveryJournalPlacedToDbFlag(dateTimeStamp olderDts , unsigned long counterDbId,int journalNumber) {
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;
	int fd = -1;
	struct stat st;
	uspdEvent_t * data_in_file;
	int result;
	int size_of_block = sizeof (uspdEvent_t);
	int blocks = ZERO;
	int blockIndex = ZERO;
	BOOL needUpdate = FALSE;
	BOOL dateInRange = FALSE;

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() enter");

	STORAGE_JournalProtect();

	char fileName[SFNL];
	if (counterDbId == STORAGE_USPD_DBID)
	{
		#if 0
		memset(fileName , 0 , SFNL);
		memcpy(fileName , STORAGE_PATH_EVENTS , strlen(STORAGE_PATH_EVENTS) );
		#else
		GET_USPD_JOURNAL_FILE_NAME(fileName,journalNumber);
		#endif
	}
	else
	{
		GET_JOURNAL_FILE_NAME(fileName , STORAGE_PATH_JOURNAL , counterDbId ,journalNumber );
	}

	//open link to file
	fd = open(fileName, O_RDWR);
	if (fd < 0) {
		//debug
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag ERROR with open path(%s)", STORAGE_PATH_EVENTS);

		STORAGE_JournalUnProtect();

		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() exit");

		return ERROR_FILE_OPEN_ERROR;
	}

	//get file size
	fstat(fd, &st);

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() file size %d", st.st_size);

	//check file size
	if (st.st_size >= size_of_block){

		//allocate mem
		blocks = st.st_size / size_of_block;
		data_in_file = malloc(size_of_block * blocks);
		if (data_in_file == NULL) {
			//debug
			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag ERROR with alloc mem()");

			//close file
			close(fd);

			STORAGE_JournalUnProtect();

			COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() exit");

			return ERROR_MEM_ERROR;
		}

		//read file content
		result = read(fd, data_in_file, size_of_block * blocks);

		//check fread result
		if (result == (size_of_block * blocks)){

			//loop on saved values
			for (blockIndex=(blocks-1); blockIndex >= 0; blockIndex--) {

				//check block crc
				//osipov
				//unsigned int blockCrc = STORAGE_CalcCrcForJournal(&data_in_file[blockIndex]);
				//if (blockCrc == data_in_file[blockIndex].crc){

					if (data_in_file[blockIndex].status & METERAGE_IN_DB_MASK) {

						//check storage values dts in requested range
						dateInRange = COMMON_DtsIsLower(&olderDts , &(data_in_file[blockIndex].dts));
						if (dateInRange == TRUE) {
							data_in_file[blockIndex].status -= METERAGE_IN_DB_MASK;
							needUpdate = TRUE;
						}
					}

				//} else {
				//	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() block with wrong crc skipped");
				//}
			}

			if (needUpdate == TRUE){
				if(lseek(fd, 0, SEEK_SET) == 0){
					write(fd, data_in_file, st.st_size);
					STORAGE_IncrementHardwareWritingCtrlCounter();
				} else {
					COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() error lseek");
				}
			}

		} else {
			res = ERROR_FILE_OPEN_ERROR;
		}

		//free mem
		free(data_in_file);
		data_in_file = NULL;

	} else {
		//file is empty
		res = OK;
	}

	//close file
	close(fd);

	//debug
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag %s", getErrorCode(res));

	STORAGE_JournalUnProtect();

	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "STORAGE_RecoveryJournalPlacedToDbFlag() exit");

	return res;
}

#if 0
void STORAGE_JournalUspdEvent( unsigned char evtype, unsigned char * evdesc)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	pthread_mutex_lock(&journalMux);

	dateTimeStamp dts;
	memset(&dts , 0 ,sizeof(dateTimeStamp));
	COMMON_GetCurrentDts(&dts);

	uspdEventsArray_t uspdEventsArray;
	//uspdEventsArray = (uspdEventsArray_t *)malloc( sizeof(uspdEventsArray_t) ) ;
	memset( &uspdEventsArray , 0 , sizeof(uspdEventsArray_t) );

	uspdEvent_t dataInFile;
	memset( &dataInFile , 0 , sizeof(uspdEvent_t) );

	//printf("sizeof(uspdEvent_t) == %d\n", sizeof(uspdEvent_t));

	int jr_fd = open( STORAGE_PATH_EVENTS , O_RDWR | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if ( jr_fd >= 0 )
	{

		while( read( jr_fd , &dataInFile , sizeof(uspdEvent_t) ) > 0 )
		{
			printf("Next reading iteration. Founded struct\n");
			printf("\t%d-%d-%d\n"
				   "\tevent=%d\n"
				   "\tdesc=%d\n"
				   "\tstatus=%d\n", dataInFile.dts.d.d , dataInFile.dts.d.m , dataInFile.dts.d.y , dataInFile.evType , dataInFile.evDesc , dataInFile.status);



			printf("Try to allocate memory for the uspdEventsArray.event\n");
			uspdEventsArray.event = (uspdEvent_t **)realloc( uspdEventsArray.event , uspdEventsArray.eventsCount * sizeof(uspdEvent_t) );
			if (uspdEventsArray.event != NULL)
			{
				printf("Try to allocate memory for the uspdEventsArray.event[%d]\n", uspdEventsArray.eventsCount);
				uspdEventsArray.event[uspdEventsArray.eventsCount] = (uspdEvent_t *)malloc( sizeof(uspdEvent_t) );
				if( uspdEventsArray.event[uspdEventsArray.eventsCount] !=NULL )
				{
					memcpy( uspdEventsArray.event[uspdEventsArray.eventsCount] , &dataInFile , sizeof(uspdEvent_t) );
					uspdEventsArray.eventsCount ++;
				}
				else
					EXIT_PATTERN;
			}
			else
				EXIT_PATTERN;

			memset( &dataInFile , 0 , sizeof(uspdEvent_t) );
		}

		// add current event to array


		memcpy(&dataInFile.dts , &dts , sizeof(dateTimeStamp));
		//memcmp(&dataInFile.evType , &evtype , sizeof(unsigned char));
		//memcmp(&dataInFile.evDesc , &evdesc , sizeof(int));


		dataInFile.evType = evtype ;
		//dataInFile.evDesc = evdesc ;


		if ( evdesc == NULL )
			memset( dataInFile.evDesc , 0 , EVENT_DESC_SIZE );
		else
			memcpy( dataInFile.evDesc,  &evdesc , EVENT_DESC_SIZE) ;

		uspdEventsArray.event = (uspdEvent_t **)realloc( uspdEventsArray.event , (uspdEventsArray.eventsCount)*sizeof(uspdEvent_t) );
		if (uspdEventsArray.event != NULL)
		{
			uspdEventsArray.event[uspdEventsArray.eventsCount] = (uspdEvent_t *)malloc( sizeof(uspdEvent_t) );
			if( uspdEventsArray.event[uspdEventsArray.eventsCount] !=NULL )
			{
				memcpy( uspdEventsArray.event[uspdEventsArray.eventsCount] , &dataInFile , sizeof(uspdEvent_t) );
				uspdEventsArray.eventsCount++;
				//printf("%d-%d-%d\n", uspdEventsArray.event[uspdEventsArray.eventsCount]->dts.d.d , uspdEventsArray.event[uspdEventsArray.eventsCount]->dts.d.m , uspdEventsArray.event[uspdEventsArray.eventsCount]->dts.d.y ) ;
			}
			else
				EXIT_PATTERN;
		}
		else
			EXIT_PATTERN;

		//debug
		//printf("!!!Total %d events in array\n" , uspdEventsArray.eventsCount);

		//truncate file
		lseek(jr_fd , 0 , SEEK_SET) ;
		ftruncate(jr_fd , 0) ;

		//write new array into the file
		int index;
		if ( uspdEventsArray.eventsCount > STORAGE_MAX_EVENTS_NUMBER )
		{
			index = uspdEventsArray.eventsCount - STORAGE_MAX_EVENTS_NUMBER ;
		}
		else
			index = 0;

		for( ; index < uspdEventsArray.eventsCount ; index++)
		{
			//printf("Next writing iteration\n");
			int bytes = 0;
			if ( ( bytes = write( jr_fd , uspdEventsArray.event[index] , sizeof(uspdEvent_t) ) ) != sizeof(uspdEvent_t) )
			{
				printf("Breaking. Bytes = %d\n , ERRNO = %s\n", bytes , strerror(errno));
				//printf("%d-%d-%d\n", uspdEventsArray.event[index].dts.d.d , uspdEventsArray.event[index].dts.d.m , uspdEventsArray.event[index].dts.d.y ) ;
				break;
			}
		}

		// free array

		for(index = 0 ; index < uspdEventsArray.eventsCount ; index++ )
			free(uspdEventsArray.event[index]);
		free( uspdEventsArray.event );

		//close file description
		close(jr_fd);

	}



	pthread_mutex_unlock(&journalMux);
}
#endif

//
//
//
#if 0
int STORAGE_JournalGetUspdEventsForDb( uspdEventsArray_t ** eventsArr)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	pthread_mutex_lock(&journalMux);
	int res= OK;
	//open file
	int fd = open(STORAGE_PATH_EVENTS, O_RDONLY);
	if (fd < 0)
	{
		pthread_mutex_unlock(&journalMux);
		return ERROR_FILE_OPEN_ERROR;
	}
	//check file size
	struct stat st;
	fstat(fd, &st);

	int blockSize = sizeof(uspdEvent_t);
	int totalSize = st.st_size;
	int blockNumber = totalSize / blockSize ;

	if( totalSize < blockSize )
	{
		pthread_mutex_unlock(&journalMux);
		return ERROR_GENERAL;
	}
	//allocate intermediate buffer
	uspdEvent_t * data_in_file = NULL;
	data_in_file = (uspdEvent_t*)malloc(totalSize);

	if (read(fd , &data_in_file , totalSize) < totalSize )
	{
		close(fd);
		free(data_in_file);
		pthread_mutex_unlock(&journalMux);
		return ERROR_FILE_OPEN_ERROR;

	}

	int blockIndex=0 ;
	for( ; blockIndex < blockNumber ; blockIndex++)
	{
		if ( data_in_file[ blockIndex ].status & STORAGE_EVENT_IN_DB )
		{
			(*eventsArr)->event = (uspdEvent_t**)realloc((*eventsArr)->event , (++((*eventsArr)->eventsCount) * sizeof(uspdEvent_t) ) );
			if ( (*eventsArr)->event != NULL )
			{
				(*eventsArr)->event[ ((*eventsArr)->eventsCount) - 1 ] = (uspdEvent_t*)malloc(blockSize);
				if ( (*eventsArr)->event[ ((*eventsArr)->eventsCount) - 1 ] != NULL )
					memcpy( &(*eventsArr)->event[ ((*eventsArr)->eventsCount) - 1 ] , &data_in_file[blockIndex] , blockSize);
				else
				{
					// achtung!!! achtung!!! achtung!!! can not allocate memory !!!
					close(fd);
					EXIT_PATTERN;
				}
			}
			else
			{
				// achtung!!! achtung!!! achtung!!! can not allocate memory !!!
				close(fd);
				EXIT_PATTERN;
			}

		}

	}
	close(fd);
	pthread_mutex_unlock(&journalMux);
	return res;
}
#endif

//
//
//

#if 0
int STARAGE_JournalMarkEventsAsPlacedToDb(dateTimeStamp dtsLast)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = OK;

	pthread_mutex_lock(&journalMux);

	int fd = open(STORAGE_PATH_EVENTS, O_RDWR);
	if (fd > 0)
	{
		//check file size
		struct stat st;
		fstat(fd, &st);

		int blockSize = sizeof(uspdEvent_t);
		int totalSize = st.st_size;
		int blockNumber = totalSize / blockSize ;

		if (totalSize >= blockSize)
		{
			uspdEvent_t * data_in_file = NULL;
			data_in_file = (uspdEvent_t*)malloc(totalSize);
			if ( read( fd , &data_in_file , totalSize ) == totalSize )
			{
				int blockIndex = 0;
				for( ; blockIndex < blockNumber ; blockIndex++)
				{
					if( COMMON_DtsIsLower( data_in_file[blockIndex].dts , dtsLast ) == TRUE )
					{
						data_in_file[blockIndex].status |= STORAGE_EVENT_IN_DB;
					}
				}
				lseek(fd , 0 , SEEK_SET);
				if (write(fd , &data_in_file , totalSize) < totalSize )
					res = ERROR_GENERAL;
			}
			else
			{
				res = ERROR_GENERAL;
			}

		}
		else
		{
			res = ERROR_FILE_OPEN_ERROR;
		}
		close(fd);
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR;
	}

	pthread_mutex_unlock(&journalMux);
	return res;
}
#endif

//
//
//

int STORAGE_SavePQP(powerQualityParametres_t * powerQualityParametres , unsigned long counterDbId)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL ;

	STORAGE_Protect();

	if (powerQualityParametres)
	{
		char filename[128];
		memset(filename , 0 , 64 * sizeof(char));
		GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_PQ, counterDbId );

		int fd = open(filename , O_CREAT | O_WRONLY | O_TRUNC , 0777);
		if (fd > 0 )
		{
			int w = write( fd , powerQualityParametres , sizeof(powerQualityParametres_t) );
			if ( w == sizeof(powerQualityParametres_t) )
			{
				STORAGE_IncrementHardwareWritingCtrlCounter();
				res = OK ;
			}
			close(fd);
		}
		else
			res = ERROR_FILE_OPEN_ERROR ;

	}

	STORAGE_Unprotect();

	return res ;

}

//
//
//

int STORAGE_GetPQPforDB(powerQualityParametres_t * powerQualityParametres , unsigned long counterDbId)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	STORAGE_Protect();

	int res = ERROR_GENERAL ;

	char filename[128];
	memset(filename , 0 , 64 * sizeof(char));
	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_PQ, counterDbId );
	int fd = open(filename , O_RDONLY);
	if (fd > 0)
	{
		powerQualityParametres_t tempPQP;
		memset(&tempPQP , 0 , sizeof(powerQualityParametres_t));
		int r = read(fd , &tempPQP , sizeof(powerQualityParametres_t));
		if (r == sizeof(powerQualityParametres_t))
		{
			memcpy(powerQualityParametres , &tempPQP , sizeof(powerQualityParametres_t));
			res = OK ;
		}

		close(fd);
	}
	else
		res = ERROR_FILE_OPEN_ERROR ;
	//todo

	STORAGE_Unprotect();

	return res ;
}

//
//
//

void STORAGE_UPDATE_PLC_NET(){
	DEB_TRACE(DEB_IDX_STORAGE);

	COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_UPDATE_PLC_NET start" );

	//stop rf loop
	POOL_StopPlc();

	//wait rf task done
	POOL_WaitPlcLoopEnds();



	BOOL oldConfigIsWithDetection = FALSE ;
	if ( plcBaseSettings.admissionMode == PLC_ADMISSION_WITH_DETECT )
	{
		COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_UPDATE_PLC_NET prev admission mode was PLC_ADMISSION_WITH_DETECT" );
		oldConfigIsWithDetection = TRUE ;
	}

	//open new cfg
	STORAGE_LoadPlcSettings();

	if ( (oldConfigIsWithDetection == TRUE) && (plcBaseSettings.admissionMode != PLC_ADMISSION_WITH_DETECT) )
	{
		COMMON_STR_DEBUG( DEBUG_STORAGE_ALL "STORAGE_UPDATE_PLC_NET new admission mode is not PLC_ADMISSION_WITH_DETECT. Reload plc config" );
		STORAGE_CountersProtect();

		//clear old configuration
		int counterIndex;
		for( counterIndex=0 ; counterIndex < countersArrayPlc.countersSize ; counterIndex++)
		{
			if ( countersArrayPlc.counter[ counterIndex ] != NULL ){
				free(countersArrayPlc.counter[ counterIndex ]);
				countersArrayPlc.counter[ counterIndex ] = NULL ;
			}
		}
		free(countersArrayPlc.counter);
		countersArrayPlc.counter = NULL ;
		countersArrayPlc.countersSize = 0;
		countersArrayPlc.currentCounterIndex = 0;

		//again load new config
		STORAGE_LoadCountersPlc(&countersArrayPlc);

		STORAGE_CountersUnprotect();

	}

	//resume rf poller part
	POOL_ResumePlcLoop();
}

//
//
//

void STORAGE_UPDATE_RS(){
	DEB_TRACE(DEB_IDX_STORAGE)


	//stop 485 poller
	POOL_Stop485();

	//wait rf task done
	POOL_Wait485LoopEnds();

	//delete statistic
	STORAGE_DeleteStatisticMap();

	//protect storage
	STORAGE_CountersProtect();

	countersArray_t newCountersArray;
	memset( &newCountersArray , 0 , sizeof(countersArray_t) );

	if ( STORAGE_LoadCounters485(&newCountersArray) == OK )
	{
		int newCounterIndex = 0 ;
		for( ; newCounterIndex < newCountersArray.countersSize ; ++newCounterIndex )
		{
			BOOL needToClear = TRUE;

			int oldCounterIndex = 0 ;
			for ( ; oldCounterIndex < countersArray485.countersSize ; ++oldCounterIndex )
			{
				if ( countersArray485.counter[ oldCounterIndex ]->dbId == newCountersArray.counter[ newCounterIndex ]->dbId )
				{
					if(newCountersArray.counter[newCounterIndex]->clear == 0x0)
					{
						needToClear = FALSE;
					}
					break;
				}
			}

			if (needToClear == TRUE){
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "Delete meterages for dbid: %u", newCountersArray.counter[ newCounterIndex ]->dbId);

				//delete
				STORAGE_ClearMeterages( newCountersArray.counter[ newCounterIndex ]->dbId  );
			}
		}
	}

	//clear old configuration
	int counterIndex;
	for( counterIndex=0 ; counterIndex < countersArray485.countersSize ; counterIndex++)
	{
		if ( countersArray485.counter[ counterIndex ] != NULL ){
			free(countersArray485.counter[ counterIndex ]);
			countersArray485.counter[ counterIndex ] = NULL ;
		}
	}
	free(countersArray485.counter);
	countersArray485.counter = NULL ;
	countersArray485.countersSize = 0;
	countersArray485.currentCounterIndex = 0;

	//clear one new configuration
	for( counterIndex=0 ; counterIndex < newCountersArray.countersSize ; counterIndex++)
	{
		if ( newCountersArray.counter[ counterIndex ] != NULL ){
			free(newCountersArray.counter[ counterIndex ]);
			newCountersArray.counter[ counterIndex ] = NULL ;
		}
	}
	free(newCountersArray.counter);
	newCountersArray.counter = NULL ;
	newCountersArray.countersSize = 0;
	newCountersArray.currentCounterIndex = 0;

	//again load new config
	STORAGE_LoadCounters485(&countersArray485);

	//resume storage
	STORAGE_CountersUnprotect();

	//restore statistic
	STORAGE_CreateStatisticMap();

	//restore 485 poll process
	POOL_Resume485Loop();

}

//
//
//

void STORAGE_UPDATE_PLC(){
	DEB_TRACE(DEB_IDX_STORAGE)

	//clear SN table on PLC
	IT700_EraseAddressDb();

	//stop rf loop
	POOL_StopPlc();

	//wait rf task done
	POOL_WaitPlcLoopEnds();

	//delete statistic
	STORAGE_DeleteStatisticMap();

	//protect storage
	STORAGE_CountersProtect();

	countersArray_t newCountersArray;
	memset( &newCountersArray , 0 , sizeof(countersArray_t) );

	if ( STORAGE_LoadCountersPlc(&newCountersArray) == OK )
	{
		int newCounterIndex = 0 ;
		for( ; newCounterIndex < newCountersArray.countersSize ; ++newCounterIndex )
		{
			BOOL needToClear = TRUE;

			int oldCounterIndex = 0 ;
			for ( ; oldCounterIndex < countersArrayPlc.countersSize ; ++oldCounterIndex )
			{
				if ( countersArrayPlc.counter[ oldCounterIndex ]->dbId == newCountersArray.counter[ newCounterIndex ]->dbId )
				{
					if(newCountersArray.counter[newCounterIndex]->clear == 0x0)
					{
						needToClear = FALSE;
					}
					break;
				}
			}

			if (needToClear == TRUE){
				//debug
				COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "Delete meterages for dbid: %u", newCountersArray.counter[ newCounterIndex ]->dbId );

				//delete
				STORAGE_ClearMeterages( newCountersArray.counter[ newCounterIndex ]->dbId  );
			}
		}
	}

	//clear old configuration
	int counterIndex;
	for( counterIndex=0 ; counterIndex < countersArrayPlc.countersSize ; counterIndex++)
	{
		if ( countersArrayPlc.counter[ counterIndex ] != NULL ){
			free(countersArrayPlc.counter[ counterIndex ]);
			countersArrayPlc.counter[ counterIndex ] = NULL ;
		}
	}
	free(countersArrayPlc.counter);
	countersArrayPlc.counter = NULL ;
	countersArrayPlc.countersSize = 0;
	countersArrayPlc.currentCounterIndex = 0;

	//clear one new configuration
	for( counterIndex=0 ; counterIndex < newCountersArray.countersSize ; counterIndex++)
	{
		if ( newCountersArray.counter[ counterIndex ] != NULL ){
			free(newCountersArray.counter[ counterIndex ]);
			newCountersArray.counter[ counterIndex ] = NULL ;
		}
	}
	free(newCountersArray.counter);
	newCountersArray.counter = NULL ;
	newCountersArray.countersSize = 0;
	newCountersArray.currentCounterIndex = 0;

	//again load new config
	STORAGE_LoadCountersPlc(&countersArrayPlc);

	//reinit que
	STORAGE_PlcQue_ReInitialize();
	
	//release storage
	STORAGE_CountersUnprotect();
	
	//create statistic
	STORAGE_CreateStatisticMap();

	//resume rf poller part
	POOL_ResumePlcLoop();
}

//
//
//

void STORAGE_UPDATE_CONN(){
	DEB_TRACE(DEB_IDX_STORAGE)


	#if 0
	CONNECTION_Stop();

	//load new settings
	STORAGE_LoadConnections();

	CONNECTION_Start();
	#endif

	CONNECTION_RestartAndLoadNewParametres();
}

//
//
//

void STORAGE_UPDATE_ETH(){
	DEB_TRACE(DEB_IDX_STORAGE)


	#if 0
	CONNECTION_Stop();

	//load new settings
	STORAGE_LoadConnections();
	ETH_EthSettings();

	CONNECTION_Start();

	#endif

	ETH_EthSettings();
	CONNECTION_RestartAndLoadNewParametres();

}

//
//
//

void STORAGE_UPDATE_GSM(){
	DEB_TRACE(DEB_IDX_STORAGE)


	#if 0

	CONNECTION_Stop();

	//load new settings
	STORAGE_LoadConnections();

	CONNECTION_Start();
	#endif

	CONNECTION_RestartAndLoadNewParametres();
}

//
//
//

void STORAGE_UPDATE_EXIO()
{
	DEB_TRACE(DEB_IDX_STORAGE)

	EXIO_RewriteDefault() ;
}

//
//
//

void STORAGE_ClearMeterages( unsigned long counterDbId )
{
	//remove files
	char filename[SFNL];
	memset(filename , 0 ,SFNL);


	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_CURRENT, counterDbId );
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_MONTH, counterDbId );
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_DAY, counterDbId );
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_PROFILE, counterDbId );
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_PQ, counterDbId );
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_STATUS, counterDbId);
	unlink(filename);
	STORAGE_IncrementHardwareWritingCtrlCounter();

	int journalIndex = 0 ;
	for( ; journalIndex < 8 ; ++journalIndex )
	{
		GET_JOURNAL_FILE_NAME(filename , STORAGE_PATH_JOURNAL , counterDbId ,journalIndex );
		unlink(filename);
		STORAGE_IncrementHardwareWritingCtrlCounter();
	}

	return ;
}

//
//
//

void STORAGE_ClearMeteragesAfterHardSync( counter_t * counter )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	if ( !counter )
		return ;

	//remove files
	char filename[SFNL];
	memset(filename , 0 ,SFNL);


	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_CURRENT, counter->dbId );
	STORAGE_IncrementHardwareWritingCtrlCounter();
	unlink(filename);

	GET_STORAGE_FILE_NAME(filename, STORAGE_PATH_PQ, counter->dbId );
	STORAGE_IncrementHardwareWritingCtrlCounter();
	unlink(filename);

	//clear status
	counter->profilePointer = POINTER_EMPTY ;
	counter->profileCurrPtr = POINTER_EMPTY ;
	counter->profilePtrCurrOverflow = 0 ;
	STORAGE_UpdateCounterStatus_LastProfilePtr( counter , POINTER_EMPTY );

	//mark last invalid meterages
	dateTimeStamp currentDts ;
	memset( &currentDts , 0 , sizeof( dateTimeStamp ) );
	COMMON_GetCurrentDts( &currentDts ) ;

	meterage_t emptyMeterage ;
	memset( &emptyMeterage , 0 , sizeof( meterage_t ) );
	emptyMeterage.status = METERAGE_TIME_WAS_SYNCED ;

	//day meterages
	memcpy( &emptyMeterage.dts.d , &currentDts.d , sizeof( dateStamp ) ) ;
	COMMON_ChangeDts( &emptyMeterage.dts , DEC , BY_DAY );
	int saveRes = STORAGE_SaveMeterages( STORAGE_DAY , counter->dbId , &emptyMeterage );
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "DEBUG_STORAGE_ALL save empty day meterage for " DTS_PATTERN " with res [%d] " , DTS_GET_BY_VAL(emptyMeterage.dts) , saveRes);

	//month meterage
	emptyMeterage.dts.d.d = 1 ;
	COMMON_ChangeDts( &emptyMeterage.dts , DEC , BY_MONTH );
	saveRes = STORAGE_SaveMeterages( STORAGE_MONTH , counter->dbId , &emptyMeterage );
	COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "DEBUG_STORAGE_ALL save empty month meterage for " DTS_PATTERN " with res [%d] " , DTS_GET_BY_VAL(emptyMeterage.dts) , saveRes);

	//profiles
	if ( counter->profile == POWER_PROFILE_READ )
	{
		profile_t emptyProfile ;
		memset( &emptyProfile , 0 , sizeof( profile_t ) );
		emptyProfile.status = METERAGE_TIME_WAS_SYNCED ;
		memcpy( &emptyProfile.dts , &currentDts , sizeof( dateTimeStamp ) ) ;
		emptyProfile.dts.t.s = 0 ;

		if ( (counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL) && (counter->profileInterval != 0) && ((60 % counter->profileInterval) == 0) )
		{
			//found current slice time start
			while( (emptyProfile.dts.t.m % counter->profileInterval) != 0 )
			{
				COMMON_ChangeDts( &emptyProfile.dts , DEC , BY_MIN );
			}
			//step into prev slice
			int minuteIndex = 0 ;
			for ( ; minuteIndex < counter->profileInterval ; ++minuteIndex )
			{
				COMMON_ChangeDts( &emptyProfile.dts , DEC , BY_MIN );
			}
		}
		else
		{
			emptyProfile.dts.t.m = 0 ;
			COMMON_ChangeDts( &emptyProfile.dts , DEC , BY_HOUR );
		}
		saveRes = STORAGE_SaveProfile( counter->dbId ,&emptyProfile );
		COMMON_STR_DEBUG(DEBUG_STORAGE_ALL "DEBUG_STORAGE_ALL save empty profile for " DTS_PATTERN " with res [%d] " , DTS_GET_BY_VAL(emptyProfile.dts) , saveRes);
	}


	return ;
}

//
//
//

void STORAGE_PROCESS_UPDATE(char * cfgName){
	DEB_TRACE(DEB_IDX_STORAGE)


	if (strstr(cfgName, "gsm.cfg") != NULL){
		//load new data
		STORAGE_UPDATE_GSM();
	}
	else if (strstr(cfgName, "connection.cfg") != NULL){

		//load new data
		STORAGE_UPDATE_CONN();

	} else if (strstr(cfgName, "plc.cfg") != NULL){
		 STORAGE_UPDATE_PLC();

	} else if (strstr(cfgName, "rs485.cfg") != NULL){
		 STORAGE_UPDATE_RS();

	} else if (strstr(cfgName, "plcNet.cfg") != NULL){
		 STORAGE_UPDATE_PLC_NET();
	}
	else if ( strstr(cfgName, "exio.cfg") != NULL )
	{
		STORAGE_UPDATE_EXIO();
	}
	else if (strstr(cfgName, "eth") != NULL){   // !!!! DO NOT CHANGE TO "ethernet.cfg" !!!!

		//load new data
		STORAGE_UPDATE_ETH();

	}
}


unsigned char STORAGE_ApplyNewConfig( unsigned int mask){
	DEB_TRACE(DEB_IDX_STORAGE)

	// 0x1 - rs485
	// 0x2 - rf
	// 0x4 - rf Net
	// 0x8 - gsm
	// 0x10- eth
	// 0x20 - connection

	unsigned char res = MICRON_RES_OK;

	if (mask & MICRON_PROP_DEV_RS){
		STORAGE_UPDATE_RS();
	}

	if (mask & MICRON_PROP_DEV_PLC){
		 STORAGE_UPDATE_PLC();
	}

	if (mask & MICRON_PROP_COORD_PLC){
		 STORAGE_UPDATE_PLC_NET();
	}

	//if ((mask & MICRON_PROP_GSM) || (mask & MICRON_PROP_ETH) || (mask & MICRON_PROP_USD)){

		//CONNECTION_Stop();


		if (mask & MICRON_PROP_GSM){
			STORAGE_UPDATE_GSM();
		}

		if (mask & MICRON_PROP_ETH){
			 STORAGE_UPDATE_ETH();
		}

		if (mask & MICRON_PROP_USD){
			 STORAGE_UPDATE_CONN();
		}

		//if (CONNECTION_Initialize() != OK )
		//	res = MICRON_RES_GEN_ERR ;

	//}

	return res;

}

//
//
//

int STORAGE_SaveServiceProperties( connection_t * connection )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;
	if ( connection == NULL )
	{
		return res ;
	}

	if ( connection->servUpdateFlag != 1 )
	{
		return res ;
	}

	STORAGE_Protect();

	int fd = open( CONFIG_PATH_SERVICE , O_WRONLY | O_TRUNC | O_CREAT , 0777);
	if ( fd > 0)
	{
		char buffer[ MAX_MESSAGE_LENGTH ];
		memset( buffer , 0,  MAX_MESSAGE_LENGTH);
		int bufferWritePos = 0 ;

		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "[server]\n");


		if ( strlen((char *)connection->serviceIp) > 0 )
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "ip=%s\n" , connection->serviceIp);
		}
		else
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "#ip=\n");
		}

		if ( connection->servicePort > 0 )
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "port=%u\n", connection->servicePort);
		}
		else
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "#port=\n");
		}

		if ( strlen((char *)connection->servicePhone) > 0 )
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "phone=%s\n" , connection->servicePhone);
		}
		else
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "#phone=\n");
		}

		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "\n[connection_properties]\n");
		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "#interval: 0 - hour , 1 - day , 2 - week\n");

		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "interval=%d\n", connection->serviceInterval);
		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "ratio=%d\n", connection->serviceRatio);

		bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "\n[sms_report]\n");
		if ( connection->smsReportAllow == TRUE )
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "allow_report=1\n");
		}
		else
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "allow_report=0\n");
		}

		if ( strlen((char *)connection->smsReportPhone) > 0 )
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "phone=%s\n" , connection->smsReportPhone);
		}
		else
		{
			bufferWritePos += snprintf( &buffer[bufferWritePos] , MAX_MESSAGE_LENGTH - bufferWritePos , "#phone=\n");
		}


		if ( write(fd , &buffer , strlen(buffer)) == (int)strlen(buffer) )
		{
			STORAGE_IncrementHardwareWritingCtrlCounter();
			connection->servUpdateFlag = 2 ;
			res = OK ;
		}

		close(fd);
	}

	STORAGE_Unprotect();

	return res;
}

//
//
//

int STORAGE_CheckNewConfig( unsigned int type , unsigned char * newConfig , int length, int * firstRetValue , int * secRetValue)
{
	int res = ERROR_GENERAL ;

	switch( type )
	{
		case MICRON_PROP_DEV_PLC:
		{
			#if REV_COMPILE_PLC
			res = STORAGE_CheckConfigCountersPlc( (char *)newConfig , length , firstRetValue , secRetValue);
			#endif
		}
		break;

		case MICRON_PROP_COORD_PLC:
		{
			#if REV_COMPILE_PLC
			res = STORAGE_CheckConfigPlcSettings( (char *)newConfig , length , firstRetValue , secRetValue );
			#endif
		}
		break;

		default:
		{
			res = OK ;
		}
		break;
	}

	return res ;
}

//
//
//

unsigned char STORAGE_SaveNewConfig( unsigned int type , unsigned char * newConfig , int length, char * reason)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char res = MICRON_RES_GEN_ERR ;
	unsigned char evDesc[EVENT_DESC_SIZE];

	char pathname[128];
	memset(pathname , 0 , 128);

	char backupName[128] ;
	memset(backupName , 0 , 128);

	// check config file name
	switch( type )
	{
		case MICRON_PROP_DEV_PLC:
		{
			#if REV_COMPILE_PLC
				memcpy( pathname , CONFIG_PATH_PLC , strlen( CONFIG_PATH_PLC ) );
				COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_DEV_PLC, evDesc );
				STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc);
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				//#warning App can not get PLC config
				return res ;
			#endif

		}
		break;

		case MICRON_PROP_DEV_RS:
		{
			memcpy( pathname , CONFIG_PATH_485 , strlen( CONFIG_PATH_485 ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_DEV_RS , evDesc );
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_COORD_PLC:
		{
			#if REV_COMPILE_PLC
				memcpy( pathname , CONFIG_PATH_NET_PLC , strlen( CONFIG_PATH_NET_PLC ) );
				COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_COORD_PLC , evDesc);
				STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				//#warning App can not get PLC config
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_GSM:
		{
			memcpy( pathname , CONFIG_PATH_GSM , strlen( CONFIG_PATH_GSM ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_GSM  , evDesc);
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_ETH:
		{
			memcpy( pathname , CONFIG_PATH_ETH , strlen( CONFIG_PATH_ETH ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_ETH  , evDesc);
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_USD:
		{
			memcpy( pathname , CONFIG_PATH_CONNECTION , strlen( CONFIG_PATH_CONNECTION ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_CONN  , evDesc);
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_EXIO:
		{
			#if REV_COMPILE_EXIO
				memcpy( pathname , EXIO_CFG_PATH , strlen( EXIO_CFG_PATH ) );
				COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_EXIO  , evDesc);
				STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				//#warning App can not get EXIO config
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_DEV_RF:
		{
			#if REV_COMPILE_RF
				memcpy( pathname , CONFIG_PATH_RF , strlen( CONFIG_PATH_RF ) );
				COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_DEV_RF  , evDesc);
				STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				//#warning App can not get RF config
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_COORD_RF:
		{
			#if REV_COMPILE_RF
				memcpy( pathname , CONFIG_PATH_NET_RF , strlen( CONFIG_PATH_NET_RF ) );
				COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_COORD_RF  , evDesc);
				STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				//#warning App can not get RF config
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_PIN:
		{
			memcpy( pathname , CONFIG_PATH_CPIN , strlen( CONFIG_PATH_CPIN ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_CFG_CPIN  , evDesc);
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_COUNTERS_BILL:
		{
			memcpy( pathname , CONFIG_PATH_COUNTERS_BILL , strlen( CONFIG_PATH_COUNTERS_BILL ) );
			COMMON_CharArrayDisjunction( reason , DESC_EVENT_UPDATE_COUNTERS_BILL  , evDesc);
			STORAGE_JournalUspdEvent( EVENT_CFG_UPDATE , evDesc ) ;
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		default:
		{
			return MICRON_RES_WRONG_PARAM;
		}
		break;

	}

	STORAGE_Protect();

	// open config file and .bak
	int cfgFd = open(pathname ,O_RDWR | O_CREAT , S_IRWXU);
	int backFd = open(backupName , O_RDWR | O_CREAT , S_IRWXU );

	if ( ( cfgFd > 0 ) && (backFd > 0) )
	{
		ftruncate(backFd , 0);
		lseek( backFd , 0, SEEK_SET );

		struct stat st;
		//get file size
		fstat(cfgFd, &st);

		int bufSize = st.st_size;
		if ( bufSize > 0 )
		{

			//copy old file to .bak

			char * buf = (char *)malloc( bufSize );
			if( buf )
			{
				memset( buf ,0 , bufSize );
				if ( read( cfgFd , buf , bufSize ) != bufSize )
				{
					close( cfgFd );
					close( backFd );
					STORAGE_Unprotect();
					return res ;
				}

				if ( write( backFd , buf , bufSize ) != bufSize )
				{
					STORAGE_IncrementHardwareWritingCtrlCounter();
					close( cfgFd );
					close( backFd );
					STORAGE_Unprotect();
					return res ;
				}

				free(buf);
			}

		}


		ftruncate(cfgFd , 0);
		lseek( cfgFd , 0, SEEK_SET );

		if( write( cfgFd , newConfig , length ) == length)
		{
			STORAGE_IncrementHardwareWritingCtrlCounter();
			res = MICRON_RES_OK ;
		}

		close( cfgFd ) ;
		close( backFd ) ;

	}
	else
	{
		if ( cfgFd > 0 )
		{
			close( cfgFd );
		}

		if ( backFd > 0 )
		{
			close( backFd );
		}

	}

	STORAGE_Unprotect();

	return res;
}

//
//
//

unsigned char STORAGE_GetCfgFile( unsigned int mask , unsigned char ** buf , int * length )
{
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char res = MICRON_RES_GEN_ERR;

	char pathname[128];
	memset(pathname , 0 , 128);

	char backupName[128] ;
	memset(backupName , 0 , 128);

	// check config file name
	switch( mask )
	{
		case MICRON_PROP_DEV_PLC:
		{
			#if REV_COMPILE_PLC
				memcpy( pathname , CONFIG_PATH_PLC , strlen( CONFIG_PATH_PLC ) );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_DEV_RS:
		{
			memcpy( pathname , CONFIG_PATH_485 , strlen( CONFIG_PATH_485 ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_COORD_PLC:
		{
			#if REV_COMPILE_PLC
				memcpy( pathname , CONFIG_PATH_NET_PLC , strlen( CONFIG_PATH_NET_PLC ) );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_GSM:
		{
			memcpy( pathname , CONFIG_PATH_GSM , strlen( CONFIG_PATH_GSM ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_ETH:
		{
			memcpy( pathname , CONFIG_PATH_ETH , strlen( CONFIG_PATH_ETH ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_USD:
		{
			memcpy( pathname , CONFIG_PATH_CONNECTION , strlen( CONFIG_PATH_CONNECTION ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_EXIO:
		{
			#if REV_COMPILE_EXIO
			memcpy( pathname , EXIO_CFG_PATH , strlen( EXIO_CFG_PATH ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_DEV_RF:
		{
			#if REV_COMPILE_RF
				memcpy( pathname , CONFIG_PATH_RF , strlen( CONFIG_PATH_RF ) );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_COORD_RF:
		{
			#if REV_COMPILE_RF
				memcpy( pathname , CONFIG_PATH_NET_RF , strlen( CONFIG_PATH_NET_RF ) );
				sprintf( backupName , "%s%s" , pathname , ".bak" );
			#else
				return res ;
			#endif
		}
		break;

		case MICRON_PROP_PIN:
		{
			memcpy( pathname , CONFIG_PATH_CPIN , strlen( CONFIG_PATH_CPIN ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		case MICRON_PROP_COUNTERS_BILL:
		{
			memcpy( pathname , CONFIG_PATH_COUNTERS_BILL , strlen( CONFIG_PATH_COUNTERS_BILL ) );
			sprintf( backupName , "%s%s" , pathname , ".bak" );
		}
		break;

		default:
		{
			return MICRON_RES_WRONG_PARAM;
		}
		break;

	}

#if 0
	int cfgFd = open( pathname , O_RDONLY );
	if ( cfgFd == -1 )
	{
		cfgFd = open( backupName , O_RDONLY );
		if ( cfgFd == -1 )
			return res ;
	}

	struct stat statStr;
	memset( &statStr , 0 , sizeof(stat) );

	if ( fstat( cfgFd , &statStr ) != 0 )
	{
		close(cfgFd);
		return res;
	}

	(*length) = statStr.st_size ;

	(*buf) = calloc( (*length) , sizeof(unsigned char) );
	if ( (*buf) != NULL )
	{
		//if ( read( cfgFd , (*buf) , (*length) ) == (*length) )
		//	res = MICRON_RES_OK ;

		int bytesRemaining = (*length);
		while( 1 )
		{
			int bytesReaded = read( cfgFd , &(*buf)[ (*length) - bytesRemaining ] , bytesRemaining );
			if (bytesReaded == -1)
				break;
			if (bytesRemaining == 0)
			{
				res = MICRON_RES_OK ;
				break;
			}
		}
	}

	close(cfgFd);
#endif

	STORAGE_Protect();

	if ( COMMON_GetFileFromFS( buf , length , (unsigned char *)pathname ) == OK )
	{
		res = MICRON_RES_OK ;
	}
	else
	{
		if( COMMON_GetFileFromFS( buf , length , (unsigned char *)backupName ) == OK )
		{
			res = MICRON_RES_OK ;
		}
		else
		{
			res = MICRON_RES_GEN_ERR ;
		}
	}

	STORAGE_Unprotect();

	return res ;
}

//
//
//
char * getPlcStrategyDesc(){
	DEB_TRACE(DEB_IDX_STORAGE)

	switch (plcBaseSettings.strategy){
		case POOL_STRATEGY_LINEAR_POOL: return "POOL_STRATEGY_LINEAR_POOL";
		case POOL_STRATEGY_PLC_PARALLEL_POOL_BY_MAP: return "POOL_STRATEGY_PLC_PARALLEL_POOL_BY_MAP";
		case POOL_STRATEGY_PLC_PARALLEL_POOL_BY_CONFIG: return "POOL_STRATEGY_PLC_PARALLEL_POOL_BY_CONFIG";
		default: return "UNKNOWN STRATEGY";
	}
}

//
//
//
char * getPlcModeDesc(){
	DEB_TRACE(DEB_IDX_STORAGE)

	switch (plcBaseSettings.admissionMode){
		case PLC_ADMISSION_AUTO: return "авто";
		case PLC_ADMISSION_APPLICATION: return "по списку конфигурации";
		case PLC_ADMISSION_WITH_DETECT: return "авто с определением типа ПУ";
		default: return "неизвестный режим";
	}
}

//
//
//

int STORAGE_GetLastSyncDts(dateTimeStamp * dts, unsigned char * from){
	DEB_TRACE(DEB_IDX_STORAGE)


	int res = ERROR_GENERAL ;

	if ( dts != NULL )
	{
		memset(dts , 0 , sizeof(dateTimeStamp));

		STORAGE_JournalProtect();

		uspdEvent_t event ;
		memset(&event , 0 , sizeof(uspdEvent_t) );
		event.evType = EVENT_TIME_SYNCED ;

		char fileName[SFNL];
		int journalNumber = STORAGE_GetUspdJournalByEvent(&event , NULL );
		GET_USPD_JOURNAL_FILE_NAME(fileName,journalNumber);

		int fd = open(fileName, O_RDONLY);
		if(fd >= 0 )
		{
			uspdEvent_t data_in_file;

			struct stat st;
			//get file size
			fstat(fd, &st);

			int size_of_block = sizeof(uspdEvent_t);
			int block_numb = st.st_size / size_of_block ;

			int idx;
			for(idx = 0 ; idx < block_numb; idx++)
			{
				memset(&data_in_file , 0 , sizeof(uspdEvent_t));
				if (read(fd , &data_in_file , sizeof(uspdEvent_t)) == sizeof(uspdEvent_t) )
				{
					if (data_in_file.evType == EVENT_TIME_SYNCED)
					{
						res = OK ;
						memcpy( dts , &(data_in_file.dts) , sizeof(dateTimeStamp) );

						if ( from )
						{
							(*from) = data_in_file.evDesc[6] ;
						}

						break;
					}
				}
			}

			close(fd);
		}

		STORAGE_JournalUnProtect();

	}

	return res;
}

//
//
//

int STORAGE_GetCounterStatistic( counterStat_t ** counterStat , int * counterStatLength )
{
	DEB_TRACE(DEB_IDX_STORAGE);

	int res = ERROR_GENERAL ;

	STORAGE_Protect();
	STORAGE_StatisticProtect();

	(*counterStatLength) = 0;

	if ( counterStatistic.countersStatCount > 0 )
	{
		(*counterStat) = (counterStat_t *)malloc( counterStatistic.countersStatCount * sizeof( counterStat_t ) );
		if ((*counterStat) != NULL){
			( *counterStatLength ) = counterStatistic.countersStatCount ;
			int i = 0 ;
			for ( i=0; i < (*counterStatLength) ; ++i )
			{
				memcpy ( &(*counterStat)[i] , counterStatistic.countersStat[i] , sizeof( counterStat_t ) ) ;
			}
			res = OK ;

		} else {

			//
			// to do mem error
			//
		}
	}
	else
	{
		res = ERROR_NO_DATA_FOUND ;
	}

	STORAGE_StatisticUnprotect();
	STORAGE_Unprotect();

	return res ;
}

//
//
//

int STORAGE_UpdateCounterDts(unsigned long counterDbId , dateTimeStamp * currentCounterDts , int taskTime)
{
	DEB_TRACE(DEB_IDX_STORAGE)

	int res = ERROR_GENERAL;
	counter_t * counter = NULL;
	
	STORAGE_CountersProtect();
	
	if ( STORAGE_FoundCounterNoAtom(counterDbId, &counter) == OK)
	{
		int worth = COMMON_GetDtsWorth( currentCounterDts );

		counter->syncWorth = worth + taskTime;
		memcpy( &counter->currentDts , currentCounterDts , sizeof(dateTimeStamp) );

		STORAGE_SwitchSyncFlagTo_Acq( &counter );
		res = OK;
	}
	
	STORAGE_CountersUnprotect();
	
	return res ;
}

//
//
//

void STORAGE_SwitchSyncFlagTo_Acq(counter_t ** counter)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	pthread_mutex_lock(&counterSyncFlagMutex);

	if ((*counter)->syncFlag & STORAGE_FLAG_EMPTY)
		(*counter)->syncFlag -=STORAGE_FLAG_EMPTY;
	(*counter)->syncFlag |= STORAGE_WORT_ACQ ;

	pthread_mutex_unlock(&counterSyncFlagMutex);
}

//
//
//

void STORAGE_SwitchSyncFlagTo_SyncProcess(counter_t ** counter)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	pthread_mutex_lock(&counterSyncFlagMutex);

	if ((*counter)->syncFlag & STORAGE_FLAG_EMPTY)
		(*counter)->syncFlag -=STORAGE_FLAG_EMPTY;
	if ((*counter)->syncFlag & STORAGE_SYNC_DONE)
		(*counter)->syncFlag -=STORAGE_SYNC_DONE;
	if ((*counter)->syncFlag & STORAGE_SYNC_FAIL)
		(*counter)->syncFlag -=STORAGE_SYNC_FAIL;

	(*counter)->syncFlag |= STORAGE_SYNC_PROCESS;

	pthread_mutex_unlock(&counterSyncFlagMutex);
}

//
//
//

void STORAGE_SwitchSyncFlagTo_NeedToSync(counter_t ** counter)
{
	//pthread_mutex_lock(&counterSyncFlagMutex);

	DEB_TRACE(DEB_IDX_STORAGE);

	if ((*counter)->syncFlag & STORAGE_FLAG_EMPTY)
		(*counter)->syncFlag -=STORAGE_FLAG_EMPTY;
	if ((*counter)->syncFlag & STORAGE_SYNC_PROCESS)
		(*counter)->syncFlag -=STORAGE_SYNC_PROCESS;
	if ((*counter)->syncFlag & STORAGE_SYNC_DONE)
		(*counter)->syncFlag -=STORAGE_SYNC_DONE;
	if ((*counter)->syncFlag & STORAGE_SYNC_FAIL)
		(*counter)->syncFlag -=STORAGE_SYNC_FAIL;

	(*counter)->syncFlag |= STORAGE_NEED_TO_SYNC;

	//pthread_mutex_unlock(&counterSyncFlagMutex);
}

//
//
//

void STORAGE_SwitchSyncFlagTo_SyncDone(counter_t ** counter)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	pthread_mutex_lock(&counterSyncFlagMutex);

	(*counter)->syncFlag = STORAGE_SYNC_DONE | STORAGE_FLAG_EMPTY;

	//need to reread counter dateTimeStamp
	memset( &(*counter)->currentDts , 0 , sizeof(dateTimeStamp) );

	pthread_mutex_unlock(&counterSyncFlagMutex);
}

//
//
//

void STORAGE_SwitchSyncFlagTo_SyncFail(counter_t ** counter)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	pthread_mutex_lock(&counterSyncFlagMutex);

	(*counter)->syncFlag = STORAGE_SYNC_FAIL | STORAGE_WORT_ACQ;

	pthread_mutex_unlock(&counterSyncFlagMutex);
}

//
//
//

BOOL STORAGE_IsCounterNeedToSync(counter_t ** counter)
{
	DEB_TRACE(DEB_IDX_STORAGE);

	BOOL res = FALSE ;

	pthread_mutex_lock(&counterSyncFlagMutex);

	if ( ( (*counter)->syncFlag & STORAGE_NEED_TO_SYNC ) && ( (*counter)->syncFlag & STORAGE_WORT_ACQ ))
	{
		res = TRUE ;
	}

	pthread_mutex_unlock(&counterSyncFlagMutex);

	return res ;
}

//
//
//
void STORAGE_CalcEffectivePollWeight(unsigned long dbid, double * epw){
	DEB_TRACE(DEB_IDX_STORAGE)

	int timeDiff = 0;
	counter_t * counter = NULL;
	dateTimeStamp lastDts;
	dateTimeStamp currDts;
	double korrective = (double)1.0;

	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_CalcEffectivePollWeight start");

	//get current time
	COMMON_GetCurrentDts(&currDts);

	//setup sub-zero initializer
	(*epw) = POLL_WEIGHT_MIN;

	//
	// process weight calculation with next table of criteria
	//

	//  needed meter job | priority |     epw[i]     |          epw[i]norm          |
	// ------------------+----------+----------------+------------------------------+
	// 1. profile        |     4    | 0,138888888889 | *(N-0.9)/(N+2)   or 0 if N=0 |
	// 2. day            |     2    | 0,194444444444 |                              |
	// 3. month          |     3    | 0,166666666667 |                              |
	// 4. current        |     7    | 0,055555555555 | *(N-0.1)/(N+2)   or 0 if N=0 |
	// 5. pqp            |     8    | 0.027777777777 | *(N-0.1)/(N+2)   or 0 if N=0 |
	// 6. power          |     6    | 0,083333333333 |                              |
	// 7. tarification   |     5    | 0,111111111111 |                              |
	// 8. user commands  |     1    | 0,222222222222 |                              |
	// ------------------+----------+----------------+------------------------------+

	//0. see in counter date time stamp
	if (STORAGE_FoundCounterNoAtom(dbid, &counter) == OK){
		if (counter != NULL){
			if( COMMON_CheckDtsForValid(&counter->currentDts) != OK ){
				(*epw) = (double)1.0;
				COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_CalcEffectivePollWeight done (no dts), dbib %u, epw %0.05f", dbid, (*epw));
				return;
			}
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}

	// 1. see in user commands
	// to do
	// if needed do user commands
	//(*epw)+= (double)0.222222222222;

	// 2. see in day storage
	timeDiff = 0;
	memset(&lastDts, 0, sizeof(dateTimeStamp));
	if(STORAGE_GetLastMeterageDts(STORAGE_DAY, dbid, &lastDts) == OK){
		timeDiff = COMMON_GetDtsDiff__Alt(&lastDts, &currDts, BY_DAY);
		if (timeDiff > 0){
			(*epw)+= (double)0.194444444444;
		}
	} else {
		(*epw)+= (double)0.194444444444;
	}

	// 3. see in month
	timeDiff = 0;
	memset(&lastDts, 0, sizeof(dateTimeStamp));
	if(STORAGE_GetLastMeterageDts(STORAGE_MONTH, dbid, &lastDts) == OK){
		timeDiff = COMMON_GetDtsDiff__Alt(&lastDts, &currDts, BY_MONTH);
		if (timeDiff > 0){
			(*epw)+= (double)0.166666666667;
		}
	} else {
		(*epw)+= (double)0.166666666667;
	}

	// 4. see in profile
	timeDiff = 0;
	memset(&lastDts, 0, sizeof(dateTimeStamp));
	if(STORAGE_GetLastProfileDts(dbid, &lastDts) == OK){
		timeDiff = COMMON_GetDtsDiff__Alt(&lastDts, &currDts, BY_HALF_HOUR);
		if (timeDiff > 0){
			//get corrective for profile weight value
			korrective = (double)(((double)timeDiff-(double)0.9)/((double)timeDiff+(double)2));

			//correction
			(*epw)+= (korrective * (double)0.138888888889);
		}
	} else {
		(*epw) = (double)0.138888888889;
	}

	// 5. see in tarification
	//TODO
	// if needed do tarification
	//(*epw)+= (double)0.111111111111;

	// 6. see in power control needs
	//TODO
	// if needed do power switching
	//(*epw)+= (double)0.083333333333;

	// 7. see in current
	timeDiff = 0;
	memset(&lastDts, 0, sizeof(dateTimeStamp));
	if(STORAGE_GetLastMeterageDts(STORAGE_CURRENT, dbid, &lastDts) == OK){
		timeDiff = COMMON_GetDtsDiff__Alt(&lastDts, &currDts, BY_HOUR);
		if (timeDiff > 0){
			//get corrective for profile weight value
			korrective = (double)(((double)timeDiff-(double)0.1)/((double)timeDiff+(double)2));

			//correction
			(*epw)+= (korrective * (double)0.055555555555);
		}
	} else {
		(*epw) = (double)0.055555555555;
	}

	//8. see in pqp
	powerQualityParametres_t pqpVals;
	memset(&pqpVals, 0, sizeof(powerQualityParametres_t));
	if (STORAGE_GetPQPforDB(&pqpVals, dbid) == OK){
		timeDiff = COMMON_GetDtsDiff__Alt(&pqpVals.dts, &currDts, BY_HOUR);
		if (timeDiff > 0){
			//get corrective for profile weight value
			korrective = (double)(((double)timeDiff-(double)0.1)/((double)timeDiff+(double)2));

			//correction
			(*epw)+= (korrective * (double)0.027777777777);
		}
	} else {
		(*epw)+= (double)0.027777777777;
	}

	COMMON_STR_DEBUG(DEBUG_STORAGE_PLC "STORAGE_CalcEffectivePollWeight done, dbib %u, epw %0.05f", dbid, (*epw));
}

//
//
//

void STORAGE_EVENT_PLC_REMOTE_WAS_DELETED (unsigned char * serial){
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char details[10];
	memset (details, 0, 10);
	if ((strlen((char *)serial) > 0) && (strlen((char *)serial) <= 10))
		memcpy (details, serial, strlen((char *)serial));

	STORAGE_JournalUspdEvent(EVENT_PLC_TABLE_DEL_REMOTE, details);
}

void STORAGE_EVENT_PLC_REMOTE_WAS_ADDED (unsigned char * serial){
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char details[10];
	memset (details, 0, 10);
	if ((strlen((char *)serial) > 0) && (strlen((char *)serial) <= 10))
		memcpy (details, serial, strlen((char *)serial));

	STORAGE_JournalUspdEvent(EVENT_PLC_TABLE_NEW_REMOTE, details);
}

void STORAGE_EVENT_PLC_DEVICE_START (unsigned char code){
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char details[10];
	memset (details, 0, 10);
	details[0] = code;

	STORAGE_JournalUspdEvent(EVENT_PLC_ISM_START_CODE, details);
}

void STORAGE_EVENT_PLC_DEVICE_ERROR (unsigned char code){
	DEB_TRACE(DEB_IDX_STORAGE)

	unsigned char details[10];
	memset (details, 0, 10);
	details[0] = code;

	STORAGE_JournalUspdEvent(EVENT_PLC_ISM_ERROR_CODE, details);
}

void STORAGE_EVENT_DISCONNECTED (unsigned char code)
{
	connection_t connection;
	int connectionTime = 0 ;

	memset(&connection , 0 , sizeof(connection_t));

	STORAGE_GetConnection( &connection);

	switch(connection.mode)
	{
		case CONNECTION_GPRS_SERVER:
		case CONNECTION_GPRS_ALWAYS:
		case CONNECTION_GPRS_CALENDAR:
		case CONNECTION_CSD_INCOMING:
		case CONNECTION_CSD_OUTGOING:
		{
			connectionTime = GSM_GetCurrentConnectionTime();

		}
		break;

		default:
		{
			connectionTime = ETH_GetCurrentConnectionTime();
		}
		break;
	}


	unsigned char evDesc[EVENT_DESC_SIZE];
	memset(evDesc , 0 , EVENT_DESC_SIZE );
	evDesc[0] = (connectionTime >> 24) & 0xFF ;
	evDesc[1] = (connectionTime >> 16) & 0xFF ;
	evDesc[2] = (connectionTime >> 8)  & 0xFF ;
	evDesc[3] = connectionTime & 0xFF ;
	evDesc[4] = code;
	STORAGE_JournalUspdEvent( EVENT_DISCONNECTED_FROM_SERVER , evDesc );
}

// EOF

