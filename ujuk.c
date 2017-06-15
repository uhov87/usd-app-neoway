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
#include <math.h>

#include "boolType.h"
#include "common.h"
#include "ujuk.h"
#include "ujuk_crc.h"
#include "storage.h"
#include "acounts.h"
#include "debug.h"

#define COUNTER_TASK_WRITE_USERCOMMANDS	1
#define COUNTER_TASK_SYNC_TIME			1
#define COUNTER_TASK_WRITE_TARIFF_MAP	1
#define COUNTER_TASK_WRITE_PSM			1
#define COUNTER_TASK_READ_DTS_AND_RATIO	1
#define COUNTER_TASK_READ_PROFILES		1
#define COUNTER_TASK_READ_CUR_METERAGES	1
#define COUNTER_TASK_READ_DAY_METERAGES	1
#define COUNTER_TASK_READ_MON_METERAGES	1
#define COUNTER_TASK_READ_JOURNALS		1
#define COUNTER_TASK_READ_PQP			1

//local variables
unsigned int startCrc = 0xFFFF;

//
//
//

unsigned int UJUK_GetCRC(unsigned char * cmd, unsigned int size) {
	DEB_TRACE(DEB_IDX_UJUK)

	unsigned int crc = 0;
	unsigned int byteIndex = 1;

	crc = UJUK_ByteCRC(cmd[0], startCrc);
	for (; byteIndex < size; byteIndex++)
		crc = UJUK_ByteCRC(cmd[byteIndex], crc);

	return crc;
}

//
//
//

unsigned int UJUK_ByteCRC(unsigned char someByte, unsigned int startCRC) {
	DEB_TRACE(DEB_IDX_UJUK)

	unsigned int crcTableIndex = 0;
	unsigned char arrCRC[2];
	unsigned int result = 0;

	arrCRC[0] = (unsigned char) ((startCRC >> 8) & 0xFF);
	arrCRC[1] = (unsigned char) ((startCRC) & 0xFF);

	crcTableIndex = ((arrCRC[1] & 0xFF) ^ (someByte & 0xFF));

	arrCRC[1] = (unsigned char) ((arrCRC[0] & 0xFF) ^ (srCRCHi[crcTableIndex] & 0xFF));
	arrCRC[0] = (unsigned char) (srCRCLo[crcTableIndex]);

	result = (arrCRC[0] << 8) + arrCRC[1];

	return result;
}

//
//
//

BOOL UJUK_GetCounterAddress(counter_t * counter, unsigned int * counterAddress) {
	DEB_TRACE(DEB_IDX_UJUK)

	//check long or short
	if (counter->serial > 0x3E7) {
		* counterAddress = (counter->serial & 0xFFFFFFFF);
		return TRUE;
	} else {

		* counterAddress = (counter->serial % 0x3E8);
		if (* counterAddress > 0xFF)
			* counterAddress = (counter->serial % 0x64);
		return FALSE;
	}
}

//
//
//

BOOL UJUK_IsCounterAbleProfile(unsigned char type)
{
	switch( type )
	{
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
		case TYPE_UJUK_SEB_1TM_02A:
			return TRUE ;

	}

	return FALSE ;
}

//
//
//

void UJUK_ParseProfileBuf( counter_t * counter, unsigned char * buf , energy_t * energy , unsigned char * status)
{
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parsing profile");

	energy->e[ 0 ] = 0 ;
	energy->e[ 1 ] = 0 ;
	energy->e[ 2 ] = 0 ;
	energy->e[ 3 ] = 0 ;


	switch( counter->type )
	{
		case TYPE_UJUK_SEB_1TM_01A:
		case TYPE_UJUK_SEB_1TM_02A:
		case TYPE_UJUK_SEB_1TM_02D:
			//only A+ channel
			energy->e[ 0 ] = COMMON_BufToShort(&buf[0]);
			break;

		case TYPE_UJUK_SEB_1TM_02M:
			//all channels except A-
			energy->e[ 0 ] = COMMON_BufToShort(&buf[0]);
			energy->e[ 2 ] = COMMON_BufToShort(&buf[4]);
			energy->e[ 3 ] = COMMON_BufToShort(&buf[6]);
			break;

		default:
			//all channels
			energy->e[ 0 ] = COMMON_BufToShort(&buf[0]);
			energy->e[ 1 ] = COMMON_BufToShort(&buf[2]);
			energy->e[ 2 ] = COMMON_BufToShort(&buf[4]);
			energy->e[ 3 ] = COMMON_BufToShort(&buf[6]);
			break;
	}

	if ( (energy->e[ 0 ] & 0x8000) ||
		 (energy->e[ 1 ] & 0x8000) ||
		 (energy->e[ 2 ] & 0x8000) ||
		 (energy->e[ 3 ] & 0x8000) )
	{
		energy->e[ 0 ] &= 0x7FFF ;
		energy->e[ 1 ] &= 0x7FFF ;
		energy->e[ 2 ] &= 0x7FFF ;
		energy->e[ 3 ] &= 0x7FFF ;
		*status = METERAGE_PARTIAL ;
	}
	else
		*status = METERAGE_VALID ;

	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK A+ [%lu]", energy->e[ 0 ]);
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK A- [%lu]", energy->e[ 1 ]);
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK R+ [%lu]", energy->e[ 2 ]);
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK R- [%lu]", energy->e[ 3 ]);
}

//
//
//

void UJUK_ParseEventBuf(counter_t * counterPtr, const counterTransaction_t * transaction)
{
	uspdEvent_t event1;
	uspdEvent_t event2;
	int aIndex;

	memset(&event1 , 0 , sizeof(uspdEvent_t));
	memset(&event2 , 0 , sizeof(uspdEvent_t));

	aIndex = 1;
	if (transaction->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
		aIndex += 4;

	switch(counterPtr->journalNumber)
	{
		case 0x56:
		{
			event1.evType = EVENT_MAGNETIC_INDUCTION_START ;
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event2.evType = EVENT_MAGNETIC_INDUCTION_STOP ;

			event2.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event2.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			if(COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
					{
						if (STORAGE_IsEventInJournal(&event2 , counterPtr->dbId) == FALSE)
						{
							event2.crc = STORAGE_CalcCrcForJournal(&event1);
							STORAGE_SaveEvent(&event2 , counterPtr->dbId);
						}
					}
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{
					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					event2.crc = STORAGE_CalcCrcForJournal(&event2);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
						STORAGE_SaveEvent(&event2 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}

			}
			else
				UJUK_StepToNextEvent(counterPtr);
		}
		break;

		case 0x53:
		{
			event1.evType = EVENT_OPENING_BOX ;
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event2.evType = EVENT_CLOSING_BOX ;

			event2.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event2.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			if(COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
					{
						if (STORAGE_IsEventInJournal(&event2 , counterPtr->dbId) == FALSE)
						{
							event2.crc = STORAGE_CalcCrcForJournal(&event1);
							STORAGE_SaveEvent(&event2 , counterPtr->dbId);
						}
					}
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{
					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					event2.crc = STORAGE_CalcCrcForJournal(&event2);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
						STORAGE_SaveEvent(&event2 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}

			}
			else
				UJUK_StepToNextEvent(counterPtr);
		}
		break;

		case 0x0A:
		{
			event1.evType = EVENT_OPENING_CONTACT ;
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event2.evType = EVENT_CLOSING_CONTACT ;

			event2.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event2.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );


			if(COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
					{
						if (STORAGE_IsEventInJournal(&event2 , counterPtr->dbId) == FALSE)
						{
							event2.crc = STORAGE_CalcCrcForJournal(&event2);

							STORAGE_SaveEvent(&event2 , counterPtr->dbId);
						}
					}
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{
					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					event2.crc = STORAGE_CalcCrcForJournal(&event2);

					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
						STORAGE_SaveEvent(&event2 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}

			}
			else
				UJUK_StepToNextEvent(counterPtr);
		}
		break;

		case 0x01:
		{
			event1.evType = EVENT_COUNTER_SWITCH_OFF ;
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event2.evType = EVENT_COUNTER_SWITCH_ON ;

			event2.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event2.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );


			if(COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
					{
						if (STORAGE_IsEventInJournal(&event2 , counterPtr->dbId) == FALSE)
						{
							event2.crc = STORAGE_CalcCrcForJournal(&event2);
							STORAGE_SaveEvent(&event2 , counterPtr->dbId);
						}
					}
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{

					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					event2.crc = STORAGE_CalcCrcForJournal(&event2);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
						STORAGE_SaveEvent(&event2 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}

			}
			else
				UJUK_StepToNextEvent(counterPtr);
		}
		break;

		case 0x02:
		{
			event1.evType = EVENT_SYNC_TIME_HARD ;

			dateTimeStamp lastTime;
			memset(&lastTime , 0 , sizeof(dateTimeStamp));

			lastTime.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			lastTime.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			lastTime.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			lastTime.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			lastTime.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			lastTime.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			memcpy(&event1.evDesc , &lastTime , sizeof(dateTimeStamp));

			if (COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{
					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}
			}
			else
				UJUK_StepToNextJournal(counterPtr);
		}
		break;

		case 0x04:
		{
			event1.evType = EVENT_CHANGE_TARIF_MAP ;
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			if (COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{
					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}
			}
			else
				UJUK_StepToNextJournal(counterPtr);
		}
		break;

		case 0x48:
		{
			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			aIndex++ ;

			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			unsigned char reason  = transaction->answer[aIndex++] ;
			unsigned char state  = transaction->answer[aIndex++] ;
			if (state == 1)
			{
				event1.evType = EVENT_SWITCH_POWER_OFF ;
			}
			else
			{
				event1.evType = EVENT_SWITCH_POWER_ON ;
			}

			if (COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if ( UJUK_GetReleReason( reason, event1.evDesc) == OK )
				{
					if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
					{
						UJUK_StepToNextJournal(counterPtr);
					}
					else
					{
						event1.crc = STORAGE_CalcCrcForJournal(&event1);
						STORAGE_SaveEvent(&event1 , counterPtr->dbId);
						UJUK_StepToNextEvent(counterPtr);
					}
				}
				else
					UJUK_StepToNextEvent(counterPtr);
			}
			else
				UJUK_StepToNextJournal(counterPtr);
		}
		break;

		case 0x15:
			event1.evType = EVENT_PQP_VOLTAGE_INCREASE_F1 ;
			event2.evType = EVENT_PQP_VOLTAGE_I_NORMALISE_F1 ;
			goto UPEB_Pqp;

		case 0x16:
			event1.evType = EVENT_PQP_VOLTAGE_DECREASE_F1;
			event2.evType = EVENT_PQP_VOLTAGE_D_NORMALISE_F1;
			goto UPEB_Pqp;

		case 0x17:
			event1.evType = EVENT_PQP_VOLTAGE_INCREASE_F2 ;
			event2.evType = EVENT_PQP_VOLTAGE_I_NORMALISE_F2 ;
			goto UPEB_Pqp;

		case 0x18:
			event1.evType = EVENT_PQP_VOLTAGE_DECREASE_F2;
			event2.evType = EVENT_PQP_VOLTAGE_D_NORMALISE_F2;
			goto UPEB_Pqp;

		case 0x19:
			event1.evType = EVENT_PQP_VOLTAGE_INCREASE_F3 ;
			event2.evType = EVENT_PQP_VOLTAGE_I_NORMALISE_F3 ;
			goto UPEB_Pqp;

		case 0x1A:
			event1.evType = EVENT_PQP_VOLTAGE_DECREASE_F3;
			event2.evType = EVENT_PQP_VOLTAGE_D_NORMALISE_F3;
			goto UPEB_Pqp;

		case 0x0B:
		case 0x0C:
			event1.evType = EVENT_PQP_FREQ_DEVIATION_02;
			event2.evType = EVENT_PQP_FREQ_NORMALISE_02;
			goto UPEB_Pqp;

		case 0x13:
		case 0x14:
			event1.evType = EVENT_PQP_FREQ_DEVIATION_04;
			event2.evType = EVENT_PQP_FREQ_NORMALISE_04;
			goto UPEB_Pqp;

		case 0x35:
			event1.evType = EVENT_PQP_KNOP_INCREASE_4;
			event2.evType = EVENT_PQP_KNOP_NORMALISE_4;
			goto UPEB_Pqp;

		case 0x36:
			event1.evType = EVENT_PQP_KNOP_INCREASE_2;
			event2.evType = EVENT_PQP_KNOP_NORMALISE_2;
			goto UPEB_Pqp;

		case 0x33:
			event1.evType = EVENT_PQP_KNNP_INCREASE_4;
			event2.evType = EVENT_PQP_KNNP_NORMALISE_4;
			goto UPEB_Pqp;

		case 0x34:
			event1.evType = EVENT_PQP_KNNP_INCREASE_2;
			event2.evType = EVENT_PQP_KNNP_NORMALISE_2;
			UPEB_Pqp:

			event1.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			aIndex++ ;
			event1.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event1.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			event2.dts.t.s = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.t.h = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			aIndex++ ;
			event2.dts.d.d = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.m = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );
			event2.dts.d.y = (unsigned char)COMMON_Translate2_10to10( (int)transaction->answer[aIndex++] );

			memset(event1.evDesc, 0xFF , 4);
			memset(event2.evDesc, 0xFF , 4);

			if(COMMON_CheckDtsForValid(&event1.dts) == OK)
			{
				if (STORAGE_IsEventInJournal(&event1 , counterPtr->dbId) == TRUE)
				{
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
					{
						if (STORAGE_IsEventInJournal(&event2 , counterPtr->dbId) == FALSE)
						{
							event2.crc = STORAGE_CalcCrcForJournal(&event2);
							STORAGE_SaveEvent(&event2 , counterPtr->dbId);
						}
					}
					UJUK_StepToNextJournal(counterPtr);
				}
				else
				{

					event1.crc = STORAGE_CalcCrcForJournal(&event1);
					event2.crc = STORAGE_CalcCrcForJournal(&event2);
					STORAGE_SaveEvent(&event1 , counterPtr->dbId);
					if(COMMON_CheckDtsForValid(&event2.dts) == OK)
						STORAGE_SaveEvent(&event2 , counterPtr->dbId);
					UJUK_StepToNextEvent(counterPtr);
				}

			}
			else
				UJUK_StepToNextEvent(counterPtr);

			break;


		default:
			break;
	}

}

//
//
//

int UJUK_Command_OpenCounter(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_OPEN_COUNTER_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_OPEN_COUNTER_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_OPEN_COUNTER;

	//set password
	memcpy(&tempCommand[commandSize], counter->password1, UJUK_SIZE_OF_PASSWORD);
	commandSize += UJUK_SIZE_OF_PASSWORD;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_COUNTER_OPEN;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(OPEN) %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_OpenCounterForWritingToPhMemmory(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_OPEN_COUNTER_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_OPEN_COUNTER_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_OPEN_COUNTER;

	//set password
	memcpy(&tempCommand[commandSize], counter->password2, UJUK_SIZE_OF_PASSWORD);
	commandSize += UJUK_SIZE_OF_PASSWORD;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_COUNTER_OPEN;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(OPEN) %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetRatioValue(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PARAMS_COUNTER_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PARAMS_COUNTER_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_PARAMS;
	tempCommand[commandSize++] = UJUK_RATIO;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_GET_RATIO;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET RATIO) %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetDateAndTime(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_COUNTER_TIME_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_COUNTER_TIME_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_JOURNAL_TIME;
	tempCommand[commandSize++] = UJUK_CURRNET_TIME;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_DATE_TIME_STAMP;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET CLOCK) %s", getErrorCode(res));

	return res;
}


//
//
//

int UJUK_Command_SyncTimeHardy(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_SHORT;
	}
	//set command
	tempCommand[commandSize++] = UJUK_CMD_SET_PROPERTY;
	tempCommand[commandSize++] = UJUK_SYNC_TIME_HARDY;

	//set dts
	dateTimeStamp dtsToSet;
	memset(&dtsToSet, 0 , sizeof(dateTimeStamp));
	COMMON_GetCurrentDts(&dtsToSet);

	int i = 0;
	for( ; i < ( counter->transactionTime / 2 ) ; ++i )
		COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );

	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.s );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.m );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.h );

	tempCommand[commandSize++] = COMMON_Translate10to2_10( COMMON_GetWeekDayByDts( &dtsToSet ));

	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.d );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.m );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.y );

	//winter-summer switcher
	unsigned char season = STORAGE_GetCurrentSeason();
	if ( season == STORAGE_SEASON_SUMMER )
		tempCommand[commandSize++] = 0x00;
	else
		tempCommand[commandSize++] = 0x01;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_SYNC_HARDY;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(SYNC TIME HARDY) %s", getErrorCode(res));

	return res;
}


//
//
//

int UJUK_Command_SetCurrentSeason(counter_t * counter, counterTransaction_t ** transaction , unsigned char season) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_SHORT;
	}
	//set command
	tempCommand[commandSize++] = UJUK_CMD_SET_PROPERTY;
	tempCommand[commandSize++] = UJUK_SYNC_TIME_HARDY;

	//set dts
	dateTimeStamp dtsToSet;
	memset(&dtsToSet, 0 , sizeof(dateTimeStamp));
	COMMON_GetCurrentDts(&dtsToSet);

	//send to counter its own time
	int syncWorth = abs(counter->syncWorth) ;
	int secondIterator = 0 ;
	if ( counter->syncWorth >=0  )
	{
		for ( ; secondIterator < syncWorth ; ++secondIterator )
			COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );
	}
	else
	{
		for ( ; secondIterator < syncWorth ; ++secondIterator )
			COMMON_ChangeDts( &dtsToSet , DEC , BY_SEC );
	}
	for( secondIterator = 0 ; secondIterator < ( counter->transactionTime / 2 ) ; ++secondIterator )
		COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );

	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.s );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.m );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.h );

	tempCommand[commandSize++] = COMMON_Translate10to2_10( COMMON_GetWeekDayByDts( &dtsToSet ));

	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.d );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.m );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.d.y );

	//winter-summer switcher
	if ( season == STORAGE_SEASON_SUMMER )
		tempCommand[commandSize++] = 0x00;
	else
		tempCommand[commandSize++] = 0x01;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_SET_CURRENT_SEASON;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(Set season) %s , setting dts " DTS_PATTERN , getErrorCode(res) , DTS_GET_BY_VAL(dtsToSet) );

	return res;
}


//
//
//

int UJUK_Command_SyncTimeSoft(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SYNC_TIME_HARDY_SHORT;
	}
	//set command
	tempCommand[commandSize++] = UJUK_CMD_SET_PROPERTY;
	tempCommand[commandSize++] = UJUK_SYNC_TIME_SOFT;

	//set dts
	dateTimeStamp dtsToSet;
	memset(&dtsToSet, 0 , sizeof(dateTimeStamp));
	COMMON_GetCurrentDts(&dtsToSet);

	int i = 0;
	for( ; i < ( counter->transactionTime / 2 ) ; ++i )
		COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );

	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.s );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.m );
	tempCommand[commandSize++] = COMMON_Translate10to2_10( dtsToSet.t.h );

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_SYNC_SOFT;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(SYNC TIME SOFT) %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_CurrentMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	switch( counter->type )
	{
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_SEB_1TM_01A:
		case TYPE_UJUK_SEB_1TM_02A:		
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
		{
			
			//check long address is used
			longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
			if (longAddressUsed) {
				//set long address
				tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);
		
				//set answer size
				if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
					(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
					(counter->type == TYPE_UJUK_SEB_1TM_02D)){
					answerLen = UJUK_ANSWER_METERAGE_1_TM_02_LONG;
		
				} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
					answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_LONG;
		
				} else {
					answerLen = UJUK_ANSWER_METERAGE_LONG;
				}
		
			} else {
				//set short address
				tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);
		
				//set answer size
				if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
					(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
					(counter->type == TYPE_UJUK_SEB_1TM_02D)){
					answerLen = UJUK_ANSWER_METERAGE_1_TM_02_SHORT;
				} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
					answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_SHORT;
				} else {
					answerLen = UJUK_ANSWER_METERAGE_SHORT;
				}
			}
		
			//set command
			tempCommand[commandSize++] = UJUK_CMD_GET_METERAGES_FROM_ARCHIVE;
			tempCommand[commandSize++] = UJUK_CURRENT;
			tempCommand[commandSize++] = ZERO;
			tempCommand[commandSize++] = tarifIndex;
		
			if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
				(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
				(counter->type == TYPE_UJUK_SEB_1TM_02D)){
				tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT;
		
			} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
				tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT_R_BOTH;
		
			} else {
				tempCommand[commandSize++] = ALL_ENERGY;
			}
			tempCommand[commandSize++] = ZERO;
		}
		break;

		// for old counters like SET-4TM-02
		default:
		{
			//check long address is used
			longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
			if (longAddressUsed) {
				//set long address
				tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
				tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);
		
				answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_LONG ;
			} else {
				//set short address
				tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);
		
				//set answer size
				
				answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_SHORT ;
			}
			
			//set command
			tempCommand[commandSize++] = UJUK_CMD_SIMPLE_GET_METERAGES;
			tempCommand[commandSize++] = 0x00;
			tempCommand[commandSize++] = tarifIndex;
		}
		break;
	}


	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_CURRENT + tarifIndex;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET CURRENT (T%d)) %s", tarifIndex, getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetMeteragesByBinaryMask(counter_t * counter, counterTransaction_t ** transaction , unsigned char memoryType , unsigned char monthNumb) {
	DEB_TRACE(DEB_IDX_UJUK);

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;

	unsigned char tempCommand[64];
	memset(tempCommand, 0, 64);

	unsigned char transactionType = 0 ;
	switch( memoryType )
	{
		case UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_CURRENT:
		{
			transactionType = TRANSACTION_METERAGE_CURRENT ;
		}
		break;

		case UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_MONTH:
		{
			transactionType = TRANSACTION_METERAGE_MONTH ;
		}
		break;

		case UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_TODAY:
		case UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_YESTERDAY:
		case UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_DAY:
		{
			transactionType = TRANSACTION_METERAGE_DAY ;
		}
		break;

		default:
		{
			return res ;
		}
		break;
	}

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_METERAGES_BY_BINARY_MASK_LONG;
	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_METERAGES_BY_BINARY_MASK_SHORT;
	}
	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_ENERGY_BY_BINARY_MASK;
	tempCommand[commandSize++] = transactionType; // request idetificator
	tempCommand[commandSize++] = memoryType ;
	tempCommand[commandSize++] = monthNumb ;
	tempCommand[commandSize++] = 0x00; // mask type

	//binary mask ( all energies all tariffs )
	tempCommand[commandSize++] = 0x00; //R4
	tempCommand[commandSize++] = 0x00; //R4 .. R3
	tempCommand[commandSize++] = 0x00; //R3 .. R2
	tempCommand[commandSize++] = 0x00; //R2 .. R1
	tempCommand[commandSize++] = 0x00; //R1 .. R-

	//R-
	if ( counter->maskEnergy & ENERGY_RM )
	{
		switch( counter->type )
		{
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SEB_1TM_02D:
			{
				tempCommand[commandSize++] = 0x00; //R- do not use
			}
			break;

			default:
			{
				tempCommand[commandSize++] = (counter->maskTarifs & 0x0F) << 3 ;
				if ( counter->maskTarifs & COUNTER_MASK_T1 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T2 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T3 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T4 ) answerLen += 4;
			}
			break;
		}
	}
	else
	{
		tempCommand[commandSize++] = 0x00;
	}

	//R+
	if ( counter->maskEnergy & ENERGY_RP )
	{
		switch( counter->type )
		{
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SEB_1TM_02D:
			{
				tempCommand[commandSize++] = 0x00; //R+ do not use
			}
			break;

			default:
			{
				tempCommand[commandSize++] = (counter->maskTarifs & 0x0F) << 2 ;
				if ( counter->maskTarifs & COUNTER_MASK_T1 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T2 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T3 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T4 ) answerLen += 4;
			}
			break;
		}
	}
	else
	{
		tempCommand[commandSize++] = 0x00;
	}

	//A-
	if ( counter->maskEnergy & ENERGY_AM )
	{
		switch( counter->type )
		{
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SEB_1TM_02D:
			case TYPE_UJUK_SEB_1TM_02M:
			{
				tempCommand[commandSize++] = 0x00; //A- do not use
			}
			break;

			default:
			{
				tempCommand[commandSize++] = (counter->maskTarifs & 0x0F) << 1 ;
				if ( counter->maskTarifs & COUNTER_MASK_T1 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T2 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T3 ) answerLen += 4;
				if ( counter->maskTarifs & COUNTER_MASK_T4 ) answerLen += 4;
			}
			break;
		}
	}
	else
	{
		tempCommand[commandSize++] = 0x00;
	}

	//A+
	if ( counter->maskEnergy & ENERGY_AP )
	{
		tempCommand[commandSize++] = (counter->maskTarifs & 0x0F) ;
		if ( counter->maskTarifs & COUNTER_MASK_T1 ) answerLen += 4;
		if ( counter->maskTarifs & COUNTER_MASK_T2 ) answerLen += 4;
		if ( counter->maskTarifs & COUNTER_MASK_T3 ) answerLen += 4;
		if ( counter->maskTarifs & COUNTER_MASK_T4 ) answerLen += 4;
	}
	else
	{
		tempCommand[commandSize++] = 0x00 ;
	}

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = transactionType;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = TRUE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask( GetMeterage_AllEnergies_AllTariffs ) %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_DayEnergy(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char archive)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];
	unsigned char transactionType = 0 ;
	memset(tempCommand, 0, 64);

	// for old counters like SET-4TM-02

	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_LONG ;
	}
	else
	{
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);
		answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_SHORT ;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_SIMPLE_GET_METERAGES;

	switch( archive )
	{
		case ASK_DAY_TODAY:
		{
			tempCommand[commandSize++] = 0x40 ;
			transactionType = TRANSACTION_POWER_TODAY ;
		}
		break;

		case ASK_DAY_YESTERDAY:
		{
			tempCommand[commandSize++] = 0x50 ;
			transactionType = TRANSACTION_POWER_YESTERDAY ;
		}
		break;

		default:
			return ERROR_GENERAL ;
			break;
	}

	tempCommand[commandSize++] = tarifIndex;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = transactionType + tarifIndex;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET DAY(T%d)) %s", tarifIndex, getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_DayMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char archive, unsigned char day) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02D)){
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02_LONG;

		} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_LONG;

		} else {
			answerLen = UJUK_ANSWER_METERAGE_LONG;
		}

	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02D)){
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02_SHORT;

		} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_SHORT;

		} else {
			answerLen = UJUK_ANSWER_METERAGE_SHORT;
		}
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_METERAGES_FROM_ARCHIVE;
	switch (archive) {
		case ASK_DAY_TODAY:
			tempCommand[commandSize++] = UJUK_TODAY;
			tempCommand[commandSize++] = ZERO;
			break;

		case ASK_DAY_YESTERDAY:
			tempCommand[commandSize++] = UJUK_YESTERDAY;
			tempCommand[commandSize++] = ZERO;
			break;

		case ASK_DAY_ARCHIVE:
			tempCommand[commandSize++] = UJUK_ARCHIVE_DAY;
			tempCommand[commandSize++] = day;
			break;
	}
	tempCommand[commandSize++] = tarifIndex;

	if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
		(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
		(counter->type == TYPE_UJUK_SEB_1TM_02D)){
		tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT;

	} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
		tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT_R_BOTH;

	} else {
		tempCommand[commandSize++] = ALL_ENERGY;
	}

	tempCommand[commandSize++] = ZERO;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_DAY + tarifIndex;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET DAY(T%d) for day %d) %s", tarifIndex, day, getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_MonthEnergy(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, int monthIndex)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];
	unsigned char transactionType = TRANSACTION_POWER_MONTH ;
	memset(tempCommand, 0, 64);

	// for old counters like SET-4TM-02

	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_LONG ;
	}
	else
	{
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);
		answerLen = UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_SHORT ;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_SIMPLE_GET_METERAGES;

	tempCommand[commandSize++] = 0x30 | monthIndex;
	tempCommand[commandSize++] = tarifIndex;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = transactionType + tarifIndex;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET MONTH ENERGY(T%d)) %s", tarifIndex, getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_MonthMeterage(counter_t * counter, counterTransaction_t ** transaction, unsigned char tarifIndex, unsigned char month) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed) {
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02D)){
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02_LONG;

		} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_LONG;

		} else {
			answerLen = UJUK_ANSWER_METERAGE_LONG;
		}

	} else {
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
			(counter->type == TYPE_UJUK_SEB_1TM_02D)){
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02_SHORT;

		} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
			answerLen = UJUK_ANSWER_METERAGE_1_TM_02M_SHORT;

		} else {
			answerLen = UJUK_ANSWER_METERAGE_SHORT;
		}
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_METERAGES_FROM_ARCHIVE;
	tempCommand[commandSize++] = UJUK_MONTH;
	tempCommand[commandSize++] = month;
	tempCommand[commandSize++] = tarifIndex;

	if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
		(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
		(counter->type == TYPE_UJUK_SEB_1TM_02D)){
		tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT;

	} else if (counter->type == TYPE_UJUK_SEB_1TM_02M) {
		tempCommand[commandSize++] = ONLY_ENERGY_A_DIRECT_R_BOTH;

	} else {
		tempCommand[commandSize++] = ALL_ENERGY;
	}

	tempCommand[commandSize++] = ZERO;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_MONTH + tarifIndex;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET MONTH(T%d) for month %d) %s", tarifIndex, month, getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetProfilePointer(counter_t * counter, counterTransaction_t ** transaction, dateTimeStamp *dts)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_POINTER_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_POINTER_SHORT;
	}
	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x28;
	//set parametres

	// storage array number
	tempCommand[commandSize++] = (unsigned char)0x00;

	//start searching address
	tempCommand[commandSize++] = (unsigned char)0xFF;
	tempCommand[commandSize++] = (unsigned char)0xFF;

	//dateTimeStamp

	//hour
	//tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10(dts->t.h);
	//mask hour
	tempCommand[commandSize++] = (unsigned char)0xFF;
	//day
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10(dts->d.d);
	//month
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10(dts->d.m);
	//year
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10(dts->d.y);

	//summer/winter time
	tempCommand[commandSize++] = (unsigned char)0xFF;

	//profile time
	tempCommand[commandSize++] = (unsigned char)counter->profileInterval;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	(*transaction)->transactionType = TRANSACTION_PROFILE_POINTER;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfilePointer() return %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_SetProfileIntervalValue(counter_t * counter, counterTransaction_t ** transaction, unsigned char value)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_ONLY_ERRORCODE_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_ONLY_ERRORCODE_SHORT;
	}
	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x00;

	//set parametres
	tempCommand[commandSize++] = value;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	(*transaction)->transactionType = TRANSACTION_PROFILE_INIT;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetProfileIntervalValue() return %s", getErrorCode(res));

	return res;
}

