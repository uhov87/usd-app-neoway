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
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <math.h>
#include <linux/rtc.h>
#include <errno.h>
#include <ctype.h>

#include "debug.h"
#include "plc.h"
#include "common.h"
#include "storage.h"
#include "pool.h"
#include "cli.h"
#include "ujuk.h"
#include "gubanov.h"

char * out = NULL;

//exported variables
objNameUndPlace_t objNameUndPlace;

#if !REV_COMPILE_RELEASE
static pthread_mutex_t debugMux = PTHREAD_MUTEX_INITIALIZER;
#endif
//
//
//

BOOL IS_DTS_ZERO(dateTimeStamp * dts){
	DEB_TRACE(DEB_IDX_COMMON)

	if (dts != NULL){
		if ((dts->d.d == 0) && (dts->d.m == 0) && (dts->d.y == 0) &&
			(dts->t.h == 0) && (dts->t.m == 0) && (dts->t.s == 0))
			return TRUE;
	}
	return FALSE;
}

//
//
//

BOOL IS_DTS_VALID(dateTimeStamp * dts){
	DEB_TRACE(DEB_IDX_COMMON)


	if (IS_DTS_ZERO(dts) == FALSE){
		if (dts != NULL){
			if (dts->t.s<=60){
				if (COMMON_CheckDtsForCorrect (dts->d.d, dts->d.m, dts->d.y+2000, dts->t.h, dts->t.m) == OK){
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

//
//
//

unsigned char maxDays(unsigned char month, unsigned char year) {
	DEB_TRACE(DEB_IDX_COMMON)

	switch (month) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return (unsigned char) 31;
			break;

		case 2:
			if ((year % 4) == 0){
				return (unsigned char) 29;
			} else {
				return (unsigned char) 28;
			}
			break;

		case 4:
		case 6:
		case 9:
		case 11:
			return (unsigned char) 30;
			break;
	}

	return 0;
}

//
//
//

int COMMON_Initialize (){
	out = (char *)malloc(DEBUG_BUFFER_SIZE);
	if (out == NULL){
		return ERROR_MEM_ERROR;
	}

	//save power on event
	if ( STORAGE_CheckUspdStartFlag() == TRUE )
		STORAGE_JournalUspdEvent(EVENT_USPD_POWER_ON , NULL);

	//save application start event
	STORAGE_JournalUspdEvent( EVENT_APP_POWER_ON , NULL );

	return OK;
}

//
//
//

int COMMON_CheckDtsForCorrect(int dd_int, int mm_int, int yyyy_int, int hh_int, int min_int)
{
	DEB_TRACE(DEB_IDX_COMMON)

	// This function check date and time. If them are correct it returns OK.
	// Else if date or time is incorrect (for example, 31 February or 61 minute) it returns ERROR_GENERAL

	int res = ERROR_GENERAL;

	if ((dd_int > 0) && (dd_int <= 31) && (mm_int > 0) && (mm_int <= 12)&& (yyyy_int > 1970) && (yyyy_int < 2070) && (hh_int >= 0) && (hh_int <=60) && (min_int >= 0) && (min_int <=60))
	{
		unsigned char yy = (yyyy_int - 1900) % 100 ;
		unsigned char mm = mm_int ;
		unsigned char dd = dd_int ;

		if ( dd <= maxDays(mm , yy ) ){
			res = OK ;
		}
	}

	return res;
}

//
//
//

int COMMON_CheckDtsForValid(dateTimeStamp * dts)
{
	DEB_TRACE(DEB_IDX_COMMON)

	/** This function check date and time. If them are correct it returns OK.
	  * Else if date or time is incorrect (for example, 31 February or 61 minute) it returns ERROR_GENERAL
	  */

	int res = ERROR_GENERAL ;

	dateTimeStamp zeroDts;
	memset(&zeroDts , 0 , sizeof(dateTimeStamp));

	if ( memcmp(dts , &zeroDts , sizeof(dateTimeStamp)) != 0 )
	{
		if ((dts->t.s < 60) && \
			(dts->t.m < 60) && \
			(dts->t.h < 24) && \
			(dts->d.m > 0) && (dts->d.m <=12 ) && \
			(dts->d.y <=99) )
		{
			if ( (dts->d.d > 0) && (dts->d.d <=maxDays(dts->d.m,dts->d.y) ) )
			{
				res = OK ;
			}
		}
	}

	return res ;

}

//
//
//

int COMMON_CheckProfileIntervalForValid( unsigned char profileInterval )
{
	int res = ERROR_GENERAL ;

	if ( (profileInterval != 0) && ( profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL ) )
	{
		if ( (60 % profileInterval) == 0 )
		{
			res = OK ;
		}
	}

	return res ;
}

//
//
//

void fixLastMonthDay(dateTimeStamp * dts) {
	DEB_TRACE(DEB_IDX_COMMON)

	if (dts->d.d > maxDays(dts->d.m, dts->d.y)) {
		dts->d.d = maxDays(dts->d.m, dts->d.y);
	}
}

//
//
//

unsigned char COMMON_HexToDec(unsigned char value) {
	DEB_TRACE(DEB_IDX_COMMON)

	unsigned char result = ZERO;

	result = (unsigned char) ((value / 16) * 10) + value % 16;

	return result;
}

//
//
//

unsigned int COMMON_BufToInt(unsigned char * value) {
	DEB_TRACE(DEB_IDX_COMMON)

	unsigned int result = (unsigned int) 0;

	result = result + (unsigned int) (value[0] << 24);
	result = result + (unsigned int) (value[1] << 16);
	result = result + (unsigned int) (value[2] << 8);
	result = result + (unsigned int) (value[3]);

	return result;
}

//
//
//

int COMMON_AsciiToInt( unsigned char * buf , int bufLength , int base)
{
	unsigned char intermediateBuf[16];
	memset( intermediateBuf , 0 , 16 );

	memcpy( intermediateBuf , buf , bufLength ) ;

	return (int)strtoul((char *)intermediateBuf, NULL, base );
}

//
//
//

void COMMON_DecToAscii( int value , int size , unsigned char * buf )
{
	const int intermediateBufLength = 8 ;
	unsigned char intermediateBuf[ intermediateBufLength ];
	memset( intermediateBuf , 0 , intermediateBufLength );

	int memberSum = 0 ;
	int ratio = 10 ;
	int idx = intermediateBufLength - 1;
	for ( idx = intermediateBufLength - 1; idx >= 0 ; --idx )
	{
		int tmp = (value % ratio) - memberSum ;
		memberSum += tmp ;
		tmp /= (ratio / 10) ;
		ratio *= 10 ;

		intermediateBuf[ idx ] = (unsigned char)(tmp + 0x30) ;
	}

	if ( size > intermediateBufLength )
		size = intermediateBufLength ;

	memcpy( buf , &intermediateBuf[ intermediateBufLength - size ] , size );
}


//
//
//

unsigned int COMMON_BufToShort(unsigned char * value) {
	DEB_TRACE(DEB_IDX_COMMON)

	unsigned int result = (unsigned int) 0;

	result = result + (unsigned int) (value[0] << 8);
	result = result + (unsigned int) (value[1]);

	return result;
}

//
//
//

int COMMON_GetCurrentDts(dateTimeStamp * dts) {
	DEB_TRACE(DEB_IDX_COMMON)

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	dts->t.s = tm.tm_sec;
	dts->t.m = tm.tm_min;
	dts->t.h = tm.tm_hour;
	dts->d.d = tm.tm_mday;
	dts->d.m = tm.tm_mon + 1;
	dts->d.y = (tm.tm_year + 1900) % 2000;

	return OK;
}

//
//
//

int COMMON_GetDtsByTime_t(dateTimeStamp * dts , time_t t) {
	DEB_TRACE(DEB_IDX_COMMON)

	struct tm tm = *localtime(&t);

	dts->t.s = tm.tm_sec;
	dts->t.m = tm.tm_min;
	dts->t.h = tm.tm_hour;
	dts->d.d = tm.tm_mday;
	dts->d.m = tm.tm_mon + 1;
	dts->d.y = (tm.tm_year + 1900) % 2000;

	return OK;
}
/*
//
//
//

time_t COMMON_GetTime_tByDts(dateTimeStamp * dts)
{
	time_t t = 0 ;

	if (dts != NULL)
	{
		struct tm time_tm;
		memset(&time_tm , 0 , sizeof(struct tm));

		time_tm.tm_hour = (int)(dts->t.h);
		time_tm.tm_min = (int)(dts->t.m) ;
		time_tm.tm_sec = (int)(dts->t.s) ;

		time_tm.tm_mday = (int)(dts->d.d) ;
		time_tm.tm_mon = (int)((dts->d.m) - 1) ;
		time_tm.tm_yday = (int)(dts->d.y) + 100 ;


		t = mktime(&time_tm);
	}
	return t;
}
*/
//
//
//

int COMMON_GetLastHalfHourDts(dateTimeStamp * dts, int mode)
{
	DEB_TRACE(DEB_IDX_COMMON)

	if( mode == CURRENT_DTS )
		COMMON_GetCurrentDts(dts);
	else if ( mode == DEFINED_DTS && dts == NULL )
		return ERROR_GENERAL;
	dts->t.s = 0;
	if(dts->t.m > 30)
	{
		dts->t.m = 30;
	}
	else if(dts->t.m > 0 && dts->t.m < 30)
	{
		dts->t.m = 0;
	}
	return OK;

}


//
//
//

int COMMON_GetDtsFromUIString(char * uiString , dateTimeStamp * dts)
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() start");

	if ( (uiString != NULL) && (dts != NULL) )
	{
		COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() uiString and dts are not equal NULL");
		if ( strlen(uiString) >= 14 )
		{
			COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() strlen of uiString is higher or equal of 14");
			memset(dts, 0 , sizeof(dateTimeStamp));

			int parsingState = 1 ;

			int hhRes = -1 , minRes = -1 , secRes = -1 , ddRes= -1 , monRes = -1 , yyyyRes = -1 ;

			while(parsingState != 0)
			{
				switch(parsingState)
				{
					case 1:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing hour");
						if ( (uiString[0] >= 0x30) && (uiString[0] <= 0x39) && (uiString[1] >= 0x30) && (uiString[1] <= 0x39))
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing hour OK");
							dts->t.h = ( ( (uiString[0]) - 0x30 ) * 10 ) + (uiString[1] - 0x30) ;
							hhRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 2:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing min");
						if ( (uiString[2] >= 0x30) && (uiString[2] <= 0x39) && (uiString[3] >= 0x30) && (uiString[3] <= 0x39))
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing min OK");
							dts->t.m = ( ( (uiString[2]) - 0x30 ) * 10 ) + (uiString[3] - 0x30) ;
							minRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 3:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing sec");
						if ( (uiString[4] >= 0x30) && (uiString[4] <= 0x39) && (uiString[5] >= 0x30) && (uiString[5] <= 0x39))
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing sec OK");
							dts->t.s = ( ( (uiString[4]) - 0x30 ) * 10 ) + (uiString[5]-0x30) ;
							secRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 4:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing day");
						if ( (uiString[6] >= 0x30) && (uiString[6] <= 0x39) && (uiString[7] >= 0x30) && (uiString[7] <= 0x39))
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing day OK");
							dts->d.d = ( ( (uiString[6]) - 0x30 ) * 10 ) + (uiString[7]-0x30) ;
							ddRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 5:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing month");
						if ( (uiString[8] >= 0x30) && (uiString[8] <= 0x39) && (uiString[9] >= 0x30) && (uiString[9] <= 0x39))
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing month OK");
							dts->d.m = ( ( (uiString[8]) - 0x30 ) * 10 ) + (uiString[9]-0x30) ;
							monRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 6:
					{
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing year");
						if ( (uiString[10] >= 0x30) && (uiString[10] <= 0x39) && \
							 (uiString[11] >= 0x30) && (uiString[11] <= 0x39) && \
							 (uiString[12] >= 0x30) && (uiString[12] <= 0x39) && \
							 (uiString[13] >= 0x30) && (uiString[13] <= 0x39) )
						{
							COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() parsing year OK");
							dts->d.y = ( ( (uiString[12]) - 0x30 ) * 10 ) + (uiString[13]-0x30) ;
							yyyyRes = OK ;
						}
						parsingState++ ;
					}
					break;

					case 7:
						COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() exiting from state machine");
						parsingState = 0;
						if ( (hhRes == OK) && (minRes == OK) && (secRes == OK) && (ddRes == OK) && (monRes == OK) && (yyyyRes == OK) )
							res = OK ;
						break;

					default:
						break;
				}


			}

		}

	}
	COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_GetDtsFromUIString() exit with res = %d" , res);
	return res ;

}

//
//
//

int COMMON_GetCurrentTs(timeStamp * ts) {
	DEB_TRACE(DEB_IDX_COMMON)

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	ts->s = tm.tm_sec;
	ts->m = tm.tm_min;
	ts->h = tm.tm_hour;

	return OK;
}

//
//
//

int COMMON_GetDtsWorth(dateTimeStamp * dts){
	DEB_TRACE(DEB_IDX_COMMON)

	int res = OK;
	int worth = 0;
	dateTimeStamp now;

	res = COMMON_GetCurrentDts(&now);
	if (res == OK){
		worth = COMMON_GetDtsDiff__Alt( &now , dts, BY_SEC);
	}

	return worth;
}

//
//
//

BOOL COMMON_DtsAreEqual(dateTimeStamp * da, dateTimeStamp * db) {
	DEB_TRACE(DEB_IDX_COMMON)

	int result = memcmp(da, db, sizeof (dateTimeStamp));

	if (result == 0)
		return TRUE;

	return FALSE;
}

//
//
//

BOOL COMMON_DtsInRange(dateTimeStamp * da, dateTimeStamp * from, dateTimeStamp * to) {
	DEB_TRACE(DEB_IDX_COMMON);

	if ( ( COMMON_DtsIsLower( from , da ) == TRUE ) && ( COMMON_DtsIsLower( da , to ) == TRUE ))
		return TRUE;

#if 0
	//check year
	if ((from->d.y <= da->d.y) && (da->d.y <= to->d.y)) {		
		//check month
		if ((from->d.m <= da->d.m) && (da->d.m <= to->d.m)) {			
			//check day
			if ((from->d.d <= da->d.d) && (da->d.d <= to->d.d)) {				
				//check time
				int timeRes = ((da->t.h * 10000) + (da->t.m * 100) + (da->t.s));
				int timeFrom = ((from->t.h * 10000) + (from->t.m * 100) + (from->t.s));
				int timeTo = ((to->t.h * 10000) + (to->t.m * 100) + (to->t.s));

				if ((timeFrom <= timeRes) && (timeRes <= timeTo))
					return TRUE;
			}
		}
	}
#endif

	return FALSE;
}

//
//
//

BOOL COMMON_DtsIsLower(dateTimeStamp * from, dateTimeStamp * to) {
	DEB_TRACE(DEB_IDX_COMMON)


	//check date
	int dateFrom = ((from->d.y * 10000) + (from->d.m * 100) + (from->d.d));
	int dateTo = ((to->d.y * 10000) + (to->d.m * 100) + (to->d.d));
	if (dateFrom < dateTo)
		return TRUE ;
	else if (dateFrom == dateTo){

		//check time
		int timeFrom = ((from->t.h * 10000) + (from->t.m * 100) + (from->t.s));
		int timeTo = ((to->t.h * 10000) + (to->t.m * 100) + (to->t.s));
		if (timeFrom <= timeTo)
			return TRUE;
	}

	return FALSE;
}

//
//
//

int COMMON_GetDtsDiff(dateTimeStamp * dStart, dateTimeStamp * dStop, int step) {
	DEB_TRACE(DEB_IDX_COMMON)

	dateTimeStamp da; //copy of start date
	dateTimeStamp db; //copy of stop date
	int iterator = 0;
	int sigma = 0;

	//check start lower than stop
	if (COMMON_DtsIsLower(dStart, dStop) == TRUE) {
		//setup local variables
		memcpy(&da, dStart, sizeof (dateTimeStamp));
		memcpy(&db, dStop, sizeof (dateTimeStamp));
		sigma = 1;
	} else {
		//setup local variables
		memcpy(&da, dStop, sizeof (dateTimeStamp));
		memcpy(&db, dStart, sizeof (dateTimeStamp));
		sigma = -1;
	}

	//printf("COMMON_GetDtsDiff: SIGMA=[%d]\n", sigma);

	//do calculation
	switch (step) {
		case BY_HALF_HOUR:
			//clear time part
			da.t.s = 0;
			db.t.s = 0;

			//increment iterator
			while (COMMON_DtsAreEqual(&da, &db) == FALSE) {
				COMMON_ChangeDts(&da, INC, BY_HALF_HOUR);
				iterator++;
			}
			break;

		case BY_DAY:
			//clear time part
			memset(&da.t, 0, sizeof (timeStamp));
			memset(&db.t, 0, sizeof (timeStamp));

			//increment iterator
			while (COMMON_DtsAreEqual(&da, &db) == FALSE) {
				COMMON_ChangeDts(&da, INC, BY_DAY);
				iterator++;
			}
			break;

		case BY_MONTH:
			//clear time part
			memset(&da.t, 0, sizeof (timeStamp));
			memset(&db.t, 0, sizeof (timeStamp));
			da.d.d = 1;
			db.d.d = 1;
			while (COMMON_DtsAreEqual(&da, &db) == FALSE) {
				COMMON_ChangeDts(&da, INC, BY_MONTH);
				iterator++;
			}
			break;

		case BY_SEC:
			//clear time part
			while (COMMON_DtsAreEqual(&da, &db) == FALSE) {
				COMMON_ChangeDts(&da, INC, BY_SEC);
				iterator++;
			}
			break;

		default:
			iterator = 0;
			break;
	}

	return (iterator * sigma);
}

//
//
//

int COMMON_GetDtsDiff__Alt(dateTimeStamp * dStart, dateTimeStamp * dStop, int step)
{
	DEB_TRACE(DEB_IDX_COMMON)

	time_t start_t , stop_t ;
	struct tm start_tm , stop_tm ;
	memset( &start_tm , 0 , sizeof(struct tm) );
	memset( &stop_tm , 0 , sizeof(struct tm) );

	start_tm.tm_sec = dStart->t.s ;
	start_tm.tm_min = dStart->t.m ;
	start_tm.tm_hour = dStart->t.h ;
	start_tm.tm_mday = dStart->d.d ;
	start_tm.tm_mon = dStart->d.m - 1 ;
	start_tm.tm_year = 100 + dStart->d.y ;

	stop_tm.tm_sec = dStop->t.s ;
	stop_tm.tm_min = dStop->t.m ;
	stop_tm.tm_hour = dStop->t.h ;
	stop_tm.tm_mday = dStop->d.d ;
	stop_tm.tm_mon = dStop->d.m - 1 ;
	stop_tm.tm_year = 100 + dStop->d.y ;


	start_t = mktime(&start_tm);
	stop_t = mktime(&stop_tm);

	int diff = 0 ;
	diff = stop_t - start_t;

	switch( step )
	{
		case BY_SEC:
			break;

		case BY_MIN:
			diff /= 60 ;
			break;

		case BY_HALF_HOUR:
			diff /= 1800 ;
			break;

		case BY_HOUR:
			diff /= 3600 ;
			break;

		case BY_DAY:
			diff /= 3600 * 24 ;
			break;

		case BY_MONTH:
			diff = COMMON_GetDtsDiff( dStart , dStop , BY_MONTH  );
			break;

		case BY_YEAR:
			diff = COMMON_GetDtsDiff( dStart , dStop , BY_YEAR  );
			break;

		default:
			diff = 0 ;
			break;
	}

	return diff ;


}

//
//
//

#if 0
int COMMON_GetWeekDayByDts( dateTimeStamp * dts )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = 0 ;
	if (dts)
	{
		struct tm time_tm;
		memset(&time_tm , 0 , sizeof(struct tm));

		time_tm.tm_mday = dts->d.d ;
		time_tm.tm_mon = dts->d.m - 1;
		time_tm.tm_year = dts->d.y + 100 ;

		time_t rawtime = mktime(&time_tm);

		time_tm = *localtime(&rawtime);

		res = time_tm.tm_wday;
		if ( res == 0 )
		{
			res = 7;
		}
	}

	return res;
}
#endif

int COMMON_GetWeekDayByDts( dateTimeStamp * dts )
{
	DEB_TRACE(DEB_IDX_COMMON)

        int day = dts->d.d ;
        int month = dts->d.m ;
        int year = 2000 + dts->d.y ;

        int res =
                ((day
                + ((153 * (month + 12 * ((14 - month) / 12) - 3) + 2) / 5)
                + (365 * (year + 4800 - ((14 - month) / 12)))
                + ((year + 4800 - ((14 - month) / 12)) / 4)
                - ((year + 4800 - ((14 - month) / 12)) / 100)
                + ((year + 4800 - ((14 - month) / 12)) / 400)
                - 32045) % 7) + 1 ;

        return res ;
}
//
//
//

void COMMON_GetSeasonChangingDts( unsigned char year , unsigned char season, dateTimeStamp * springDts , dateTimeStamp * autumnDts )
{
	if ( (!springDts) || (!autumnDts) )
		return ;

	//set last marth day
	springDts->d.y = year ;
	springDts->d.m = 3 ; //marth
	springDts->d.d = 31 ;
	springDts->t.h = 2 ;
	springDts->t.m = 0 ;
	springDts->t.s = 0 ;
	//find last marth sunday
	while( COMMON_GetWeekDayByDts( springDts ) != 7 )
	{
		COMMON_ChangeDts( springDts , DEC , BY_DAY );
	}

	//set last october dts
	autumnDts->d.y = year ;
	autumnDts->d.m = 10 ; //october
	autumnDts->d.d = 31 ;
	autumnDts->t.h = 3 ;
	if ( season == STORAGE_SEASON_WINTER )
		autumnDts->t.h = 2 ;
	autumnDts->t.m = 0 ;
	autumnDts->t.s = 0 ;
	//find last october sunday
	while( COMMON_GetWeekDayByDts( autumnDts ) != 7 )
	{
		COMMON_ChangeDts( autumnDts , DEC , BY_DAY );
	}
	return ;
}

//
//
//

void COMMON_ChangeDts(dateTimeStamp * dts, int operation, int step) {
	DEB_TRACE(DEB_IDX_COMMON)

	switch (operation) {
		case INC:
			switch (step) {

				case BY_SEC:
					if (dts->t.s < 59) {
						dts->t.s++;
					}
					else {
						dts->t.s = 0;
						COMMON_ChangeDts(dts, INC, BY_MIN);
					}
					break;

				case BY_MIN:
					if (dts->t.m < 59) {
						dts->t.m++;
					}
					else {
						dts->t.m = 0;
						COMMON_ChangeDts(dts, INC, BY_HOUR);
					}
					break;

				case BY_HOUR:
					if (dts->t.h < 23) {
						dts->t.h++;
					}
					else {
						dts->t.h = 0;
						COMMON_ChangeDts(dts, INC, BY_DAY);
					}
					break;

				case BY_HALF_HOUR:
					dts->t.m += 30;
					if (dts->t.m > 59) {
						dts->t.m -= 60;
						COMMON_ChangeDts(dts, INC, BY_HOUR);
					}
					break;

				case BY_DAY:
					if (dts->d.d < maxDays(dts->d.m, dts->d.y)) {
						dts->d.d++;
					} else {
						dts->d.d = 1;
						COMMON_ChangeDts(dts, INC, BY_MONTH);
					}
					break;

				case BY_MONTH:
					if (dts->d.m < 12) {
						dts->d.m++;
						fixLastMonthDay(dts);
					} else {
						dts->d.m = 1;
						COMMON_ChangeDts(dts, INC, BY_YEAR);
					}
					break;

				case BY_YEAR:
					dts->d.y++;


					fixLastMonthDay(dts);
					break;
			}
			break;

		case DEC:
			switch (step) {

				case BY_SEC:
					if (dts->t.s > 0) {
						dts->t.s--;
					} else {
						dts->t.s = 59;
						COMMON_ChangeDts(dts, DEC, BY_MIN);
					}
					break;

				case BY_MIN:
					if (dts->t.m > 0) {
						dts->t.m--;
					} else {
						dts->t.m = 59;
						COMMON_ChangeDts(dts, DEC, BY_HOUR);
					}
					break;

				case BY_HALF_HOUR:
					if (dts->t.m > 29) {
						dts->t.m -= 30;
					} else {
						dts->t.m += 30;
						COMMON_ChangeDts(dts, DEC, BY_HOUR);
					}
					break;

				case BY_HOUR:
					if (dts->t.h > 0) {
						dts->t.h--;
					} else {
						dts->t.h = 23;
						COMMON_ChangeDts(dts, DEC, BY_DAY);
					}
					break;

				case BY_DAY:
					if (dts->d.d > 1) {
						dts->d.d--;
					} else {
						dts->d.d = 31;
						COMMON_ChangeDts(dts, DEC, BY_MONTH);
					}
					break;

				case BY_MONTH:
					if (dts->d.m > 1) {
						dts->d.m--;
						fixLastMonthDay(dts);
					} else {
						dts->d.m = 12;
						COMMON_ChangeDts(dts, DEC, BY_YEAR);
					}
					break;

				case BY_YEAR:
					dts->d.y--;

					fixLastMonthDay(dts);
					break;
			}
			break;
	}
}

//
// Sleep function
//

void COMMON_Sleep(long millisecs) {
	DEB_TRACE(DEB_IDX_COMMON)

	//do sleep over milliseconds
	usleep(millisecs * 1000L);
}

//
//
//

// returns TRUE if firstDts is less or equal then secDts and returns FALSE if firstDts is higher then secDts
BOOL COMMON_CheckDts( dateTimeStamp * firstDts , dateTimeStamp * secDts )
{
	DEB_TRACE(DEB_IDX_COMMON)

	BOOL res = FALSE ;
	if ( firstDts->d.y == secDts->d.y )
	{
		if (firstDts->d.m == secDts->d.m)
		{
			if (firstDts->d.d == secDts->d.d)
			{
				if (firstDts->t.h == secDts->t.h)
				{
					if (firstDts->t.m == secDts->t.m)
					{
						if (firstDts->t.s <= secDts->t.s)
							res = TRUE ;
					}
					else if (firstDts->t.m < secDts->t.m)
						res = TRUE;
				}
				else if (firstDts->t.h < secDts->t.h)
					res = TRUE ;
			}
			else if (firstDts->d.d < secDts->d.d)
				res = TRUE ;
		}
		else if (firstDts->d.m < secDts->d.m)
			res = TRUE ;
	}
	else if (firstDts->d.y < secDts->d.y)
		res = TRUE;


	return res ;
}

//
//
//

BOOL alreadyInSyncRoutine = FALSE;
dateTimeStamp startSyncStamp;
dateTimeStamp needdSyncStamp;

int COMMON_SetNewTimeStep1(dateTimeStamp * dtsToSet, int * timeDiff)
{
	int res = ERROR_GENERAL;

	//check we already start sync
	if (alreadyInSyncRoutine == TRUE){
		return res;
	}

	//start sync routine
	alreadyInSyncRoutine = TRUE;

	//get current time as start of sync routine
	COMMON_GetCurrentDts(&startSyncStamp);

	//save timestamp needed to set
	memcpy(&needdSyncStamp, dtsToSet, sizeof(dateTimeStamp));

	//calculate difference betwen usod time for returning value
	(* timeDiff) = COMMON_GetDtsDiff__Alt(&startSyncStamp, dtsToSet, BY_SEC);

	//ok
	res = OK;

	return res;
}

int COMMON_SetNewTimeStep2(unsigned char reason)
{
	int i = 0;
	int res = ERROR_GENERAL;
	dateTimeStamp currentStamp;

	//pause pooler
	POOL_Pause();

	//get current time
	COMMON_GetCurrentDts(&currentStamp);

	//get time spen between recieving dtsToSet and current time
	int syncOffset = COMMON_GetDtsDiff__Alt(&startSyncStamp, &currentStamp, BY_SEC);

	//change recieved dtsToSet up to wait poller pause time
	for (;i<syncOffset;i++){
		COMMON_ChangeDts(&needdSyncStamp, INC, BY_SEC);
	}

	//set correct time
	res = COMMON_SetTime(&needdSyncStamp, &syncOffset, reason);

	//ask drivers to set new time to counters
	STORAGE_SetSyncFlagForCounters(syncOffset);

	//resume poller
	POOL_Continue();

	//change lock flag
	COMMON_SetNewTimeAbort();

	//exit
	return res;
}

void COMMON_SetNewTimeAbort()
{
	//change lock flag
	alreadyInSyncRoutine = FALSE;
}

//
//
//

int COMMON_SetTime(dateTimeStamp * dtsToSet , int * diff, unsigned int reason)
{
	DEB_TRACE(DEB_IDX_COMMON)


	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() start with params" );

	if ( dtsToSet == NULL )
		return res ;

	struct tm tmToSync;
	memset(&tmToSync , 0 , sizeof(struct tm));

	tmToSync.tm_hour = dtsToSet->t.h;
	tmToSync.tm_min = dtsToSet->t.m;
	tmToSync.tm_sec = dtsToSet->t.s;

	tmToSync.tm_year = dtsToSet->d.y + 100;
	tmToSync.tm_mon = dtsToSet->d.m - 1;
	tmToSync.tm_mday = dtsToSet->d.d;

	   COMMON_STR_DEBUG(DEBUG_COMMON "STRUCT CURRENT TIME:\n" \
			  "tm_sec=%d\n"  \
			  "tm_min=%d\n" \
			  "tm_hour=%d\n" \
			  "tm_mday=%d\n" \
			  "tm_mon=%d\n" \
			  "tm_year=%d\n" \
			  "tm_wday=%d\n" \
			  "tm_yday=%d\n" \
			  "tm_isdst=%d" , tmToSync.tm_sec,tmToSync.tm_min,tmToSync.tm_hour,tmToSync.tm_mday,tmToSync.tm_mon, \
								tmToSync.tm_year,tmToSync.tm_wday,tmToSync.tm_yday,tmToSync.tm_isdst);


	time_t NewTime_t = mktime(&tmToSync);

	//COMMON_STR_DEBUG(DEBUG_COMMON "TIME_T = %d\n", NewTime_t);

	time_t currentTime = time(NULL);

	//debug
	#if 0
	   struct tm currTime;
	   memset(&currTime , 0 , sizeof(struct tm));
	   currTime = *localtime(&currentTime);
	   printf("STRUCT CURRENT TIME:\n" \
			  "tm_sec=%d\n"  \
			  "tm_min=%d\n" \
			  "tm_hour=%d\n" \
			  "tm_mday=%d\n" \
			  "tm_mon=%d\n" \
			  "tm_year=%d\n" \
			  "tm_wday=%d\n" \
			  "tm_yday=%d\n" \
			  "tm_isdst=%d\n" , currTime.tm_sec,currTime.tm_min,currTime.tm_hour,currTime.tm_mday,currTime.tm_mon, \
								currTime.tm_year,currTime.tm_wday,currTime.tm_yday,currTime.tm_isdst);

		EXIT_PATTERN;
	#endif


	COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() try to set time by stime()" );

	if ( stime( &NewTime_t ) == 0 )
	{

		struct tm NewTime_tm;
		memset(&NewTime_tm , 0 ,sizeof(struct tm));
		localtime_r(&NewTime_t , &NewTime_tm);


		struct rtc_time NewTime_rtc;
		NewTime_rtc.tm_sec = NewTime_tm.tm_sec;
		NewTime_rtc.tm_min = NewTime_tm.tm_min;
		NewTime_rtc.tm_hour = NewTime_tm.tm_hour;

		NewTime_rtc.tm_mday = NewTime_tm.tm_mday;
		NewTime_rtc.tm_mon = NewTime_tm.tm_mon;
		NewTime_rtc.tm_year = NewTime_tm.tm_year;

		COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() try to open /dev/rtc");

		int fd = open ("/dev/rtc", O_RDWR);
		if (fd < 0)
		{
			return res ;
		}
		else
		{
			COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() try to call ioctl");
			if (ioctl(fd, RTC_SET_TIME, &NewTime_rtc) < 0 )
			{
				return res;
			}
			else
			{
				COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() calculating difference");
				//calculate difference between current time and new time
				if (diff != NULL)
				{
					(*diff) = currentTime - NewTime_t;
					//(*diff) = abs(  (*diff)  );
				}

				res = OK ;

				//add current event timesync to the log
				unsigned char evDesc[EVENT_DESC_SIZE];
				memset(evDesc , 0 , EVENT_DESC_SIZE);

				dateTimeStamp last_dts;
				COMMON_GetDtsByTime_t(&last_dts , currentTime);
				evDesc[0] = last_dts.d.y;
				evDesc[1] = last_dts.d.m;
				evDesc[2] = last_dts.d.d;
				evDesc[3] = last_dts.t.h;
				evDesc[4] = last_dts.t.m;
				evDesc[5] = last_dts.t.s;
				evDesc[6] = (unsigned char)(reason & 0xFF) ;

				STORAGE_JournalUspdEvent( EVENT_TIME_SYNCED , evDesc );
			}
			close(fd);
		}
	}

	COMMON_STR_DEBUG(DEBUG_COMMON "COMMON_SetTime() exit with res = %d" , res );

	return res ;
}

//
//
//

counterTransaction_t ** COMMON_AllocTransaction( counterTask_t ** counterTask )
{
	(*counterTask)->transactions = (counterTransaction_t **) realloc((*counterTask)->transactions, (++(*counterTask)->transactionsCount) * sizeof (counterTransaction_t *));
	if ((*counterTask)->transactions != NULL) {
		(*counterTask)->transactions[(*counterTask)->transactionsCount - 1] = (counterTransaction_t *) malloc(sizeof (counterTransaction_t));
		if (((*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) != NULL)
			return &(*counterTask)->transactions[(*counterTask)->transactionsCount - 1];
		else
			EXIT_PATTERN;
	} else
		EXIT_PATTERN;

}

//
//
//

int COMMON_FreeCounterTask(counterTask_t ** counterTask) {
	DEB_TRACE(DEB_IDX_COMMON)

	int res = OK;

	if ( (*counterTask) )
	{
		if ( (*counterTask)->transactions )
		{
			int transactionIndex = (*counterTask)->transactionsCount - 1;
			for (; transactionIndex >= 0; transactionIndex--)
			{
				if ( (*counterTask)->transactions[transactionIndex] )
				{
					//free answer
					if ((*counterTask)->transactions[transactionIndex]->answerSize != 0)
					{
						if ((*counterTask)->transactions[transactionIndex]->answer )
						{
							free((*counterTask)->transactions[transactionIndex]->answer);
							(*counterTask)->transactions[transactionIndex]->answer = NULL;
						}
						(*counterTask)->transactions[transactionIndex]->answerSize = 0 ;
					}

					//free command
					if ((*counterTask)->transactions[transactionIndex]->commandSize != 0)
					{
						if ((*counterTask)->transactions[transactionIndex]->command )
						{
							free((*counterTask)->transactions[transactionIndex]->command);
							(*counterTask)->transactions[transactionIndex]->command = NULL;
						}
						(*counterTask)->transactions[transactionIndex]->commandSize = 0 ;
					}

					//free transaction
					free((*counterTask)->transactions[transactionIndex]);
					(*counterTask)->transactions[transactionIndex] = NULL;
				}
			}

			free((*counterTask)->transactions);
			(*counterTask)->transactions = NULL;
		}

		free(*counterTask);
		(*counterTask) = NULL;
	}


	return res;
}

//
//
//

int COMMON_FreeMeteragesArray(meteregesArray_t ** mArray) {
	DEB_TRACE(DEB_IDX_COMMON)

	int mIndex = 0;

	if ((*mArray) != NULL){
		if ((*mArray)->meteragesCount > 0){
			mIndex = (*mArray)->meteragesCount - 1;
			for (; mIndex >= 0; mIndex--) {
				if ((*mArray)->meterages[mIndex] != NULL){
					free((*mArray)->meterages[mIndex]);
					(*mArray)->meterages[mIndex] = NULL;
				}
			}

			if ( (*mArray)->meterages ){
				free((*mArray)->meterages);
				(*mArray)->meterages = NULL;
			}

		}

		free(*mArray);
		(*mArray) = NULL;
	}

	return OK;
}

//
//
//

int COMMON_FreeUspdEventsArray(uspdEventsArray_t ** mArray){
	DEB_TRACE(DEB_IDX_COMMON)

	int mIndex = 0;

	if ((*mArray) != NULL){
		if ((*mArray)->eventsCount > 0){
			mIndex = (*mArray)->eventsCount - 1;
			for (; mIndex >= 0; mIndex--) {
				if ((*mArray)->events[mIndex] != NULL){
					free((*mArray)->events[mIndex]);
					(*mArray)->events[mIndex] = NULL;
				}
			}

			if ( (*mArray)->events ){
				free((*mArray)->events);
				(*mArray)->events = NULL;
			}
		}
		free(*mArray);
		(*mArray) = NULL;
	}

	return OK;
}

//
//
//

int COMMON_FreeEnergiesArray(energiesArray_t ** eArray) {
	DEB_TRACE(DEB_IDX_COMMON)

	int eIndex = 0;

	if ((*eArray) != NULL){
		if ((*eArray)->energiesCount > 0){
			eIndex = (*eArray)->energiesCount - 1;
			for (; eIndex >= 0; eIndex--) {
				if ((*eArray)->energies[eIndex] != NULL){
					//free energies
					free((*eArray)->energies[eIndex]);
					(*eArray)->energies[eIndex] = NULL;
				}
			}

			if ( (*eArray)->energies ){
				free((*eArray)->energies);
				(*eArray)->energies = NULL;
			}
		}

		free(*eArray);
		(*eArray) = NULL;
	}

	return OK;
}


//
//
//

char COMMON_GetMayakRatio(int ratio){
	DEB_TRACE(DEB_IDX_COMMON)

	switch (ratio){
		case 1:    return 0xFA;
		case 10:   return 0xFB;
		case 100:  return 0xFC;
		case 1000: return 0xFD;
		default:   return 0xFE;
	}
}

//
//
//
int COMMON_GetRatioValueUjuk(int ratioByte){
	DEB_TRACE(DEB_IDX_COMMON)

	switch (ratioByte) {
		case 0:
			return 5000;
		case 1:
			return 25000;
		case 2:
			return 1250;
		case 3:
			return 6250;
		case 4:
			return 500;
		case 5:
			return 250;
		case 6:
			return 6400;
		default:
			return 1;
	}
}

//
//
//

BOOL COMMON_CheckRatioIndexForValid( unsigned char counterType , unsigned char ratioIndex )
{
	BOOL res = TRUE ;

	if ( ratioIndex == 0xFF )
		return FALSE ;

	switch( counterType )
	{
		case TYPE_UJUK_SEB_1TM_02A:
		case TYPE_UJUK_SEB_1TM_02M:
		case TYPE_UJUK_SEB_1TM_02D:
		case TYPE_UJUK_SEB_1TM_03A:
		{
			if ( ratioIndex != 4 )
			{
				res = FALSE ;
			}
		}
		break;


		case TYPE_UJUK_SEB_1TM_01A:
		case TYPE_UJUK_SET_4TM_01A:
		case TYPE_UJUK_SET_4TM_02A:
		case TYPE_UJUK_SET_4TM_03A:
		case TYPE_UJUK_PSH_3TM_05A:
		case TYPE_UJUK_PSH_4TM_05A:
		case TYPE_UJUK_SEO_1_16:
		case TYPE_UJUK_PSH_4TM_05MB:
		case TYPE_UJUK_PSH_3TM_05MB:
		case TYPE_UJUK_PSH_4TM_05MD:
		case TYPE_UJUK_SET_4TM_02MB:
		case TYPE_UJUK_PSH_3TM_05D:
		case TYPE_UJUK_PSH_4TM_05D:
		case TYPE_UJUK_PSH_4TM_05MK:
		case TYPE_UJUK_SET_1M_01:
		case TYPE_UJUK_SET_4TM_03MB:
		{
			if ( ratioIndex > 6 )
			{
				res = FALSE ;
			}
		}
		break;
	}

	return res;
}

//
//
//
int COMMON_GetRatioValueMayak(int ratio){
	DEB_TRACE(DEB_IDX_COMMON)

	switch (ratio){
		case 0xFA:    return 1;
		case 0xFB:   return 10;
		case 0xFC:  return 100;
		case 0xFD: return 1000;
		default:   return 1;
	}
}

//
//
//
#if !REV_COMPILE_RELEASE

void COMMON_STR_DEBUG(char * str, ...) {
	DEB_TRACE(DEB_IDX_COMMON)

	pthread_mutex_lock(&debugMux);

	va_list fooArgs;

	memset(out, 0, DEBUG_BUFFER_SIZE);

	//glue line
	va_start(fooArgs, str);
	vsprintf(out, str, fooArgs);
	va_end(fooArgs);

	//if (DEBUG_ALL_LEVELS == "N"){
	if (!strcmp( DEBUG_ALL_LEVELS ,"N") ){
		if (out[0] != 'Y'){
			pthread_mutex_unlock(&debugMux);
			return;
		}
	}

#if DEBUG_INCLUDE_TIME_TO_OUTPUT
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {

		dateTimeStamp dts;
		COMMON_GetCurrentDts(&dts);

		printf(DTS_PATTERN ": %s\n", DTS_GET_BY_VAL(dts), &out[1]);
	}
#else
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {
		printf("%s\n", &out[1]);
	}

#endif

	//if (DEBUG_TO_FILE == "Y") {
	if (!strcmp(DEBUG_TO_FILE , "Y")) {

	}

	pthread_mutex_unlock(&debugMux);
}

#endif
//
//
//

#if !REV_COMPILE_RELEASE
void COMMON_BUF_DEBUG(char * prefix, unsigned char * buf, unsigned int size) {
	DEB_TRACE(DEB_IDX_COMMON)

	pthread_mutex_lock(&debugMux);

	unsigned int i = 0;

	//if (DEBUG_ALL_LEVELS == "N"){
	if (!strcmp(DEBUG_ALL_LEVELS , "N")){
		if (prefix[0] != 'Y'){
			pthread_mutex_unlock(&debugMux);
			return;
		}
	}

#if DEBUG_INCLUDE_TIME_TO_OUTPUT
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {
		dateTimeStamp dts;
		COMMON_GetCurrentDts(&dts);

		printf(DTS_PATTERN ": %s %04d: ", DTS_GET_BY_VAL(dts), &prefix[1], size);
		for (; i < size; i++)
			printf(" %02x", buf[i]);
		printf("\n");
	}
#else
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {
		printf("%s %04d: ", &prefix[1], size);
		for (; i < size; i++)
			printf(" %02x", buf[i]);
		printf("\n");
	}
#endif

	//if (DEBUG_TO_FILE == "Y") {
	if (!strcmp(DEBUG_TO_FILE , "Y")) {

	}

	pthread_mutex_unlock(&debugMux);
}
#endif

//
//
//

#if !REV_COMPILE_RELEASE
void COMMON_ASCII_DEBUG(char * prefix, unsigned char * buf, unsigned int size) {
	DEB_TRACE(DEB_IDX_COMMON)

	pthread_mutex_lock(&debugMux);

	unsigned int i = 0;

	//if (DEBUG_ALL_LEVELS == "N"){
	if (!strcmp(DEBUG_ALL_LEVELS , "N")){
		if (prefix[0] != 'Y'){
			pthread_mutex_unlock(&debugMux);
			return;
		}
	}

#if DEBUG_INCLUDE_TIME_TO_OUTPUT
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {
		dateTimeStamp dts;
		COMMON_GetCurrentDts(&dts);

		printf(DTS_PATTERN ": %s %04d: ", DTS_GET_BY_VAL(dts), &prefix[1], size);
		for (; i < size; i++)
		{
			if ( (buf[i] > 0x20) && (buf[i] < 0x7F) )
			{
				printf(" %c", buf[i]);
			}
			else
			{
				switch( buf[i] )
				{
					case 0x00: printf(" \\0"); break;
					case 0x09: printf(" \\t"); break;
					case 0x0A: printf(" \\n"); break;
					case 0x0B: printf(" \\v"); break;
					case 0x0D: printf(" \\r"); break;
					case 0x1B: printf(" ESC"); break;
					case 0x7F: printf(" DEL"); break;
					case 0x20: printf(" \' \'"); break;
					default: printf(" \\x%02X", buf[i]); break;
				}
			}
		}
		printf("\n");
	}
#else
	//if (DEBUG_TO_CONSOLE == "Y") {
	if (!strcmp(DEBUG_TO_CONSOLE , "Y")) {
		printf("%s %04d: ", &prefix[1], size);
		for (; i < size; i++)
		{
			if ( (buf[i] > 0x20) && (buf[i] < 0x7F) )
			{
				printf(" %c", buf[i]);
			}
			else
			{
				switch( buf[i] )
				{
					case 0x00: printf(" \\0"); break;
					case 0x09: printf(" \\t"); break;
					case 0x0A: printf(" \\n"); break;
					case 0x0B: printf(" \\v"); break;
					case 0x0D: printf(" \\r"); break;
					case 0x1B: printf(" ESC"); break;
					case 0x7F: printf(" DEL"); break;
					case 0x20: printf(" \' \'"); break;
					default: printf(" \\x%02X", buf[i]); break;
				}
			}
		}
		printf("\n");
	}
#endif

	//if (DEBUG_TO_FILE == "Y") {
	if (!strcmp(DEBUG_TO_FILE , "Y")) {

	}

	pthread_mutex_unlock(&debugMux);
}
#endif

//
//
//

void COMMON_GET_USPD_SN_UL(unsigned long * retSn){
	char uspdSn[32];
	
	(*retSn) = 0;
	memset(uspdSn, 32, 0);
	COMMON_GET_USPD_SN((unsigned char *)uspdSn);
	if (strlen(uspdSn) > 1){
		(*retSn) = strtoul(&uspdSn[1], NULL, 10);
	}
}

//
//
//

void COMMON_GET_USPD_SN(unsigned char * p){
	DEB_TRACE(DEB_IDX_COMMON)


	int fd = open(STORAGE_PATH_USPD_SN, O_RDONLY | O_NONBLOCK);
	if (fd < 0){
		sprintf((char*)p, "err fd");
		CLI_SetErrorCode(CLI_NO_CONFIG_SERIAL, CLI_SET_ERROR);
		return;
	}

	CLI_SetErrorCode(CLI_NO_CONFIG_SERIAL, CLI_CLEAR_ERROR);

	char sn[32];
	memset(sn, 0, 32);
	read(fd, sn, 32);

	sprintf ((char*)p, "%s", sn);

	close(fd);
}

//
//
//

void COMMON_GET_USPD_OBJ_NAME(unsigned char * p){
	DEB_TRACE(DEB_IDX_COMMON)


// ukhov - reading object name from file connection.cfg
#if 0
	int fd = open(STORAGE_PATH_USPD_SN, O_RDONLY | O_NONBLOCK);
	if (fd < 0){
		sprintf(p, "err fd");
		return;
	}

	char on[129];
	memset(on, 0, 129);
	read( fd, on, 128);

	sprintf (p, "%s", on);

	close(fd);
#endif


	if (p != NULL )
	{
		memset( p , 0 , STORAGE_PLACE_NAME_LENGTH );
		memcpy( p , &objNameUndPlace.objName , STORAGE_PLACE_NAME_LENGTH );
	}
}

//
//
//

void COMMON_GET_USPD_LOCATION(unsigned char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	if (p != NULL )
	{
		memset( p , 0 , STORAGE_PLACE_NAME_LENGTH );
		memcpy( p , &objNameUndPlace.objPlace , STORAGE_PLACE_NAME_LENGTH );
	}
}

//
//
//

void COMMON_GET_USPD_CABLE(char *p){
	DEB_TRACE(DEB_IDX_COMMON)

	int fd = open("/sys/class/net/eth0/carrier", O_RDONLY | O_NONBLOCK);
	if (fd < 0){
		sprintf(p, "err ETH");
		return;
	}

	char v;
	read(fd, &v , 1);

	switch (v){
		case '1':
			sprintf(p, "on");
			break;
		default:
			sprintf(p, "off");
			break;

	}

	close(fd);
}

//
//
//

void COMMON_GET_USPD_IP(char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	int fd;
	struct ifreq ifr;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0){
		sprintf(p, "err no soc");
		return;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

	if (ioctl(fd, SIOCGIFADDR, &ifr)<0){
		sprintf(p, "err no ioctl");
		close(fd);
		return;
	}

	sprintf(p, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	close(fd);
}

//
//
//

void COMMON_GET_USPD_MAC(char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	int fd;
	struct ifreq ifr;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0){
		sprintf(p, "err no soc");
		return;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

	if(ioctl(fd, SIOCGIFHWADDR, &ifr)<0){
		sprintf(p, "err no ioctl");
		close(fd);
		return;
	}

	sprintf(p, "%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5]);

	close(fd);
}

//
//
//

void COMMON_GET_USPD_MASK(char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	int fd;
	struct sockaddr_in *in_addr;
	struct ifreq ifdata;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0){
		sprintf(p, "err no soc");
		return;
	}

	strncpy(ifdata.ifr_name, "eth0", IFNAMSIZ-1);
	if (ioctl(fd, SIOCGIFNETMASK, &ifdata) < 0)
	{
		sprintf(p, "err no ioctl");
		close(fd);
		return;
	}

	in_addr = (struct sockaddr_in *) &ifdata.ifr_addr;
	sprintf(p, "%s", inet_ntoa(in_addr->sin_addr));

	close(fd);
}

//
//
//

void COMMON_USPD_GET_GW(char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	FILE *fp;
	char buf[512];
	unsigned char gateway[16];
	fp = fopen("/proc/net/route", "r");
	int gtw_presence_flag = 0;
	if (fp == NULL){
		sprintf(p , "err fd");
		return;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL){
		unsigned long dest, gate;
		dest = gate = 0;
		sscanf(buf, "%*s%lx%lx", &dest, &gate);
		if ((dest == 0) && (gate != 0)){
			memset(gateway, 0, 16);
			inet_ntop(AF_INET, &gate, /*ip->gateway*/ (char*)gateway, 16);
			gtw_presence_flag = 1;
			break;
		}
	}
	fclose(fp);

	if (gtw_presence_flag == 1){
		sprintf(p, "%s", gateway);
	} else {
		sprintf(p, "not set");
	}
}

//
//
//

void COMMON_ShiftTextDown( char * buf , int length )
{
	DEB_TRACE(DEB_IDX_COMMON)

	if ( buf != NULL )
	{
		int idx = 0;
		for( ; idx < length ; idx++ )
		{
			if ( buf[idx] >=65 && buf[idx] <= 90 )
				buf[idx] += 32 ;
		}

	}
	return ;
}

//
//
//

void COMMON_ShiftTextUp( char * buf , int length)
{
	DEB_TRACE(DEB_IDX_COMMON)

   if ( buf != NULL )
	{
		int idx = 0;
		for( ; idx < length ; idx++ )
		{
			if ( buf[idx] >=97 && buf[idx] <= 122 )
				buf[idx] -= 32 ;
		}

	}
	return ;
}

//
//
//


void COMMON_GetWebUIPass( unsigned char * pass )
{
	DEB_TRACE(DEB_IDX_COMMON)

	if ( pass == NULL )
		return ;
	FILE * fd;
	fd = fopen ( CONFIG_PATH_WUI , "r");
	if ( fd !=NULL )
	{
		char buf[MAX_MESSAGE_LENGTH];

		memset( buf, 0 , MAX_MESSAGE_LENGTH );
		//fgets(buf , MAX_MESSAGE_LENGTH , fd ); // [login]
		fscanf(fd , "%s" , buf);

		memset( buf, 0 , MAX_MESSAGE_LENGTH );
		//fgets(buf , MAX_MESSAGE_LENGTH , fd ); // user
		fscanf(fd , "%s" , buf);

		memset( buf, 0 , MAX_MESSAGE_LENGTH );
		//fgets(buf , MAX_MESSAGE_LENGTH , fd ); // [passwd]
		fscanf(fd , "%s" , buf);

		memset( buf, 0 , MAX_MESSAGE_LENGTH );
		//fgets(buf , MAX_MESSAGE_LENGTH , fd ); // user_pass
		fscanf(fd , "%s" , buf);

		memcpy( pass , buf , strlen(buf) );

		fclose(fd);
	}

	return ;
}

//
//
//

int COMMON_SetWebUIPass( unsigned char * pass )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	///TODO

	if ( pass )
	{
		int fd = open( CONFIG_PATH_WUI , O_RDWR );
		if ( fd > 0 )
		{
			unsigned char buf[MAX_MESSAGE_LENGTH];
			memset(buf, 0 , MAX_MESSAGE_LENGTH);

			int bufLength = read( fd , buf , MAX_MESSAGE_LENGTH);

			int nCounter = 0 ;
			int idx;
			unsigned char * passPos = NULL;

			for(idx = 0 ; idx < bufLength ; idx++)
			{
				if ( buf[idx] == '\n')
				{
					nCounter++;
				}
				if ( nCounter == 3)
				{
					passPos = &buf[idx+1];
					break;
				}
			}

			if ( passPos != NULL )
			{
				memset( passPos , 0 , (&buf[MAX_MESSAGE_LENGTH]) - passPos );
				sprintf((char *)passPos , "%s\n" , pass);

				lseek(fd , 0 , SEEK_SET );
				ftruncate( fd , 0);

				if (write( fd , buf , strlen((char *)buf)  ) == (int)strlen((char *)buf) )
					res = OK;
			}


			close(fd);
		}

	}


	return res;
}

//
//
//

void COMMON_USPD_GET_DNS(char * p){
	DEB_TRACE(DEB_IDX_COMMON)

	int fd;
	char buf[512];
	int buf_len = 0;
	memset( buf , 0 , 512 );
	fd = open( "/etc/resolv.conf" , O_RDONLY | O_NONBLOCK);
	if (fd<0){
		sprintf (p, "err fd");
		return;
	}

	buf_len = read( fd, buf, 512 );
	close(fd);

	//removing spaces and tabs
	int i;
	for( i=0 ; i<buf_len ; i++)
	{
		if((buf[i]==' ') || (buf[i]=='\t')){
			int j;
			for(j=i ; j< (buf_len-1) ; j++){
				buf[j] = buf[j+1];
			}
			buf_len--;
		}
	}

	char * ch_nameserver_pos;
	ch_nameserver_pos = strstr(buf , "nameserver");
	if(ch_nameserver_pos != NULL)
	{
		int nameserver_pos = (int)(ch_nameserver_pos - buf) + strlen("nameserver");
		i = 0;
		char dns[15];
		memset( dns , 0 , 15);
		while((buf[ nameserver_pos ] != '\n') && (i<15)){
			dns[i] = buf[nameserver_pos];
			nameserver_pos++;
			i++;
		}
		sprintf(p, "%s", dns);

	} else {
		sprintf(p, "not set");
	}

}

//
//
//

char * getTransactionResult(unsigned char result) {
	DEB_TRACE(DEB_IDX_COMMON)

	switch (result) {
		case TRANSACTION_NO_RESULT:
			return "TRANSACTION_NO_RESULT";

		case TRANSACTION_DONE_OK:
			return "TRANSACTION_DONE_OK";

		case TRANSACTION_DONE_ERROR_A:
			return "TRANSACTION_DONE_ERROR";

		case TRANSACTION_DONE_ERROR_SIZE:
			return "TRANSACTION_DONE_ERROR_SIZE";

		case TRANSACTION_COUNTER_NOT_OPEN:
			return "TRANSACTION_COUNTER_NOT_OPEN";

		case TRANSACTION_COUNTER_POOL_DONE:
			return "TRANSACTION_COUNTER_POOL_DONE";

		case TRANSACTION_DONE_ERROR_RESPONSE:
			return "TRANSACTION_DONE_ERROR_RESPONSE";

		default:
			return "UNKNOWN";
	}
}

//
//
//

char * getErrorCode(int ErrorCode) {
	DEB_TRACE(DEB_IDX_COMMON)

	switch (ErrorCode) {
		case OK:
			return "OK";

		case ERROR_GENERAL:
			return "ERROR_GENERAL";

		case ERROR_PLC:
			return "ERROR_PLC";

		case ERROR_SERIAL_GLOBAL:
			return "ERROR_SERIAL_GLOBAL";
		case ERROR_SERIAL_IOCTL:
			return "ERROR_SERIAL_IOCTL";
		case ERROR_SERIAL_OVERLAPPED:
			return "ERROR_SERIAL_OVERLAPPED";

		case ERROR_STORAGE:
			return "ERROR_STORAGE";

		case ERROR_CONNECTION:
			return "ERROR_CONNECTION";
		case ERROR_NO_CONNECTION_PARAMS:
			return "ERROR_NO_CONNECTION_PARAMS";

		case ERROR_NO_COUNTERS:
			return "ERROR_NO_COUNTERS";
		case ERROR_NO_DATA_YET:
			return "ERROR_NO_DATA_YET";
		case ERROR_NO_DATA_FOUND:
			return "ERROR_NO_DATA_FOUND";

		case ERROR_POOL_INIT:
			return "ERROR_POOL_INIT";

		case ERROR_TIME_IS_OUT:
			return "ERROR_TIME_IS_OUT";

		case ERROR_SIZE_WRITE:
			return "ERROR_SIZE_WRITE";

		case ERROR_SIZE_READ:
			return "ERROR_SIZE_READ";

		case ERROR_IS_NOT_FOUND:
			return "ERROR_IS_NOT_FOUND";

		case ERROR_MEM_ERROR:
			return "ERROR_MEM_ERROR";

		case ERROR_FILE_OPEN_ERROR:
			return "ERROR_FILE_OPEN_ERROR";

		case ERROR_CONFIG_FORMAT_ERROR:
			return "ERROR_CONFIG_FORMAT_ERROR";

		case ERROR_ARRAY_INDEX:
			return "ERROR_ARRAY_INDEX";

		case ERROR_COUNTER_NOT_OPEN:
			return "ERROR_COUNTER_NOT_OPEN";

		case ERROR_REPEATER:
			return "ERROR_REPEATER";

		case ERROR_NO_ANSWER:
			return "ERROR_NO_ANSWER";
			
		case ERROR_POOL_PLC_SHAMANING:
			return "ERROR_POOL_PLC_SHAMANING";
			
		case ERROR_COUNTER_TYPE:
			return "ERROR_COUNTER_TYPE";

		case ERROR_FILE_FORMAT:
			return "ERROR_FILE_FORMAT";

		default:
			return "ERROR_UNKNOWN (common.c: line 2204)";
	}
}


//
//
//
char * BOOL_VAL(BOOL val){
	if (val)
		return "TRUE";

	return "FALSE";
}

//
//
//

unsigned char * COMMON_FindNextSms(unsigned char * buf, smsBuffer_t * smsBuf)
{
	DEB_TRACE(DEB_IDX_COMMON)

	/**
	* This function returns OK if sms was found in incoming buffer. Also it stuff "smsBuffer_t" structure.
	*
	*/

	unsigned char * res = NULL;

	if ( (strstr((char *)buf , "+CMGL: ")!= NULL) && (strstr((char *)buf, "OK")!= NULL))
	{
		/**
		 *
		 * Searching for 3 bracket, because the phone number locate after 3 bracket
		 * Then searching for 4 bracket which close the text block contains the phone number
		 * then searching for first '\n', because the smsText is following after that
		 *
		 */

		int bracketCounter = 0 ;
		int idx = 0;

		buf = (unsigned char *)strstr((char *)buf , "+CMGL: ") ;

		/**
		  *+CMGR: "REC READ","+79616321904","","13/04/01,15:42:38+16"
		  *Uspd ipp 225.35.74.2:10100
		  */


		/*
		 *
		 *huawei
		 *
		 *

		02-12-13 14:33:50:
		GSM AT+CMGL="ALL"
		+CMGL: 1,"REC UNREAD","+79616321904",,"13/12/02,14:21:12+16"
		Uspd getsnsq


		OK



		02-12-13 14:33:51:
		GSM AT+CMGR=1
		+CMGR: "REC READ","+79616321904",,"13/12/02,14:21:12+16"
		Uspd getsnsq

		OK

		*
		*
		*/



		//COMMON_STR_DEBUG( DEBUG_COMMON "###################\nlength=%d\nbuf: %s", bufLength,buf);

		COMMON_STR_DEBUG( DEBUG_COMMON "SSCANF FOR SMS NUMBER");
		unsigned int currentSmsNumb = 0;
		sscanf((char *)buf , "+CMGL: %u" , &currentSmsNumb);
		COMMON_STR_DEBUG( DEBUG_COMMON "SSCANFED SUCCESS");

		unsigned char * phoneNumber = NULL;
		int phoneNumberLength = -1;
		unsigned char * smsText = NULL;

		unsigned char * endPos = NULL;
		endPos = (unsigned char *)strstr((char *)(buf+1) , "\r\n\r\n+CMGL: ");
		if (endPos == NULL)
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "CAN NOT FIND NEXT +CMGL: , SEARCHING FOR OK");
			endPos = (unsigned char *)strstr((char *)(buf+1) , "\r\n\r\n\r\nOK");
			if (endPos == NULL)
				return res;
		}
		else
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "FIRST SMS WAS FOUNDED");
		}

		//for( ; index < bufLength ; index++)
		while(&buf[idx] != endPos)
		{
			//COMMON_STR_DEBUG( DEBUG_GSM "%x\n",buf[index]);
			if (buf[idx] == '\"')
			{
				bracketCounter++;
				COMMON_STR_DEBUG( DEBUG_COMMON "bracket was found");
			}
			if ((bracketCounter == 3) && (phoneNumber == NULL))
			{
				phoneNumber = &buf[idx + 1];
			}
			if (bracketCounter == 3)
				phoneNumberLength++;
			if ((buf[idx] == '\n') && (smsText==NULL))
			{
				smsText = &buf[idx + 1];
				break;
			}
			idx++;
		}

		if ( (phoneNumber == NULL) || (smsText == NULL))
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "SmsParseBuffer() - exit. Pointers are NULL\nsmsText=%p\nphoneNumber=%p", smsText , phoneNumber);
			return res;
		}

		memcpy(smsBuf->phoneNumber , phoneNumber , phoneNumberLength);

		int smsTextLength = /*strstr(smsText , "\r\n\r\nOK\r\n")*/ endPos - smsText ;
		if (smsTextLength > SMS_TEXT_LENGTH)
			smsTextLength = SMS_TEXT_LENGTH - 1 ;

		memcpy(smsBuf->incomingText , smsText , smsTextLength);

		smsBuf->smsStorageNumber = currentSmsNumb ;

		COMMON_STR_DEBUG( DEBUG_GSM "\n!!!!!!!!!!!!!!!\r\nTEL: %s\nNUMB: %u\nTEXT: %s  \r\n!!!!!!!!!!!!!!!", smsBuf->phoneNumber , smsBuf->smsStorageNumber , smsBuf->incomingText);

	   // buf = buf + 1;

		res = buf;

	}
	return res;
}

