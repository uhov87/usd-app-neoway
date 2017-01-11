/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_STORAGE_H
#define __ZIF_STORAGE_H

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
int STORAGE_Initialize();

int STORAGE_LoadConnections();
int STORAGE_LoadServiceConnection();
int STORAGE_LoadCounters485(countersArray_t * countersArray);
int STORAGE_LoadCountersPlc(countersArray_t * countersArray);
unsigned int STORAGE_GetPlcUniqRemotesNumber( countersArray_t * countersArray );
void STORAGE_ClearPlcCounterConfiguration();
int STORAGE_AddPlcCounterToConfiguration(unsigned char * serialRemote);
int STORAGE_SavePlcCounterToConfiguration( counter_t * counter );
BOOL STORAGE_IsCounterPresentInConfiguration(unsigned long serial);
BOOL STORAGE_IsRemotePresentInConfiguration( unsigned char * serialRemote );
void STORAGE_CounterTypeDetection(unsigned long serial , counter_t * counter);
unsigned long STORAGE_GetNextFreeDbId();
int STORAGE_LoadPlcSettings();

int STORAGE_GetCountersNumberInRawConfig(char * buf, unsigned int bufLength);
int STORAGE_CheckConfigPlcSettings(char * buf, unsigned int bufLength, int * firstRetValue, int * secRetValue);
int STORAGE_CheckConfigCountersPlc(char * buf, unsigned int bufLength, int * firstRetValue, int * secRetValue);

BOOL STORAGE_CheckPollingCounterAgainNoAtom(counter_t * counter, int maxPossibleTime);
int STORAGE_GetMaxPossibleTaskTimeFor485();
int STORAGE_GetMaxPossibleTaskTimeForPLC();

BOOL STORAGE_CheckTimeToServiceConnect();
int STORAGE_GetServiceCRC( uspdService_t * uspdService , unsigned int * crc );
int STORAGE_LoadCpin();

int STORAGE_LoadExio();
int STORAGE_GetExioCfg( exioCfg_t ** exioConf );
int STORAGE_GetFirmwareVersion( char * version );
int STORAGE_TranslateFirmwareVersion(char * version, unsigned int * ver_);
int STORAGE_GetUspdTypeAndProtoVersion( unsigned char * type , unsigned int * version);

int STORAGE_UpdateImsi(int sim, char * imsi , BOOL * needToSendSms);

int STORAGE_LoadCounterStatus( unsigned long counterDbId , counterStatus_t * counterStatus );
int STORAGE_SaveCounterStatus( unsigned long counterDbId , counterStatus_t * counterStatus );
unsigned int STORAGE_GetCounterStatusCrc( counterStatus_t * counterStatus );
int STORAGE_UpdateCounterStatus_LastProfilePtr( counter_t * counter , unsigned int newLastProfilePtr );
int STORAGE_UpdateCounterStatus_CounterType( counter_t * counter , unsigned char newCounterType );
int STORAGE_UpdateCounterStatus_ProfileInterval( counter_t * counter , unsigned char newProfileInterval );
int STORAGE_UpdateCounterStatus_Ratio( counter_t * counter , unsigned char newRatio );
int STORAGE_UpdateCounterStatus_StateWord( counter_t * counter , counterStateWord_t * counterStateWord );
int STORAGE_UpdateCounterStatus_AllowSeasonChangeFlg( counter_t * counter , unsigned char allowSeasonChangeFlg );
void STORAGE_StuffCounterStatusByDefault( counterStatus_t * counterStatus );

int STORAGE_LoadSeasonAllowFlag();
int STORAGE_LoadCurrentSeason();
int STORAGE_UpdateSeasonAllowFlag( unsigned char allow );
int STORAGE_UpdateCurrentSeason( unsigned char season );
unsigned char STORAGE_GetSeasonAllowFlag();
unsigned char STORAGE_GetCurrentSeason();

int STORAGE_GetConnection(connection_t * connection);
int STORAGE_CountOfCountersPlc();
int STORAGE_CountOfCounters485();
int STORAGE_GetNextCounterPlc(counter_t ** counter);
int STORAGE_GetNextCounter485(counter_t ** counter);

int STORAGE_GetPlcSettings(it700params_t * params);

int STORAGE_SetupRatio(unsigned long counterDbId, unsigned int ratio);
int STORAGE_SetupProfileInterval(unsigned long counterDbId, unsigned char interval);