//
//
//
int UJUK_Command_GetProfileAllHour(counter_t * counter, counterTransaction_t ** transaction, unsigned int pointer, int hoursNumb)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];
	int value = 0 ;

	if ( COMMON_CheckProfileIntervalForValid( counter->profileInterval ) == OK )
	{
		if ( (counter->profileInterval < 30) ||
			 ((counter->type != TYPE_UJUK_PSH_4TM_05MK) &&
			  (counter->type != TYPE_UJUK_SEB_1TM_02M) &&
			  (counter->type != TYPE_UJUK_SEB_1TM_03A)) )
			hoursNumb = 1 ;

		if (hoursNumb > 3)
			hoursNumb = 3 ;

		value = hoursNumb * 8 * ( (60 / counter->profileInterval) + 1 ) ;
	}
	else
	{
		return res ;
	}

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_VALUE_LONG + value;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_VALUE_SHORT + value;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x06;

	//memory storage number
	tempCommand[commandSize++] = (unsigned char)0x03;

	//profile address
	//tempCommand[commandSize++] = (unsigned char)((counter->lastRequestedProfilePointer & 0xFF00) >> 8 );
	//tempCommand[commandSize++] = (unsigned char)(counter->lastRequestedProfilePointer & 0xFF);
	tempCommand[commandSize++] = (unsigned char)((pointer & 0xFF00) >> 8 );
	tempCommand[commandSize++] = (unsigned char)(pointer & 0xFF);

	//number of bytes to read
	tempCommand[commandSize++] = (unsigned char)value;


	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_PROFILE_GET_ALL_HOUR;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfileValue return %s", getErrorCode(res));

	return res;
}

//
//
//

#if 0
int UJUK_Command_GetProfileValue(counter_t * counter, counterTransaction_t ** transaction, unsigned int pointer)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	int value = 0 ;
	if ( COMMON_CheckProfileIntervalForValid( counter->profileInterval ) == OK )
	{
		value = 8 * ( (60 / counter->profileInterval) + 1 ) ;
	}
	else
	{
		return res ;
	}


	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_VALUE_LONG + value;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_VALUE_SHORT + value;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x06;

	//memory storage number
	tempCommand[commandSize++] = (unsigned char)0x03;

	//profile address
	//tempCommand[commandSize++] = (unsigned char)((counter->lastRequestedProfilePointer & 0xFF00) >> 8 );
	//tempCommand[commandSize++] = (unsigned char)(counter->lastRequestedProfilePointer & 0xFF);
	tempCommand[commandSize++] = (unsigned char)((pointer & 0xFF00) >> 8 );
	tempCommand[commandSize++] = (unsigned char)(pointer & 0xFF);

	//number of bytes to read
	tempCommand[commandSize++] = (unsigned char)value;


	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_PROFILE_GET_VALUE;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfileValue return %s", getErrorCode(res));

	return res;
}

#endif
//
//
//

int UJUK_Command_GetProfileSearchState(counter_t * counter , counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_SEARCHING_STATE_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_PROFILE_SEARCHING_STATE_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x08;
	tempCommand[commandSize++] = (unsigned char)0x18;
	tempCommand[commandSize++] = (unsigned char)0x00;
	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_PROFILE_GET_SEARCHING_STATE;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfileSearchState return %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_GetProfileIntervalValue(counter_t * counter, counterTransaction_t ** transaction) {
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PROFILE_INTERVAL_VALUE_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PROFILE_INTERVAL_VALUE_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_PROFILE_SETTINGS;
	tempCommand[commandSize++] = 0x06;
	//tempCommand[commandSize++] = 0x00;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_PROFILE_INTERVAL;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfileInterval() %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetProfileCurrentPtr(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PROFILE_POINTER_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_PROFILE_POINTER_SHORT;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_PROFILE_SETTINGS;
	tempCommand[commandSize++] = 0x04;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_CURR_PPTR;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL) {
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetProfileInterval() %s", getErrorCode(res));

	return res;
}

//
//
//

int UJUK_Command_GetNextEventFromCurrentJournal(counter_t * counter , counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	int shortEventLength = 0 ;

	if (counter->journalNumber == 0x00)
		UJUK_StepToNextJournal(counter);

	switch(counter->journalNumber)
	{
		case 0x04:
			shortEventLength = 10 ;
			break;

		default:
			shortEventLength = 17 ;
			break;
	}

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = shortEventLength + 4 ;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = shortEventLength;
	}

	//set command
	tempCommand[commandSize++] = UJUK_CMD_GET_EVENT;
	tempCommand[commandSize++] = counter->journalNumber;
	tempCommand[commandSize++] = counter->eventNumber;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_GET_EVENT;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET EVENT [%d] FROM JOURNAL [%d]) %s", counter->eventNumber , counter->journalNumber , getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_GetPowerQualityParametres(counter_t * counter, counterTransaction_t ** transaction , int pqpPart , int phaseNumber)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];
	memset(tempCommand, 0, 64);

	unsigned char rwri = 0 ;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET PQP) [pqp part %u]", pqpPart);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = 10;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = 6;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x08;

	switch( counter->type )
	{
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_SEB_1TM_01A:
		case TYPE_UJUK_SEB_1TM_02A:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
		{
			switch(pqpPart)
			{
				case PQP_STATE_READING_VOLTAGE:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_PHASE1;						
						rwri = 0x11;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_GROUP;
						rwri = 0x12 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_AMPERAGE:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_PHASE1;
						rwri = 0x21;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_GROUP;
						rwri = 0x20 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_POWER_P:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_PHASE1;
						rwri = 0x01;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_GROUP;
						rwri = 0x00 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_POWER_Q:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_PHASE1;
						rwri = 0x05;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_GROUP;
						rwri = 0x04 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_POWER_S:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_PHASE1;
						rwri = 0x09;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_GROUP;
						rwri = 0x08 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_FREQUENCY:
				{
					tempCommand[commandSize++] = (unsigned char)0x11;
					(*transaction)->transactionType = TRANSACTION_GET_PQP_FREQUENCY;
					rwri = 0x40 ;
				}
				break;

				case PQP_STATE_READING_RATIO_COS:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_COS_PHASE1;
						rwri = 0x31;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_COS_GROUP;
						rwri = 0x30 ;
						answerLen += 9 ;
					}
				}
				break;

				case PQP_STATE_READING_RATIO_TAN:
				{
					if (phaseNumber == 1)
					{
						tempCommand[commandSize++] = (unsigned char)0x11;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_TAN_PHASE1;
						rwri = 0x39;
					}
					else
					{
						tempCommand[commandSize++] = (unsigned char)0x16;
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_TAN_GROUP;
						rwri = 0x38 ;
						answerLen += 9 ;
					}
				}
				break;

				default:
					return res ;
					break;
			} // switch(pqpPart)
		}
		break;

		default:
		{
			tempCommand[commandSize++] = (unsigned char)0x11;
			switch(pqpPart)
			{
				case PQP_STATE_READING_VOLTAGE:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_PHASE1;
						rwri = 0x11;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_PHASE2;
						rwri = 0x12;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_PHASE3;
						rwri = 0x13;
					}
					else
					{
						return res ;
					}
				}
				break;

				case PQP_STATE_READING_AMPERAGE:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_PHASE1;
						rwri = 0x21;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_PHASE2;
						rwri = 0x22;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_PHASE3;
						rwri = 0x23;
					}
					else
					{
						return res ;
					}
				}
				break;

				case PQP_STATE_READING_POWER_P:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_PHASE1;
						rwri = 0x01;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_PHASE2;
						rwri = 0x02;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_PHASE3;
						rwri = 0x03;
					}
					else
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_P_PHASE_SIGMA;
						rwri = 0x00;
					}
				}
				break;

				case PQP_STATE_READING_POWER_Q:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_PHASE1;
						rwri = 0x05;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_PHASE2;
						rwri = 0x06;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_PHASE3;
						rwri = 0x07;
					}
					else
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_Q_PHASE_SIGMA;
						rwri = 0x04;
					}
				}
				break;

				case PQP_STATE_READING_POWER_S:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_PHASE1;
						rwri = 0x09;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_PHASE2;
						rwri = 0x0A;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_PHASE3;
						rwri = 0x0B;
					}
					else
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_S_PHASE_SIGMA;
						rwri = 0x08;
					}
				}
				break;

				case PQP_STATE_READING_FREQUENCY:
				{
					(*transaction)->transactionType = TRANSACTION_GET_PQP_FREQUENCY;
					rwri = 0x40 ;
				}
				break;

				case PQP_STATE_READING_RATIO_COS:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_COS_PHASE1;
						rwri = 0x31;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_COS_PHASE2;
						rwri = 0x32;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_COS_PHASE3;
						rwri = 0x33;
					}
					else
					{
						return res ;
					}
				}
				break;

				case PQP_STATE_READING_RATIO_TAN:
				{
					if (phaseNumber == 1)
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_TAN_PHASE1;
						rwri = 0x39;
					}
					else if ( phaseNumber == 2 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_TAN_PHASE2;
						rwri = 0x3A;
					}
					else if ( phaseNumber == 3 )
					{
						(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO_TAN_PHASE3;
						rwri = 0x3B;
					}
					else
					{
						return res ;
					}
				}
				break;

				default:
					return res ;
					break;
			}
		}
		break;
	}


	tempCommand[commandSize++] = rwri;
	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->shortAnswerIsPossible = TRUE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_CreateTask(GET PQP) %s", getErrorCode(res));

	return res ;
}

//
//
//


int UJUK_Command_WriteDataToPhisicalMemory(counter_t * counter , counterTransaction_t ** transaction, unsigned short address, unsigned char * data , int dataLength ,unsigned char dataType)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_DATA_TO_PH_MEMORY_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_DATA_TO_PH_MEMORY_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x07;

	//set password
//	memcpy(&tempCommand[commandSize], counter->password1, UJUK_SIZE_OF_PASSWORD);
//	commandSize += UJUK_SIZE_OF_PASSWORD;

	tempCommand[commandSize++] = (unsigned char)0x02;
	tempCommand[commandSize++] = (unsigned char) ((address >> 8) & 0xFF);
	tempCommand[commandSize++] = (unsigned char) ((address) & 0xFF);
	tempCommand[commandSize++] = (unsigned char) (dataLength & 0xFF);

	memcpy( &tempCommand[commandSize] , data , dataLength  );
	commandSize += dataLength ;



	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = dataType;

	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_WriteDataToPhisicalMemory %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetPowerConterolRules(counter_t * counter , counterTransaction_t ** transaction, unsigned char propNumb , unsigned char * tariffNumb , unsigned int * propValue)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_POWER_CTRL_RULES_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_POWER_CTRL_RULES_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x2F;
	tempCommand[commandSize++] = propNumb;
	if ( tariffNumb != NULL )
	{
		tempCommand[commandSize++] = (*tariffNumb);
	}
	if ( propValue != NULL )
	{
		tempCommand[commandSize++] = (unsigned char) (((*propValue) >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) (((*propValue) >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) (((*propValue) >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) (((*propValue)) & 0xFF);
	}

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_SET_POWER_CONTROLS;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetPowerConterolRules() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetControlProgrammFlags(counter_t * counter , counterTransaction_t ** transaction, unsigned char propByte )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x18;
	tempCommand[commandSize++] = propByte;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_SET_PROGRAM_FLAGS;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetControlProgrammFlags() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_FixTariffMapCrc(counter_t * counter , counterTransaction_t ** transaction )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x21;


	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_SET_PROGRAM_FLAGS;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetControlProgrammFlags() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetAllowSeasonChangeFlg(counter_t * counter , counterTransaction_t ** transaction, unsigned char allowSeasonChange )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_PROGRAM_FLAGS_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x18;
	if ( allowSeasonChange == STORAGE_SEASON_CHANGE_ENABLE )
		tempCommand[commandSize++] = 0x00;
	else
		tempCommand[commandSize++] = 0x01;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_ALLOW_SEASON_CHANGING;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetAllowSeasonChangeFlg() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetPowerLimitControls(counter_t * counter , counterTransaction_t ** transaction, unsigned int byteArr )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_POWER_LIMIT_CONTROL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_WRITING_POWER_LIMIT_CONTROL_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x26;
	tempCommand[commandSize++] = (unsigned char)( byteArr >> 24 )& 0xFF;
	tempCommand[commandSize++] = (unsigned char)( byteArr >> 16 )& 0xFF;
	tempCommand[commandSize++] = (unsigned char)( byteArr >> 8 )& 0xFF;
	tempCommand[commandSize++] = (unsigned char)( byteArr & 0xFF );

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_SET_POWER_LIMIT_CONTROL;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetControlProgrammFlags() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_GetCounterStateWord(counter_t * counter , counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_COUNTER_STATE_WORD_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_GET_COUNTER_STATE_WORD_SHORT;
	}
	if ( counter->type == TYPE_UJUK_SET_4TM_01A )
	{
		--answerLen ;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x08;
	tempCommand[commandSize++] = (unsigned char)0x0A;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_GET_COUNTER_STATE_WORD ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_GetCounterStateWord() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetTariffZone(counter_t * counter , counterTransaction_t ** transaction, ujukTariffZone_t * ujukTariffZone , int trId)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

//	if ( trId > 15 )
//	{
//		while( trId > 15 )
//		{
//			trId -= 16 ;
//		}
//	}

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_TARIFF_ZONE_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_TARIFF_ZONE_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x37;
	tempCommand[commandSize++] = (unsigned char)trId & 0xFF;

	tempCommand[commandSize++] = (unsigned char)ujukTariffZone->zoneIndex ;

	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->weekDayStart.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->weekDayStart.h );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->weekDayEnd.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->weekDayEnd.h ) ;

	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->saturdayStart.m ) ;
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->saturdayStart.h ) ;
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->saturdayEnd.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->saturdayEnd.h );

	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->sundayStart.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->sundayStart.h );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->sundayEnd.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->sundayEnd.h );

	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->holidayStart.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->holidayStart.h );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->holidayEnd.m );
	tempCommand[commandSize++] = (unsigned char)COMMON_Translate10to2_10((int)ujukTariffZone->holidayEnd.h );

	tempCommand[commandSize++] = (unsigned char) ((ujukTariffZone->seasonMask >> 8) & 0xFF);
	tempCommand[commandSize++] = (unsigned char) ((ujukTariffZone->seasonMask) & 0xFF);

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_UJUK_WRITE_TARIFF_ZONE + ( trId % 10 ) ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = TRUE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetTariffZone() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetIndicationGeneral(counter_t * counter , counterTransaction_t ** transaction , unsigned int param )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x06;

	//set parmametres
	tempCommand[commandSize++] = (unsigned char)(( param >> 16 ) & 0xFF );
	tempCommand[commandSize++] = (unsigned char)(( param >> 8) & 0xFF );
	tempCommand[commandSize++] = (unsigned char)(param & 0xFF);

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_INDICATIONS ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetIndicationGeneral() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetIndicationExtended(counter_t * counter , counterTransaction_t ** transaction , unsigned int param )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x07;

	//set parmametres
	tempCommand[commandSize++] = (unsigned char)(( param >> 8) & 0xFF );
	tempCommand[commandSize++] = (unsigned char)(param & 0xFF);

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_INDICATIONS ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetIndicationExtended() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetIndicationPeriod(counter_t * counter , counterTransaction_t ** transaction , unsigned char period )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x07;

	//set period
	tempCommand[commandSize++] = (unsigned char)period;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_INDICATIONS ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetIndicationPeriod() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_Command_SetDynamicIndicationProperties(counter_t * counter , counterTransaction_t ** transaction , unsigned char property )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;
	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT;
	}

	//set command
	tempCommand[commandSize++] = (unsigned char)0x03;
	tempCommand[commandSize++] = (unsigned char)0x18;

	//set property byte
	tempCommand[commandSize++] = (unsigned char)property;

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_INDICATIONS ;
	(*transaction)->waitSize = answerLen;
	(*transaction)->shortAnswerIsPossible = FALSE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_SetynamicIndicationProperties() %s", getErrorCode(res));

	return res ;
}

//
//
//

int UJUK_SyncTime(counter_t * counter, counterTask_t ** counterTask )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL;

	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK time worth is [%d]" , counter->syncWorth );

	if ( abs(counter->syncWorth) > UJUK_SOFT_HARD_SYNC_SW )
	{
		STORAGE_SwitchSyncFlagTo_SyncProcess(&counter);
		res = UJUK_Command_SyncTimeHardy(counter, COMMON_AllocTransaction(counterTask) );

		if ( (counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL) && (!(60 % counter->profileInterval)) )
		{
			int stepTotal = 1 ;
			if( !strlen((char *)counter->serialRemote) )
			{
				//repeat transaction 3 times for 485 counter
				stepTotal = 3 ;
			}

			int stepIndex = 0 ;
			for ( ; stepIndex < stepTotal ; ++stepIndex )
			{
				res = UJUK_Command_SetProfileIntervalValue(counter, COMMON_AllocTransaction(counterTask) , counter->profileInterval );
			}
		}
	}
	else
	{		
		STORAGE_SwitchSyncFlagTo_SyncProcess(&counter);
		UJUK_Command_SyncTimeSoft(counter, COMMON_AllocTransaction(counterTask) );
	}

	return OK ;
}

int UJUK_CreateProfileTr( counter_t * counter, counterTask_t ** counterTask )
{
	DEB_TRACE(DEB_IDX_UJUK);

	int res = ERROR_GENERAL ;
	unsigned int ptrToRead = counter->profilePointer ;
	int readProfileState ;
	int canHoursRead = 1 ;

	enum UJUK_READ_PROFILE_STATE
	{
		STATE_START,

		STATE_CHECK_TIME_TO_REQUEST,
		STATE_SEARCH_PTR,
		STATE_ASK_SEARCH_STATE,
		STATE_READ_BY_PTR,

		STATE_END
	};

	readProfileState = STATE_START;

	memset( &(*counterTask)->resultProfile , 0 , sizeof(profile_t) );
	(*counterTask)->resultProfile.ratio = counter->ratioIndex & 0x0f;

	while( readProfileState != STATE_END )
	{
		switch(readProfileState)
		{
			case STATE_START:
			{

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : STATE START"); //new profile debug

				switch(	counter->profileReadingState )
				{
					default:
					case STORAGE_PROFILE_STATE_NOT_PROCESS:
						readProfileState = STATE_CHECK_TIME_TO_REQUEST ;
						break;

					case STORAGE_PROFILE_STATE_WAITING_SEARCH_RES:
						readProfileState = STATE_ASK_SEARCH_STATE ;
						break;

					case STORAGE_PROFILE_STATE_READ_BY_PTR:
						readProfileState = STATE_READ_BY_PTR ;
						break;
				}
			}
				break;

			case STATE_CHECK_TIME_TO_REQUEST:

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : CHECK TIME TO REQUEST"); //new profile debug

				//if ( STORAGE_CheckTimeToRequestProfile(counter, &ptrToRead, &(*counterTask)->resultProfile.dts) == TRUE )
				if ( STORAGE_CheckTimeToRequestProfile(counter, &ptrToRead, &counter->profileLastRequestDts, &canHoursRead) == TRUE )
				{

					COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : NEED DTS " DTS_PATTERN, DTS_GET_BY_VAL(counter->profileLastRequestDts)); //new profile debug

					if ( ptrToRead == POINTER_EMPTY )
						readProfileState = STATE_SEARCH_PTR ;
					else
						readProfileState = STATE_READ_BY_PTR ;
				}
				else
					return OK ;

				break;

			case STATE_SEARCH_PTR:

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : NEED SEARCH"); //new profile debug				
				return  UJUK_Command_GetProfilePointer(counter, COMMON_AllocTransaction(counterTask) , &counter->profileLastRequestDts);


			case STATE_ASK_SEARCH_STATE:

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : CHECK SEARCH STATE"); //new profile debug


				return UJUK_Command_GetProfileSearchState(counter , COMMON_AllocTransaction(counterTask)) ;


			case STATE_READ_BY_PTR:

				COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : READ BY PTR %04X %d hours", ptrToRead, canHoursRead); //new profile debug

				return UJUK_Command_GetProfileAllHour(counter , COMMON_AllocTransaction(counterTask) , ptrToRead, canHoursRead) ;


		} //switch(readProfileState)
	} //while( readProfileState != STATE_END )

	return res ;
}

int UJUK_Command_UserCommand(counter_t * counter, counterTransaction_t ** transaction , char * usrCommand){
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	BOOL longAddressUsed = FALSE;
	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];
	unsigned char payloadData[32];
	unsigned int payloadSize = 0;
	unsigned int pIndx = 0;

	//clear temp command
	memset(tempCommand, 0, 64);
	memset(payloadData, 0, 32);

	//make payload from usrCommand
	payloadSize = strlen(usrCommand)/2;
	for (pIndx=0; pIndx<payloadSize; pIndx++){
		unsigned char tmpHex[3];
		memset(tmpHex,0,3);
		memcpy(tmpHex, &usrCommand[pIndx*2], 2);
		payloadData[pIndx] = (strtoul((char *)tmpHex, NULL, 16) & 0xFF);
	}

	COMMON_BUF_DEBUG(DEBUG_UJUK "UJUK_Command_UserCommand payload", payloadData, payloadSize);

	//check long address is used
	longAddressUsed = UJUK_GetCounterAddress(counter, &counterAddress);
	if (longAddressUsed)
	{
		//set long address
		tempCommand[commandSize++] = UJUK_LONG_ADDRESS_START_BYTE;
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 24)& 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 16) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress >> 8) & 0xFF);
		tempCommand[commandSize++] = (unsigned char) ((counterAddress) & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_LONG;
	}
	else
	{
		//set short address
		tempCommand[commandSize++] = (unsigned char) (counterAddress & 0xFF);

		//set answer size
		answerLen = UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT;
	}

	//set command ayload
	for (pIndx = 0; pIndx < payloadSize; pIndx++){
		tempCommand[commandSize++] = payloadData[pIndx];
	}

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_USER_DEFINED_COMMAND ;
	(*transaction)->waitSize = 8;
	(*transaction)->shortAnswerIsPossible = TRUE ;
	(*transaction)->shortAnswerFlag = FALSE ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc(commandSize);
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK;
	}
	else
	{
		res = ERROR_MEM_ERROR;
	}

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_Command_UserCommand exit %s", getErrorCode(res));

	return res;
}

int UJUK_WriteUserCommands(counter_t * counter, counterTask_t ** counterTask ,unsigned char * usrCmnds, int usrCmndsLen){
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	//checking the tariff map template version

	int templateVersion = -1 ;
	int commandsCount = 0;
	int counterType = 0;
	int pwdLevel = 0;
	blockParsingResult_t * blockParsingResult = NULL ;
	int blockParsingResultLength = 0 ;

	/*
	 [CounterType]
	 Version=1.0
	 Type=32|34
	 Count=1|N
	 PwdLevel=1|2
	 [Commands]
	 cmd1=0А
	 */

	if ( COMMON_SearchBlock( (char *)usrCmnds , usrCmndsLen , "CounterType", &blockParsingResult , &blockParsingResultLength ) == OK )
	{
		if ( blockParsingResult )
		{
			int searchIndex = 0 ;

			for( searchIndex = 0 ; searchIndex < blockParsingResultLength ; ++searchIndex )
			{
				//get version
				if ( !strcmp( blockParsingResult[ searchIndex ].variable , "Version" ) )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK block version was found. It is [%s]", blockParsingResult[ searchIndex ].value );
					if ( strstr( blockParsingResult[ searchIndex ].value , "1.0" ) )
					{
						templateVersion = 1 ;
					}
				}

				//get counter type
				if ( !strcmp( blockParsingResult[ searchIndex ].variable , "Type" ) )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK counter type was found. It is [%s]", blockParsingResult[ searchIndex ].value );
					counterType = atoi( blockParsingResult[ searchIndex ].value );
				}

				//get counts
				if ( !strcmp( blockParsingResult[ searchIndex ].variable , "Count" ) )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK count of commands was found. It is [%s]", blockParsingResult[ searchIndex ].value );
					commandsCount = atoi( blockParsingResult[ searchIndex ].value );
				}

				//get pwdLevel
				if ( !strcmp( blockParsingResult[ searchIndex ].variable , "PwdLevel" ) )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK password level was found. It is [%s]", blockParsingResult[ searchIndex ].value );
					pwdLevel = atoi( blockParsingResult[ searchIndex ].value );
				}
			}

			//free head
			free( blockParsingResult );
			blockParsingResult = NULL ;
			blockParsingResultLength = 0 ;

			//check counter type
			if (counter->type != counterType)
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK user commands counter type [%d] errorm wait [%d]", counterType, counter->type);
				res = ERROR_COUNTER_TYPE;
			}
			//check consistanse of file
			else if ((templateVersion != 1) || (commandsCount == 0))
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK user commands error in tempplateVersion or count of commands");
				res = ERROR_FILE_FORMAT;
			}
			//check pwd level
			else if ((pwdLevel != 1) && (pwdLevel != 2))
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK user commands error in pwd level");
				res = ERROR_FILE_FORMAT;
			}
			//all seems is ok
			else
			{
				//do searching commands data
				blockParsingResult_t * dataParsingResult = NULL ;
				int dataParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)usrCmnds , usrCmndsLen , "Commands", &dataParsingResult , &dataParsingResultLength ) == OK )
				{
					if ( dataParsingResult )
					{
						if (dataParsingResultLength == commandsCount){

							//setup res
							res = OK;

							//loop on commands data to check len
							for( searchIndex = 0 ; searchIndex < dataParsingResultLength ; ++searchIndex )
							{
								//check len of command
								if (strlen(dataParsingResult[ searchIndex ].value) % 2){

									COMMON_STR_DEBUG( DEBUG_UJUK "UJUK command[$d] length is not aligned, break user commands");
									res = ERROR_FILE_FORMAT;
									break;
								}
							}

							//create open command
							switch (pwdLevel){
								case 1:
									//get counter channel open level 1									
									res = UJUK_Command_OpenCounter(counter, COMMON_AllocTransaction(counterTask));
									break;

								case 2:
									//get counter channel open level 2
									res = UJUK_Command_OpenCounterForWritingToPhMemmory (counter, COMMON_AllocTransaction(counterTask));
									break;
							}

							//loop on commands data to make commands
							for( searchIndex = 0 ; ((searchIndex < dataParsingResultLength) && (res == OK)) ; ++searchIndex )
							{
								//add transactions
								res = UJUK_Command_UserCommand(counter, COMMON_AllocTransaction(counterTask) , dataParsingResult[ searchIndex ].value);
							}

						} else {
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK real count of command[%d] not equal count in header[%d]", dataParsingResultLength, commandsCount);
							res = ERROR_FILE_FORMAT;
						}

						//free commands data
						free( dataParsingResult );
						dataParsingResult = NULL ;
						dataParsingResultLength = 0 ;

					} else {
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK commands data seems to be NULL");
						res = ERROR_FILE_FORMAT;
					}

				} else {
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK commands data seems to be absent");
					res = ERROR_FILE_FORMAT;
				}
			}
		}
	}

	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_WriteUserCommands exit %s", getErrorCode(res));

	return res ;
}