//
//
//

int COMMON_FindSmsIn_CMGR_Output(unsigned char * buf, const int bufLength , smsBuffer_t * smsBuf)
{
	COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output start");
	int res = ERROR_GENERAL ;

	if ( !smsBuf )
	{
		COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output smsBuf is null");
		return res ;
	}

	COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output searching for +CMGR: and OK in buffer");

	char * cmgrPos = strstr((char *)buf , "+CMGR: ");
	char * okPos = strstr((char *)buf, "\r\n\r\nOK") ;
	if ( (cmgrPos == NULL) || (okPos == NULL))
	{
		COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output can not find +CMGR: or OK in buffer");
		return res ;
	}

	//searching for phone number
	int idx = 0;
	int bracketCounter = 0 ;

	char * phoneNumber = NULL ;
	int phoneNumberLength = -1 ;
	char * smsText = NULL ;

	COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output() searching for phone number");

	for(  ; idx < bufLength ; ++idx )
	{
		if ( cmgrPos[idx] == '\"' )
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "bracket was found");
			++bracketCounter ;
		}

		if ( ( bracketCounter == 3  ) && ( phoneNumber == NULL ) )
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "phone number was found");
			phoneNumber = (char *)&cmgrPos[idx + 1];
		}

		if ( ( bracketCounter == 3  ) && ( phoneNumber != NULL ) )
		{
			phoneNumberLength++;
		}

		if ((cmgrPos[idx] == '\n') && (smsText==NULL))
		{
			smsText = (char *)&cmgrPos[idx + 1];
			break;
		}
	}

	if ( (phoneNumber == NULL) || (smsText == NULL))
	{
		COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output() - exit. Pointers are NULL\nsmsText=%p\nphoneNumber=%p", smsText , phoneNumber);
		return res;
	}

	COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_FindSmsIn_CMGR_Output() phoneNumber and smsText were been found");

	memcpy(smsBuf->phoneNumber , phoneNumber , phoneNumberLength);

	int smsTextLength = okPos - smsText ;

	if (smsTextLength > SMS_TEXT_LENGTH)
		smsTextLength = SMS_TEXT_LENGTH - 1 ;

	memcpy(smsBuf->incomingText , smsText , smsTextLength);

	COMMON_STR_DEBUG( DEBUG_GSM "\n!!!!!!!!!!!!!!!\r\nTEL: %s\nNUMB: %u\nTEXT: %s  \r\n!!!!!!!!!!!!!!!", smsBuf->phoneNumber , smsBuf->smsStorageNumber , smsBuf->incomingText);

	res = OK ;

	return res ;
}