void STORAGE_ResetRepeatFlag(unsigned long counterDbId);
//void STORAGE_ResetWriteTarrifsFlag (unsigned long counterDbId);

unsigned int STORAGE_CalcCrcForMeterage(meterage_t * data);
unsigned int STORAGE_CalcCrcForProfile(profile_t * data);
unsigned int STORAGE_CalcCrcForJournal(uspdEvent_t * event);

int STORAGE_CalcCrcForHardwareWritingCtrlCounter(hardwareWritingCtrl_t * hardwareWritingCtrl , unsigned int * crc);
void STORAGE_IncrementHardwareWritingCtrlCounter();

int STORAGE_SaveMeterages(int storage, unsigned long counterDbId, meterage_t *data);
int STORAGE_SaveProfile(unsigned long counterDbId, profile_t *data);
int STORAGE_GetJournalNumberByEvent(uspdEvent_t * event, int * maxEventsNumber);
int STORAGE_GetUspdJournalByEvent(uspdEvent_t * event , int * maxEventsNumber);
int STORAGE_SaveEvent(uspdEvent_t *event, unsigned long counterDbId);
BOOL STORAGE_CheckUspdStartFlag();
BOOL STORAGE_IsEventInJournal(uspdEvent_t * event , unsigned long counterDbId);

int STORAGE_GetMeterages(int storage, unsigned long counterDbId, dateTimeStamp date_from, dateTimeStamp date_to, meteregesArray_t ** data);
int STORAGE_GetProfile(unsigned long counterDbId, dateTimeStamp date_from, dateTimeStamp date_to, energiesArray_t ** data);

int STORAGE_GetLastMeterageDts(int storage, unsigned long counterDbId, dateTimeStamp * dts);
int STORAGE_GetLastProfileDts(unsigned long counterDbId, dateTimeStamp * dts);
BOOL STORAGE_CheckTimeToRequestMeterage( counter_t * counter , int storage , dateTimeStamp * dtsToRequest);
BOOL STORAGE_CheckTimeToRequestProfile(counter_t * counter, unsigned int * ptr, dateTimeStamp * dtsToRequest , int * canHoursRead);

int STORAGE_CreateStatisticMap();
void STORAGE_AppendToStatisticMap(counter_t * counter);
int STORAGE_SetCounterStatistic(unsigned long counterDbId, BOOL inPool, unsigned int transactionIndex, unsigned char transactionResult);
void STORAGE_DeleteStatisticMap();

int STORAGE_GetMeteragesForDb(int storage, unsigned long counterDbId, meteregesArray_t ** data);
int STORAGE_MarkMeteragesAsPlacedToDb(int storage, unsigned long counterDbId, dateTimeStamp lastDts);
int STORAGE_RecoveryMetragesPlasedToDbFlag(int storage, unsigned long counterDbId, dateTimeStamp olderDts);

int STORAGE_GetProfileForDb(unsigned long counterDbId, energiesArray_t ** data);
BOOL STORAGE_IsProfilePresenceInStorage(unsigned long counterDbId, dateTimeStamp * checkDts);
int STORAGE_MarkProfileAsPlacedToDb(unsigned long counterDbId, dateTimeStamp dtsLast);
int STORAGE_RecoveryProfilePlacedToDbFlag(unsigned long counterDbId, dateTimeStamp olderDts);
int STORAGE_RefactoreProfileWithId();

//int STORAGE_JournalUspdEvent( int evtype, int evdesc);
void STORAGE_JournalUspdEvent( unsigned char evtype, unsigned char * evdesc);

#if 0
int STORAGE_JournalGetUspdEventsForDb( uspdEventsArray_t ** eventsArr, unsigned long counterDbId,int journalNumber);
int STARAGE_JournalMarkEventsAsPlacedToDb(dateTimeStamp dtsLast, unsigned long counterDbId,int journalNumber);
#else
int STORAGE_GetJournalForDb(uspdEventsArray_t ** data , unsigned long counterDbId , int journalNumber);
int STORAGE_MarkJournalAsPlacedToDb(dateTimeStamp dtsLast , unsigned long counterDbId,int journalNumber);
int STORAGE_RecoveryJournalPlacedToDbFlag(dateTimeStamp olderDts , unsigned long counterDbId,int journalNumber);
#endif

int STORAGE_JournalCounterEvent(unsigned long counterDbId , dateTimeStamp dts , int eventType , unsigned char * evdesc );