int UJUK_WriteTariffMap(counter_t * counter, counterTask_t ** counterTask ,unsigned char * tariff, int tarifLength)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	//checking the tariff map template version

	int templateVersion = -1 ;

	blockParsingResult_t * blockParsingResult = NULL ;
	int blockParsingResultLength = 0 ;

	counterStatus_t counterStatus ;
	memset(&counterStatus, 0 , sizeof(counterStatus_t));
	if ( STORAGE_LoadCounterStatus( counter->dbId , &counterStatus ) == OK)
	{
		counterStateWord_t emptyCounterStWrd;
		memset(&emptyCounterStWrd, 0xFF, sizeof(counterStateWord_t));

		if ( memcmp(&counterStatus.counterStateWord , &emptyCounterStWrd, sizeof(counterStateWord_t) ) )
		{
			if (counterStatus.counterStateWord.word[1] & 0x10)
			{
				//fix tariff map crc error
				UJUK_Command_FixTariffMapCrc(counter, COMMON_AllocTransaction(counterTask));
			}
		}
	}


	if ( COMMON_SearchBlock( (char *)tariff , tarifLength , "CounterType", &blockParsingResult , &blockParsingResultLength ) == OK )
	{
		if ( blockParsingResult )
		{
			int searchIndex = 0 ;

			for( searchIndex = 0 ; searchIndex < blockParsingResultLength ; ++searchIndex )
			{
				if ( !strcmp( blockParsingResult[ searchIndex ].variable , "Version" ) )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK block version was found. It is [%s]", blockParsingResult[ searchIndex ].value );
					if ( strstr( blockParsingResult[ searchIndex ].value , "1.0" ) )
					{
						templateVersion = 1 ;
					}
					else if ( strstr( blockParsingResult[ searchIndex ].value , "2.0" ) )
					{
						templateVersion = 2 ;
					}
					break;
				}
			}

			free( blockParsingResult );
			blockParsingResult = NULL ;
			blockParsingResultLength = 0 ;
		}
	}

	if ( templateVersion == 2 )
	{
		res = ERROR_FILE_FORMAT ;
		if ( (counter->type == TYPE_UJUK_PSH_4TM_05MK) ||
			 (counter->type == TYPE_UJUK_SEB_1TM_02M) ||
			 (counter->type == TYPE_UJUK_SEB_1TM_03A) )
		{
			if ( COMMON_SearchBlock( (char *)tariff , tarifLength , "Tariffs", &blockParsingResult , &blockParsingResultLength ) == OK )
			{
				if ( blockParsingResult )
				{
					res =  OK ;
					int blockParsingIndex = 0 ;
					for ( ; blockParsingIndex < blockParsingResultLength ; ++blockParsingIndex )
					{
						if ( res == OK )
						{
							//parsing block
							ujukTariffZone_t ujukTariffZone ;
							memset( &ujukTariffZone , 0 , sizeof(ujukTariffZone_t));

							sscanf( blockParsingResult[ blockParsingIndex ].variable , "Tariff_%hhu" , &ujukTariffZone.zoneIndex);
							int fields = sscanf( blockParsingResult[ blockParsingIndex ].value , "%02hhu%02hhu%02hhu%02hhu %02hhu%02hhu%02hhu%02hhu %02hhu%02hhu%02hhu%02hhu %02hhu%02hhu%02hhu%02hhu %u" ,
									&ujukTariffZone.weekDayStart.m , &ujukTariffZone.weekDayStart.h , &ujukTariffZone.weekDayEnd.m , &ujukTariffZone.weekDayEnd.h ,
									&ujukTariffZone.saturdayStart.m , &ujukTariffZone.saturdayStart.h , &ujukTariffZone.saturdayEnd.m , &ujukTariffZone.saturdayEnd.h ,
									&ujukTariffZone.sundayStart.m , &ujukTariffZone.sundayStart.h , &ujukTariffZone.sundayEnd.m , &ujukTariffZone.sundayEnd.h ,
									&ujukTariffZone.holidayStart.m , &ujukTariffZone.holidayStart.h , &ujukTariffZone.holidayEnd.m , &ujukTariffZone.holidayEnd.h ,
									&ujukTariffZone.seasonMask );

							//there is 17 fiels to parse in config string
							if (fields == 17)
							{
								//add transaction
								res = UJUK_Command_SetTariffZone(counter, COMMON_AllocTransaction(counterTask), &ujukTariffZone , blockParsingIndex );
								(*counterTask)->tariffMapTransactionCounter++;

							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_UJUK "UJUK tariff string format error. Trere is only [%d] fields" , fields);
							}
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK res is [%s]. break loop" , getErrorCode(res) );
							break;
						}
					}


					free( blockParsingResult );
					blockParsingResult = NULL ;
					blockParsingResultLength = 0 ;
				}
			}

			if ( res == OK )
			{
				ujukMovedDaysList_t ujukMovedDaysList ;
				memset( &ujukMovedDaysList , 0 , sizeof(ujukMovedDaysList_t) );

				if ( UJUK_GetMovedDaysList(&ujukMovedDaysList , tariff , tarifLength) == OK )
				{
					//
					//writing moved day list
					//

					int bufLength = 0 ;
					int bufPos = 0 ;
					unsigned char * buf = NULL ;
					unsigned short startMovedDayAddr = 0 ;

					bufLength = ( ujukMovedDaysList.dataSize * 4 ) + 1 ;

					buf = calloc( bufLength , sizeof(unsigned char) );
					if ( buf )
					{
						int dataIndex = 0 ;

						for( ; dataIndex < ujukMovedDaysList.dataSize ; ++dataIndex )
						{
							buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF000000 ) >> 24;
							buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF0000 ) >> 16;
							buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF00 ) >> 8;
							buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF );
						}


						buf[ bufPos ] = ujukMovedDaysList.crc ;

						switch(counter->type )
						{
							case TYPE_UJUK_SET_4TM_02MB:
							case TYPE_UJUK_SET_4TM_03A:
							case TYPE_UJUK_PSH_4TM_05A:
							case TYPE_UJUK_PSH_4TM_05MD:
							case TYPE_UJUK_PSH_4TM_05D:
							case TYPE_UJUK_PSH_4TM_05MB:
							case TYPE_UJUK_PSH_4TM_05MK:
							case TYPE_UJUK_PSH_3TM_05A:
							case TYPE_UJUK_PSH_3TM_05D:
							case TYPE_UJUK_PSH_3TM_05MB:
							case TYPE_UJUK_SET_4TM_03MB:
							case TYPE_UJUK_SEB_1TM_02M:
							case TYPE_UJUK_SEB_1TM_03A:
							{
								startMovedDayAddr = 0x3B80 ;
							}
								break;

							case TYPE_UJUK_SEB_1TM_01A:
							case TYPE_UJUK_SEB_1TM_02A:
							case TYPE_UJUK_SEB_1TM_02D:
							{
								startMovedDayAddr = 0x0B10 ;
							}
								break;

							default:
								break;
						}

						if ( startMovedDayAddr > 0 )
						{
							bufPos = 0 ;
							while(1)
							{
								int bytesNumb = bufLength - bufPos ;

								if ( bytesNumb == 0 )
									break;

								if ( bytesNumb > UJUK_MAX_PH_MEM_BYTES )
									bytesNumb = UJUK_MAX_PH_MEM_BYTES ;

								if (res == OK) {
									res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
											startMovedDayAddr , &buf[ bufPos ] , bytesNumb , TRANSACTION_WRITING_TARIFF_MOVED_DAYS_TO_PH_MEMMORY);
									(*counterTask)->tariffMovedDaysTransactionsCounter++;
								}


								startMovedDayAddr += bytesNumb ;
								bufPos += bytesNumb ;
							}


						}
						free( buf );
						buf = NULL ;
					}
					else
						return ERROR_MEM_ERROR ;
				}
			}

			if ( res == OK )
			{
				//
				//writing holiday list
				//

				ujukHolidayArr_t ujukHolidayArr ;
				memset( &ujukHolidayArr , 0 , sizeof(ujukHolidayArr_t) );

				if ( UJUK_GetHolidayArray(&ujukHolidayArr , tariff , tarifLength) == OK )
				{
					unsigned short startHolidayAddress = 0 ;
					switch( counter->type )
					{
						case TYPE_UJUK_SEB_1TM_01A:
						case TYPE_UJUK_SEB_1TM_02A:
						case TYPE_UJUK_SEB_1TM_02D:
							startHolidayAddress = 0x0400 ;
							break;

						default:
							startHolidayAddress = 0x2000 ;
							break;
					}
					int holidayArrPos = 0 ;
					while(1)
					{
						int bytesNumb = UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE - holidayArrPos ;

						if ( bytesNumb == 0 )
							break;

						if ( bytesNumb > UJUK_MAX_PH_MEM_BYTES )
							bytesNumb = UJUK_MAX_PH_MEM_BYTES ;

						if (res == OK) {
							res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
									startHolidayAddress , &ujukHolidayArr.data[ holidayArrPos ] , bytesNumb , TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY);
							(*counterTask)->tariffHolidaysTransactionCounter++;
						}

						holidayArrPos += bytesNumb ;
						startHolidayAddress += bytesNumb ;

					}

					//writing holiday list CRC

					if (res == OK) {
						res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
								startHolidayAddress , &ujukHolidayArr.crc , 1 , TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY);
						(*counterTask)->tariffHolidaysTransactionCounter++;
					}
				}
			}
		}

	}
	else if ( templateVersion == 1 )
	{
		ujukHolidayArr_t ujukHolidayArr ;
		ujukTariffMap_t ujukTariffMap ;
		ujukMovedDaysList_t ujukMovedDaysList ;

		int tariffTotal = 0 ;
		res = UJUK_GetTariffMap(&ujukTariffMap , tariff , tarifLength , &tariffTotal);
		if ( res != OK )
			return res ;

		res = UJUK_GetMovedDaysList(&ujukMovedDaysList , tariff , tarifLength);
		if (res != OK)
			return res ;

		res = UJUK_GetHolidayArray(&ujukHolidayArr , tariff , tarifLength);

		if (res!=OK)
			return res ;

		switch (counter->type)
		{
			case TYPE_UJUK_SET_4TM_01A:
			case TYPE_UJUK_SET_4TM_02A:
			case TYPE_UJUK_SET_4TM_03A:
			case TYPE_UJUK_SET_4TM_02MB:
			case TYPE_UJUK_SET_4TM_03MB:
			{
				if (tariffTotal != 8)
					return ERROR_GENERAL;
			}
				break;

			default:
			{
				if (tariffTotal != 4)
					return ERROR_GENERAL;
			}
				break;
		}

		//
		//writing holiday list
		//

		unsigned short startHolidayAddress = 0 ;
		switch( counter->type )
		{
			case TYPE_UJUK_SEB_1TM_01A:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SEB_1TM_02D:
				startHolidayAddress = 0x0400 ;
				break;

			default:
				startHolidayAddress = 0x2000 ;
				break;
		}
		int holidayArrPos = 0 ;
		while(1)
		{
			int bytesNumb = UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE - holidayArrPos ;

			if ( bytesNumb == 0 )
				break;

			if ( bytesNumb > UJUK_MAX_PH_MEM_BYTES )
				bytesNumb = UJUK_MAX_PH_MEM_BYTES ;

			if (res == OK)
			{
				res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
						startHolidayAddress , &ujukHolidayArr.data[ holidayArrPos ] , bytesNumb , TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY);
				(*counterTask)->tariffHolidaysTransactionCounter++;
			}

			holidayArrPos += bytesNumb ;
			startHolidayAddress += bytesNumb ;

		}

		//writing holiday list CRC

		if (res == OK)
		{
			res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
					startHolidayAddress , &ujukHolidayArr.crc , 1 , TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY);
			(*counterTask)->tariffHolidaysTransactionCounter++;
		}

		//
		//writing tariff map
		//

		unsigned short startTariffMapAddr = 0 ;
		int bufLength = 0 ;
		int bufPos = 0 ;
		unsigned char * buf = NULL ;

		switch (counter->type)
		{
			case TYPE_UJUK_SET_4TM_01A:
			case TYPE_UJUK_SET_4TM_02A:
			case TYPE_UJUK_SET_4TM_03A:
			case TYPE_UJUK_SET_4TM_02MB:
			case TYPE_UJUK_SET_4TM_03MB:
			{
				startTariffMapAddr = 0x2040 ;

				bufLength = (12 /*month number*/ * 8 /*day types number*/ * 72 /*bytes in 1 day type*/) + 1 /*crc*/ ;

				buf = calloc( bufLength , sizeof(unsigned char) );
				if (buf)
				{

					int monthIndex = 0 ;
					for( ; monthIndex < 12 ; ++monthIndex )
					{
						int dayIndex = 0 ;
						for( ; dayIndex < 8 ; ++dayIndex)
						{
							memcpy( &buf[bufPos] , &ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].data , ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].dataSize );
							bufPos += 72 ;
						}
					}

					buf[ bufPos ] = ujukTariffMap.yearCrc ;
				}
				else
					return ERROR_MEM_ERROR ;

			}
			break;

			case TYPE_UJUK_PSH_4TM_05A:
			case TYPE_UJUK_PSH_4TM_05D:
			case TYPE_UJUK_PSH_4TM_05MD:
			case TYPE_UJUK_PSH_4TM_05MB:
			case TYPE_UJUK_PSH_4TM_05MK:
			case TYPE_UJUK_PSH_3TM_05A:
			case TYPE_UJUK_PSH_3TM_05MB:
			case TYPE_UJUK_PSH_3TM_05D:
			case TYPE_UJUK_SEB_1TM_02A:
			case TYPE_UJUK_SEB_1TM_02D:
			case TYPE_UJUK_SEB_1TM_02M:
			case TYPE_UJUK_SEB_1TM_03A:
			{
				startTariffMapAddr = 0x2040 ;

				bufLength = (12 /*month number*/ * 4 /*day types number*/ * 36 /*bytes in 1 day type*/) + 1 /*crc*/ ;
				buf = calloc( bufLength , sizeof(unsigned char) );
				if (buf)
				{

					int monthIndex = 0 ;
					for( ; monthIndex < 12 ; ++monthIndex )
					{
						int dayIndex = 0 ;
						for( ; dayIndex < 4 ; ++dayIndex)
						{
							memcpy( &buf[bufPos] , &ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].data , ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].dataSize );
							bufPos += 36 ;
						}
					}

					buf[ bufPos ] = ujukTariffMap.yearCrc ;
				}
				else
					return ERROR_MEM_ERROR ;
			}
			break;


			default:
			{

				startTariffMapAddr = 0x0440 ;

				bufLength = (12 /*month number*/ * 4 /*day types number*/ * 36 /*bytes in 1 day type*/) + 1 /*crc*/ ;
				buf = calloc( bufLength , sizeof(unsigned char) );
				if (buf)
				{

					int monthIndex = 0 ;
					for( ; monthIndex < 12 ; ++monthIndex )
					{
						int dayIndex = 0 ;
						for( ; dayIndex < 4 ; ++dayIndex)
						{
							memcpy( &buf[bufPos] , &ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].data , ujukTariffMap.monthMap[ monthIndex ].day[dayIndex].dataSize );
							bufPos += 36 ;
						}
					}

					buf[ bufPos ] = ujukTariffMap.yearCrc ;

				}
				else
					return ERROR_MEM_ERROR ;

			}
			break;
		} //switch()


		bufPos = 0 ;
		while(1)
		{
			int bytesNumb = bufLength - bufPos ;

			if ( bytesNumb == 0 )
				break;

			if ( bytesNumb > UJUK_MAX_PH_MEM_BYTES )
				bytesNumb = UJUK_MAX_PH_MEM_BYTES ;

			if (res == OK)
			{
				res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
						startTariffMapAddr , &buf[ bufPos ] , bytesNumb , TRANSACTION_WRITING_TARIFF_MAP_TO_PH_MEMORY);
				(*counterTask)->tariffMapTransactionCounter++;
			}


			startTariffMapAddr += bytesNumb ;
			bufPos += bytesNumb ;
		}

		free(buf);

		//
		//writing moved day list
		//

		//unsigned char * buf = NULL ;
		//bufLength = 0 ;
		bufPos = 0 ;
		unsigned short startMovedDayAddr = 0 ;

		bufLength = ( ujukMovedDaysList.dataSize * 4 ) + 1 ;

		buf = calloc( bufLength , sizeof(unsigned char) );
		if ( buf )
		{
			int dataIndex = 0 ;

			for( ; dataIndex < ujukMovedDaysList.dataSize ; ++dataIndex )
			{
				buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF000000 ) >> 24;
				buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF0000 ) >> 16;
				buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF00 ) >> 8;
				buf[ bufPos++ ] = ( ujukMovedDaysList.data[ dataIndex ] & 0xFF );
			}


			buf[ bufPos ] = ujukMovedDaysList.crc ;

			switch(counter->type )
			{
				case TYPE_UJUK_SET_4TM_02MB:
				case TYPE_UJUK_SET_4TM_03A:
				case TYPE_UJUK_PSH_4TM_05A:
				case TYPE_UJUK_PSH_4TM_05D:
				case TYPE_UJUK_PSH_4TM_05MD:
				case TYPE_UJUK_PSH_4TM_05MB:
				case TYPE_UJUK_PSH_4TM_05MK:
				case TYPE_UJUK_PSH_3TM_05A:
				case TYPE_UJUK_PSH_3TM_05D:
				case TYPE_UJUK_PSH_3TM_05MB:
				case TYPE_UJUK_SET_4TM_03MB:
				case TYPE_UJUK_SEB_1TM_02M:
				case TYPE_UJUK_SEB_1TM_03A:
				{
					startMovedDayAddr = 0x3B80 ;
				}
					break;

				case TYPE_UJUK_SEB_1TM_01A:
				case TYPE_UJUK_SEB_1TM_02A:
				case TYPE_UJUK_SEB_1TM_02D:
				{
					startMovedDayAddr = 0x0B10 ;
				}
					break;

				default:
					break;

			}

			if ( startMovedDayAddr > 0 )
			{
				bufPos = 0 ;
				while(1)
				{
					int bytesNumb = bufLength - bufPos ;

					if ( bytesNumb == 0 )
						break;

					if ( bytesNumb > UJUK_MAX_PH_MEM_BYTES )
						bytesNumb = UJUK_MAX_PH_MEM_BYTES ;

					if (res == OK) {

						res = UJUK_Command_WriteDataToPhisicalMemory(counter, COMMON_AllocTransaction(counterTask) , \
								startMovedDayAddr , &buf[ bufPos ] , bytesNumb , TRANSACTION_WRITING_TARIFF_MOVED_DAYS_TO_PH_MEMMORY);
						(*counterTask)->tariffMovedDaysTransactionsCounter++;

					}


					startMovedDayAddr += bytesNumb ;
					bufPos += bytesNumb ;
				}


			}




			free( buf );
		}
		else
			return ERROR_MEM_ERROR ;

	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Can not find version in tariff data. Do not write tariff");
		res = ERROR_FILE_FORMAT ;
	}

	//parsing indications
	if ( res == OK )
	if ( COMMON_SearchBlock( (char *)tariff , tarifLength , "indication", &blockParsingResult , &blockParsingResultLength ) == OK )
	{
		if ( blockParsingResult )
		{
			unsigned char counterType = 0 ;
			//searching for counterType
			int blockParsingResultIndex = 0 ;
			for ( ; blockParsingResultIndex < blockParsingResultLength ; ++blockParsingResultIndex)
			{
				if ( !strcmp(blockParsingResult[ blockParsingResultIndex ].variable , "CounterType") )
				{
					counterType = (unsigned char)atoi( blockParsingResult[ blockParsingResultIndex ].value ) ;
				}
			}

			if ( counterType == 0 )
			{
				res = ERROR_FILE_FORMAT ;
			}
			else
			{
				if ( counterType != counter->type )
				{
					res = ERROR_COUNTER_TYPE ;
				}
			}

			if ( res == OK )
			{
				for ( blockParsingResultIndex = 0 ; blockParsingResultIndex < blockParsingResultLength ; ++blockParsingResultIndex)
				{
					if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_0") )
					{
						unsigned char bori = 0 ;
						unsigned char nit = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &bori , &nit);
						unsigned int param = ( bori << 8 ) + nit;

						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;

						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_1") )
					{
						unsigned char bwri = 0 ;
						unsigned char nit = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &bwri , &nit);
						unsigned int param = (1 << 16) + ( bwri << 8 ) + nit;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_2") )
					{
						unsigned char hi = 0 ;
						unsigned char lo = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &hi , &lo);
						unsigned int param = (2 << 16) + ( hi << 8 ) + lo;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_3") )
					{
						unsigned char hi = 0 ;
						unsigned char lo = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &hi , &lo);
						unsigned int param = (3 << 16) + ( hi << 8 ) + lo;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_4") )
					{
						unsigned char hi = 0 ;
						unsigned char lo = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &hi , &lo);
						unsigned int param = (4 << 16) + ( hi << 8 ) + lo;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_5") )
					{
						unsigned char hi = 0 ;
						unsigned char lo = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &hi , &lo);
						unsigned int param = (5 << 16) + ( hi << 8 ) + lo;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_6") )
					{
						unsigned char bori = 0 ;
						unsigned char nit = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &bori , &nit);
						unsigned int param = (6 << 16) + ( bori << 8 ) + nit;
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CMD_7") )
					{
						unsigned char btri = 0 ;
						unsigned char zero = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &btri , &zero);
						unsigned int param = (7 << 16) + ( btri << 8 );
						if (res == OK)
						{
							res = UJUK_Command_SetIndicationGeneral(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( strstr( blockParsingResult[ blockParsingResultIndex ].variable , "IndExtParams_") )
					{
						unsigned char hi = 0 ;
						unsigned char lo = 0;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value ,  "%02hhx %02hhx" , &hi , &lo);
						unsigned int param = ( hi << 8 ) + lo ;

						if (res == OK)
						{
							res = UJUK_Command_SetIndicationExtended(counter, COMMON_AllocTransaction(counterTask) , param);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "IndPrd") )
					{
						unsigned char period = 0 ;
						sscanf( blockParsingResult[ blockParsingResultIndex ].value , "%02hhx" , &period ) ;

						if (res == OK)
						{
							res = UJUK_Command_SetIndicationPeriod(counter, COMMON_AllocTransaction(counterTask) , period);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "AllowDynInd") )
					{
						unsigned char property_byte = 0x4D ;
						if ( atoi( blockParsingResult[ blockParsingResultIndex ].value ) != 0 )
						{
							property_byte = 0x4C ;
						}
						if (res == OK)
						{
							res = UJUK_Command_SetDynamicIndicationProperties(counter, COMMON_AllocTransaction(counterTask) , property_byte);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "AllowGivenInd") )
					{
						unsigned char property_byte = 0x4F ;
						if ( atoi( blockParsingResult[ blockParsingResultIndex ].value ) != 0 )
						{
							property_byte = 0x4E ;
						}
						if (res == OK)
						{
							res = UJUK_Command_SetDynamicIndicationProperties(counter, COMMON_AllocTransaction(counterTask) , property_byte);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
					else if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "AllowIndModeStore") )
					{
						unsigned char property_byte = 0x05 ;
						if ( atoi( blockParsingResult[ blockParsingResultIndex ].value ) != 0 )
						{
							property_byte = 0x04 ;
						}
						if (res == OK)
						{
							res = UJUK_Command_SetDynamicIndicationProperties(counter, COMMON_AllocTransaction(counterTask) , property_byte);
							(*counterTask)->indicationWritingTransactionCounter++;
						}
					}
				}
			}

			free( blockParsingResult ) ;
			blockParsingResult = NULL ;
			blockParsingResultLength = 0 ;
		}

	}


	return res ;
}
//
//
//

int UJUK_WritePSM(counter_t * counter, counterTask_t ** counterTask ,unsigned char * psmRules, int psmRulesLength , unsigned char counterRatio)
{
	DEB_TRACE(DEB_IDX_UJUK);

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM start");

	int res = ERROR_GENERAL ;

	unsigned char tariffNumb = 0 ;
	unsigned int propValue = 0 ;

	enum
	{
		PARSING_COUNTER_TYPE ,
		PARSING_INSTANTANTEOUS_CONTROL ,
		PARSING_STATE_VOLTAGE_CONTROL ,
		PARSING_DAILY_LIMITS ,
		PARSING_DAILY_LIMITS_T1 ,
		PARSING_DAILY_LIMITS_T2 ,
		PARSING_DAILY_LIMITS_T3 ,
		PARSING_DAILY_LIMITS_T4 ,
		PARSING_DAILY_LIMITS_TS ,

		PARSING_CALC_PERIOD_LIMITS ,
		PARSING_CALC_PERIOD_LIMITS_T1 ,
		PARSING_CALC_PERIOD_LIMITS_T2 ,
		PARSING_CALC_PERIOD_LIMITS_T3 ,
		PARSING_CALC_PERIOD_LIMITS_T4 ,
		PARSING_CALC_PERIOD_LIMITS_TS ,

		PARSING_POWER_LIMIT ,
		PARSING_STATE_PROFILE_1 ,
		PARSING_STATE_PROFILE_2 ,
		PARSING_STATE_PROFILE_3 ,

		PARSING_OVERHEAT_CHECK ,

		PARSING_STATE_DONE
	};

	int parsingState = PARSING_COUNTER_TYPE ;
	while( parsingState != PARSING_STATE_DONE )
	{
		switch( parsingState )
		{
			case PARSING_COUNTER_TYPE:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_COUNTER_TYPE");

				parsingState = PARSING_STATE_DONE;
				res = ERROR_COUNTER_TYPE ;

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;
				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "counter", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "model" ) )
							{
								if( atoi( blockParsingResult[ propertyIndex ].value ) == counter->type )
								{
									parsingState = PARSING_INSTANTANTEOUS_CONTROL ;
									res = OK ;
								}
								break;
							}
						}

						free( blockParsingResult );
					}
				}

				if ( res != OK )
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM counter type is not equal");
					return res ;
				}

			}
			break;

			//
			//
			//

			case PARSING_INSTANTANTEOUS_CONTROL:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_INSTANTANTEOUS_CONTROL");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;
				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "instantaneous_control", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadPowerOn") )
							{
								if ( atoi( blockParsingResult[ propertyIndex ].value) == 1 )
								{
									int stepTotal = 1 ;
									if( !strlen((char *)counter->serialRemote) )
									{
										//repeat transaction 3 times for 485 counter
										stepTotal = 3 ;
									}

									int stepIndex = 0 ;
									for ( ; stepIndex < stepTotal ; ++stepIndex )
									{
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0 , NULL , NULL );
										}
									}

								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "loadPowerOff") )
							{
								if ( atoi( blockParsingResult[ propertyIndex ].value) == 1 )
								{
									int stepTotal = 1 ;
									if( !strlen((char *)counter->serialRemote) )
									{
										//repeat transaction 3 times for 485 counter
										stepTotal = 3 ;
									}

									int stepIndex = 0 ;
									for ( ; stepIndex < stepTotal ; ++stepIndex )
									{
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 1 , NULL , NULL );
										}
									}
								}
							}

						}

						free( blockParsingResult );
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_STATE_VOLTAGE_CONTROL;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}

			}
			break;

			//
			//
			//

			case PARSING_STATE_VOLTAGE_CONTROL:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_STATE_VOLTAGE_CONTROL");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "mains_voltage_contol", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "mainsUpperThresholdVoltage") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x09 , &tariffNumb , &propValue );
								}

							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "mainsLowerThresholdVoltage") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0D , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "mainsThresholdVoltageHysteresis") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0A , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "mainsAveragingPeriodsQty") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0B , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "powerOnLagAfterVoltageReturnToGivenLimits") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0C , &tariffNumb , &propValue );
								}
							}
						}

						free( blockParsingResult );
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtDailyEnergyLimitOvershoot") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 1 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x3A  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x39  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtDailyEnergyLimitOvershoot_Tariff") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 1 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x51  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x50  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "totalEnergy") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}

						free( blockParsingResult );
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS_T1;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}

			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS_T1:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS_T1");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								tariffNumb = 0x01 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								tariffNumb = 0x11 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								tariffNumb = 0x21 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								tariffNumb = 0x31 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS_T2;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS_T2:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS_T2");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								tariffNumb = 0x02 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								tariffNumb = 0x12 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								tariffNumb = 0x22 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								tariffNumb = 0x32 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS_T3;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS_T3:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS_T3");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								tariffNumb = 0x03 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								tariffNumb = 0x13 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								tariffNumb = 0x23 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								tariffNumb = 0x33 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS_T4;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS_T4:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS_T4");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								tariffNumb = 0x04 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								tariffNumb = 0x14 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								tariffNumb = 0x24 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								tariffNumb = 0x34 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_DAILY_LIMITS_TS;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_DAILY_LIMITS_TS:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_DAILY_LIMITS_TS");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "daily_limit_of_energy.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								tariffNumb = 0x00 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								tariffNumb = 0x10 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								tariffNumb = 0x20 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								tariffNumb = 0x30 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;
								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x0E , &tariffNumb , &propValue );
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtDailyEnergyLimitOvershoot") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;

								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x07 , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowAccountingPeriodFromGivenDay") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 1 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x20  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x21  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtAccountingPeriodEnergyOvershoot") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 1 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x53  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x52  );
									}
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS_T1;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS_T1:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS_T1");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										tariffNumb = 0x01 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;

									default:
									{
										tariffNumb = 0x01 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x11 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x21 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x31 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}

						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS_T2;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS_T2:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS_T2");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										tariffNumb = 0x02 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;

									default:
									{
										tariffNumb = 0x02 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x12 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x22 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x32 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}

						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS_T3;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS_T3:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS_T3");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										tariffNumb = 0x03 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;

									default:
									{
										tariffNumb = 0x03 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x13 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x23 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x33 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}

						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS_T4;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS_T4:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS_T4");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										tariffNumb = 0x04 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;

									default:
									{
										tariffNumb = 0x04 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x14 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x24 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x34 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}

						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_CALC_PERIOD_LIMITS_TS;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_CALC_PERIOD_LIMITS_TS:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_CALC_PERIOD_LIMITS_TS");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "calc_period_energy_limit.T1", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										tariffNumb = 0x00 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;

									default:
									{
										tariffNumb = 0x00 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x10 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x20 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-") )
							{
								switch( counter->type )
								{
									case TYPE_UJUK_SEB_1TM_02D:
									case TYPE_UJUK_SEB_1TM_02A:
									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									{
										res = ERROR_FILE_FORMAT ;
									}
									break;

									default:
									{
										tariffNumb = 0x30 ;
										propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) * 2 * (unsigned int)COMMON_GetRatioValueUjuk( counterRatio ) ;

										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x04 , &tariffNumb , &propValue );
										}
									}
									break;
								}
							}

						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_LIMIT;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_POWER_LIMIT:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_POWER_LIMIT");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				int powerThreshold = -1 ;
				int averagingTimeForPowerThreshold = -1 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "power_limit", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "onSaturday") )
							{
								tariffNumb = 1 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;

								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x08 , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "onSunday") )
							{
								tariffNumb = 2 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;

								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x08 , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "onWorkdays") )
							{
								tariffNumb = 0 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;

								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x08 , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "onHoliday") )
							{
								tariffNumb = 3 ;
								propValue = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value ) ;

								if ( res == OK )
								{
									((*counterTask)->powerControlTransactionCounter)++;
									res = UJUK_Command_SetPowerConterolRules( counter, COMMON_AllocTransaction(counterTask) , 0x08 , &tariffNumb , &propValue );
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "numberOfUsedAlgorithm") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 0 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x4B  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x4A  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "powerThreshold") )
							{
								powerThreshold = atoi(blockParsingResult[ propertyIndex ].value) ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "averagingTimeForPowerThreshold") )
							{
								averagingTimeForPowerThreshold = atoi(blockParsingResult[ propertyIndex ].value) ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtPowerLevelOvershoot") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 0 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x31  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x32  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlInVoltageControlMode") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 0 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x37  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x38  );
									}
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadPowerOnWithoutButtonPress") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 0 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x33  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x34  );
									}
								}
							}
						}
					}
				}

				if ( ( powerThreshold >= 0 ) && ( averagingTimeForPowerThreshold >= 0 ) )
				{
					unsigned int byteArr = (0x07000000 ) | ( ( powerThreshold << 16 ) & 0x00FF0000 ) | ( averagingTimeForPowerThreshold & 0x0000FFFF ) ;
					if ( res == OK )
					{
						((*counterTask)->powerControlTransactionCounter)++;
						res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_STATE_PROFILE_1;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_STATE_PROFILE_1:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_STATE_PROFILE_1");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				unsigned int profileStorageNumb = 0 ;
				if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
				{
					if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "power_limit.profile0", &blockParsingResult , &blockParsingResultLength ) == OK )
					{
						if ( blockParsingResult )
						{
							int propertyIndex = 0 ;
							for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
							{
								if ( strstr( blockParsingResult[ propertyIndex ].variable , "P+") )
								{
									unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
											(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
									unsigned int byteArr = (0x08000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
									}
								}
								else if ( strstr( blockParsingResult[ propertyIndex ].variable , "P-") )
								{
									unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
											(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
									unsigned int byteArr = (0x09000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
									}
								}
								else if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q+") )
								{
									unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
											(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
									unsigned int byteArr = (0x0A000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
									}
								}
								if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q-") )
								{
									unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
											(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
									unsigned int byteArr = (0x0B000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
									}
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_STATE_PROFILE_2;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_STATE_PROFILE_2:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_STATE_PROFILE_2");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
				{
					unsigned int profileStorageNumb = 1 ;
					if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
					{
						if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "power_limit.profile1", &blockParsingResult , &blockParsingResultLength ) == OK )
						{
							if ( blockParsingResult )
							{
								int propertyIndex = 0 ;
								for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
								{
									if ( strstr( blockParsingResult[ propertyIndex ].variable , "P+") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x08000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									else if ( strstr( blockParsingResult[ propertyIndex ].variable , "P-") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x09000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									else if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q+") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x0A000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q-") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x0B000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}

									}
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_STATE_PROFILE_3;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_STATE_PROFILE_3:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_STATE_PROFILE_3");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
				{
					unsigned int profileStorageNumb = 2 ;
					if ( counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL )
					{
						if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "power_limit.profile2", &blockParsingResult , &blockParsingResultLength ) == OK )
						{
							if ( blockParsingResult )
							{
								int propertyIndex = 0 ;
								for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
								{
									if ( strstr( blockParsingResult[ propertyIndex ].variable , "P+") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x08000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									else if ( strstr( blockParsingResult[ propertyIndex ].variable , "P-") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x09000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									else if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q+") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x0A000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}
									}
									if ( strstr( blockParsingResult[ propertyIndex ].variable , "Q-") )
									{
										unsigned int limit = (unsigned int)atoi( blockParsingResult[ propertyIndex ].value) * 2 * \
												(unsigned int)COMMON_GetRatioValueUjuk(counterRatio) * (unsigned int)counter->profileInterval / 60000 ;
										unsigned int byteArr = (0x0B000000 ) | ( ( profileStorageNumb << 24 ) & 0x00FF0000 ) | ( limit & 0x0000FFFF ) ;
										if ( res == OK )
										{
											((*counterTask)->powerControlTransactionCounter)++;
											res = UJUK_Command_SetPowerLimitControls( counter, COMMON_AllocTransaction(counterTask) , byteArr  );
										}

									}
								}
							}
						}
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_OVERHEAT_CHECK;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}
			}
			break;

			//
			//
			//

			case PARSING_OVERHEAT_CHECK:
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_WritePSM parsingState PARSING_OVERHEAT_CHECK");

				blockParsingResult_t * blockParsingResult = NULL ;
				int blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)psmRules , psmRulesLength , "overheat_check", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex )
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "allowLoadControlAtCounterOverheat") )
							{
								if ( atoi(blockParsingResult[ propertyIndex ].value) == 0 )
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x2F  );
									}
								}
								else
								{
									if ( res == OK )
									{
										((*counterTask)->powerControlTransactionCounter)++;
										res = UJUK_Command_SetControlProgrammFlags( counter, COMMON_AllocTransaction(counterTask) , 0x30  );
									}
								}
							}
						}
					}
				}


				parsingState = PARSING_STATE_DONE ;

			}
			break;

			//
			//
			//

			default:
			{
				parsingState = PARSING_STATE_DONE ;
			}
			break;
		}
	}

	if ( (res == OK) && (((*counterTask)->powerControlTransactionCounter) == 0 ) )
	{
		res = ERROR_FILE_FORMAT ;
	}



	return res ;
}

