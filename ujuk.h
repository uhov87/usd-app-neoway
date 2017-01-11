/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_UJUK_H
#define __ZIF_UJUK_H

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
unsigned int UJUK_GetCRC(unsigned char * cmd, unsigned int size);
unsigned int UJUK_ByteCRC(unsigned char someByte, unsigned int startCRC);
BOOL UJUK_GetCounterAddress(counter_t * counter, unsigned int * counterAddress);

int UJUK_Command_OpenCounter(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_OpenCounterForWritingToPhMemmory(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_GetDateAndTime(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_GetRatioValue(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_DayEnergy(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char archive);

int UJUK_Command_GetMeteragesByBinaryMask(counter_t * counter, counterTransaction_t ** transaction , unsigned char memoryType , unsigned char monthNumb);

int UJUK_Command_CurrentMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex);
int UJUK_Command_DayMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char archive, unsigned char day);
int UJUK_Command_MonthMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char month);

int UJUK_Command_SetCurrentSeason(counter_t * counter, counterTransaction_t ** transaction , unsigned char season);
int UJUK_Command_SetAllowSeasonChangeFlg(counter_t * counter , counterTransaction_t ** transaction, unsigned char allowSeasonChange );

int UJUK_Command_SetProfileIntervalValue(counter_t * counter, counterTransaction_t ** transaction, unsigned char value);
int UJUK_Command_GetProfileIntervalValue(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_GetProfilePointer(counter_t * counter, counterTransaction_t ** transaction, dateTimeStamp *dts);
int UJUK_Command_GetProfileAllHour(counter_t * counter, counterTransaction_t ** transaction, unsigned int pointer, int hoursNumb);
int UJUK_Command_GetProfileValue(counter_t * counter, counterTransaction_t ** transaction, unsigned int pointer);
//int UJUK_Command_GetProfileHead(counter_t * counter, counterTransaction_t ** transaction);
int UJUK_Command_GetProfileSearchState(counter_t *counter , counterTransaction_t ** transaction);

int UJUK_Command_SetPowerConterolRules(counter_t * counter , counterTransaction_t ** transaction, unsigned char propNumb , unsigned char * tariffNumb , unsigned int * propValue);
int UJUK_Command_SetControlProgrammFlags(counter_t * counter , counterTransaction_t ** transaction, unsigned char propByte );
int UJUK_Command_SetPowerLimitControls(counter_t * counter , counterTransaction_t ** transaction, unsigned int byteArr );

int UJUK_Command_SetIndicationGeneral(counter_t * counter , counterTransaction_t ** transaction , unsigned int param );
int UJUK_Command_SetIndicationExtended(counter_t * counter , counterTransaction_t ** transaction , unsigned int param );
int UJUK_Command_SetIndicationPeriod(counter_t * counter , counterTransaction_t ** transaction , unsigned char period );
int UJUK_Command_SetDynamicIndicationProperties(counter_t * counter , counterTransaction_t ** transaction , unsigned char property );

int UJUK_Command_SetTariffZone(counter_t * counter , counterTransaction_t ** transaction, ujukTariffZone_t * ujukTariffZone , int trId);
int UJUK_Command_GetNextEventFromCurrentJournal(counter_t * counter , counterTransaction_t ** transaction);
int UJUK_Command_GetPowerQualityParametres(counter_t * counter, counterTransaction_t ** transaction , int pqpPart, int phaseNumber);
int UJUK_Command_WriteDataToPhisicalMemory(counter_t * counter , counterTransaction_t ** transaction, unsigned short address, unsigned char * data , int dataLength,unsigned char dataType);
int UJUK_WriteTariffMap(counter_t * counter, counterTask_t ** counterTask, unsigned char * tariff, int tarifLength);
int UJUK_WritePSM(counter_t * counter, counterTask_t ** counterTask ,unsigned char * psmRules, int psmRulesLength , unsigned char counterRatio);

void UJUK_StepToNextJournal(counter_t * counter);
void UJUK_StepToNextEvent(counter_t * counter);
int UJUK_GetProfileMaxPtr(counter_t * counter);
int UJUK_GetMaxProfileEntriesNumber(counter_t * counter);
unsigned int UJUK_GetNextProfileHeadPtr( counter_t * counter , unsigned int lastProfilePtr );

int UJUK_CreateCounterTask(counter_t  * counter, counterTask_t ** counterTask);
int UJUK_SaveTaskResults(counterTask_t * counterTask);

void UJUK_GetTransformationRatioByType(counter_t * counter , int * Kc , int * Ci );
int UJUK_UnknownCounterIdentification( unsigned char * counterType , unsigned char * version );
int UJUK_GetReleReason( unsigned char reason, unsigned char * evDesc);

int UJUK_GetHolidayArray(ujukHolidayArr_t * ujukHolidayArr , unsigned char * tariff , unsigned int tariffSize);
int UJUK_GetMovedDaysList(ujukMovedDaysList_t * ujukMovedDaysList , unsigned char * tariff , int tarifLength);
void UJUK_PrintfTariff(unsigned char * tariff , int tarifLength);
int UJUK_GetTariffMap(ujukTariffMap_t * ujukTariffMap , unsigned char * tariff , int tarifLength , int * tariffNumber);
#ifdef __cplusplus
}
#endif

#endif

//EOF