//
//
//

void COMMMON_CreateDescriptionForConnectEvent(unsigned char * evDesc , connection_t * connection , BOOL serviceMode)
{
	DEB_TRACE(DEB_IDX_COMMON)

	if( (!evDesc) || (!connection) )
	{
		return ;
	}

	memset(evDesc , 0 , EVENT_DESC_SIZE);

	if ( serviceMode == TRUE )
	{

		switch(connection->mode)
		{
			case CONNECTION_ETH_ALWAYS:
			case CONNECTION_ETH_CALENDAR:
			case CONNECTION_ETH_SERVER:
			case CONNECTION_GPRS_ALWAYS:
			case CONNECTION_GPRS_CALENDAR:
			case CONNECTION_GPRS_SERVER:
			{
				struct hostent *server;
				server = gethostbyname((char *)connection->serviceIp);

				if( server )
				{
					int ipLength = 4;
					if ( server->h_length < 4 )
						ipLength = server->h_length ;

					memcpy( evDesc , server->h_addr , ipLength );
				}

				evDesc[4] = ( connection->servicePort >> 24 ) & 0xFF ;
				evDesc[5] = ( connection->servicePort >> 16 ) & 0xFF ;
				evDesc[6] = ( connection->servicePort >> 8 ) & 0xFF ;
				evDesc[7] =   connection->servicePort & 0xFF ;
			}
			break;

			default:
				break;
		}

		if (connection->mode == CONNECTION_GPRS_SERVER)
			evDesc[8] = CONNECTION_GPRS_ALWAYS & 0xFF ;
		else if (connection->mode == CONNECTION_ETH_SERVER)
			evDesc[8] = CONNECTION_ETH_ALWAYS & 0xFF ;
		else
			evDesc[8] = connection->mode & 0xFF ;
	}
	else
	{
		switch(connection->mode)
		{
			case CONNECTION_ETH_ALWAYS:
			case CONNECTION_ETH_CALENDAR:
			case CONNECTION_GPRS_ALWAYS:
			case CONNECTION_GPRS_CALENDAR:
			{
				struct hostent *server;
				server = gethostbyname((char *)connection->server);

				if( server )
				{
					int ipLength = 4;
					if ( server->h_length < 4 )
						ipLength = server->h_length ;

					memcpy( evDesc , server->h_addr , ipLength );
				}

				evDesc[4] = ( connection->port >> 24 ) & 0xFF ;
				evDesc[5] = ( connection->port >> 16 ) & 0xFF ;
				evDesc[6] = ( connection->port >> 8 ) & 0xFF ;
				evDesc[7] =   connection->port & 0xFF ;
			}
				break;

			case CONNECTION_GPRS_SERVER:
			case CONNECTION_ETH_SERVER:
			{
				struct hostent *server;
				server = gethostbyname((char *)connection->clientIp);

				if( server )
				{
					int ipLength = 4;
					if ( server->h_length < 4 )
						ipLength = server->h_length ;

					memcpy( evDesc , server->h_addr , ipLength );
				}

				evDesc[4] = ( connection->clientPort >> 24 ) & 0xFF ;
				evDesc[5] = ( connection->clientPort >> 16 ) & 0xFF ;
				evDesc[6] = ( connection->clientPort >> 8 ) & 0xFF ;
				evDesc[7] =   connection->clientPort & 0xFF ;
			}
			break;

			default:
				break;
		}


		evDesc[8] = connection->mode & 0xFF ;
	}
	evDesc[9] = connection->protocol & 0xFF ;

	return ;
}