//
//
//

int UJUK_ShuffleTask(counter_t * counter, counterTask_t ** counterTask)
{
	DEB_TRACE(DEB_IDX_UJUK);

	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_ShuffleTask started" );

	int res = OK ;
	if ( (*counterTask)->transactionsCount < 3 )
	{
		COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_ShuffleTask there is only [%d] transactions" , (*counterTask)->transactionsCount ) ;
		return res ;
	}

	//debug
//	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_ShuffleTask transactions before shuffling:");
	int i=0;
//	for( i=0 ; i < (*counterTask)->transactionsCount ; ++i)
//	{
//		COMMON_STR_DEBUG( DEBUG_UJUK "\ttransaction [%02X] waitLength [%d]", (*counterTask)->transactions[i]->transactionType , (*counterTask)->transactions[i]->waitSize);
//	}

	unsigned int transactionIndex = 1 ;
	while( transactionIndex < (*counterTask)->transactionsCount )
	{
		//shuffling

		unsigned int currentTransactionWaitSize = (*counterTask)->transactions[ transactionIndex ]->waitSize ;
		unsigned int prevTransactionWaitSize = (*counterTask)->transactions[ transactionIndex - 1 ]->waitSize ;
		if ( currentTransactionWaitSize != prevTransactionWaitSize )
		{
			//not need shuffle. step next transaction
			++transactionIndex ;
		}
		else
		{
			//search for next transaction which waitSize is not equal previous transaction waitSize
			BOOL changePossible = FALSE ;
			unsigned int changeTransactionIndex = transactionIndex ;
			for ( ; changeTransactionIndex < (*counterTask)->transactionsCount ; ++changeTransactionIndex )
			{
				if ( (*counterTask)->transactions[ changeTransactionIndex ]->waitSize != prevTransactionWaitSize)
				{
					changePossible = TRUE ;
					break;
				}
			}

			if ( changePossible == TRUE )
			{
				// change two transactions
				counterTransaction_t * tempTransactionPtr = (*counterTask)->transactions[ changeTransactionIndex ] ;
				(*counterTask)->transactions[ changeTransactionIndex ] = (*counterTask)->transactions[ transactionIndex ] ;
				(*counterTask)->transactions[ transactionIndex ] = tempTransactionPtr ;
			}
			else
			{
				//add transaction
				if ( res == OK )
				{
					res = UJUK_Command_GetCounterStateWord (counter, COMMON_AllocTransaction(counterTask));
				}
				else
				{
					return res ;
				}
			}
		}
	}

	//debug
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_ShuffleTask transactions after shuffling:");
	for( i=0 ; i < (int)(*counterTask)->transactionsCount ; ++i)
	{
		COMMON_STR_DEBUG( DEBUG_UJUK "\ttransaction [%02X] waitLength [%d]", (*counterTask)->transactions[i]->transactionType , (*counterTask)->transactions[i]->waitSize);
	}


	return res ;
}

//
//
//

int UJUK_CreateCounterTask(counter_t * counter, counterTask_t ** counterTask)
{
	DEB_TRACE(DEB_IDX_UJUK);

	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK_CreateCounterTask start for counter = [%ld], type is [%d]" , counter->dbId , counter->type );

	int res = ERROR_GENERAL;
	//dateTimeStamp dtsToday;

	//allocate task
	(*counterTask) = (counterTask_t *) malloc(sizeof (counterTask_t));
	if ((*counterTask) == NULL)
		return ERROR_MEM_ERROR;

	//setup shuffle flag
	(*counterTask)->needToShuffle = TRUE ;

	//setup transactions pointers
	(*counterTask)->transactionsCount = 0;
	(*counterTask)->transactions = NULL;

	//setup associated serial
	memset((*counterTask)->associatedSerial, 0, SIZE_OF_SERIAL + 1);
	memcpy((*counterTask)->associatedSerial, counter->serialRemote, SIZE_OF_SERIAL + 1);

	//setup dbid and handle and others
	(*counterTask)->counterDbId = counter->dbId;
	(*counterTask)->counterType = counter->type;
	(*counterTask)->associatedId = -1;
	(*counterTask)->currentTransaction = 0;
	(*counterTask)->transactionsCount = 0;

	//free statuses of result
	(*counterTask)->resultCurrentReady = ZERO;
	(*counterTask)->resultDayReady = ZERO;
	(*counterTask)->resultMonthReady = ZERO;
	(*counterTask)->resultProfileReady = ZERO;

	//setup tariff transaction counters
	(*counterTask)->tariffHolidaysTransactionCounter = 0 ;
	(*counterTask)->tariffMapTransactionCounter = 0 ;
	(*counterTask)->tariffMovedDaysTransactionsCounter = 0 ;
	(*counterTask)->indicationWritingTransactionCounter = 0 ;

	//setup power control transaction counters
	(*counterTask)->powerControlTransactionCounter = 0 ;

	//setup user control transaction counters
	(*counterTask)->userCommandsTransactionCounter = 0;

	//free results
	memset(&(*counterTask)->resultCurrent, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultDay, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultMonth, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultProfile, 0, sizeof (profile_t));

	int readingPqp = 1 ;
	BOOL currentMerteragesWasRequested = FALSE; //osipov sets

	enum{
		TASK_CREATION_STATE_START,

		TASK_CREATION_STATE_WRITE_USER_COMMANDS,
		TASK_CREATION_STATE_SET_SEASON,
		TASK_CREATION_STATE_SYNC_TIME,
		TASK_CREATION_STATE_WRITE_TARIFF_MAP,
		TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES,
		TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ,
		TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME,
		TASK_CREATION_STATE_READ_PROFILES,
		TASK_CREATION_STATE_READ_CUR_METERAGES,
		TASK_CREATION_STATE_READ_DAY_METERAGES,
		TASK_CREATION_STATE_READ_MON_METERAGES,
		TASK_CREATION_STATE_READ_JOURNALS,
		TASK_CREATION_STATE_READ_PQP,

		TASK_CREATION_STATE_DONE
	};

	int taskCreationState = TASK_CREATION_STATE_START ;
	while ( taskCreationState != TASK_CREATION_STATE_DONE )
	{
		switch(taskCreationState)
		{
			case TASK_CREATION_STATE_START:
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_START FOR DbId = [%ld]" , counter->dbId );

				taskCreationState = TASK_CREATION_STATE_WRITE_USER_COMMANDS;
			}
			break; //case TASK_CREATION_STATE_START:


			//
			//****
			//

			case TASK_CREATION_STATE_WRITE_USER_COMMANDS:
			{
				#if COUNTER_TASK_WRITE_USERCOMMANDS

				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_WRITE_USER_COMMANDS" );

				taskCreationState = TASK_CREATION_STATE_DONE ;

				unsigned char * userCmds = NULL ;
				int usrCmdsLength = 0 ;

				if ( ACOUNT_Get( counter->dbId , &userCmds , &usrCmdsLength , ACOUNTS_PART_USER_COMMANDS) == OK )
				{
					if ( userCmds )
					{
						ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_USER_COMMANDS );

						res = UJUK_WriteUserCommands(counter , counterTask , userCmds , usrCmdsLength);
						if ( res == OK )
						{
							unsigned char evDesc[EVENT_DESC_SIZE];
							unsigned char dbidDesc[EVENT_DESC_SIZE];
							memset(dbidDesc , 0, EVENT_DESC_SIZE);
							COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
							COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_START, evDesc);
							STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );

							ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_USER_COMMANDS );

							(*counterTask)->needToShuffle = FALSE ;
						}
						else
						{
							if( res == ERROR_COUNTER_TYPE )
							{
								taskCreationState = TASK_CREATION_STATE_SET_SEASON ;
								res = OK ;
								ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_COUNTER_TYPE , ACOUNTS_PART_USER_COMMANDS );

								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_ERR_TYPE, evDesc);
								STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
							}
							else if ( res == ERROR_FILE_FORMAT )
							{
								taskCreationState = TASK_CREATION_STATE_SET_SEASON ;
								res = OK ;
								ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_USER_COMMANDS );

								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_ERR_FORM, evDesc);
								STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
							}

							//free transactions in current task if need
							COMMON_FreeCounterTask(counterTask);
						}

						free( userCmds );
						userCmds = NULL ;
						usrCmdsLength = 0 ;
					}
					else
					{
						ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_USER_COMMANDS );

						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_ERR_FORM, evDesc);
						STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );

						taskCreationState = TASK_CREATION_STATE_SET_SEASON ;
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Do not need to write user commands");
					taskCreationState = TASK_CREATION_STATE_SET_SEASON ;

					if( userCmds )
					{
						free( userCmds );
						userCmds = NULL ;
					}
				}
				#else
					taskCreationState = TASK_CREATION_STATE_SYNC_TIME;
				#endif
			}
			break; //case TASK_CREATION_STATE_WRITE_USER_COMMANDS:

			case TASK_CREATION_STATE_SET_SEASON:
			{
				taskCreationState = TASK_CREATION_STATE_SYNC_TIME ;

				unsigned char allowSeasonFlg = STORAGE_GetSeasonAllowFlag() ;
				unsigned char currentSeason = STORAGE_GetCurrentSeason() ;

				unsigned char counterAllowSeason = STORAGE_SEASON_CHANGE_DISABLE ;
				counterStatus_t counterStatus;
				memset( &counterStatus , 0 , sizeof(counterStatus_t) );
				if ( STORAGE_LoadCounterStatus( counter->dbId , &counterStatus ) == OK )
				{
					counterAllowSeason = counterStatus.allowSeasonChange;
				}

				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK allowSeasonFlg=[%d] counterAllowSeason=[%d]" , allowSeasonFlg , counterAllowSeason );

				if ( COMMON_CheckDtsForValid( &counter->currentDts ) == OK )
				{
					if (  allowSeasonFlg != counterAllowSeason )
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Need to set season flg");
						//get counter channel open

						res = UJUK_Command_OpenCounterForWritingToPhMemmory (counter, COMMON_AllocTransaction(counterTask));


						if ( res == OK )
						{
							//set allow flag
							res = UJUK_Command_SetAllowSeasonChangeFlg(counter, COMMON_AllocTransaction(counterTask), allowSeasonFlg);

						}

						if ( allowSeasonFlg == STORAGE_SEASON_CHANGE_ENABLE )
						{
							if (res == OK )
							{
								//set season
								res = UJUK_Command_SetCurrentSeason(counter, COMMON_AllocTransaction(counterTask) , currentSeason) ;
							}
						}

						taskCreationState = TASK_CREATION_STATE_DONE ;
						(*counterTask)->needToShuffle = FALSE ;
					}
				}
			}
			break;

			case TASK_CREATION_STATE_SYNC_TIME:
			{
				#if COUNTER_TASK_SYNC_TIME
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_SYNC_TIME syncFlag [%02X]" , counter->syncFlag );
				if ( STORAGE_IsCounterNeedToSync( &counter ) == TRUE )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Need to sync");
					//get counter channel open

					res = UJUK_Command_OpenCounterForWritingToPhMemmory (counter, COMMON_AllocTransaction(counterTask));


					res = UJUK_SyncTime( counter , counterTask  );

					(*counterTask)->needToShuffle = FALSE ;

					taskCreationState = TASK_CREATION_STATE_DONE ;
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Do not need to sync");
					taskCreationState = TASK_CREATION_STATE_WRITE_TARIFF_MAP ;
				}
				#else
					taskCreationState = TASK_CREATION_STATE_WRITE_TARIFF_MAP ;
				#endif
			}
			break; //case TASK_CREATION_STATE_SYNC_TIME:



			//
			//****
			//



			case TASK_CREATION_STATE_WRITE_TARIFF_MAP:
			{
				#if COUNTER_TASK_WRITE_TARIFF_MAP
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_WRITE_TARIFF_MAP" );

				taskCreationState = TASK_CREATION_STATE_DONE ;

				unsigned char * tariff = NULL ;
				int tariffLength = 0 ;

				if ( ACOUNT_Get( counter->dbId , &tariff , &tariffLength , ACOUNTS_PART_TARIFF) == OK )
				{
					if ( tariff )
					{
						ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_TARIFF );

						//get counter channel open

						res = UJUK_Command_OpenCounterForWritingToPhMemmory (counter, COMMON_AllocTransaction(counterTask));


						if ( res == OK )
						{
							res = UJUK_WriteTariffMap(counter , counterTask , tariff , tariffLength);
							if ( res == OK )
							{
								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_START, evDesc);
								STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );

								ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_TARIFF );

								(*counterTask)->needToShuffle = FALSE ;
							}
							else
							{
								if( res == ERROR_COUNTER_TYPE )
								{
									taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
									res = OK ;
									ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_COUNTER_TYPE , ACOUNTS_PART_TARIFF );

									unsigned char evDesc[EVENT_DESC_SIZE];
									unsigned char dbidDesc[EVENT_DESC_SIZE];
									memset(dbidDesc , 0, EVENT_DESC_SIZE);
									COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
									COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_TYPE, evDesc);
									STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
								}
								else if ( res == ERROR_FILE_FORMAT )
								{
									taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
									res = OK ;
									ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_TARIFF );

									unsigned char evDesc[EVENT_DESC_SIZE];
									unsigned char dbidDesc[EVENT_DESC_SIZE];
									memset(dbidDesc , 0, EVENT_DESC_SIZE);
									COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
									COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_FORM, evDesc);
									STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
								}

								//free transactions in current task if need
								COMMON_FreeCounterTask(counterTask);
							}
						}

						free( tariff );
						tariff = NULL ;
						tariffLength = 0 ;
					}
					else
					{
						ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_TARIFF );

						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_FORM, evDesc);
						STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );

						taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
					}
				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Do not need to write tariff");
					taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;

					if( tariff )
					{
						free( tariff );
						tariff = NULL ;
					}
				}

				#else
				taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
				#endif

			}
			break; //case TASK_CREATION_STATE_WRITE_TARIFF_MAP:



			//
			//****
			//



			case TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES:
			{
				#if COUNTER_TASK_WRITE_PSM
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES" );

				taskCreationState = TASK_CREATION_STATE_DONE ;

				counterStatus_t counterStatus;
				memset(&counterStatus , 0 , sizeof(dateTimeStamp) );
				if ( STORAGE_LoadCounterStatus(counter->dbId , &counterStatus) == OK )
				{
					if ( counterStatus.ratio != 0xFF )
					{
						unsigned char * psmRules = NULL ;
						int psmRulesLength = 0 ;
						if ( ACOUNT_Get( counter->dbId , &psmRules , &psmRulesLength , ACOUNTS_PART_POWR_CTRL) == OK )
						{
							if ( psmRules )
							{
								// open counter for writing

								res = UJUK_Command_OpenCounterForWritingToPhMemmory (counter, COMMON_AllocTransaction(counterTask));


								if ( res == OK )
								{
									res = UJUK_WritePSM( counter, counterTask , psmRules , psmRulesLength , counterStatus.ratio);
									if ( res == OK )
									{
										ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_POWR_CTRL );

										unsigned char evDesc[EVENT_DESC_SIZE];
										unsigned char dbidDesc[EVENT_DESC_SIZE];
										memset(dbidDesc , 0, EVENT_DESC_SIZE);
										COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
										COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_START, evDesc);
										STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

										(*counterTask)->needToShuffle = FALSE ;
									}
									else
									{
										if ( res == ERROR_COUNTER_TYPE )
										{
											res = OK ;
											unsigned char evDesc[EVENT_DESC_SIZE];
											unsigned char dbidDesc[EVENT_DESC_SIZE];
											memset(dbidDesc , 0, EVENT_DESC_SIZE);
											COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
											COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_TYPE, evDesc);
											STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

											ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_COUNTER_TYPE , ACOUNTS_PART_POWR_CTRL);

											taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
										}
										if ( res == ERROR_FILE_FORMAT )
										{
											res = OK ;
											unsigned char evDesc[EVENT_DESC_SIZE];
											unsigned char dbidDesc[EVENT_DESC_SIZE];
											memset(dbidDesc , 0, EVENT_DESC_SIZE);
											COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
											COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_FORM, evDesc);
											STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

											ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_POWR_CTRL);

											taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
										}

										//free transactions in current task if need
										COMMON_FreeCounterTask(counterTask);
									}
								}

								free(psmRules);
								psmRules = NULL ;
							}
							else
							{
								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_FORM, evDesc);
								STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

								ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_POWR_CTRL);

								taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
							}
						}
						else
						{
							taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
						}


					}
					else
					{
						taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
				}


				#else
				taskCreationState = TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ ;
				#endif
			}
			break; //case TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES:



			//
			//****
			//



			case TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ:
			{
				#if ( COUNTER_TASK_READ_DTS_AND_RATIO || \
					COUNTER_TASK_READ_PROFILES || \
					COUNTER_TASK_READ_CUR_METERAGES || \
					COUNTER_TASK_READ_DAY_METERAGES || \
					COUNTER_TASK_READ_MON_METERAGES || \
					COUNTER_TASK_READ_JOURNALS || \
					COUNTER_TASK_READ_PQP )

				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ" );

				//get counter channel open
				char defaultPass[PASSWORD_SIZE];
				memset(defaultPass, '0', PASSWORD_SIZE);

				if( !strlen((char *)counter->serialRemote) || memcmp(defaultPass, counter->password1, PASSWORD_SIZE) )
					res = UJUK_Command_OpenCounter(counter, COMMON_AllocTransaction(counterTask));
				else
					res = OK ;



				taskCreationState = TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME ;

				#else
				taskCreationState = TASK_CREATION_STATE_DONE ;
				#endif

			}
			break; //case TASK_CREATION_STATE_OPEN_COUNTER_FOR_READ:



			//
			//****
			//



			case TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME:
			{
				#if COUNTER_TASK_READ_DTS_AND_RATIO
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME" );

				//get current time and date of counter
				if (res == OK) {
					res = UJUK_Command_GetDateAndTime(counter, COMMON_AllocTransaction(counterTask));
				}

				//get ratio value
				if (res == OK)  {

					if (counter->ratioIndex == 0xFF){

						res = UJUK_Command_GetRatioValue(counter, COMMON_AllocTransaction(counterTask));

						taskCreationState = TASK_CREATION_STATE_DONE;

					} else {

						taskCreationState = TASK_CREATION_STATE_READ_CUR_METERAGES ;
					}

					//get profile if needed
					if ((res == OK) && (UJUK_IsCounterAbleProfile(counter->type)))
					{
						if ( counter->profileInterval == STORAGE_UNKNOWN_PROFILE_INTERVAL )
						{
							res = UJUK_Command_GetProfileIntervalValue(counter, COMMON_AllocTransaction(counterTask) );
						}

						if (res == OK)
						{
							res = UJUK_Command_GetProfileCurrentPtr(counter, COMMON_AllocTransaction(counterTask));
						}
					}
				}

				#else
				taskCreationState = TASK_CREATION_STATE_READ_CUR_METERAGES ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME:



			//
			//****
			//


			case TASK_CREATION_STATE_READ_CUR_METERAGES:
			{
				#if COUNTER_TASK_READ_CUR_METERAGES
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_CUR_METERAGES" );

				taskCreationState = TASK_CREATION_STATE_DONE ;

				if (res == OK)
				{
					if ( counter->profileReadingState != STORAGE_PROFILE_STATE_WAITING_SEARCH_RES )
					{
						dateTimeStamp dtsToRequest;
						memset(&dtsToRequest , 0 , sizeof(dateTimeStamp));

						if ( STORAGE_CheckTimeToRequestMeterage( counter , STORAGE_CURRENT , &dtsToRequest ) == TRUE )
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "its time to read current meterages" );

							switch( counter->type )
							{								
								case TYPE_UJUK_PSH_4TM_05MK:								
								case TYPE_UJUK_SEB_1TM_02M:
								case TYPE_UJUK_SEB_1TM_03A:
								{
									res = UJUK_Command_GetMeteragesByBinaryMask(counter, COMMON_AllocTransaction(counterTask), UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_CURRENT , 0);
									currentMerteragesWasRequested = TRUE;
								}
								break;

								default:
								{
									//get current meterage for T1
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
										res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1);
										currentMerteragesWasRequested = TRUE; //osipov sets
									}

									//get current meterage for T2
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
										res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2);
										currentMerteragesWasRequested = TRUE; //osipov sets
									}

									//get current meterage for T3
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
										res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3);
										currentMerteragesWasRequested = TRUE; //osipov sets
									}

									//get current meterage for T4
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
										res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4);
										currentMerteragesWasRequested = TRUE; //osipov sets
									}
								}
								break;
							} //switch
						} else {
							COMMON_STR_DEBUG( DEBUG_UJUK "its not time to read current meterages, skip..." );
						}

						if ( res == OK )
						{
							taskCreationState = TASK_CREATION_STATE_READ_MON_METERAGES ;
						}
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Counter is searching for profile. Do not asking it for current meterages");
						taskCreationState = TASK_CREATION_STATE_READ_PROFILES ;
					}
				}

				#else
				taskCreationState = TASK_CREATION_STATE_READ_MON_METERAGES ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_CUR_METERAGES:


			//
			//****
			//


			case TASK_CREATION_STATE_READ_MON_METERAGES:
			{
				#if COUNTER_TASK_READ_MON_METERAGES
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_MON_METERAGES" );

				taskCreationState = TASK_CREATION_STATE_DONE ;

				if (res == OK)
				{
					if ( counter->profileReadingState != STORAGE_PROFILE_STATE_WAITING_SEARCH_RES)
					{
						dateTimeStamp dtsToRequest;
						memset(&dtsToRequest , 0 , sizeof(dateTimeStamp));

						if ( STORAGE_CheckTimeToRequestMeterage( counter , STORAGE_MONTH , &dtsToRequest ) == TRUE )
						{
							//check if meterage is out of archive bound
							dateTimeStamp counterCurrentDts;
							memcpy( &counterCurrentDts , &counter->currentDts , sizeof(dateTimeStamp));
							counterCurrentDts.d.d = 1 ;
							counterCurrentDts.t.h = 0 ;
							counterCurrentDts.t.m = 0 ;
							counterCurrentDts.t.s = 0 ;

							int monthesDiff = COMMON_GetDtsDiff__Alt(&dtsToRequest, &counterCurrentDts, BY_MONTH) + 1;
							int meteragesStorageDepth = COMMON_GetStorageDepth( counter , STORAGE_MONTH ); //osipov sets

							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK monthes difference is [%d] monthes" , monthesDiff );

							if ( monthesDiff > meteragesStorageDepth )
							{
								COMMON_STR_DEBUG( DEBUG_UJUK "UJUK month diff is higher then storage depth for this counter!");
								while ((monthesDiff > meteragesStorageDepth) && (res == OK))
								{
									meterage_t meterage;
									memset(&meterage, 0, sizeof (meterage_t));
									memcpy(&meterage.dts, &dtsToRequest, sizeof (dateTimeStamp));
									meterage.status = METERAGE_OUT_OF_ARCHIVE_BOUNDS;
									res = STORAGE_SaveMeterages(STORAGE_MONTH, counter->dbId, &meterage);
									if (res == OK)
									{
										COMMON_ChangeDts(&dtsToRequest, INC, BY_MONTH);
										monthesDiff--;
									}
								}
							}

							//setup dts in appropriate result
							memcpy(&(*counterTask)->resultMonth.dts, &dtsToRequest, sizeof (dateTimeStamp));

							switch( counter->type )
							{
								case TYPE_UJUK_SET_1M_01: //osipov sets
								{
									res = OK;
								}
								break;

								case TYPE_UJUK_PSH_4TM_05MK:
								case TYPE_UJUK_SEB_1TM_02M:
								case TYPE_UJUK_SEB_1TM_03A:
								{
									unsigned char askMonthIndex = dtsToRequest.d.m;
									if (res == OK)
									{
										res = UJUK_Command_GetMeteragesByBinaryMask(counter, COMMON_AllocTransaction(counterTask), UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_MONTH, askMonthIndex);
									}
								}
								break;

								//osipov sets
								case TYPE_UJUK_SET_4TM_01A:
								case TYPE_UJUK_SET_4TM_02A:
								case TYPE_UJUK_SET_4TM_02MB:
								{
									//if we are not request current meterages at this time, request it for future calculations
									if (currentMerteragesWasRequested == FALSE){
										//get current meterage for T1
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
											res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1);
											currentMerteragesWasRequested = TRUE;
										}

										//get current meterage for T2
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
											res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2);
											currentMerteragesWasRequested = TRUE;
										}

										//get current meterage for T3
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
											res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3);
											currentMerteragesWasRequested = TRUE;
										}

										//get current meterage for T4
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
											res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4);
											currentMerteragesWasRequested = TRUE;
										}
									}

									//request monthes
									int monthIndex = 0 ;
									for ( ; monthIndex < monthesDiff ; ++monthIndex )
									{

										int askMonthIndex = dtsToRequest.d.m ;
										//get day meterage for T1
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
											res = UJUK_Command_MonthEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askMonthIndex);
											(*counterTask)->transactions[ (*counterTask)->transactionsCount - 1 ]->monthEnergyRequest = monthIndex ;
										}
										//get day meterage for T2
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
											res = UJUK_Command_MonthEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askMonthIndex);
											(*counterTask)->transactions[ (*counterTask)->transactionsCount - 1 ]->monthEnergyRequest = monthIndex ;
										}
										//get day meterage for T3
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
											res = UJUK_Command_MonthEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askMonthIndex);
											(*counterTask)->transactions[ (*counterTask)->transactionsCount - 1 ]->monthEnergyRequest = monthIndex ;
										}
										//get day meterage for T4
										if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
											res = UJUK_Command_MonthEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askMonthIndex);
											(*counterTask)->transactions[ (*counterTask)->transactionsCount - 1 ]->monthEnergyRequest = monthIndex ;
										}
										COMMON_ChangeDts( &dtsToRequest , INC , BY_MONTH );
									} //for()
									(*counterTask)->monthRequestCount = monthIndex ;
								}
								break;

								default:
								{
									//setup ask month index
									unsigned char askMonthIndex = dtsToRequest.d.m;

									//get day meterage for T1
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
										res = UJUK_Command_MonthMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askMonthIndex);
									}

									//get day meterage for T2
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
										res = UJUK_Command_MonthMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askMonthIndex);
									}

									//get day meterage for T3
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
										res = UJUK_Command_MonthMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askMonthIndex);
									}

									//get day meterage for T4
									if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {										
										res = UJUK_Command_MonthMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askMonthIndex);
									}
								}
								break;
							} //switch
						}
						else
						{
							taskCreationState = TASK_CREATION_STATE_READ_DAY_METERAGES ;
						}
					}
					else
					{
						taskCreationState = TASK_CREATION_STATE_READ_PROFILES ;
					}
				}


				#else
				taskCreationState = TASK_CREATION_STATE_READ_DAY_METERAGES ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_MON_METERAGES:

			//
			//****
			//


			case TASK_CREATION_STATE_READ_DAY_METERAGES:
			{
				#if COUNTER_TASK_READ_DAY_METERAGES
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_DAY_METERAGES" );

				if (res == OK)
				{
					if ( counter->profileReadingState != STORAGE_PROFILE_STATE_WAITING_SEARCH_RES)
					{

						//check we don't poll counter too much longer time (more than 23 hours)
						//don't read daily meterages if counter stamp is became old
						//this is prevent meterages smashing with 'today' and 'yesterday' stamps
						time_t tPollNow;
						time(&tPollNow);
						int tDiff = (tPollNow - counter->lastPollTime);
						if (tDiff > MAX_POLL_DELAY_FOR_UJUK)
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK skip day meterages read, counter stamp is too much old");
							counter->stampReread = 1;
							taskCreationState = TASK_CREATION_STATE_DONE;
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK try to touch day meterages");
							dateTimeStamp dtsToRequest;
							memset(&dtsToRequest , 0 , sizeof(dateTimeStamp));

							if ( STORAGE_CheckTimeToRequestMeterage( counter , STORAGE_DAY , &dtsToRequest ) == TRUE )
							{
								int daysDiff = COMMON_GetDtsDiff__Alt(&dtsToRequest, &counter->currentDts, BY_DAY ) + 1;
								int meteragesStorageDepth = COMMON_GetStorageDepth( counter , STORAGE_DAY );

								COMMON_STR_DEBUG(DEBUG_UJUK "ujuk diff dates: %d days", daysDiff);

								if ( daysDiff > meteragesStorageDepth )
								{
									COMMON_STR_DEBUG( DEBUG_UJUK "UJUK days diff is higher then storage depth for this counter!");
									//fill the storage by empty meterages
									while( daysDiff > meteragesStorageDepth )
									{
										COMMON_STR_DEBUG(DEBUG_UJUK "ujuk fill empty day for: " DTS_PATTERN, DTS_GET_BY_VAL(dtsToRequest));

										meterage_t meterage;
										memset(&meterage, 0, sizeof (meterage_t));
										memcpy(&meterage.dts, &dtsToRequest, sizeof (dateTimeStamp));
										meterage.status = METERAGE_OUT_OF_ARCHIVE_BOUNDS;
										res = STORAGE_SaveMeterages(STORAGE_DAY, counter->dbId, &meterage);
										if (res == OK)
										{
											COMMON_ChangeDts(&dtsToRequest, INC, BY_DAY);
											daysDiff--;
										}
									} //while()
									COMMON_STR_DEBUG( DEBUG_UJUK "UJUK days diff after filling is [%d]", daysDiff);
								}

								//setup dts in appropriate result
								memcpy(&(*counterTask)->resultDay.dts, &dtsToRequest, sizeof (dateTimeStamp));

								switch( counter->type )
								{
									case TYPE_UJUK_SET_1M_01: //osipov sets
									{
										res = OK; //no form, no arch
									}
									break;

									case TYPE_UJUK_SEB_1TM_02M:
									case TYPE_UJUK_SEB_1TM_03A:
									case TYPE_UJUK_PSH_4TM_05MK:
									{
										int memoryType = UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_DAY ;

										unsigned char dayIdx = dtsToRequest.d.d ;
										if ( daysDiff == 2 )
										{
											memoryType = UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_YESTERDAY ;
											dayIdx = 0 ;
										}
										else if ( daysDiff == 1 )
										{
											memoryType = UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_TODAY ;
											dayIdx = 0 ;
										}

										res = UJUK_Command_GetMeteragesByBinaryMask(counter, COMMON_AllocTransaction(counterTask), memoryType, dayIdx);

									}
									break;

									//osipov sets
									case TYPE_UJUK_SET_4TM_03A:
									case TYPE_UJUK_SET_4TM_03MB:
									{
										//ask day archive by extended request by tariffs
										int askDayMode = ASK_DAY_ARCHIVE;
										unsigned char askDayIndex = dtsToRequest.d.d;
										if (daysDiff == 2)
										{
											askDayMode = ASK_DAY_YESTERDAY;
											askDayIndex = ZERO;
										}
										else if (daysDiff == 1)
										{
											askDayMode = ASK_DAY_TODAY;
											askDayIndex = ZERO;
										}

										if (askDayMode != ASK_EMPTY)
										{
											//get day meterage for T1
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askDayMode, askDayIndex);
											}

											//get day meterage for T2
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askDayMode, askDayIndex);
											}

											//get day meterage for T3
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askDayMode, askDayIndex);
											}

											//get day meterage for T4
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askDayMode, askDayIndex);
											}
										} // if (askDayMode != ASK_EMPTY)
									}
									break;

									//osipov sets
									case TYPE_UJUK_SET_4TM_01A:
									case TYPE_UJUK_SET_4TM_02A:
									case TYPE_UJUK_SET_4TM_02MB:
									{
										//ask day archive by day energies
										int askDayMode = ASK_EMPTY;
										unsigned char askDayIndex = ZERO;
										if (daysDiff == 2)
										{
											askDayMode = ASK_DAY_YESTERDAY;
											askDayIndex = ZERO;
										}
										else if (daysDiff == 1)
										{
											askDayMode = ASK_DAY_TODAY;
											askDayIndex = ZERO;
										}

										if (askDayMode != ASK_EMPTY)
										{

											//if we are not request current meterages at this time, request it for future calculations
											if (currentMerteragesWasRequested == FALSE){
												//get current meterage for T1
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
													res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1);
													currentMerteragesWasRequested = TRUE;
												}

												//get current meterage for T2
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
													res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2);
													currentMerteragesWasRequested = TRUE;
												}

												//get current meterage for T3
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
													res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3);
													currentMerteragesWasRequested = TRUE;
												}

												//get current meterage for T4
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
													res = UJUK_Command_CurrentMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4);
													currentMerteragesWasRequested = TRUE;
												}
											}

											//request day data
											if ( askDayMode == ASK_DAY_YESTERDAY )
											{
												//get day meterage for T1
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
													res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askDayMode );
												}

												//get day meterage for T2
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
													res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askDayMode );
												}

												//get day meterage for T3
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
													res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askDayMode );
												}

												//get day meterage for T4
												if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
													res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askDayMode );
												}

												askDayMode = ASK_DAY_TODAY ;
											}

											//get day meterage for T1
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
												res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askDayMode );
											}

											//get day meterage for T2
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
												res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askDayMode );
											}

											//get day meterage for T3
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
												res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askDayMode );
											}

											//get day meterage for T4
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
												res = UJUK_Command_DayEnergy(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askDayMode );
											}
										}
									}
									break;

									default:
									{
										//ask day archive by extended request by tariffs
										int askDayMode = ASK_EMPTY;
										unsigned char askDayIndex = ZERO;
										if (daysDiff == 2)
										{
											askDayMode = ASK_DAY_YESTERDAY;
											askDayIndex = ZERO;
										}
										else if (daysDiff == 1)
										{
											askDayMode = ASK_DAY_TODAY;
											askDayIndex = ZERO;
										}

										if (askDayMode != ASK_EMPTY)
										{
											//get day meterage for T1
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T1)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_1, askDayMode, askDayIndex);
											}

											//get day meterage for T2
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T2)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_2, askDayMode, askDayIndex);
											}

											//get day meterage for T3
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T3)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_3, askDayMode, askDayIndex);
											}

											//get day meterage for T4
											if ((res == OK) && (counter->maskTarifs & COUNTER_MASK_T4)) {
												res = UJUK_Command_DayMeterage(counter, COMMON_AllocTransaction(counterTask), TARIFF_4, askDayMode, askDayIndex);
											}
										} // if (askDayMode != ASK_EMPTY)
									}
									break;
								} //switch(counter->type)

								// do not asking for anything
								taskCreationState = TASK_CREATION_STATE_DONE ;
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_UJUK "UJUK it is not time to ask day meterages");
								taskCreationState = TASK_CREATION_STATE_READ_PROFILES ;
							}
						}
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Counter is searching for profile. Do not asking it for day meterages");
						taskCreationState = TASK_CREATION_STATE_READ_PROFILES ;
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
				taskCreationState = TASK_CREATION_STATE_READ_PROFILES ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_DAY_METERAGES:


				//
				//****
				//


			case TASK_CREATION_STATE_READ_PROFILES:
			{
				#if COUNTER_TASK_READ_PROFILES
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_PROFILES" );

				if (res == OK)
				{
					taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;

					if ( UJUK_IsCounterAbleProfile( counter->type ))
					{
						 if ( (counter->profile == POWER_PROFILE_READ) &&
							  (counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL) )
						 {

							 res = UJUK_CreateProfileTr( counter, counterTask );

							 if ( res != OK )
								 taskCreationState = TASK_CREATION_STATE_DONE ;
						 }
					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Conter is too old to have profile. Do not read profile");
					}
				}

				#else

				taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;

				#endif
			}
			break; //case TASK_CREATION_STATE_READ_PROFILES:


			//
			//****
			//


			case TASK_CREATION_STATE_READ_JOURNALS:
			{
				#if COUNTER_TASK_READ_JOURNALS

				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_JOURNALS" );

				if ( counter->profileReadingState != STORAGE_PROFILE_STATE_WAITING_SEARCH_RES)
				{
					if (res == OK)
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "ujuk try to touch journal data");

						if (counter->journalReadingState == STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER)
						{
							counter->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT ;
							UJUK_StepToNextJournal(counter);
						}

						if (counter->journalReadingState == STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT)
						{
							res = UJUK_Command_GetNextEventFromCurrentJournal(counter , COMMON_AllocTransaction(counterTask) ) ;
						}
					}

					if ( readingPqp == 1 )
					{
						taskCreationState = TASK_CREATION_STATE_READ_PQP ;
					}
					else
					{
						taskCreationState = TASK_CREATION_STATE_DONE ;
					}


				}
				else
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Counter is searching for profile. Do not asking it for journals");
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
				taskCreationState = TASK_CREATION_STATE_READ_PQP ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_JOURNALS:



			//
			//****
			//



			case TASK_CREATION_STATE_READ_PQP:
			{
				#if COUNTER_TASK_READ_PQP
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_READ_PQP" );

				int hoursDiff = 24 ;
				powerQualityParametres_t pqp ;
				memset(&pqp, 0 , sizeof(powerQualityParametres_t));
				if (( STORAGE_GetPQPforDB(&pqp, counter->dbId) == OK ) && (COMMON_CheckDtsForValid(&counter->currentDts) == OK))
				{
					hoursDiff = COMMON_GetDtsDiff__Alt( &counter->currentDts, &pqp.dts, BY_HOUR );
				}


				switch( counter->type )
				{
					case TYPE_UJUK_SET_4TM_03A:
					case TYPE_UJUK_SET_4TM_02MB:
					case TYPE_UJUK_SET_4TM_03MB:
					case TYPE_UJUK_PSH_4TM_05A:
					case TYPE_UJUK_PSH_4TM_05D:
					case TYPE_UJUK_PSH_4TM_05MB:
					case TYPE_UJUK_PSH_4TM_05MD:
					case TYPE_UJUK_PSH_4TM_05MK:
					case TYPE_UJUK_PSH_3TM_05A:
					case TYPE_UJUK_PSH_3TM_05D:
					case TYPE_UJUK_PSH_3TM_05MB:
					case TYPE_UJUK_SEB_1TM_01A:
					case TYPE_UJUK_SEB_1TM_02A:
					case TYPE_UJUK_SEB_1TM_02D:
					case TYPE_UJUK_SEB_1TM_02M:
					case TYPE_UJUK_SEB_1TM_03A:
					{
						if (( counter->profileReadingState != STORAGE_PROFILE_STATE_WAITING_SEARCH_RES) && \
								(hoursDiff > 12))
						{
							if ( res == OK)
							{
								int phaseNumber = 0 ;

								COMMON_STR_DEBUG(DEBUG_UJUK "ujuk try to touch pqp data");
								if ((counter->type == TYPE_UJUK_SEB_1TM_01A) ||
									(counter->type == TYPE_UJUK_SEB_1TM_02A) ||
									(counter->type == TYPE_UJUK_SEB_1TM_02D) ||
									(counter->type == TYPE_UJUK_SEB_1TM_02M) ||
									(counter->type == TYPE_UJUK_SEB_1TM_03A))
								{
									COMMON_STR_DEBUG(DEBUG_UJUK "UJUK Counter type is SEB_1TM. Only 7 requests for PQP");
									phaseNumber = 1 ;

								}
								else
								{
									COMMON_STR_DEBUG(DEBUG_UJUK "UJUK Counter type is [%hhu]. Request group values" , counter->type);
									phaseNumber = 0 ;

									if (res == OK)
									{
										res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_TAN , phaseNumber ) ;
									}
								}

								//voltage
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_VOLTAGE , phaseNumber ) ;
								}

								//amperage
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_AMPERAGE , phaseNumber ) ;
								}

								//P power
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_P , phaseNumber ) ;
								}

								//Q power
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_Q , phaseNumber ) ;
								}

								//S power
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_S , phaseNumber ) ;
								}

								//cos
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_COS , phaseNumber ) ;
								}

								//freq
								if (res == OK)
								{
									res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_FREQUENCY , phaseNumber ) ;
								}
							}
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK it is not time for ask PQP...");
						}
					}
						break;

					case TYPE_UJUK_SET_4TM_01A:
					case TYPE_UJUK_SET_4TM_02A:
					{
						//voltage
						res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_VOLTAGE , 1 ) ;

						res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_VOLTAGE , 2 ) ;

						res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_VOLTAGE , 3 ) ;



						//amperage
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_AMPERAGE , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_AMPERAGE , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_AMPERAGE , 3 ) ;
						}

						//P power
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_P , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_P , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_P , 3 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_P , 0 ) ;
						}

						//Q power
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_Q , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_Q , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_Q , 3 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_Q , 0 ) ;
						}

						//S power
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_S , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_S , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_S , 3 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER_S , 0 ) ;
						}

						//cos
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_COS , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_COS , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_COS , 3 ) ;
						}


						//tan
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_TAN , 1 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_TAN , 2 ) ;
						}

						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO_TAN , 3 ) ;
						}

						//freq
						if (res == OK)
						{
							res = UJUK_Command_GetPowerQualityParametres(counter , COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_FREQUENCY , 0 ) ;
						}
					}
					break;


					default:
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Counter type is [%hhu]. Do not ask for PQP" , counter->type);
					}
					break;
				}


				taskCreationState = TASK_CREATION_STATE_DONE ;

				#else
				taskCreationState = TASK_CREATION_STATE_DONE ;
				#endif
			}
			break; //case TASK_CREATION_STATE_READ_PQP:



			//
			//****
			//



			default:
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TASK_CREATION_STATE_DONE" );
			}
			break; //case TASK_CREATION_STATE_DONE:
		} //switch

	}

	COMMON_GetCurrentDts( &(*counterTask)->dtsStart );

	//debug
	if ( (res == OK) && ( (*counterTask)->needToShuffle == TRUE ) )
	{
		res = UJUK_ShuffleTask( counter , counterTask );
	}

	return res ;
}


