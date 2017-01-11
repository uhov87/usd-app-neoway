/*
		application code v.1
		for uspd micron 2.0x

		2012 - January
		NPO Frunze
		RUSSIA NIGNY NOVGOROD

		OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "boolType.h"
#include "common.h"
#include "pool.h"
#include "connection.h"
#include "storage.h"
#include "ujuk.h"
#include "gubanov.h"
#include "mayk.h"
#include "serial.h"
#include "unknown.h"
#include "plc.h"
#include "pio.h"
#include "cli.h"
#include "micron_v1.h"
#include "debug.h"


//local variables
volatile BOOL poolThreadPlcOk = FALSE;
volatile BOOL poolExecutionPlc = FALSE;
volatile BOOL poolThread485Ok = FALSE;
volatile BOOL poolExecution485 = FALSE;
volatile BOOL poolExecutionPlcSuspended = FALSE;
volatile BOOL poolExecution485Suspended = FALSE;
volatile BOOL poolPlcPaused = FALSE;
volatile BOOL poolPlcInPause = FALSE;
volatile BOOL pool485Paused = FALSE;
volatile BOOL pool485InPause = FALSE;

volatile BOOL poolTransparentRs485 = FALSE;
volatile BOOL poolTransparentPlc = FALSE;
volatile BOOL poolTransparentInLoop = FALSE;

volatile int transparentInWaitAnswer = 0;
volatile int plcTransparentReady = 0; //flag if plc answer

unsigned char poolPlcSerialTransparent[SIZE_OF_SERIAL+1];
unsigned long transparentDbId = 0xFFFFFFFF;
time_t transparentStart;

int appendTimeout = 0;

unsigned char * plcTrBuf = NULL;
int plcTrBufSz = 0;
unsigned int plcTransparentRemoteId = 0xFFFFFFFF;

int lastRemoteIndex = 0;
counterTask_t * counter485Task = NULL;

extern int plcReinitErrors;

pthread_t poolPlcThread;
pthread_t poolRs485Thread;
pthread_mutex_t poolPlcMux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t poolRs485Mux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t poolPlcTrMux = PTHREAD_MUTEX_INITIALIZER;

//thread declaration
void * POOL_DoPlc(void * pAram);
void * POOL_Do485(void * pAram);
void POOL_ReadTransparentPlc();
void POOL_ReadTransparentRs485(unsigned char ** recvBuf, int * size);

void POOL_WriteTransparentRs485(unsigned char * sendBuf, int size);
void POOL_WriteTransparentPlc(unsigned char * sendBuf, int size);

void POOL_TransparentCheckTime();
void POOL_ResetTransparent();

//
//
//

void POOL_ProtectPlc() {

	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_lock(&poolPlcMux);
}

//
//
//

void POOL_UnprotectPlc() {

	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_unlock(&poolPlcMux);
}

//
//
//

void POOL_ProtectRs485() {
	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_lock(&poolRs485Mux);
}

//
//
//

void POOL_UnprotectRs485() {
	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_unlock(&poolRs485Mux);
}

//
//
//

void POOL_ProtectPlcTrBuffer(){
	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_lock(&poolPlcTrMux);
}

//
//
//

void POOL_UnprotectPlcTrBuffer(){
	DEB_TRACE(DEB_IDX_POOL)

	pthread_mutex_unlock(&poolPlcTrMux);
}

//
// Initialize pool task stack
//

int POOL_Initialize() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = OK;
	int poolInitCount;

	//init thread for Plc
#if (REV_COMPILE_PLC == 1)
	poolExecutionPlc = TRUE;

	//create thread
	pthread_create(&poolPlcThread, NULL, POOL_DoPlc, 0);
	poolInitCount = 0;

	//wait pool thread creation comlete
	do {
		poolInitCount++;
		COMMON_Sleep(DELAY_POOL_INIT);
	} while ((!POOL_IsRunPlc()) && (poolInitCount < MAX_COUNT_POOL_INIT_WAIT));

	//check thread ok
	if (POOL_IsWorkPlc()){
		res = OK;
	} else {
		CLI_SetErrorCode(CLI_ERROR_POOL_PLC_TR, CLI_SET_ERROR);
	}
#endif

	//init thread for 485
#if (REV_COMPILE_485 == 1)
	//create thread
	poolExecution485 = TRUE;
	pthread_create(&poolRs485Thread, NULL, POOL_Do485, 0);
	poolInitCount = 0;

	//wait pool thread creation complete
	do {
		poolInitCount++;
		COMMON_Sleep(DELAY_POOL_INIT);
	} while ((!POOL_IsRun485()) && (poolInitCount < MAX_COUNT_POOL_INIT_WAIT));


	//check thread ok
	if (POOL_IsWork485()){
		res = OK;
	} else {
		CLI_SetErrorCode(CLI_ERROR_POOL_485_TR, CLI_SET_ERROR);
	}
#endif

	//debug
	COMMON_STR_DEBUG(DEBUG_MAIN "POOL_Initialize() %s", getErrorCode(res));

	return res;
}

//
// Stop pool execution on rf part
//

void POOL_StopPlc() {
	DEB_TRACE(DEB_IDX_POOL)

	poolExecutionPlc = FALSE;
}

//
// Start pool execution on rf part
//

void POOL_StartPlc() {
	DEB_TRACE(DEB_IDX_POOL)

	poolExecutionPlc = TRUE;
}

//
// Check pool execution on rf part
//

BOOL POOL_IsWorkPlc() {
	DEB_TRACE(DEB_IDX_POOL)

	return (poolExecutionPlc && poolThreadPlcOk);
}

//
// Check pool thread state on rf part
//

BOOL POOL_IsRunPlc() {
	DEB_TRACE(DEB_IDX_POOL)

	return poolThreadPlcOk;
}

//
//
//
void POOL_GetStatusOfPlc(char * p){
	DEB_TRACE(DEB_IDX_POOL)

	if (POOL_IsWorkPlc() == TRUE){
		sprintf(p, "опрос / приборов: %d", STORAGE_CountOfCountersPlc());
	} else {
		sprintf(p, "простой / приборов: %d", STORAGE_CountOfCountersPlc());
	}
}

//
// Stop pool execution on 485 part
//

void POOL_Stop485() {
	DEB_TRACE(DEB_IDX_POOL)

	poolExecution485 = FALSE;
}

//
// Start pool execution on 485 part
//

void POOL_Start485() {
	DEB_TRACE(DEB_IDX_POOL)

	poolExecution485 = TRUE;
}

//
// Check pool execution on 485 part
//

BOOL POOL_IsWork485() {
	DEB_TRACE(DEB_IDX_POOL)

	return (poolExecution485 && poolThread485Ok);
}

//
// Check pool thread state on 485 part
//

BOOL POOL_IsRun485() {
	DEB_TRACE(DEB_IDX_POOL)

	return poolThread485Ok;
}

//
//
//
void POOL_GetStatusOf485(char * p){
	DEB_TRACE(DEB_IDX_POOL)

	if (POOL_IsWork485() == TRUE){
		sprintf(p, "опрос / приборов: %d", STORAGE_CountOfCounters485());
	} else {
		sprintf(p, "простой / приборов: %d", STORAGE_CountOfCounters485());
	}
}

//
// pause pool functions
//
void POOL_Pause(){
	poolPlcPaused = TRUE;
	pool485Paused = TRUE;

	COMMON_STR_DEBUG(DEBUG_MAIN "POOL_Pause ...wait till plc and 485 threads will paused");

	while ((poolPlcInPause == FALSE) || (pool485InPause == FALSE)){
		COMMON_Sleep(100);
	}

	COMMON_STR_DEBUG(DEBUG_MAIN "POOL paused");
}

//
// resume pool functions
//
void POOL_Continue(){
	poolPlcPaused = FALSE;
	pool485Paused = FALSE;

	COMMON_STR_DEBUG(DEBUG_MAIN "POOL_Continue");
}

//
// thread completition ends wait foo
//

void POOL_WaitPlcLoopEnds(){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "enter wait PLC loop snooze");

	if (poolExecutionPlc == FALSE){
		while (poolExecutionPlcSuspended != TRUE){
			COMMON_Sleep (100);
		}
	}

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "exit wait PLC loop snooze");
}

//
// thread completition ends wait foo
//

void POOL_Wait485LoopEnds(){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_485 "enter wait 485 loop snooze");

	if (poolExecution485 == FALSE){
		while (poolExecution485Suspended != TRUE){
			COMMON_Sleep (100);
		}
	}

	COMMON_STR_DEBUG(DEBUG_POOL_485 "exit wait 485 loop snooze");
}

//
//
//

void POOL_ResumePlcLoop(){
	DEB_TRACE(DEB_IDX_POOL)

	poolExecutionPlc = TRUE;
}

//
//
//

void POOL_Resume485Loop(){
	DEB_TRACE(DEB_IDX_POOL)

	poolExecution485 = TRUE;
}

//
// thread foo of pool, executed in cycle, on rf part
//


void * POOL_DoPlc(void * pAram __attribute__((unused))) {
	DEB_TRACE(DEB_IDX_POOL)

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL thread PLC started");

	poolThreadPlcOk = TRUE;

	while (poolThreadPlcOk) {

		//if we resume plc poller loop -> reset suspend flag
		if (poolExecutionPlc == TRUE){
			if (poolExecutionPlcSuspended == TRUE){
				poolExecutionPlcSuspended = FALSE;
				PLC_SetNeedToReset(TRUE);
			}
		}

		while (poolExecutionPlc) {

			SVISOR_THREAD_SEND(THREAD_POOL_PLC);

			//process table job
			PLC_ProcessTable();

			//check plc reinits grows
			if (plcReinitErrors > MAX_PLC_REINIT_COUNT){

				//debug
				COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL thread PLC reinit tries reached, need to reboot");

				//do error reboot
				PIO_RebootUspd();
				SHELL_CMD_REBOOT;
			}


			if (poolTransparentPlc == TRUE){

				poolTransparentInLoop = TRUE;

				if (transparentInWaitAnswer == 1){

					POOL_ReadTransparentPlc();

					POOL_ProtectPlcTrBuffer();
					MICRON_SendTransparent(plcTrBuf, plcTrBufSz);
					if ((plcTrBuf != NULL) && (plcTrBufSz > 0)){
						free (plcTrBuf);
						plcTrBuf = NULL;
						plcTrBufSz = 0;
					}
					POOL_UnprotectPlcTrBuffer();


				}

				COMMON_Sleep(100);

			} else {

				int res = ERROR_GENERAL;
				int lastWasTimeouted = 0;

				//process counters polling
				res = POOL_ProcessCountersOnPlc(&lastWasTimeouted);

				//if poll was success and no timeouts with 20 secs was occured, ok, well, pim can sleep 2 secs
				if ((res == OK) && (lastWasTimeouted == 0)){
					COMMON_Sleep(2000);

				//else - no slep, do poll job, do poll job...
				} else {
					COMMON_Sleep(100);

				}
			}

			//check we need be paused
			while (poolPlcPaused && poolExecutionPlc){
				poolPlcInPause = TRUE;
				COMMON_Sleep(100);
			}
			poolPlcInPause = FALSE;
		}


		//if we snoose plc poller loop -> setup suspend flag and stop rf part
		if (poolExecutionPlc == FALSE){
			if (poolExecutionPlcSuspended == FALSE){
				//stop plc thread
				PLC_Stop();

				//reset pused state
				poolPlcPaused = FALSE;

				//wait till plc thread will be stopped
				while (PLC_IsPaused() == FALSE){
					COMMON_Sleep(100);
				}

				//setup suspended flag
				poolExecutionPlcSuspended = TRUE;

				//debug
				COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL thread PLC PAUSE");
			}
		}

		COMMON_Sleep(100);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL thread PLC quit");

	poolThreadPlcOk = FALSE;

	return NULL ;
}

//
// thread foo of pool, executed in cycle, on 485 part
//

void * POOL_Do485(void * pAram __attribute__((unused))) {
	DEB_TRACE(DEB_IDX_POOL)

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL thread 485 started");

	poolThread485Ok = TRUE;

	while (poolThread485Ok) {

		//if we resume rf poller loop -> reset suspend flag
		if (poolExecution485 == TRUE){
			if (poolExecution485Suspended == TRUE){

				COMMON_STR_DEBUG(DEBUG_POOL_485 "485 loop will be resume");

				//reset suspend flag
				poolExecution485Suspended = FALSE;

				//debug
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL thread 485 RESUMED");
			}
		}

		while (poolExecution485) {
			SVISOR_THREAD_SEND(THREAD_POOL_485);

			if (poolTransparentRs485 == TRUE){

				poolTransparentInLoop = TRUE;

				if (transparentInWaitAnswer == 1){

					int recvSize = 0;
					unsigned char * recvBuf = NULL;

					POOL_ReadTransparentRs485(&recvBuf, &recvSize);
					MICRON_SendTransparent(recvBuf, recvSize);
					if ((recvBuf != NULL) && (recvSize > 0)){
						free (recvBuf);
						recvBuf = NULL;
					}
				}


			} else {

				int res = ERROR_GENERAL;
				res = POOL_ProcessCountersOn485();
				if (res != OK){
					res = POOL_ErrorHandle(res, POOL_ID_485);
				} else {
					CLI_SetErrorCode(CLI_ERROR_POOL_485_TR, CLI_CLEAR_ERROR);
				}
			}

			COMMON_Sleep(500);

			//check we need be paused
			while (pool485Paused && poolExecution485){
				pool485InPause = TRUE;
				COMMON_Sleep(100);
			}
			pool485InPause = FALSE;

		}

		//if we snoose rf poller loop -> setup suspend flag and stop rf part
		if (poolExecution485 == FALSE){
			if (poolExecution485Suspended == FALSE){

				COMMON_STR_DEBUG (DEBUG_POOL_485 "485 loop will be snooze");

				//setup suspended flag
				poolExecution485Suspended = TRUE;

				//debug
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL thread 485 PAUSE");
			}
		}

		COMMON_Sleep(100);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL thread 485 quit");

	poolThread485Ok = FALSE;

	return NULL ;
}

//
//
//

int POOL_ProcessCountersOn485() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	res = POOL_Create485Task();
	if (res == OK) {
		res = POOL_Run485Task();
		if (res == OK) {
			res = POOL_Save485TaskResults();
		}
	}
	POOL_Free485Task();

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_ProcessCountersOn485() %s", getErrorCode(res));

	return res;
}


//
//
//

int POOL_Create485Task() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	counter_t * counter = NULL;

	//enter cs
	POOL_ProtectRs485();

	res = STORAGE_GetNextCounter485(&counter);

	//setup serial attributes for next counter

	if (res == OK) {

		SERIAL_SetupAttributesRs485( &counter->serial485Attributes );

		switch (counter->type) {
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SET_4TM_01A:
			case TYPE_UJUK_SET_4TM_02A:
			case TYPE_UJUK_SET_4TM_03A:
			case TYPE_UJUK_PSH_3TM_05A:
			case TYPE_UJUK_PSH_4TM_05A:
			case TYPE_UJUK_SEO_1_16:
			case TYPE_UJUK_PSH_4TM_05MB:
			case TYPE_UJUK_PSH_4TM_05MD:
			case TYPE_UJUK_PSH_3TM_05MB:
			case TYPE_UJUK_SET_4TM_02MB:
			case TYPE_UJUK_SEB_1TM_02D:
			case TYPE_UJUK_PSH_3TM_05D:
			case TYPE_UJUK_PSH_4TM_05D:
			case TYPE_UJUK_PSH_4TM_05MK:
			case TYPE_UJUK_SET_1M_01:
			case TYPE_UJUK_SEB_1TM_02M:
			case TYPE_UJUK_SEB_1TM_03A:
			case TYPE_UJUK_SET_4TM_03MB:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() for ujuk");
				res = UJUK_CreateCounterTask(counter, &counter485Task);
				break;

			case TYPE_GUBANOV_PSH_3TA07_DOT_1:
			case TYPE_GUBANOV_PSH_3TA07_DOT_2:
			case TYPE_GUBANOV_PSH_3TA07A:
			case TYPE_GUBANOV_SEB_2A05:
			case TYPE_GUBANOV_SEB_2A07:
			case TYPE_GUBANOV_SEB_2A08:
			case TYPE_GUBANOV_PSH_3ART:
			case TYPE_GUBANOV_PSH_3ART_1:
			case TYPE_GUBANOV_PSH_3ART_D:
			case TYPE_GUBANOV_MAYK_101: //MAYAK-101
			case TYPE_GUBANOV_MAYK_102: //MAYAK-102
			case TYPE_GUBANOV_MAYK_301: //MAYAK-301
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() for gubanov");
				res = GUBANOV_CreateCounterTask(counter, &counter485Task);
				break;

			case TYPE_MAYK_MAYK_101: //MAYAK
			case TYPE_MAYK_MAYK_103: //MAYAK
			case TYPE_MAYK_MAYK_302: //MAYAK
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() for mayak");
				res = MAYK_CreateCounterTask( counter , &counter485Task );
				break;

			case TYPE_COUNTER_UNKNOWN:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() for unknown");
				res = UNKNOWN_CreateCounterTask( counter , &counter485Task );
				break;

			default:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() for counter type [%hhu] error", counter->type);
				res = ERROR_GENERAL;
				break;
		}
	}

	if (res == OK){
		STORAGE_SetCounterStatistic(counter->dbId, TRUE, 0, 0);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Create485Task() %s", getErrorCode(res));

	//leave cs
	POOL_UnprotectRs485();

	return res;
}

//
//
//

int POOL_Run485Task() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;
	int bytesWrite = 0;
	int bytesRead = 0;
	unsigned int transactionIndex = 0;
	time_t tRun;
	time_t tEnd;
	unsigned char bytes[255];
	int unknownId = -1;
	
	if (counter485Task != NULL) {

		//osipov 2015-03-13
		//useful trick
		if (counter485Task->counterType == TYPE_COUNTER_UNKNOWN)
		{			
			if (STORAGE_GetUnknownId(counter485Task->counterDbId, &unknownId) != OK)
			{
				COMMON_STR_DEBUG(DEBUG_POOL_485 "Error getting unknown type of counter while RS-485 poll");
				return res;
			}
		}

		//loop on transactions
		for (; transactionIndex < counter485Task->transactionsCount; transactionIndex++)
		{
			BOOL breakLoop = FALSE ;

			//check transparent
			if (poolTransparentRs485 == TRUE){
				res = ERROR_TRANSPARENT_INTERRUPT;
				break;
			}

			//debug
			COMMON_BUF_DEBUG(DEBUG_POOL_485 "485 TX", counter485Task->transactions[transactionIndex]->command, counter485Task->transactions[transactionIndex]->commandSize);

			//write command (byte to bytes write on rs-485)
			res = SERIAL_Write(PORT_RS,
					counter485Task->transactions[transactionIndex]->command,
					counter485Task->transactions[transactionIndex]->commandSize,
					&bytesWrite);

			//set start transaction time
			counter485Task->transactions[ transactionIndex ]->tStart = time( NULL );

			//check write
			if (res == OK){
				//sleep before read
				COMMON_Sleep(DELAY_BEETWEN_SERIAL_READ_WRITE);

			} else {
				//exit pool
				res = ERROR_SIZE_WRITE;
				counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
				break;
			}


			//try to get answer
			switch (counter485Task->counterType){

				//read routine for MAYAK counters
				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_302:
				case TYPE_MAYK_MAYK_103:
				{
					int packetBoundsCount = 0;
					time(&tRun);
					while (packetBoundsCount != 2)
					{

						//calc we need to read
						bytesRead = 255; //counter485Task->transactions[transactionIndex]->waitSize - counter485Task->transactions[transactionIndex]->answerSize;

						//read
						memset(bytes, 0, 255);
						res = SERIAL_Read(PORT_RS, bytes, bytesRead, &bytesRead);
						if (res == OK)
						{
							if (bytesRead > 0)
							{

								//debug
								COMMON_BUF_DEBUG(DEBUG_POOL_485 "485 RX", bytes, bytesRead);

								//realloc mem for amswer
								counter485Task->transactions[transactionIndex]->answerSize+=bytesRead;
								counter485Task->transactions[transactionIndex]->answer = (unsigned char *) realloc(counter485Task->transactions[transactionIndex]->answer, ( (counter485Task->transactions[transactionIndex]->answerSize)* sizeof (unsigned char)));

								//setup read byte
								memcpy(&counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize - bytesRead], bytes, bytesRead);

								//check start of packet
								if ((packetBoundsCount == 0) && (counter485Task->transactions[transactionIndex]->answer[0] == 0x7E)){
									packetBoundsCount = 1;
									counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
								}

								//check end of packet
								if ((packetBoundsCount == 1) &&  (counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize-1] == 0x7E)){
									packetBoundsCount = 2;
									counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
								}

								//retime start time
								time(&tRun);

							}
							else
							{
								COMMON_Sleep(DELAY_BEETWEN_SERIAL_READ_WRITE); //sleep to recieve some new data
							}
						}
						else
						{
							breakLoop = TRUE;
							break; //serial error
						}

						//fix reading times
						time(&tEnd);
						int tDiff = tEnd - tRun;
						if ((tDiff > TIMEOUT_READING_ON_RS485) || (tDiff < 0)){
							res = ERROR_TIME_IS_OUT;
							breakLoop = TRUE;
							break;
						}
					}
				}
				break;

				case TYPE_COUNTER_UNKNOWN:
				{
					switch( unknownId )
					{
						case UNKNOWN_ID_MAYK:
						{
							int packetBoundsCount = 0;
							time(&tRun);
							while (packetBoundsCount != 2) {

								//calc we need to read
								bytesRead = 255;

								//read
								memset(bytes, 0, 255);
								res = SERIAL_Read(PORT_RS, bytes, bytesRead, &bytesRead);
								if (res == OK) {
									if (bytesRead > 0) {

										//debug
										COMMON_BUF_DEBUG(DEBUG_POOL_485 "485 RX", bytes, bytesRead);

										//realloc mem for amswer
										counter485Task->transactions[transactionIndex]->answerSize+=bytesRead;
										counter485Task->transactions[transactionIndex]->answer = (unsigned char *) realloc(counter485Task->transactions[transactionIndex]->answer, ( (counter485Task->transactions[transactionIndex]->answerSize)* sizeof (unsigned char)));

										//setup read byte
										memcpy(&counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize - bytesRead], bytes, bytesRead);

										//check start of packet
										if ((packetBoundsCount == 0) && (counter485Task->transactions[transactionIndex]->answer[0] == 0x7E)){
											packetBoundsCount = 1;
											counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
										}

										//check end of packet
										if (packetBoundsCount == 1) {
											if ((counter485Task->transactions[transactionIndex]->answerSize) > 9 ) {
												if (counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize-1] == 0x7E) {
													packetBoundsCount = 2;
													counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;

												}
											}
										}

										//retime start time
										time(&tRun);

									} else {

										COMMON_Sleep(DELAY_BEETWEN_SERIAL_READ_WRITE); //sleep to recieve some new data
									}
								} else {
									breakLoop = TRUE;
									break; //serial error
								}

								//fix reading times
								time(&tEnd);
								int tDiff = tEnd - tRun;
								if ((tDiff > TIMEOUT_READING_ON_RS485) || (tDiff < 0)){
									res = ERROR_TIME_IS_OUT;
									breakLoop = TRUE;
									break;
								}
							}
						}
						break;

						default:
						{
							time(&tRun);
							while (counter485Task->transactions[transactionIndex]->answerSize < counter485Task->transactions[transactionIndex]->waitSize) {

								//calc we need to read
								bytesRead = counter485Task->transactions[transactionIndex]->waitSize - counter485Task->transactions[transactionIndex]->answerSize;

								//read
								memset(bytes, 0, 255);
								res = SERIAL_Read(PORT_RS, bytes, bytesRead, &bytesRead);
								if (res == OK) {
									if (bytesRead > 0) {

										//debug
										COMMON_BUF_DEBUG(DEBUG_POOL_485 "485 RX", bytes, bytesRead);

										//realloc mem for amswer
										counter485Task->transactions[transactionIndex]->answerSize+=bytesRead;
										counter485Task->transactions[transactionIndex]->answer = (unsigned char *) realloc(counter485Task->transactions[transactionIndex]->answer, ( (counter485Task->transactions[transactionIndex]->answerSize)* sizeof (unsigned char)));

										//setup read byte
										memcpy(&counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize - bytesRead], bytes, bytesRead);

										//retime start time
										time(&tRun);
									} else {
										COMMON_Sleep(DELAY_BEETWEN_SERIAL_READ_WRITE); //sleep to recieve some new data
									}
								} else {
									breakLoop = TRUE;
									break; //serial error
								}

								//fix reading times
								time(&tEnd);
								int tDiff = tEnd - tRun;
								if ((tDiff > TIMEOUT_READING_ON_RS485) || (tDiff < 0)){
									res = ERROR_TIME_IS_OUT;
									breakLoop = TRUE;
									break;
								}
							}
						}
						break;
					}
				}
				break;

				//read routine for OTHER counters
				default: //OTHER
				{
					time(&tRun);
					while (counter485Task->transactions[transactionIndex]->answerSize < counter485Task->transactions[transactionIndex]->waitSize) {

						//calc we need to read
						bytesRead = counter485Task->transactions[transactionIndex]->waitSize - counter485Task->transactions[transactionIndex]->answerSize;

						//read
						memset(bytes, 0, 255);
						res = SERIAL_Read(PORT_RS, bytes, bytesRead, &bytesRead);
						if (res == OK) {
							if (bytesRead > 0) {

								//debug
								COMMON_BUF_DEBUG(DEBUG_POOL_485 "485 RX", bytes, bytesRead);

								//realloc mem for amswer
								counter485Task->transactions[transactionIndex]->answerSize+=bytesRead;
								counter485Task->transactions[transactionIndex]->answer = (unsigned char *) realloc(counter485Task->transactions[transactionIndex]->answer, ( (counter485Task->transactions[transactionIndex]->answerSize)* sizeof (unsigned char)));

								//setup read byte
								memcpy(&counter485Task->transactions[transactionIndex]->answer[counter485Task->transactions[transactionIndex]->answerSize - bytesRead], bytes, bytesRead);

								//retime start time
								time(&tRun);
							} else {
								COMMON_Sleep(DELAY_BEETWEN_SERIAL_READ_WRITE); //sleep to recieve some new data
							}
						} else {
							breakLoop = TRUE;
							break; //serial error
						}

						//fix reading times
						time(&tEnd);
						int tDiff = tEnd - tRun;
						if ((tDiff > TIMEOUT_READING_ON_RS485) || (tDiff < 0)){
							res = ERROR_TIME_IS_OUT;
							breakLoop = TRUE;
							break;
						}
					}
				}
				break;
			}

			//error is time out
			if ((res == ERROR_TIME_IS_OUT) && (counter485Task->transactions[transactionIndex]->answerSize == 0)){
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL RS 485 READ: TIME OUT [0 bytes recieved]");
				counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_TIMEOUT;
				break; //exit for loop
			}

			//setup and check answer
			switch( counter485Task->counterType )
			{
				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_103:
				case TYPE_MAYK_MAYK_302:
					if (counter485Task->transactions[transactionIndex]->result != TRANSACTION_DONE_OK){
						res = ERROR_SIZE_READ;
						breakLoop = TRUE;
					}
					break;

				case TYPE_COUNTER_UNKNOWN:
				{
					//check answer size
					switch (unknownId){
						case UNKNOWN_ID_GUBANOV:
						case UNKNOWN_ID_UJUK:
							if (counter485Task->transactions[transactionIndex]->answerSize != counter485Task->transactions[transactionIndex]->waitSize){
								COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL RS 485 READ: TIME OUT [not all bytes recieved]");
								counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
								res = ERROR_SIZE_READ;
								breakLoop = TRUE;
							}
							break;
					}
				}
				break;


				case  TYPE_UJUK_SEB_1TM_01A:
				case  TYPE_UJUK_SEB_1TM_02A:
				case  TYPE_UJUK_SET_4TM_01A:
				case  TYPE_UJUK_SET_4TM_02A:
				case  TYPE_UJUK_SET_4TM_03A:
				case  TYPE_UJUK_PSH_3TM_05A:
				case  TYPE_UJUK_PSH_4TM_05A:
				case  TYPE_UJUK_SEO_1_16:
				case  TYPE_UJUK_PSH_4TM_05MB:
				case  TYPE_UJUK_PSH_4TM_05MD:
				case  TYPE_UJUK_PSH_3TM_05MB:
				case  TYPE_UJUK_SET_4TM_02MB:
				case  TYPE_UJUK_SEB_1TM_02D:
				case  TYPE_UJUK_PSH_3TM_05D:
				case  TYPE_UJUK_PSH_4TM_05D:
				case  TYPE_UJUK_PSH_4TM_05MK:
				case  TYPE_UJUK_SET_1M_01:
				case  TYPE_UJUK_SEB_1TM_02M:
				case TYPE_UJUK_SEB_1TM_03A:
				case  TYPE_UJUK_SET_4TM_03MB:
				{
					if (counter485Task->transactions[transactionIndex]->answerSize != counter485Task->transactions[transactionIndex]->waitSize)
					{
						COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL RS 485 READ: TIME OUT [not all bytes recieved]");
						counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
						res = ERROR_SIZE_READ;
						breakLoop = TRUE ;

						//check short answer possible flag
						if ( counter485Task->transactions[transactionIndex]->shortAnswerIsPossible == TRUE )
						{
							if ( counter485Task->transactions[transactionIndex]->answerSize > 0 )
							{
								unsigned int waitLength = 4 ;
								if (counter485Task->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
								{
									waitLength = 8 ;
								}

								if ( counter485Task->transactions[transactionIndex]->answerSize == waitLength )
								{
									//check answer crc
									unsigned int expectedCrc = UJUK_GetCRC( counter485Task->transactions[transactionIndex]->answer , waitLength - UJUK_SIZE_OF_CRC );
									expectedCrc &= 0xFFFF ;
									unsigned int realCrc = 0 ;
									memcpy( &realCrc , &counter485Task->transactions[transactionIndex]->answer[ waitLength - UJUK_SIZE_OF_CRC ]  , UJUK_SIZE_OF_CRC );

									if ( expectedCrc == realCrc )
									{
										res = OK ;
										breakLoop = FALSE ;
										counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
										counter485Task->transactions[transactionIndex]->shortAnswerFlag = TRUE ;
									}
								}
							}
						}
					}
					else
					{
						counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
					}
				}
				break;

				//gubanov
				case TYPE_GUBANOV_PSH_3TA07_DOT_1:
				case TYPE_GUBANOV_PSH_3TA07_DOT_2:
				case TYPE_GUBANOV_PSH_3TA07A:
				case TYPE_GUBANOV_SEB_2A05:
				case TYPE_GUBANOV_SEB_2A07:
				case TYPE_GUBANOV_SEB_2A08:
				case TYPE_GUBANOV_PSH_3ART:
				case TYPE_GUBANOV_PSH_3ART_1:
				case TYPE_GUBANOV_PSH_3ART_D:
				case TYPE_GUBANOV_MAYK_101:
				case TYPE_GUBANOV_MAYK_102:
				case TYPE_GUBANOV_MAYK_301:
				{
					//check result of size
					if (counter485Task->transactions[transactionIndex]->waitSize != counter485Task->transactions[transactionIndex]->answerSize)
					{
						res = ERROR_SIZE_READ;
						breakLoop = TRUE ;

						counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;

						//check short answer possible flag
						if ( counter485Task->transactions[transactionIndex]->shortAnswerIsPossible == TRUE )
						{
							if ( counter485Task->transactions[transactionIndex]->answerSize > 9 )
							{
								if ( (counter485Task->transactions[transactionIndex]->answer[0] == '~') &&
									 (counter485Task->transactions[transactionIndex]->answer[ counter485Task->transactions[transactionIndex]->answerSize - 1 ] == '\r') )
								{
									//check answer crc
									unsigned char expectedCrc[ GUBANOV_SIZE_OF_CRC ];
									memset( expectedCrc , 0 , GUBANOV_SIZE_OF_CRC );
									GUBANOV_GetCRC( counter485Task->transactions[transactionIndex]->answer , counter485Task->transactions[transactionIndex]->answerSize - GUBANOV_SIZE_OF_CRC - 1, &expectedCrc[ 0 ] );
									if ( !memcmp( &counter485Task->transactions[transactionIndex]->answer[ counter485Task->transactions[transactionIndex]->answerSize - GUBANOV_SIZE_OF_CRC - 1] , expectedCrc , GUBANOV_SIZE_OF_CRC) )
									{
										breakLoop = FALSE ;
										counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
										counter485Task->transactions[transactionIndex]->shortAnswerFlag = TRUE ;
									}
								}
							}
						}
					} else {
						counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
					}
				}
				break;

				default:
				{
					if (counter485Task->transactions[transactionIndex]->answerSize != counter485Task->transactions[transactionIndex]->waitSize){
						COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL RS 485 READ: TIME OUT [not all bytes recieved]");
						counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
						res = ERROR_SIZE_READ;
						breakLoop = TRUE ;
					}
				}
			}

			//set stop transaction time
			counter485Task->transactions[ transactionIndex ]->tStop = time( NULL );

			if ( breakLoop == TRUE )
			{

				break;
			}

			//setup transaction state ok and continue
			counter485Task->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;

			//sleep between transact next command
			COMMON_Sleep(50);


		} //end for



		//reset repeate flag if error was occured
		if (res != OK){
			STORAGE_ResetRepeatFlag(counter485Task->counterDbId);
			STORAGE_SetCounterStatistic(counter485Task->counterDbId, FALSE, counter485Task->transactions[transactionIndex]->transactionType, counter485Task->transactions[transactionIndex]->result);
		}


	}



	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Run485Task() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_Save485TaskResults() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	if (counter485Task != NULL) {
		//get task results
		switch (counter485Task->counterType) {
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SET_4TM_01A:
			case TYPE_UJUK_SET_4TM_02A:
			case TYPE_UJUK_SET_4TM_03A:
			case TYPE_UJUK_PSH_3TM_05A:
			case TYPE_UJUK_PSH_4TM_05A:
			case TYPE_UJUK_PSH_4TM_05MD:
			case TYPE_UJUK_SEO_1_16:
			case TYPE_UJUK_PSH_4TM_05MB:
			case TYPE_UJUK_PSH_3TM_05MB:
			case TYPE_UJUK_SET_4TM_02MB:
			case TYPE_UJUK_SEB_1TM_02D:
			case TYPE_UJUK_PSH_3TM_05D:
			case TYPE_UJUK_PSH_4TM_05D:
			case TYPE_UJUK_PSH_4TM_05MK:
			case TYPE_UJUK_SET_1M_01:
			case TYPE_UJUK_SEB_1TM_02M:
			case TYPE_UJUK_SEB_1TM_03A:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL 485: save task for ujuk counter");
				res = UJUK_SaveTaskResults(counter485Task);
				break;

			case TYPE_GUBANOV_PSH_3TA07_DOT_1:
			case TYPE_GUBANOV_PSH_3TA07_DOT_2:
			case TYPE_GUBANOV_PSH_3TA07A:
			case TYPE_GUBANOV_SEB_2A05:
			case TYPE_GUBANOV_SEB_2A07:
			case TYPE_GUBANOV_SEB_2A08:
			case TYPE_GUBANOV_PSH_3ART:
			case TYPE_GUBANOV_PSH_3ART_1:
			case TYPE_GUBANOV_PSH_3ART_D:
			case TYPE_GUBANOV_MAYK_301:
			case TYPE_GUBANOV_MAYK_102:
			case TYPE_GUBANOV_MAYK_101:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL 485: save task for gubanov counter");
				res = GUBANOV_SaveTaskResults(counter485Task);
				break;

			case TYPE_MAYK_MAYK_101:
			case TYPE_MAYK_MAYK_103:
			case TYPE_MAYK_MAYK_302:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL 485: save task for mayk counter");
				res = MAYK_SaveTaskResults(counter485Task);
				break;

			case TYPE_COUNTER_UNKNOWN:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL 485: create task for unknown counter");
				res = UNKNOWN_SaveTaskResults(counter485Task);
				break;

			default:
				COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL: error: undefined counter");
				res = ERROR_GENERAL; //error condition
				break;
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Save485TaskResults() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_Free485Task() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	if (counter485Task != NULL)
		res = COMMON_FreeCounterTask(&counter485Task);

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_Free485Task() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_ProcessCountersOnPlc(int * lastWasTimeouted) {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	res = POOL_CreatePlcTask();
	if (res == OK) {
		res = POOL_RunPlcTask();
		if (res == OK) {
			res = POOL_WaitPlcTaskComplete(lastWasTimeouted);
			if (res == OK) {
				res = POOL_SavePlcTaskResults();
			}
		} else {
			POOL_ResetBeforeFreePlcTask();
		}
	}
	POOL_BeforeFreePlcTask();
	POOL_FreePlcTask();

	if ((res == ERROR_REPEATER) || (res == ERROR_POOL_PLC_SHAMANING)){
		res = OK;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_ProcessCountersOnPlc() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_CreatePlcTask() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;
	counter_t * counter = NULL;
	
	//enter cs
	POOL_ProtectPlc();

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "\r\n\r\n----------------------------------\r\nPOOL PLC COUNTER\r\n----------------------------------");

	//try to get next counter
	res = STORAGE_GetNextCounterPlc(&counter);

	//check res and create counter task
	if ((res == OK) && (counter != NULL)) {

		//debug
		COMMON_STR_DEBUG(DEBUG_POOL_PLC "\r\n\r\nPOOL PLC: pool SN[%lu] via RS[%s]\r\n", counter->serial, counter->serialRemote);

		//check lastPollTime to do someshing shamaning on PLC net
		time_t tPollNow;
		time(&tPollNow);
		int tDiff = tPollNow - counter->lastPollTime;

		//if clock was resetup
		if (tDiff < 0){
			time (&counter->lastPollTime);
		}
		
		//Nalkin says: if we need shamaning - do it
		COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL UNSUCCESS TIME: " H_PATTERN, H_FROM_S(tDiff));

		//if unseccess polling time is too large
		if (tDiff > TIMEOUT_EMPTY_PLC_POLLS){
	
			COMMON_STR_DEBUG(DEBUG_POOL_PLC "PLC INDIVIDUAL LEAVE NEEDED: counter not answer during 12 hours");
			
			//setup shamaning with leave net
			if (counter->pollAlternativeAlgo == POOL_ALTER_NOT_SET){
				if ((counter->shamaningTryes % 2) == 0){
					tDiff = tPollNow - counter->lastLeaveCmd;
					if (tDiff > TIMEOUT_BETWEENS_LEAVES){
						counter->pollAlternativeAlgo = POOL_ALTER_NEED_00_LEAVE;
						counter->shamaningTryes++;
					}
				}else {
					counter->shamaningTryes++;
				}
			}	
		}

		//if we need leave net, do it
		if(counter->pollAlternativeAlgo != POOL_ALTER_NOT_SET){
			COMMON_STR_DEBUG(DEBUG_POOL_PLC "ALTERNATIVE POOL: call leave network");
			
			PLC_LeaveNetworkIndividual(counter);
			
			res = ERROR_POOL_PLC_SHAMANING;	 
		}
		
		
		//usual vanil pool
		if (res == OK){
		
			COMMON_STR_DEBUG(DEBUG_POOL_PLC "TASK CREATING WILL BE PROCESSED");
			
			counterTask_t * counterTask = NULL;
			switch (counter->type) {
				case TYPE_UJUK_SEB_1TM_01A:
				case TYPE_UJUK_SEB_1TM_02A:
				case TYPE_UJUK_SET_4TM_01A:
				case TYPE_UJUK_SET_4TM_02A:
				case TYPE_UJUK_SET_4TM_03A:
				case TYPE_UJUK_PSH_3TM_05A:
				case TYPE_UJUK_PSH_4TM_05MD:
				case TYPE_UJUK_PSH_4TM_05A:
				case TYPE_UJUK_SEO_1_16:
				case TYPE_UJUK_PSH_4TM_05MB:
				case TYPE_UJUK_PSH_3TM_05MB:
				case TYPE_UJUK_SET_4TM_02MB:
				case TYPE_UJUK_SEB_1TM_02D:
				case TYPE_UJUK_PSH_3TM_05D:
				case TYPE_UJUK_PSH_4TM_05D:
				case TYPE_UJUK_PSH_4TM_05MK:
				case TYPE_UJUK_SET_1M_01:
				case TYPE_UJUK_SEB_1TM_02M:
				case TYPE_UJUK_SEB_1TM_03A:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: create task for ujuk counter");
					res = UJUK_CreateCounterTask(counter, &counterTask);
					break;

				case TYPE_GUBANOV_PSH_3TA07_DOT_1:
				case TYPE_GUBANOV_PSH_3TA07_DOT_2:
				case TYPE_GUBANOV_PSH_3TA07A:
				case TYPE_GUBANOV_SEB_2A05:
				case TYPE_GUBANOV_SEB_2A07:
				case TYPE_GUBANOV_SEB_2A08:
				case TYPE_GUBANOV_PSH_3ART:
				case TYPE_GUBANOV_PSH_3ART_1:
				case TYPE_GUBANOV_PSH_3ART_D:
				case TYPE_GUBANOV_MAYK_301:
				case TYPE_GUBANOV_MAYK_102:
				case TYPE_GUBANOV_MAYK_101:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: create task for gubanov counter");
					res = GUBANOV_CreateCounterTask(counter, &counterTask);
					break;

				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_103:
				case TYPE_MAYK_MAYK_302:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: create task for mayk counter");
					res = MAYK_CreateCounterTask(counter , &counterTask);
					break;

				case TYPE_COUNTER_UNKNOWN:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: create task for unknown counter");
					res = UNKNOWN_CreateCounterTask(counter , &counterTask);
					break;

				case TYPE_COUNTER_REPEATER:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: skip task for plc repeater");
					res = ERROR_REPEATER ;
					break;

				default:
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL PLC: error: undefined counter");
					res = ERROR_GENERAL; //error condition
					break;
			}

			if (res == OK) {
				res = PLC_AddCounterTask(&counterTask);
				if (res == OK){
					STORAGE_SetCounterStatistic(counter->dbId, TRUE, 0, 0);
				}
			}
		}
	}



	//leave cs
	POOL_UnprotectPlc();

	return res;
}

//
//
//

int POOL_RunPlcTask() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	res = PLC_RunTask();

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_RunPlcTask() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_WaitPlcTaskComplete(int * lastWasTimeouted) {
	DEB_TRACE(DEB_IDX_POOL)

	int res = OK;
	BOOL waitResult = FALSE;

	do {

		//sleep
		COMMON_Sleep(DELAY_BEETWEN_CHECK_TASK_STATE);

		//get task result
		waitResult = PLC_IsTaskComplete(lastWasTimeouted);

	} while (!waitResult);

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_WaitPLCTaskComplete() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_SavePlcTaskResults() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;
	int taskIndex = 0;
	counterTask_t * counterTask = NULL;

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_SavePlcTaskResults");

	for (; taskIndex < PLC_GetCounterTasksCount(); taskIndex++) {
		res = PLC_GetCounterTaskByIndex(taskIndex, &counterTask);
		if (res == OK) {
			switch (counterTask->counterType) {
				case TYPE_UJUK_SEB_1TM_01A:
				case TYPE_UJUK_SEB_1TM_02A:
				case TYPE_UJUK_SET_4TM_01A:
				case TYPE_UJUK_SET_4TM_02A:
				case TYPE_UJUK_SET_4TM_03A:
				case TYPE_UJUK_PSH_4TM_05MD:
				case TYPE_UJUK_PSH_3TM_05A:
				case TYPE_UJUK_PSH_4TM_05A:
				case TYPE_UJUK_SEO_1_16:
				case TYPE_UJUK_PSH_4TM_05MB:
				case TYPE_UJUK_PSH_3TM_05MB:
				case TYPE_UJUK_SET_4TM_02MB:
				case TYPE_UJUK_SEB_1TM_02D:
				case TYPE_UJUK_PSH_3TM_05D:
				case TYPE_UJUK_PSH_4TM_05D:
				case TYPE_UJUK_PSH_4TM_05MK:
				case TYPE_UJUK_SET_1M_01:
				case TYPE_UJUK_SEB_1TM_02M:
				case TYPE_UJUK_SEB_1TM_03A:
					res = UJUK_SaveTaskResults(counterTask);
					break;

				case TYPE_GUBANOV_PSH_3TA07_DOT_1:
				case TYPE_GUBANOV_PSH_3TA07_DOT_2:
				case TYPE_GUBANOV_PSH_3TA07A:
				case TYPE_GUBANOV_SEB_2A05:
				case TYPE_GUBANOV_SEB_2A07:
				case TYPE_GUBANOV_SEB_2A08:
				case TYPE_GUBANOV_PSH_3ART:
				case TYPE_GUBANOV_PSH_3ART_1:
				case TYPE_GUBANOV_PSH_3ART_D:
				case TYPE_GUBANOV_MAYK_301:
				case TYPE_GUBANOV_MAYK_102:
				case TYPE_GUBANOV_MAYK_101:
					res = GUBANOV_SaveTaskResults(counterTask);
					break;

				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_103:
				case TYPE_MAYK_MAYK_302:
					res = MAYK_SaveTaskResults(counterTask);
					break;

				case TYPE_COUNTER_UNKNOWN:
					res = UNKNOWN_SaveTaskResults(counterTask);
					break;

				default:
					//to do
					break;
			}
		}
	}


	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_SavePlcTaskResults() %s", getErrorCode(res));

	return res;
}

//
//
//

int POOL_BeforeFreePlcTask(){
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_NO_COUNTERS;
	int taskIndex = 0;
	counterTask_t * counterTask = NULL;

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_BeforeFreePlcTask");

	res = PLC_GetCounterTaskByIndex(taskIndex, &counterTask);
	if ((res == OK) && (counterTask != NULL)) {
		int res2 = OK;
		int timeSpendForTask = 0;

		dateTimeStamp lastRunDts;
		dateTimeStamp currentDts;

		memset(&lastRunDts, 0, sizeof(dateTimeStamp));
		memset(&currentDts, 0, sizeof(dateTimeStamp));

		COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL Calc time spended for task");

		res2 = STORAGE_GetStatisticStartDts(counterTask->counterDbId, &lastRunDts);
		if (res2 == OK){
			res2 = COMMON_CheckDtsForValid(&lastRunDts);
			if (res2 == OK){
				res2 = COMMON_GetCurrentDts(&currentDts);
				if (res2 == OK){
					timeSpendForTask = COMMON_GetDtsDiff__Alt(&lastRunDts, &currentDts, BY_SEC);
					timeSpendForTask = STORAGE_GetMaxPossibleTaskTimeForPLC() - timeSpendForTask;
					COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL spend time %d sec", timeSpendForTask);
				}
			}
		}

		appendTimeout += timeSpendForTask;

		if (appendTimeout < 0){
			appendTimeout = 0;
		}

		if (appendTimeout > MAX_APPEND_TIMEOUT){
			appendTimeout = MAX_APPEND_TIMEOUT;
		}

		COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL append timeout %d sec", appendTimeout);

	}


	return res;
}

//
//
//

int POOL_ResetBeforeFreePlcTask(){
	DEB_TRACE(DEB_IDX_POOL)

	int res = OK;
	int taskIndex = 0;
	counterTask_t * counterTask = NULL;

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_BeforeBeforeFreePlcTask");

	for (; taskIndex < PLC_GetCounterTasksCount(); taskIndex++) {
		res = PLC_GetCounterTaskByIndex(taskIndex, &counterTask);
		if (res == OK) {
			STORAGE_ResetRepeatFlag(counterTask->counterDbId);
		}
	}

	return res;
}

//
//
//

int POOL_FreePlcTask() {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	res = PLC_FreeTask();

	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_FreePlcTask() %s", getErrorCode(res));

	return res;
}


int POOL_ErrorHandle(int errorCode, int poolId) {
	DEB_TRACE(DEB_IDX_POOL)

	int res = ERROR_GENERAL;

	int countOfRfCounters = 0;
	int countOf485Counters = 0;

	switch (errorCode) {
		case ERROR_NO_COUNTERS:
			countOfRfCounters = STORAGE_CountOfCountersPlc();
			countOf485Counters = STORAGE_CountOfCounters485();
			if ((countOfRfCounters + countOf485Counters) == 0){
				CLI_SetErrorCode(CLI_NO_COUNTERS, CLI_SET_ERROR);
			}
			res = OK;
			COMMON_Sleep(SECOND);
			break;

		case ERROR_TIME_IS_OUT:
			//DO NOTHING
			break;

		default:
			if (poolId == POOL_ID_485){
				CLI_SetErrorCode(CLI_ERROR_POOL_485_TR, CLI_SET_ERROR);
			} else {
				CLI_SetErrorCode(CLI_ERROR_POOL_PLC_TR, CLI_SET_ERROR);
			}
			break;
	}

	return res;

}

//
//
//

int POOL_TurnTransparentOn(unsigned long counterDbId){
	DEB_TRACE(DEB_IDX_POOL);

	COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL POOL_TurnTransparentOn for dbid [%lu]" , counterDbId );

	unsigned char evDesc[EVENT_DESC_SIZE];
	unsigned char dbidDesc[EVENT_DESC_SIZE];
	memset(dbidDesc , 0, EVENT_DESC_SIZE);
	COMMON_TranslateLongToChar(counterDbId, dbidDesc);

	if ((poolTransparentPlc == TRUE) || (poolTransparentRs485 == TRUE)){

		COMMON_CharArrayDisjunction((char *)dbidDesc, DESC_EVENT_TRANSPARENT_OPEN_ERR, evDesc);

		STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_ENABLE , evDesc);

		COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent alerady open");
		return ERROR_GENERAL;
	}

	int res = OK;
	counter_t * counter = NULL;
	time_t tRun;
	time_t tEnd;

	res = STORAGE_FoundCounter(counterDbId, &counter);
	if ((res == OK) && (counter != NULL)){

		COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent counter was find");

		//reset flag
		poolTransparentInLoop = FALSE;

		//if its plc, start plc, otherwise start 485
		if(strlen((char *)counter->serialRemote) > 0){

			COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent counter on plc");

			memset(poolPlcSerialTransparent, 0, SIZE_OF_SERIAL+1);
			memcpy(poolPlcSerialTransparent, counter->serialRemote, SIZE_OF_SERIAL);
			poolTransparentPlc = TRUE;

			PLC_BreakTask();

		} else {

			COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent counter on 485");

			SERIAL_SetupAttributesRs485( &counter->serial485Attributes );

			poolTransparentRs485 = TRUE;
		}

		time(&tRun);
		res = OK;

		COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent go to waiting plc or 485 loop done");

		//wait till current task will be done and poller goes to transparent loop
		while (poolTransparentInLoop == FALSE){
			COMMON_Sleep (SECOND);

			time(&tEnd);
			int tDiff = tEnd - tRun;
			if ((tDiff > TIMEOUT_TRANSPARENT_OPEN) || (tDiff<0)){
				res = ERROR_TIME_IS_OUT;
				break;
			}
		}

		COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent waiting end, wait result %s", getErrorCode(res));

		//check waiting result
		if (res == OK){
			COMMON_BUF_DEBUG( DEBUG_POOL_PLC "DESC_EVENT_TRANSPARENT_OPEN_ERR description before disjunction" , evDesc , EVENT_DESC_SIZE );
			COMMON_CharArrayDisjunction((char *)dbidDesc, DESC_EVENT_TRANSPARENT_OPEN_OK, evDesc);
			COMMON_BUF_DEBUG( DEBUG_POOL_PLC "DESC_EVENT_TRANSPARENT_OPEN_ERR description after disjunction" , evDesc , EVENT_DESC_SIZE );
			STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_ENABLE , evDesc);

			time(&transparentStart);
			transparentDbId = counterDbId;
			transparentInWaitAnswer = 0;

		} else {

			COMMON_CharArrayDisjunction((char *)dbidDesc, DESC_EVENT_TRANSPARENT_OPEN_ERR, evDesc);
			STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_ENABLE , evDesc);

			POOL_ResetTransparent();
		}

	} else {

		COMMON_STR_DEBUG( DEBUG_POOL_PLC "POOL transparent counter not find");

		res = ERROR_IS_NOT_FOUND;
	}

	return res;
}

//
//
//

void POOL_TurnTransparentOff(){
	DEB_TRACE(DEB_IDX_POOL)

	STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_DISABLE , (unsigned char *)DESC_EVENT_TRANSPARENT_CLOSE_PROTO);

	POOL_ResetTransparent();
}

//
//
//

void POOL_TurnTransparentOffConnDie(){
	DEB_TRACE(DEB_IDX_POOL)

	STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_DISABLE , (unsigned char *)DESC_EVENT_TRANSPARENT_CLOSE_CONNECT);

	POOL_ResetTransparent();
}

//
//
//

void POOL_TurnTransparentOffByTimeout(){
	DEB_TRACE(DEB_IDX_POOL)

	STORAGE_JournalUspdEvent(EVENT_TRANSPARENT_MODE_DISABLE , (unsigned char *)DESC_EVENT_TRANSPARENT_CLOSE_TIMEOUT);

	POOL_ResetTransparent();
}

//
//
//

void POOL_ResetTransparent(){
	transparentInWaitAnswer = 0;
	transparentDbId = 0xFFFFFFFF;
	poolTransparentInLoop = FALSE;
	poolTransparentPlc = FALSE;
	poolTransparentRs485 = FALSE;
}

//
//
//

void POOL_TrasparentCheckTime(){
	time_t currentTime;

	time(&currentTime);
	int tDiff = currentTime - transparentStart;
	if ((tDiff > TIMEOUT_TRANSPARENT_MODE) || (tDiff < 0)){
		POOL_TurnTransparentOffByTimeout();
	}
}

//
//
//

BOOL POOL_GetTransparent(unsigned long * dbid){
	DEB_TRACE(DEB_IDX_POOL)

	BOOL res =  FALSE;

	res = (poolTransparentPlc || poolTransparentRs485);
	if (res == TRUE){
		(*dbid) = transparentDbId;
	}

	return res;
}

//
//
//

BOOL POOL_GetTransparentPlc(){
	return poolTransparentPlc;
}

//
//
//

BOOL POOL_GetTransparent485(){
	return poolTransparentRs485;
}

//
//
//
BOOL POOL_InTransparent(){
	return (POOL_GetTransparentPlc() || POOL_GetTransparent485());
}

//
//
//

int POOL_WriteTransparent(unsigned char * sendBuf, int size){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_WriteTransparent()");

	//if during wait of answer write is occured
	if (transparentInWaitAnswer == 1){
		return ERROR_GENERAL;
	}

	//check timeouted transparent
	POOL_TrasparentCheckTime();

	//check transparent was closed previously
	if ((poolTransparentPlc == FALSE) && (poolTransparentRs485 == FALSE)){
		return ERROR_GENERAL;
	}

	//update transparent data
	time(&transparentStart);

	//write depending transparent
	if (poolTransparentPlc == TRUE){
		POOL_WriteTransparentPlc(sendBuf, size);

	} else if (poolTransparentRs485 == TRUE){
		POOL_WriteTransparentRs485(sendBuf, size);
	}

	transparentInWaitAnswer = 1;

	return OK;
}

//
//
//
void POOL_ReadTransparentRs485(unsigned char ** recvBuf, int * size){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_ReadTransparentRs485()");

	int res;
	int readyToRead = 0 ;
	int readSize = 0;

	(*size) = 0 ;

	time_t tRunAll;
	time_t tEndAll;
	time_t tRunPart;
	time_t tEndPart;

	time(&tRunAll);

	do{
		SERIAL_Check(PORT_RS , &readyToRead);
		if (readyToRead > 0)
		{
			(*recvBuf) = realloc ((*recvBuf), readSize+readyToRead);
			if ((*recvBuf) != NULL)
			{
				int readed = 0 ;
				res = SERIAL_Read(PORT_RS, &(*recvBuf)[readSize], readyToRead, &readed);
				readSize+=readed;
				(*size) += readed ;
				time(&tRunPart);
			}

			COMMON_BUF_DEBUG(DEBUG_POOL_485 "TRANSPARENT (485) RX ", (*recvBuf), readSize);
		}

		if (readSize > 0){
			//insert delay of 1 second for more readings of segmentated answer in next loops
			time(&tEndPart);
			int tDiff = tEndPart - tRunPart;
			if ((tDiff > TIMEOUT_TRANSPARENT_RS) || (tDiff < 0)){
				break;
			}
		}

		//calc total wait time (approx 20 seconds)
		time(&tEndAll);
		int tDiff = tEndAll - tRunAll;
		if ((tDiff > TIMEOUT_TRANSPARENT_TIME) || (tDiff < 0) ){
			break;
		}

		//check timeouted transparent
		POOL_TrasparentCheckTime();
		if (poolTransparentRs485 == FALSE)
			break;

	} while (1);

	//reset transparent wait answer flag
	transparentInWaitAnswer = 0;

	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_ReadTransparentRs485() read %d bytes", readSize);
}

//
//
//
void POOL_WriteTransparentRs485(unsigned char * sendBuf, int size){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_WriteTransparentRs485()");
	COMMON_BUF_DEBUG(DEBUG_POOL_485 "TRANSPARENT (485) TX ", sendBuf, size);

	int res;
	int totalWrite = 0;

	res = SERIAL_Write(PORT_RS, sendBuf, size, &totalWrite);

	COMMON_STR_DEBUG(DEBUG_POOL_485 "POOL_WriteTransparentRs485() %s write %d bytes", getErrorCode(res), totalWrite);
}

//
//
//
void POOL_ReadTransparentPlc(){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_ReadTransparentPlc() enter");

	time_t tRunAll;
	time_t tEndAll;
	int ready = 0;

	time(&tRunAll);
	do
	{
		POOL_ProtectPlcTrBuffer();
		ready = plcTransparentReady;
		POOL_UnprotectPlcTrBuffer();

		//check data is ready coollected from plc
		if ( ready == 1){
			COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_ReadTransparentPlc() answer catched");
			break;

		} else {

			COMMON_Sleep (DELAY_BEETWEN_CHECK_TASK_STATE);
		}


		//calc total wait time (approx 20 seconds)
		time(&tEndAll);
		int tDiff = tEndAll - tRunAll;
		if ((tDiff > TIMEOUT_TRANSPARENT_TIME) || (tDiff < 0)){
			COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_ReadTransparentPlc() timeout");
			break;
		}

		//check timeouted transparent
		POOL_TrasparentCheckTime();
		if (poolTransparentPlc == FALSE)
			break;

	} while (1);

	//reset transparent wait answer flag
	transparentInWaitAnswer = 0;

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_ReadTransparentPlc() exit");
}

//
//
//

void POOL_SetTransparentPlcAnswer(unsigned long remoteId, unsigned char * plcPayload, unsigned int plcPayloadSize){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_SetTransparentPlcAnswer()");
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "recv plc packet from %u", remoteId);
	COMMON_BUF_DEBUG(DEBUG_POOL_PLC "recv plc packet ", plcPayload, plcPayloadSize);

	POOL_ProtectPlcTrBuffer();

	plcTrBuf = (unsigned char *)realloc(plcTrBuf, plcTrBufSz + plcPayloadSize);
	if (plcTrBuf != NULL){
		memcpy(&plcTrBuf[plcTrBufSz], plcPayload, plcPayloadSize);
		plcTrBufSz+=plcPayloadSize;

		if (plcTrBuf[0] == 0x7E){
			if ((plcTrBuf[plcTrBufSz-1] == 0x7E) || (plcTrBuf[plcTrBufSz-1] == '\r')){
				//mayak packet, completed, direct out to top level
				plcTransparentReady = 1;

			} else {
				//mayak packet, not full, need wait more
				//nop
			}

		} else {
			//ujuk packet
			//direct out to top level
			plcTransparentReady = 1;
		}
	}

	POOL_UnprotectPlcTrBuffer();
}

//
//
//
void POOL_WriteTransparentPlc(unsigned char * sendBuf, int size){
	DEB_TRACE(DEB_IDX_POOL)

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_WriteTransparentPlc()");
	COMMON_BUF_DEBUG(DEBUG_POOL_PLC "TRANSPARENT (PLC) TX ", sendBuf, size);

	int res;
	int assignedTag;

	//prepear buffer
	POOL_ProtectPlcTrBuffer();
	plcTransparentReady = 0;
	if ((plcTrBuf != NULL) && (plcTrBufSz > 0)){
		free (plcTrBuf);
		plcTrBuf = NULL;
		plcTrBufSz = 0;
	}
	POOL_UnprotectPlcTrBuffer();

	#if PLC_TRANSPARENT_MODE_USE_SHORT_INTERNETWORKING_SERVICE
		res = PLC_SendToRemote(poolPlcSerialTransparent, sendBuf, size, &assignedTag);

	#else
		res = PLC_SendToRemote2(poolPlcSerialTransparent, sendBuf, size, 0, 0x52, &assignedTag);
		res = PLC_SendToRemote2(poolPlcSerialTransparent, sendBuf, size, 0, 0x57, &assignedTag);
		res = PLC_SendToRemote2(poolPlcSerialTransparent, sendBuf, size, 0, 0x78, &assignedTag);

	#endif

	COMMON_STR_DEBUG(DEBUG_POOL_PLC "POOL_WriteTransparentPlc() %s [write %d bytes]", getErrorCode(res),  size);
}

//EOF