//
//
//

void COMMON_CharArrayDisjunction( char * firstString , char * secondString, unsigned char * eventDescBuf)
{
	DEB_TRACE(DEB_IDX_COMMON)

	memset(eventDescBuf , 0 , EVENT_DESC_SIZE);

	int i;
	for (i = 0 ; i < EVENT_DESC_SIZE ; i++)
	{
		eventDescBuf[i] = (firstString[i]) | (secondString[i]);
	}
}

//
//
//

void COMMON_TranslateLongToChar(unsigned long counterDbID, unsigned char * eventDescBuf)
{
	DEB_TRACE(DEB_IDX_COMMON)

	memset( eventDescBuf ,0 , EVENT_DESC_SIZE) ;

	eventDescBuf[3] = (counterDbID & 0xFF) ;
	eventDescBuf[2] = (counterDbID & 0xFF00) >> 8 ;
	eventDescBuf[1] = (counterDbID & 0xFF0000) >> 16;
	eventDescBuf[0] = (counterDbID & 0xFF000000) >> 24 ;
}

//
//
//

int COMMON_Translate10to2_10(int dec)
{
	DEB_TRACE(DEB_IDX_COMMON)

	char buf[8];
	memset(buf , 0 , 8);

	sprintf( buf , "%d" , dec);
	int hex = 0 ;
	sscanf(buf , "%x" , &hex);
	return hex;
}