int STORAGE_GetUnknownId (unsigned long counterDbId, int * unknownId);

int STORAGE_FoundCounter(unsigned long counterDbId, counter_t ** counter);
int STORAGE_FoundCounterNoAtom(unsigned long counterDbId, counter_t ** counter);
int STORAGE_FoundPlcCounter(unsigned char * remoteSn, counter_t ** counter);
void STORAGE_SetPlcCounterAged (unsigned char * remoteSn);
int STORAGE_GetNextAgedPlcCounter(counter_t ** counter);
int STORAGE_UpdateCounterDts(unsigned long counterDbId , dateTimeStamp * currentCounterDts, int taskTime);
//int STORAGE_DropSyncDtsFlag(unsigned long counterDbId);

int STORAGE_GetCounterStatistic( counterStat_t ** counterStat , int * counterStatLength );
int STORAGE_GetQuizStatistic(quizStatistic_t ** quizStatistic , int * quizStatisticLength);
int STORAGE_GetCounterAutodetectionStatistic(counterAutodetectionStatistic_t ** counterAutodetectionStatistic , int * counterAutodetectionStatisticLength);

void STORAGE_SwitchSyncFlagTo_Acq(counter_t ** counter);
void STORAGE_SwitchSyncFlagTo_SyncProcess(counter_t ** counter);
void STORAGE_SwitchSyncFlagTo_NeedToSync(counter_t ** counter);
void STORAGE_SwitchSyncFlagTo_SyncDone(counter_t ** counter);
void STORAGE_SwitchSyncFlagTo_SyncFail(counter_t ** counter);
BOOL STORAGE_IsCounterNeedToSync(counter_t ** counter);

int STORAGE_SavePQP(powerQualityParametres_t * powerQualityParametres , unsigned long counterDbId);
int STORAGE_GetPQPforDB(powerQualityParametres_t * powerQualityParametres , unsigned long counterDbId);

void STORAGE_PROCESS_UPDATE(char * cfgName);
void STORAGE_UPDATE_PLC_NET();
void STORAGE_UPDATE_RS();
void STORAGE_UPDATE_PLC();
void STORAGE_UPDATE_CONN();
void STORAGE_UPDATE_ETH();
void STORAGE_UPDATE_GSM();
void STORAGE_UPDATE_EXIO();

void STORAGE_ClearMeterages( unsigned long counterDdId );
void STORAGE_ClearMeteragesAfterHardSync(counter_t * counter );

unsigned char STORAGE_ApplyNewConfig(unsigned int mask);
int STORAGE_SaveServiceProperties( connection_t * connection );
int STORAGE_CheckNewConfig(unsigned int type , unsigned char * newConfig , int length, int * firstRetValue, int * secRetValue);
unsigned char STORAGE_SaveNewConfig(unsigned int type , unsigned char * newConfig , int length, char * reason);
unsigned char STORAGE_GetCfgFile(unsigned int mask , unsigned char ** buf , int * length );

void STORAGE_SetSyncFlagForCounters(int globalDiff) ;
int STORAGE_GetSyncStateWord( syncStateWord_t ** syncStateWord , int * syncStateWordSize);
BOOL STORAGE_CheckCounterDbIdPresenceInConfiguration( unsigned long counterDbId );
void STORAGE_SetTransactionTime(unsigned long counterDbId, double transactionTime);
int STORAGE_GetLastSyncDts(dateTimeStamp * dts, unsigned char * from);

int STORAGE_UpdateConnectionConfigFiles( connection_t * connection );
void STORAGE_SmsNewConfig (char * str , char * answer );

int STORAGE_GetStatisticStartDts(unsigned long dbid, dateTimeStamp * dts) ;

void STORAGE_CalcEffectivePollWeight(unsigned long dbid, double * epw);

//events foos
void STORAGE_EVENT_PLC_REMOTE_WAS_DELETED (unsigned char * serial);
void STORAGE_EVENT_PLC_REMOTE_WAS_ADDED (unsigned char * serial);
void STORAGE_EVENT_PLC_DEVICE_START (unsigned char code);
void STORAGE_EVENT_PLC_DEVICE_ERROR (unsigned char code);
void STORAGE_EVENT_DISCONNECTED (unsigned char code);

//debug
char * getPlcStrategyDesc();
char * getPlcModeDesc();

#ifdef __cplusplus
}
#endif

#endif

//EOF

