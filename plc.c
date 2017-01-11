/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "ujuk.h"
#include "ujuk_crc.h"
#include "gubanov.h"
#include "boolType.h"
#include "common.h"
#include "debug.h"
#include "serial.h"
#include "storage.h"
#include "plc.h"
#include "cli.h"
#include "pio.h"
#include "pool.h"

#if WERY_BAD_MAYK_ANSWER
extern void MAYK_Gniffuts( unsigned char ** cmd , unsigned int * length );
extern int MAYK_RemoveQuotes( unsigned char ** cmd , unsigned int * length );
#endif

//local variables
BOOL plcThreadOk = FALSE;
volatile BOOL plcExecution = FALSE;
volatile BOOL plcExecutionSuspend = FALSE;
volatile int plcBreakFlag = 0;

//error counters
int plcErrorsCounter = 0;
int plcMaxErrors = MAX_PLC_ERROR_COUNT;
int plcReinitErrors = 0;

//proto reader state variable
int plcReaderState = PROTOCOL_PLC_CA_BYTE;

//plc base object
plcBaseStation_t plcBase;

//plc reader thread
pthread_t plcThread;
static pthread_mutex_t plcMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t plcNodesMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t plcTaskMux = PTHREAD_MUTEX_INITIALIZER;

//tag counter
int plcPacTag = 0;

//table first reads
BOOL snTableFullRead = FALSE;

			
//thread func
void * PLC_Do(void * param);

//
//
//
void PLC_ErrorIncrement(){
	plcErrorsCounter++;
	COMMON_STR_DEBUG(DEBUG_PLC "PLC PLC_ErrorIncrement(). plcErrorsCounter = [%d]/[%d]" , plcErrorsCounter, plcMaxErrors);
	if (plcErrorsCounter > plcMaxErrors){
		COMMON_STR_DEBUG(DEBUG_PLC "plcMaxErrors reached, setup plcBase.needToReset flag");
		STORAGE_EVENT_PLC_DEVICE_ERROR(0x2); //too much plc errors
		plcBase.needToReset = TRUE;
	}
}

//
//
//
void PLC_ErrorReset(){
	plcErrorsCounter = 0;
}

//
//
//
void PLC_Protect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc protect in");
	pthread_mutex_lock(&plcMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc protect out");
}

//
//
//
void PLC_Unprotect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc unprotect in");
	pthread_mutex_unlock(&plcMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc unprotect out");
}

//
//
//
void PLC_NodesTableProtect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc nodes protect in");
	pthread_mutex_lock(&plcNodesMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc nodes protect out");
}

//
//
//
void PLC_NodesTableUnprotect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc nodes unprotect in");
	pthread_mutex_unlock(&plcNodesMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc nodes unprotect out");
}

//
//
//
void PLC_TaskProtect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc task protect in");
	pthread_mutex_lock(&plcTaskMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc task protect out");
}

//
//
//
void PLC_TaskUnprotect()
{
	DEB_TRACE(DEB_IDX_PLC)

	//COMMON_STR_DEBUG(DEBUG_PLC "plc task unprotect in");
	pthread_mutex_unlock(&plcTaskMux);
	//COMMON_STR_DEBUG(DEBUG_PLC "plc task unprotect out");
}

//
//
//
void PLC_Stop(){
	DEB_TRACE(DEB_IDX_PLC)

	plcExecution = FALSE;
}

//
//
//
void PLC_Start(){
	DEB_TRACE(DEB_IDX_PLC)

	plcExecution = TRUE;
}

//
//
//
BOOL PLC_IsWork(void){
	DEB_TRACE(DEB_IDX_PLC)

	return (plcExecution && plcThreadOk);
}

//
//
//
BOOL PLC_IsRun(void){
	DEB_TRACE(DEB_IDX_PLC)

	 return plcThreadOk;
}

//
//
//
BOOL PLC_IsStop(void){
	DEB_TRACE(DEB_IDX_PLC)

	return ((plcExecution==FALSE) && (plcThreadOk==FALSE));
}

//
//
//
BOOL PLC_IsPaused(void){
	DEB_TRACE(DEB_IDX_PLC)

	return ((plcExecution==FALSE) && (plcExecutionSuspend==TRUE));
}

//
//
//
int PLC_Initialize(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	int plcInitCount = 0;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_Initialize");

	//setup NULL to plcBase.nodes pointer
	plcBase.nodes = NULL;

	//setpo exec flag
	plcExecution = TRUE;

	//init thread
	pthread_create(&plcThread, NULL, PLC_Do, 0);

	//wait plc thread creation comlete
	do{
		plcInitCount++;
		COMMON_Sleep(DELAY_PLC_INIT);
	}
	while ((!PLC_IsRun()) && (plcInitCount < MAX_COUNT_PLC_INIT_WAIT));

	//check thread ok
	if (PLC_IsWork()){
		res = OK;
		COMMON_STR_DEBUG(DEBUG_PLC "PLC thread is executed");
	}

	//initialize base
	if (res == OK){
		res = PLC_InitializePlcBase();
	}

	//set cli error
	if (res == OK){
		CLI_SetErrorCode(CLI_ERROR_INIT_PIM, CLI_CLEAR_ERROR);

	} else {
		CLI_SetErrorCode(CLI_ERROR_INIT_PIM, CLI_SET_ERROR);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_Initialize %s", getErrorCode(res));

	return res;
}

//
//
//
int PLC_InitializePlcBase(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	int initTries = 0;
	unsigned long uspdSnI = 0;
	it700params_t plcParams;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_InitializePlcBase");

	//get uspd sn as unsigned long value
	COMMON_GET_USPD_SN_UL(&uspdSnI);

	//get mode, sizes, node key
	STORAGE_GetPlcSettings(&plcParams);

	//clear base settings
	PLC_Protect();

	plcBase.networkId = 0;

	//set current uspd sn (without start 'U' character) as network id
	plcBase.networkIdForced = uspdSnI % 1000;

	//reset flags of erset
	plcBase.externalReset = FALSE;

	//free nodes if its not null
	if (plcBase.nodes != NULL){
		free (plcBase.nodes);
	}

	//set some variables
	plcBase.nodes = NULL;
	plcBase.nodesCount = 0;

	//copy settings
	memcpy(&plcBase.settings, &plcParams, sizeof(it700params_t));

	//set max errors count
	plcMaxErrors = plcBase.settings.netSizePhisycal * 3;

	PLC_Unprotect();

	COMMON_STR_DEBUG(DEBUG_PLC "IT700 PH SZ %d", plcBase.settings.netSizePhisycal);
	COMMON_STR_DEBUG(DEBUG_PLC "IT700 LG SZ %d", plcBase.settings.netSizeLogical);
	COMMON_STR_DEBUG(DEBUG_PLC "IT700 AMODE %d", plcBase.settings.admissionMode);
	COMMON_STR_DEBUG(DEBUG_PLC "IT700 TRY TO RESET");

	//set execution is ok
	plcExecution = TRUE;

	//go to reset-init loop
	while (initTries < MAX_PLC_INIT_TRIES){

		//flush uart buffers
		SERIAL_PLC_FLUSH();

		//enable uart buffer and reset IT700 PLC PIM
		res = PIO_SetValue(PIO_PLC_RESET, 0);
		COMMON_Sleep(50);

		res = PIO_SetValue(PIO_PLC_NOT_OE, 0);
		COMMON_Sleep(100);

		res = PIO_SetValue(PIO_PLC_RESET, 1);
		COMMON_Sleep(50);

		//wait reset answer
		if (res == OK){
			res = IT700_WaitAnswer(0x20, 0x7);
		}
		//configure plc base
		if (res == OK){

			COMMON_STR_DEBUG(DEBUG_PLC "IT700 TRY TO CONFIG");

			PLC_Protect();
			plcBase.externalReset = FALSE;
			PLC_Unprotect();
			res = IT700_SetConfig();
		}

		//init
		if (res == OK){
			COMMON_STR_DEBUG(DEBUG_PLC "IT700 TRY TO INIT BS");
			res = IT700_Init();
		}

		//go online
		if (res == OK){
			COMMON_STR_DEBUG(DEBUG_PLC "IT700 TRY TO GO ONLINE");
			res = IT700_GoOnline();
		}

		if (res == OK){
			PLC_Protect();
			plcBase.needToReset = FALSE;
			PLC_Unprotect();
			plcErrorsCounter = 0;
			break;

		} else {
			initTries++;
			res = ERROR_PLC;
			COMMON_Sleep(DELAY_PLC_INIT);
		}
	}

	//save plc start event to journal
	if (res == OK){
		STORAGE_EVENT_PLC_DEVICE_START(0x0);
		plcReinitErrors = 0;

	} else {
		STORAGE_EVENT_PLC_DEVICE_ERROR(0x0);
		plcReinitErrors++;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_InitializePlcBase %s", getErrorCode(res));

	return res;
}

//
//
//
void PLC_TerminatePlcBase(){
	DEB_TRACE(DEB_IDX_PLC)

	//delete plc nodes
	if ((plcBase.nodesCount > 0) && (plcBase.nodes != NULL)){
		free (plcBase.nodes);
		plcBase.nodes = NULL;
		plcBase.nodesCount = 0;
	}
}

//
//
//
unsigned char IT700_GetCrc(unsigned char * buf, int size){
	DEB_TRACE(DEB_IDX_PLC)

	int bIndex = 1;
	unsigned char crc = 0;
	for (; bIndex<size; bIndex++){
		crc+=buf[bIndex];
	}

	return crc;
}

//
//
//
int IT700_WaitAnswer(unsigned char opCode, unsigned char responce){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_TIME_IS_OUT;
	int waitLoop = 1;
	time_t tRun, tEnd;
	unsigned char gettedResp = 0x0;

	COMMON_STR_DEBUG(DEBUG_PLC "IT700_WaitAnswer");

	time(&tRun);
	do{
		switch (opCode){

			case 0x41: //configure steps
			case 0x42:
			case 0x43:
				if (responce == plcBase.paramResponce){
					waitLoop = 0;
					res = OK;
				} else {
					gettedResp = plcBase.paramResponce;
				}
				break;

			case 0x63: // sn table
			case 0x65:
			case 0xA2:
			case 0xA3:
				if (responce == plcBase.snTableResponce){
					waitLoop = 0;
					res = OK;
				} else {
					gettedResp = plcBase.snTableResponce;
				}
				break;

			case 0x20: // reset
				if (plcBase.externalReset == TRUE){
					waitLoop = 0;
					if ((plcBase.stackResponce == 0x7) || (plcBase.stackResponce == 0x40)){
						res = OK;

					} else {
						gettedResp = plcBase.stackResponce;
						res = ERROR_PLC;
					}
				}
				break;

			case 0x21: //init
			case 0x22: // go online
				if (responce == plcBase.stackResponce){
					waitLoop = 0;
					res = OK;
				} else {
					gettedResp = plcBase.stackResponce;
				}
				break;

			case 0x60: //responces
				if (responce == 0x1){
					if (plcBase.responce1 != 0xFF){
						waitLoop = 0;
						res = OK;
					} else {
						gettedResp = plcBase.responce1;
					}
				}
				if (responce == 0x2){
					if (plcBase.responce2 != 0xFF){
						waitLoop = 0;
						res = OK;
					} else {
						gettedResp = plcBase.responce2;
					}
				}
				break;

			case 0x68: //answer
				break;

			default:
				break;
		}

		if (res == OK){
			waitLoop = 0;

		} else {

			time(&tEnd);
			int tDiff = tEnd - tRun;
			if (tDiff > TIMEOUT_READING_ON_PLC){
				waitLoop = 0;
				res = ERROR_TIME_IS_OUT;
			}
		}

		COMMON_Sleep(DELAY_BEETWEN_CHECK_TASK_STATE);

	} while (waitLoop);

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_WaitAnswer: opcode [0x%02x] responce [0x%02x], recv [0x%02x] %s", opCode, responce, gettedResp, getErrorCode(res));

	return res;
}

//
//
//
int IT700_WaitAnswerInt(unsigned char opCode, unsigned char responce, int * value){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	plcBase.paramResponce = 0xFF;
	res = IT700_WaitAnswer(opCode, responce);
	if (res == OK){
		*value = plcBase.paramValueI;
	}

	return res;
}

//
//
//
int IT700_WaitAnswerVariable(unsigned char opCode, unsigned char responce, unsigned char * value, int waitSize){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	memset(plcBase.paramValueV, 0, 32);
	plcBase.paramResponce = 0xFF;
	res = IT700_WaitAnswer(opCode, responce);
	if (res == OK){
		memcpy(value, plcBase.paramValueV, waitSize);
	}

	return res;
}

//
//
//
int IT700_WaitAnswerNode(unsigned char opCode, unsigned char responce, plcNode_t ** node){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	char tempSn[SIZE_OF_SERIAL+1];

	plcBase.snTableResponce = 0xFF;
	res = IT700_WaitAnswer(opCode, responce);
	if (res == OK){
		switch (opCode){
			case 0x63:
				(*node)->pid = plcBase.entryNodePid;
				(*node)->did = plcBase.entryNodeDid;
				(*node)->age = plcBase.entryNodeAge;
				break;

			case 0xA3:
				memset(tempSn, 0, SIZE_OF_SERIAL+1);
				sprintf(tempSn, "%lu", plcBase.entryNodeSn);
				memset((*node)->serial, 0, SIZE_OF_SERIAL+1);
				memcpy((*node)->serial, tempSn, SIZE_OF_SERIAL);
				break;
		}
	}

	return res;
}

//
//
//
int IT700_GetTableEntry(int idx, plcNode_t ** node){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "\r\n----------------------------------\r\nIT700_GetTableEntry index [%04u]\r\n----------------------------------", idx);

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x05;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x63;
	plcOutput[5] = 0x01;
	plcOutput[6] = (idx & 0xFF);
	plcOutput[7] = ((idx >> 8) & 0xFF);
	plcOutputLen = 8;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		res = IT700_WaitAnswerNode(0x63, 0x1, node);
	}

	//check result
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetTableEntry %s", getErrorCode(res));
		return res;
	}

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x04;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0xA3;
	plcOutput[5] = ((*node)->did & 0xFF);
	plcOutput[6] = (((*node)->did >> 8) & 0xFF);
	plcOutputLen = 7;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		res = IT700_WaitAnswerNode(0xA3, 0x1, node);
	}

	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetTableEntry %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_GetTable(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;
	int snTableSize = 0;

	//protect
	PLC_NodesTableProtect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetTable");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x03;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x65;
	plcOutput[5] = 0x01;
	plcOutputLen = 6;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.snTableResponce = 0xFF;
		res = IT700_WaitAnswerInt(0x65, 0x1, &snTableSize);
	}

	//if answer is OK
	if (res == OK){

		//debug
		COMMON_STR_DEBUG(DEBUG_PLC "----------------------------------");
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetTable table size  [%u]", snTableSize);
		COMMON_STR_DEBUG(DEBUG_PLC "----------------------------------");

		//create nodes
		PLC_Protect();

		plcBase.nodesCount = snTableSize;
		if (snTableSize > 0){
			plcBase.nodes = (plcNode_t *)malloc(plcBase.nodesCount * sizeof (plcNode_t));
			if (plcBase.nodes != NULL){
				int nodeIndex = 0;

				//clear nodes
				memset(plcBase.nodes, 0, plcBase.nodesCount * sizeof (plcNode_t));

				//get nodes info
				for (; ((nodeIndex < plcBase.nodesCount) && (res == OK)); nodeIndex++ ){
					plcNode_t * someNode = &plcBase.nodes[nodeIndex];
					res = IT700_GetTableEntry(nodeIndex, &someNode);
				}

			} else {
				res = ERROR_MEM_ERROR;
			}
		}

		PLC_Unprotect();
	}

	//fix last update time
	time(&plcBase.lastTableUpdate);

	//unprotect
	PLC_NodesTableUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetTable %s", getErrorCode(res));

	return res;
}