//
//
//

int COMMON_Translate2_10to10(int hex)
{
	DEB_TRACE(DEB_IDX_COMMON)

	char buf[8];
	memset(buf , 0 , 8);

	sprintf(buf , "%x" , hex);
	int dec = 0 ;
	sscanf(buf , "%d" , &dec);
	return dec ;
}

//
//
//

int COMMON_TranslateHexPwdToAnsi( unsigned char * startString , unsigned char * password )
{
	int res = ERROR_GENERAL ;

	if( ( startString == NULL ) || ( password == NULL ) )
		return res ;

	memset( password , 0 , PASSWORD_SIZE );
	int passwordPos = 0 ;

	int idx = 0 ;
	while(1)
	{
		char buf[3];
		memset( buf , 0 , 3);

		buf[0] = startString[ idx ];
		buf[1] = startString[ idx + 1];

		int decBuf = 0 ;
		//sscanf( buf , "%02x" , &decBuf);
		decBuf  = (int)strtoul(buf, NULL, 16);

		password[ passwordPos++ ] = decBuf & 0xFF ;

		idx += 2 ;
		if( ( passwordPos == PASSWORD_SIZE ) || ( idx >= (int)( strlen( (char *)startString) - 1 ) ) )
		{
			res = OK ;
			break;
		}
	}

	return res ;
}

