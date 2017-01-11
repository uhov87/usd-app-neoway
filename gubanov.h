/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_GUBANOV_H
#define __ZIF_GUBANOV_H

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

BOOL GUBANOV_IsCounterAbleSkill(const counter_t * counter, BYTE skill, int * rVariantIdx);
int GUBANOV_GetMaxDayMeteragesDeep(const counter_t * counter);

void GUBANOV_GetCRC(unsigned char * cmd, unsigned int size, unsigned char * crc);
int GUBANOV_GetProfileIndex(dateTimeStamp * dtsToRequest );
void GUBANOV_GetCounterAddress(counter_t * counter, unsigned char * counterAddress);
unsigned char GUBANOV_GetStartCommandSymbol( counter_t * counter , BOOL broadcastCommand );

int GUBANOV_Command_CurrentMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex);
int GUBANOV_Command_DayMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex , int dayDiff);
int GUBANOV_Command_MonthMeterage(counter_t * counter, counterTransaction_t ** transaction , dateTimeStamp * dtsToRequest , unsigned char tarifIndex);
int GUBANOV_Command_GetProfile(counter_t * counter, counterTransaction_t ** transaction , dateTimeStamp * dtsToRequest);
int GUBANOV_Command_GetPqp_ActivePower(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_GetSimpleJournal_BoxOpen(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_GetSimpleJournal_PowerSwitchOn(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_GetSimpleJournal_PowerSwitchOff(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_GetSimpleJournal_TimeSynced(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_PowerControl(counter_t * counter, counterTransaction_t ** transaction , BOOL releState );
int GUBANOV_Command_SetPowerLimit(counter_t * counter, counterTransaction_t ** transaction , int limit );
int GUBANOV_Command_SetConsumerCategory(counter_t * counter, counterTransaction_t ** transaction );
int GUBANOV_Command_SetMonthLimit(counter_t * counter, counterTransaction_t ** transaction , int limit);
int GUBANOV_Command_SetTariffDayTemplate(counter_t * counter, counterTransaction_t ** transaction , int dayTemplate );
int GUBANOV_Command_SetTariff_1_StartTime(counter_t * counter, counterTransaction_t ** transaction , gubanovTariff_t * gubanovTariff );
int GUBANOV_Command_SetTariff_2_StartTime(counter_t * counter, counterTransaction_t ** transaction , gubanovTariff_t * gubanovTariff );
int GUBANOV_Command_SetTariff_3_StartTime(counter_t * counter, counterTransaction_t ** transaction , gubanovTariff_t * gubanovTariff );
int GUBANOV_Command_SetTariffHoliday(counter_t * counter, counterTransaction_t ** transaction , gubanovHoliday_t * gubanovHoliday );
int GUBANOV_Command_SetIndicationSimple(counter_t * counter, counterTransaction_t ** transaction , gubanovIndication_t * gubanovIndication );
int GUBANOV_Command_SetIndicationDuration(counter_t * counter, counterTransaction_t ** transaction , int duration );
int GUBANOV_Command_SetIndicationImproved(counter_t * counter, counterTransaction_t ** transaction , int loopIndex , unsigned int loopMask );

int GUBANOV_GetSimpleJournal(counter_t * counter, counterTask_t ** counterTask);
int GUBANOV_GetImprovedJournal(counter_t * counter, counterTask_t ** counterTask);

int GUBANOV_WriteTariff(counter_t * counter, counterTask_t ** counterTask ,unsigned char * tariff, int tariffLength);
int GUBANOV_WritePSM(counter_t * counter, counterTask_t ** counterTask ,unsigned char * psmRules, int psmRulesLength);

int GUBANOV_CreateCounterTask(counter_t  * counter, counterTask_t ** counterTask);
int GUBANOV_SaveTaskResults(counterTask_t * counterTask);

int GUBANOV_UnknownCounterIdentification( unsigned char * counterType , unsigned char * version );


#ifdef __cplusplus
}
#endif

#endif

//EOF