//
//
//

#define MAX_PLC_SN_TABLES_ITEMS_FOR_READ 5

int IT700_UpdateTable(int * agedCounters){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;
	int snTableSize = 0;

	int nodeIndex = 0;
	int startIndex = 0;
	int stopIndex = 0;
	static int lastIndex = 0;

	plcNode_t * someNode = NULL;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_UpdateTable");

	//protect
	PLC_NodesTableProtect();

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x03;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x65;
	plcOutput[5] = 0x01;
	plcOutputLen = 6;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.snTableResponce = 0xFF;
		res = IT700_WaitAnswerInt(0x65, 0x1, &snTableSize);
	}

	//if answer is OK
	if (res == OK){

		//debug
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_UpdateTable table size  [%u]", snTableSize);

		if (snTableSize > 0){

			startIndex = lastIndex;
			if (startIndex >= snTableSize){
				startIndex = 0;
			}

			int portionSize = MAX_PLC_SN_TABLES_ITEMS_FOR_READ;
			if (snTableFullRead == FALSE)
				portionSize+=MAX_PLC_SN_TABLES_ITEMS_FOR_READ;

			stopIndex = startIndex + portionSize;
			if (stopIndex >= snTableSize){
				stopIndex = snTableSize;
				lastIndex = 0;
			} else {
				lastIndex = stopIndex;
			}

			someNode = (plcNode_t *)malloc(sizeof (plcNode_t));
			if (someNode != NULL){
				for (nodeIndex = startIndex; (nodeIndex < stopIndex) && (res == OK); nodeIndex++ ){
					res = IT700_GetTableEntry(nodeIndex, &someNode);
					if (res == OK){
						res = PLC_INFO_TABLE_UpdateRemoteNode(someNode, agedCounters);
					}

				}
				free(someNode);
				someNode = NULL;

			} else {

				res = ERROR_MEM_ERROR;
			}
		}
	}

	//unprotect
	PLC_NodesTableUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_UpdateTable %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_DeleteNode (unsigned int nodeId){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_DeleteNode [%04x]", nodeId);

	//protect
	PLC_NodesTableProtect();

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x05;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x6A;
	plcOutput[5] = 0x01;
	plcOutput[6] = (unsigned char)(nodeId & 0xFF);
	plcOutput[7] = (unsigned char)((nodeId >> 8) & 0xFF);
	plcOutputLen = 8;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) || (res != OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.snTableResponce = 0xFF;
		res = IT700_WaitAnswer(0x6A, 0x1);
		if (res == OK){
			res = PLC_INFO_TABLE_DeleteNode(nodeId);
		}
	}

	//unprotect
	PLC_NodesTableUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_DeleteNode [%04x] result %s", nodeId, getErrorCode(res));

	return res;
}