//
//
//

int COMMON_SearchBlock(char * text , int textLength, char * blockName , blockParsingResult_t ** searchResult , int * searchResultSize)
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	if (blockName)
	{
		char * buffer = calloc(textLength + 1 , sizeof( char ) ) ;
		if ( buffer )
		{
			memcpy( buffer , text , textLength );

			char ** tokens = NULL ;
			int tokensNumb = 0 ;
			char * lastPtr = NULL ;
			tokens = (char **)calloc( ACOUNTS_SEARCHING_BLOCK_TOKENS_NUMB , sizeof(char *) );
			if (tokens)
			{
				char * pch = strtok_r(buffer , "\r\n" , &lastPtr) ;
				while(pch)
				{

					COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock writing pointer to the [%08X]" , &tokens[ tokensNumb ] ) ;
					tokens[ tokensNumb++ ] = pch ;
					pch = strtok_r( NULL , "\r\n" , &lastPtr) ;

					if ( tokensNumb >= ACOUNTS_SEARCHING_BLOCK_TOKENS_NUMB )
					{
						COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock Number of tokens is greater then array elements number. Try to allocate one more.");
						tokens = (char **)realloc( tokens , (tokensNumb + 1) * sizeof(char *) );
						if ( !tokens )
						{
							COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock Allocating one more element of array is ERROR\n");
							free( tokens );
							return ERROR_MEM_ERROR ;
						}


					}

					if ( tokensNumb == ACOUNTS_TARIFF_MAX_TOKENS )
						break;
				}

				//searching block header in the tokens array

				char header[64];
				memset( header , 0 , 64 );

				snprintf( header , 64 , "[%s]" , blockName) ;

				int tokensIndex = 0 ;

				int headerIndex = -1 ;
				for ( ; tokensIndex < tokensNumb ; ++tokensIndex)
				{
					if ( strcmp( tokens[ tokensIndex ] , header ) == 0)
					{
						headerIndex = tokensIndex ;
						break ;
					}
				}

				if ( headerIndex >= 0 )
				{
					COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock header was found");
					//header was found
					res = OK ;

					//searching next header position
					for( tokensIndex = headerIndex + 1 ; tokensIndex < tokensNumb ; ++tokensIndex )
					{
						if ( (tokens[ tokensIndex ][ 0 ] == '[') && (tokens[ tokensIndex ][ strlen(tokens[ tokensIndex ]) - 1 ] == ']') )
						{
							break;
						}
					}

					int nextHeaderPos = tokensIndex ;


					*searchResultSize = 0 ;
					for( tokensIndex = headerIndex + 1 ; tokensIndex < nextHeaderPos ; ++tokensIndex )
					{
						COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock handle token [%s] by index", tokens[ tokensIndex ] , tokensIndex);
						char * eqPos = strstr(tokens[ tokensIndex ] , "=");
						if ( eqPos )
						{
							if ( ( eqPos != tokens[ tokensIndex ] ) && \
								( eqPos != &tokens[ tokensIndex ][ strlen( tokens[ tokensIndex ] ) - 1 ] ) )
							{
								COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock [=] was founeded");
								COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock (*searchResultSize)=[%d]" , (*searchResultSize) );
								(*searchResult) = realloc( (*searchResult) , (++(*searchResultSize))*sizeof(blockParsingResult_t) ) ;
								if ( (*searchResult) )
								{
									memset( &(*searchResult)[ ((*searchResultSize) - 1) ].variable , 0 , ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE );
									memset( &(*searchResult)[ ((*searchResultSize) - 1) ].value , 0 , ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE );

									int variableLength = eqPos - tokens[ tokensIndex ] ;

									memcpy( &(*searchResult)[ ((*searchResultSize) - 1) ].variable , \
										   tokens[ tokensIndex ] , \
										   ( variableLength >= ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE ) ? ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE - 1 : variableLength );

									memcpy( &(*searchResult)[ ((*searchResultSize) - 1) ].value , \
										   (eqPos + 1) , \
										   ( strlen(eqPos + 1) >= ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE) ? ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE - 1 : strlen( eqPos + 1 ) );

								}
								else
								{
									free( (*searchResult) );
									*searchResultSize = 0 ;
									res = ERROR_MEM_ERROR ;
									break;
								}

							}



						}
					}


				}


				free(tokens);

			}
			else
				res = ERROR_MEM_ERROR ;


			free(buffer);

		}
		else
			res = ERROR_MEM_ERROR ;
	}

	COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_SearchBlock() return  %s" , getErrorCode(res));

	return res ;
}