//
//
//

int UJUK_SaveTaskResults(counterTask_t * counterTask) {
	DEB_TRACE(DEB_IDX_UJUK)

	COMMON_STR_DEBUG( DEBUG_UJUK " -=SAVE TASK RESULTS=-");

	int res = ERROR_GENERAL;
	unsigned int aIndex;
	unsigned int ratio = 0xFF;
	unsigned int transactionIndex = 0;
	unsigned char transactionType;
	unsigned char transactionResult = TRANSACTION_DONE_OK;

	powerQualityParametres_t pqp;
	memset(&pqp , 0 , sizeof(powerQualityParametres_t));
	int pqpOkFlag = 0 ;

	int tariffMovedDaysTransactionsCounter = 0;
	int tariffHolidaysTransactionCounter = 0;
	int tariffMapTransactionCounter = 0;
	int indicationWritingTransactionCounter = 0 ;
	int userCommandsCount = 0;

	//unsigned char profileHead[8];
	//memset(profileHead , 0 , 8);
	int powerControlTransactionCounter = 0 ;

	unsigned int resultMetrMonthMsk = 0 ;
	unsigned int resultMetrCurrMsk = 0 ;
	unsigned int resultMetrDayMsk = 0 ;

	energiesArray_t allProfilesInHour;
	memset( &allProfilesInHour , 0 , sizeof( energiesArray_t ) );

	//only for old counters like SET-4TM
	meterage_t dayEnergyToday;
	meterage_t dayEnergyYesterday;
	memset( &dayEnergyToday , 0 , sizeof(meterage_t) );
	memset( &dayEnergyYesterday , 0 , sizeof(meterage_t) );
	unsigned int resultEnergyTodayMsk = 0 ;
	unsigned int resultEnergyYesterdayMsk = 0 ;
	unsigned char resultEnergyToday = 0 ;
	unsigned char resultEnergyYesterday = 0 ;

	meterage_t monthEnergy[12];
	unsigned int resultEnergyMonth[12] ;
	unsigned char resultEnergyMonthMsk[12] ;
	memset( monthEnergy , 0 , 12 * sizeof(meterage_t) );
	memset( resultEnergyMonth , 0 , 12 * sizeof(unsigned int) ) ;
	memset( resultEnergyMonthMsk , 0 , 12 * sizeof(unsigned char ) );
	int readyMonthEnergyCount = 0 ;


	//resetRepeateFlag and get counert
	counter_t * counterPtr = NULL;
	if ( STORAGE_FoundCounter( counterTask->counterDbId , &counterPtr ) == OK )
	{
		if( counterPtr )
		{
			counterPtr->lastTaskRepeateFlag = 0 ;
			ratio = counterPtr->ratioIndex;

			COMMON_STR_DEBUG( DEBUG_UJUK "ratio saved on counter is %02x", ratio);

		} else {

			COMMON_STR_DEBUG( DEBUG_UJUK "counter for this task became NULL");
			return OK;
		}
	}

	//body
	for (; ((transactionIndex < counterTask->transactionsCount) && (transactionResult == TRANSACTION_DONE_OK)); transactionIndex++) {

		transactionType = counterTask->transactions[transactionIndex]->transactionType;
		transactionResult = counterTask->transactions[transactionIndex]->result;

		COMMON_STR_DEBUG( DEBUG_UJUK "\nUJUK SaveTaskResult: transaction [%02X], result [ %s ]" , transactionType, getTransactionResult(transactionResult));
		COMMON_BUF_DEBUG( DEBUG_UJUK "REQUEST: " , counterTask->transactions[transactionIndex]->command , counterTask->transactions[transactionIndex]->commandSize );
		COMMON_BUF_DEBUG( DEBUG_UJUK "ANSWER: ", counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize);

		if (transactionResult == TRANSACTION_DONE_OK) {

			//check answer crc
			unsigned int expectedCrc = UJUK_GetCRC( counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize - UJUK_SIZE_OF_CRC );
			expectedCrc &= 0xFFFF ;
			unsigned int realCrc = 0 ;
			memcpy( &realCrc , &counterTask->transactions[transactionIndex]->answer[ counterTask->transactions[transactionIndex]->answerSize - UJUK_SIZE_OF_CRC ]  , UJUK_SIZE_OF_CRC );

			if ( expectedCrc != realCrc ){
				transactionResult = TRANSACTION_DONE_ERROR_CRC;
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK get answer with bad crc [%x], wait [%x]", realCrc, expectedCrc);
			}


			//check address of counter
			if ( transactionResult == TRANSACTION_DONE_OK )
			{
				unsigned long checkSn = 0;
				if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE) {
					checkSn+=(counterTask->transactions[transactionIndex]->answer[1]<<24);
					checkSn+=(counterTask->transactions[transactionIndex]->answer[2]<<16);
					checkSn+=(counterTask->transactions[transactionIndex]->answer[3]<<8);
					checkSn+=(counterTask->transactions[transactionIndex]->answer[4]);

				} else {
					checkSn=(counterTask->transactions[transactionIndex]->answer[0]) & 0xFF;
				}

				if (counterPtr)
				{
					if (checkSn != counterPtr->serial){
						transactionResult = TRANSACTION_DONE_ERROR_ADDRESS;
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK get answer from another counter [%u], wait from [%u]", checkSn, counterPtr->serial);
					}
				}
			}
		}

		//check transaction again
		if (transactionResult == TRANSACTION_DONE_OK) {

			//fix last success poll time in  memory
			time(&counterPtr->lastPollTime);
		
			int tTime = counterTask->transactions[transactionIndex]->tStop - counterTask->transactions[transactionIndex]->tStart;
			if (tTime >= 0){
				counterPtr->transactionTime = tTime;
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK transaction time : %d sec", tTime);
			}

			switch (transactionType) {
				case TRANSACTION_COUNTER_OPEN:
				{
					unsigned char result;

					//check open ok
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					result = counterTask->transactions[transactionIndex]->answer[aIndex] & 0x0F;

					if (result == ZERO) {
						transactionResult = TRANSACTION_DONE_OK;
					} else {
						transactionResult = TRANSACTION_COUNTER_NOT_OPEN;
						STORAGE_SetCounterStatistic(counterTask->counterDbId, FALSE, transactionType, transactionResult);
					}

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_COUNTER_OPEN %s", getTransactionResult(transactionResult));
				}
				break;


				case TRANSACTION_USER_DEFINED_COMMAND:
				{
					userCommandsCount++;
					COMMON_STR_DEBUG( DEBUG_UJUK "TRANSACTION_USER_DEFINED_COMMAND catched %d" , userCommandsCount);
				}
				break;


				case TRANSACTION_DATE_TIME_STAMP:
				{
					COMMON_BUF_DEBUG( DEBUG_UJUK "ANSWER: ", counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize);
					dateTimeStamp counterDts ;
					//save date and time stamp
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					counterDts.t.s = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex]);
					counterDts.t.m = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex + 1]);
					counterDts.t.h = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex + 2]);

					counterDts.d.d = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
					counterDts.d.m = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex + 5]);
					counterDts.d.y = COMMON_HexToDec(counterTask->transactions[transactionIndex]->answer[aIndex + 6]);

					transactionResult = TRANSACTION_DONE_OK;

					if (counterPtr)
					{
						if (( COMMON_CheckDtsForValid(&counterPtr->currentDts) != OK ) || (counterPtr->stampReread == TRUE) )
						{
							counterPtr->lastTaskRepeateFlag = 1 ;
						}

						//debug
						dateTimeStamp currentDts ;
						memset(&currentDts , 0 ,sizeof(dateTimeStamp) );
						COMMON_GetCurrentDts(&currentDts);
						int taskTime = COMMON_GetDtsDiff__Alt( &counterTask->dtsStart , &currentDts , BY_SEC );

						STORAGE_UpdateCounterDts( counterTask->counterDbId , &counterDts ,taskTime);

						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TRANSACTION_DATE_TIME_STAMP " DTS_PATTERN, DTS_GET_BY_VAL(counterDts));
						COMMON_STR_DEBUG(DEBUG_UJUK "Current USPD dts is " DTS_PATTERN, DTS_GET_BY_VAL(currentDts));
						COMMON_STR_DEBUG(DEBUG_UJUK "worth is [%d] , transactionTime = [%d]", counterPtr->syncWorth , counterPtr->transactionTime);
					}

					COMMON_BUF_DEBUG( DEBUG_UJUK "DATE_TIME_STAMP_ANSWER" , counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize  );

				}
					break;

				case TRANSACTION_GET_RATIO:
				{					 
					//save ratio (to counter)
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char ratioIndex = counterTask->transactions[transactionIndex]->answer[aIndex + 1] & RATIO_MASK;

					counterPtr->transformationRatios = counterTask->transactions[transactionIndex]->answer[aIndex ] & 0x07;
					if (counterPtr->type == TYPE_UJUK_PSH_4TM_05MK)
					{
						counterPtr->transformationRatios |= (counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0x04) << 1;
					}

					if ( COMMON_CheckRatioIndexForValid( counterTask->counterType , ratioIndex ) == FALSE )
					{
						COMMON_STR_DEBUG( DEBUG_UJUK "UJUK Incorrect ratio index value [%hhu]", ratioIndex );
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_RATIO, evDesc);
						STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );

						//try to fix
						switch( counterTask->counterType )
						{
							case TYPE_UJUK_SEB_1TM_02M:
							case TYPE_UJUK_SEB_1TM_02A:
							case TYPE_UJUK_SEB_1TM_02D:
							case TYPE_UJUK_SEB_1TM_03A:
							{
								ratioIndex = 0x04 ;
								transactionResult = TRANSACTION_DONE_OK;
								COMMON_STR_DEBUG( DEBUG_UJUK "UJUK ratio fixed to [%hhu] for SEB_1TM_02(M,D)",ratioIndex );
							}
							break;

							default:
							{
								transactionResult = TRANSACTION_DONE_ERROR_A;
								if ( counterPtr )
									counterPtr->lastTaskRepeateFlag = 0 ;
							}
							break;
						}


					}

					if ( transactionResult == TRANSACTION_DONE_OK )
					{
						//debug
						COMMON_STR_DEBUG(DEBUG_UJUK "RATIO %d / %d", ratioIndex, COMMON_GetRatioValueUjuk(ratioIndex));
						ratio = ratioIndex;

						//save counter ratio with dbId of counter
						if ( counterPtr )
						{
							counterPtr->ratioIndex = ratioIndex;
							STORAGE_UpdateCounterStatus_Ratio( counterPtr , ratioIndex );
							counterPtr->lastTaskRepeateFlag = 1 ;
							transactionResult = TRANSACTION_DONE_OK;
						}
					}

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_RATIO %s", getTransactionResult(transactionResult));
				}
					break;

				case TRANSACTION_PROFILE_INTERVAL:
				{
					aIndex = 2;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;


					if ( counterPtr->profileInterval == STORAGE_UNKNOWN_PROFILE_INTERVAL )
					{
						counterPtr->profileInterval = counterTask->transactions[transactionIndex]->answer[ aIndex ] ;
						STORAGE_UpdateCounterStatus_ProfileInterval( counterPtr , counterPtr->profileInterval  );
						counterPtr->lastTaskRepeateFlag = 1 ;
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK Setting profile interval value to [%hhu]", counterPtr->profileInterval );
					}
					else if ( counterPtr->profileInterval != counterTask->transactions[transactionIndex]->answer[ aIndex ] )
						counterPtr->profile = 0 ; //do not ask profile


					transactionResult = TRANSACTION_DONE_OK;
				}
					break;

				case TRANSACTION_PROFILE_INIT:
				{
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK profile was initialised with errorcode [%hhu]" , counterTask->transactions[transactionIndex]->answer[ aIndex ] );
				}
				break ;


				case TRANSACTION_METERAGE_CURRENT: //binary mask
				{
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == FALSE )
					{
						//check transaction id
						if ( counterTask->transactions[transactionIndex]->answer[ aIndex++ ] == counterTask->transactions[transactionIndex]->transactionType )
						{
							transactionResult = TRANSACTION_DONE_OK;

							//parse result

							enum
							{
								PARSING_START ,
								PARSING_GET_VALUE ,
								PARSING_END
							};

							unsigned char energyToParse = 0xFF ;
							int parsingState = PARSING_START ;
							while( parsingState != PARSING_END )
							{
								switch( parsingState )
								{
									case PARSING_START:
									{
										parsingState = PARSING_GET_VALUE ;
										switch (energyToParse )
										{
											case ENERGY_AP:
												energyToParse = ENERGY_AM ;
												break;

											case ENERGY_AM:
												energyToParse = ENERGY_RP ;
												break;

											case ENERGY_RP:
												energyToParse = ENERGY_RM ;
												break;

											case ENERGY_RM:
												parsingState = PARSING_END ;
												break;

											default:
												energyToParse = ENERGY_AP ;
												break;
										}

									}
									break;

									case PARSING_GET_VALUE:
									{
										parsingState = PARSING_START ;

										counterTask->resultCurrent.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultCurrent.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultCurrent.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultCurrent.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = 0 ;

										if ( (energyToParse == ENERGY_AM) &&
											 (	 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02M ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))   )
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM. Skip energy A-" );
										}
										else if ( ((energyToParse == ENERGY_RM) || (energyToParse == ENERGY_RP)) &&
												  (	 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))	)
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM-01(02D). Skip reactive energy" );
										}
										else
										{
											switch( energyToParse )
											{
												case ENERGY_AP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A+" );
													break;
												case ENERGY_AM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A-" );
													break;
												case ENERGY_RP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R+" );
													break;
												case ENERGY_RM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R-" );
													break;
												default:
													break;
											}

											if ( counterPtr->maskEnergy & energyToParse )
											{
												if ( counterPtr->maskTarifs & COUNTER_MASK_T1 )
												{
													counterTask->resultCurrent.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T1 [%u]" , counterTask->resultCurrent.t[ TARIFF_1 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T2 )
												{
													counterTask->resultCurrent.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T2 [%u]" , counterTask->resultCurrent.t[ TARIFF_2 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T3 )
												{
													counterTask->resultCurrent.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T3 [%u]" , counterTask->resultCurrent.t[ TARIFF_3 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T4 )
												{
													counterTask->resultCurrent.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T4 [%u]" , counterTask->resultCurrent.t[ TARIFF_4 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
											}

										}
									}
									break;

									default:
									{
										parsingState = PARSING_END ;
									}
									break;
								}
							} //while( parsingState != PARSING_END )

							memcpy(&counterTask->resultCurrent.dts, &counterPtr->currentDts, sizeof (dateTimeStamp));
							counterTask->resultCurrent.ratio = ratio;
							counterTask->resultCurrent.status = METERAGE_VALID;
							//TO DO: depending type of counter
							counterTask->resultCurrent.delimeter = 4;
							counterTask->resultCurrentReady = READY;
						}
						else
						{
							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK transaction id is not equal to expected");
							transactionResult = TRANSACTION_DONE_ERROR_A;
						}
					}
					else
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK counter return errrorcode [%hhu]" , counterTask->transactions[transactionIndex]->answer[ aIndex ]);
					}
				}
				break;

				case (TRANSACTION_METERAGE_CURRENT + TARIFF_1):
				case (TRANSACTION_METERAGE_CURRENT + TARIFF_2):
				case (TRANSACTION_METERAGE_CURRENT + TARIFF_3):
				case (TRANSACTION_METERAGE_CURRENT + TARIFF_4):
				{
					unsigned int eAp = 0;
					unsigned int eAm = 0;
					unsigned int eRp = 0;
					unsigned int eRm = 0;
					unsigned char tarifIndex = (transactionType - TRANSACTION_METERAGE_CURRENT);

					//save current for T
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE){
						aIndex += 4;
					}

					switch( counterPtr->type )
					{
						case TYPE_UJUK_SET_4TM_03A:
						case TYPE_UJUK_SET_4TM_02MB:
						case TYPE_UJUK_SET_4TM_03MB:
						case TYPE_UJUK_PSH_4TM_05A:
						case TYPE_UJUK_PSH_4TM_05D:
						case TYPE_UJUK_PSH_4TM_05MD:
						case TYPE_UJUK_PSH_4TM_05MB:
						case TYPE_UJUK_PSH_4TM_05MK:
						case TYPE_UJUK_PSH_3TM_05A:
						case TYPE_UJUK_PSH_3TM_05D:
						case TYPE_UJUK_PSH_3TM_05MB:
						case TYPE_UJUK_SEB_1TM_01A:
						case TYPE_UJUK_SEB_1TM_02A:
						case TYPE_UJUK_SEB_1TM_02D:
						case TYPE_UJUK_SEB_1TM_02M:
						case TYPE_UJUK_SEB_1TM_03A:
						{
							eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
							if ((counterTask->counterType == TYPE_UJUK_SEB_1TM_01A) ||
								(counterTask->counterType == TYPE_UJUK_SEB_1TM_02A) ||
								(counterTask->counterType == TYPE_UJUK_SEB_1TM_02D)){
								eAm = 0;
								eRp = 0;
								eRm = 0;
							} else if (counterTask->counterType == TYPE_UJUK_SEB_1TM_02M){
								eAm = 0;
								eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
								eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);;
							} else {
								eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
								eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
								eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);
							}
						}
						break;

						//for old counters like SET-4TM-02
						default:
						{
							eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
							eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
							eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
							eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);
						}
						break;
					}

					transactionResult = TRANSACTION_DONE_OK;

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "CURRENT T%d A+ %d", COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "CURRENT T%d A- %d", COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "CURRENT T%d R+ %d", COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "CURRENT T%d R- %d", COMMON_HexToDec(tarifIndex), eRm);

					//save current meterage to result
					if ( counterPtr )
					{
						memcpy(&counterTask->resultCurrent.dts, &counterPtr->currentDts, sizeof (dateTimeStamp));

						resultMetrCurrMsk |= (1 << (tarifIndex - 1) ) ;
						if ( resultMetrCurrMsk == counterPtr->maskTarifs )
						{
							counterTask->resultCurrentReady = READY;							
						}
					}
					else
					{
						dateTimeStamp dtsToday ;
						COMMON_GetCurrentDts(&dtsToday);
						memcpy(&counterTask->resultCurrent.dts, &dtsToday, sizeof (dateTimeStamp));
						//counterTask->resultCurrentReady = READY;
					}
					counterTask->resultCurrent.ratio = ratio;
					counterTask->resultCurrent.status = METERAGE_VALID;
					counterTask->resultCurrent.t[tarifIndex - 1].e[ENERGY_AP - 1] = eAp;
					counterTask->resultCurrent.t[tarifIndex - 1].e[ENERGY_AM - 1] = eAm;
					counterTask->resultCurrent.t[tarifIndex - 1].e[ENERGY_RP - 1] = eRp;
					counterTask->resultCurrent.t[tarifIndex - 1].e[ENERGY_RM - 1] = eRm;
					//TO DO: depending type of counter
					counterTask->resultCurrent.delimeter = 4;


					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_METERAGE_CURRENT T%d %s", COMMON_HexToDec(tarifIndex), getTransactionResult(transactionResult));
				}
					break;

				case TRANSACTION_METERAGE_DAY:
				{
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == FALSE )
					{
						//check transaction id
						if ( counterTask->transactions[transactionIndex]->answer[ aIndex++ ] == counterTask->transactions[transactionIndex]->transactionType )
						{
							transactionResult = TRANSACTION_DONE_OK;

							enum
							{
								PARSING_START ,
								PARSING_GET_VALUE ,
								PARSING_END
							};

							unsigned char energyToParse = 0xFF ;
							int parsingState = PARSING_START ;
							while( parsingState != PARSING_END )
							{
								switch( parsingState )
								{
									case PARSING_START:
									{
										parsingState = PARSING_GET_VALUE ;
										switch (energyToParse )
										{
											case ENERGY_AP:
												energyToParse = ENERGY_AM ;
												break;

											case ENERGY_AM:
												energyToParse = ENERGY_RP ;
												break;

											case ENERGY_RP:
												energyToParse = ENERGY_RM ;
												break;

											case ENERGY_RM:
												parsingState = PARSING_END ;
												break;

											default:
												energyToParse = ENERGY_AP ;
												break;
										}
									}
									break;

									case PARSING_GET_VALUE:
									{
										parsingState = PARSING_START ;

										counterTask->resultDay.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultDay.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultDay.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultDay.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = 0 ;

										if ( (energyToParse == ENERGY_AM) &&  (
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02M ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))   )
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM. Skip energy A-" );
										}
										else if ( ((energyToParse == ENERGY_RM) || (energyToParse == ENERGY_RP)) &&  (
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))	)
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM-01(02D). Skip reactive energy" );
										}
										else
										{
											switch( energyToParse )
											{
												case ENERGY_AP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A+" );
													break;
												case ENERGY_AM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A-" );
													break;
												case ENERGY_RP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R+" );
													break;
												case ENERGY_RM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R-" );
													break;
												default:
													break;
											}

											if ( counterPtr->maskEnergy & energyToParse )
											{
												if ( counterPtr->maskTarifs & COUNTER_MASK_T1 )
												{
													counterTask->resultDay.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T1 [%u]" , counterTask->resultDay.t[ TARIFF_1 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T2 )
												{
													counterTask->resultDay.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T2 [%u]" , counterTask->resultDay.t[ TARIFF_2 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T3 )
												{
													counterTask->resultDay.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T3 [%u]" , counterTask->resultDay.t[ TARIFF_3 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T4 )
												{
													counterTask->resultDay.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T4 [%u]" , counterTask->resultDay.t[ TARIFF_4 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
											}
										}
									}
									break;

									default:
									{
										parsingState = PARSING_END ;
									}
									break;
								} //switch( parsingState )
							} //while( parsingState != PARSING_END )

							//save month meterage to result
							counterTask->resultDay.ratio = ratio;
							counterTask->resultDay.delimeter = 4;
							counterTask->resultDay.status = METERAGE_VALID;
							counterTask->resultDayReady = READY;
						}
						else
						{
							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK transaction id is not equal to expected");
							transactionResult = TRANSACTION_DONE_ERROR_A;
						}
					}
					else
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK counter return errrorcode [%hhu]" , counterTask->transactions[transactionIndex]->answer[ aIndex ]);
					}
				}
				break;

				case (TRANSACTION_METERAGE_DAY + TARIFF_1):
				case (TRANSACTION_METERAGE_DAY + TARIFF_2):
				case (TRANSACTION_METERAGE_DAY + TARIFF_3):
				case (TRANSACTION_METERAGE_DAY + TARIFF_4):
				{
					unsigned int eAp;
					unsigned int eAm;
					unsigned int eRp;
					unsigned int eRm;
					unsigned char tarifIndex = (transactionType - TRANSACTION_METERAGE_DAY);

					//save day for T
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
					if ((counterTask->counterType == TYPE_UJUK_SEB_1TM_01A) ||
						(counterTask->counterType == TYPE_UJUK_SEB_1TM_02A) ||
						(counterTask->counterType == TYPE_UJUK_SEB_1TM_02D)){
						eAm = 0;
						eRp = 0;
						eRm = 0;
					} else if (counterTask->counterType == TYPE_UJUK_SEB_1TM_02M){
						eAm = 0;
						eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
						eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
					} else {
						eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
						eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
						eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);
					}
					transactionResult = TRANSACTION_DONE_OK;

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "DAY T%d A+ %d", COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "DAY T%d A- %d", COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "DAY T%d R+ %d", COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "DAY T%d R- %d", COMMON_HexToDec(tarifIndex), eRm);

					//save month meterage to result
					counterTask->resultDay.ratio = ratio;
					counterTask->resultDay.status = METERAGE_VALID;
					counterTask->resultDay.t[tarifIndex - 1].e[ENERGY_AP - 1] = eAp;
					counterTask->resultDay.t[tarifIndex - 1].e[ENERGY_AM - 1] = eAm;
					counterTask->resultDay.t[tarifIndex - 1].e[ENERGY_RP - 1] = eRp;
					counterTask->resultDay.t[tarifIndex - 1].e[ENERGY_RM - 1] = eRm;


					if ( counterPtr )
					{
						counterPtr->lastTaskRepeateFlag = 1 ;

						resultMetrDayMsk |= (1 << (tarifIndex - 1) ) ;
						if ( resultMetrDayMsk == counterPtr->maskTarifs )
						{
							counterTask->resultDayReady = READY;
						}
					}

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_METERAGE_DAY T%d %s", COMMON_HexToDec(tarifIndex), getTransactionResult(transactionResult));
				}
					break;

				case (TRANSACTION_POWER_YESTERDAY + TARIFF_1):
				case (TRANSACTION_POWER_YESTERDAY + TARIFF_2):
				case (TRANSACTION_POWER_YESTERDAY + TARIFF_3):
				case (TRANSACTION_POWER_YESTERDAY + TARIFF_4):
				{
					unsigned int eAp = 0;
					unsigned int eAm = 0;
					unsigned int eRp = 0;
					unsigned int eRm = 0;
					unsigned char tarifIndex = (transactionType - TRANSACTION_POWER_YESTERDAY);
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
					{
						aIndex += 4;
					}
					eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
					eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
					eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
					eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "YESTERDAY ENERGY T%d A+ %d", COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "YESTERDAY ENERGY T%d A- %d", COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "YESTERDAY ENERGY T%d R+ %d", COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "YESTERDAY ENERGY T%d R- %d", COMMON_HexToDec(tarifIndex), eRm);

					dayEnergyYesterday.t[ tarifIndex - 1 ].e[ ENERGY_AP - 1 ] = eAp ;
					dayEnergyYesterday.t[ tarifIndex - 1 ].e[ ENERGY_AM - 1 ] = eAm ;
					dayEnergyYesterday.t[ tarifIndex - 1 ].e[ ENERGY_RP - 1 ] = eRp ;
					dayEnergyYesterday.t[ tarifIndex - 1 ].e[ ENERGY_RM - 1 ] = eRm ;

					resultEnergyYesterdayMsk |= (1 << (tarifIndex - 1) ) ;
					if ( counterPtr )
					{
						if ( resultEnergyYesterdayMsk == counterPtr->maskTarifs )
						{
							resultEnergyYesterday = READY ;
						}
					}
				}
				break;

				case (TRANSACTION_POWER_TODAY + TARIFF_1):
				case (TRANSACTION_POWER_TODAY + TARIFF_2):
				case (TRANSACTION_POWER_TODAY + TARIFF_3):
				case (TRANSACTION_POWER_TODAY + TARIFF_4):
				{
					unsigned int eAp = 0;
					unsigned int eAm = 0;
					unsigned int eRp = 0;
					unsigned int eRm = 0;
					unsigned char tarifIndex = (transactionType - TRANSACTION_POWER_TODAY);
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
					{
						aIndex += 4;
					}
					eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
					eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
					eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
					eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "TODAY ENERGY T%d A+ %d", COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "TODAY ENERGY T%d A- %d", COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "TODAY ENERGY T%d R+ %d", COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "TODAY ENERGY T%d R- %d", COMMON_HexToDec(tarifIndex), eRm);

					dayEnergyToday.t[ tarifIndex - 1 ].e[ ENERGY_AP - 1 ] = eAp ;
					dayEnergyToday.t[ tarifIndex - 1 ].e[ ENERGY_AM - 1 ] = eAm ;
					dayEnergyToday.t[ tarifIndex - 1 ].e[ ENERGY_RP - 1 ] = eRp ;
					dayEnergyToday.t[ tarifIndex - 1 ].e[ ENERGY_RM - 1 ] = eRm ;

					resultEnergyTodayMsk |= (1 << (tarifIndex - 1) ) ;
					if ( counterPtr )
					{
						if ( resultEnergyTodayMsk == counterPtr->maskTarifs )
						{
							resultEnergyToday = READY ;
						}
					}
				}
				break;

				case TRANSACTION_METERAGE_MONTH:
				{
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == FALSE )
					{
						//check transaction id
						if ( counterTask->transactions[transactionIndex]->answer[ aIndex++ ] == counterTask->transactions[transactionIndex]->transactionType )
						{
							transactionResult = TRANSACTION_DONE_OK;

							enum
							{
								PARSING_START ,
								PARSING_GET_VALUE ,
								PARSING_END
							};

							unsigned char energyToParse = 0xFF ;
							int parsingState = PARSING_START ;
							while( parsingState != PARSING_END )
							{
								switch( parsingState )
								{
									case PARSING_START:
									{
										parsingState = PARSING_GET_VALUE ;
										switch (energyToParse )
										{
											case ENERGY_AP:
												energyToParse = ENERGY_AM ;
												break;

											case ENERGY_AM:
												energyToParse = ENERGY_RP ;
												break;

											case ENERGY_RP:
												energyToParse = ENERGY_RM ;
												break;

											case ENERGY_RM:
												parsingState = PARSING_END ;
												break;

											default:
												energyToParse = ENERGY_AP ;
												break;
										}
									}
									break;

									case PARSING_GET_VALUE:
									{
										parsingState = PARSING_START ;

										counterTask->resultMonth.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultMonth.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultMonth.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = 0 ;
										counterTask->resultMonth.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = 0 ;

										if ( (energyToParse == ENERGY_AM) &&  (
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_02M ) ||
												 ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))   )
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM. Skip energy A-" );
										}
										else if ( ((energyToParse == ENERGY_RM) || (energyToParse == ENERGY_RP)) &&  (
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_02A ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_02D ) ||
													  ( counterPtr->type == TYPE_UJUK_SEB_1TM_01A ))	)
										{
											COMMON_STR_DEBUG( DEBUG_UJUK "UJUK CounterType is SEB-1TM-01(02D). Skip reactive energy" );
										}
										else
										{
											switch( energyToParse )
											{
												case ENERGY_AP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A+" );
													break;
												case ENERGY_AM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy A-" );
													break;
												case ENERGY_RP:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R+" );
													break;
												case ENERGY_RM:
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK parse energy R-" );
													break;
												default:
													break;
											}

											if ( counterPtr->maskEnergy & energyToParse )
											{
												if ( counterPtr->maskTarifs & COUNTER_MASK_T1 )
												{
													counterTask->resultMonth.t[ TARIFF_1 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T1 [%u]" , counterTask->resultMonth.t[ TARIFF_1 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T2 )
												{
													counterTask->resultMonth.t[ TARIFF_2 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T2 [%u]" , counterTask->resultMonth.t[ TARIFF_2 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T3 )
												{
													counterTask->resultMonth.t[ TARIFF_3 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T3 [%u]" , counterTask->resultMonth.t[ TARIFF_3 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
												if ( counterPtr->maskTarifs & COUNTER_MASK_T4 )
												{
													counterTask->resultMonth.t[ TARIFF_4 - 1 ].e[energyToParse - 1] = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
													COMMON_STR_DEBUG( DEBUG_UJUK "UJUK T4 [%u]" , counterTask->resultMonth.t[ TARIFF_4 - 1 ].e[energyToParse - 1] );
													aIndex += 4;
												}
											}
										}
									}
									break;

									default:
									{
										parsingState = PARSING_END ;
									}
									break;
								} //switch( parsingState )
							} //while( parsingState != PARSING_END )

							//save month meterage to result
							counterTask->resultMonth.ratio = ratio;
							counterTask->resultMonth.delimeter = 4;
							counterTask->resultMonth.status = METERAGE_VALID;
							counterTask->resultMonthReady = READY;
						}
						else
						{
							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK transaction id is not equal to expected");
							transactionResult = TRANSACTION_DONE_ERROR_A;
						}
					}
					else
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK counter return errrorcode [%hhu]" , counterTask->transactions[transactionIndex]->answer[ aIndex ]);
					}
				}
				break;

				case (TRANSACTION_METERAGE_MONTH + TARIFF_1):
				case (TRANSACTION_METERAGE_MONTH + TARIFF_2):
				case (TRANSACTION_METERAGE_MONTH + TARIFF_3):
				case (TRANSACTION_METERAGE_MONTH + TARIFF_4):
				{
					unsigned int eAp;
					unsigned int eAm;
					unsigned int eRp;
					unsigned int eRm;
					unsigned char tarifIndex = (transactionType - TRANSACTION_METERAGE_MONTH);

					//save month for T
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
					if ((counterTask->counterType == TYPE_UJUK_SEB_1TM_01A) ||
						(counterTask->counterType == TYPE_UJUK_SEB_1TM_02A) ||
						(counterTask->counterType == TYPE_UJUK_SEB_1TM_02D)){
						eAm = 0;
						eRp = 0;
						eRm = 0;
					} else if (counterTask->counterType == TYPE_UJUK_SEB_1TM_02M) {
						eAm = 0;
						eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
						eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);;
					} else {
						eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
						eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
						eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);
					}
					transactionResult = TRANSACTION_DONE_OK;

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "MONTH T%d A+ %d", COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "MONTH T%d A- %d", COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "MONTH T%d R+ %d", COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "MONTH T%d R- %d", COMMON_HexToDec(tarifIndex), eRm);

					//save month meterage to result
					counterTask->resultMonth.ratio = ratio;
					counterTask->resultMonth.status = METERAGE_VALID;
					counterTask->resultMonth.t[tarifIndex - 1].e[ENERGY_AP - 1] = eAp;
					counterTask->resultMonth.t[tarifIndex - 1].e[ENERGY_AM - 1] = eAm;
					counterTask->resultMonth.t[tarifIndex - 1].e[ENERGY_RP - 1] = eRp;
					counterTask->resultMonth.t[tarifIndex - 1].e[ENERGY_RM - 1] = eRm;


					if ( counterPtr )
					{
						counterPtr->lastTaskRepeateFlag = 1 ;

						resultMetrMonthMsk |= (1 << (tarifIndex - 1) ) ;
						if ( resultMetrMonthMsk == counterPtr->maskTarifs )
						{
							counterTask->resultMonthReady = READY;
						}

					}

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_METERAGE_MONTH T%d %s", COMMON_HexToDec(tarifIndex), getTransactionResult(transactionResult));
				}
					break;

				case (TRANSACTION_POWER_MONTH + TARIFF_1):
				case (TRANSACTION_POWER_MONTH + TARIFF_2):
				case (TRANSACTION_POWER_MONTH + TARIFF_3):
				case (TRANSACTION_POWER_MONTH + TARIFF_4):
				{
					unsigned int eAp = 0;
					unsigned int eAm = 0;
					unsigned int eRp = 0;
					unsigned int eRm = 0;
					unsigned char tarifIndex = (transactionType - TRANSACTION_POWER_MONTH);

					//save month for T
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
					{
						aIndex += 4;
					}

					eAp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex]);
					eAm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 4]);
					eRp = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 8]);
					eRm = COMMON_BufToInt(&counterTask->transactions[transactionIndex]->answer[aIndex + 12]);

					//debug
					COMMON_STR_DEBUG(DEBUG_UJUK "ENERGY MONTH [%d] T%d A+ %d", counterTask->transactions[transactionIndex]->monthEnergyRequest ,COMMON_HexToDec(tarifIndex), eAp);
					COMMON_STR_DEBUG(DEBUG_UJUK "ENERGY MONTH [%d] T%d A- %d", counterTask->transactions[transactionIndex]->monthEnergyRequest ,COMMON_HexToDec(tarifIndex), eAm);
					COMMON_STR_DEBUG(DEBUG_UJUK "ENERGY MONTH [%d] T%d R+ %d", counterTask->transactions[transactionIndex]->monthEnergyRequest ,COMMON_HexToDec(tarifIndex), eRp);
					COMMON_STR_DEBUG(DEBUG_UJUK "ENERGY MONTH [%d] T%d R- %d", counterTask->transactions[transactionIndex]->monthEnergyRequest ,COMMON_HexToDec(tarifIndex), eRm);

					monthEnergy[ counterTask->transactions[transactionIndex]->monthEnergyRequest ].ratio = ratio ;
					monthEnergy[ counterTask->transactions[transactionIndex]->monthEnergyRequest ].t[ tarifIndex - 1 ].e[ ENERGY_AP - 1 ] = eAp ;
					monthEnergy[ counterTask->transactions[transactionIndex]->monthEnergyRequest ].t[ tarifIndex - 1 ].e[ ENERGY_AM - 1 ] = eAm ;
					monthEnergy[ counterTask->transactions[transactionIndex]->monthEnergyRequest ].t[ tarifIndex - 1 ].e[ ENERGY_RP - 1 ] = eRp ;
					monthEnergy[ counterTask->transactions[transactionIndex]->monthEnergyRequest ].t[ tarifIndex - 1 ].e[ ENERGY_RM - 1 ] = eRm ;

					if ( counterPtr )
					{
						if ( !(resultEnergyMonthMsk[ counterTask->transactions[transactionIndex]->monthEnergyRequest ] & (1 << (tarifIndex - 1)) ) )
						{
							resultEnergyMonthMsk[ counterTask->transactions[transactionIndex]->monthEnergyRequest ] |= (1 << (tarifIndex - 1) ) ;
							if ( resultEnergyMonthMsk[ counterTask->transactions[transactionIndex]->monthEnergyRequest ] == counterPtr->maskTarifs )
							{
								resultEnergyMonth[ counterTask->transactions[transactionIndex]->monthEnergyRequest ] = READY ;
								readyMonthEnergyCount++ ;
								if ( readyMonthEnergyCount == counterTask->monthRequestCount )
								{
									counterTask->resultMonthReady = READY;
								}
							}
						}
					}
				}
				break;

				case TRANSACTION_PROFILE_POINTER:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_PROFILE_POINTER");

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					if ( !(counterTask->transactions[transactionIndex]->answer[ aIndex ] & 0x0F) )
					{
						if ( counterPtr )
						{
							counterPtr->profileReadingState = STORAGE_PROFILE_STATE_WAITING_SEARCH_RES ;
							counterPtr->lastTaskRepeateFlag = 1;
						}
					}

					transactionResult = TRANSACTION_DONE_OK;
				}
				break;

				case TRANSACTION_UJUK_CURR_PPTR:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_UJUK_CURR_PPTR");

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					if ( counterPtr )
					{
						if ( COMMON_CheckProfileIntervalForValid( counterPtr->profileInterval ) == OK )
						{
							counterPtr->profilePtrCurrOverflow = (counterTask->transactions[transactionIndex]->answer[ aIndex ] & 0x80) >> 7 ;
							counterPtr->profileCurrPtr = COMMON_BufToShort( &counterTask->transactions[transactionIndex]->answer[ aIndex + 5] ) ;
							counterPtr->profileCurrPtr -= (( COMMON_Translate2_10to10( counterTask->transactions[transactionIndex]->answer[ aIndex ] & 0x7F ) / counterPtr->profileInterval ) + 1)  * 8 ;

							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK current ptr [%04X] overflow flg [%hhu]", counterPtr->profileCurrPtr, counterPtr->profilePtrCurrOverflow);
						}

						transactionResult = TRANSACTION_DONE_OK;
					}


				}
				break;

				case TRANSACTION_PROFILE_GET_SEARCHING_STATE:
				{
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					counterPtr->lastTaskRepeateFlag = 1;
					switch( counterTask->transactions[transactionIndex]->answer[ aIndex ] )
					{
						case 0x00:
							//slice was found
							counterPtr->profileReadingState = STORAGE_PROFILE_STATE_READ_BY_PTR ;
							counterPtr->profilePointer = COMMON_BufToShort( &counterTask->transactions[transactionIndex]->answer[ aIndex + 3] );
							COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : SLISE fOUND AT %u", counterPtr->profilePointer); //new profile debug

							if (counterPtr->profilePointer == counterPtr->profileCurrPtr)
							{
								counterPtr->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;
								counterPtr->profilePointer = POINTER_EMPTY ;
								COMMON_STR_DEBUG(DEBUG_NEWPROFILE "PROFILE TR CREATE : WAS FOUND CURRENT PTR! DO NOT READ!");
							}
							break;

						case 0x01:
							//search in progress now. Do not change state
							break;

						case 0x02:
							//counter could not find profile. save empty profile
							counterPtr->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;

							COMMON_STR_DEBUG( DEBUG_NEWPROFILE "PROFILE NOT FOUND, SAVE EMTPY FOR " DTS_PATTERN, DTS_GET_BY_VAL(counterPtr->profileLastRequestDts)); //new profile debug

							//save empty profile for all day
							counterTask->resultProfile.status = METERAGE_NOT_PRESET ;
							memcpy( &counterTask->resultProfile.dts, &counterPtr->profileLastRequestDts, sizeof(dateTimeStamp));

							int pri = 0 ;							
							for ( pri =0 ; pri < ( (60 / counterPtr->profileInterval) * 24 ); pri++)
							{

								if ((counterTask->resultProfile.dts.d.y == counterPtr->currentDts.d.y) &&
								(counterTask->resultProfile.dts.d.m == counterPtr->currentDts.d.m) &&
								(counterTask->resultProfile.dts.d.d == counterPtr->currentDts.d.d) &&
								(counterTask->resultProfile.dts.t.h == counterPtr->currentDts.t.h)){
									COMMON_STR_DEBUG( DEBUG_NEWPROFILE "PROFILE STOP SAVING, COUNTER CURRENT TIME REACHED");
									break;
								}

								STORAGE_SaveProfile( counterPtr->dbId , &counterTask->resultProfile );
								int i = 0 ;
								for ( ; i < counterPtr->profileInterval ; ++i){
									COMMON_ChangeDts( &counterTask->resultProfile.dts, INC , BY_MIN );
								}
							}
							break;

						default:
							//unknown result
							counterPtr->lastTaskRepeateFlag = 0;
							counterPtr->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;
							break;
					}
				}
				break;

				case TRANSACTION_PROFILE_GET_ALL_HOUR:
				{
					int hourSize;
					if ( COMMON_CheckProfileIntervalForValid(counterPtr->profileInterval) != OK )
					{
						break;
					}

					hourSize = 8 * ((60 / counterPtr->profileInterval) + 1);

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;



					while ( (aIndex + hourSize) < counterTask->transactions[transactionIndex]->answerSize )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK Parse profile from aIndex [%d]", aIndex);
						//check head crc
						unsigned int crc = counterTask->transactions[transactionIndex]->answer[ aIndex ] +
										   counterTask->transactions[transactionIndex]->answer[ aIndex + 1] +
										   counterTask->transactions[transactionIndex]->answer[ aIndex + 2] +
										   counterTask->transactions[transactionIndex]->answer[ aIndex + 3] +
										   counterTask->transactions[transactionIndex]->answer[ aIndex + 4] +
										   counterTask->transactions[transactionIndex]->answer[ aIndex + 5] ;
						crc &= 0xFF ;

						if ( crc != counterTask->transactions[transactionIndex]->answer[ aIndex + 6] )
						{
							//head is invalid

							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK crc is invalid!");
							//TODO
							break;
						}

						if ( counterPtr->profileInterval != counterTask->transactions[transactionIndex]->answer[ aIndex + 5] )
						{
							//interal is invalid

							//TODO
							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK interval is invalid!");
							break;
						}

						counterTask->resultProfile.dts.t.h = COMMON_Translate2_10to10(counterTask->transactions[transactionIndex]->answer[ aIndex ] );
						counterTask->resultProfile.dts.d.d = COMMON_Translate2_10to10(counterTask->transactions[transactionIndex]->answer[ aIndex + 1] );
						counterTask->resultProfile.dts.d.m = COMMON_Translate2_10to10(counterTask->transactions[transactionIndex]->answer[ aIndex + 2] );
						counterTask->resultProfile.dts.d.y = COMMON_Translate2_10to10(counterTask->transactions[transactionIndex]->answer[ aIndex + 3] );

						int sli = 0;
						for ( ; sli < (60 / counterPtr->profileInterval) ; ++sli )
						{
							aIndex += 8;
							UJUK_ParseProfileBuf( counterPtr, &counterTask->transactions[transactionIndex]->answer[ aIndex ] , &counterTask->resultProfile.p , &counterTask->resultProfile.status) ;

							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK Save profile slice for " DTS_PATTERN, DTS_GET_BY_VAL(counterTask->resultProfile.dts));

							STORAGE_SaveProfile( counterTask->counterDbId , &counterTask->resultProfile );

							int i = 0 ;
							for ( ; i < counterPtr->profileInterval ; ++i)
								COMMON_ChangeDts( &counterTask->resultProfile.dts, INC , BY_MIN );
						}

						if ( counterPtr->profileReadingState == STORAGE_PROFILE_STATE_READ_BY_PTR )
							counterPtr->profileReadingState = STORAGE_PROFILE_STATE_NOT_PROCESS ;
						else
						{
							counterPtr->profilePointer = UJUK_GetNextProfileHeadPtr( counterPtr , counterPtr->profilePointer );
							STORAGE_UpdateCounterStatus_LastProfilePtr( counterPtr , counterPtr->profilePointer ) ;
						}

						aIndex += 8;

					}

					counterPtr->lastTaskRepeateFlag = 1;

				}
					break;

				case TRANSACTION_GET_EVENT:
				{
					if ( !counterTask->transactions[transactionIndex]->shortAnswerFlag )
						UJUK_ParseEventBuf(counterPtr, counterTask->transactions[transactionIndex]);
					else
						UJUK_StepToNextJournal(counterPtr);
				}
				break;


				case TRANSACTION_GET_PQP_FREQUENCY:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_FREQUENCY was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.frequency = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.frequency =  value * 1000  / 100 ;
					}
					pqpOkFlag++;


				}
				break;

				case TRANSACTION_GET_PQP_VOLTAGE_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_VOLTAGE_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.U.a = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.a = value * 1000  / 100 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_VOLTAGE_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_VOLTAGE_PHASE2 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.U.b = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.b = value * 1000  / 100 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_VOLTAGE_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_VOLTAGE_PHASE3 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.U.c = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.c = value * 1000  / 100 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_VOLTAGE_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_VOLTAGE_GROUP was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.U.a = 0 ;
						pqp.U.b = 0 ;
						pqp.U.c = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						//no phase sigma value
						aIndex += 3 ;
						unsigned int value = 0;

						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.a = value * 1000  / 100 ;

						aIndex += 3 ;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.b = value * 1000  / 100 ;

						aIndex += 3 ;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.U.c = value * 1000  / 100 ;
					}
					pqpOkFlag++;
				}
				break;

				case TRANSACTION_GET_PQP_AMPERAGE_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_AMPERAGE_PHASE1 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.I.a = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;

						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						//pqp.I.a = value * 1000  / 100 ;
						pqp.I.a = value * Ci  / 10 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_AMPERAGE_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_AMPERAGE_PHASE2 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.I.b = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;

						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						//pqp.I.a = value * 1000  / 100 ;
						pqp.I.b = value * Ci  / 10 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_AMPERAGE_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_AMPERAGE_PHASE3 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.I.c = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;

						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						//pqp.I.a = value * 1000  / 100 ;
						pqp.I.c = value * Ci  / 10 ;
						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_AMPERAGE_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_AMPERAGE_GROUP was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.I.a = 0 ;
						pqp.I.b = 0 ;
						pqp.I.c = 0 ;

					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;

						unsigned int value = 0;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						//no phase sigma value
						aIndex += 3 ;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.I.a = value * Ci  / 10 ;

						aIndex += 3 ;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.I.b = value * Ci  / 10 ;

						aIndex += 3 ;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.I.c = value * Ci  / 10 ;
					}
					pqpOkFlag++;
				}
				break;

				case TRANSACTION_GET_PQP_POWER_P_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_P_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.P.a = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.P.a = value * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.a *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_P_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_P_PHASE2 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.P.b = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.P.b = value * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.b *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_P_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_P_PHASE3 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.P.c = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.P.c = value * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.c *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_P_PHASE_SIGMA:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_P_PHASE_SIGMA was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.P.sigma = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.P.sigma = value * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.sigma *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_P_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_P_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.P.a = 0 ;
						pqp.P.b = 0 ;
						pqp.P.c = 0 ;
						pqp.P.sigma = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);

						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.P.sigma = value * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.sigma *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.P.a = value * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.a *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.P.b = value * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.b *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.P.c = value * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 )
						{
							//value is negative
							pqp.P.c *=-1;
						}
					}
					pqpOkFlag++;
				}
				break;

				case TRANSACTION_GET_PQP_POWER_Q_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_Q_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.Q.a = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.Q.a = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.a *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_Q_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_Q_PHASE2 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.Q.b = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.Q.b = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.b *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_Q_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_Q_PHASE3 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.Q.c = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.Q.c = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.c *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_Q_PHASE_SIGMA:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_Q_PHASE_SIGMA was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.Q.sigma = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.Q.sigma = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.sigma *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_Q_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_Q_GROUP was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.Q.a = 0 ;
						pqp.Q.b = 0 ;
						pqp.Q.c = 0 ;
						pqp.Q.sigma = 0 ;

					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);

						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.Q.sigma = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.sigma *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.Q.a = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.a *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.Q.b = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.b *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.Q.c = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x40) != 0 )
						{
							//value is negative
							pqp.Q.c *=-1;
						}
					}
					pqpOkFlag++;
				}
				break;

				case TRANSACTION_GET_PQP_POWER_S_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_S_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.S.a = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.S.a = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0xC0
						{
							//value is negative
							pqp.S.a *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_S_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_S_PHASE2 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.S.b = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.S.b = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0xC0
						{
							//value is negative
							pqp.S.b *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_S_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_S_PHASE3 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.S.c = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.S.c = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0xC0
						{
							//value is negative
							pqp.S.c *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_S_PHASE_SIGMA:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_S_PHASE_SIGMA was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.S.sigma = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);
						pqp.S.sigma = value  * Kc;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0xC0
						{
							//value is negative
							pqp.S.sigma *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_POWER_S_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_POWER_S_GROUP was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.S.a = 0 ;
						pqp.S.b = 0 ;
						pqp.S.c = 0 ;
						pqp.S.sigma = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						int Kc = 0 ;
						int Ci = 0 ;
						UJUK_GetTransformationRatioByType( counterPtr , &Kc , &Ci);

						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.S.sigma = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0x40
						{
							//value is negative
							pqp.S.sigma *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.S.a = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0x40
						{
							//value is negative
							pqp.S.a *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.S.b = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0x40
						{
							//value is negative
							pqp.S.b *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.S.c = value  * Kc;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was &0x40
						{
							//value is negative
							pqp.S.c *=-1;
						}
					}
					pqpOkFlag++;
				}
				break;

				case TRANSACTION_GET_PQP_RATIO_COS_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_COS_PHASE1 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.cos_ab = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_ab = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_ab *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_COS_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_COS_PHASE2 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.cos_bc = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_bc = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_bc *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_COS_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_COS_PHASE3 was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.cos_ac = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_ac = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_ac *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_COS_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_COS_GROUP was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.cos_ab = 0 ;
						pqp.coeff.cos_bc = 0 ;
						pqp.coeff.cos_ac = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;

						//no value for phase sigma
						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_ab = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_ab *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_bc = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_bc *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.cos_ac = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0x80) != 0 ) //was&0xC0
						{
							//value is negative
							pqp.coeff.cos_ac *=-1;
						}
					}
					pqpOkFlag++;

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_TAN_PHASE1:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_TAN_PHASE1 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.tan_ab = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_ab = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_ab *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_TAN_PHASE2:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_TAN_PHASE2 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.tan_bc = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_bc = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_bc *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_TAN_PHASE3:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_TAN_PHASE3 was catch");
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.tan_ac = 0 ;
						pqpOkFlag++;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_ac = value * 1000  / 100 ;

						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_ac *=-1;
						}

						pqpOkFlag++;
					}

				}
				break;

				case TRANSACTION_GET_PQP_RATIO_TAN_GROUP:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_GET_PQP_RATIO_TAN_GROUP was catch");

					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == TRUE )
					{
						COMMON_STR_DEBUG(DEBUG_UJUK "UJUK there is only error code in answer. Fill the value by zero");
						pqp.coeff.tan_ab = 0 ;
						pqp.coeff.tan_bc = 0 ;
						pqp.coeff.tan_ac = 0 ;
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;
						unsigned int value = 0;

						//no value for phase sigma
						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_ab = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_ab *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_bc = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_bc *=-1;
						}

						aIndex += 3;
						value = (((counterTask->transactions[transactionIndex]->answer[aIndex] & 0x3F) << 16 ) + \
								((counterTask->transactions[transactionIndex]->answer[aIndex + 1] & 0xFF) << 8 ) + \
								(counterTask->transactions[transactionIndex]->answer[aIndex + 2] & 0xFF)) ;
						pqp.coeff.tan_ac = value * 1000  / 100 ;
						if ( (counterTask->transactions[transactionIndex]->answer[aIndex] & 0xC0) != 0 )
						{
							//value is negative
							pqp.coeff.tan_ac *=-1;
						}
					}
					pqpOkFlag++;

				}
				break;

				case TRANSACTION_WRITING_TARIFF_MAP_TO_PH_MEMORY:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_WRITING_TARIFF_MAP_TO_PH_MEMORY was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if (result == ZERO)
					{
						if (tariffMapTransactionCounter >= 0)
							tariffMapTransactionCounter++ ;
					}
					else if (result == UJUK_RES_ERR_PWD )
					{
						tariffMapTransactionCounter = -2 ;
					}
					else
						tariffMapTransactionCounter = -1;

				}
				break;

				case TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY was catch");


					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if (result == ZERO)
					{
						if (tariffHolidaysTransactionCounter >= 0)
							tariffHolidaysTransactionCounter ++;
					}
					else if (result == UJUK_RES_ERR_PWD )
					{
						tariffHolidaysTransactionCounter =-2 ;
					}
					else
						tariffHolidaysTransactionCounter =-1 ;

				}
				break;

				case TRANSACTION_WRITING_TARIFF_MOVED_DAYS_TO_PH_MEMMORY:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_WRITING_TARIFF_MOVED_DAYS_TO_PH_MEMMORY was catch");

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if (result == ZERO)
					{
						if ( tariffMovedDaysTransactionsCounter >= 0 )
							tariffMovedDaysTransactionsCounter++;
					}
					else if (result == UJUK_RES_ERR_PWD )
					{
						tariffMovedDaysTransactionsCounter = -2 ;
					}
					else
						tariffMovedDaysTransactionsCounter = -1 ;

				}
				break;

				case  TRANSACTION_WRITING_INDICATIONS:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_WRITING_INDICATIONS was catch");

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if ( result == ZERO )
					{
						if ( indicationWritingTransactionCounter >= 0 )
							indicationWritingTransactionCounter++;
					}
					else if ( result == UJUK_RES_ERR_PWD )
					{
						indicationWritingTransactionCounter = -2 ;
					}
					else
						indicationWritingTransactionCounter = -1 ;
				}
				break;

				case TRANSACTION_SET_CURRENT_SEASON:
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TRANSACTION_SET_CURRENT_SEASON was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					#if !REV_COMPILE_RELEASE
					unsigned char result = counterTask->transactions[transactionIndex]->answer[aIndex] & 0x0F;
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK result = [%02X]" , result );
					#endif

				}
				break;

				case TRANSACTION_ALLOW_SEASON_CHANGING:
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TRANSACTION_ALLOW_SEASON_CHANGING was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char result = counterTask->transactions[transactionIndex]->answer[aIndex] & 0x0F;
					if ( result != 0 )
					{
						//create journal event
						unsigned char desc[ EVENT_DESC_SIZE ];
						memset(desc , 0 , EVENT_DESC_SIZE);

						desc[0] = (char)( ( counterPtr->dbId & 0xFF000000) >> 24 );
						desc[1] = (char)( ( counterPtr->dbId & 0xFF0000) >> 16 );
						desc[2] = (char)( ( counterPtr->dbId & 0xFF00) >> 8 );
						desc[3] = (char) ( counterPtr->dbId & 0xFF) ;
						STORAGE_JournalUspdEvent( EVENT_UNABLE_TO_SET_ALLOW_SEASON_FLG , desc );
					}

					//do not check result. Set season anyway
					unsigned char seasonFlg = STORAGE_GetSeasonAllowFlag();
					STORAGE_UpdateCounterStatus_AllowSeasonChangeFlg( counterPtr , seasonFlg ) ;

					counterPtr->lastTaskRepeateFlag = 1 ;

					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK result = [%02X]" , result );
				}
				break;

				case TRANSACTION_SYNC_HARDY:
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "UJUK TRANSACTION_SYNC_HARDY was catch");

					COMMON_BUF_DEBUG( DEBUG_UJUK "REQUEST" , counterTask->transactions[transactionIndex]->command , counterTask->transactions[transactionIndex]->commandSize );
					COMMON_BUF_DEBUG( DEBUG_UJUK "ANSWER: ", counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize);

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if ( counterPtr )
					{
						if ( result == 0 )
						{
								STORAGE_SwitchSyncFlagTo_SyncDone(&counterPtr);
								COMMON_STR_DEBUG( DEBUG_UJUK "UJUK time syncing done");

								//clear meterages
								STORAGE_ClearMeteragesAfterHardSync( counterPtr );

								//create journal event
								unsigned char desc[ EVENT_DESC_SIZE ];
								memset(desc , 0 , EVENT_DESC_SIZE);

								desc[0] = (char)( ( counterPtr->dbId & 0xFF000000) >> 24 );
								desc[1] = (char)( ( counterPtr->dbId & 0xFF0000) >> 16 );
								desc[2] = (char)( ( counterPtr->dbId & 0xFF00) >> 8 );
								desc[3] = (char) ( counterPtr->dbId & 0xFF) ;
								STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_CLEAR , desc );								
						}
						else
						{
							STORAGE_SwitchSyncFlagTo_SyncFail(&counterPtr);
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK time syncing is impossible by reason [%hhu]" , result);
						}
					}

				}
				break;

				case TRANSACTION_SYNC_SOFT:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_SYNC_SOFT was catch");
					COMMON_BUF_DEBUG( DEBUG_UJUK "REQUEST" , counterTask->transactions[transactionIndex]->command , counterTask->transactions[transactionIndex]->commandSize );
					COMMON_BUF_DEBUG( DEBUG_UJUK "ANSWER: ", counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize);

					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];
					if ( counterPtr )
					{
						switch (result)
						{
							case 0x00:
							{
								STORAGE_SwitchSyncFlagTo_SyncDone(&counterPtr);
								COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_SYNC_SOFT OK");
							}
							break;

							case 0x04:
							case 0x08:
							{
								STORAGE_SwitchSyncFlagTo_SyncFail(&counterPtr);
								COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_SYNC_SOFT IS IN PROCESS NOW");
							}
							break;

							default:
							{
								counterPtr->syncWorth = UJUK_SOFT_HARD_SYNC_SW + 1 ;
								COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_SYNC_SOFT IS IMPOSSIBLE BY 120 sec");
							}
							break;
						}
					}

				}
				break;

				case TRANSACTION_UJUK_SET_POWER_CONTROLS:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_UJUK_SET_POWER_CONTROLS was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];

					if (powerControlTransactionCounter >= 0)
					{
						if ( (result == 0x00) || (result == 0x06))
						{
							powerControlTransactionCounter++ ;
						}
						else if ( result == UJUK_RES_ERR_PWD )
						{
							powerControlTransactionCounter = -2 ;
						}
						else
						{
							powerControlTransactionCounter = -1 ;
						}
					}
				}
				break;

				case TRANSACTION_UJUK_SET_PROGRAM_FLAGS:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_UJUK_SET_PROGRAM_FLAGS was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];
					if (powerControlTransactionCounter >= 0)
					{
						if ( result == 0x00)
						{
							powerControlTransactionCounter++ ;
						}
						else if ( result == UJUK_RES_ERR_PWD )
						{
							powerControlTransactionCounter = -2 ;
						}
						else
						{
							powerControlTransactionCounter = -1 ;
						}
					}
				}
				break;

				case TRANSACTION_UJUK_SET_POWER_LIMIT_CONTROL:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_UJUK_SET_POWER_LIMIT_CONTROL was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];
					if (powerControlTransactionCounter >= 0)
					{
						if ( result == 0x00)
						{
							powerControlTransactionCounter++ ;
						}
						else if ( result == UJUK_RES_ERR_PWD )
						{
							powerControlTransactionCounter = -2 ;
						}
						else
						{
							powerControlTransactionCounter = -1 ;
						}
					}
				}
				break;

				case TRANSACTION_UJUK_GET_COUNTER_STATE_WORD:
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK TRANSACTION_UJUK_GET_COUNTER_STATE_WORD was catch");
					aIndex = 1;
					if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
						aIndex += 4;

					//try to get state word from storage
					counterStateWord_t counterStateWord ;
					memset( &counterStateWord , 0 , sizeof(counterStateWord_t) ) ;
					int stateWordLength = STORAGE_COUNTER_STATE_WORD ;
					if ( counterPtr->type == TYPE_UJUK_SET_4TM_01A )
					{
						--stateWordLength ;
					}
					memcpy( counterStateWord.word , &counterTask->transactions[transactionIndex]->answer[ aIndex ] , stateWordLength );

					BOOL needToUpdateStateWord = TRUE ;

					counterStatus_t counterStatus ;
					if ( STORAGE_LoadCounterStatus( counterTask->counterDbId , &counterStatus ) == OK)
					{
						counterStateWord_t zeroCounterStateWord ;
						memset( &zeroCounterStateWord , 0xFF , sizeof( counterStateWord_t ));
						if ( memcmp( &counterStatus.counterStateWord , &zeroCounterStateWord , sizeof( counterStateWord_t ) ) )
						{
							//check old and new state word
							if ( memcmp( &counterStatus.counterStateWord , &counterStateWord , sizeof( counterStateWord_t ) ) )
							{
								// state word was changed
								uspdEvent_t event;
								memset(&event , 0 , sizeof( uspdEvent_t ) ) ;

								COMMON_GetCurrentDts(&event.dts);
								event.evType = EVENT_SYNC_STATE_WORD_CH ;
								memcpy(&event.evDesc[0] , counterStatus.counterStateWord.word , STORAGE_COUNTER_STATE_WORD );
								memcpy(&event.evDesc[STORAGE_COUNTER_STATE_WORD] , counterStateWord.word , STORAGE_COUNTER_STATE_WORD );

								event.crc = STORAGE_CalcCrcForJournal(&event);
								STORAGE_SaveEvent(&event , counterTask->counterDbId);
							}
							else
							{
								needToUpdateStateWord = FALSE ;
							}
						}
					}

					if ( needToUpdateStateWord == TRUE )
					{
						STORAGE_UpdateCounterStatus_StateWord( counterPtr , &counterStateWord ) ;
					}
				}
				break;

				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE     :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 1 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 2 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 3 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 4 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 5 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 6 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 7 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 8 :
				case TRANSACTION_UJUK_WRITE_TARIFF_ZONE + 9 :				
				{
					if ( counterTask->transactions[transactionIndex]->shortAnswerFlag == FALSE )
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;

						unsigned char trId = counterTask->transactions[transactionIndex]->answer[ aIndex ] ;
						//unsigned char result = counterTask->transactions[transactionIndex]->answer[ aIndex + 1] ;

						if ( ( transactionType - TRANSACTION_UJUK_WRITE_TARIFF_ZONE ) == (trId % 10) )
						{

							//check	answer code
							unsigned char answCode = counterTask->transactions[transactionIndex]->answer[ aIndex + 1] & 0x0F;

							if ( answCode == 0x00 )
							{
								if ( tariffMapTransactionCounter >= 0 )
								{
									tariffMapTransactionCounter++ ;
								}
							}
							else
							{
								tariffMapTransactionCounter = -1 ;
							}
						}
						else
						{
							transactionResult = TRANSACTION_DONE_ERROR_A ;
						}
					}
					else
					{
						aIndex = 1;
						if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							aIndex += 4;

						unsigned char  result = counterTask->transactions[transactionIndex]->answer[aIndex];
						if ( result == UJUK_RES_ERR_PWD )
						{
							tariffMapTransactionCounter = -2 ;
						}
						else
						{
							tariffMapTransactionCounter = -1 ;
						}
					}
				}
				break;



				default:
					COMMON_STR_DEBUG(DEBUG_UJUK "UJUK UNKNOWN TRANSANCTION was catch 0x%X", transactionType);
					break;

			}

		} else {
			//if errprs in transaction reset repeat flags
			STORAGE_ResetRepeatFlag(counterTask->counterDbId);

			//set counter status with error
			STORAGE_SetCounterStatistic(counterTask->counterDbId, FALSE, transactionType, transactionResult);

			#if REV_COMPILE_EVENT_METER_FAIL
			if ( transactionResult == TRANSACTION_DONE_ERROR_SIZE )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_SIZE, evDesc);
				STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
			}
			else if ( transactionResult == TRANSACTION_DONE_ERROR_TIMEOUT )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_TIMEOUT, evDesc);
				STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
			}
			#endif

			//quit transaction results parse loop
			break;
		}
	}

	//save statistic if is all ok
	if(transactionResult == TRANSACTION_DONE_OK){
		STORAGE_SetCounterStatistic(counterTask->counterDbId, FALSE, 0, TRANSACTION_COUNTER_POOL_DONE);
		res = OK ;
	}

	//save results of its present
	if ((res == OK) && (counterTask->resultCurrentReady == READY)) {
		if ( COMMON_CheckSawInMeterage( &counterTask->resultCurrent ) == OK )
		{
			res = STORAGE_SaveMeterages(STORAGE_CURRENT, counterTask->counterDbId, &counterTask->resultCurrent);
		}
		else
		{
			//res = ERROR_GENERAL ;

			#if REV_COMPILE_EVENT_METER_FAIL
			unsigned char evDesc[EVENT_DESC_SIZE];
			unsigned char dbidDesc[EVENT_DESC_SIZE];
			memset(dbidDesc , 0, EVENT_DESC_SIZE);
			COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
			COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
			STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
			#endif
		}
	}

	switch( counterTask->counterType )
	{
		//osipov sets
		default:
//		case TYPE_UJUK_SET_4TM_03A:
//		//case TYPE_UJUK_SET_4TM_02MB: //osipov sets
//		case TYPE_UJUK_SET_4TM_03MB:
//		case TYPE_UJUK_PSH_4TM_05A:
//		case TYPE_UJUK_PSH_4TM_05D:
//		case TYPE_UJUK_PSH_4TM_05MB:
//		case TYPE_UJUK_PSH_4TM_05MK:
//		case TYPE_UJUK_PSH_3TM_05A:
//		case TYPE_UJUK_PSH_3TM_05D:
//		case TYPE_UJUK_PSH_3TM_05MB:
//		case TYPE_UJUK_SEB_1TM_01A:
//		case TYPE_UJUK_SEB_1TM_02A:
//		case TYPE_UJUK_SEB_1TM_02D:
//		case TYPE_UJUK_SEB_1TM_02M:
		{
			if ((res == OK) && (counterTask->resultDayReady == READY))
			{

				//check saw and dublicate in meterages

				if ( COMMON_CheckSawInMeterage( &counterTask->resultDay ) == OK )
				{
					res = STORAGE_SaveMeterages(STORAGE_DAY, counterTask->counterDbId, &counterTask->resultDay);
				}
				else
				{
					//res = ERROR_GENERAL ;
					#if REV_COMPILE_EVENT_METER_FAIL
					unsigned char evDesc[EVENT_DESC_SIZE];
					unsigned char dbidDesc[EVENT_DESC_SIZE];
					memset(dbidDesc , 0, EVENT_DESC_SIZE);
					COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
					COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
					STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
					#endif
				}
			}
		}
		break;

		//osipov sets
		case TYPE_UJUK_SET_4TM_01A:
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_02MB:
		//default:
		{
			if ( (resultEnergyToday == READY) && (counterTask->resultCurrentReady == READY))
			{
				BOOL saveTodayMeterage = TRUE ;

				//
				//saving yesterday meterage
				//

				if ( resultEnergyYesterday == READY )
				{
					saveTodayMeterage = FALSE ;
					counterTask->resultDay.ratio = ratio ;
					counterTask->resultDay.status = METERAGE_CALCULATED_VALUE;

					int tarifIndex = 0 ;
					for( ; tarifIndex < MAX_TARIFF_NUMBER ; ++tarifIndex )
					{
						counterTask->resultDay.t[ tarifIndex ].e[ ENERGY_AP - 1] = counterTask->resultCurrent.t[ tarifIndex ].e[ ENERGY_AP - 1] - dayEnergyToday.t[ tarifIndex ].e[ ENERGY_AP - 1] - dayEnergyYesterday.t[ tarifIndex ].e[ ENERGY_AP - 1];
						counterTask->resultDay.t[ tarifIndex ].e[ ENERGY_AM - 1] = counterTask->resultCurrent.t[ tarifIndex ].e[ ENERGY_AM - 1] - dayEnergyToday.t[ tarifIndex ].e[ ENERGY_AM - 1] - dayEnergyYesterday.t[ tarifIndex ].e[ ENERGY_AM - 1];
						counterTask->resultDay.t[ tarifIndex ].e[ ENERGY_RP - 1] = counterTask->resultCurrent.t[ tarifIndex ].e[ ENERGY_RP - 1] - dayEnergyToday.t[ tarifIndex ].e[ ENERGY_RP - 1] - dayEnergyYesterday.t[ tarifIndex ].e[ ENERGY_RP - 1];
						counterTask->resultDay.t[ tarifIndex ].e[ ENERGY_RM - 1] = counterTask->resultCurrent.t[ tarifIndex ].e[ ENERGY_RM - 1] - dayEnergyToday.t[ tarifIndex ].e[ ENERGY_RM - 1] - dayEnergyYesterday.t[ tarifIndex ].e[ ENERGY_RM - 1];
					}
					//saving meterage
					if ( COMMON_CheckSawInMeterage( &counterTask->resultDay ) == OK )
					{
						res = STORAGE_SaveMeterages(STORAGE_DAY, counterTask->counterDbId, &counterTask->resultDay);
						if ( res == OK )
						{
							saveTodayMeterage = TRUE ;
							COMMON_ChangeDts( &counterTask->resultDay.dts , INC , BY_DAY );
						}
					}
					else
					{
						#if REV_COMPILE_EVENT_METER_FAIL
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
						STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
						#endif
					}

				}

				//
				//saving today meterage
				//

				if ( saveTodayMeterage == TRUE )
				{
					counterTask->resultDay.ratio = ratio ;
					counterTask->resultDay.status = METERAGE_CALCULATED_VALUE;

					int tariffIndex = 0 ;
					for( ; tariffIndex < MAX_TARIFF_NUMBER ; ++tariffIndex )
					{
						counterTask->resultDay.t[ tariffIndex ].e[ ENERGY_AP - 1] = counterTask->resultCurrent.t[ tariffIndex ].e[ ENERGY_AP - 1] - dayEnergyToday.t[ tariffIndex ].e[ ENERGY_AP - 1] ;
						counterTask->resultDay.t[ tariffIndex ].e[ ENERGY_AM - 1] = counterTask->resultCurrent.t[ tariffIndex ].e[ ENERGY_AM - 1] - dayEnergyToday.t[ tariffIndex ].e[ ENERGY_AM - 1] ;
						counterTask->resultDay.t[ tariffIndex ].e[ ENERGY_RP - 1] = counterTask->resultCurrent.t[ tariffIndex ].e[ ENERGY_RP - 1] - dayEnergyToday.t[ tariffIndex ].e[ ENERGY_RP - 1] ;
						counterTask->resultDay.t[ tariffIndex ].e[ ENERGY_RM - 1] = counterTask->resultCurrent.t[ tariffIndex ].e[ ENERGY_RM - 1] - dayEnergyToday.t[ tariffIndex ].e[ ENERGY_RM - 1] ;
					}

					//saving meterage
					if ( COMMON_CheckSawInMeterage( &counterTask->resultDay ) == OK )
					{
						res = STORAGE_SaveMeterages(STORAGE_DAY, counterTask->counterDbId, &counterTask->resultDay);
					}
					else
					{
						#if REV_COMPILE_EVENT_METER_FAIL
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
						STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
						#endif
					}
				}
			}
		}
		break;
	}


	if ((res == OK) && (counterTask->resultMonthReady == READY)) {

		switch( counterTask->counterType )
		{
			//osipov sets
			default:
//			case TYPE_UJUK_SET_4TM_03A:
//			//case TYPE_UJUK_SET_4TM_02MB: //osipov sets
//			case TYPE_UJUK_SET_4TM_03MB:
//			case TYPE_UJUK_PSH_4TM_05A:
//			case TYPE_UJUK_PSH_4TM_05D:
//			case TYPE_UJUK_PSH_4TM_05MB:
//			case TYPE_UJUK_PSH_4TM_05MK:
//			case TYPE_UJUK_PSH_3TM_05A:
//			case TYPE_UJUK_PSH_3TM_05D:
//			case TYPE_UJUK_PSH_3TM_05MB:
//			case TYPE_UJUK_SEB_1TM_01A:
//			case TYPE_UJUK_SEB_1TM_02A:
//			case TYPE_UJUK_SEB_1TM_02D:
//			case TYPE_UJUK_SEB_1TM_02M:
			{
				if ( COMMON_CheckSawInMeterage( &counterTask->resultMonth ) == OK )
				{
					res = STORAGE_SaveMeterages(STORAGE_MONTH, counterTask->counterDbId, &counterTask->resultMonth);
				}
				else
				{
					#if REV_COMPILE_EVENT_METER_FAIL
					//res = ERROR_GENERAL ;

					unsigned char evDesc[EVENT_DESC_SIZE];
					unsigned char dbidDesc[EVENT_DESC_SIZE];
					memset(dbidDesc , 0, EVENT_DESC_SIZE);
					COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
					COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
					STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
					#endif
				}
			}
			break;

			//osipov sets
			case TYPE_UJUK_SET_4TM_01A:
			case TYPE_UJUK_SET_4TM_02A:
			case TYPE_UJUK_SET_4TM_02MB:
			//default:
			{
				counterTask->resultMonth.ratio = ratio ;
				counterTask->resultMonth.status = METERAGE_CALCULATED_VALUE ;

				int monthIndex = 0 ;
				for ( ; monthIndex < counterTask->monthRequestCount ; ++monthIndex )
				{
					COMMON_STR_DEBUG( DEBUG_UJUK "\nUJUK energy month index [%d] for month [%hhu]" , monthIndex , counterTask->resultMonth.dts.d.m );
					memcpy( counterTask->resultMonth.t , counterTask->resultCurrent.t , MAX_TARIFF_NUMBER * sizeof(energy_t) );
					int offsetIndex = monthIndex ;
					for ( ; offsetIndex < counterTask->monthRequestCount ; ++offsetIndex )
					{
						if ( resultEnergyMonth[ offsetIndex ] == READY )
						{
							COMMON_STR_DEBUG( DEBUG_UJUK "UJUK decrement value by month index [%d]", offsetIndex);
							int tariffIndex = 0 ;
							for( ; tariffIndex < MAX_TARIFF_NUMBER ; ++tariffIndex )
							{
								counterTask->resultMonth.t[ tariffIndex ].e[ ENERGY_AP - 1] -= monthEnergy[ offsetIndex ].t[ tariffIndex ].e[ ENERGY_AP - 1] ;
								counterTask->resultMonth.t[ tariffIndex ].e[ ENERGY_AM - 1] -= monthEnergy[ offsetIndex ].t[ tariffIndex ].e[ ENERGY_AM - 1] ;
								counterTask->resultMonth.t[ tariffIndex ].e[ ENERGY_RP - 1] -= monthEnergy[ offsetIndex ].t[ tariffIndex ].e[ ENERGY_RP - 1] ;
								counterTask->resultMonth.t[ tariffIndex ].e[ ENERGY_RM - 1] -= monthEnergy[ offsetIndex ].t[ tariffIndex ].e[ ENERGY_RM - 1] ;
							}
						}
					}

					if ( COMMON_CheckSawInMeterage( &counterTask->resultMonth ) == OK )
					{
						res = STORAGE_SaveMeterages(STORAGE_MONTH, counterTask->counterDbId, &counterTask->resultMonth);
						if ( res != OK )
						{
							break;
						}
					}
					else
					{
						#if REV_COMPILE_EVENT_METER_FAIL
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_METERAGES_FAIL_DOUBLE, evDesc);
						STORAGE_JournalUspdEvent( EVENT_COUNTER_METERAGES_FAIL , evDesc );
						break;
						#endif
					}

					COMMON_ChangeDts( &counterTask->resultMonth.dts , INC , BY_MONTH );
				}
			}
			break;
		}

	}