//
//
//
int IT700_EraseAddressDb(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_EraseAddressDb");

	//protect
	PLC_NodesTableProtect();

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x02;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0xA2;
	plcOutputLen = 5;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.snTableResponce = 0xFF;
		res = IT700_WaitAnswer(0xA2, 0x1);

		if (res == OK){
			res = PLC_INFO_TABLE_Erase();
		}
	}


	//unprotect
	PLC_NodesTableUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_EraseAddressDb result %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_EraseTable(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_EraseTable");

	//journaling
	STORAGE_JournalUspdEvent( EVENT_PLC_TABLE_START_CLEARING , NULL );

	//erase
	res = IT700_EraseAddressDb();
	if (res == OK){
		if (PLC_GetNetworkMode() == PLC_ADMISSION_WITH_DETECT)
		{
			//stop plc loop
			POOL_StopPlc();

			//wait plc task done
			POOL_WaitPlcLoopEnds();

			//delete statistic
			STORAGE_DeleteStatisticMap();

			//delete plc config
			STORAGE_ClearPlcCounterConfiguration();

			//resume rf poller part
			POOL_ResumePlcLoop();
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_EraseTable result %s", getErrorCode(res));

	STORAGE_JournalUspdEvent( EVENT_PLC_TABLE_STOP_CLEARING , NULL );

	return res;
}

int IT700_LeaveNetwork()
{
	DEB_TRACE(DEB_IDX_PLC);

	int res = ERROR_GENERAL ;
	int written = 0 ;
	int plcOutputLength = 0 ;
	unsigned char plcOutput[64];
	memset( plcOutput , 0 , 64 );

	COMMON_STR_DEBUG( DEBUG_PLC "PLC Try to pause plc pool");
	POOL_Pause();

	COMMON_STR_DEBUG( DEBUG_PLC "PLC paused, try to clear Address DB (SN Table)");
	IT700_EraseAddressDb();

	//journaling
	STORAGE_JournalUspdEvent( EVENT_PLC_TABLE_ALL_LEAVE_NET , (unsigned char *)DESC_EVENT_PLC_LEAVE_NET_ALL_COUNTERS );

	COMMON_STR_DEBUG( DEBUG_PLC "PLC paused, send command leave network");
	plcOutput[ plcOutputLength++ ] = 0xCA ;
	plcOutput[ plcOutputLength++ ] = 0x11 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x60 ;
	plcOutput[ plcOutputLength++ ] = 0x03 ;
	plcOutput[ plcOutputLength++ ] = 0x01 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x08 ;
	plcOutput[ plcOutputLength++ ] = 0x07 ;
	plcOutput[ plcOutputLength++ ] = 0x01 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x01 ;
	plcOutput[ plcOutputLength++ ] = 0xCA ;
	plcOutput[ plcOutputLength++ ] = 0x02 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0x00 ;
	plcOutput[ plcOutputLength++ ] = 0xA6 ;
	plcOutput[ plcOutputLength++ ] = 0xA8 ;
	plcOutput[ plcOutputLength++ ] = 0xA0 ;

	res = SERIAL_Write(PORT_PLC, (unsigned char *)plcOutput, plcOutputLength, &written);
	if ((plcOutputLength != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	COMMON_STR_DEBUG( DEBUG_PLC "PLC resume plc pool");
	POOL_Continue();

	return res ;
}

//
//
//
int IT700_CheckNodeKey(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[32];
	int res = ERROR_PLC;
	int written = 0;
	int plcOutputLen = 0;
	unsigned char n[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	unsigned char ni[8];
	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckNodeKey");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x07;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x42;
	plcOutput[5] = 0x06;
	plcOutput[6] = 0x4E;
	plcOutput[7] = 0x00;
	plcOutput[8] = 0x08;
	plcOutput[9] = 0x00;
	plcOutputLen = 10;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		res = IT700_WaitAnswerVariable(0x42, 0x1, n, 16);
		if (res == OK){
			unsigned char * p = plcBase.settings.nodeKey;

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckNodeKey nodekey is %02x %02x %02x %02x %02x %02x %02x %02x", n[0], n[2], n[4], n[6], n[8], n[10], n[12], n[14]);
			COMMON_STR_DEBUG(DEBUG_PLC "plc nodekey in bs settings is %02x %02x %02x %02x %02x %02x %02x %02x", p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);

			ni[0] = n[14];
			ni[1] = n[12];
			ni[2] = n[10];
			ni[3] = n[8];
			ni[4] = n[6];
			ni[5] = n[4];
			ni[6] = n[2];
			ni[7] = n[0];

			if (memcmp(p, ni, 8) != 0){

				COMMON_STR_DEBUG(DEBUG_PLC "NODE KEY DIFFERS, NEED TO SET NEW NODE KEY");

				//create command
				plcOutput[0] = 0xCA;

				plcOutput[3] = 0x00;
				plcOutput[4] = 0x41;
				plcOutput[5] = 0x06;
				plcOutput[6] = 0x4E;
				plcOutput[7] = 0x00;

				plcOutput[8] = p[7];
				plcOutput[9] = 0x0;
				plcOutput[10] = p[6];
				plcOutput[11] = 0x0;
				plcOutput[12] = p[5];
				plcOutput[13] = 0x0;
				plcOutput[14] = p[4];
				plcOutput[15] = 0x0;
				plcOutput[16] = p[3];
				plcOutput[17] = 0x0;
				plcOutput[18] = p[2];
				plcOutput[19] = 0x0;
				plcOutput[20] = p[1];
				plcOutput[21] = 0x0;
				plcOutput[22] = p[0];
				plcOutput[23] = 0x0;

				//set total len
				plcOutputLen = 24;

				//set len to packet
				plcOutput[1] = ((plcOutputLen-3) & 0xFF);
				plcOutput[2] = (((plcOutputLen-3) >> 8) & 0xFF);

				//add crc
				plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
				plcOutputLen++;

				//send
				res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
				if ((plcOutputLen != written) && (res == OK)){
					res = ERROR_SIZE_WRITE;
				}

				COMMON_STR_DEBUG(DEBUG_PLC "NEW NODE KEY WRITED, wait answer");

				//wait answer
				if (res == OK){
					plcBase.paramResponce = 0xFF;
					res = IT700_WaitAnswer(0x41, 0x1);
				}

				//save parameters
				if (res == OK){
					res = IT700_SaveParameters(4);
				}

			}
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckNodeKey %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_CheckSerial(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[64];
	int res = ERROR_PLC;
	int written = 0;
	int plcOutputLen = 0;
	unsigned long uspdSnI = 0;
	unsigned char sn[32];
	
	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckSerial");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x07;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x42;
	plcOutput[5] = 0x05;
	plcOutput[6] = 0x00;
	plcOutput[7] = 0x00;
	plcOutput[8] = 0x10;
	plcOutput[9] = 0x00;
	plcOutputLen = 10;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		res = IT700_WaitAnswerVariable(0x42, 0x1, sn, 16);
		if (res == OK){
			//debug
			COMMON_BUF_DEBUG(DEBUG_PLC "IT700_CheckSerial", sn, 16);

			//get current uspd sn (without start 'U' character)
			COMMON_GET_USPD_SN_UL(&uspdSnI);

			//return if USPD SN is not set
			if (uspdSnI == 0){
				COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckSerial quit: uspd sn is not set, can't compare and correct");
				return OK;
			}

		   //checjk uspd sn
		   if ((sn[0] == (uspdSnI & 0xFF)) &&
			   (sn[1] == ((uspdSnI >> 8) & 0xFF))  &&
			   (sn[2] == ((uspdSnI >> 16) & 0xFF)) &&
			   (sn[3] == (((uspdSnI >> 24) & 0xFF))+0x10) && //special offset for unique base station number

			   (sn[4]  == 0x00) &&
			   (sn[5]  == 0x00) &&
			   (sn[6]  == 0x00) &&
			   (sn[7]  == 0x00) &&

			   (sn[8]  == 0x00) &&
			   (sn[9]  == 0x02) &&
			   (sn[10] == 0x52) &&
			   (sn[11] == 0x35) &&

			   (sn[12] == 0x00) &&
			   (sn[13] == 0x00) &&
			   (sn[14] == 0x01) &&
			   (sn[15] == 0x00)){

				res = OK;
				//debug
				COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckSerial ok");

		   } else {

				//debug
				COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckSerial not ok, need to fix");

				sn[0] = (uspdSnI & 0xFF);
				sn[1] = ((uspdSnI >> 8) & 0xFF);
				sn[2] = ((uspdSnI >> 16) & 0xFF);
				sn[3] = (((uspdSnI >> 24) & 0xFF) + 0x10); //special offset for unique base station number

				sn[4] = 0x00;
				sn[5] = 0x00;
				sn[6] = 0x00;
				sn[7] = 0x00;

				sn[8] = 0x00;
				sn[9] = 0x02;
				sn[10]= 0x52;
				sn[11]= 0x35;

				sn[12]= 0x00;
				sn[13]= 0x00;
				sn[14]= 0x01;
				sn[15]= 0x00;

				//create command
				plcOutput[0] = 0xCA;

				plcOutput[3] = 0x00;
				plcOutput[4] = 0x41;
				plcOutput[5] = 0x05;
				plcOutput[6] = 0xAB;
				plcOutput[7] = 0xBA;

				plcOutput[8] = sn[0];
				plcOutput[9] = sn[1];
				plcOutput[10] = sn[2];
				plcOutput[11] = sn[3];
				plcOutput[12] = sn[4];
				plcOutput[13] = sn[5];
				plcOutput[14] = sn[6];
				plcOutput[15] = sn[7];
				plcOutput[16] = sn[8];
				plcOutput[17] = sn[9];
				plcOutput[18] = sn[10];
				plcOutput[19] = sn[11];
				plcOutput[20] = sn[12];
				plcOutput[21] = sn[13];
				plcOutput[22] = sn[14];
				plcOutput[23] = sn[15];

				//set total len
				plcOutputLen = 24;

				//set len to packet
				plcOutput[1] = ((plcOutputLen-3) & 0xFF);
				plcOutput[2] = (((plcOutputLen-3) >> 8) & 0xFF);

				//add crc
				plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
				plcOutputLen++;

				//send
				res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
				if ((plcOutputLen != written) && (res == OK)){
					res = ERROR_SIZE_WRITE;
				}

				COMMON_STR_DEBUG(DEBUG_PLC "NEW SN WRITED, wait answer");

				//wait answer
				if (res == OK){
					plcBase.paramResponce = 0xFF;
					res = IT700_WaitAnswer(0x41, 0x1);
				}

				//save parameters
				if (res == OK){
					res = IT700_SaveParameters(0xFF);
				}
			}
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckSerial %s", getErrorCode(res));

	return res;
}


int IT700_CheckMDNICR(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written = 0;
	int plcOutputLen = 0;
	unsigned char n[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	//unsigned char b[6] = {0,0,0,0,0,0};

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckMDNICR");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x07;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x42;
	plcOutput[5] = 0x02;
	plcOutput[6] = 0x5F;
	plcOutput[7] = 0x00;
	plcOutput[8] = 0x06;
	plcOutput[9] = 0x00;
	plcOutputLen = 10;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.paramResponce = 0xFF;
		memset(plcBase.paramValueV, 0, 32);
		res = IT700_WaitAnswerVariable(0x42, 0x1, n, 12);
		if (res == OK){
			//unsigned char * p = plcBase.settings->nodeKey;

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckMDNICR data is %02x %02x %02x %02x %02x %02x", n[0], n[2], n[4], n[6], n[8], n[10]);
			//COMMON_STR_DEBUG(DEBUG_PLC "plc base setting  data is %02x %02x %02x %02x %02x %02x", b[0], b[1], b[2], b[3], b[4], b[5]);

			//
			//to do - check and set mdnicr
			//

		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_CheckMDNICR %s", getErrorCode(res));

	return res;
}
//
//
//
int IT700_GetParam(int paramIndex, int paramTable, int * value){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written = 0;
	int plcOutputLen = 0;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetParam: paramIndex [0x%02x] paramTable [0x%02x]", paramIndex, paramTable);

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x07;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x42;
	plcOutput[5] = (paramTable & 0xFF);
	plcOutput[6] = (paramIndex & 0xFF);
	plcOutput[7] = ((paramIndex >> 8) & 0xFF);
	plcOutput[8] = 0x01;
	plcOutput[9] = 0x00;
	plcOutputLen = 10;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.paramResponce = 0xFF;
		res = IT700_WaitAnswerInt(0x42, 0x1, value);

		//debug
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetParam value [0x%04x]", *value);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GetParam %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_SetParam(int value, int paramIndex, int paramTable){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written = 0;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetParam: value [0x%04x] paramIndex [0x%02x] paramTable [0x%02x]", value, paramIndex, paramTable);

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x07;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x41;
	plcOutput[5] = (paramTable & 0xFF);
	plcOutput[6] = (paramIndex & 0xFF);
	plcOutput[7] = ((paramIndex >> 8) & 0xFF);
	plcOutput[8] = (value & 0xFF);
	plcOutput[9] = ((value >> 8) & 0xFF);
	plcOutputLen = 10;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.paramResponce = 0xFF;
		res = IT700_WaitAnswer(0x41, 0x1);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetParam %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_SaveParameters (int paramTable){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_SaveParameters: paramTable [0x%02x]", paramTable);

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x03;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x43;
	plcOutput[5] = (paramTable & 0xFF);
	plcOutputLen = 6;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK){
		plcBase.paramResponce = 0xFF;
		res = IT700_WaitAnswer(0x43, 0x1);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_SaveParameters %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_Init(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen = 6;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_Init");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x02;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x21;
	plcOutput[5] = 0x23;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK) {
		plcBase.stackResponce = 0xFF;
		res = IT700_WaitAnswer(0x21, 0x1);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_Init %s", getErrorCode(res));

	return res;
}


//
//
//
int IT700_GoOnline(){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned char plcOutput[20];
	int res = ERROR_PLC;
	int written;
	int plcOutputLen = 6;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GoOnline");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 0x02;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0x22;
	plcOutput[5] = 0x24;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//wait answer
	if (res == OK) {
		plcBase.stackResponce = 0xFF;
		res = IT700_WaitAnswer(0x22, 0x1);
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_GoOnline %s", getErrorCode(res));

	return res;
}

//
//
//
int IT700_SetConfig(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	int value;
	int checkValue;
	
	BOOL needSave = FALSE;
	
	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig");

	//check automode
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK AUTOMODE");
	value = -1;
	checkValue = 0;
	res = IT700_GetParam(14, 1, &value);
	if ((res == OK) && (value != -1)){
		if (value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK AUTOMODE : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 14, 1);
			if (res){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK AUTOMODE : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//get and set node key
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NODE KEY");
	res = IT700_CheckNodeKey();
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//get and set node key
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK SERIAL");
	res = IT700_CheckSerial();
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//get MDNICR indicator bits
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK MDNICR BITS");
	res = IT700_CheckMDNICR();
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//base station
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK BASE");
	value = -1;
	checkValue = 3; //operation mode - base station
	res = IT700_GetParam(36, 4, &value);
	if ((res == OK) && (value != -1)){
		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK BASE : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 36, 4);
			if (res == OK){
				needSave = TRUE;
			}

		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK BASE : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//addmission
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK MODE");
	value = -1;
	res = IT700_GetParam(50, 4, &value);
	if ((res == OK) && (value != -1)){

		//depending configurated mode
		switch (PLC_GetNetworkMode()){
			case PLC_ADMISSION_AUTO:
			case PLC_ADMISSION_WITH_DETECT:
				checkValue = 0; // mode 'auto'
				break;

			default:
			case PLC_ADMISSION_APPLICATION:
				checkValue = 3; // mode 'application approval'
				break;
		}

		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK MODE : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 50, 4);
			if (res){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK MODE : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//if network id forced is not null
	if (plcBase.networkIdForced != 0){

		//check net id mode selection
		COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID SEL MODE");
		value = -1;
		checkValue = 1; //forced
		res = IT700_GetParam(52, 4, &value);
		if ((res == OK) && (value != -1)){
			if(value != checkValue){
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID SEL MODE : NEED TO CORRECT");
				res = IT700_SetParam(checkValue, 52, 4);
				if (res){
					needSave = TRUE;
				}
			} else {
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID SEL MODE : OK");
			}
		}

		//check to quit
		if (res != OK){
			COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
			return res;
		}

		//check net id value
		COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID ");
		value = -1;
		checkValue = plcBase.networkIdForced; //last serial digits
		res = IT700_GetParam(33, 4, &value);
		if ((res == OK) && (value != -1)){
			if(value != checkValue){
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID : read net ID: %u", value);
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID : need net ID: %u", checkValue);			
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID : NEED TO CORRECT");
				res = IT700_SetParam(checkValue, 33, 4);
				if (res){
					needSave = TRUE;
				}
			} else {
				COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK NET ID : OK");
			}
		}

		//check to quit
		if (res != OK){
			COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
			return res;
		}
	}

	//network size
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK P SZ");
	value = -1;
	checkValue = plcBase.settings.netSizePhisycal;
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: PHISICAL SIZE TO SET: %d", checkValue);
	res = IT700_GetParam(91, 6, &value);
	if ((res == OK) && (value != -1)){
		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK P SZ : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 91, 6);
			if (res){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK P SZ : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//logical size
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK L SZ");
	value = -1;
	checkValue = plcBase.settings.netSizeLogical;
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: LOGICAL SIZE TO SET: %d", checkValue);
	res = IT700_GetParam(56, 6, &value);
	if ((res == OK) && (value != -1)){
		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK L SZ : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 56, 6);
			if (res){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK L SZ : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//distribution index
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DIDX");
	value = -1;
	checkValue = 0xB8; //was 255
	res = IT700_GetParam(93, 6, &value);
	if ((res == OK) && (value != -1)){
		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DIDX : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 93, 6);
			if (res == OK){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DIDX : OK");
		}
	}

	//check to quit
	if (res != OK){
		COMMON_STR_DEBUG(DEBUG_PLC "IT700_SetConfig %s", getErrorCode(res));
		return res;
	}

	//distribution value
	COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DVAL");
	value = -1;
	checkValue = plcBase.settings.netSizePhisycal;
	if (checkValue == 0)
		checkValue = 10;
	res = IT700_GetParam(94, 6, &value);
	if ((res == OK) && (value != -1)){
		if(value != checkValue){
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DVAL : NEED TO CORRECT");
			res = IT700_SetParam(checkValue, 94, 6);
			if (res == OK){
				needSave = TRUE;
			}
		} else {
			COMMON_STR_DEBUG(DEBUG_PLC "PLC CONFIGURE: CHECK DVAL : OK");
		}
	}
	
	// check we nned to save some in PIM NVM
	if (needSave == TRUE){
		IT700_SaveParameters(0xFF); //in all tables
	}

	return res;
}

//
//
//
int IT700_SendAdmissionResponce(int result, unsigned char * admissionData){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	unsigned char plcOutput[60];
	int written;
	int plcOutputLen;

	//debug
	COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_SendAdmissionResponce");

	//create command
	plcOutput[0] = 0xCA;
	plcOutput[1] = 33;
	plcOutput[2] = 0x00;
	plcOutput[3] = 0x00;
	plcOutput[4] = 0xA4;
	plcOutput[5] = (result & 0xFF);
	plcOutput[6] = ((result >> 8) & 0xFF);
	memcpy(&plcOutput[7], admissionData, 29);
	plcOutputLen = 7+29;
	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_SendAdmissionResponce %s", getErrorCode(res));

	return res;
}

//
//
//
void IT700_Admission(unsigned char * admissionData){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	unsigned long checkSn = 0;

	unsigned char modemSerial[32];

	counter_t * counterPtr = NULL;

	//get admission data
	checkSn = (admissionData[0] ) + ((admissionData[1] <<8)) + ((admissionData[2] <<16)) + ((admissionData[3] <<24));

	//debug
	COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_Admission new sn [%u]", checkSn);

	//create modemSerial
	memset(modemSerial, 0 , 32);
	sprintf((char *)modemSerial, "%lu", checkSn);

	//check modem presence in configuration
	res = STORAGE_FoundPlcCounter(modemSerial, &counterPtr);
	if (res == OK){

		//debug
		COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_Admission, modem present in configuration");

		//allow connect modem
		res = IT700_SendAdmissionResponce(0x0000, admissionData);
		if (res == OK){
			// save event about admission of modem ok
			STORAGE_EVENT_PLC_REMOTE_WAS_ADDED(modemSerial);
		}
		
	} else {

		//debug
		COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_Admission, modem NOT present in configuration");

		//disallow modem connect
		res =  IT700_SendAdmissionResponce(0x2000, admissionData);
		if (res == OK){
			//save event about admission of modem not in list
			STORAGE_EVENT_PLC_REMOTE_WAS_DELETED(modemSerial);
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_Admission %s", getErrorCode(res));
}

//
//
//

void PLC_IncPackTag(){
	//change tag of packet
	plcPacTag++;
	if (plcPacTag > PLC_MAX_PACK_TAG){
		plcPacTag = 0;
	}
}

void PLC_SetNeedToReset(BOOL setVal){
	PLC_Protect();
	plcBase.needToReset = setVal;
	PLC_Unprotect();
}

BOOL PLC_IsExternalReset(void){
	BOOL retVal = FALSE;

	PLC_Protect();
	retVal = plcBase.externalReset;
	PLC_Unprotect();

	return retVal;
}

BOOL PLC_IsNeedToReset(void){
	BOOL retVal = FALSE;

	PLC_Protect();
	retVal = plcBase.needToReset;
	PLC_Unprotect();

	return retVal;
}

int PLC_GetSNTableTimeout(void){
	int tDiff = 0;

	PLC_Protect();
	tDiff = time(NULL) - plcBase.lastTableUpdate;
	PLC_Unprotect();

	return tDiff;
}

void PLC_UpdateSNTableTimeout(void){
	PLC_Protect();
	time(&plcBase.lastTableUpdate);
	PLC_Unprotect();
}

void PLC_ProcessTable(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = OK;
	int agedCounters = 0;

	//check unhandled reset
	if (PLC_IsExternalReset() == TRUE){
		STORAGE_EVENT_PLC_DEVICE_ERROR(0x1); //external reset occured
		PLC_SetNeedToReset(TRUE);
	}

	//check needed reset manual
	if (PLC_IsNeedToReset() == TRUE){
		res = PLC_InitializePlcBase();
		if (res != OK){
			PIO_RebootUspd();
			SHELL_CMD_REBOOT;
		}
	}

	//do table reread
	int tDiff = PLC_GetSNTableTimeout();
	if (tDiff > TIMEOUT_TABLE_UPDATE_ON_PLC) {

		COMMON_STR_DEBUG(DEBUG_POOL_PLC "\r\n\r\n----------------------------------\r\nPLC PROCESS TABLE\r\n----------------------------------");

		//read table of remotes
		res = IT700_UpdateTable(&agedCounters);
		if (res != OK){
			PLC_ErrorIncrement();
		} 

		//update time
		PLC_UpdateSNTableTimeout();
	}
}

//
//
//
int PLC_AddCounterTask(counterTask_t ** counterTask){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_GENERAL;

	//enter cs
	PLC_TaskProtect();

	plcBase.plcTask.tasks = (counterTask_t **)realloc(plcBase.plcTask.tasks, (++(plcBase.plcTask.tasksCount) * sizeof(counterTask_t *))); /* 1 pointer for elem */
	if (plcBase.plcTask.tasks != NULL)
	{
		plcBase.plcTask.tasks[plcBase.plcTask.tasksCount-1] = *counterTask;
		res = OK;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_AddCounterTask([SN %s]) %s", plcBase.plcTask.tasks[plcBase.plcTask.tasksCount-1]->associatedSerial, getErrorCode(res));

	//leave cs
	PLC_TaskUnprotect();

	return res;
}

//
//
//
int PLC_GetCounterTasksCount(){
	DEB_TRACE(DEB_IDX_PLC)


	int count = 0;

	//enter cs
	PLC_TaskProtect();

	count = plcBase.plcTask.tasksCount;

	//exit cs
	PLC_TaskUnprotect();

	return count;
}

//
//
//
int PLC_GetCounterTaskByIndex(int taskIndex, counterTask_t ** plcTask){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_GENERAL;

	//enter cs
	PLC_TaskProtect();

	if (plcBase.plcTask.tasksCount > 0) {

		if (taskIndex < (int)plcBase.plcTask.tasksCount)
		{
			(*plcTask) = plcBase.plcTask.tasks[taskIndex];
			res = OK;
		}
		else
		{
			(*plcTask) = NULL;
			res = ERROR_IS_NOT_FOUND;
		}

	}
	
	//exit cs
	PLC_TaskUnprotect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_GetCounterTasks() %s", getErrorCode(res));

	return res;
}

//
//
//
int PLC_GetTaskIndexByTagNoAtom(int taskTag, int * foundTaskIndex){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int taskIndex = 0;
	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_GetTaskIndexByTagNoAtom() try enter");

	(*foundTaskIndex) = -1;

	//loop on tasks
	for(;taskIndex < (int)plcBase.plcTask.tasksCount; taskIndex++){
		if(plcBase.plcTask.tasks[taskIndex]->taskTag == taskTag){
			(*foundTaskIndex) = taskIndex;
			res = OK;
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_GetTaskIndexByTagNoAtom(%s) exit", getErrorCode(res));

	return res;
}

//
//
//
int PLC_RunTask(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;

	unsigned int taskIndex = 0;
	unsigned int transactionIndex = 0;
	int gettedTaskTag = 0;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_RunTask");

	//enter cs
	PLC_TaskProtect();

	if (plcBase.plcTask.tasksCount > 0) {
		
		COMMON_STR_DEBUG(DEBUG_PLC "PLC_RunTask send to %s", plcBase.plcTask.tasks[taskIndex]->associatedSerial);

		//set index
		plcBase.plcTask.tasks[taskIndex]->currentTransaction  = 0;
		transactionIndex = plcBase.plcTask.tasks[taskIndex]->currentTransaction;
		
		plcBase.plcTask.tasks[taskIndex]->associatedId = 0;
		plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->tStart = time(NULL);
		
		//call send of first command
		res = PLC_SendToRemote(plcBase.plcTask.tasks[taskIndex]->associatedSerial,
							   plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->command,
							   plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->commandSize,
							   &gettedTaskTag);
		
		//setup task tag
		if (res == OK){
			plcBase.plcTask.tasks[taskIndex]->taskTag = gettedTaskTag;
			plcBase.plcTask.tasks[taskIndex]->responsesCout = 0;
			time(&plcBase.plcTask.tasks[taskIndex]->lastAction);
		}
		
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_RunTask exit (res = %s)", getErrorCode(res));

	//leave cs
	PLC_TaskUnprotect();

	return res;
}

//
//
//
int PLC_NextStepOfTask(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = OK;
	unsigned int taskIndex = 0;
	unsigned int transactionIndex = 0;
	int gettedTaskTag = 0;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_NextStepOfTask enter");

	//enter cs
	PLC_TaskProtect();

	if (plcBase.plcTask.tasksCount > 0){

		//check current transaction index
		if ((plcBase.plcTask.tasks[taskIndex]->currentTransaction + 1) < plcBase.plcTask.tasks[taskIndex]->transactionsCount)
		{
			COMMON_STR_DEBUG(DEBUG_PLC "PLC_NextStepOfTask (SN[%s]) process", plcBase.plcTask.tasks[taskIndex]->associatedSerial);

			//increment transaction count
			plcBase.plcTask.tasks[taskIndex]->currentTransaction++;

			//set index
			transactionIndex = plcBase.plcTask.tasks[taskIndex]->currentTransaction;

			//call send of next command
			plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->tStart = time(NULL);
			res = PLC_SendToRemote(plcBase.plcTask.tasks[taskIndex]->associatedSerial,
								   plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->command,
								   plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->commandSize,
								   &gettedTaskTag);

			//setup task tag
			if (res == OK){
				plcBase.plcTask.tasks[taskIndex]->taskTag = gettedTaskTag;
				plcBase.plcTask.tasks[taskIndex]->responsesCout = 0;
				time(&plcBase.plcTask.tasks[taskIndex]->lastAction);
			}
		}

	}
	
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_NextStepOfTask exit");

	//leave cs
	PLC_TaskUnprotect();

	return res;
}

//
//
//
BOOL PLC_IsTaskComplete(int * lastWasTimeouted){
	DEB_TRACE(DEB_IDX_PLC)

	unsigned int taskIndex = 0;
	unsigned int transactionIndex = 0;
	unsigned int transactionIndexSkip = 0;
	unsigned char lastTransactionResult = TRANSACTION_NO_RESULT;

	time_t nowTime;
	int taskTime;

	*lastWasTimeouted = 0;

	//enter cs
	PLC_TaskProtect();

	//COMMON_STR_DEBUG(DEBUG_PLC "PLC TASK is complete ?");

	//check we have any tasks
	if (plcBase.plcTask.tasksCount > 0) {
	
		//check timings on last transaction
		time(&nowTime);
		taskTime = nowTime - plcBase.plcTask.tasks[taskIndex]->lastAction;

		//get trasaction skip start index
		transactionIndexSkip=plcBase.plcTask.tasks[taskIndex]->currentTransaction;

		//get last transaction result
		lastTransactionResult = plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->result;

		//check timeout, break or no-responce on last transaction
		if ((taskTime > TIMEOUT_READING_ON_PLC) || (plcBreakFlag == 1) || (taskTime < 0) || (lastTransactionResult==TRANSACTION_DONE_ERROR_RESPONSE)) {

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "PLC TASK %d TRANSACTION %d TIMEOUTED, skip future task transactions", taskIndex, transactionIndexSkip);

			//depending no route or timeout
			if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->result == TRANSACTION_NO_RESULT) {
				STORAGE_SetCounterStatistic(plcBase.plcTask.tasks[taskIndex]->counterDbId,
										 FALSE,
										 plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->transactionType,
										 TRANSACTION_DONE_ERROR_TIMEOUT);

			} else {
				STORAGE_SetCounterStatistic(plcBase.plcTask.tasks[taskIndex]->counterDbId,
										 FALSE,
										 plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->transactionType,
										 plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->result);
			}


			//transaction timeouted, so skip al other transactions also
			for (; (transactionIndexSkip < plcBase.plcTask.tasks[taskIndex]->transactionsCount); transactionIndexSkip++){
				plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndexSkip]->result = TRANSACTION_DONE_ERROR_TIMEOUT;
			}

			*lastWasTimeouted = 1;
		}

		//and theirs all transactions
		for (; (transactionIndex < plcBase.plcTask.tasks[taskIndex]->transactionsCount); transactionIndex++)
		{
			//if some task is still have no DONE or ERROR result - return FALSE
			if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result == TRANSACTION_NO_RESULT)
			{
				PLC_TaskUnprotect();
				return FALSE;
			}			
		}
		
	}

	
	//reset flag if needed
	if (plcBreakFlag == 1){
		plcBreakFlag = 0;
	}

	//enter cs
	PLC_TaskUnprotect();

	//if no returns were passed, so all tasks and theirs transaction have anyone result
	return TRUE;
}

//
//
//
void PLC_BreakTask(){
	//enter cs
	PLC_TaskProtect();

	plcBreakFlag = 1;

	//enter cs
	PLC_TaskUnprotect();
}

//
//
//
int PLC_FreeTask(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_GENERAL;
	int taskIndex = 0;

	//enter cs
	PLC_TaskProtect();

	if (plcBase.plcTask.tasksCount > 0)
	{
		//do loop over tasks from last to first
		taskIndex = plcBase.plcTask.tasksCount - 1;

		//loop
		for(; taskIndex >= 0 ; taskIndex--)
		{
			res = COMMON_FreeCounterTask(&plcBase.plcTask.tasks[taskIndex]);
			if (res != OK)
				SET_RES_AND_BREAK_WITH_MEM_ERROR
		}
	}

	//free task
	if (res == OK)
	{
		free(plcBase.plcTask.tasks);
		plcBase.plcTask.tasks = NULL;
		plcBase.plcTask.tasksCount = 0;
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_FreeTask() %s", getErrorCode(res));

	//leave cs
	PLC_TaskUnprotect();

	return res;
}


//
//
//
int PLC_SendToRemote(unsigned char * serial, unsigned char * buf, unsigned char toSend, int * taskTagAssigned){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	unsigned long remoteSerial = strtoul((char *)serial, NULL, 10);
	
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SendToRemote enter");
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SendToRemote send to RS [%s]", serial);
	COMMON_BUF_DEBUG(DEBUG_PLC "PLC_SendToRemote ", buf, toSend);

	int written;
	int plcOutputLen = 30 + toSend;
	unsigned char * plcOutput = (unsigned char *)malloc(plcOutputLen+1);
	if (plcOutput == NULL){
		//debug
		COMMON_STR_DEBUG(DEBUG_PLC "PLC_SendToRemote exit with ERROR_MEM_ERROR");

		res = ERROR_MEM_ERROR;
		return res;
	}

	PLC_IncPackTag();
	
	//setup return filed of assigned task tag
	(*taskTagAssigned) = (plcPacTag & 0xFFFF);

	//fill packet
	plcOutput[0] = 0xCA;
	plcOutput[1] = ((27+toSend) & 0xFF);
	plcOutput[2] = (((27+toSend) >> 8) & 0xFF);
	plcOutput[3] = 0x00;					// request
	plcOutput[4] = 0x60;					// opcode
	plcOutput[5] = 0x02;					// intranet by SN
	plcOutput[6] = 0x01;					// prority
	plcOutput[7] = 0x00;					// ack (with acknolage)
	plcOutput[8] = 0x08;					// max hop count
	plcOutput[9] = 0x07;					// max gain
	plcOutput[10] = (plcPacTag & 0xFF);		// tag lo depending on recieved
	plcOutput[11] = (plcPacTag >> 8) & 0xFF;// tag hi depending on recieved
	plcOutput[12] = 0x00;					// not encrypted
	plcOutput[13] = 0x00;					// port
	plcOutput[14] = (remoteSerial & 0xFF);
	plcOutput[15] = (remoteSerial >> 8) & 0xFF;
	plcOutput[16] = ((remoteSerial >> 8) >> 8)& 0xFF;
	plcOutput[17] = (((remoteSerial >> 8) >> 8) >> 8) & 0xFF;

	plcOutput[18] = 0x00; // hi short id
	plcOutput[19] = 0x00; // hi short id
	plcOutput[20] = 0x00; // hi short id
	plcOutput[21] = 0x00; // hi short id

	plcOutput[22] = 0x00; // hi short id
	plcOutput[23] = 0x00; // hi short id
	plcOutput[24] = 0x00; // hi short id
	plcOutput[25] = 0x00; // hi short id

	plcOutput[26] = 0x00; // hi short id
	plcOutput[27] = 0x00; // hi short id
	plcOutput[28] = 0x00; // hi short id
	plcOutput[29] = 0x00; // hi short id

	memcpy(&plcOutput[30], buf, toSend);    //data

	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//reset responces indexes
	plcBase.responce1 = 0xff;
	plcBase.responce2 = 0xff;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//
	free (plcOutput);
	plcOutput = NULL;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SendToRemote exit");

	return res;
}


int PLC_SendToRemote2(unsigned char * remoteSN, unsigned char * buf, unsigned char toSend, unsigned char port, int useValidHiSnPart, int * taskTagAssigned){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_PLC;
	unsigned long remAddr;
	
	COMMON_STR_DEBUG(DEBUG_SHAMANING_PLC "PLC_SendToRemote2 enter");
	COMMON_STR_DEBUG(DEBUG_SHAMANING_PLC "PLC_SendToRemote2 send to SN[%s]", remoteSN);
	COMMON_BUF_DEBUG(DEBUG_SHAMANING_PLC "PLC_SendToRemote2 ", buf, toSend);

	int written;
	int plcOutputLen = 30 + toSend;
	unsigned char * plcOutput = (unsigned char *)malloc(plcOutputLen+1);
	if (plcOutput == NULL){
		//debug
		COMMON_STR_DEBUG(DEBUG_SHAMANING_PLC "PLC_SendToRemote2 exit with ERROR_MEM_ERROR");

		res = ERROR_MEM_ERROR;
		return res;
	}

	PLC_IncPackTag();

	//setup return filed of assigned task tag
	(*taskTagAssigned) = (plcPacTag & 0xFFFF);

	//get remote adress as unsigned long
	remAddr = strtoul((char *)remoteSN, NULL, 10);

	//fill packet
	plcOutput[0] = 0xCA;
	plcOutput[1] = ((27+toSend) & 0xFF);
	plcOutput[2] = (((27+toSend) >> 8) & 0xFF);
	
	plcOutput[3] = 0x00;					// request
	plcOutput[4] = 0x60;					// opcode NL packet TX
	plcOutput[5] = 0x04;					// internetworking unicast (long sn)
	plcOutput[6] = 0x01;					// prority
	plcOutput[7] = 0x00;					// ack (without acknolage)
	plcOutput[8] = 0x08;					// max hop count
	plcOutput[9] = 0x07;					// max gain
	plcOutput[10] = (plcPacTag & 0xFF);		// tag lo depending on recieved
	plcOutput[11] = (plcPacTag >> 8) & 0xFF;// tag hi depending on recieved
	plcOutput[12] = 0x00;					// not encrypted
	plcOutput[13] = port;					// port
	
	plcOutput[14] = (remAddr & 0xFF);
	plcOutput[15] = (remAddr >> 8) & 0xFF;
	plcOutput[16] = (remAddr >> 16) & 0xFF;
	plcOutput[17] = (remAddr >> 24) & 0xFF;

	plcOutput[18] = 0x00;
	plcOutput[19] = 0x00;
	plcOutput[20] = 0x00;
	plcOutput[21] = 0x00;
	
	switch (useValidHiSnPart){
		case 0x00:
			plcOutput[22] = 0x00;
			plcOutput[23] = 0x00;
			plcOutput[24] = 0x00;
			plcOutput[25] = 0x00;
			
			plcOutput[26] = 0x00;
			plcOutput[27] = 0x00;
			plcOutput[28] = 0x00;
			plcOutput[29] = 0x00;
			break;

		case 0x52:
			plcOutput[22] = 0x00;
			plcOutput[23] = 0x02;
			plcOutput[24] = 0x52;
			plcOutput[25] = 0x35;
			
			plcOutput[26] = 0x00;
			plcOutput[27] = 0x00;
			plcOutput[28] = 0x01;
			plcOutput[29] = 0x00;
			break;

		case 0x57:
			plcOutput[22] = 0x00;
			plcOutput[23] = 0x02;
			plcOutput[24] = 0x57;
			plcOutput[25] = 0x35;
			
			plcOutput[26] = 0x00;
			plcOutput[27] = 0x00;
			plcOutput[28] = 0x01;
			plcOutput[29] = 0x00;
			break;

		case 0x78:
			plcOutput[22] = 0x00;
			plcOutput[23] = 0x02;
			plcOutput[24] = 0x78;
			plcOutput[25] = 0x35;
			
			plcOutput[26] = 0x00;
			plcOutput[27] = 0x00;
			plcOutput[28] = 0x01;
			plcOutput[29] = 0x00;
			break;
		
	}
	
	memcpy(&plcOutput[30], buf, toSend);    //data

	plcOutput[plcOutputLen] = IT700_GetCrc(plcOutput, plcOutputLen);
	plcOutputLen++;

	//reset responces indexes
	plcBase.responce1 = 0xff;
	plcBase.responce2 = 0xff;

	//send
	res = SERIAL_Write(PORT_PLC, plcOutput, plcOutputLen, &written);
	if ((plcOutputLen != written) && (res == OK)){
		res = ERROR_SIZE_WRITE;
	}

	//
	free (plcOutput);
	plcOutput = NULL;

	COMMON_STR_DEBUG(DEBUG_SHAMANING_PLC "PLC_SendToRemote2 exit");

	return res;
}


//
//
//
int PLC_LeaveNetworkIndividual(counter_t * counter){
	int res = ERROR_PLC;
	unsigned char leaveNetCmdPaylod[6];
	int assignedTag = 0;
	
	//debug
	COMMON_STR_DEBUG(DEBUG_POOL_PLC "PLC SEND INDIVIDUAL LEAVE NET FOR [%s]", counter->serialRemote);
	
	//fill leave net command
	leaveNetCmdPaylod[0] = 0xCA ;
	leaveNetCmdPaylod[1] = 0x02 ;
	leaveNetCmdPaylod[2] = 0x00 ;
	leaveNetCmdPaylod[3] = 0x00 ;
	leaveNetCmdPaylod[4] = 0xA6 ;
	leaveNetCmdPaylod[5] = 0xA8 ;


	switch (counter->pollAlternativeAlgo){
		case POOL_ALTER_NEED_00_LEAVE:
			counter->pollAlternativeAlgo = POOL_ALTER_NEED_52_LEAVE;
			counter->lastTaskRepeateFlag = 1;
			res = PLC_SendToRemote2(counter->serialRemote, leaveNetCmdPaylod, 6, 1, 0x00, &assignedTag);
			break;
			
		case POOL_ALTER_NEED_52_LEAVE:
			counter->pollAlternativeAlgo = POOL_ALTER_NEED_57_LEAVE;
			counter->lastTaskRepeateFlag = 1;
			res = PLC_SendToRemote2(counter->serialRemote, leaveNetCmdPaylod, 6, 1, 0x52, &assignedTag);
			break;
			
		case POOL_ALTER_NEED_57_LEAVE:
			counter->pollAlternativeAlgo = POOL_ALTER_NEED_78_LEAVE;
			counter->lastTaskRepeateFlag = 1;
			res = PLC_SendToRemote2(counter->serialRemote, leaveNetCmdPaylod, 6, 1, 0x57, &assignedTag);
			break;
			
		case POOL_ALTER_NEED_78_LEAVE:
			counter->pollAlternativeAlgo = POOL_ALTER_NOT_SET;
			counter->lastTaskRepeateFlag = 0;
			res = PLC_SendToRemote2(counter->serialRemote, leaveNetCmdPaylod, 6, 1, 0x78, &assignedTag);
			break;
	}

	//setup last leave net time stamp
	counter->lastLeaveCmd = time(NULL);

	//journal event
	unsigned char evDesc[EVENT_DESC_SIZE];
	memset(evDesc , 0 , EVENT_DESC_SIZE);
	COMMON_TranslateLongToChar(counter->dbId, evDesc);
	STORAGE_JournalUspdEvent(EVENT_PLC_TABLE_ALL_LEAVE_NET , evDesc);
	
	return res;
}

const char * setErrorChArr[] = {"No", "Yes"};

//
//
//
void PLC_SetupResponce(char responceIndex){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int taskIndex = -1;
	int transactionIndex = 0;
	int setError = 0;
	int responceData = 0;
	BOOL processNextSteep = FALSE;

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupResponce try enter");

	//protect plc
	PLC_Protect();

	//check if responce is error, so set error to task result
	switch (responceIndex){
		case 1:
			responceData = plcBase.responce1;
			break;

		case 2:
			responceData = plcBase.responce2;
			break;
	}

	//unprotect
	PLC_Unprotect();

	//enter cs
	PLC_TaskProtect();
		
	res = PLC_GetTaskIndexByTagNoAtom(plcBase.responceTag, &taskIndex);
	if ( res == OK ){

		//check responce data
		if (responceData != 0x0) {
			//get  transaction
			transactionIndex = plcBase.plcTask.tasks[taskIndex]->currentTransaction;
			//set error on transaction result
			plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_RESPONSE;
			//set error for debug output
			setError = 1;

		} else {
			//update wait time
			plcBase.plcTask.tasks[taskIndex]->lastAction = time(NULL);

			//update response count
			plcBase.plcTask.tasks[taskIndex]->responsesCout++;

			//osipov - gubanov group commands have no answers
			if (plcBase.plcTask.tasks[taskIndex]->responsesCout == 2){
				switch (plcBase.plcTask.tasks[taskIndex]->counterType){
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
					case TYPE_GUBANOV_MAYK_301:{
							//if current transaction have group commands, we don't need to wait anymore answer
							transactionIndex = plcBase.plcTask.tasks[taskIndex]->currentTransaction;
							if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize == 0){
								//so done transaction and go to next
								plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
								plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->tStop = time(NULL);
								//for next step
								processNextSteep = TRUE;
							}
						}
						break;
				}
			}
		}

	}

	//leave cs
	PLC_TaskUnprotect();


	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupResponce (responce [%d] data[%d], set error responce [%s]) pre exit", responceIndex, responceData, setErrorChArr[setError]);

	//osipov - gubanov group commands can have no answers, so we can continue sends from this place
	if ((res == OK) && (processNextSteep == TRUE))
	{
		//sleep between commands sends
		COMMON_Sleep(SECOND);

		//go to call next step of task for this modem (by serial)
		res = PLC_NextStepOfTask();
	}

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupResponce exit");
}

//
//
//
int PLC_SetupAnswer(unsigned long remoteId, unsigned char * plcPayload, unsigned int plcPayloadSize){
	DEB_TRACE(DEB_IDX_PLC)

	int res = OK;
	int currentSize = 0;
	int transactionIndex = 0;
	unsigned int taskIndex = 0;
	BOOL processNextSteep = FALSE;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer try enter");

	
	//check poll in transparent mode
	if (POOL_GetTransparentPlc() == TRUE){
		POOL_SetTransparentPlcAnswer(remoteId, plcPayload, plcPayloadSize);
		return OK ;
	}

	//enter cs
	PLC_TaskProtect();

	//COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer recv from ID [%d] pacTag[0x%04x]", remoteId, pacTag);
	COMMON_BUF_DEBUG(DEBUG_PLC "PLC_SetupAnswer ", plcPayload, plcPayloadSize);

	//if we have task allocated
	if (plcBase.plcTask.tasksCount > 0){

			//check tag associated with current command on the selected task
			if (plcBase.plcTask.tasks[taskIndex]->responsesCout == 2)
			{
				//get transaction index
				transactionIndex = plcBase.plcTask.tasks[taskIndex]->currentTransaction;

				//check previously parts was not change result with errors or other values
				if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result == TRANSACTION_NO_RESULT)
				{
					//get current size of current answer
					currentSize = plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize;

					//reallocate answer array to new size
					plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer = (unsigned char *)realloc(plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer, ((currentSize + plcPayloadSize) * sizeof(unsigned char)));
					if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer != NULL)
					{
						//copy part of payload if it present
						if (plcPayloadSize > 0){
							memcpy(&plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[currentSize], plcPayload, plcPayloadSize);

							//update last action time for task if some part of answer is placed to buffer
							plcBase.plcTask.tasks[taskIndex]->lastAction = time(NULL);
						}

						//setup answer size
						plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize+=plcPayloadSize;
						plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->tStop = time(NULL);
						currentSize = plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize;

						//check size of answer that we was waiting
						switch (plcBase.plcTask.tasks[taskIndex]->counterType){
							//case TYPE_MAYK_PR_301:
							case TYPE_MAYK_MAYK_302:
							case TYPE_MAYK_MAYK_101:
							case TYPE_MAYK_MAYK_103:{
							
								//check we have all 7E bounds
								if (currentSize >= 10){
									if (((plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[0] == 0x7E) && (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[currentSize-1] == 0x7E))){
										plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
										processNextSteep = TRUE;
									}
								}
							}
							break;

							case TYPE_COUNTER_UNKNOWN:
							{
								int unknownId = -1;

								if (STORAGE_GetUnknownId(plcBase.plcTask.tasks[taskIndex]->counterDbId, &unknownId) == OK){
									switch (unknownId)
									{
										case UNKNOWN_ID_MAYK:
										{
											//check we have all 7E bounds
											if (currentSize >= 10){
												if ((plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[0] == 0x7E) && (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[currentSize-1]== 0x7E)){
													plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
													processNextSteep = TRUE;
												}
											}
										}
										break;

										default:
										{
											//printf("\n\nPLC wait for [%d] bytes but received [%d]!!!!!\n\n", plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize , plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize );
											//check result of size
											if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize == plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize){
												plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
												processNextSteep = TRUE;
											} else {
												plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
											}
										}
										break;
									}
								}
								else
								{
									res = ERROR_GENERAL ;
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_A;
								}
							}
							break;

							//ujuk
							case  TYPE_UJUK_SEB_1TM_01A:
							case  TYPE_UJUK_SEB_1TM_02A:
							case  TYPE_UJUK_SET_4TM_01A:
							case  TYPE_UJUK_SET_4TM_02A:
							case  TYPE_UJUK_SET_4TM_03A:
							case  TYPE_UJUK_PSH_3TM_05A:
							case  TYPE_UJUK_PSH_4TM_05A:
							case  TYPE_UJUK_SEO_1_16:
							case  TYPE_UJUK_PSH_4TM_05MB:
							case  TYPE_UJUK_PSH_3TM_05MB:
							case  TYPE_UJUK_PSH_4TM_05MD:
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
								//check result of size
								if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize == plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize){
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
									processNextSteep = TRUE;
								} else {
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;

									//check short answer possible flag
									if ( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->shortAnswerIsPossible == TRUE )
									{
										if ( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize > 0 )
										{
											unsigned int waitLength = 4 ;
											if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
											{
												waitLength = 8 ;
											}

											if ( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize == waitLength )
											{
												//check answer crc
												unsigned int expectedCrc = UJUK_GetCRC( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer , waitLength - UJUK_SIZE_OF_CRC );
												expectedCrc &= 0xFFFF ;
												unsigned int realCrc = 0 ;
												memcpy( &realCrc , &plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[ waitLength - UJUK_SIZE_OF_CRC ]  , UJUK_SIZE_OF_CRC );

												if ( expectedCrc == realCrc )
												{
													processNextSteep = TRUE;
													plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
													plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->shortAnswerFlag = TRUE ;
												}
											}
										}

									}
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
								if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize == plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize){
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
									processNextSteep = TRUE;
								} else {
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;

									//check short answer possible flag
									if ( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->shortAnswerIsPossible == TRUE )
									{

										//                           012345678
										//osipov: why great than 9 ? ~AAAcNCCr ...ok, really 9 symbols, but why GREAT, not GREAT EQUALS?
										//

										//
										//ukhov: because only additional list commands (B1) can return short answer and they have next format:
										//                      ~AAA0CNCcr      - 10 symbols (gubanov commands)
										//                      ~AAA0CNPCcr     - 11 symbols (asshole shagin commands)
										//                      ~AAA0CNPpCcr    - 12 symbols (asshole shagin commands)
										//      general list commands (A1,A2) couldn't return short answer

										if ( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize > 9 )
										{                                        
											if ( (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[0] == '~') &&
												 (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[ plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize - 1 ] == '\r') )
											{
												//check answer crc
												unsigned char expectedCrc[ GUBANOV_SIZE_OF_CRC ];
												memset( expectedCrc , 0 , GUBANOV_SIZE_OF_CRC );
												GUBANOV_GetCRC( plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer , plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize - GUBANOV_SIZE_OF_CRC - 1, &expectedCrc[ 0 ] );
												if ( !memcmp( &plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answer[ plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize - GUBANOV_SIZE_OF_CRC - 1] , expectedCrc , GUBANOV_SIZE_OF_CRC) )
												{
													processNextSteep = TRUE;
													plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
													plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->shortAnswerFlag = TRUE ;
												}
											}
										}
									}
								}
							}
							break;

							default:{
								//check result of size
								if (plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize == plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->answerSize){
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_OK;
									processNextSteep = TRUE;
								} else {
									plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_SIZE;
								}
							}
							break;
						}

					}
					else
					{
						plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result = TRANSACTION_DONE_ERROR_A;
						res = ERROR_MEM_ERROR;
					}
				}

				//debug
				COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer(wait size: %d, recieve %d bytes)", plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->waitSize, plcPayloadSize);
				COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer([SN %s]) %s / transaction: %s", plcBase.plcTask.tasks[taskIndex]->associatedSerial, getErrorCode(res), getTransactionResult(plcBase.plcTask.tasks[taskIndex]->transactions[transactionIndex]->result));

				//update time action
				if (res == OK)
				{
					time(&plcBase.plcTask.tasks[taskIndex]->lastAction);
				}

			} else {
				PLC_ErrorIncrement();
				COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer, too small count of responces to setup answer [count %d]", plcBase.plcTask.tasks[taskIndex]->responsesCout);
			}
	}

	//leave cs
	PLC_TaskUnprotect();

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer pre exit");

	if ((res == OK) && (processNextSteep == TRUE))
	{
		//reset errors
		PLC_ErrorReset();

		//go to call next step of task for this modem (by serial)
		res = PLC_NextStepOfTask();
	}

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_SetupAnswer exit");

	return res;

}

//
//
//
int PLC_INFO_TABLE_GetRemotesCount(){
	DEB_TRACE(DEB_IDX_PLC)

	int count = 0;

	PLC_Protect();

	count = plcBase.nodesCount;

	PLC_Unprotect();

	return count;
}

//
//
//
int PLC_INFO_TABLE_GetRemoteIdBySn(unsigned char * serial, unsigned int * remoteId){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int remoteIndex = 0;
	unsigned long sn1 = strtoul((char*)serial, NULL, 10);
	unsigned long sn2 = 0;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_GetRemoteIdBySn enter");

	PLC_Protect();

	for (;remoteIndex < plcBase.nodesCount; remoteIndex++){
		sn2 =  strtoul((char *)plcBase.nodes[remoteIndex].serial, NULL, 10);
		if(sn1 == sn2){
			*remoteId = plcBase.nodes[remoteIndex].did;
			res = OK;
			break;
		}
	}

	PLC_Unprotect();

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_GetRemoteIdBySn exit");

	return res;
}

//
//
//
int PLC_INFO_TABLE_GetRemoteSnById(unsigned int remoteId, unsigned char ** serial){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int remoteIndex = 0;

	PLC_Protect();

	for (;remoteIndex < plcBase.nodesCount; remoteIndex++){
		if(remoteId == plcBase.nodes[remoteIndex].did){
			(*serial) = plcBase.nodes[remoteIndex].serial;
			res = OK;
			break;
		}
	}

	PLC_Unprotect();

	return res;

}

//
//
//
int PLC_INFO_TABLE_GetRemoteNodeByIndex(unsigned int remoteIndex, plcNode_t * node){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;

	PLC_Protect();

	if ((int)remoteIndex < plcBase.nodesCount){
		memcpy(node, &plcBase.nodes[remoteIndex], sizeof(plcNode_t));
		res = OK;
	}

	PLC_Unprotect();

	return res;
}

//
//
//
int PLC_INFO_TABLE_GetRemoteNodeBySn(unsigned char * serial, plcNode_t * node){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int remoteIndex = 0;
	unsigned long sn1 = strtoul((char*)serial, NULL, 10);
	unsigned long sn2 = 0;

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_GetRemoteNodeBySn enter");

	PLC_Protect();

	for (;remoteIndex < plcBase.nodesCount; remoteIndex++){
		sn2 =  strtoul((char *)plcBase.nodes[remoteIndex].serial, NULL, 10);
		if(sn1 == sn2){
			memcpy(node, &plcBase.nodes[remoteIndex], sizeof(plcNode_t));
			res = OK;
			break;
		}
	}

	PLC_Unprotect();

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_GetRemoteNodeBySn exit");

	return res;
}

//
//
//
int PLC_INFO_TABLE_GetRemoteSerialByIndex(unsigned int remoteIndex, unsigned char ** serial){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;

	PLC_Protect();

	if ((int)remoteIndex < plcBase.nodesCount){
		(*serial) = plcBase.nodes[remoteIndex].serial;
		res = OK;
	}

	PLC_Unprotect();

	return res;
}

//
//
//
int PLC_INFO_TABLE_DeleteNode(unsigned int nodeId){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int remoteIndex = 0;
	int newIndex = 0;

	plcNode_t * oldPtr = NULL;
	plcNode_t * newPtr = NULL;

	PLC_Protect();

	//check count
	if (plcBase.nodesCount > 1){

		//set pointers
		oldPtr = plcBase.nodes;
		newPtr = (plcNode_t *)malloc(sizeof(plcNode_t)*(plcBase.nodesCount-1));
		if (newPtr != NULL){
			//shift nodes
			for (;remoteIndex < plcBase.nodesCount; remoteIndex++){
				if(oldPtr[remoteIndex].did != nodeId){
					memcpy(&newPtr[newIndex], &oldPtr[remoteIndex], sizeof(plcNode_t));
					newIndex++;

				} else {
					res = OK;
				}
			}

			//decrease size
			plcBase.nodesCount--;

			//set new Ptr array
			plcBase.nodes = newPtr;

			//free oldPtr array
			free (oldPtr);
			oldPtr = NULL;

		} else {
			res = ERROR_MEM_ERROR;
		}

	} else {
		res = PLC_INFO_TABLE_Erase();
	}

	PLC_Unprotect();

	return res;
}

//
//
//
int PLC_INFO_TABLE_Erase(){
	DEB_TRACE(DEB_IDX_PLC)

	int res = OK;

	PLC_Protect();

	if (plcBase.nodes != NULL)
		free(plcBase.nodes);
	plcBase.nodes = NULL;
	plcBase.nodesCount = 0;

	PLC_Unprotect();

	return res;
}

//
//
//
int PLC_INFO_TABLE_UpdateRemoteNode(plcNode_t * someNode, int * agedCounters){
	DEB_TRACE(DEB_IDX_PLC)

	int res = ERROR_IS_NOT_FOUND;
	int remoteIndex = 0;
	unsigned long sn1 = strtoul ((char *)someNode->serial, NULL, 10);
	unsigned long sn2 = 0;

	if (sn1 == 0){
		return OK; //fix NC behaviour after clenup SN table
	}

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_UpdateRemoteNode enter");

	PLC_Protect();

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "check node [%s]", someNode->serial);
	COMMON_STR_DEBUG(DEBUG_PLC "nodes count = %d", plcBase.nodesCount);

	for (;remoteIndex < plcBase.nodesCount; remoteIndex++){

		sn2 = strtoul ((char *)plcBase.nodes[remoteIndex].serial, NULL, 10);
		if(sn2 == sn1){

			//debug
			COMMON_STR_DEBUG(DEBUG_PLC "node[ %s ] is present, update", plcBase.nodes[remoteIndex].serial);

			//setup aged flag if age is updated and not in transparent plc mode
			if (POOL_GetTransparentPlc() == FALSE){
				if (someNode->age > plcBase.nodes[remoteIndex].age){
					STORAGE_SetPlcCounterAged(plcBase.nodes[remoteIndex].serial);
					(*agedCounters)++;
				}
			}
			
			plcBase.nodes[remoteIndex].age = someNode->age;
			plcBase.nodes[remoteIndex].pid = someNode->pid;
			plcBase.nodes[remoteIndex].did = someNode->did;
			res = OK;
			snTableFullRead = TRUE; // if first updated is occured, so sn table was completetlly reread
			break;
		}
	}

	//this is new node, append it
	if (res != OK){

		//debug
		COMMON_STR_DEBUG(DEBUG_PLC "node[ %s ] is not present, append", someNode->serial);

		plcBase.nodes = (plcNode_t *)realloc(plcBase.nodes, (plcBase.nodesCount+1)*sizeof(plcNode_t));
		if (plcBase.nodes != NULL){
			memset(&plcBase.nodes[plcBase.nodesCount], 0, sizeof(plcNode_t));
			memcpy(&plcBase.nodes[plcBase.nodesCount], someNode, sizeof(plcNode_t));
			plcBase.nodesCount++;
			res = OK;
		}

		if (plcBase.settings.admissionMode == PLC_ADMISSION_WITH_DETECT){
			//debug
			COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "ADD NEW NODE TO CONFIG: %s", someNode->serial);

			// add new counter to config
			STORAGE_AddPlcCounterToConfiguration( someNode->serial );
		}
	}

	PLC_Unprotect();

	COMMON_STR_DEBUG(DEBUG_PLC "PLC_INFO_TABLE_UpdateRemoteNode exit");

	return res;
}



//
//
//
int PLC_GetNetowrkId(){
	DEB_TRACE(DEB_IDX_PLC)

	return plcBase.networkId;
}

//
//
//

unsigned char PLC_GetNetworkMode()
{
	DEB_TRACE(DEB_IDX_PLC)
	unsigned char mode = 0xFF;

	PLC_Protect();
	mode = plcBase.settings.admissionMode;
	PLC_Unprotect();

	return mode ;
}

//
//
//
void PLC_ProcessIncomingPacket(unsigned char plcPacketType, unsigned char  plcPacketCode, unsigned char * plcPayload, unsigned int plcPacketLen) {
	DEB_TRACE(DEB_IDX_PLC)

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC process incoming packet type[0x%02x], opcode [0x%02x]", plcPacketType, plcPacketCode);
	COMMON_BUF_DEBUG(DEBUG_PLC "PLC payload ", plcPayload, plcPacketLen);

	//process packet
	switch (plcPacketCode){
		case 0x68: //input data indication

			//if intranet packet
			if ((plcPayload[4] == 0x8) ||
				(plcPayload[4] == 0x9) ||
				(plcPayload[4] == 0xA) || //internetworking
				(plcPayload[4] == 0xB) ){

				//if not broadcast
				if ((plcPayload[1] == 0x00) && (plcPayload[0] == 0x02)){ //unicast and for me

					//if with short Id
					if (plcPayload[15] == 0){

						int senderId = plcPayload[16] + ((plcPayload[17] << 8) & 0xFF00);
						if ((plcPacketLen-21) > 0){
							PLC_SetupAnswer(senderId, &plcPayload[21], (plcPacketLen-21));
						} else {

							COMMON_STR_DEBUG(DEBUG_PLC "PLC payload len error");
						}
					}
				}
			}
			else if (plcPayload[4] == 0x10) //intranetworking
			{
				if ((plcPayload[1] == 0x00) && (plcPayload[0] == 0x02)) //unicast and for me
				{
					unsigned long senderId = 0;
					senderId+=plcPayload[9];
					senderId+=(plcPayload[10]<<8);
					senderId+=(plcPayload[11]<<16);
					senderId+=(plcPayload[12]<<24);
					if ((plcPacketLen-26) > 0){
						PLC_SetupAnswer(senderId, &plcPayload[26], (plcPacketLen-26));
					} else {

						COMMON_STR_DEBUG(DEBUG_PLC "PLC payload len error");
					}
				}
			}
			break;

		case 0x60: //responces after sending answer
			if (plcPayload[1] == 0x1){
				PLC_Protect();
				plcBase.responce1 = plcPayload[2];
				plcBase.responceTag = plcPayload[3] + ((plcPayload[4] << 8) & 0xFF00);
				PLC_Unprotect();
				PLC_SetupResponce(1);
			}

			if (plcPayload[1] == 0x3){
				PLC_Protect();
				plcBase.responce2 = plcPayload[2];
				plcBase.responceTag = plcPayload[5] + ((plcPayload[6] << 8) & 0xFF00);
				PLC_Unprotect();
				PLC_SetupResponce(2);
			}
			break;

		case 0xA4: //admission result
			if (plcPayload[0] == 1){
				COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "IT700_SendAdmissionResponce OK (async)");
			}
			break;

		case 0xBA: //sat connected to NC (possible only remote indication)
			{
			#if !REV_COMPILE_RELEASE
				int nodeId = plcPayload[2] + ((plcPayload[3] << 8) & 0xFF00);
				COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "NEW CONNECTION TO NC (0xBA): node id = %u", nodeId);
			#endif
			}
			break;

		case 0xBE: //sat connected
			{
			#if !REV_COMPILE_RELEASE
				int nodeId = plcPayload[0] + ((plcPayload[1] << 8) & 0xFF00);
				COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "NEW CONNECTION TO NC (0xBE): node id = %u", nodeId);
			#endif
			}
			break;

		case 0xB1: //sat connection status
			{
			#if !REV_COMPILE_RELEASE
				int nodeId = plcPayload[0] + ((plcPayload[1] << 8) & 0xFF00);
				int sn = plcPayload[2] + (plcPayload[3] << 8) + (plcPayload[4] << 16) + (plcPayload[5] << 24);
				int state = plcPayload[18];
				COMMON_STR_DEBUG(DEBUG_IT700_APPROVE "NEW CONNECTION TO NC (0xB1): node id = %u, sn=%u, state=%u", nodeId, sn, state);
			#endif
			}
			break;

		case 0xB3:
			break;

		case 0xBF: //network ID assigned
			plcBase.networkId = plcPayload[0] + ((plcPayload[1] << 8) & 0xFF00);
			break;

		case 0xB8: //network connection event, admission
			if (PLC_GetNetworkMode() == PLC_ADMISSION_APPLICATION){
				COMMON_BUF_DEBUG(DEBUG_SHAMANING_PLC "DATA: " , plcPayload , 29);
				IT700_Admission(plcPayload);				
			}
			break;

		case 0x43: // save parameters
		case 0x41: // set parameters
			plcBase.paramResponce = plcPayload[0];
			break;

		case 0x42: //get parameter value
			plcBase.paramResponce = plcPayload[0];
			plcBase.paramValueI = (plcPayload[1] & 0xFF) + ((plcPayload[2] <<8) & 0xFF00);
			if ((plcPacketLen-1) > 0){
				int copySz = plcPacketLen-1;
				if (copySz <= 32){ //copy to var len
					memcpy(plcBase.paramValueV, &plcPayload[1], copySz);
				}
			}
			break;

		case 0x20: // reset was recieved
			COMMON_STR_DEBUG(DEBUG_PLC "IT700 EXTERNAL RESET RECIEVED, STACK RESP: %x", plcPayload[0]);
			PLC_Protect();
			plcBase.externalReset = TRUE;
			plcBase.stackResponce = plcPayload[0];
			PLC_Unprotect();
			break;

		case 0x21: // init
		case 0x22: // go online answer
			plcBase.stackResponce = plcPayload[0];
			break;

		case 0xA2: // delete node or table
			plcBase.snTableResponce = plcPayload[0];
			break;

		case 0x65: // sn table size
			plcBase.snTableResponce = plcPayload[0];
			plcBase.paramValueI = (plcPayload[3] & 0xFF) + ((plcPayload[4] <<8) & 0xFF00);
			break;

		case 0x63: //table entry ID
			plcBase.snTableResponce = plcPayload[0];
			plcBase.entryNodeDid = (plcPayload[2] & 0xFF) + ((plcPayload[3] <<8) & 0xFF00);
			plcBase.entryNodePid = (plcPayload[4] & 0xFF) + ((plcPayload[5] <<8) & 0xFF00);
			plcBase.entryNodeAge = (plcPayload[6] & 0xFF);
			break;

		case 0xA3://table entry SN
			plcBase.snTableResponce = plcPayload[0];
			plcBase.entryNodeSn = (plcPayload[1] & 0xFF) + ((plcPayload[2] <<8) & 0xFF00) + ((plcPayload[3] <<16) & 0xFF0000) + ((plcPayload[4] <<24) & 0xFF000000);
			break;

		default:
			break;
	}
}

//
// thread foo of plc, executed in cycle
//
void * PLC_Do(void * param __attribute__((unused))){
	DEB_TRACE(DEB_IDX_PLC)

	int prevstate;

	unsigned char plcByte = 0;
	unsigned int plcPacketLen = 0;
	unsigned int plcPayloadPtr = 0;
	unsigned char plcPacketType = 0;
	unsigned char plcPacketCode = 0;
	unsigned char plcPacketCrc = 0;
	unsigned char plcPayload[1500];

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC thread started");

	plcThreadOk = TRUE;

	plcReaderState = PROTOCOL_PLC_CA_BYTE;

	while (plcThreadOk){

		if (plcExecution == TRUE){
			SVISOR_THREAD_SEND(THREAD_PLC);

			int bytesRead = 0;
			int res = SERIAL_Read(PORT_PLC, &plcByte, 1, &bytesRead);

			plcExecutionSuspend = FALSE;

			if (res == OK)
			{
				if (bytesRead > 0)
				{
					switch (plcReaderState)
					{
						case PROTOCOL_PLC_CA_BYTE:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "\nPROTOCOL_PLC_CA_BYTE\t 0x%02x", plcByte);
							}

							if (plcByte == PLC_SEQ_BYTE_RUN){
								plcPacketCrc = 0;
								plcPacketLen = 0;
								prevstate = plcReaderState;
								plcReaderState = PROTOCOL_PLC_LEN_LO;
							}
							break;

						case PROTOCOL_PLC_LEN_LO:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_LEN_LO\t 0x%02x", plcByte);
							}

							plcPacketLen = plcPacketLen + plcByte;
							plcPacketCrc+=plcByte;
							plcReaderState = PROTOCOL_PLC_LEN_HI;
							break;

						case PROTOCOL_PLC_LEN_HI:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_LEN_HI\t 0x%02x", plcByte);
							}

							plcPayloadPtr = 0;
							plcPacketLen = plcPacketLen + ((plcByte << 8) & 0xFF00);
							plcPacketLen-=2;
							plcPacketCrc+=plcByte;
							plcReaderState = PROTOCOL_PLC_TYPE;
							break;

						case PROTOCOL_PLC_TYPE:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_TYPE\t 0x%02x", plcByte);
							}

							plcPacketType = plcByte;
							plcPacketCrc+=plcByte;
							plcReaderState = PROTOCOL_PLC_OPCODE;
							break;

						case PROTOCOL_PLC_OPCODE:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_OPCODE\t 0x%02x", plcByte);
							}

							plcPacketCode = plcByte;
							plcPacketCrc+=plcByte;
							if (plcPacketLen > 0 ) {
								plcReaderState = PROTOCOL_PLC_PAYLOAD;
							} else {
								plcReaderState = PROTOCOL_PLC_CRC;
							}

							break;

						case PROTOCOL_PLC_PAYLOAD:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_PAYLOAD\t 0x%02x", plcByte);
							}

							plcPayload[plcPayloadPtr] = plcByte;
							plcPacketCrc+=plcByte;
							plcPayloadPtr++;

							if (plcPayloadPtr == plcPacketLen)
								plcReaderState = PROTOCOL_PLC_CRC;

							break;

						case PROTOCOL_PLC_CRC:
							if (prevstate != plcReaderState)
							{
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC_CRC\t 0x%02x", plcByte);
							}

							if (plcPacketCrc == plcByte) {
								PLC_ProcessIncomingPacket(plcPacketType, plcPacketCode, plcPayload, plcPacketLen);

							} else {
								COMMON_STR_DEBUG(DEBUG_IT700_PROTO "crc bad, skip packet processing");
							}

							prevstate = plcReaderState;
							plcReaderState = PROTOCOL_PLC_CA_BYTE;
							break;
					}
				} else { //no bytes read
					COMMON_Sleep(THREAD_SLEEPS_DELAY);
				}
			}
			else
			{
				//handle serial ERROR_GENERAL
				COMMON_STR_DEBUG(DEBUG_IT700_PROTO "PROTOCOL_PLC read error");
				plcErrorsCounter++;
				plcReaderState = PROTOCOL_PLC_CA_BYTE;
			}

		} else { //no plc listener
			COMMON_Sleep(DELAY_BEETWEN_CHECK_TASK_STATE);
			plcExecutionSuspend = TRUE;
			plcReaderState = PROTOCOL_PLC_CA_BYTE;
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_PLC "PLC therad quit");

	plcThreadOk = FALSE;

	return NULL ;
}