//
//
//

int COMMON_RemoveComments(char * text , int * textLength , char startCharacter )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	if (text && textLength )
	{

		int cursorIndex = 0 ;
		//for( ; cursorIndex < (*textLength) ; ++cursorIndex )
		while( cursorIndex < (*textLength) )
		{
			if( ( (cursorIndex == 0) && ( text[ cursorIndex ] == startCharacter ) ) || \
				( ( text[ cursorIndex ] == startCharacter ) && ( text[ cursorIndex - 1 ] == '\n' ) ) )
			{
				int commentLength = 0;
				int stringTailPos = cursorIndex ;
				while( (text[ ++stringTailPos ] != '\n') && ( ((++commentLength) + (cursorIndex)) < (*textLength) ) ) ;

				memmove( &text[ cursorIndex ] , &text[ stringTailPos + 1 ] , (*textLength) - stringTailPos + 2) ;
				(*textLength) -= commentLength + 2;
				text[ (*textLength) ] = 0 ;

			}
			else
				++cursorIndex ;
		}

		res = OK ;

	}


	return res ;
}

//
//
//

int COMMON_CheckPhrasePresence( unsigned char ** searchPhrases , unsigned char * buf , int bufLength , unsigned char * foundedMessage )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	int presenceFlag = 0 ;
	int searchIndex = 0 ;
	unsigned char * currentSearch = searchPhrases[ searchIndex ];

	while( currentSearch )
	{
		//printf("!!!PRASE [%s]\n", currentSearch);
		//COMMON_BUF_DEBUG( DEBUG_GSM "!!!CHECK PHRASE", currentSearch , strlen( currentSearch ) );
		if( bufLength >= (int)strlen((char *)currentSearch) )
		{
			int bufIndex = 0 ;
			for( ; bufIndex <= (int)(bufLength-strlen((char *)currentSearch)) ; ++bufIndex )
			{
				//COMMON_BUF_DEBUG( DEBUG_GSM "!!!CHECK BUF", &buf[ bufIndex ] , strlen( currentSearch ) );
				if( memcmp( &buf[ bufIndex ] , currentSearch , strlen((char *)currentSearch) ) == 0 )
				{
					presenceFlag = 1 ;
					break;
				}
			}

			//if found something
			if ( presenceFlag )
			{
				if( foundedMessage )
				{
					snprintf( (char *)foundedMessage , 64 , "%s" , currentSearch);
				}
				return OK ;
			}
		}
		currentSearch = searchPhrases[ ++searchIndex ];
	}

	return res ;
}

//
//
//

static pthread_mutex_t fsMux = PTHREAD_MUTEX_INITIALIZER;
int COMMON_WriteFileToFS( unsigned char * buffer , int bufferLength , unsigned char * fileName)
{
	DEB_TRACE(DEB_IDX_COMMON);

	pthread_mutex_lock( &fsMux );

	int res = ERROR_GENERAL ;

	if ( buffer )
	{
		int fd = open((char *)fileName , O_WRONLY | O_CREAT | O_TRUNC, 0777);
		if ( fd > 0 )
		{
			if ( write( fd , buffer, bufferLength ) == bufferLength )
				res = OK ;
			close(fd);
		}
	}

	pthread_mutex_unlock( &fsMux );

	return res ;

}

//
//
//
int COMMON_GetFileFromFS(unsigned char ** buffer , int * bufferLength , unsigned char * fileName)
{
	DEB_TRACE(DEB_IDX_COMMON);

	pthread_mutex_lock( &fsMux );

	int res = ERROR_GENERAL ;
	if ( ((*buffer) == NULL) && (buffer) )
	{
		int fd = open( (char *)fileName , O_RDONLY );
		if ( fd >= 0 )
		{
			struct stat fd_stat;
			memset(&fd_stat , 0 , sizeof(stat));
			fstat(fd , &fd_stat);

			if( fd_stat.st_size > 0 )
			{
				(*buffer) = (unsigned char *)malloc(fd_stat.st_size);
				(*bufferLength) = 0 ;

				if ( (*buffer) )
				{
					memset( (*buffer) , 0 , fd_stat.st_size );
					int bytes = 0 ;
					while( (bytes = read(fd , &(*buffer)[(*bufferLength)] , 128)) > 0 )
					{
						(*bufferLength) += bytes ;
					}
					if (fd_stat.st_size == (*bufferLength))
					{
						res = OK ;
					}
					else
					{
						free( (*buffer) );
						(*buffer) = NULL ;
					}
				}
				else
				{
					res = ERROR_MEM_ERROR ;
				}
			}
			else
			{
				res = ERROR_FILE_IS_EMPTY ;
			}


			close(fd);
		}
	}

	pthread_mutex_unlock( &fsMux );

	return res ;
}

//
//
//

int COMMON_RemoveFileFromFS( unsigned char * fileName  )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	if (unlink((char *)fileName) == 0 )
		res = OK;

	return res ;
}

//
// if ERROR_MAX_LEN_REACHED than '\0' is not present at the end of buffer
//

int COMMON_ReadLn( int fd , char * buffer , int max, int * r  )
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = ERROR_GENERAL ;

	//reset buffer content
	memset(buffer, 0, max);
	(*r) = 0 ;

	//loop read
	do
	{
		if ((*r) >= max){
			res = ERROR_MAX_LEN_REACHED;
			break;
		}

		int readed = read( fd , &buffer[ (*r)++ ] , 1 );
		if ( readed == 0 )
		{
			--(*r);
			if ( (*r) > 0)
				res = OK ;
			break;
		}
		else if ( readed < 0 )
		{
			COMMON_STR_DEBUG( DEBUG_COMMON "COMMON_ReadLn() error : %s ", strerror(errno) );
			break;
		}
		else
		{
			if ( buffer[ (*r) - 1 ] == '\n' )
			{
				buffer[ --(*r) ] = '\0';
				res = OK ;
				break;
			}
		}
	}
	while( 1 );

	return res ;
}

//
//
//

int COMMON_AllocateMemory(unsigned char ** pointer, int new_size)
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res=ERROR_MEM_ERROR;
	int i=0;

	if ( new_size > 0 )
	{
		for( ;i<MICRON_MALLOC_ATTEMPTS; i++)
		{
			(*pointer)=realloc((*pointer), new_size*sizeof(unsigned char));
			if ((*pointer)!=NULL)
			{
				res=OK;
				break;
			}
		}
	}
	else
	{
		res = COMMON_FreeMemory( pointer );
	}

	// If realloc() can not allocate memory
	if(res == ERROR_MEM_ERROR)
	{
		COMMON_STR_DEBUG("Y" "ERROR - can not allocate memory");
		EXIT_PATTERN;
	}
	return res;
}

//
//
//

int COMMON_FreeMemory(unsigned char ** pointer)
{
	DEB_TRACE(DEB_IDX_COMMON)

	int res = OK;

	if ((*pointer) != NULL)
	{
		free( (*pointer) );
		(*pointer) = NULL;
	}

	return res;
}

//
//
//

int COMMON_CheckSawInMeterage(meterage_t * meterage)
{
	if ( !meterage ){
		return ERROR_GENERAL ;
	}

	energy_t nullEnergy;
	memset( &nullEnergy , 0 , sizeof(energy_t) );

	int res = OK ;

	int i = 0 ;
	for ( ; i < 4 ; ++i )
	{
		int j = 0 ;
		for ( ; j < 4 ; ++j)
		{
			if ( i != j )
			{
				if ( (memcmp( &meterage->t[i] , &nullEnergy , sizeof(energy_t) ) ) && (memcmp( &meterage->t[j] , &nullEnergy , sizeof(energy_t) ) ) )
				{
					if ( !memcmp( &meterage->t[i] , &meterage->t[j] , sizeof(energy_t)) )
					{
						COMMON_STR_DEBUG( "Y\nCOMMON_CheckSawInMeterage() dublicate meterage in tariff [%d] and tariff [%d]!!!\n" , i+1 , j+1 ) ;

						#if REV_COMPILE_PLC
							PLC_ErrorIncrement();
						#endif
						return ERROR_GENERAL ;
					}
				}
			}
		}
	}
	return res ;
}