#if 0
	if ((res == OK) && (counterTask->resultProfileReady == READY)) {
		res = STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);

		if ( res == OK )
		{
			if ( counterPtr )
			{
				if ((counterPtr->profilePointer == POINTER_EMPTY) && (counterPtr->tmpProfilePointer != POINTER_EMPTY))
				{
					counterPtr->profilePointer = counterPtr->tmpProfilePointer;
					counterPtr->tmpProfilePointer  = POINTER_EMPTY;
				}

				STORAGE_UpdateCounterStatus_LastProfilePtr( counterPtr , counterPtr->profilePointer );
			}
		}
	}
#endif

	// second profile by one time
//	if ((res == OK) && (secondProfileReadyFlag == READY))
//	{
//		res = STORAGE_SaveProfile(counterTask->counterDbId , &secondProfile);
//	}

#if 0
	COMMON_STR_DEBUG( DEBUG_UJUK "UJUK res = [%d] , energiesCount = [%d] ", res , allProfilesInHour.energiesCount );
	if ( (res == OK) && (allProfilesInHour.energiesCount != 0))
	{
		COMMON_STR_DEBUG( DEBUG_UJUK "UJUK profile array is not empty. Save it" );
		if ( allProfilesInHour.energies )
		{
			unsigned int profileIterator = 0 ;
			for ( ; profileIterator < allProfilesInHour.energiesCount ; ++profileIterator )
			{
				COMMON_STR_DEBUG( DEBUG_UJUK "UJUK saving [%d] slice" , profileIterator );
				if ( allProfilesInHour.energies[ profileIterator ] )
				{
					res = STORAGE_SaveProfile(counterTask->counterDbId , allProfilesInHour.energies[ profileIterator ] );
					free( allProfilesInHour.energies[ profileIterator ] );
					allProfilesInHour.energies[ profileIterator ] = NULL ;
				}
			}

			if ( ( res == OK ) && ( counterPtr ) )
			{
				if ((counterPtr->profilePointer == POINTER_EMPTY) && (counterPtr->tmpProfilePointer != POINTER_EMPTY))
				{
					counterPtr->profilePointer = counterPtr->tmpProfilePointer;
					counterPtr->tmpProfilePointer  = POINTER_EMPTY;
				}

				STORAGE_UpdateCounterStatus_LastProfilePtr( counterPtr , counterPtr->profilePointer );
			}

			free( allProfilesInHour.energies );
			allProfilesInHour.energies = NULL ;
		}
	}
