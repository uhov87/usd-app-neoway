/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_MAYK_H
#define __ZIF_MAYK_H

#include "boolType.h"
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

int MAYK_GetDateAndTime(counter_t * counter, counterTransaction_t ** transaction);
//int MAYK_GetType(counter_t * counter, counterTransaction_t ** transaction);
int MAYK_GetRatio(counter_t * counter , counterTransaction_t ** transaction);
int MAYK_GetCurrentMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tarifIndex);



int MAYK_GetDayMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tariffMask ,dateStamp * date);
int MAYK_GetMonthMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tariffMask , dateStamp * date);



int MAYK_GetProfileValue(counter_t * counter, counterTransaction_t ** transaction , dateTimeStamp * dts);



int MAYK_SyncTimeHardy(counter_t * counter, counterTransaction_t ** transaction );

//int MAYK_GetJournal(counter_t * counter, counterTransaction_t ** transaction );

int MAYK_AskEventsNumberInCurrentJournal(counter_t * counter, counterTransaction_t ** transaction , int journalNumber);
int MAYK_GetNextEventFromCurrentJournal(counter_t * counter, counterTransaction_t ** transaction , int journalNumber , int eventNumber );


int MAYK_GetPowerQualityParametres(counter_t * counter, counterTransaction_t ** transaction , int pqpPart);

int MAYK_CmdSetPowerControls( counter_t * counter, counterTransaction_t ** transaction , int offset , unsigned char * controls , int size );
int MAYK_CmdSetPowerControlsDefault( counter_t * counter, counterTransaction_t ** transaction  );
int MAYK_CmdSetPowerControlsDone( counter_t * counter, counterTransaction_t ** transaction  );
int MAYK_CmdSetPowerModuleSwitcherState( counter_t * counter, counterTransaction_t ** transaction , int state  );

int MAYK_WriteTariff( counter_t * counter , counterTask_t ** counterTask , unsigned char * tariff , int tariffLength) ;


int MAYK_BeginWritingTariffMap(counter_t * counter, counterTransaction_t ** transaction);
int MAYK_EndWritingTariffMap(counter_t * counter, counterTransaction_t ** transaction);
int MAYK_WritingTariffMap_STT(counter_t * counter, counterTransaction_t ** transaction , maykTariffSTT_t * stt, int sttIndex);
int MAYK_WritingTariffMap_WTT(counter_t * counter, counterTransaction_t ** transaction , maykTariffWTT_t * wtt, int wttIndex);
int MAYK_WritingTariffMap_MTT(counter_t * counter, counterTransaction_t ** transaction , maykTariffMTT_t * mtt);
int MAYK_WritingTariffMap_LWJ(counter_t * counter, counterTransaction_t ** transaction , maykTariffLWJ_t * lwj);

int MAYK_CmdStartWritingIndications(counter_t * counter, counterTransaction_t ** transaction , maykIndication_t * maykIndication);

unsigned char MAYK_GetCRC( unsigned char * cmd , unsigned int size );
void MAYK_Stuffing(unsigned char ** cmd , unsigned int * length );
void MAYK_Gniffuts(unsigned char ** cmd , unsigned int * length );
int  MAYK_AddQuotes(unsigned char ** cmd , unsigned int * length);
int MAYK_RemoveQuotes(unsigned char ** cmd , unsigned int * length );
BOOL MAYK_GetCounterAddress(counter_t * counter, unsigned int * counterAddress);

void MAYK_StepToNextJournal(counter_t * counter);
void MAYK_StepToNextEvent(counter_t * counter);

int MAYK_GetSTT(unsigned char * tariff , int tarifLength , maykTariffSTT_t ** stt , int * sttSize);
int MAYK_GetWTT(unsigned char * tariff , int tarifLength , maykTariffWTT_t ** wtt , int * wttSize);
int MAYK_GetMTT(unsigned char * tariff , int tarifLength , maykTariffMTT_t * mtt);
int MAYK_GetLWJ(unsigned char * tariff , int tarifLength , maykTariffLWJ_t * lwj);
int MAYK_GetIndications(unsigned char counterType , unsigned char * tariff, int tariffLength, maykIndication_t * maykIndication );

#if REV_COMPILE_ASK_MAYK_FRM_VER
int MAYK_CmdGetVersion(counter_t * counter, counterTransaction_t ** transaction);
#endif

unsigned char MAYK_GetJournalPqpEvType(unsigned char value);



void MAYK_FillAsByteArray(int field, unsigned char * data, mayk_PSM_Parametres_t * psmParams);
int MAYK_SetPowerMasks1( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMasks2( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskPq( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskE30( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskEd( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskEm( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
//int MAYK_SetPowerMaskUI( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskUmax( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskUmin( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetPowerMaskImax( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask );
int MAYK_SetTimingsMR_P( counter_t * counter, counterTransaction_t ** transaction , unsigned int tP);
int MAYK_SetTimingsUmin( counter_t * counter, counterTransaction_t ** transaction , unsigned int tUmin);
int MAYK_SetTimingsUmax( counter_t * counter, counterTransaction_t ** transaction , unsigned int tUmax);
int MAYK_SetTimingsDelayOff( counter_t * counter, counterTransaction_t ** transaction , unsigned int tOff);
int MAYK_SetPowerLimitsPq( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits );
int MAYK_SetPowerLimitsE30( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits );
int MAYK_SetPowerLimitsEd( counter_t * counter, counterTransaction_t ** transaction , int tariffIndex , unsigned char * limits );
int MAYK_SetPowerLimitsEm( counter_t * counter, counterTransaction_t ** transaction , int tariffIndex , unsigned char * limits );
int MAYK_SetPowerLimitsUI( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits );
int MAYK_GetReleReason( unsigned char * maskArray, unsigned char * evDesc);

void MAYK_AllocateMemory(unsigned char **ptr , const int newSize);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

int MAYK_CreateCounterTask(counter_t * counter, counterTask_t ** counterTask);
int MAYK_SaveTaskResults(counterTask_t * counterTask);

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

int MAYK_UnknownCounterIdentification( unsigned char * counterType , unsigned char * ver);

#ifdef __cplusplus
}
#endif

#endif

//EOF