//
//
//

int COMMON_GetStorageDepth( counter_t * counter , int storage )
{
	switch( storage )
	{
		case STORAGE_CURRENT:
		{
			return 1 ;
		}
		break;

		case STORAGE_DAY:
		{
			switch( counter->type )
			{
				case TYPE_UJUK_SET_1M_01: //osipov sets
				{
					return 0;
				}
				break;


				case TYPE_UJUK_PSH_4TM_05A:
				case TYPE_UJUK_PSH_4TM_05MB:
				case TYPE_UJUK_PSH_4TM_05D:
				case TYPE_UJUK_PSH_4TM_05MD:

				case TYPE_UJUK_PSH_3TM_05A:
				case TYPE_UJUK_PSH_3TM_05MB:
				case TYPE_UJUK_PSH_3TM_05D:

				case TYPE_UJUK_SEB_1TM_01A:
				case TYPE_UJUK_SEB_1TM_02A:
				case TYPE_UJUK_SEB_1TM_02D:
				case TYPE_UJUK_SEB_1TM_02M:

				case TYPE_UJUK_SEO_1_16:

				case TYPE_UJUK_SET_4TM_01A:
				case TYPE_UJUK_SET_4TM_02A:
				case TYPE_UJUK_SET_4TM_02MB:
				{
					return 2 ;
				}
				break;

				case TYPE_UJUK_SEB_1TM_03A:
				case TYPE_UJUK_PSH_4TM_05MK:
				case TYPE_UJUK_SET_4TM_03A:
				case TYPE_UJUK_SET_4TM_03MB:
				{
					return 30 ;
				}
				break;

				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_103:
				case TYPE_MAYK_MAYK_302:				
				{
					return 30 ;
				}

//				case TYPE_GUBANOV_PSH_3TA07_DOT_1:
//				case TYPE_GUBANOV_PSH_3TA07_DOT_2:
//				case TYPE_GUBANOV_PSH_3TA07A:
//				case TYPE_GUBANOV_SEB_2A05:
//				case TYPE_GUBANOV_SEB_2A07:
//				case TYPE_GUBANOV_SEB_2A08:
//				case TYPE_GUBANOV_MAYK_101:
//				{
//					return 1;
//				}
//				break;

//				case TYPE_GUBANOV_MAYK_102:
//				{
//					return 45 ;
//				}
//				break;

//				case TYPE_GUBANOV_PSH_3ART:
//				case TYPE_GUBANOV_PSH_3ART_1:
//				case TYPE_GUBANOV_PSH_3ART_D:
//				case TYPE_GUBANOV_MAYK_301:
//				{
//					return 2 ;
//				}
//				break;

				case TYPE_GUBANOV_PSH_3TA07_DOT_1:
				case TYPE_GUBANOV_PSH_3TA07_DOT_2:
				case TYPE_GUBANOV_PSH_3TA07A:
				case TYPE_GUBANOV_SEB_2A05:
				case TYPE_GUBANOV_SEB_2A07:
				case TYPE_GUBANOV_SEB_2A08:
				case TYPE_GUBANOV_MAYK_101:
				case TYPE_GUBANOV_MAYK_102:
				case TYPE_GUBANOV_PSH_3ART:
				case TYPE_GUBANOV_PSH_3ART_1:
				case TYPE_GUBANOV_PSH_3ART_D:
				case TYPE_GUBANOV_MAYK_301:
					return GUBANOV_GetMaxDayMeteragesDeep(counter);
			}
		}
		break;

		case STORAGE_MONTH:
		{
			switch( counter->type )
			{
				case TYPE_UJUK_SET_1M_01: //osipov sets
				{
					return 0 ;
				}
				break;

				default:
				{
					return 12 ;
				}
				break;
			}

		}
		break;

		case STORAGE_PROFILE:
		{
			switch( counter->type )
			{
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
					return (1488 * 2);
				}
				break;

				case TYPE_UJUK_SEB_1TM_01A:
				case TYPE_UJUK_SEB_1TM_02A:
				case TYPE_UJUK_SET_4TM_01A:
				case TYPE_UJUK_SET_4TM_02A:
				case TYPE_UJUK_SET_4TM_03A:
				case TYPE_UJUK_PSH_3TM_05A:
				case TYPE_UJUK_PSH_4TM_05A:
				case TYPE_UJUK_SEO_1_16:
				case TYPE_UJUK_PSH_4TM_05MB:
				case TYPE_UJUK_PSH_3TM_05MB:
				case TYPE_UJUK_PSH_4TM_05MD:
				case TYPE_UJUK_SET_4TM_02MB:
				case TYPE_UJUK_SEB_1TM_02D:
				case TYPE_UJUK_PSH_3TM_05D:
				case TYPE_UJUK_PSH_4TM_05D:
				case TYPE_UJUK_PSH_4TM_05MK:
				case TYPE_UJUK_SET_1M_01:
				case TYPE_UJUK_SEB_1TM_02M:
				case TYPE_UJUK_SEB_1TM_03A:
				case TYPE_UJUK_SET_4TM_03MB:
				{
					if( (counter->profileInterval != STORAGE_UNKNOWN_PROFILE_INTERVAL) && (counter->profileInterval != 0) )
					{
						return ( UJUK_GetMaxProfileEntriesNumber( counter ) / ((60 / counter->profileInterval) + 1) ) ;
					}
				}
				break;

				case TYPE_MAYK_MAYK_101:
				case TYPE_MAYK_MAYK_103:
				case TYPE_MAYK_MAYK_302:
				{
					return ( 123 * 24 * 2 ) ;
				}
				break;
			}
		}
		break;

		default:
			break;
	}

	return 0 ;
}

//
//
//

#if 0
int COMMON_TranslateSpecialSmsEncodingToAscii( unsigned char * text , int * length )
{
	int res = OK ;

	unsigned char buf[3];
	memset(buf , 0 , 3);

	int oldLength = (*length);

	int idx = 0;
	while( idx < (*length) )
	{
		//printf("Parsing symbol %d : [%c]\n" , index , text[index]);
		if ( text[ idx ] == '='  )
		{
			if ( (idx + 2 ) < (*length) )
			{
				if ( text[ idx + 1 ] == '=')
				{
					int shiftIndex = idx + 1 ;
					while( shiftIndex < (*length) )
					{
						text[ shiftIndex - 1 ] = text[ shiftIndex ];
						shiftIndex++;
					}
					(*length) -= 1 ;
				}
				else
				{
					memset( buf , 0 , 3);
					memcpy( buf , &text[ idx + 1] , 2);

					int asciiCode = 0 ;
					if ( sscanf( (char *)buf , "%02X" , &asciiCode ) > 0 )
					{
						if ( asciiCode )
						{
							if ( asciiCode < 0x10 )
							{
								if ( buf[0] != '0' )
								{
									//printf("buf is [%s]\n", buf);
									res = ERROR_GENERAL ;
									break;
								}
							}

							text[ idx ] = (unsigned char)asciiCode ;
							//shift tail of array
							int shiftIndex = idx + 1 ;
							while( shiftIndex < ((*length) - 1) )
							{
								text[ shiftIndex ] = text[ shiftIndex + 2 ] ;
								shiftIndex++;
							}
							(*length) -= 2 ;

						}
						else
						{
							//printf("parsed ascii code is [%d]\n" , asciiCode);
							res = ERROR_GENERAL ;
							break;
						}
					}
					else
					{
						//printf("ssanf return zero\n");
						res = ERROR_GENERAL ;
						break;
					}
				}

			}
			else if ( (idx + 2) == ( *length ) )
			{
				if ( text[ idx + 1 ] == '=' )
				{
					text[ idx + 1 ] = 0 ;
					(*length) -= 1 ;
				}
				else
				{
					res = ERROR_GENERAL ;
					break;
				}
			}
			else if ( (idx + 1) == (*length) )
			{
				res = ERROR_GENERAL ;
				break ;
			}
		}
		idx++;
	}

	if ( (res == OK) && ( oldLength != (*length) ) )
	{
		memset( &text[ (*length) ] , 0 , oldLength - (*length) );
	}

	return res ;
}

#endif

void COMMON_DecodeSmsSpecialChars(char * str)
{
	char buf[3] = {0} ;
	char * cp = NULL ;
	int str_idx = 0 ;
	int cp_idx = 0 ;
	int length = strlen(str);

	if (!str)
		return ;

	if ( !strchr( str , '=' ) )
		return ;

	cp = strdup(str);
	if ( !cp )
		return ;

	memset(str , 0 , length);

	while( cp_idx < length )
	{
		if ( cp[ cp_idx ] != '=' )
			str[ str_idx++ ] = cp[ cp_idx++ ];

		else
		{
			if ( cp [cp_idx + 1] == '\0' )
				break;
			else if ( cp [ cp_idx + 1 ] == '=' )
			{
				str[str_idx++] = '=';
				cp_idx += 2 ;
			}
			else
			{
				strncpy(buf, &cp[cp_idx + 1], 2);
				str[str_idx] = (unsigned int)strtoul( buf , NULL , 16 );
				//sscanf(buf, "%02X", (unsigned int *)&str[str_idx] );
				if ( iscntrl( str[str_idx] ) )
				{
					str[str_idx] = '\0' ;
					break;
				}
				str_idx++ ;
				cp_idx += strlen(buf) + 1 ;
			}
		}
	}

	free( cp ) ;
	return ;
}

//
//
//

void COMMON_RemoveExcessBlanks( char * str )
{
	int idx = 0 ;
	int length = strlen(str) ;
	BOOL inBlank = FALSE ;

	while( idx < length )
	{
		if ( str[idx] == ' ' )
		{
			if ( !inBlank )
			{
				inBlank = TRUE;
				idx++;
			}
			else
				memmove( &str[idx] , &str[idx + 1], (length--) - idx );
		}
		else
		{
			if ( inBlank )
				inBlank = FALSE ;
			idx++;
		}

	} //while( idx < length )

	//removing leading blank
	if ( str[0] == ' ' )
		memmove( &str[0], &str[1], length-- );

	return ;
}

//
//
//

int COMMON_TranslateFormatStringToArray( char * formatString , int * arrLength , unsigned char ** arr , int maxLength )
{
	int res = ERROR_GENERAL ;
	char * lastPtr = NULL ;
	char * ptr = strtok_r( formatString , " \t" , &lastPtr );
	char ** tokens = NULL;
	int tokensCounter = 0;

	while( ptr )
	{
		tokens = ( char ** )realloc( (void *)tokens , (++tokensCounter) * sizeof( char * ) );
		if ( !tokens )
		{
			fprintf( stderr , "Can not allocate [%d] bytes\n" , (tokensCounter) * sizeof( char * ) );
			EXIT_PATTERN;
		}
		tokens[ tokensCounter - 1 ] = ptr ;
		ptr = strtok_r( NULL , " \t" , &lastPtr );

		if ( maxLength > 0 )
		{
			if ( tokensCounter == maxLength )
			{
				break;
			}
		}
	}

	if ( tokens )
	{
		if ( tokensCounter > 0 )
		{
			res = OK ;
			(*arrLength) = tokensCounter ;
			(*arr) = (unsigned char *)malloc( tokensCounter*sizeof( unsigned char ) );
			if ( !(*arr) )
			{
					fprintf( stderr , "Can not allocate [%d] bytes\n" , (*arrLength));
					EXIT_PATTERN;
			}
			memset( (*arr) , 0 , tokensCounter * sizeof(unsigned char) );

			int tokensIndex = 0 ;
			for ( ; tokensIndex < tokensCounter ; ++tokensIndex )
			{
				(*arr)[ tokensIndex ] = (unsigned char)atoi( tokens[ tokensIndex ] );
			}
		}

		free( tokens );
		tokens = NULL ;
	}
	return res ;
}
//EOF