#endif

	if (res == OK)
	{
		int waitedPqp = 0 ;
		if ((counterTask->counterType == TYPE_UJUK_SEB_1TM_01A) ||
			(counterTask->counterType == TYPE_UJUK_SEB_1TM_02A) ||
			(counterTask->counterType == TYPE_UJUK_SEB_1TM_02D) ||
			(counterTask->counterType == TYPE_UJUK_SEB_1TM_02M) ||
			(counterTask->counterType == TYPE_UJUK_SEB_1TM_03A)	)
		{
			waitedPqp = 7 ;
		}
		else if (( counterTask->counterType == TYPE_UJUK_SET_4TM_01A ) ||
				( counterTask->counterType == TYPE_UJUK_SET_4TM_02A ))
		{
			waitedPqp = 25 ;
		}
		else
		{
			waitedPqp = 8 ;
		}

		if (pqpOkFlag != waitedPqp)
		{
			COMMON_STR_DEBUG(DEBUG_UJUK "Waiting for pqpOkFlag = [%d], but pqpOkFlag = [%d]", waitedPqp , pqpOkFlag);			
		}
		else
		{
			COMMON_STR_DEBUG(DEBUG_UJUK "Pqp flag is OK");

			if( counterPtr )
			{
				if( COMMON_CheckDtsForValid( &counterPtr->currentDts ) == OK )
				{
					memcpy( &pqp.dts , &counterPtr->currentDts , sizeof( dateTimeStamp ));

					if ((counterPtr->type == TYPE_UJUK_SEB_1TM_01A) || \
						(counterPtr->type == TYPE_UJUK_SEB_1TM_02A) || \
						(counterPtr->type == TYPE_UJUK_SEB_1TM_02D) || \
						(counterPtr->type == TYPE_UJUK_SEB_1TM_02M) || \
						(counterPtr->type == TYPE_UJUK_SEB_1TM_03A)	)
					{
						pqp.P.sigma = pqp.P.a ;
						pqp.Q.sigma = pqp.Q.a ;
						pqp.S.sigma = pqp.S.a ;
					}
					res = STORAGE_SavePQP(&pqp , counterTask->counterDbId);
				}
			}



				//debug
				COMMON_STR_DEBUG( DEBUG_UJUK "Date: %hhu-%hhu-%hhu %hhu:%hhu:%hhu", pqp.dts.d.d , pqp.dts.d.m , pqp.dts.d.y , pqp.dts.t.h , pqp.dts.t.m , pqp.dts.t.s );

				COMMON_STR_DEBUG( DEBUG_UJUK "Voltage: A=%d B=%d C=%d" , pqp.U.a , pqp.U.b , pqp.U.c);
				COMMON_STR_DEBUG( DEBUG_UJUK "Amperage: A=%d B=%d C=%d", pqp.I.a , pqp.I.b , pqp.I.c);
				COMMON_STR_DEBUG( DEBUG_UJUK "P power: A=%d B=%d C=%d SIGMA=%d", pqp.P.a , pqp.P.b , pqp.P.c, pqp.P.sigma);
				COMMON_STR_DEBUG( DEBUG_UJUK "Q power: A=%d B=%d C=%d SIGMA=%d", pqp.Q.a , pqp.Q.b , pqp.Q.c, pqp.Q.sigma);
				COMMON_STR_DEBUG( DEBUG_UJUK "S power: A=%d B=%d C=%d SIGMA=%d", pqp.S.a , pqp.S.b , pqp.S.c, pqp.S.sigma);
				COMMON_STR_DEBUG( DEBUG_UJUK "Freuqency = %d" , pqp.frequency);
				COMMON_STR_DEBUG( DEBUG_UJUK "Ratios: cosAB=%d cosBC=%d cosAC=%d tanAB=%d tanBC=%d tanAC=%d", pqp.coeff.cos_ab, pqp.coeff.cos_bc, pqp.coeff.cos_ac, pqp.coeff.tan_ab, pqp.coeff.tan_bc, pqp.coeff.tan_ac);

				//EXIT_PATTERN;


		}
	}


	//
	//checking user commands status
	//

	if ( counterTask->userCommandsTransactionCounter > 0 )
	{
		if ( counterPtr )
		{
			if ( transactionResult == TRANSACTION_COUNTER_NOT_OPEN )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_ERR_PASS, evDesc);
				STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
				ACOUNT_StatusSet(counterPtr->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD , ACOUNTS_PART_USER_COMMANDS);
			}
			else if ( userCommandsCount == counterTask->userCommandsTransactionCounter )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_USR_CMNDS_WRITING_OK, evDesc);
				STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
				ACOUNT_StatusSet(counterPtr->dbId, ACOUNTS_STATUS_WRITTEN_OK , ACOUNTS_PART_USER_COMMANDS);

				//reset profile pointer and profile data saved for this counter to start profile reading again
				counterPtr->profilePointer = POINTER_EMPTY ;
				STORAGE_UpdateCounterStatus_LastProfilePtr( counterPtr , counterPtr->profilePointer);
			}
		}
	}

	//
	//checking tariff writing status
	//

	if ( res == OK )
	{
		if ( (tariffMapTransactionCounter != 0 ) || \
			 (tariffHolidaysTransactionCounter != 0 ) || \
			 (tariffMovedDaysTransactionsCounter != 0 ) || \
			 (indicationWritingTransactionCounter != 0) )
		{
			if ( (tariffMapTransactionCounter == (counterTask->tariffMapTransactionCounter) ) && \
				 (tariffHolidaysTransactionCounter == (counterTask->tariffHolidaysTransactionCounter) ) && \
				 (tariffMovedDaysTransactionsCounter == (counterTask->tariffMovedDaysTransactionsCounter) ) && \
				 (indicationWritingTransactionCounter == (counterTask->indicationWritingTransactionCounter)))
			{
				COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing OK");

				if ( counterPtr )
				{
					//counter->tariffMapWritingFlag = FALSE ;
					ACOUNT_StatusSet( counterPtr->dbId , ACOUNTS_STATUS_WRITTEN_OK, ACOUNTS_PART_TARIFF);

					unsigned char evDesc[EVENT_DESC_SIZE];
					unsigned char dbidDesc[EVENT_DESC_SIZE];
					memset(dbidDesc , 0, EVENT_DESC_SIZE);
					COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
					COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_OK, evDesc);
					STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
				}
			}

			else if ( (tariffMapTransactionCounter == -2 ) || \
					  (tariffHolidaysTransactionCounter == -2) || \
					  (tariffMovedDaysTransactionsCounter == -2) || \
					  (indicationWritingTransactionCounter == -2))
			{
				#if !REV_COMPILE_RELEASE
				if (tariffMapTransactionCounter < 0 )
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: general map");
				}
				if (tariffHolidaysTransactionCounter < 0)
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: holidays list");
				}
				if (tariffMovedDaysTransactionsCounter < 0)
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: moved days list");
				}
				if ( indicationWritingTransactionCounter < 0 )
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: indications");
				}
				#endif

				if ( counterPtr )
				{
					//counter->tariffMapWritingFlag = FALSE ;
					ACOUNT_StatusSet( counterPtr->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD , ACOUNTS_PART_TARIFF);

					unsigned char evDesc[EVENT_DESC_SIZE];
					unsigned char dbidDesc[EVENT_DESC_SIZE];
					memset(dbidDesc , 0, EVENT_DESC_SIZE);
					COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
					COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_PASS, evDesc);
					STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc);
				}
			}
			else if ( (tariffMapTransactionCounter == -1 ) || \
					  (tariffHolidaysTransactionCounter == -1) || \
					  (tariffMovedDaysTransactionsCounter == -1) || \
					  (indicationWritingTransactionCounter == -1))
			{
				#if !REV_COMPILE_RELEASE
				if (tariffMapTransactionCounter < 0 )
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: general map");
				}
				if (tariffHolidaysTransactionCounter < 0)
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: holidays list");
				}
				if (tariffMovedDaysTransactionsCounter < 0)
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: moved days list");
				}
				if ( indicationWritingTransactionCounter < 0 )
				{
					COMMON_STR_DEBUG(DEBUG_UJUK "Tariff writing ERROR: indications");
				}
				#endif

				if ( counterPtr )
				{
					//counter->tariffMapWritingFlag = FALSE ;
					ACOUNT_StatusSet( counterPtr->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_TARIFF);

					unsigned char evDesc[EVENT_DESC_SIZE];
					unsigned char dbidDesc[EVENT_DESC_SIZE];
					memset(dbidDesc , 0, EVENT_DESC_SIZE);
					COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
					COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_GEN, evDesc);
					STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc);
				}
			}

		}

	}


	//
	//checking power control status
	//

	if ( res == OK )
	{
		if ( counterTask->powerControlTransactionCounter > 0 )
		{
			//counter_t * counter = NULL;
			//if ( STORAGE_FoundCounter( counterTask->counterDbId , &counter ) == OK )
			//{
				if ( counterPtr )
				{
					if ( powerControlTransactionCounter == -1 )
					{
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_GEN, evDesc);
						STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						ACOUNT_StatusSet(counterPtr->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_POWR_CTRL);
					}
					if ( powerControlTransactionCounter == -2 )
					{
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_PASS, evDesc);
						STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						ACOUNT_StatusSet(counterPtr->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD , ACOUNTS_PART_POWR_CTRL);
					}
					else if ( powerControlTransactionCounter == counterTask->powerControlTransactionCounter )
					{
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counterPtr->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_OK, evDesc);
						STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						ACOUNT_StatusSet(counterPtr->dbId, ACOUNTS_STATUS_WRITTEN_OK , ACOUNTS_PART_POWR_CTRL);
					}
				}
			//}
		}
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_SaveTaskResults() %s", getErrorCode(res));

	return res;
}

//
//
//

void UJUK_GetTransformationRatioByType( counter_t * counter , int * Kc , int * Ci )
{
	unsigned char nominalAmperage = (counter->transformationRatios & 0x03);
	unsigned char nominalVoltage = (counter->transformationRatios & 0x04) >> 2 ;
	unsigned char connectionType = (counter->transformationRatios & 0x08) >> 3 ;

	switch( counter->type )
	{
		case TYPE_UJUK_PSH_4TM_05MK:
			if ( connectionType == UJUK_PQP_TRANSF_CONN )
				goto transConnPsh;
			else
				goto directConnPsh;


		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MB:
			transConnPsh:
			*Ci = 1L ;
			*Kc = 1L ;

			if (( nominalVoltage == UJUK_PQP_V_230 ) && ( nominalAmperage == UJUK_PQP_RATIO_AM_5 ))
				*Kc = 2L ;
			return;

		case TYPE_UJUK_SET_4TM_03A:
			*Ci = 1L ;
			*Kc = 1L ;

			if (( nominalVoltage == UJUK_PQP_V_230 ) && ( nominalAmperage == UJUK_PQP_RATIO_AM_1 ))
				*Kc = 2L ;
			return ;

		case TYPE_UJUK_SEB_1TM_01A:
			*Kc = 10L ;
			*Ci = 10L ;
			return ;

		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_PSH_3TM_05D:
			directConnPsh:
			*Kc = 20L ;
			*Ci = 1L ;
			return ;

		case TYPE_UJUK_SEB_1TM_02A:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
			*Kc = 10L ;
			*Ci = 10L ;
			return ;

		default:
			*Kc = 1L ;
			*Ci = 1L ;
			return ;
	}
	return ;
}

//
//
//

int UJUK_UnknownCounterIdentification( unsigned char * counterType , unsigned char * version )
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	unsigned char type = 0 ;
	unsigned char desc = 0 ;

	type = ( version[2] & 0xF0 ) >> 4 ;
	desc = (version[2] & 0x0F ) ;

	switch (type)
	{
		case 0x00:
		{
			if (desc == 0x00)
			{
				(*counterType) = TYPE_UJUK_SET_4TM_02A ;
				res = OK ;
			}
			else
			{
				// SET-1M.01M
				// do nothing
			}
		}
		break;

		case 0x01:
		{
			(*counterType) = TYPE_UJUK_SET_4TM_03A ;
			res = OK ;
		}
		break;

		case 0x02:
		{
			if ((desc == 0x00) || (desc == 0x01) || (desc == 0x02) || (desc == 0x00))
			{
				res = OK ;
				(*counterType) = TYPE_UJUK_SEB_1TM_01A ;
			}
			else if ((desc == 0x08) || (desc == 0x09) || (desc == 0x0A) || (desc == 0x0B))
			{
				res = OK ;
				(*counterType) = TYPE_UJUK_SEB_1TM_02A ;
			}

		}
		break;

		case 0x03:
		{
			(*counterType) = TYPE_UJUK_PSH_4TM_05A ;
			res = OK ;
		}
		break;


		case 0x04:
		{
			(*counterType) = TYPE_UJUK_SEO_1_16 ;
			res = OK ;
		}
		break;

		case 0x05:
		{
			(*counterType) = TYPE_UJUK_PSH_3TM_05A ;
			res = OK;
		}
		break;

		case 0x06:
		{
			(*counterType) = TYPE_UJUK_PSH_4TM_05MB ;
			res = OK ;
		}
		break;

		case 0x07:
		{
			(*counterType) = TYPE_UJUK_PSH_3TM_05MB ;
			res = OK;
		}
		break;

		case 0x08:
		{
			(*counterType) = TYPE_UJUK_SET_4TM_02MB ;
			res = OK ;
		}
		break;

		case 0x09:
		{
			(*counterType) = TYPE_UJUK_SEB_1TM_02D ;
			res = OK ;
		}
		break;

		case 0x0A:
		{
			if (desc & 0x04)
			{
				res = OK ;
				(*counterType) = TYPE_UJUK_PSH_3TM_05D ;
			}
			else
			{
				res = OK ;
				(*counterType) = TYPE_UJUK_PSH_4TM_05D ;
			}
		}
		break;

		case 0x0B:
		{
			(*counterType) = TYPE_UJUK_PSH_4TM_05MK ;
			res = OK ;
		}
		break;

		case 0x0C:
		{
			(*counterType) = TYPE_UJUK_SEB_1TM_02M ;
			res = OK ;
		}
		break;

		case 0x0D:
		{
			(*counterType) = TYPE_UJUK_PSH_4TM_05D; // TYPE_UJUK_PSH_4TM_05 M D
			res = OK ;
		}
		break;

		case 0x0F:
		{
			(*counterType) = TYPE_UJUK_PSH_4TM_05MK ; // TYPE_UJUK_PSH_4TM_05 M N
			res = OK ;
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

BOOL UJUK_IsCounterAblePqpJournal(counter_t * counter)
{
	if (strlen((char *)counter->serialRemote))
		return FALSE;

	switch(counter->type)
	{
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_PSH_4TM_05MD:
			return TRUE;

		default:
			return FALSE ;
	}
}

//
//
//

void UJUK_StepToNextJournal(counter_t * counter)
{
	DEB_TRACE(DEB_IDX_UJUK)


	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK current journal number [%02X]", counter->journalNumber);
	// find journal number
	switch (counter->journalNumber)
	{

		case 0x04:
		{
			if (counter->type == TYPE_UJUK_PSH_4TM_05MK)
			{
				counter->journalNumber = 0x48 ;
				counter->eventsTotal = 49 ;
				break;
			}
			else if ((counter->type == TYPE_UJUK_SEB_1TM_02D) || \
					 (counter->type == TYPE_UJUK_SEB_1TM_02A) || \
					 (counter->type == TYPE_UJUK_SEB_1TM_02M) || \
					 (counter->type == TYPE_UJUK_SEB_1TM_03A))
			{
				counter->journalNumber = 0x48 ;
				counter->eventsTotal = 19 ;
				break;
			}
		}
		default:
		case 0x48:
		{
			if (counter->type == TYPE_UJUK_PSH_4TM_05MK)
			{
				counter->eventsTotal = 9 ;
				counter->journalNumber = 0x53 ;
				break;
			}
		}
		case 0x53:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x0A ;

			if ((counter->type == TYPE_UJUK_SEB_1TM_03A) || \
				(counter->type == TYPE_UJUK_PSH_4TM_05MD) || \
				(counter->type == TYPE_UJUK_PSH_4TM_05MK) )
			{
				counter->journalNumber = 0x56 ;
			}
		}
		break;

		case 0x56:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x0A ;
		}
		break;

		case 0x0A:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x01 ;
		}
		break;

		case 0x01:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x02 ;
		}
		break;

		case 0x02:
		{
			if (!UJUK_IsCounterAblePqpJournal(counter))
			{
				counter->eventsTotal = 9 ;
				counter->journalNumber = 0x04 ;
			}
			else
			{
				counter->eventsTotal = 9 ;
				counter->journalNumber = 0x15 ;
			}
		}
		break;

		//PQP Journals
		case 0x15:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x16;
		}
			break;

		case 0x16:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x17;
		}
			break;

		case 0x17:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x18;
		}
			break;

		case 0x18:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x19;
		}
			break;

		case 0x19:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x1A;
		}
			break;

		case 0x1A:
		{
			counter->eventsTotal = 19 ;
			counter->journalNumber = 0x0B;
		}
			break;

		case 0x0B:
		{
			counter->eventsTotal = 19 ;
			counter->journalNumber = 0x0C;
		}
			break;


		case 0x0C:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x13;
		}
			break;

		case 0x13:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x14;
		}
			break;


		case 0x14:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x35;
		}
			break;

		case 0x35:
		{
			counter->eventsTotal = 19 ;
			counter->journalNumber = 0x36;
		}
			break;

		case 0x36:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x33;
		}
			break;

		case 0x33:
		{
			counter->eventsTotal = 19 ;
			counter->journalNumber = 0x34;
		}
			break;

		case 0x34:
		{
			counter->eventsTotal = 9 ;
			counter->journalNumber = 0x04 ;
		}
			break;
	} //switch (counter->journalNumber)

	counter->eventNumber = counter->eventsTotal ;

	COMMON_STR_DEBUG(DEBUG_UJUK "UJUK new journal number [%02X]", counter->journalNumber);
}

//
//
//

void UJUK_StepToNextEvent(counter_t * counter)
{
	DEB_TRACE(DEB_IDX_UJUK)

	if (counter)
	{
		if (counter->eventNumber == 0)
		{
			UJUK_StepToNextJournal(counter);
		}
		else
		{
			counter->eventNumber -= 1 ;
		}
	}
}


//
//
//

int UJUK_GetProfileMaxPtr(counter_t * counter)
{
	int maxEntriesNumber;
	if ( COMMON_CheckProfileIntervalForValid( counter->profileInterval ) != OK )
	{
		return 0 ;
	}

	switch( counter->type )
	{
		case TYPE_UJUK_SET_4TM_01A:
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
			maxEntriesNumber = 2730 ;
			break;

		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_SEB_1TM_02A:
			maxEntriesNumber = 1365 ;
			break;

		default:
			return 0;
	}

	return (maxEntriesNumber * (8 * ((60 / counter->profileInterval) + 1))) ;
}

//
//
//

int UJUK_GetMaxProfileEntriesNumber(counter_t * counter)
{
	int res = ERROR_GENERAL ;

	if ( counter == NULL )
		return res ;

	switch ( counter->type )
	{
		case TYPE_UJUK_SET_4TM_01A:
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_SET_4TM_03MB:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_03A:
		{
			res = 8192 ;
		}
		break;

		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_SEB_1TM_02A:
		{
			res = 4096 ;
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

unsigned int UJUK_GetNextProfileHeadPtr( counter_t * counter , unsigned int lastProfilePtr )
{
	unsigned int res = 0 ;

	int maxEntriesNumber = UJUK_GetMaxProfileEntriesNumber( counter ) ;

	if( maxEntriesNumber <= 0 )
		return res;

	int profileInterval = counter->profileInterval ;
	int storageMaxDeep = maxEntriesNumber / ( ( 60 / profileInterval ) + 1 ) ;

	unsigned int firstImpossiblePtr = storageMaxDeep * ( 8 * ( ( 60 / profileInterval ) + 1 ) ) ;

	res = lastProfilePtr + ( 8 * ( ( 60 / profileInterval ) + 1 ) ) ;

	if( res >= firstImpossiblePtr )
	{
		res = 0 ;
	}

	return res ;
}

//
//
//

int UJUK_GetReleReason( unsigned char reason, unsigned char * evDesc)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	if (reason && evDesc)
	{
		switch(reason)
		{
			case 0x06:
			{
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_ON_BY_BUTTON_PRESSED , EVENT_DESC_SIZE);
				res = OK ;
			}
			break;

			case 0x30:
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x39:
			case 0x3A:
			case 0x3B:
			case 0x3C:
			case 0x3D:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x4B:
			case 0x4C:
			case 0x4D:
			case 0x4E:
			case 0x4F:
			{
				COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Eday, evDesc);
				res = OK ;
			}
			break;

			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			case 0x58:
			case 0x5D:
			case 0x5E:
			case 0x5F:
			case 0x60:
			case 0x61:
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x69:
			case 0x6A:
			case 0x6F:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			{
				COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Emnth, evDesc);
				res = OK ;
			}
			break;

			case 0x1B:
			case 0x1D:
			case 0x1F:
			case 0x0B:
			{
				COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_PQ, evDesc);
				res = OK ;
			}
			break;

			case 0x21:
			case 0x24:
			case 0x0F:
			{
				COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umax, evDesc);
				res = OK ;
			}
			break;

			case 0x22:
			case 0x25:
			case 0x10:
			{
				COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umin, evDesc);
				res = OK ;
			}
			break;

			case 0x01:
			{
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_UI , EVENT_DESC_SIZE);
				res = OK ;
			}
			break;

			case 0x02:
			case 0x03:
			case 0x04:
			{
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_PRE_PAY_SYS_RULES , EVENT_DESC_SIZE);
				res = OK ;
			}
			break;

			case 0x05:
			{
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_TEMPERATURE , EVENT_DESC_SIZE);
				res = OK ;
			}
			break;

			case 0x0D:
			{
				//scedule. there is no light at 11 o'clock every day. Like in a jail, dude. Ha-ha!
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_SCEDULE , EVENT_DESC_SIZE);
				res = OK ;
			}
			break;

			case 0x12:
			{
				//day energy limit. Make couple cups of tee by electric boiler and stay without light all day))
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_ENERGY_DAY_LIMIT , EVENT_DESC_SIZE);
				res = OK ;
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

int UJUK_GetHolidayArray(ujukHolidayArr_t * ujukHolidayArr , unsigned char * tariff , unsigned int tarifLength)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	if ((tariff != NULL) && (ujukHolidayArr!= NULL) && (tarifLength != 0 ))
	{
		memset(ujukHolidayArr , 0 , sizeof(ujukHolidayArr_t));
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(unsigned char *));
		if (token)
		{
			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;
			unsigned char * tariff_ = calloc(tarifLength + 1 , sizeof(unsigned char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r((char *)tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for Holidays array

				int holidayPos = -1 ;

				int tokenIndex = 0 ;
				for( ; tokenIndex < tokenNumb ; tokenIndex++ )
				{
					if ( strstr(token[tokenIndex] , "[Holidays]") )
					{
						holidayPos = tokenIndex ;
						break;
					}
				}

				if (holidayPos > 0)
				{
					int holidayArrPos = 0 ;
					for( tokenIndex = (holidayPos + 2) ; tokenIndex < ((holidayPos + 2) + (UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE / 4) + 1) ; tokenIndex++)
					{
						if (tokenIndex == tokenNumb)
						{
							res = ERROR_GENERAL ;
							COMMON_STR_DEBUG(DEBUG_UJUK "UJUK_GetHolidayArray() : ERROR GENERAL: tokenIndex == tokenNumb", res);
							break;
						}
						else
						{
							unsigned int bufer = 0 ;
							//bufer = atoi( token[tokenIndex] );
							sscanf( token[tokenIndex] , "%u" , &bufer);
							//printf("buffer = [%s] , atoi = [%u]\r\n",token[tokenIndex] , bufer);
							if (holidayArrPos < UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE)
							{
								ujukHolidayArr->data[holidayArrPos++] = (bufer & 0xFF000000) >> 24;
								ujukHolidayArr->data[holidayArrPos++] = (bufer & 0xFF0000) >> 16;
								ujukHolidayArr->data[holidayArrPos++] = (bufer & 0xFF00) >> 8;
								ujukHolidayArr->data[holidayArrPos++] = (bufer & 0xFF);
							}
							else
							{
								ujukHolidayArr->crc = bufer & 0xFF ;
								res = OK ;
								break;
							}

						}
					}
				}
				free(tariff_) ;
			}
			else
				res = ERROR_MEM_ERROR ;

			free(token);
		}


	}

	return res ;

}

//
//
//

int UJUK_GetMovedDaysList(ujukMovedDaysList_t * ujukMovedDaysList , unsigned char * tariff , int tarifLength)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	if ((tariff != NULL) && (ujukMovedDaysList!= NULL) && (tarifLength != 0 ))
	{
		memset(ujukMovedDaysList , 0 ,sizeof(ujukMovedDaysList_t));
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(unsigned char *));
		if (token)
		{

			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;
			unsigned char * tariff_ = calloc(tarifLength + 1 , sizeof(unsigned char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);



				pch = strtok_r((char *)tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}



				//searching for Moved Days section
				int movedDaysSectionPos = -1 ;

				int tokenIndex = 0 ;
				for( ; tokenIndex < tokenNumb ; tokenIndex++)
				{
					if ( strstr( token[tokenIndex] , "[MovedDays]" ) )
					{
						movedDaysSectionPos = tokenIndex ;
						break;
					}
				}

				if (movedDaysSectionPos > 0)
				{

					//ujukMovedDaysList->data = 0 ;
					tokenIndex = movedDaysSectionPos + 2;
					unsigned int buffer = 0 ;

					do
					{
						sscanf(token[tokenIndex] , "%u" , &buffer);

						if( buffer == 0 )
						{
							ujukMovedDaysList->dataSize++;
							if(tokenIndex < (tokenNumb - 1))
							{
								ujukMovedDaysList->crc = atoi(token[ tokenIndex + 1]);
								//printf("crc = [%d]\r\n" , ujukMovedDaysList->crc);
								res = OK ;
							}
							break;
						}
						else
						{
							ujukMovedDaysList->data[ujukMovedDaysList->dataSize++] = buffer ;
							//printf("buffer = [%08X] , size = [%d]\r\n" , ujukMovedDaysList->data[ujukMovedDaysList->dataSize - 1] , ujukMovedDaysList->dataSize);
						}

						++tokenIndex;
					}
					while( (tokenIndex < tokenNumb) && (strstr(token[tokenIndex] , "[") == NULL) );


				}


				free(tariff_) ;
			}
			else
				res = ERROR_MEM_ERROR ;

			free(token);
		}


	}

	return res ;
}

//
//
//

int UJUK_GetTariffMap(ujukTariffMap_t * ujukTariffMap , unsigned char * tariff , int tarifLength , int * tariffNumber)
{
	DEB_TRACE(DEB_IDX_UJUK)

	int res = ERROR_GENERAL ;

	if ((tariff != NULL) && (ujukTariffMap!= NULL) && (tarifLength != 0 ) && (tariffNumber))
	{
		memset(ujukTariffMap , 0 , sizeof(ujukHolidayArr_t));

		printf("!!!! sizeof(ujukHolidayArr_t) = [%d]\r\n" , sizeof(ujukHolidayArr_t));

		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(unsigned char *));
		if (token)
		{
			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;
			unsigned char * tariff_ = calloc(tarifLength + 1 , sizeof(unsigned char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r((char *)tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for tariff number in current map

				int trNumbPos = - 1;
				int tokenIndex = 0 ;

				for ( tokenIndex = 0 ; tokenIndex < tokenNumb ; ++tokenIndex )
				 {
					if ( strcmp( token[tokenIndex] , "TarifCount" ) == 0 )
					{
						trNumbPos = tokenIndex + 1;
						break;
					}

				}

				if ( ( trNumbPos > 0 ) && (trNumbPos < ( tokenNumb - 1 )) )
				{
					int trNumb = atoi( token[ trNumbPos ] );
					if ( (trNumb == 4)  || ( trNumb == 8 ))
					{
						(*tariffNumber) = trNumb ;
						//searching for year crc
						int yearCrcPos = -1 ;

						for( tokenIndex = 0 ; tokenIndex < tokenNumb ; ++tokenIndex )
						{
							if ( strcmp(token[tokenIndex], "[YearCRC]" ) == 0)
							{
								yearCrcPos = tokenIndex ;
								break;
							}
						}

						if ( (yearCrcPos > 0) && ( (yearCrcPos + 2) < tokenNumb ))
						{
							sscanf( token[yearCrcPos + 2], "%hhu" , &ujukTariffMap->yearCrc );

							//printf("YearCrc = [%hhu]\r\n" , ujukTariffMap->yearCrc );

							//searching for Tariff array

							int monthIndex = 0 ;
							for( ; monthIndex < 12 ; monthIndex++)
							{
								char sectionName[8];
								memset(sectionName , 0 , 8);
								sprintf(sectionName , "[M%d]" , (monthIndex + 1) );

								int monthPos = -1 ;

								tokenIndex = 0 ;

								for( ; tokenIndex < tokenNumb; tokenIndex++ )
								{
									if( strstr(token[tokenIndex] , sectionName) )
									{
										monthPos = tokenIndex ;
										break;
									}
								}

								if ( monthPos > 0 )
								{
									ujukTariffMap->monthMap[monthIndex].dayNumb = 1 ;
									ujukTariffMap->monthMap[monthIndex].day[ujukTariffMap->monthMap[monthIndex].dayNumb - 1].dataSize = 1;
									tokenIndex = monthPos + 2;
									//unsigned int buffer = 0;

									int dataIndex = 0 ;
									int dayIndex = 0 ;
									while(1)
									{
										if ( strstr(token[tokenIndex] , "[") )
										{
											ujukTariffMap->monthMap[monthIndex].dayNumb = (++dayIndex) ;

											////
											if ( ujukTariffMap->monthMap[monthIndex].dayNumb < trNumb )
											{
												int i = ujukTariffMap->monthMap[monthIndex].dayNumb ;
												for( ; i < trNumb ; ++i)
												{
													ujukTariffMap->monthMap[monthIndex].day[i].dataSize = ujukTariffMap->monthMap[monthIndex].day[0].dataSize ;

													memset(&ujukTariffMap->monthMap[monthIndex].day[i].data , 0 , ujukTariffMap->monthMap[monthIndex].day[0].dataSize) ;
												}
												ujukTariffMap->monthMap[monthIndex].dayNumb = trNumb;
											}
											////

											break;
										}

										if ( strstr(token[tokenIndex] , "Data") )
										{
											dataIndex = 0 ;
											//printf("month = [%d] , [%s]\r\n" , monthIndex + 1 , token[tokenIndex]);
											tokenIndex++ ;
											dayIndex++;
											if (dayIndex == UJUK_TARIFF_DAY_TYPE_NUMB)
											{
												ujukTariffMap->monthMap[monthIndex].dayNumb = (++dayIndex) ;

												////

												if ( ujukTariffMap->monthMap[monthIndex].dayNumb < trNumb )
												{
													int i = ujukTariffMap->monthMap[monthIndex].dayNumb ;
													for( ; i < trNumb ; ++i)
													{
														ujukTariffMap->monthMap[monthIndex].day[i].dataSize = ujukTariffMap->monthMap[monthIndex].day[0].dataSize ;
														memset(&ujukTariffMap->monthMap[monthIndex].day[i].data , 0 , ujukTariffMap->monthMap[monthIndex].day[0].dataSize) ;
													}
													ujukTariffMap->monthMap[monthIndex].dayNumb = trNumb;
												}

												////

												break;
											}
										}


										sscanf(token[tokenIndex],"%hhu" , &ujukTariffMap->monthMap[monthIndex].day[dayIndex].data[dataIndex++] );

										ujukTariffMap->monthMap[monthIndex].day[dayIndex].dataSize = dataIndex ;
										//ujukTariffMap->monthMap[monthIndex].dayNumb = dayIndex ;

										res = OK ;

										tokenIndex++ ;
									}


								}
								else
								{
									res = ERROR_GENERAL ;
									break;
								}


							} //for


						} // if


					}


				}

				free(tariff_) ;
			}
			else
				res = ERROR_MEM_ERROR ;

			free(token);
		}


	}

	return res ;
}

//EOF

