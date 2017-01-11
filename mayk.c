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
#include "mayk.h"
#include "storage.h"
#include "acounts.h"
#include "debug.h"

#define COUNTER_TASK_SYNC_TIME			1
#define COUNTER_TASK_WRITE_TARIFF_MAP	1
#define COUNTER_TASK_WRITE_PSM			1
#define COUNTER_TASK_UPDATE_SOFTWARE	1
#define COUNTER_TASK_READ_PROFILES		1
#define COUNTER_TASK_READ_CUR_METERAGES	1
#define COUNTER_TASK_READ_DAY_METERAGES	1
#define COUNTER_TASK_READ_MON_METERAGES	1
#define COUNTER_TASK_READ_JOURNALS		1
#define COUNTER_TASK_READ_PQP			1

//local variables

//
//
//

unsigned char MAYK_GetCRC(unsigned char * cmd , unsigned int size)
{

	DEB_TRACE(DEB_IDX_MAYAK)

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetCRC() start.");
	unsigned char crc = 0;
	unsigned int byteIndex ;

	for( byteIndex = 0 ; byteIndex < size ; byteIndex++)
		crc^=cmd[byteIndex];
	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetCRC() exit. CRC=0x%X" , crc);
	return crc;
}

//
//
//

void MAYK_Stuffing(unsigned char ** cmd , unsigned int * length)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Stuffing() start. Old cmd length=%d", *length);

	int stuffCounter = 0 ;
	unsigned int bufIndex = 0;

	// calc bytes to stuffing number

	for( ; bufIndex < (*length) ; ++bufIndex )
	{
		if ( ( (*cmd)[ bufIndex ] == 0x7E ) || ( (*cmd)[ bufIndex ] == 0x5D ) )
		{
			++stuffCounter ;
		}
	}

	//allocate memory
	if( stuffCounter > 0 )
	{
		int oldIndex = (*length) - 1 ;
		int newIndex = oldIndex + stuffCounter ;

		(*length) += stuffCounter ;


		(*cmd) = realloc( (*cmd) , (*length) );
		if( !(*cmd) )
		{
			printf("STUFFING ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}

		while( oldIndex >= 0 )
		{
			//printf("oldIndex = [%d] , newIndex = [%d] , stuffConter = [%d]. " , oldIndex , newIndex  , stuffCounter);
			if ( ( (*cmd)[ oldIndex ] == 0x7E ) || ( (*cmd)[ oldIndex ] == 0x5D ) )
			{
				//printf("making stuffing\n");
				(*cmd)[newIndex--] = (*cmd)[ oldIndex-- ] ^ 0x20  ;
				(*cmd)[newIndex--] = 0x5D ;

				if( (--stuffCounter) == 0 )
				{
					break;
				}

			}
			else
			{
				//printf("Simple copy\n");
				(*cmd)[ newIndex-- ] = (*cmd)[ oldIndex-- ] ;
			}
		}


	}

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Stuffing() exit. New cmd length=%d", *length);
}

//
//
//

int  MAYK_AddQuotes(unsigned char ** cmd , unsigned int * length)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AddQuotes() start. Old cmd length=%d" , *length);
	int res = ERROR_GENERAL;

	(*length) += 2 ;
	COMMON_AllocateMemory( cmd , (*length) );
	//shift all elements of array to the rigth

	int shiftIndex;
	for( shiftIndex = (*length)-1-1 ; shiftIndex > 0 ; shiftIndex--)
		(*cmd)[ shiftIndex ] = (*cmd)[ shiftIndex - 1 ] ;
	//add 7E bytes to the first and last byte of array
	(*cmd)[0] = (unsigned char)0x7E;
	(*cmd)[ (*length)-1 ] = (unsigned char)0x7E;
	res = OK;


	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AddQuotes() exit. New cmd length=%d", *length);
	return res;
}

//
//
//

int MAYK_RemoveQuotes( unsigned char ** cmd , unsigned int * length )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_RemoveQuotes() start. Old cmd length=%d", *length);
	int res = ERROR_GENERAL;

	if((cmd == NULL) || ((*cmd)==NULL) || ((*length)==0))
		return res;

	unsigned int cmdIndex = 0;
	int counter7E = 0;

	while(cmdIndex < (*length))
	{
		if( (*cmd)[cmdIndex] == 0x7E)
		{
			counter7E++;
			//shift all elements of array to the left
			(*length) -= 1;
			unsigned int shiftIndex ;
			for( shiftIndex = cmdIndex ; shiftIndex < (*length) ; shiftIndex++ )
				(*cmd)[ shiftIndex ] = (*cmd)[ shiftIndex + 1 ] ;
			(*cmd) = realloc( (*cmd) , (*length)*sizeof(unsigned char) );
		}
		cmdIndex++;
	}

	if (counter7E == 2 && (*length) >= (4+2+1+1) )
		res = OK;
	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_RemoveQuotes() exit. New cmd length=%d", *length);
	return res;
}

//
//
//

void MAYK_Gniffuts( unsigned char ** cmd , unsigned int * length )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Gniffuts() start. Old cmd length=%d", *length);

	if ( ( (*cmd) == NULL ) || ( (*length) == 0 ) )
			return ;

	unsigned int oldIndex = 0 ;
	unsigned int newIndex = 0 ;
	unsigned int stuffCounter = 0 ;

	while( oldIndex < (*length) )
	{
		if( (*cmd)[ oldIndex ] == 0x5D )
		{
			++oldIndex ;
			++stuffCounter;
			(*cmd)[ newIndex++ ] = (*cmd)[ oldIndex++ ] ^ 0x20 ;
		}
		else
		{
			(*cmd)[ newIndex++ ] = (*cmd)[ oldIndex++ ];
		}
	}

	if ( stuffCounter > 0 )
	{
		(*length) -= stuffCounter ;
		(*cmd) = realloc( (*cmd) , (*length) );
		if( (*cmd) == NULL)
		{
			printf("GNUFFITS ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}

	}

	//COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Gniffuts() exit. New cmd length=%d", *length);
	return ;
}

//
//
//

BOOL MAYK_GetCounterAddress(counter_t * counter, unsigned int * counterAddress)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	BOOL res = TRUE;

	*counterAddress = ( counter->serial & 0xFFFFFFFF );

	return res;
}

//
//
//

int MAYK_GetDateAndTime(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Command_GetDateAndTime() start.");
	int res = ERROR_GENERAL;

	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_DATE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_DATE ) & 0xFF;

	unsigned char crc = MAYK_GetCRC(tempCmd , tempCmdLength);
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_DATE_TIME_STAMP;
	(*transaction)->waitSize = 15;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET TIME AND DATE: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_Command_GetDateAndTime() exit.");
	return res;
}

//
//
//
/*
int MAYK_GetType(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_TYPE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_TYPE ) & 0xFF;

	tempCmd[ tempCmdLength++ ] = MAYK_GetCRC( tempCmd , tempCmdLength );
	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_READ_DEVICE_SW_VERSION ;
	(*transaction)->waitSize = 21;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_AddQuotes( (*transaction)->command , &((*transaction)->commandSize) );
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	return res ;
}
*/
//
//
//

int MAYK_GetRatio(counter_t * counter , counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetRatio() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_RATIO >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_RATIO ) & 0xFF;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;
	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_GET_RATIO ;
	(*transaction)->waitSize = 1+/*crc*/ +4/*sn*/+2/*cmd*/+1/*res*/+4/*type*/+7/*produce dts*/+4/*ratio*/+1/*summer time flag*/;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET RATIO: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetRatio() exit.");
	return res ;



}


//
//
//


int MAYK_GetCurrentMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tarifMask)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetCurrentMeterage() start with tarif mask [%02X]." , tarifMask );
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_CURRENT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_CURRENT ) & 0xFF;

	//set tarif index
	tempCmd[ tempCmdLength++ ] = (unsigned char) tarifMask & MAX_TARIFF_MASK;

	//checking number of tarifs in request
	int tarifNumb = 0;
	unsigned char tarifNumbIndex = 0x01;
	for( tarifNumbIndex = 0x01 ; tarifNumbIndex <= MAX_TARIFF_MASK && tarifNumbIndex>0 ; tarifNumbIndex=tarifNumbIndex << 1)
	{
		if ((tarifMask & tarifNumbIndex) != 0x00)
			tarifNumb++;
	}

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_CURRENT ;
	(*transaction)->waitSize = 1/*crc*/ + 4/*sn*/ + 2/*cmd*/ + 1/*res*/ + 1/*tarMask*/ + (16 * tarifNumb);
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET CURRENT METRAGES: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetCurrentMeterage() exit.");
	return res ;
}

//
//
//

int MAYK_GetDayMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tariffMask ,dateStamp * date)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetDayMeterage() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	memcpy( &counter->dayMetaragesLastRequestDts , date , sizeof(dateStamp));

	//checking tariffs number in request
	int tarifNumb = 0;
	unsigned char tarifNumbIndex = 0x01;
	for( tarifNumbIndex = 0x01 ; tarifNumbIndex <= MAX_TARIFF_MASK && tarifNumbIndex>0 ; tarifNumbIndex=tarifNumbIndex << 1)
	{
		if ((tariffMask & tarifNumbIndex) != 0x00)
			tarifNumb++;
	}

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_METERAGE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_METERAGE ) & 0xFF;

	//set type mask
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0;

	//set arch type
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0;

	//set tarif mask
	tempCmd[ tempCmdLength++ ] = (unsigned char) tariffMask & MAX_TARIFF_MASK;

	//set begin dateStamp
	tempCmd[ tempCmdLength++ ] = date->d;
	tempCmd[ tempCmdLength++ ] = date->m;
	tempCmd[ tempCmdLength++ ] = date->y;

	//set end dateStamp
	tempCmd[ tempCmdLength++ ] = date->d;
	tempCmd[ tempCmdLength++ ] = date->m;
	tempCmd[ tempCmdLength++ ] = date->y;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;
	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_DAY ;
	(*transaction)->waitSize = 4/*SN*/ + 1 /*CRC*/ + 2/*CMD*/ + 1 /*ErrCode*/ + 1/*archNumb*/ + 1/*tarifMask*/ + (tarifNumb*16) + 3/*DStamp*/;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET DAY METRAGES: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetDayMeterage() exit.");

	return res ;
}


//
//
//

int MAYK_GetMonthMeterage(counter_t * counter, counterTransaction_t ** transaction , unsigned char tariffMask , dateStamp * date)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetMonthMeterage() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	memcpy( &counter->monthMetaragesLastRequestDts , date , sizeof(dateStamp));

	//checking tariffs number in request
	int tarifNumb = 0;
	unsigned char tarifNumbIndex = 0x01;
	for( tarifNumbIndex = 0x01 ; tarifNumbIndex <= MAX_TARIFF_MASK && tarifNumbIndex>0 ; tarifNumbIndex=tarifNumbIndex << 1)
	{
		if ((tariffMask & tarifNumbIndex) != 0x00)
			tarifNumb++;
	}

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_METERAGE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_METERAGE ) & 0xFF;

	//set type mask
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0;

	//set arch type
	tempCmd[ tempCmdLength++ ] = (unsigned char) 1;

	//set tarif mask
	tempCmd[ tempCmdLength++ ] = (unsigned char) tariffMask & MAX_TARIFF_MASK;

	//set begin dateStamp
	tempCmd[ tempCmdLength++ ] = date->d;
	tempCmd[ tempCmdLength++ ] = date->m;
	tempCmd[ tempCmdLength++ ] = date->y;

	//set end dateStamp
	tempCmd[ tempCmdLength++ ] = date->d;
	tempCmd[ tempCmdLength++ ] = date->m;
	tempCmd[ tempCmdLength++ ] = date->y;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;
	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_METERAGE_MONTH ;
	(*transaction)->waitSize = 4/*SN*/ + 1 /*CRC*/ + 2/*CMD*/ + 1 /*ErrCode*/ + 1/*archNumb*/ + 1/*tarifMask*/ + (tarifNumb*16) + 3/*DStamp*/;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET MONTH METRAGES: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetMonthMeterage() exit.");

	return res ;
}

//
//
//

int MAYK_GetProfileValue(counter_t * counter, counterTransaction_t ** transaction , dateTimeStamp * dts)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetProfileValue() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );
	dateTimeStamp dtsStop;

	memcpy( &dtsStop, dts, sizeof(dateTimeStamp));
	memcpy( &counter->profileLastRequestDts , dts , sizeof( dateTimeStamp )  );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_PROFILE  >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_PROFILE  ) & 0xFF;

	//set type mask
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;

	//set dts
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00; //seconds
	tempCmd[ tempCmdLength++ ] = (unsigned char) dts->t.m ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dts->t.h ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0xFF ; //day of week
	tempCmd[ tempCmdLength++ ] = (unsigned char) dts->d.d ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dts->d.m ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dts->d.y ;

	//addon for mayak-302 (read two profiles in one time)
	//do not change answer size if two slices will be readed
	//whole answer parsing will be done with unexpected answer size in saveTaskResults foo


	int readSlices = 1;
	if(counter->type == TYPE_MAYK_MAYK_302) {

		#if MAYAK_KALUJNIY_RECCOMEND

		dateTimeStamp dtsOIct2014;
		dtsOIct2014.d.y = 14;
		dtsOIct2014.d.m = 10;
		dtsOIct2014.d.d = 26;
		dtsOIct2014.t.h = 2;
		dtsOIct2014.t.m = 0;
		dtsOIct2014.t.s = 0;

		if (COMMON_DtsAreEqual(&dtsOIct2014, &dtsStop) == TRUE){
			readSlices = 5;
			COMMON_ChangeDts(&dtsStop, INC, BY_HOUR);

		} else {
			if (dts->t.m == 0){
				int halfHourDiff = COMMON_GetDtsDiff__Alt(&dtsStop, &counter->currentDts, BY_HALF_HOUR);
				if (halfHourDiff > 2){

					if (halfHourDiff > MAYK_MAX_HALF_HOURS_DIFF){
						halfHourDiff  = MAYK_MAX_HALF_HOURS_DIFF; 
					}
					
					readSlices = halfHourDiff;

					while (halfHourDiff > 1){
						COMMON_ChangeDts(&dtsStop, INC, BY_HALF_HOUR);
						halfHourDiff--;
					}
				}
			}
		}

		#else

		if (dts->t.m == 0)
		{
			int halfHourDiff = COMMON_GetDtsDiff__Alt(&dtsStop, &counter->currentDts, BY_HALF_HOUR);
			if (halfHourDiff > 2){
				if (halfHourDiff > MAYK_MAX_HALF_HOURS_DIFF){
					halfHourDiff  = MAYK_MAX_HALF_HOURS_DIFF; 
				}
				
				readSlices = halfHourDiff;

				while (halfHourDiff > 1){
					COMMON_ChangeDts(&dtsStop, INC, BY_HALF_HOUR);
					halfHourDiff--;
				}
				
			}
		}

		#endif
	}




	//set dts last
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00; //seconds
	tempCmd[ tempCmdLength++ ] = (unsigned char) dtsStop.t.m ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dtsStop.t.h ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0xFF ; //day of week
	tempCmd[ tempCmdLength++ ] = (unsigned char) dtsStop.d.d ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dtsStop.d.m ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) dtsStop.d.y ;

	//debug
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetProfileValue() read slices: %d", readSlices);
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetProfileValue() dts start " DTS_PATTERN, DTS_GET_BY_PTR(dts));
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetProfileValue() dts stop  " DTS_PATTERN, DTS_GET_BY_VAL(dtsStop));

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_PROFILE_INTERVAL ;
	(*transaction)->waitSize = 4/*SN*/ + 1 /*CRC*/ + 2/*CMD*/ + 1 /*ErrCode*/ + 7  /*DTS*/ + 33 /*PPR*/;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->answerSize = 0 ;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET PROFILE VALUE: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetProfileValue() exit.");



	return res ;
}


//
//
//

int MAYK_SyncTimeHardy(counter_t * counter, counterTransaction_t ** transaction )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SyncTimeHardy() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	int weekday = 0 ;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SYNC_HARDY >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SYNC_HARDY ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];
	//add dts

	dateTimeStamp dtsToSet;
	COMMON_GetCurrentDts( &dtsToSet );

	int i = 0;
	for( ; i < ( counter->transactionTime / 2 ) ; ++i )
		COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );

	tempCmd[ tempCmdLength++ ] = dtsToSet.t.s ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.t.m ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.t.h ;

	weekday = COMMON_GetWeekDayByDts( &dtsToSet ) + 1 ;
	if ( weekday == 7 )
		weekday = 1 ;

	tempCmd[ tempCmdLength++ ] =  weekday ;

	tempCmd[ tempCmdLength++ ] = dtsToSet.d.d ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.d.m ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.d.y ;



	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_SYNC_HARDY ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "SYNC TIME HARDY: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SyncTimeHardy() exit.");
	return res ;


}
//
//
//

int MAYK_SyncTimeSoft(counter_t * counter, counterTransaction_t ** transaction )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SyncTimeSoft() start.");
	int res = ERROR_GENERAL;
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	int weekday = 0 ;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SYNC_SOFT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SYNC_SOFT ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];
	//add dts
	dateTimeStamp dtsToSet;
	COMMON_GetCurrentDts( &dtsToSet );

	int i = 0;
	for( ; i < ( counter->transactionTime / 2 ) ; ++i )
		COMMON_ChangeDts( &dtsToSet , INC , BY_SEC );

	tempCmd[ tempCmdLength++ ] = dtsToSet.t.s ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.t.m ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.t.h ;

	weekday = COMMON_GetWeekDayByDts( &dtsToSet ) + 1 ;
	if ( weekday == 7 )
		weekday = 1 ;

	tempCmd[ tempCmdLength++ ] =  weekday ;

	tempCmd[ tempCmdLength++ ] = dtsToSet.d.d ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.d.m ;
	tempCmd[ tempCmdLength++ ] = dtsToSet.d.y ;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_SYNC_SOFT ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "SYNC TIME SOFT: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;
	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SyncTimeSoft() exit.");
	return res ;


}

//
//
//
/**
  Reading journal
  */

int MAYK_AskEventsNumberInCurrentJournal(counter_t * counter, counterTransaction_t ** transaction , int journalNumber )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AskEventsNumberInCurrentJournal() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_EVENTS_NUMB >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_EVENTS_NUMB ) & 0xFF;

	//set jourenal number
	tempCmd[ tempCmdLength++ ] = (unsigned char)(journalNumber & 0xFF);

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_GET_EVENTS_NUMBER ;
	(*transaction)->waitSize = 1/*crc*/ + 4/*sn*/ + 2/*cmd*/ + 1/*res*/ + 1/*journal index*/ + 2/*items count*/;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "ASK EVENT NUMBER IN CURRENT JOURNAL: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AskEventsNumberInCurrentJournal() exit with res = [%d].",res);
	return res ;
}

//
//
//

int MAYK_GetNextEventFromCurrentJournal(counter_t * counter, counterTransaction_t ** transaction , int journalNumber , int eventNumber )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AskEventsNumberInCurrentJournal() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_EVENT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_EVENT ) & 0xFF;

	//set journal number
	tempCmd[ tempCmdLength++ ] = (unsigned char)(journalNumber & 0xFF);

	//set event number
	tempCmd[ tempCmdLength++ ] = (unsigned char)((eventNumber >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char)(eventNumber & 0xFF);

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	int journalLength = 0 ;
	switch(journalNumber)
	{

		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			journalLength = 14 ;
			break;

		case 5:
			journalLength = 7 ;
			break;

		case 6:
			journalLength = 28 ;
			break;

		case 8:
			journalLength = 12 ;
			break;

		default:
			break;
	}

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_GET_EVENT ;
	(*transaction)->waitSize = 1/*crc*/ + 4/*sn*/ + 2/*cmd*/ + 1/*res*/ + 1/*journal type*/ + 2/*item index*/ + journalLength;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET NEXT EVENT FROM CURRENT JOURNAL: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_AskEventsNumberInCurrentJournal() exit with res = [%d].",res);
	return res ;
}

//
//
//


int MAYK_GetPowerQualityParametres(counter_t * counter, counterTransaction_t ** transaction , int pqpPart)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetPowerQualityParametres() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_PQP >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_PQP ) & 0xFF;

	int pqpLength = 0 ;
	switch(pqpPart)
	{
		case PQP_STATE_READING_VOLTAGE:
			tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00 ;
			(*transaction)->transactionType = TRANSACTION_GET_PQP_VOLTAGE_GROUP ;
			pqpLength = 12;
			break;

		case PQP_STATE_READING_AMPERAGE:
			tempCmd[ tempCmdLength++ ] = (unsigned char) 0x01 ;
			(*transaction)->transactionType = TRANSACTION_GET_PQP_AMPERAGE_GROUP ;
			pqpLength = 12;
			break;

		case PQP_STATE_READING_POWER:
			tempCmd[ tempCmdLength++ ] = (unsigned char) 0x02 ;
			(*transaction)->transactionType = TRANSACTION_GET_PQP_POWER_ALL ;
			pqpLength = 48;
			break;

		case PQP_STATE_READING_FREQUENCY:
			tempCmd[ tempCmdLength++ ] = (unsigned char) 0x03 ;
			(*transaction)->transactionType = TRANSACTION_GET_PQP_FREQUENCY ;
			pqpLength = 4;
			break;

		case PQP_STATE_READING_RATIO:
			tempCmd[ tempCmdLength++ ] = (unsigned char) 0x04 ;
			(*transaction)->transactionType = TRANSACTION_GET_PQP_RATIO ;
			pqpLength = 24;
			break;

		default:
			break;
	}

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes



	(*transaction)->waitSize = 1/*crc*/ + 4/*sn*/ + 2/*cmd*/ + 1/*res*/ + 1/*pqp type*/ + pqpLength;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET PQP: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_GetPowerQualityParametres() exit with res = [%d].",res);

   return res;
}

//
//
//

int MAYK_CmdSetPowerControls( counter_t * counter, counterTransaction_t ** transaction , int offset , unsigned char * controls , int size )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControls() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[128];
	memset( tempCmd , 0 , 128 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SET_POWER_CTRL >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SET_POWER_CTRL ) & 0xFF;

	//set passord
	memcpy( &tempCmd[ tempCmdLength ] , counter->password1 , PASSWORD_SIZE ) ;
	tempCmdLength += PASSWORD_SIZE ;

	//set type
	tempCmd[ tempCmdLength++ ] = (unsigned char)0x01  ;

	//set offset
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((offset >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( offset ) & 0xFF;

	//set size
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((size >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( size ) & 0xFF;

	//set data
	memcpy( &tempCmd[ tempCmdLength ] , controls , size ) ;
	tempCmdLength += size ;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_MAYK_SET_POWER_CONTROLS ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControls() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_CmdSetPowerControlsDefault( counter_t * counter, counterTransaction_t ** transaction  )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControlsDefault() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[128];
	memset( tempCmd , 0 , 128 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SET_POWER_CTRL >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SET_POWER_CTRL ) & 0xFF;

	//set passord
	memcpy( &tempCmd[ tempCmdLength ] , counter->password1 , PASSWORD_SIZE ) ;
	tempCmdLength += PASSWORD_SIZE ;

	//set type
	tempCmd[ tempCmdLength++ ] = (unsigned char)0x00  ;

	//set offset
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;

	//set size
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_MAYK_SET_POWER_CONTROLS_DEFAULT ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControlsDefault() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_CmdSetPowerControlsDone( counter_t * counter, counterTransaction_t ** transaction  )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControlsDone() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[128];
	memset( tempCmd , 0 , 128 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SET_POWER_CTRL >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SET_POWER_CTRL ) & 0xFF;

	//set passord
	memcpy( &tempCmd[ tempCmdLength ] , counter->password1 , PASSWORD_SIZE ) ;
	tempCmdLength += PASSWORD_SIZE ;

	//set type
	tempCmd[ tempCmdLength++ ] = (unsigned char)0x02  ;

	//set offset
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;

	//set size
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;
	tempCmd[ tempCmdLength++ ] = (unsigned char) 0x00;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_MAYK_SET_POWER_CONTROLS_DONE ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControlsDone() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_CmdSetPowerModuleSwitcherState( counter_t * counter, counterTransaction_t ** transaction , int state  )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerControlsDone() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[128];
	memset( tempCmd , 0 , 128 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SET_POWER_SWITCHER_STATE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SET_POWER_SWITCHER_STATE ) & 0xFF;

	//set passord
	memcpy( &tempCmd[ tempCmdLength ] , counter->password1 , PASSWORD_SIZE ) ;
	tempCmdLength += PASSWORD_SIZE ;

	//set mode
	tempCmd[ tempCmdLength++ ] = (unsigned char)state  ;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_MAYK_POWER_SWITCHER_STATE_SET ;
	(*transaction)->waitSize = /*sn*/4+/*cmd*/2+/*res*/1+/*crc*/1;;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdSetPowerModuleSwitcherState() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_BeginWritingTariffMap(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_BeginWritingTariffMap() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_BEGIN_TARIFF_WRITING >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_BEGIN_TARIFF_WRITING ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_BEGIN_WRITING_TARIFF_MAP ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "BEGIN WRITING TARIFF MAP: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_BeginWritingTariffMap() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_EndWritingTariffMap(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_EndWritingTariffMap() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_END_TARIFF_WRITING >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_END_TARIFF_WRITING ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_END_WRITING_TARIFF_MAP ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "END WRITING TARIFF MAP: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_EndWritingTariffMap() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_CmdStartWritingIndications(counter_t * counter, counterTransaction_t ** transaction , maykIndication_t * maykIndication)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdStartWritingIndications() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_WRITING_INDICATIONS >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_WRITING_INDICATIONS ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	//set index
	tempCmd[ tempCmdLength++ ] = 0xFF ;

	//set time auto switch loops
	tempCmd[ tempCmdLength++ ] = maykIndication->loopAutoSwitchTime ;

	//set max loops number
	tempCmd[ tempCmdLength++ ] = (unsigned char)maykIndication->loopCounter ;

	int loopIndex = 0 ;
	for ( ; loopIndex < maykIndication->loopCounter ; ++loopIndex)
	{
		tempCmd[ tempCmdLength++ ] = maykIndication->loop[ loopIndex ].length ;
	}

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_START_WRITING_INDICATIONS ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "END START WRITING INDICATIONS: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdStartWritingIndications() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_CmdWritingIndicationsLoop(counter_t * counter, counterTransaction_t ** transaction , int loopIndex , maykIndicationLoop_t * maykIndicationLoop)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdStartWritingIndications() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_WRITING_INDICATIONS >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_WRITING_INDICATIONS ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	//set loop index
	tempCmd[ tempCmdLength++ ] = loopIndex ;

	//set loop memebers
	int loopMembersLength = maykIndicationLoop->length ;
	if ( loopMembersLength > MAYK_INDICATIONS_MAX_VALUE )
	{
		loopMembersLength = MAYK_INDICATIONS_MAX_VALUE ;
	}
	memcpy( &tempCmd[ tempCmdLength ] , maykIndicationLoop->values , loopMembersLength );
	tempCmdLength += loopMembersLength ;

	//calc CRC
	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_INDICATIONS ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "END START WRITING INDICATIONS: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdStartWritingIndications() exit with res = [%d].",res);

	return res ;
}

//
//
//

#if REV_COMPILE_ASK_MAYK_FRM_VER
int MAYK_CmdGetVersion(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_MAYAK);

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdGetVersion() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_GET_VERSION >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_GET_VERSION ) & 0xFF;

	//calc CRC
	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_READ_DEVICE_SW_VERSION ;
	(*transaction)->waitSize = /*sn*/4  +  /*cmd*/2  +  /*res*/1  +  /*VER_PRTCL*/4  +  /*TYPE_MTR*/4  +  /*VER_FRM*/4  +  /*VER_TECH_FRM*/4  +  /*CRC_FRM*/4  +  /*NAME_FRM 2...32*/2  +  /*crc*/1;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "GET VERSION: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdGetVersion() exit with res = [%d].",res);

	return res ;
}
#endif

//
//
//

int MAYK_WritingTariffMap_STT(counter_t * counter, counterTransaction_t ** transaction , maykTariffSTT_t * stt, int sttIndex)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_STT() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[256];
	memset( tempCmd , 0 , 256 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_TARIFF_WRITING_STT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_TARIFF_WRITING_STT ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	//set STT index
	tempCmd[ tempCmdLength++ ] = sttIndex ;

	//set TP number
	tempCmd[ tempCmdLength++ ] = stt->tpNumb;

	int tpIndex = 0 ;
	for(;tpIndex < stt->tpNumb ; tpIndex++)
	{
		tempCmd[ tempCmdLength++ ] = stt->tp[ tpIndex ].ts.s ;
		tempCmd[ tempCmdLength++ ] = stt->tp[ tpIndex ].ts.m ;
		tempCmd[ tempCmdLength++ ] = stt->tp[ tpIndex ].ts.h ;
		tempCmd[ tempCmdLength++ ] = stt->tp[ tpIndex ].tariffNumber ;
	}

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_TARIFF_STT ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "WRITING STT: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_STT() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_WritingTariffMap_WTT(counter_t * counter, counterTransaction_t ** transaction , maykTariffWTT_t * wtt, int wttIndex)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_WTT() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_TARIFF_WRITING_WTT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_TARIFF_WRITING_WTT ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	//set WTT index
	tempCmd[ tempCmdLength++ ] = wttIndex ;

	//set wtt
	tempCmd[ tempCmdLength++ ] = wtt->weekDaysStamp ;
	tempCmd[ tempCmdLength++ ] = wtt->weekEndStamp ;
	tempCmd[ tempCmdLength++ ] = wtt->holidayStamp ;
	tempCmd[ tempCmdLength++ ] = wtt->shaginMindStamp ;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_TARIFF_WTT ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "WRITING WTT: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_WTT() exit with res = [%d].",res);

	return res ;
}


//
//
//

int MAYK_WritingTariffMap_MTT(counter_t * counter, counterTransaction_t ** transaction , maykTariffMTT_t * mtt)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_MTT() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_TARIFF_WRITING_MTT >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_TARIFF_WRITING_MTT ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	int mttIndex = 0 ;
	for(;mttIndex < 12 ; mttIndex++)
		tempCmd[ tempCmdLength++ ] = mtt->wttNumb[ mttIndex ];

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_TARIFF_MTT ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "WRITING WTT: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_MTT() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_WritingTariffMap_LWJ(counter_t * counter, counterTransaction_t ** transaction , maykTariffLWJ_t * lwj)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_LWJ() start.");
	unsigned int counterAddress = 0;
	unsigned char tempCmd[256];
	memset( tempCmd , 0 , 256 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_TARIFF_WRITING_LWJ >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_TARIFF_WRITING_LWJ ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	tempCmd[ tempCmdLength++ ] = 0 ;
	tempCmd[ tempCmdLength++ ] = lwj->mdNumb ;

	int mdIndex = 0 ;
	for( ; mdIndex < lwj->mdNumb ; mdIndex++)
	{
		tempCmd[ tempCmdLength++ ] = lwj->md[ mdIndex ].d;
		tempCmd[ tempCmdLength++ ] = lwj->md[ mdIndex ].m;
		tempCmd[ tempCmdLength++ ] = lwj->md[ mdIndex ].t;
	}

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_WRITING_TARIFF_LWJ ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "WRITING LWJ: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritingTariffMap_LWJ() exit with res = [%d].",res);

	return res ;
}

//
//
//

int MAYK_WriteTariff( counter_t * counter , counterTask_t ** counterTask , unsigned char * tariff , int tariffLength)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	maykTariffSTT_t * stt = NULL ;
	int sttLength = 0 ;
	maykTariffWTT_t * wtt = NULL;
	int wttLength = 0 ;
	maykTariffMTT_t mtt;
	memset(&mtt , 0 , sizeof(maykTariffMTT_t));
	maykTariffLWJ_t lwj;
	memset(&lwj , 0 , sizeof(maykTariffLWJ_t));

	maykIndication_t maykIndication ;
	maykIndication.loop = NULL ;
	maykIndication.loopCounter = 0 ;
	maykIndication.loopAutoSwitchTime = 0 ;

	int indicationParsingRes = MAYK_GetIndications( counter->type , tariff , tariffLength , &maykIndication ) ;
	if ( (MAYK_GetSTT(tariff , tariffLength , &stt , &sttLength) == OK) && \
		 (MAYK_GetWTT(tariff , tariffLength , &wtt , &wttLength ) == OK) && \
		 (MAYK_GetMTT(tariff , tariffLength , &mtt) == OK) && \
		 (MAYK_GetLWJ(tariff , tariffLength , &lwj) == OK) && \
		 (indicationParsingRes == OK ))
	{
		if (stt && wtt)
		{


			//begin writing tariff map

			res = MAYK_BeginWritingTariffMap( counter, COMMON_AllocTransaction(counterTask) );

			//writing stt's
			int idx = 0 ;
			for( ; idx < sttLength ; idx++ )
			{
				if(res == OK)
				{
					res = MAYK_WritingTariffMap_STT( counter, COMMON_AllocTransaction(counterTask) , &stt[idx] , idx );
				}
			}

			//writing wtt's

			for(idx = 0 ; idx < wttLength ; idx++ )
			{
				if(res == OK)
				{
					res = MAYK_WritingTariffMap_WTT( counter, COMMON_AllocTransaction(counterTask) , &wtt[idx] , idx );
				}
			}

			//writing mtt
			if(res == OK)
			{
				res = MAYK_WritingTariffMap_MTT( counter, COMMON_AllocTransaction(counterTask) , &mtt );
			}


			//writing lwj
			if ( lwj.mdNumb > 0 )
			{
				if(res == OK)
				{
					res = MAYK_WritingTariffMap_LWJ( counter, COMMON_AllocTransaction(counterTask) , &lwj );
				}
			}

			//end writing tariff map
			if(res == OK)
			{
				res = MAYK_EndWritingTariffMap( counter, COMMON_AllocTransaction(counterTask));
			}

			if ( maykIndication.loopCounter > 0 )
			{
				//start writing indications

				if(res == OK)
				{
					res = MAYK_CmdStartWritingIndications( counter, COMMON_AllocTransaction(counterTask) , &maykIndication);
					(*counterTask)->indicationWritingTransactionCounter++;
				}

				//writing indications

				int loopIndex = 0 ;
				for ( ; loopIndex < maykIndication.loopCounter ; ++loopIndex)
				{
					res = MAYK_CmdWritingIndicationsLoop( counter, COMMON_AllocTransaction(counterTask) , loopIndex , &maykIndication.loop[ loopIndex ]);
					(*counterTask)->indicationWritingTransactionCounter++;
				}
			}

			free(stt);
			free(wtt);

			if ( maykIndication.loop )
			{
				int loopIndex = 0 ;
				for ( ;loopIndex < maykIndication.loopCounter ; ++loopIndex  )
				{
					if ( maykIndication.loop[ loopIndex ].values )
					{
						free(maykIndication.loop[ loopIndex ].values);
						maykIndication.loop[ loopIndex ].values = NULL ;
						maykIndication.loop[ loopIndex ].length = 0 ;
					}
				}

				free( maykIndication.loop );
				maykIndication.loop = NULL ;
				maykIndication.loopCounter = 0 ;
			}
		}
	}
	else
	{
		if ( indicationParsingRes != OK )
		{
			res = indicationParsingRes ;
		}
		else
		{
			res = ERROR_FILE_FORMAT ;
		}
		if (stt)
			free(stt);
		if (wtt)
			free(wtt);
	}



	return res ;
}

//
//
//

int MAYK_WritePSM( counter_t * counter , counterTask_t ** counterTask , unsigned char * powerSwithModuleRules , int powerSwithModuleRulesLength)
{
	DEB_TRACE(DEB_IDX_MAYAK);

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM start");

	int res = ERROR_GENERAL ;

	int powermanState = -1;

	enum {  PARSING_COUNTER_TYPE ,
			SET_DEFAULT_RULES ,
			PARSING_POWER_SWITCH_MODES ,
			PARSING_POWER_SWITCH_LIMITS_PQ ,
			PARSING_POWER_SWITCH_LIMITS_E30 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T1 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T2 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T3 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T4 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T5 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T6 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T7 ,
			PARSING_POWER_SWITCH_LIMITS_ED_T8 ,
			PARSING_POWER_SWITCH_LIMITS_ED_TS ,
			PARSING_POWER_SWITCH_LIMITS_EM_T1 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T2 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T3 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T4 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T5 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T6 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T7 ,
			PARSING_POWER_SWITCH_LIMITS_EM_T8 ,
			PARSING_POWER_SWITCH_LIMITS_EM_TS ,
			PARSING_POWER_SWITCH_UI ,
			PARSING_POWER_SWITCH_TIMINGS ,
			PARSING_STATE_CAST_FINISH_CMD ,
			PARSING_STATE_DONE} ;

	mayk_PSM_Parametres_t mayk_PSM_Parametres ;
	memset( &mayk_PSM_Parametres , 0 , sizeof(mayk_PSM_Parametres_t) );

	blockParsingResult_t * blockParsingResult = NULL ;
	int blockParsingResultLength = 0 ;

	unsigned char dataToSet[MAYK_DATA_TO_SET_SIZE];

	int parsingState = PARSING_COUNTER_TYPE;
	while(parsingState != PARSING_STATE_DONE)
	{
		switch( parsingState )
		{
			case PARSING_COUNTER_TYPE:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_COUNTER_TYPE");
				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				//searching for counter type in text file

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "counter", &blockParsingResult , &blockParsingResultLength ) == OK )
				{

					if ( blockParsingResult )
					{

						int counterTypePropertyIndex = -1 ;
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "model" ) )
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM model was found");
								counterTypePropertyIndex = propertyIndex ;
								break;
							}
						}

						if ( counterTypePropertyIndex >= 0 )
						{
							if ( atoi( blockParsingResult[ counterTypePropertyIndex ].value ) == counter->type )
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM model is equal");
								parsingState = SET_DEFAULT_RULES ;
							}
							else
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM model is not equal [%d]" , blockParsingResult[ counterTypePropertyIndex ].value);
							}
						}



						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if( parsingState == PARSING_COUNTER_TYPE )
				{
					res = ERROR_FILE_FORMAT ;
					parsingState = PARSING_STATE_DONE ;
				}

			}
			break;

			//
			//
			//

			case SET_DEFAULT_RULES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is SET_DEFAULT_RULES");

				res = MAYK_CmdSetPowerControlsDefault( counter, COMMON_AllocTransaction(counterTask) );


				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_MODES ;
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

			case PARSING_POWER_SWITCH_MODES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_MODES");
				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "power_switch_modes", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						//searching for essential properties

						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskPowerOffOtherCases" ) )
							{
								mayk_PSM_Parametres.maskOther = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_OTHER , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{									
									res = MAYK_SetPowerMasks1( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskGivenLevelsOvershoot" ) )
							{
								mayk_PSM_Parametres.maskRules = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_RULES , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMasks2( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskSwitchOffRelayAtEventUmax" ) )
							{
								mayk_PSM_Parametres.maskUmax = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_UMAX , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskUmax( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}


							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskSwitchOffRelayAtEventUmin" ) )
							{
								mayk_PSM_Parametres.maskUmin = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_UMIN , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskUmin( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskSwitchOffRelayAtEventImax" ) )
							{
								mayk_PSM_Parametres.maskImax = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_IMAX , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskImax( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( !strncmp( blockParsingResult[ propertyIndex ].variable , "maskPowerOffPQ", strlen("maskPowerOffPQ") ) )
							{
								mayk_PSM_Parametres.maskPq = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_PQ , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskPq( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskE30" ) )
							{
								mayk_PSM_Parametres.maskE30 = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_E30 , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskE30( counter, COMMON_AllocTransaction(counterTask) , dataToSet);

								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskEDay" ) )
							{
								mayk_PSM_Parametres.maskEd = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_ED , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{
									res = MAYK_SetPowerMaskEd( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "maskEMonth" ) )
							{
								mayk_PSM_Parametres.maskEm = (int)strtoul( blockParsingResult[ propertyIndex ].value , NULL , 16);

								memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
								MAYK_FillAsByteArray(MAYK_PC_FLD_MASK_EM , dataToSet , &mayk_PSM_Parametres);

								if ( res == OK )
								{									
									res = MAYK_SetPowerMaskEm( counter, COMMON_AllocTransaction(counterTask) , dataToSet);
								}
								else
								{
									break;
								}

							}

							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "setLoadPowerOffState" ) )
							{
								powermanState = atoi( blockParsingResult[ propertyIndex ].value );
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}


				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_PQ ;
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

			case PARSING_POWER_SWITCH_LIMITS_PQ:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_PQ");
				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int powerLimitsFlag = 0 ;

				// searching for powerLimits

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "powerLimits.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsPq[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsPq[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsPq[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsPq[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "powerLimits.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsPq[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsPq[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsPq[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsPq[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "powerLimits.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsPq[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsPq[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsPq[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsPq[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "powerLimits.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsPq[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsPq[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsPq[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsPq[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) * 1000 ;
								powerLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if (powerLimitsFlag != 0)
				{					
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_PQ , dataToSet , &mayk_PSM_Parametres);

					res = MAYK_SetPowerLimitsPq( counter, COMMON_AllocTransaction(counterTask) , dataToSet ) ;
				}


				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_E30 ;
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

			case PARSING_POWER_SWITCH_LIMITS_E30:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_E30");
				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int e30LimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "E30_limits.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsE30[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsE30[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsE30[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsE30[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "E30_limits.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsE30[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsE30[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsE30[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsE30[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "E30_limits.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsE30[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsE30[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsE30[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsE30[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "E30_limits.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsE30[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsE30[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsE30[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsE30[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsE30[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsE30[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								e30LimitsFlag = 1 ;
							}
						}

						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}


				if ( e30LimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_E30 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_E30 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_E30 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T1 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T1:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T1");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T1.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T1.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T1.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T1.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT1[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 1 , dataToSet , &mayk_PSM_Parametres);

					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T1 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T1 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T2 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T2:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T2");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T2.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T2.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T2.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T2.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT2[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 2 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T2 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T2 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T3 ;
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
			//

			case PARSING_POWER_SWITCH_LIMITS_ED_T3:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T3");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T3.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T3.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T3.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T3.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT3[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 3 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T3 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T3 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T4 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T4:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T4");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T4.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T4.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T4.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T4.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT4[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 4 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T4 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T4 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T5 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T5:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T5");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T5.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T5.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T5.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T5.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT5[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 5 , dataToSet , &mayk_PSM_Parametres);

					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T5 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T5 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T6 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T6:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T6");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T6.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T6.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T6.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T6.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT6[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 6 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T6 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T6 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T7 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T7:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T7");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T7.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T7.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T7.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T7.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT7[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 7 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T7 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T7 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_T8 ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_T8:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_T8");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T8.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T8.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T8.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.T8.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdT8[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 8 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T8 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_T8 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_ED_TS ;
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

			case PARSING_POWER_SWITCH_LIMITS_ED_TS:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_ED_TS");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int edLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.TS.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.TS.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.TS.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "day_limits.TS.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEdTS[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								edLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( edLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_ED + 9 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_TS , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_ED_TS + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T1 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T1:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T1");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T1.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T1.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T1.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T1.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT1[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 1 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T1 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T1 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T2 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T2:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T2");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T2.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T2.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T2.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T2.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT2[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 2 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T2 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T2 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T3 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T3:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T3");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T3.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T3.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T3.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T3.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT3[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 3 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T3 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T3 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T4 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T4:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T4");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T4.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T4.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T4.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T4.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT4[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 4 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T4 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T4 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T5 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T5:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T5");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T5.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T5.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T5.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T5.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT5[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 5 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T5 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T5 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T6 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T6:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T6");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T6.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T6.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T6.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T6.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT6[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 6 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T6 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T6 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T7 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T7:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T7");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T7.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T7.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T7.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T7.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT7[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 7 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T7 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T7 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_T8 ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_T8:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_T8");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T8.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T8.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T8.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.T8.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmT8[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 8 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T8 , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_T8 + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_LIMITS_EM_TS ;
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

			case PARSING_POWER_SWITCH_LIMITS_EM_TS:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_LIMITS_EM_TS");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int emLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.TS.phaseA", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[0][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[0][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[0][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[0][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.TS.phaseB", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[1][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[1][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[1][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[1][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.TS.phaseC", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[2][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[2][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[2][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[2][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "month_limits.TS.phaseS", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "A+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[3][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "A-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[3][1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R+" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[3][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "R-" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[3][3] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|A|" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[4][0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "|R|" ) )
							{
								mayk_PSM_Parametres.limitsEmTS[4][2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								emLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( emLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_EM + 9 , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_TS , &dataToSet[0] , 40 ) ;


					if ( res == OK )
					{
						res = MAYK_CmdSetPowerControls( counter, COMMON_AllocTransaction(counterTask) , MAYK_MCPCR_OFFSET_LIMITS_EM_TS + 40 , &dataToSet[40] , 40 ) ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_UI ;
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

			case PARSING_POWER_SWITCH_UI:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_UI");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				int uiLimitsFlag = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "threshold_Umin", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseA" ) )
							{
								mayk_PSM_Parametres.limitsUmin[0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseB" ) )
							{
								mayk_PSM_Parametres.limitsUmin[1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseC" ) )
							{
								mayk_PSM_Parametres.limitsUmin[2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "threshold_Umax", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseA" ) )
							{
								mayk_PSM_Parametres.limitsUmax[0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseB" ) )
							{
								mayk_PSM_Parametres.limitsUmax[1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseC" ) )
							{
								mayk_PSM_Parametres.limitsUmax[2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}

				}
				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "threshold_Imax", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseA" ) )
							{
								mayk_PSM_Parametres.limitsImax[0] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseB" ) )
							{
								mayk_PSM_Parametres.limitsImax[1] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "phaseC" ) )
							{
								mayk_PSM_Parametres.limitsImax[2] = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								uiLimitsFlag = 1 ;
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( uiLimitsFlag != 0 )
				{
					memset(dataToSet, 0, MAYK_DATA_TO_SET_SIZE);
					MAYK_FillAsByteArray(MAYK_PC_FLD_LIMIT_UI , dataToSet , &mayk_PSM_Parametres);


					res = MAYK_SetPowerLimitsUI( counter, COMMON_AllocTransaction(counterTask) , &dataToSet[0] ) ;

				}

				if ( res == OK )
				{
					parsingState = PARSING_POWER_SWITCH_TIMINGS ;
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

			case PARSING_POWER_SWITCH_TIMINGS:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_POWER_SWITCH_TIMINGS");

				//clear blockParsingResult array
				if ( blockParsingResult )
				{
					free(blockParsingResult);
				}
				blockParsingResult = NULL ;
				blockParsingResultLength = 0 ;

				if ( COMMON_SearchBlock( (char *)powerSwithModuleRules , powerSwithModuleRulesLength , "time_to_go_out_of_limits", &blockParsingResult , &blockParsingResultLength ) == OK )
				{
					if ( blockParsingResult )
					{
						int propertyIndex = 0 ;
						for( ; propertyIndex < blockParsingResultLength ; ++propertyIndex)
						{
							if ( strstr( blockParsingResult[ propertyIndex ].variable , "Power" ) )
							{
								mayk_PSM_Parametres.tP = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									res = MAYK_SetTimingsMR_P( counter, COMMON_AllocTransaction(counterTask) , (unsigned int)mayk_PSM_Parametres.tP ) ;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "Umin" ) )
							{
								mayk_PSM_Parametres.tMin = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									res = MAYK_SetTimingsUmin( counter, COMMON_AllocTransaction(counterTask) , (unsigned int)mayk_PSM_Parametres.tMin ) ;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "UImax" ) )
							{
								mayk_PSM_Parametres.tMax = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									res = MAYK_SetTimingsUmax( counter, COMMON_AllocTransaction(counterTask) , (unsigned int)mayk_PSM_Parametres.tMax ) ;
								}
							}
							else if ( strstr( blockParsingResult[ propertyIndex ].variable , "delayBeforePowerOff" ) )
							{
								mayk_PSM_Parametres.tOff = atoi ( blockParsingResult[ propertyIndex ].value ) ;
								if ( res == OK )
								{
									res = MAYK_SetTimingsDelayOff( counter, COMMON_AllocTransaction(counterTask) , (unsigned int)mayk_PSM_Parametres.tOff ) ;
								}
							}
						}
						free(blockParsingResult);
						blockParsingResult = NULL ;
						blockParsingResultLength = 0 ;
					}
				}

				if ( res == OK )
				{
					parsingState = PARSING_STATE_CAST_FINISH_CMD ;
				}
				else
				{
					parsingState = PARSING_STATE_DONE ;
				}

			}
			break;

			case PARSING_STATE_CAST_FINISH_CMD:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_WritePSM parsing state is PARSING_STATE_CAST_FINISH_CMD");

				if ( res == OK )
				{
					res = MAYK_CmdSetPowerControlsDone( counter, COMMON_AllocTransaction(counterTask) ) ;

				}

				if ( ( res == OK ) && ( powermanState >= 0 ) )
				{
					res = MAYK_CmdSetPowerModuleSwitcherState( counter, COMMON_AllocTransaction(counterTask) , powermanState  );
				}

				parsingState = PARSING_STATE_DONE ;
			}
			break;

			default:
				parsingState = PARSING_STATE_DONE ;
				break;
		}
	}

	return res ;
}

//
//
//

int MAYK_CmdAllowSeasonChange(counter_t * counter , counterTransaction_t ** transaction , int flag){
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdAllowSeasonChange(%d) start.", flag);
	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((MAYK_CMD_SEASON_CHANGE >> 8) & 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ( MAYK_CMD_SEASON_CHANGE ) & 0xFF;

	//enter password
	tempCmd[ tempCmdLength++ ] = counter->password1[0];
	tempCmd[ tempCmdLength++ ] = counter->password1[1];
	tempCmd[ tempCmdLength++ ] = counter->password1[2];
	tempCmd[ tempCmdLength++ ] = counter->password1[3];
	tempCmd[ tempCmdLength++ ] = counter->password1[4];
	tempCmd[ tempCmdLength++ ] = counter->password1[5];

	//add flag
	tempCmd[ tempCmdLength++ ] = flag;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;

	//allocate transaction and set command data and sizes
	(*transaction)->transactionType = TRANSACTION_ALLOW_SEASON_CHANGING ;
	(*transaction)->waitSize = 8;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = tempCmdLength;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * tempCmdLength );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCmd, tempCmdLength);
		MAYK_Stuffing( &(*transaction)->command , &((*transaction)->commandSize) );
		MAYK_AddQuotes( &(*transaction)->command , &((*transaction)->commandSize) );
		COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK_CmdAllowSeasonChange: " , tempCmd , tempCmdLength);
		res = OK;
	} else
		res = ERROR_MEM_ERROR;

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CmdAllowSeasonChange() exit with res = [%d].",res);

	return res ;

}

//
//
//

int MAYK_SyncCounterClock( counter_t * counter , counterTask_t ** counterTask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ( (abs(counter->syncWorth) > 1) && (abs(counter->syncWorth) <= (int)MAYK_SOFT_HARD_SYNC_SW))
	{
		//soft sync

		res = MAYK_SyncTimeSoft( counter, COMMON_AllocTransaction(counterTask) );


		STORAGE_SwitchSyncFlagTo_SyncProcess(&counter);

	}
	else
	{
		//hard sync

		res = MAYK_SyncTimeHardy( counter, COMMON_AllocTransaction(counterTask) );

		STORAGE_SwitchSyncFlagTo_SyncProcess(&counter);

	}

	return res ;
}

//
//
//


int MAYK_CreateCounterTask(counter_t * counter, counterTask_t ** counterTask)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CreateCounterTask() start.");
	int res = OK;

	//allocate task
	(*counterTask) = (counterTask_t *) malloc(sizeof (counterTask_t));

	if ((*counterTask) == NULL)
	{
		//ACHTUNG!!! Can not alloacate memory!

		res = ERROR_MEM_ERROR;
		COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CreateCounterTask() exit with errorcode=%d", res);
		return res;
	}

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
	(*counterTask)->resultCurrentReady = 0;
	(*counterTask)->resultDayReady = 0;
	(*counterTask)->resultMonthReady = 0;
	(*counterTask)->resultProfileReady = 0;

	//setup tariff transaction counters
	(*counterTask)->tariffHolidaysTransactionCounter = 0 ;
	(*counterTask)->tariffMapTransactionCounter = 0 ;
	(*counterTask)->tariffMovedDaysTransactionsCounter = 0 ;
	(*counterTask)->powerControlTransactionCounter = 0 ;
	(*counterTask)->indicationWritingTransactionCounter = 0 ;

	//free results
	memset(&(*counterTask)->resultCurrent, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultDay, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultMonth, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultProfile, 0, sizeof (profile_t));


	enum{
		TASK_CREATION_STATE_START,

		TASK_CREATION_STATE_SET_SEASON ,
		TASK_CREATION_STATE_SYNC_TIME,
		TASK_CREATION_STATE_WRITE_TARIFF_MAP,
		TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES,
		TASK_CREATION_STATE_UPDATE_SOFTWARE ,
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
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_START");
				taskCreationState = TASK_CREATION_STATE_SET_SEASON ;
			}
			break;

			//----------

			case TASK_CREATION_STATE_SET_SEASON:
			{
				taskCreationState = TASK_CREATION_STATE_SYNC_TIME ;

				unsigned char allowSeasonFlg = STORAGE_GetSeasonAllowFlag() ;
				unsigned char counterAllowSeason = STORAGE_SEASON_CHANGE_DISABLE ;
				counterStatus_t counterStatus;
				memset( &counterStatus , 0 , sizeof(counterStatus_t) );
				if ( STORAGE_LoadCounterStatus( counter->dbId , &counterStatus ) == OK )
				{
					counterAllowSeason = counterStatus.allowSeasonChange;
				}

				if ( COMMON_CheckDtsForValid( &counter->currentDts ) == OK )
				{
					if (  allowSeasonFlg != counterAllowSeason )
					{
						//setup season change flag

						res = MAYK_CmdAllowSeasonChange (counter, COMMON_AllocTransaction(counterTask), allowSeasonFlg );


						taskCreationState = TASK_CREATION_STATE_DONE ;
					}
				}
			}
			break;

			//----------

			case TASK_CREATION_STATE_SYNC_TIME:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_SYNC_TIME syncFlag is [%02X]" , counter->syncFlag );
				#if COUNTER_TASK_SYNC_TIME
				if ( STORAGE_IsCounterNeedToSync( &counter ) == TRUE )
				{
					COMMON_STR_DEBUG(DEBUG_MAYK "MAYK need to sync");
					res = MAYK_SyncCounterClock( counter , counterTask );
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}
				else
				{
					COMMON_STR_DEBUG(DEBUG_MAYK "MAYK do not need to sync");
					taskCreationState = TASK_CREATION_STATE_WRITE_TARIFF_MAP ;
				}
				#else
					taskCreationState = TASK_CREATION_STATE_WRITE_TARIFF_MAP ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_WRITE_TARIFF_MAP:
			{
				taskCreationState = TASK_CREATION_STATE_DONE ;

				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_WRITE_TARIFF_MAP");
				#if COUNTER_TASK_WRITE_TARIFF_MAP
				unsigned char * tariff = NULL ;
				int tariffLength = 0 ;
				if ( ACOUNT_Get( counter->dbId , &tariff , &tariffLength , ACOUNTS_PART_TARIFF) == OK )
				{
					if ( tariff )
					{
						res = MAYK_WriteTariff( counter , counterTask , tariff , tariffLength ) ;

						if ( res == OK )
						{
							ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_TARIFF );
							unsigned char evDesc[EVENT_DESC_SIZE];
							unsigned char dbidDesc[EVENT_DESC_SIZE];
							memset(dbidDesc , 0, EVENT_DESC_SIZE);
							COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
							COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_START, evDesc);
							STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
						}
						else
						{
							if ( res == ERROR_FILE_FORMAT )
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
							else if ( res == ERROR_COUNTER_TYPE )
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

							//free transactions in current task if need
							COMMON_FreeCounterTask(counterTask);
						}

						free(tariff);
						tariff = NULL;

					}
					else
					{
						ACOUNT_StatusSet( counter->dbId , ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_TARIFF );
						taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_FORM, evDesc);
						STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES:
			{
				taskCreationState = TASK_CREATION_STATE_DONE ;
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_WRITE_POWER_SWITCHER_MODULE_RULES");
				#if COUNTER_TASK_WRITE_PSM
				unsigned char * powerSwithModuleRules = NULL ;
				int powerSwithModuleRulesLength = 0 ;

				if ( ACOUNT_Get(counter->dbId , &powerSwithModuleRules , &powerSwithModuleRulesLength , ACOUNTS_PART_POWR_CTRL) == OK)
				{
					if ( powerSwithModuleRules )
					{
						res = MAYK_WritePSM( counter , counterTask , powerSwithModuleRules , powerSwithModuleRulesLength );
						if ( res == OK )
						{
							ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_IN_PROCESS , ACOUNTS_PART_POWR_CTRL);
							unsigned char evDesc[EVENT_DESC_SIZE];
							unsigned char dbidDesc[EVENT_DESC_SIZE];
							memset(dbidDesc , 0, EVENT_DESC_SIZE);
							COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
							COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_START, evDesc);
							STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						}
						else
						{
							if ( res == ERROR_FILE_FORMAT )
							{
								res = OK ;

								ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_POWR_CTRL);

								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_FORM, evDesc);
								STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

								taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
							}
							else if ( res == ERROR_COUNTER_TYPE )
							{
								res = OK ;

								ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_COUNTER_TYPE , ACOUNTS_PART_POWR_CTRL);

								unsigned char evDesc[EVENT_DESC_SIZE];
								unsigned char dbidDesc[EVENT_DESC_SIZE];
								memset(dbidDesc , 0, EVENT_DESC_SIZE);
								COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
								COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_TYPE, evDesc);
								STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );

								taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
							}

							//free transactions in current task if need
							COMMON_FreeCounterTask(counterTask);

						}
						free( powerSwithModuleRules );
						powerSwithModuleRules = NULL ;

						//taskCreationState = TASK_CREATION_STATE_DONE ;
						//taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
					}
					else
					{
						ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT , ACOUNTS_PART_POWR_CTRL);
						unsigned char evDesc[EVENT_DESC_SIZE];
						unsigned char dbidDesc[EVENT_DESC_SIZE];
						memset(dbidDesc , 0, EVENT_DESC_SIZE);
						COMMON_TranslateLongToChar(counter->dbId, dbidDesc);
						COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_FORM, evDesc);
						STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
				}
				#else
					taskCreationState = TASK_CREATION_STATE_UPDATE_SOFTWARE ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_UPDATE_SOFTWARE :
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_UPDATE_SOFTWARE");
				#if COUNTER_TASK_UPDATE_SOFTWARE

				unsigned char * firmware = NULL ;
				int firmwareLength = 0 ;
				if ( ACOUNT_Get(counter->dbId , &firmware , &firmwareLength , ACOUNTS_PART_POWR_CTRL) == OK)
				{
					if ( firmware )
					{
						//TODO - set status ACOUNTS_STATUS_IN_PROCESS instead of ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL
						ACOUNT_StatusSet(counter->dbId, ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_FIRMWARE);
						//
						// TODO - updating counter software
						//

						free( firmware );
					}
				}

				#if REV_COMPILE_ASK_MAYK_FRM_VER
				else
				{
					//asking firmvare version
					BOOL needToAskFirmvareVersion = TRUE ;

					counterStatus_t counterStatus ;
					memset( &counterStatus , 0 , sizeof( counterStatus_t ) );
					if ( STORAGE_LoadCounterStatus( counter->dbId , &counterStatus ) == OK)
					{
						counterStatus_t emptyCounterStatus;
						STORAGE_StuffCounterStatusByDefault( &emptyCounterStatus );

						//if firmvare version in counter status is not empty then do not ask it
						if ( memcmp( emptyCounterStatus.counterStateWord.word , counterStatus.counterStateWord.word , sizeof( unsigned char ) * STORAGE_COUNTER_STATE_WORD ) )
						{
							COMMON_STR_DEBUG( DEBUG_MAYK "MAYK Do not need to ask firmvare version" );
							needToAskFirmvareVersion = FALSE ;
						}
					}

					if ( needToAskFirmvareVersion == TRUE)
					{

						res = MAYK_CmdGetVersion( counter, COMMON_AllocTransaction(counterTask) );

					}
				}
				#endif


				taskCreationState = TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME ;
				#else
					taskCreationState = TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME ;
				#endif

			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_RATIO_DATE_AND_TIME");
				#if ( (COUNTER_TASK_READ_PROFILES == 1 ) || \
					(COUNTER_TASK_READ_CUR_METERAGES == 1) || \
					(COUNTER_TASK_READ_DAY_METERAGES==1) || \
					(COUNTER_TASK_READ_MON_METERAGES==1) || \
					(COUNTER_TASK_READ_JOURNALS == 1 ) || \
					(COUNTER_TASK_READ_PQP == 1) )



					res = MAYK_GetDateAndTime( counter, COMMON_AllocTransaction(counterTask) );



				if (res == OK)
				{
					if (counter->ratioIndex == 0xFF){
						// get ratio

						res = MAYK_GetRatio( counter, COMMON_AllocTransaction(counterTask) );

					}

					taskCreationState = TASK_CREATION_STATE_READ_CUR_METERAGES ;

				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_DONE ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_CUR_METERAGES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_CUR_METERAGES");
				#if COUNTER_TASK_READ_CUR_METERAGES

				if (res == OK)
				{
					dateTimeStamp dtsToRequest;
					memset(&dtsToRequest , 0 , sizeof(dateTimeStamp));

					if ( STORAGE_CheckTimeToRequestMeterage( counter , STORAGE_CURRENT , &dtsToRequest ) == TRUE )
					{

						res = MAYK_GetCurrentMeterage( counter, COMMON_AllocTransaction(counterTask) , counter->maskTarifs);

					}

					taskCreationState = TASK_CREATION_STATE_READ_MON_METERAGES ;
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_READ_MON_METERAGES ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_MON_METERAGES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_MON_METERAGES");
				#if COUNTER_TASK_READ_MON_METERAGES

				if (res == OK)
				{
					dateTimeStamp dtsToRequest;
					memset(&dtsToRequest , 0 , sizeof(dateTimeStamp) );
					if ( STORAGE_CheckTimeToRequestMeterage( counter ,STORAGE_MONTH , &dtsToRequest) == TRUE )
					{
						//check if meterage is out of archive bound
						dateTimeStamp counterCurrentDts;
						memcpy( &counterCurrentDts , &counter->currentDts , sizeof(dateTimeStamp));
						counterCurrentDts.d.d = 1 ;
						counterCurrentDts.t.h = 0 ;
						counterCurrentDts.t.m = 0 ;
						counterCurrentDts.t.s = 0 ;

						int monthesDiff = COMMON_GetDtsDiff__Alt(&dtsToRequest, &counterCurrentDts, BY_MONTH) + 1;
						int meteragesDepht = COMMON_GetStorageDepth( counter , STORAGE_MONTH ) ;
						if ( monthesDiff > meteragesDepht )
						{
							while ((monthesDiff > meteragesDepht) && (res == OK))
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

						COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is time for read month meterage for [ " DTS_PATTERN " ]", DTS_GET_BY_VAL(dtsToRequest) );


						res = MAYK_GetMonthMeterage( counter, \
														COMMON_AllocTransaction(counterTask) , \
														counter->maskTarifs , \
														&dtsToRequest.d );


						if ( res == OK )
						{
							taskCreationState = TASK_CREATION_STATE_READ_DAY_METERAGES ;
						}
						else
						{
							taskCreationState = TASK_CREATION_STATE_DONE ;
						}

					}
					else
					{
						COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is not time to read month meterages");
						taskCreationState = TASK_CREATION_STATE_READ_DAY_METERAGES ;
					}

				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_READ_DAY_METERAGES ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_DAY_METERAGES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_DAY_METERAGES");
				#if COUNTER_TASK_READ_DAY_METERAGES

				if (res == OK)
				{
					dateTimeStamp dtsToRequest;
					memset(&dtsToRequest , 0 , sizeof(dateTimeStamp) );
					if ( STORAGE_CheckTimeToRequestMeterage( counter ,STORAGE_DAY , &dtsToRequest) == TRUE )
					{
						int daysDiff = COMMON_GetDtsDiff__Alt(&dtsToRequest, &counter->currentDts, BY_DAY ) + 1;
						int meteragesStorageDepth = COMMON_GetStorageDepth( counter , STORAGE_DAY );

						if ( daysDiff > meteragesStorageDepth )
						{
							COMMON_STR_DEBUG( DEBUG_MAYK "MAYK days diff is higher then storage depth for this counter!");
							//fill the storage by empty meterages
							while( daysDiff > meteragesStorageDepth )
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK fill empty day for: " DTS_PATTERN, DTS_GET_BY_VAL(dtsToRequest));

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
							COMMON_STR_DEBUG( DEBUG_MAYK "MAYK days diff after filling is [%d]", daysDiff);
						}

						COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is time for read day meterage for [ " DTS_PATTERN " ]", DTS_GET_BY_VAL(dtsToRequest) );


						res = MAYK_GetDayMeterage( 	counter, \
														COMMON_AllocTransaction(counterTask) , \
														counter->maskTarifs , \
														&dtsToRequest.d );


						taskCreationState = TASK_CREATION_STATE_DONE ;
					}
					else
					{
						COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is not time to read day meterages");
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
			break;

			//---------

			case TASK_CREATION_STATE_READ_PROFILES:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_PROFILES");
				#if COUNTER_TASK_READ_PROFILES

				if ( res == OK )
				{
					if ( counter->profile == POWER_PROFILE_READ )
					{
						if ( counter->profileInterval == STORAGE_UNKNOWN_PROFILE_INTERVAL ){
							counter->profileInterval = 30 ;
						}

						dateTimeStamp dtsToRequest;
						memset(&dtsToRequest , 0 , sizeof(dateTimeStamp) );
						if ( STORAGE_CheckTimeToRequestMeterage( counter ,STORAGE_PROFILE , &dtsToRequest) == TRUE )
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is time for read profiles for [ " DTS_PATTERN " ]", DTS_GET_BY_VAL(dtsToRequest) );

							//check the storage depth
							int sliceDiff = COMMON_GetDtsDiff__Alt(&dtsToRequest, &counter->currentDts, BY_HALF_HOUR ) + 1;
							COMMON_STR_DEBUG(DEBUG_MAYK "diff slices: %d ", sliceDiff );
							int sliceStorageDepth = COMMON_GetStorageDepth( counter , STORAGE_PROFILE );
							if ( sliceDiff > sliceStorageDepth )
							{
								COMMON_STR_DEBUG( DEBUG_MAYK "MAYK profiles diff (in slices) is higher then storage depth for this counter!");

								profile_t emptyProfile ;
								memset(&emptyProfile, 0, sizeof( profile_t ));
								emptyProfile.status = METERAGE_OUT_OF_ARCHIVE_BOUNDS;

								while( sliceDiff > sliceStorageDepth )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "mayk fill empty profile for: " DTS_PATTERN, DTS_GET_BY_VAL(dtsToRequest));
									memcpy(&emptyProfile.dts, &dtsToRequest, sizeof (dateTimeStamp));
									res = STORAGE_SaveProfile( counter->dbId, &emptyProfile);
									if ( res == OK )
									{
										COMMON_ChangeDts(&dtsToRequest, INC, BY_HALF_HOUR );
										--sliceDiff ;
									}
									else
									{
										break;
									}
								}
							}

							if ( res == OK )
							{

								res = MAYK_GetProfileValue( counter, \
																COMMON_AllocTransaction(counterTask) , \
																&dtsToRequest );

							}

							if ( res == OK )
							{
								taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;
							}
							else
							{
								taskCreationState = TASK_CREATION_STATE_DONE ;
							}


						}
						else
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK It is not time to ask profile");
							taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;
						}

					}
					else
					{
						taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_READ_JOURNALS ;
				#endif
			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_JOURNALS:
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK TASK_CREATION_STATE_READ_JOURNALS");
				#if COUNTER_TASK_READ_JOURNALS

				if (res == OK)
				{
					//getting journal
					if (counter->journalReadingState == STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER)
					{						
						res = MAYK_AskEventsNumberInCurrentJournal(counter, COMMON_AllocTransaction(counterTask) , counter->journalNumber );

					}
					else if (counter->journalReadingState == STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT)
					{						
						res = MAYK_GetNextEventFromCurrentJournal(counter, COMMON_AllocTransaction(counterTask) , counter->journalNumber , counter->eventNumber);
					}

					if ( res == OK )
					{
						if ( counter->profile == POWER_PROFILE_READ)
						{
							taskCreationState = TASK_CREATION_STATE_DONE ;
						}
						else
						{
							taskCreationState = TASK_CREATION_STATE_READ_PQP ;
						}
					}
					else
					{
						taskCreationState = TASK_CREATION_STATE_DONE ;
					}
				}
				else
				{
					taskCreationState = TASK_CREATION_STATE_DONE ;
				}

				#else
					taskCreationState = TASK_CREATION_STATE_READ_PQP ;
				#endif

			}
			break;

			//----------

			case TASK_CREATION_STATE_READ_PQP:
			{
				#if COUNTER_TASK_READ_PQP

				int hoursDiff = 24 ;
				powerQualityParametres_t pqp ;
				memset(&pqp, 0 , sizeof(powerQualityParametres_t));
				if (( STORAGE_GetPQPforDB(&pqp, counter->dbId) == OK ) && (COMMON_CheckDtsForValid(&counter->currentDts) == OK))
				{
					hoursDiff = COMMON_GetDtsDiff__Alt( &counter->currentDts, &pqp.dts, BY_HOUR );
				}

				if (hoursDiff > 12)
				{

					if(res == OK)
					{
						res = MAYK_GetPowerQualityParametres( counter, COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_VOLTAGE );
					}

					if(res == OK)
					{
						res = MAYK_GetPowerQualityParametres( counter, COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_AMPERAGE );
					}

					if(res == OK)
					{
						res = MAYK_GetPowerQualityParametres( counter, COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_POWER );
					}

					if(res == OK)
					{
						res = MAYK_GetPowerQualityParametres( counter, COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_FREQUENCY );
					}

					if(res == OK)
					{
						res = MAYK_GetPowerQualityParametres( counter, COMMON_AllocTransaction(counterTask) , PQP_STATE_READING_RATIO );
					}
				}

				taskCreationState = TASK_CREATION_STATE_DONE ;

				#else
					taskCreationState = TASK_CREATION_STATE_DONE ;
				#endif
			}
			break;

			//----------


			default:
				break;
		}
	}


	COMMON_GetCurrentDts( &(*counterTask)->dtsStart );

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_CreateCounterTask() exit with errorcode=%d", res);
	return res;
}

//
//
//

int MAYK_SaveTaskResults(counterTask_t * counterTask)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() started\r\n-----------------------------------------------------------\r\n");
	int res = ERROR_GENERAL;
	unsigned int transactionIndex;
	unsigned char transactionType;
	unsigned char transactionResult = TRANSACTION_DONE_OK;

	dateTimeStamp maykCurrentDts;
	memset( &maykCurrentDts , 0 , sizeof(dateTimeStamp) );

	powerQualityParametres_t pqp;
	memset( &pqp , 0, sizeof(powerQualityParametres_t));

	unsigned char pqpOkFlag = 0 ;
	int ratio = 0;
	BOOL tariffWriting = FALSE ;
	int tarifsWriteDoneOk = 1;
	int indicationWritingTransactionCounter = 0 ;
	int powerControlFlag = -1 ;
	int	settingDefaultPsmRulesFlag = -1 ;


	//resetRepeateFlag
	counter_t * counterPtr = NULL;
	if ( STORAGE_FoundCounter( counterTask->counterDbId , &counterPtr ) == OK )
	{
		if( counterPtr )
		{
			counterPtr->lastTaskRepeateFlag = 0 ;
		} else {

			COMMON_STR_DEBUG( DEBUG_MAYK "counter for this task became NULL");
			return OK;
		}
	}

	//get ratio value if ratio index is present on counter
	if (counterPtr->ratioIndex != 0xFF){
		ratio = COMMON_GetRatioValueMayak(counterPtr->ratioIndex);
	}

	//loop on transaction
	for
	(
		transactionIndex = 0 ; \
		((transactionIndex < counterTask->transactionsCount) && (transactionResult == TRANSACTION_DONE_OK)); \
		transactionIndex++
	)
	{
		transactionType = counterTask->transactions[transactionIndex]->transactionType;
		transactionResult = counterTask->transactions[transactionIndex]->result;

		if ( transactionResult == TRANSACTION_DONE_OK )
		{
			transactionResult = TRANSACTION_DONE_ERROR_A ;

			int tTime = counterTask->transactions[transactionIndex]->tStop - counterTask->transactions[transactionIndex]->tStart;
			if (tTime >= 0){
				counterPtr->transactionTime = tTime;
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK transaction time : %d sec", tTime);
			}

			unsigned char * answer = NULL;
			int answerSize = counterTask->transactions[transactionIndex]->answerSize ;
			int waitSize = counterTask->transactions[transactionIndex]->waitSize;

			//check size of answer is the same as a wait size
			if (answerSize < 10){
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK ANSWER - TOO LITTLE SIZE OF ANSWER [answer size %d]", answerSize);
				transactionResult = TRANSACTION_DONE_ERROR_SIZE;
				break;
			}

			COMMON_AllocateMemory( &answer  , answerSize );
			memcpy( answer , counterTask->transactions[transactionIndex]->answer , answerSize*sizeof(unsigned char)  );

			COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK REQUEST: " ,  counterTask->transactions[transactionIndex]->command ,  counterTask->transactions[transactionIndex]->commandSize );
			COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK ANSWER: " , answer , answerSize);

			//remove 7E-quotes and make backstuffing
			if ( MAYK_RemoveQuotes( &answer , (unsigned int *)&answerSize ) == OK )
			{

				MAYK_Gniffuts( &answer , (unsigned int *)&answerSize ) ;

				//check crc
				unsigned char crc = MAYK_GetCRC( &answer[0] , answerSize - 1);

				unsigned int counterSn = COMMON_BufToInt(&answer[0]);

				//debug
				/*
				printf("ANSWER CRC = [0x%02X] , ANSWER LENGTH = [%d]\n", answer[ answerSize - 2 ] , answerSize );
				EXIT_PATTERN;
				*/

				//check crc
				if ( (crc == answer[ answerSize - 1 ]) && (counterSn == counterPtr->serial))
				{
					//fix last success poll time in  memory
					time(&counterPtr->lastPollTime);
					
					//check status of answer is good
					unsigned char errorStatus = answer[ MAYK_RESULT_CODE_POS ];
					if ( errorStatus != MAYK_RESULT_OK ){
						COMMON_STR_DEBUG(DEBUG_MAYK "MAYK ANSWER / ERROR IN STATUS FIELD [status %02x] " , errorStatus);
					}

					BOOL unexpectedAnswerSize = FALSE ;

					//check size of answer is the same as a wait size (only if answer status is good)
					if (errorStatus == MAYK_RESULT_OK){
						if ( waitSize != answerSize ){
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK ANSWER / ERROR IN ANSWER SIZE [wait %d, recv %d] " , waitSize , answerSize);
//							transactionResult = TRANSACTION_DONE_ERROR_SIZE;
//							COMMON_FreeMemory(&answer);
//							break;
							unexpectedAnswerSize = TRUE ;
						}
					}

					//process transactions
					switch(transactionType)
					{

						case TRANSACTION_SYNC_HARDY:
						{
							//COMMON_BUF_DEBUG( DEBUG_MAYK "MAYK REQUEST: " ,  counterTask->transactions[transactionIndex]->command ,  counterTask->transactions[transactionIndex]->commandSize );
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK ANSWER: " , answer , answerSize);
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_SYNC_HARDY catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SYNC_HARDY | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									//STORAGE_DropSyncDtsFlag( counterTask->counterDbId );
									if ( counterPtr)
									{
										STORAGE_SwitchSyncFlagTo_SyncDone(&counterPtr);
										transactionResult = TRANSACTION_DONE_OK;

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
										transactionResult = TRANSACTION_DONE_ERROR_A;
								}
								else
								{
									if ( counterPtr )
									{
										STORAGE_SwitchSyncFlagTo_SyncFail(&counterPtr);
										transactionResult = TRANSACTION_DONE_OK;
									}
								}
							}


						}
						break ; // TRANSACTION_SYNC_HARDY



						case TRANSACTION_SYNC_SOFT:
						{
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK REQUEST: " ,  counterTask->transactions[transactionIndex]->command ,  counterTask->transactions[transactionIndex]->commandSize );
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK ANSWER: " , answer , answerSize);
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_SYNC_SOFT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SYNC_SOFT | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									//STORAGE_DropSyncDtsFlag( counterTask->counterDbId );
									STORAGE_SwitchSyncFlagTo_SyncDone(&counterPtr);
									transactionResult = TRANSACTION_DONE_OK;

								}
								else if ( errorStatus == MAYK_RESULT_TIME_DIFF_TO_LARGE)
								{
									//soft sync is impossible									
									counterPtr->syncWorth = MAYK_SOFT_HARD_SYNC_SW + 1;
									transactionResult = TRANSACTION_DONE_OK;

								}
								else //if ( errorStatus == MAYK_RESULT_TIME_IN_PREV_SYNC )
								{
									//last sync is not done yet
									STORAGE_SwitchSyncFlagTo_SyncFail(&counterPtr);
									transactionResult = TRANSACTION_DONE_OK;

								}

							}

						}
						break ; // TRANSACTION_SYNC_SOFT

						case TRANSACTION_ALLOW_SEASON_CHANGING:
						{
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK REQUEST: " ,  counterTask->transactions[transactionIndex]->command ,  counterTask->transactions[transactionIndex]->commandSize );
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK ANSWER: " , answer , answerSize);
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_ALLOW_SEASON_CHANGING catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SEASON_CHANGE | 0x8000 ) )
							{
								if ( errorStatus != MAYK_RESULT_OK )
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
							}

							//do not check result. Set season anyway
							unsigned char seasonFlg = STORAGE_GetSeasonAllowFlag();
							STORAGE_UpdateCounterStatus_AllowSeasonChangeFlg( counterPtr , seasonFlg ) ;

							transactionResult = TRANSACTION_DONE_OK;

							counterPtr->lastTaskRepeateFlag = 1 ;
						}
						break;

						case TRANSACTION_DATE_TIME_STAMP:
						{
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK REQUEST: " ,  counterTask->transactions[transactionIndex]->command ,  counterTask->transactions[transactionIndex]->commandSize );
							//COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK ANSWER: " , answer , answerSize);
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_DATE_TIME_STAMP catched.");

							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_DATE | 0x8000 ) )
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									if( counterPtr )
									{
										if ( COMMON_CheckDtsForValid( &counterPtr->currentDts ) != OK )
										{
											counterPtr->lastTaskRepeateFlag = 1 ;
										}
									}

									maykCurrentDts.t.s = answer[7];
									maykCurrentDts.t.m = answer[8];
									maykCurrentDts.t.h = answer[9];
									// answer[10]; - day of week. do not present in stuct DateTimeStamp
									maykCurrentDts.d.d = answer[11];
									maykCurrentDts.d.m = answer[12];
									maykCurrentDts.d.y = answer[13];

									dateTimeStamp currentDts;
									COMMON_GetCurrentDts( &currentDts );
									int taskTime = COMMON_GetDtsDiff__Alt( &counterTask->dtsStart , &currentDts , BY_SEC );
									STORAGE_UpdateCounterDts( counterTask->counterDbId , &maykCurrentDts , taskTime );

									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_DATE_TIME_STAMP current counter dts is " DTS_PATTERN, DTS_GET_BY_VAL(maykCurrentDts));



									if ( counterPtr )
									{
										COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() worth = [%d], DTS = [" DTS_PATTERN "]" , counterPtr->syncWorth , DTS_GET_BY_VAL(counterPtr->currentDts)) ;
										//debug
										//EXIT_PATTERN;
									}



									transactionResult = TRANSACTION_DONE_OK;

									//debug
									//EXIT_PATTERN;
								}

							}
							else
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_DATE_TIME_STAMP cmd format is incorect.");
							}
						}
						break ; //TRANSACTION_DATE_TIME_STAMP



						case TRANSACTION_GET_RATIO:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_RATIO catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_RATIO | 0x8000 ) )
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									ratio = COMMON_BufToInt( &answer[ 18 ] );

									if (counterPtr){
										//setup ratio index
										counterPtr->ratioIndex = COMMON_GetMayakRatio(ratio);
										STORAGE_UpdateCounterStatus_Ratio( counterPtr , counterPtr->ratioIndex );
										counterPtr->lastTaskRepeateFlag = 1;
									}

									transactionResult = TRANSACTION_DONE_OK;
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() RATIO = [%d]", ratio);
								}
							}
							else
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_RATIO cmd format is incorect.");
							}
						}
						break; // TRANSACTION_GET_RATIO


						case TRANSACTION_METERAGE_CURRENT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_CURRENT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_CURRENT | 0x8000 ) )
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									//count the number of tarifs in current answer
									unsigned char tarifMask = answer[7];
									int tarifNumb = 0;
									int answerPos = 8;

									unsigned char tarifNumbIndex = 0x01;
									for( tarifNumbIndex = 0x01 ; tarifNumbIndex <= MAX_TARIFF_MASK && tarifNumbIndex > 0 ; tarifNumbIndex = tarifNumbIndex << 1 )
									{
										if ( (tarifNumbIndex & tarifMask) != 0x00)
										{
											unsigned int eAp = COMMON_BufToInt( &answer[ answerPos ] );
											unsigned int eAm = COMMON_BufToInt( &answer[ answerPos + 4 ] );
											unsigned int eRp = COMMON_BufToInt( &answer[ answerPos + 8 ] );
											unsigned int eRm = COMMON_BufToInt( &answer[ answerPos + 12] );


											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_AP - 1] = eAp;
											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_AM - 1] = eAm;
											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_RP - 1] = eRp;
											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_RM - 1] = eRm;

											COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_CURRENT tariff N%d, eAp = [%d] , eAm = [%d] , eRp = [%d] , eRm = [%d]", tarifNumb , eAp, eAm , eRp , eRm);


											answerPos+=16;
											if ( answerPos >= answerSize )
												break;
										}

//										else
//										{
//											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_AP - 1] = 0xFFFFFFFF;
//											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_AM - 1] = 0xFFFFFFFF;
//											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_RP - 1] = 0xFFFFFFFF;
//											counterTask->resultCurrent.t[tarifNumb].e[ENERGY_RM - 1] = 0xFFFFFFFF;
//										}


										tarifNumb++;
									}
									if ( counterPtr )
									{
										memcpy(&counterTask->resultCurrent.dts , &counterPtr->currentDts , sizeof(dateTimeStamp));
									}
									else
									{
										dateTimeStamp currentDts;
										memset(&currentDts , 0 , sizeof(dateTimeStamp));
										if (COMMON_GetCurrentDts(&currentDts) == OK )
											memcpy(&counterTask->resultCurrent.dts , &currentDts , sizeof(dateTimeStamp));
									}

									counterTask->resultCurrent.ratio = COMMON_GetMayakRatio(ratio);
									// TODO - what about delimetr?
									counterTask->resultCurrent.delimeter = 0; //?
									counterTask->resultCurrent.status = METERAGE_VALID;
									counterTask->resultCurrentReady = READY;
									transactionResult = TRANSACTION_DONE_OK;

									res = STORAGE_SaveMeterages(STORAGE_CURRENT, counterTask->counterDbId, &counterTask->resultCurrent);

									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() current metrages saved with errorcode = [%d]" , res);

								}
							}
							else
							{
								COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_CURRENT cmd format is incorect.");
							}
						}
						break; // TRANSACTION_METERAGE_CURRENT


						case TRANSACTION_METERAGE_DAY:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_DAY catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_METERAGE | 0x8000 ) )
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE))
								{
									unsigned char tariffMask = answer[8];

									int tarifNumb = 0;

									int answerPos = 9;

									int tarifmap[4];
									memset( tarifmap , 0 , 4 * sizeof(int) );

									unsigned char tarifNumbIndex = 0x01;
									int tarifMapIndex = 0;
									for( tarifNumbIndex = 0x01 ; (tarifNumbIndex <= MAX_TARIFF_MASK) && (tarifNumbIndex > 0) ; tarifNumbIndex = tarifNumbIndex << 1 )
									{
										if ( (tarifNumbIndex & tariffMask) != 0x00)
										{
											tarifmap[ tarifMapIndex ] = tarifNumb ;
											tarifMapIndex++;
										}
										tarifNumb++;
									}

									while(1)
									{
										dateTimeStamp dts;
										memset ( &dts , 0 , sizeof(dateTimeStamp) ) ;
										dts.d.d = answer[ answerPos++ ];
										dts.d.m = answer[ answerPos++ ];
										dts.d.y = answer[ answerPos++ ];

										memcpy( &counterTask->resultDay.dts , &dts , sizeof(dateTimeStamp) );

										counterTask->resultDay.status = METERAGE_VALID;
										int tarifIndex;
										for( tarifIndex = 0 ; tarifIndex < tarifMapIndex ; tarifIndex++)
										{
											unsigned int eAp = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eAm = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eRp = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eRm = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;

											if ( (eAp == 0xFFFFFFFF) && (eAm == 0xFFFFFFFF) && (eRp == 0xFFFFFFFF) && (eRm == 0xFFFFFFFF))
											{
												counterTask->resultDay.status = METERAGE_NOT_PRESET;
												COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() set up metrage status NOT PRESENT");

												eAp = 0 ;
												eAm = 0 ;
												eRp = 0 ;
												eRm = 0 ;
											}
											counterTask->resultDay.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_AP - 1] = eAp;
											counterTask->resultDay.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_AM - 1] = eAm;
											counterTask->resultDay.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_RP - 1] = eRp;
											counterTask->resultDay.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_RM - 1] = eRm;

											COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_DAY tariff N%d, eAp = [%d] , eAm = [%d] , eRp = [%d] , eRm = [%d]", tarifIndex , eAp, eAm , eRp , eRm);
										}
										counterTask->resultDay.ratio = COMMON_GetMayakRatio(ratio);;
										// TODO - what about delimetr?
										//counterTask->resultDay.status = METERAGE_VALID;
										counterTask->resultDayReady = READY;
										res = STORAGE_SaveMeterages(STORAGE_DAY, counterTask->counterDbId, &counterTask->resultDay);

										if(answerPos >= (answerSize - 1) )
											break;

										//
										// TO DO : if meter send more tha we wait - we will infinitive loop here
										//
									}


									transactionResult = TRANSACTION_DONE_OK;

									if( counterPtr )
									{
										counterPtr->lastTaskRepeateFlag = 1 ;
									}

								}
								else
									//if (errorStatus == MAYK_RESULT_NOT_FOUND )
								{

									//oispov 2015-03-13
									//we have already counterPtr, use it
									
									//counter_t * counter = NULL ;
									//if ( STORAGE_FoundCounter( counterTask->counterDbId , &counter) == OK)
									//{
										if( counterPtr )
										{
											dateTimeStamp lastRequestDts;
											memset( &lastRequestDts.t , 0 , sizeof(timeStamp));
											memcpy( &lastRequestDts.d , &counterPtr->dayMetaragesLastRequestDts , sizeof(dateStamp));

											if ( COMMON_CheckDtsForValid ( &lastRequestDts ) == OK )
											{
												memcpy( &counterTask->resultDay.dts.d , &counterPtr->dayMetaragesLastRequestDts , sizeof(dateStamp));

												counterTask->resultDay.ratio = COMMON_GetMayakRatio(ratio);;
												counterTask->resultDay.status = METERAGE_NOT_PRESET;
												counterTask->resultDayReady = READY;
												res = STORAGE_SaveMeterages(STORAGE_DAY, counterTask->counterDbId, &counterTask->resultDay);

											}

										}
									//}

								}
							}

							//debug
							//EXIT_PATTERN;

						}
						break; // TRANSACTION_METERAGE_DAY


						case TRANSACTION_METERAGE_MONTH:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_MONTH catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_METERAGE | 0x8000 ) )
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE))
								{

									unsigned char tarifMask = answer[8];
									int tarifNumb = 0;

									int answerPos = 9;

									unsigned char tarifNumbIndex = 0x01;

									int tarifmap[4];
									memset( tarifmap , 0 , 4 * sizeof(int) );

									int tarifMapIndex = 0;
									for( tarifNumbIndex = 0x01 ; tarifNumbIndex <= MAX_TARIFF_MASK && tarifNumbIndex > 0 ; tarifNumbIndex = tarifNumbIndex << 1 )
									{
										if ( (tarifNumbIndex & tarifMask) != 0x00)
										{
											tarifmap[ tarifMapIndex ] = tarifNumb ;
											tarifMapIndex++;
										}
										tarifNumb++;
									}

									while(1)
									{
										dateTimeStamp dts;
										memset ( &dts , 0 , sizeof(dateTimeStamp) ) ;
										dts.d.d = answer[ answerPos++ ];
										dts.d.m = answer[ answerPos++ ];
										dts.d.y = answer[ answerPos++ ];

//										dateTimeStamp lastRequestedDts;
//										memset( &lastRequestedDts , 0 , sizeof(dateTimeStamp) );
//										memcpy( &lastRequestedDts.d , &counterPtr->monthMetaragesLastRequestDts , sizeof(dateStamp) );
//										if ( COMMON_GetDtsDiff( &dts ,&lastRequestedDts , sizeof(dateStamp) ) == 0 )
//										{
//											memcpy( &counterTask->resultMonth.dts.d , &lastRequestedDts , sizeof(dateStamp));
//										}
//										else
//										{
//											memcpy( &counterTask->resultMonth.dts , &dts , sizeof(dateTimeStamp) );
//										}

										memcpy( &counterTask->resultMonth.dts , &dts , sizeof(dateTimeStamp) );

										counterTask->resultMonth.status = METERAGE_VALID;

										int tarifIndex;
										for( tarifIndex = 0 ; tarifIndex < tarifMapIndex ; tarifIndex++)
										{
											unsigned int eAp = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eAm = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eRp = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;
											unsigned int eRm = COMMON_BufToInt( &answer[ answerPos ] );
											answerPos+=4;

											COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_METERAGE_MONTH tariff N%d, eAp = [%d] , eAm = [%d] , eRp = [%d] , eRm = [%d]", tarifIndex , eAp, eAm , eRp , eRm);
											if ( (eAp == 0xFFFFFFFF) || (eAm == 0xFFFFFFFF) || (eRp == 0xFFFFFFFF) || (eRm == 0xFFFFFFFF))
											{
												counterTask->resultMonth.status = METERAGE_NOT_PRESET;
												COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() set up metrage status NOT PRESENT");

												if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
												if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
												if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
												if (eRm == 0xFFFFFFFF) { eRm = 0 ; }
											}

											counterTask->resultMonth.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_AP - 1] = eAp;
											counterTask->resultMonth.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_AM - 1] = eAm;
											counterTask->resultMonth.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_RP - 1] = eRp;
											counterTask->resultMonth.t[ ( tarifmap[ tarifIndex ] ) ].e[ENERGY_RM - 1] = eRm;
										}
										counterTask->resultMonth.ratio = COMMON_GetMayakRatio(ratio);;
										// TODO - what about delimetr?

										counterTask->resultMonthReady = READY;
										res = STORAGE_SaveMeterages(STORAGE_MONTH, counterTask->counterDbId, &counterTask->resultMonth);


										if(answerPos >= (answerSize - 1) )
											break;

										//
										//TO DO : infinitive loop in answer more than count of tarrifs
										//
									}

									if( counterPtr )
									{
										counterPtr->lastTaskRepeateFlag = 1 ;
									}

									transactionResult = TRANSACTION_DONE_OK;

								}
								else
									//if (errorStatus == MAYK_RESULT_NOT_FOUND )
								{
									//oispov 2015-03-13
									//we have already counterPtr, use 
									
									//counter_t * counter = NULL ;
									//if ( STORAGE_FoundCounter( counterTask->counterDbId , &counter) == OK)
									//{
										if( counterPtr )
										{
											dateTimeStamp lastRequestDts;
											memset( &lastRequestDts.t , 0 , sizeof(timeStamp));
											memcpy( &lastRequestDts.d , &counterPtr->monthMetaragesLastRequestDts , sizeof(dateStamp));
											if ( COMMON_CheckDtsForValid (&lastRequestDts ) == OK )
											{
												memcpy( &counterTask->resultMonth.dts.d , &counterPtr->monthMetaragesLastRequestDts , sizeof(dateStamp));

												counterTask->resultMonth.ratio = COMMON_GetMayakRatio(ratio);;
												counterTask->resultMonth.status = METERAGE_NOT_PRESET;
												counterTask->resultMonthReady = READY;
												res = STORAGE_SaveMeterages(STORAGE_MONTH, counterTask->counterDbId, &counterTask->resultMonth);
											}
										}
									//}
								}

							}
						}
						break; // TRANSACTION_METERAGE_MONTH


						//
						//OSIPOV:: INVESTIGATE THIS!!!
						//

						//
						//Ukhov: Sir, yes, sir !!!
						//

						//
						//OSIPOV:: WTF, Man?! 
						//

						case TRANSACTION_PROFILE_INTERVAL:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_PROFILE_INTERVAL catched.");

//							unsigned char addStatus = 0 ;

							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_GET_PROFILE | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									if ( counterPtr )
									{
										counterPtr->lastTaskRepeateFlag = 1 ;

										if ( unexpectedAnswerSize == TRUE )
										{
											//unsigned char addStatus = 0 ;
											int profileDataSize = answerSize-4-2-1-1;
											int profileParts = profileDataSize/40; //(dts 7 + ppr 33)

											COMMON_STR_DEBUG( DEBUG_MAYK "profile parts: %d", profileParts);

											if (profileParts > 0){
												int pIndex = 0;
												int mrIndex = 7;
												for (;pIndex<profileParts; pIndex++)
												{
													COMMON_STR_DEBUG( DEBUG_MAYK "parse profile part %d", pIndex);

													dateTimeStamp dts;
													dts.t.s = answer[mrIndex++];
													dts.t.m = answer[mrIndex++];
													dts.t.h = answer[mrIndex++];

													++mrIndex ;

													dts.d.d = answer[mrIndex++];
													dts.d.m = answer[mrIndex++];
													dts.d.y = answer[mrIndex++];

													COMMON_STR_DEBUG(DEBUG_MAYK "profile part dts is " DTS_PATTERN, DTS_GET_BY_VAL(dts));

													if ( COMMON_CheckDtsForValid( &dts ) == OK )
													{
														unsigned int eAp = COMMON_BufToInt( &answer[mrIndex] );
														mrIndex+=4;
														unsigned int eAm = COMMON_BufToInt( &answer[mrIndex] );
														mrIndex+=4;
														unsigned int eRp = COMMON_BufToInt( &answer[mrIndex] );
														mrIndex+=4;
														unsigned int eRm = COMMON_BufToInt( &answer[mrIndex] );
														mrIndex+=4+16;

														COMMON_STR_DEBUG(DEBUG_MAYK "eAp = [%u] , eAm = [%u] , eRp = [%u] , eRm = [%u]" , eAp , eAm , eRp , eRm ) ;

														unsigned char status = answer[mrIndex];
														mrIndex++;
														COMMON_STR_DEBUG(DEBUG_MAYK "status = [%d]" , status );
														if (status == 0)
														{

															if ( (eAp == 0xFFFFFFFF) || (eAm == 0xFFFFFFFF) || (eRp == 0xFFFFFFFF) || (eRm == 0xFFFFFFFF))
															{
																if ((eAp == 0xFFFFFFFF) && (eAm == 0xFFFFFFFF) && (eRp == 0xFFFFFFFF) && (eRm == 0xFFFFFFFF))
																{
																	counterTask->resultProfile.status =  METERAGE_NOT_PRESET;
																	eAp = 0 ;
																	eAm = 0 ;
																	eRp = 0 ;
																	eRm = 0 ;
																}
																else
																{
																	counterTask->resultProfile.status =  METERAGE_PARTIAL;
																	if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
																	if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
																	if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
																	if (eRm == 0xFFFFFFFF) { eRm = 0 ; }
																}

															}
															else
															{
																counterTask->resultProfile.status =  METERAGE_VALID;
																COMMON_STR_DEBUG(DEBUG_MAYK "Metrages are valid" );
															}

															//counterTask->resultProfile.status =  METERAGE_VALID;
															//COMMON_STR_DEBUG(DEBUG_MAYK "Metrages are valid" );
															counterTask->resultProfileReady = READY;
														}
														else if (status & 0x80)
														{
															if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
															if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
															if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
															if (eRm == 0xFFFFFFFF) { eRm = 0 ; }

															counterTask->resultProfile.status = METERAGE_NOT_PRESET;
															COMMON_STR_DEBUG(DEBUG_MAYK "Status = [%d] . Metrages are partial" , status );
															counterTask->resultProfileReady = READY;
														}
														else if ( status == 1 )
														{
															if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
															if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
															if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
															if (eRm == 0xFFFFFFFF) { eRm = 0 ; }

															counterTask->resultProfile.status = METERAGE_PARTIAL;
															COMMON_STR_DEBUG(DEBUG_MAYK "Status = [%d] . Metrages are partial" , status );
															counterTask->resultProfileReady = READY;
														}

														memcpy( &counterTask->resultProfile.dts , &dts , sizeof(dateTimeStamp)  );

														counterTask->resultProfile.ratio = COMMON_GetMayakRatio(ratio); ;
														counterTask->resultProfile.p.e[ENERGY_AP - 1] = eAp;
														counterTask->resultProfile.p.e[ENERGY_AM - 1] = eAm;
														counterTask->resultProfile.p.e[ENERGY_RP - 1] = eRp;
														counterTask->resultProfile.p.e[ENERGY_RM - 1] = eRm;

														if ( counterTask->resultProfileReady == READY )
														{
//															addStatus = 0 ;
//															if ( STORAGE_IsProfilePresenceInStorage( counterTask->counterDbId , &counterTask->resultProfile.dts ) == TRUE )
//															{
//																addStatus = METERAGE_DUBLICATED;
//															}
//															counterTask->resultProfile.status |= addStatus ;
															STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);															
															counterTask->resultProfileReady = ZERO;
														}
													}

												}

											} else {
												if ( COMMON_CheckDtsForValid( &counterPtr->profileLastRequestDts ) == OK )
												{
													COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() profile for dts [" DTS_PATTERN "] unexpected length" , DTS_GET_BY_VAL(counterPtr->profileLastRequestDts) );
													memcpy( &counterTask->resultProfile.dts , &counterPtr->profileLastRequestDts , sizeof( dateTimeStamp ) );
													counterTask->resultProfile.ratio = COMMON_GetMayakRatio(ratio); ;
													counterTask->resultProfile.status = METERAGE_UNEXPECTED_ANSWER;

//													if ( STORAGE_IsProfilePresenceInStorage( counterTask->counterDbId , &counterTask->resultProfile.dts ) == TRUE )
//													{
//														addStatus = METERAGE_DUBLICATED;
//													}
//													counterTask->resultProfile.status |= addStatus ;

													STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);
													counterTask->resultProfileReady = ZERO;
												}
											}

											transactionResult = TRANSACTION_DONE_OK;

										}
										else //expected answer size
										{
											int mrIndex = 7;

											dateTimeStamp dts;
											dts.t.s = answer[mrIndex++];
											dts.t.m = answer[mrIndex++];
											dts.t.h = answer[mrIndex++];

											++mrIndex ;

											dts.d.d = answer[mrIndex++];
											dts.d.m = answer[mrIndex++];
											dts.d.y = answer[mrIndex++];

											COMMON_STR_DEBUG(DEBUG_MAYK "DTS is " DTS_PATTERN , DTS_GET_BY_VAL(dts));


											if (COMMON_DtsAreEqual( &counterPtr->profileLastRequestDts , &dts ) == TRUE )
											{
												unsigned int eAp = COMMON_BufToInt( &answer[mrIndex] );
												mrIndex+=4;
												unsigned int eAm = COMMON_BufToInt( &answer[mrIndex] );
												mrIndex+=4;
												unsigned int eRp = COMMON_BufToInt( &answer[mrIndex] );
												mrIndex+=4;
												unsigned int eRm = COMMON_BufToInt( &answer[mrIndex] );
												mrIndex+=16 + 4;

												COMMON_STR_DEBUG(DEBUG_MAYK "eAp = [%u] , eAm = [%u] , eRp = [%u] , eRm = [%u]" , eAp , eAm , eRp , eRm ) ;

												unsigned char status = answer[mrIndex];
												COMMON_STR_DEBUG(DEBUG_MAYK "Status = [%d]" , status );
												if (status == 0)
												{

													if ( (eAp == 0xFFFFFFFF) || (eAm == 0xFFFFFFFF) || (eRp == 0xFFFFFFFF) || (eRm == 0xFFFFFFFF))
													{
														if ((eAp == 0xFFFFFFFF) && (eAm == 0xFFFFFFFF) && (eRp == 0xFFFFFFFF) && (eRm == 0xFFFFFFFF))
														{
															counterTask->resultProfile.status =  METERAGE_NOT_PRESET;
															eAp = 0 ;
															eAm = 0 ;
															eRp = 0 ;
															eRm = 0 ;
														}
														else
														{
															counterTask->resultProfile.status =  METERAGE_PARTIAL;
															if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
															if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
															if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
															if (eRm == 0xFFFFFFFF) { eRm = 0 ; }
														}

													}
													else
													{
														counterTask->resultProfile.status =  METERAGE_VALID;
														COMMON_STR_DEBUG(DEBUG_MAYK "Metrages are valid" );
													}


													//counterTask->resultProfile.status =  METERAGE_VALID;
													//COMMON_STR_DEBUG(DEBUG_MAYK "Metrages are valid" );
													counterTask->resultProfileReady = READY;
												}
												else if (status & 0x80)
												{
													if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
													if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
													if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
													if (eRm == 0xFFFFFFFF) { eRm = 0 ; }

													counterTask->resultProfile.status = METERAGE_NOT_PRESET;
													COMMON_STR_DEBUG(DEBUG_MAYK "Status = [%d] . Metrages are partial" , status );
													counterTask->resultProfileReady = READY;
												}
												else if ( status == 1 )
												{
													if (eAp == 0xFFFFFFFF) { eAp = 0 ; }
													if (eAm == 0xFFFFFFFF) { eAm = 0 ; }
													if (eRp == 0xFFFFFFFF) { eRp = 0 ; }
													if (eRm == 0xFFFFFFFF) { eRm = 0 ; }

													counterTask->resultProfile.status = METERAGE_PARTIAL;
													COMMON_STR_DEBUG(DEBUG_MAYK "Status = [%d] . Metrages are partial" , status );
													counterTask->resultProfileReady = READY;
												}



												memcpy( &counterTask->resultProfile.dts , &dts , sizeof(dateTimeStamp)  );

												counterTask->resultProfile.ratio = COMMON_GetMayakRatio(ratio); ;
												counterTask->resultProfile.p.e[ENERGY_AP - 1] = eAp;
												counterTask->resultProfile.p.e[ENERGY_AM - 1] = eAm;
												counterTask->resultProfile.p.e[ENERGY_RP - 1] = eRp;
												counterTask->resultProfile.p.e[ENERGY_RM - 1] = eRm;

												if ( counterTask->resultProfileReady == READY )
												{
//													if ( STORAGE_IsProfilePresenceInStorage( counterTask->counterDbId , &counterTask->resultProfile.dts ) == TRUE )
//													{
//														addStatus = METERAGE_DUBLICATED;
//													}
//													counterTask->resultProfile.status |= addStatus ;
													STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);
												}
											}
											else
											{
												memcpy( &counterTask->resultProfile.dts , &counterPtr->profileLastRequestDts , sizeof(dateTimeStamp)  );

												COMMON_STR_DEBUG(DEBUG_MAYK "DTS's are not equal!!!" );
												counterTask->resultProfile.status =  METERAGE_NOT_PRESET;
												counterTask->resultProfile.p.e[ENERGY_AP - 1] = 0;
												counterTask->resultProfile.p.e[ENERGY_AM - 1] = 0;
												counterTask->resultProfile.p.e[ENERGY_RP - 1] = 0;
												counterTask->resultProfile.p.e[ENERGY_RM - 1] = 0;
												counterTask->resultProfileReady = READY;

//												if ( STORAGE_IsProfilePresenceInStorage( counterTask->counterDbId , &counterTask->resultProfile.dts ) == TRUE )
//												{
//													addStatus = METERAGE_DUBLICATED;
//												}
//												counterTask->resultProfile.status |= addStatus ;

												STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);
											}
											transactionResult = TRANSACTION_DONE_OK;

											//debug
											//EXIT_PATTERN;
										}
									}
								}
								else
									//if ( errorStatus == MAYK_RESULT_NOT_FOUND )
								{
									if( counterPtr )
									{
										counterPtr->lastTaskRepeateFlag = 1 ;

										if ( COMMON_CheckDtsForValid( &counterPtr->profileLastRequestDts ) == OK )
										{
											COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() profile for dts [" DTS_PATTERN "] was not found" , DTS_GET_BY_VAL(counterPtr->profileLastRequestDts) );
											memcpy( &counterTask->resultProfile.dts , &counterPtr->profileLastRequestDts , sizeof( dateTimeStamp ) );
											counterTask->resultProfile.ratio = COMMON_GetMayakRatio(ratio); ;
											counterTask->resultProfile.status = METERAGE_NOT_PRESET;

//											if ( STORAGE_IsProfilePresenceInStorage( counterTask->counterDbId , &counterTask->resultProfile.dts ) == TRUE )
//											{
//												addStatus = METERAGE_DUBLICATED;
//											}
//											counterTask->resultProfile.status |= addStatus ;

											STORAGE_SaveProfile(counterTask->counterDbId, &counterTask->resultProfile);
										}
									}
									transactionResult = TRANSACTION_DONE_OK;
								}
							}
						}
						break; //TRANSACTION_PROFILE_INTERVAL


						case TRANSACTION_GET_EVENTS_NUMBER:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_EVENTS_NUMBER catched. Transaction Index = [%d]" , transactionIndex);
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_EVENTS_NUMB | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE))
								{
									if ( counterPtr )
									{
										int eventsNumber = 0 ;
										// 8 and 9 position
										eventsNumber = ((answer[8] & 0xFF) << 8) + (answer[9] & 0xFF);
										if (eventsNumber == 0)
										{
											counterPtr->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER ;											
											MAYK_StepToNextJournal(counterPtr);
										}
										else
										{
											counterPtr->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT ;
											counterPtr->eventsTotal = eventsNumber ;
											counterPtr->eventNumber = 0 ;
										}
										transactionResult = TRANSACTION_DONE_OK;

									}
								}
								else
									MAYK_StepToNextJournal(counterPtr);

							}
						}
						break; //TRANSACTION_GET_EVENTS_NUMBER


						case TRANSACTION_GET_EVENT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_EVENT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_EVENT | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									//create struct uspdEvent_t
									uspdEvent_t counterEvent_1;
									uspdEvent_t counterEvent_2;
									memset(&counterEvent_1 , 0 , sizeof(uspdEvent_t));
									memset(&counterEvent_2 , 0 , sizeof(uspdEvent_t));

									//if uspdEvent_t is in local journal go to the next journal exept journal N 0,1,2
									//if uspdEvent_t is not in local journal put it to the accoring local journal
									int journalType = answer[7] & 0xFF ;
									switch (journalType)
									{
										case 0x0:
										{
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_OPENING_BOX ;
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											counterEvent_2.dts.t.s = answer[17];
											counterEvent_2.dts.t.m = answer[18];
											counterEvent_2.dts.t.h = answer[19];
											//answer[20] - day of week. Not using
											counterEvent_2.dts.d.d = answer[21];
											counterEvent_2.dts.d.m = answer[22];
											counterEvent_2.dts.d.y = answer[23];
											counterEvent_2.evType = EVENT_CLOSING_BOX ;
											counterEvent_2.crc = STORAGE_CalcCrcForJournal(&counterEvent_2);

											//oispov 2015-03-13
											//we have already counterPtr, use it
									
											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{

													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if (STORAGE_IsEventInJournal(&counterEvent_1 , counterPtr->dbId) == TRUE)
														{
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
															{
																if (STORAGE_IsEventInJournal(&counterEvent_2 , counterPtr->dbId) == FALSE)
																{
																	STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
																}
															}
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															STORAGE_SaveEvent(&counterEvent_1 , counterPtr->dbId);
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
															{
																STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
															}
															MAYK_StepToNextEvent(counterPtr);
														}

													}
													else
														MAYK_StepToNextEvent(counterPtr);

												}
											//}
										}
										break;

										case 0x1:
										{
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_OPENING_CONTACT ;
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											counterEvent_2.dts.t.s = answer[17];
											counterEvent_2.dts.t.m = answer[18];
											counterEvent_2.dts.t.h = answer[19];
											//answer[20] - day of week. Not using
											counterEvent_2.dts.d.d = answer[21];
											counterEvent_2.dts.d.m = answer[22];
											counterEvent_2.dts.d.y = answer[23];
											counterEvent_2.evType = EVENT_CLOSING_CONTACT ;
											counterEvent_2.crc = STORAGE_CalcCrcForJournal(&counterEvent_2);

											//oispov 2015-03-13
											//we have already counterPtr, use it
											
											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{

													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if (STORAGE_IsEventInJournal(&counterEvent_1 , counterPtr->dbId) == TRUE)
														{
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
															{
																if (STORAGE_IsEventInJournal(&counterEvent_2 , counterPtr->dbId) == FALSE)
																{
																	STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
																}
															}
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															STORAGE_SaveEvent(&counterEvent_1 , counterPtr->dbId);
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
																STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
															MAYK_StepToNextEvent(counterPtr);
														}

													}
													else
														MAYK_StepToNextEvent(counterPtr);

												}
											//}
										}
										break;

										case 0x2:
										{
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_COUNTER_SWITCH_ON ;
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);


											counterEvent_2.dts.t.s = answer[17];
											counterEvent_2.dts.t.m = answer[18];
											counterEvent_2.dts.t.h = answer[19];
											//answer[20] - day of week. Not using
											counterEvent_2.dts.d.d = answer[21];
											counterEvent_2.dts.d.m = answer[22];
											counterEvent_2.dts.d.y = answer[23];
											counterEvent_2.evType = EVENT_COUNTER_SWITCH_OFF ;
											counterEvent_2.crc = STORAGE_CalcCrcForJournal(&counterEvent_2);

											//oispov 2015-03-13
											//we have already counterPtr, use it
											
											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{

													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if (STORAGE_IsEventInJournal(&counterEvent_1 , counterPtr->dbId) == TRUE)
														{
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
															{
																if (STORAGE_IsEventInJournal(&counterEvent_2 , counterPtr->dbId) == FALSE)
																{
																	STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
																}
															}
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															STORAGE_SaveEvent(&counterEvent_1 , counterPtr->dbId);
															if(COMMON_CheckDtsForValid(&counterEvent_2.dts) == OK)
																STORAGE_SaveEvent(&counterEvent_2 , counterPtr->dbId);
															MAYK_StepToNextEvent(counterPtr);
														}

													}
													else
														MAYK_StepToNextEvent(counterPtr);

												}
											//}



											//    event_1_Flag = 1 ;


											  //  event_2_Flag = 1 ;
										}
										break;

										case 0x3:
										{
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_SYNC_TIME_HARD ;


											dateTimeStamp tempDts;
											memset(&tempDts , 0 , sizeof(dateTimeStamp));

											tempDts.t.s = answer[17];
											tempDts.t.m = answer[18];
											tempDts.t.h = answer[19];
											//answer[20] - day of week. Not using
											tempDts.d.d = answer[21];
											tempDts.d.m = answer[22];
											tempDts.d.y = answer[23];

											memcpy(counterEvent_1.evDesc , &tempDts , sizeof(dateTimeStamp));
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											//oispov 2015-03-13
											//we have already counterPtr, use it

											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{
													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if ( STORAGE_IsEventInJournal(&counterEvent_1 , counterTask->counterDbId) == TRUE)
														{
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															if (STORAGE_SaveEvent(&counterEvent_1 , counterTask->counterDbId) == OK )
															{
																MAYK_StepToNextEvent(counterPtr);
															}
														}
													}
												}
											//}

										}
										break;

										case 0x4:
										{
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_SYNC_TIME_SOFT ;

											dateTimeStamp tempDts;
											memset(&tempDts , 0 , sizeof(dateTimeStamp));

											tempDts.t.s = answer[17];
											tempDts.t.m = answer[18];
											tempDts.t.h = answer[19];
											//answer[20] - day of week. Not using
											tempDts.d.d = answer[21];
											tempDts.d.m = answer[22];
											tempDts.d.y = answer[23];

											memcpy(counterEvent_1.evDesc , &tempDts , sizeof(dateTimeStamp));
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											//event_1_Flag = 1 ;

											//oispov 2015-03-13
											//we have already counterPtr, use it

											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{
													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if ( STORAGE_IsEventInJournal(&counterEvent_1 , counterTask->counterDbId) == TRUE)
														{
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															if (STORAGE_SaveEvent(&counterEvent_1 , counterTask->counterDbId) == OK )
															{
																MAYK_StepToNextEvent(counterPtr);
															}
														}
													}
												}
											//}
										}
										break;

										case 0x5:
										{
											// 1 dts
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];
											counterEvent_1.evType = EVENT_CHANGE_TARIF_MAP ;
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											//event_1_Flag = 1 ;

											//oispov 2015-03-13
											//we have already counterPtr, use it
											
											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{
													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if ( STORAGE_IsEventInJournal(&counterEvent_1 , counterTask->counterDbId) == TRUE)
														{
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															if (STORAGE_SaveEvent(&counterEvent_1 , counterTask->counterDbId) == OK )
															{
																MAYK_StepToNextEvent(counterPtr);
															}
														}
													}
												}
											//}
										}
										break;

										case 0x06:
										{
											//1 dts
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];

											if ( answer[37] == 0)
											{
												counterEvent_1.evType = EVENT_SWITCH_POWER_OFF ;
											}
											else
											{
												counterEvent_1.evType = EVENT_SWITCH_POWER_ON ;
											}

											//event_1_Flag = 1 ;
											MAYK_GetReleReason( &answer[17], counterEvent_1.evDesc);
											counterEvent_1.crc = STORAGE_CalcCrcForJournal(&counterEvent_1);

											//oispov 2015-03-13
											//we have already counterPtr, use it
											
											//counter_t * counter = NULL;
											//if ( STORAGE_FoundCounter(counterTask->counterDbId, &counter) == OK)
											//{
												if (counterPtr)
												{
													if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
													{
														if ( STORAGE_IsEventInJournal(&counterEvent_1 , counterTask->counterDbId) == TRUE)
														{
															MAYK_StepToNextJournal(counterPtr);
														}
														else
														{
															if (STORAGE_SaveEvent(&counterEvent_1 , counterTask->counterDbId) == OK )
															{
																MAYK_StepToNextEvent(counterPtr);
															}
														}
													}
												}
											//}
										}
										break;

										case 0x08:
										{
											//1 dts
											// from the 10 position
											counterEvent_1.dts.t.s = answer[10];
											counterEvent_1.dts.t.m = answer[11];
											counterEvent_1.dts.t.h = answer[12];
											//answer[13] - day of week. Not using
											counterEvent_1.dts.d.d = answer[14];
											counterEvent_1.dts.d.m = answer[15];
											counterEvent_1.dts.d.y = answer[16];

											counterEvent_1.evType = MAYK_GetJournalPqpEvType(answer[17]);

											memcpy ( counterEvent_1.evDesc, &answer[18], 4 ) ;

											COMMON_STR_DEBUG(DEBUG_MAYK "MAYK Saving PQP event for " DTS_PATTERN " evType = [%hhu]", DTS_GET_BY_VAL(counterEvent_1.dts), counterEvent_1.evType);
											COMMON_BUF_DEBUG(DEBUG_MAYK "MAYK evDesc", counterEvent_1.evDesc, EVENT_DESC_SIZE);

											if(COMMON_CheckDtsForValid(&counterEvent_1.dts) == OK)
											{
												if ( STORAGE_IsEventInJournal(&counterEvent_1 , counterTask->counterDbId) == TRUE)
												{
													MAYK_StepToNextJournal(counterPtr);
												}
												else
												{
													int errorCode = STORAGE_SaveEvent(&counterEvent_1 , counterTask->counterDbId);
													COMMON_STR_DEBUG(DEBUG_MAYK "MAYK Saving event status [%d]", errorCode);
													if ( errorCode == OK )
													{
														MAYK_StepToNextEvent(counterPtr);
													}
												}
											}

										}
										break;

										default:
											break;

									}
									transactionResult = TRANSACTION_DONE_OK;
								}
							}
						}
						break; //TRANSACTION_GET_EVENT


						case TRANSACTION_GET_PQP_VOLTAGE_GROUP:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_PQP_VOLTAGE catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_PQP | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									unsigned char pqpType = answer[ MAYK_RESULT_CODE_POS + 1];
									if ( pqpType == 0x00 )
									{
										pqpVoltage_t pqpVoltage;
										memset(&pqpVoltage , 0 , sizeof(pqpVoltage_t));

										int pos = MAYK_RESULT_CODE_POS + 2 ;

										pqpVoltage.a = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpVoltage.b = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpVoltage.c = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										memcpy(&pqp.U , &pqpVoltage , sizeof(pqpVoltage_t));
										pqpOkFlag |= PQP_FLAG_VOLTAGE ;
									}
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_GET_PQP_AMPERAGE_GROUP:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_PQP_AMPERAGE catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_PQP | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									unsigned char pqpType = answer[ MAYK_RESULT_CODE_POS + 1];
									if ( pqpType == 0x01 )
									{
										pqpAmperage_t pqpAmperage;
										memset(&pqpAmperage , 0 , sizeof(pqpAmperage_t));

										int pos = MAYK_RESULT_CODE_POS + 2 ;



										pqpAmperage.a = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpAmperage.b = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpAmperage.c = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										memcpy(&pqp.I , &pqpAmperage , sizeof(pqpAmperage_t));
										pqpOkFlag |= PQP_FLAG_AMPERAGE ;
									}
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_GET_PQP_POWER_ALL:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_PQP_POWER catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_PQP | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									unsigned char pqpType = answer[ MAYK_RESULT_CODE_POS + 1];
									if ( pqpType == 0x02 )
									{
										pqpPower_t pqpPower_P;
										memset(&pqpPower_P , 0 , sizeof(pqpPower_t));

										pqpPower_t pqpPower_Q;
										memset(&pqpPower_Q , 0 , sizeof(pqpPower_t));

										pqpPower_t pqpPower_S;
										memset(&pqpPower_S , 0 , sizeof(pqpPower_t));

										int pos = MAYK_RESULT_CODE_POS + 2 ;


										pqpPower_P.a = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_P.b = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_P.c = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_P.sigma = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										pqpPower_Q.a = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_Q.b = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_Q.c = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_Q.sigma = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										pqpPower_S.a = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_S.b = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_S.c = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpPower_S.sigma = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;


										memcpy(&pqp.P , &pqpPower_P , sizeof(pqpPower_t));
										memcpy(&pqp.Q , &pqpPower_Q , sizeof(pqpPower_t));
										memcpy(&pqp.S , &pqpPower_S , sizeof(pqpPower_t));


										pqpOkFlag |= PQP_FLAG_POWER ;
									}
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_GET_PQP_FREQUENCY:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_PQP_FREQUENCY catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_PQP | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									unsigned char pqpType = answer[ MAYK_RESULT_CODE_POS + 1];
									if ( pqpType == 0x03 )
									{
									   int frequency = 0 ;

										int pos = MAYK_RESULT_CODE_POS + 2 ;

										frequency = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;

										memcpy(&pqp.frequency , &frequency , sizeof(int));
										pqpOkFlag |= PQP_FLAG_FREQUENCY ;
									}
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_GET_PQP_RATIO:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_GET_PQP_RATIO catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_PQP | 0x8000))
							{
								if ( (errorStatus == MAYK_RESULT_OK) && (unexpectedAnswerSize == FALSE) )
								{
									unsigned char pqpType = answer[ MAYK_RESULT_CODE_POS + 1];
									if ( pqpType == 0x04 )
									{
										pqpRatio_t pqpRatio;
										memset(&pqpRatio , 0 , sizeof(pqpRatio_t));

										int pos = MAYK_RESULT_CODE_POS + 1 + 1;

										pqpRatio.cos_ab = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpRatio.cos_bc = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpRatio.cos_ac = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										pqpRatio.tan_ab = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpRatio.tan_bc = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;
										pqpRatio.tan_ac = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
										pos += 4 ;

										memcpy(&pqp.coeff , &pqpRatio , sizeof(pqpRatio_t));
										pqpOkFlag |= PQP_FLAG_RATIO ;
									}

								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_MAYK_POWER_SWITCHER_STATE_SET:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_MAYK_POWER_SWITCHER_STATE_SET catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SET_POWER_SWITCHER_STATE | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									powerControlFlag = 1 ;
								}
								else
								{
									powerControlFlag = 0 ;
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;

						case TRANSACTION_MAYK_SET_POWER_CONTROLS:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_MAYK_SET_POWER_CONTROLS catched.");
							transactionResult = TRANSACTION_DONE_OK;
						}
						break;


						case TRANSACTION_MAYK_SET_POWER_CONTROLS_DEFAULT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_MAYK_SET_POWER_CONTROLS_DEFAULT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SET_POWER_CTRL | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									settingDefaultPsmRulesFlag = 1 ;
								}
								else
								{
									settingDefaultPsmRulesFlag = 0 ;
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;


						case TRANSACTION_MAYK_SET_POWER_CONTROLS_DONE:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_MAYK_SET_POWER_CONTROLS_DONE catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == ( MAYK_CMD_SET_POWER_CTRL | 0x8000 ) )
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									if ( (settingDefaultPsmRulesFlag != 0) && (powerControlFlag != 0) )
									{
										ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_OK , ACOUNTS_PART_POWR_CTRL);

										unsigned char evDesc[EVENT_DESC_SIZE];
										unsigned char dbidDesc[EVENT_DESC_SIZE];
										memset(dbidDesc , 0, EVENT_DESC_SIZE);
										COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
										COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_OK, evDesc);
										STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
									}
									else
									{
										ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_POWR_CTRL);

										unsigned char evDesc[EVENT_DESC_SIZE];
										unsigned char dbidDesc[EVENT_DESC_SIZE];
										memset(dbidDesc , 0, EVENT_DESC_SIZE);
										COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
										COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_GEN, evDesc);
										STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
									}

								}
								else if ( errorStatus == MAYK_RESULT_ERR_PWD )
								{
									ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD , ACOUNTS_PART_POWR_CTRL);
									unsigned char evDesc[EVENT_DESC_SIZE];
									unsigned char dbidDesc[EVENT_DESC_SIZE];
									memset(dbidDesc , 0, EVENT_DESC_SIZE);
									COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
									COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_PASS, evDesc);
									STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
								}
								else
								{
									ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_POWR_CTRL);
									unsigned char evDesc[EVENT_DESC_SIZE];
									unsigned char dbidDesc[EVENT_DESC_SIZE];
									memset(dbidDesc , 0, EVENT_DESC_SIZE);
									COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
									COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_PSM_RULES_WRITING_ERR_GEN, evDesc);
									STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;


						case TRANSACTION_BEGIN_WRITING_TARIFF_MAP:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_BEGIN_WRITING_TARIFF_MAP catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_BEGIN_TARIFF_WRITING | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() starting writing tariff map OK");

//									counter_t * counter = NULL ;
//									if ( STORAGE_FoundCounter( counterTask->counterDbId , &counter ) == OK )
//									{
//										if ( counter )
//										{
//											counter->tariffWritingAttemptsCounter = 0 ;
//										}
//									}

								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{									
									tarifsWriteDoneOk = -1;
								}
								else
								{									
									tarifsWriteDoneOk = 0;
									//STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING_ERR, COMMON_TranslateLongToChar(counterTask->counterDbId));
									//ACOUNT_SetStatus(counterTask->counterDbId , ACOUNTS_STATUS_WRITTEN_ERROR);
									//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() starting writing tariff map ERROR");
								}

								transactionResult = TRANSACTION_DONE_OK;
							}

						}
						break;

						case TRANSACTION_END_WRITING_TARIFF_MAP:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_END_WRITING_TARIFF_MAP catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_END_TARIFF_WRITING | 0x8000))
							{
								tariffWriting = TRUE ;
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK end wrinting tariff OK" );
								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{
									tarifsWriteDoneOk = -1;
								}
								else
								{
									tarifsWriteDoneOk = 0 ;
								}

								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						case TRANSACTION_WRITING_TARIFF_STT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_WRITING_TARIFF_STT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_TARIFF_WRITING_STT | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing stt OK");
								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{
									tarifsWriteDoneOk = -1;
								}
								else
								{
									tarifsWriteDoneOk = 0;
									//STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING_ERR, COMMON_TranslateLongToChar(counterTask->counterDbId));
									//ACOUNT_SetStatus(counterTask->counterDbId , ACOUNTS_STATUS_WRITTEN_ERROR);
									//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing stt ERROR");
								}

								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						case TRANSACTION_WRITING_TARIFF_WTT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_WRITING_TARIFF_WTT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_TARIFF_WRITING_WTT | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing wtt OK");
								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{
									tarifsWriteDoneOk = -1;
								}
								else
								{
									tarifsWriteDoneOk = 0;
									//STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING_ERR, COMMON_TranslateLongToChar(counterTask->counterDbId));
									//ACOUNT_SetStatus(counterTask->counterDbId , ACOUNTS_STATUS_WRITTEN_ERROR);
									//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing wtt ERROR");
								}

								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						case TRANSACTION_WRITING_TARIFF_MTT:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_WRITING_TARIFF_MTT catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_TARIFF_WRITING_MTT | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing mtt OK");
								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{
									tarifsWriteDoneOk = -1;
								}
								else
								{
									tarifsWriteDoneOk = 0;
									//STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING_ERR, COMMON_TranslateLongToChar(counterTask->counterDbId));
									//ACOUNT_SetStatus(counterTask->counterDbId , ACOUNTS_STATUS_WRITTEN_ERROR);
									//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing mtt ERROR");
								}

								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						case TRANSACTION_WRITING_TARIFF_LWJ:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_WRITING_TARIFF_LWJ catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_TARIFF_WRITING_LWJ | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing lwj OK");
								}
								else if (errorStatus == MAYK_RESULT_ERR_PWD)
								{
									tarifsWriteDoneOk = -1;
								}
								else
								{
									tarifsWriteDoneOk = 0;
									//STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING_ERR, COMMON_TranslateLongToChar(counterTask->counterDbId));
									//ACOUNT_SetStatus(counterTask->counterDbId , ACOUNTS_STATUS_WRITTEN_ERROR);
									//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
									COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing lwj ERROR");
								}

								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						case TRANSACTION_START_WRITING_INDICATIONS:
						case TRANSACTION_WRITING_INDICATIONS:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_START_WRITING_INDICATIONS catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_WRITING_INDICATIONS | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									if ( indicationWritingTransactionCounter >= 0 )
									{
										indicationWritingTransactionCounter++ ;
									}
								}
								else
								{
									indicationWritingTransactionCounter = -1 ;
								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
							break;

						#if REV_COMPILE_ASK_MAYK_FRM_VER
						case TRANSACTION_READ_DEVICE_SW_VERSION:
						{
							COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResults() TRANSACTION_READ_DEVICE_SW_VERSION catched.");
							unsigned short cmd = ( answer[ MAYK_COMMAND_CODE_FIRST_POS ] << 8 ) | answer[ MAYK_COMMAND_CODE_SEC_POS ] ;
							if (cmd == (MAYK_CMD_GET_VERSION | 0x8000))
							{
								if ( errorStatus == MAYK_RESULT_OK )
								{
									int pos = MAYK_RESULT_CODE_POS + 1;

									#if !REV_COMPILE_RELEASE
									unsigned int VER_PRTCL = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK VER_PRTCL [%u]", VER_PRTCL );
									#endif
									pos += 4 ;

									#if !REV_COMPILE_RELEASE
									unsigned int TYPE_MTR = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK TYPE_MTR [%u]", TYPE_MTR );
									#endif
									pos += 4 ;

									#if !REV_COMPILE_RELEASE
									unsigned int VER_FRM = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK VER_FRM [%u]", VER_FRM );
									#endif
									pos += 4 ;

									unsigned int VER_TECH_FRM = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK VER_TECH_FRM [%u]", VER_TECH_FRM );
									pos += 4 ;

									#if !REV_COMPILE_RELEASE
									unsigned int CRC_FRM = ( (answer[pos] & 0xFF) << 24 ) + ((answer[pos + 1 ] & 0xFF) << 16) + ((answer[pos + 2 ] & 0xFF) << 8) + (answer[pos + 3] & 0xFF)  ;
									COMMON_STR_DEBUG( DEBUG_MAYK "MAYK CRC_FRM [%u]", CRC_FRM );
									#endif
									pos += 4 ;

									#if !REV_COMPILE_RELEASE
									//NAME_FRM
									int stopNameFrmPos = -1 ;
									int answerIndex = pos ;
									for( ; answerIndex < answerSize ; ++answerIndex )
									{
										if ( answer[ answerIndex ] == 0x00 )
										{
											stopNameFrmPos = answerIndex ;
											break;
										}
									}
									if ( stopNameFrmPos > 0 )
									{
										COMMON_STR_DEBUG( DEBUG_MAYK "MAYK NAME_FRM: [%s]", &answer[ pos ] );
									}
									#endif

									counterStateWord_t counterStateWord ;
									memset( &counterStateWord , 0 , sizeof(counterStateWord_t) ) ;

									counterStateWord.word[ 0 ] = (VER_TECH_FRM >> 24) & 0xFF;
									counterStateWord.word[ 1 ] = (VER_TECH_FRM >> 16) & 0xFF;
									counterStateWord.word[ 2 ] = (VER_TECH_FRM >> 8 ) & 0xFF;
									counterStateWord.word[ 3 ] = VER_TECH_FRM & 0xFF ;

									STORAGE_UpdateCounterStatus_StateWord( counterPtr , &counterStateWord ) ;

								}
								transactionResult = TRANSACTION_DONE_OK;
							}
						}
						break;
						#endif

						default:
							//TODO ?
							transactionResult = TRANSACTION_DONE_OK;
							break;

					} //switch

				} //checking CRC and serial

			} // if ( MAYK_RemoveQuotes() )
			else
			{
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_RemoveQuotes() returned [%s]" , getErrorCode(res));
			}


			COMMON_FreeMemory(&answer);

		} // if ( transactionResult == TRANSACTION_DONE_OK )
		else
		{
			//quit transaction results parse loop if transaction in not OK
			break;
		}

	} //for

	//save statistic if is all ok
	if(transactionResult == TRANSACTION_DONE_OK){
		res = OK ;
		STORAGE_SetCounterStatistic(counterTask->counterDbId, FALSE, 0, TRANSACTION_COUNTER_POOL_DONE);

		if ( tariffWriting == TRUE )
		{
			if ( (tarifsWriteDoneOk == 1) && ( indicationWritingTransactionCounter == counterTask->indicationWritingTransactionCounter) )
			{
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_OK, evDesc);
				STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING, evDesc);
				ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_OK , ACOUNTS_PART_TARIFF);
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing tariff map is over with result [OK]");
			}
			else if ( (tarifsWriteDoneOk == -1) || (indicationWritingTransactionCounter == -1) )
			{
				COMMON_STR_DEBUG( DEBUG_MAYK "MAYK_SaveTaskResult() expected ind transactions [%d] but really [%d]. TarifWritingFlag is [%d]" , counterTask->indicationWritingTransactionCounter , indicationWritingTransactionCounter , tarifsWriteDoneOk );
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_PASS, evDesc);
				STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING, evDesc);
				ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD , ACOUNTS_PART_TARIFF);
				//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing tariff map is over with result [ERROR_PWD]");
			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_MAYK "MAYK_SaveTaskResult() expected ind transactions [%d] but really [%d]. TarifWritingFlag is [%d]" , counterTask->indicationWritingTransactionCounter , indicationWritingTransactionCounter , tarifsWriteDoneOk );
				unsigned char evDesc[EVENT_DESC_SIZE];
				unsigned char dbidDesc[EVENT_DESC_SIZE];
				memset(dbidDesc , 0, EVENT_DESC_SIZE);
				COMMON_TranslateLongToChar(counterTask->counterDbId, dbidDesc);
				COMMON_CharArrayDisjunction( (char *)dbidDesc , DESC_EVENT_TARIFF_WRITING_ERR_GEN, evDesc);
				STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING, evDesc);
				ACOUNT_StatusSet(counterTask->counterDbId, ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL , ACOUNTS_PART_TARIFF);
				//STORAGE_ResetWriteTarrifsFlag(counterTask->counterDbId);
				COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() writing tariff map is over with result [ERROR]");
			}

			//debug
			//EXIT_PATTERN;
		}

		if ( (pqpOkFlag & PQP_FLAG_AMPERAGE ) && \
			 (pqpOkFlag & PQP_FLAG_VOLTAGE ) && \
			 (pqpOkFlag & PQP_FLAG_POWER ) && \
			 (pqpOkFlag & PQP_FLAG_FREQUENCY ) && \
			 (pqpOkFlag & PQP_FLAG_RATIO ))
		{
			if( counterPtr )
			{
				if( COMMON_CheckDtsForValid( &counterPtr->currentDts ) == OK )
				{
					memcpy( &pqp.dts , &counterPtr->currentDts , sizeof( dateTimeStamp ));
					STORAGE_SavePQP(&pqp , counterTask->counterDbId);
				}
			}

				//debug
//				printf("Date: %hhu-%hhu-%hhu %hhu:%hhu:%hhu\n", pqp.dts.d.d , pqp.dts.d.m , pqp.dts.d.y , pqp.dts.t.h , pqp.dts.t.m , pqp.dts.t.s );

//				printf("Voltage: A=%d B=%d C=%d\n" , pqp.U.a , pqp.U.b , pqp.U.c);
//				printf("Amperage: A=%d B=%d C=%d\n", pqp.I.a , pqp.I.b , pqp.I.c);
//				printf("P power: A=%d B=%d C=%d SIGMA=%d\n", pqp.P.a , pqp.P.b , pqp.P.c, pqp.P.sigma);
//				printf("Q power: A=%d B=%d C=%d SIGMA=%d\n", pqp.Q.a , pqp.Q.b , pqp.Q.c, pqp.Q.sigma);
//				printf("S power: A=%d B=%d C=%d SIGMA=%d\n", pqp.S.a , pqp.S.b , pqp.S.c, pqp.S.sigma);
//				printf("Freuqency = %d\n" , pqp.frequency);
//				printf("Ratios: cosAB=%d cosBC=%d cosAC=%d tanAB=%d tanBC=%d tanAC=%d\n", pqp.coeff.cos_ab, pqp.coeff.cos_bc, pqp.coeff.cos_ac, pqp.coeff.tan_ab, pqp.coeff.tan_bc, pqp.coeff.tan_ac);

//				EXIT_PATTERN;


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
	}

	COMMON_STR_DEBUG(DEBUG_MAYK "MAYK_SaveTaskResult() returned %d\r\n-----------------------------------------------------------\r\n",res);


	return res ;
} // int MAYK_SaveTaskResults()


//
//
//

void MAYK_StepToNextJournal(counter_t * counter)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	if (counter)
	{		
		counter->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER;
		counter->journalNumber += 1;

		if ( (counter->type == TYPE_MAYK_MAYK_302) && !strlen((char*)counter->serialRemote))
		{
			if ( counter->journalNumber == 7 )
				counter->journalNumber = 8 ;
			else if ( counter->journalNumber > 8 )
				counter->journalNumber = 0 ;
		}
		else
		{
			if(counter->journalNumber > 6)
				counter->journalNumber = 0 ;
		}

		counter->eventNumber = 0 ;
		counter->eventsTotal = 0 ;
	}
}

//
//
//

void MAYK_StepToNextEvent(counter_t * counter)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	if (counter)
	{
		counter->journalReadingState = STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT;
		counter->eventNumber += 1;
		if (counter->eventNumber == counter->eventsTotal)
		{
			MAYK_StepToNextJournal(counter);
		}
	}
}

//
//
//

int MAYK_GetReleReason( unsigned char * maskArray, unsigned char * evDesc)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ( maskArray && evDesc )
	{
		unsigned int mskOther = ( (maskArray[0] & 0xFF) << 8 ) + (maskArray[1] & 0xFF) ;
		unsigned int mskPQ =( (maskArray[2] & 0xFF) << 24 ) + ( (maskArray[3] & 0xFF) << 16 ) + ( (maskArray[4] & 0xFF) << 8 ) + (maskArray[5] & 0xFF);
		unsigned int mskE30 = ( (maskArray[6] & 0xFF) << 8 ) + (maskArray[7] & 0xFF) ;
		unsigned int mskEday = ( (maskArray[8] & 0xFF) << 8 ) + (maskArray[9] & 0xFF) ;
		unsigned int makEmnth = ( (maskArray[10] & 0xFF) << 8 ) + (maskArray[11] & 0xFF) ;
		unsigned int mskUmax = ( (maskArray[12] & 0xFF) << 8 ) + (maskArray[13] & 0xFF) ;
		unsigned int mskUmin =  ( (maskArray[14] & 0xFF) << 8 ) + (maskArray[15] & 0xFF) ;
		unsigned int mskImax = ( (maskArray[16] & 0xFF) << 8 ) + (maskArray[17] & 0xFF) ;
		unsigned int mskRules = ( (maskArray[18] & 0xFF) << 8 ) + (maskArray[19] & 0xFF) ;
		//20 position is rele state
		if ( maskArray[20] == 0 )
		{
			if (mskOther & 0x01)
			{
				// by the UI
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_UI , EVENT_DESC_SIZE);
			}
			else if (mskOther & 0x02)
			{
				//by the opening contact box
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_OPEN_CONTACT , EVENT_DESC_SIZE);
			}
			else if (mskOther & 0x04)
			{
				//by the opening counter box
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_OPEN_BOX , EVENT_DESC_SIZE);
			}
			else
			{
				if (mskRules & 0x01)
				{
					if (mskPQ)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_PQ, evDesc);
					}
					else if (mskE30)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_E30, evDesc);
					}
					else if (mskEday)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Eday, evDesc);
					}
					else if (makEmnth)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Emnth, evDesc);
					}
					else if (mskUmax)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umax, evDesc);
					}
					else if (mskUmin)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umin, evDesc);
					}
					else if (mskImax)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Imax, evDesc);
					}
					else
					{
						//unknown rule
						memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_RULES , EVENT_DESC_SIZE);
					}
				}
				else
				{
					//unknown reason
					memcpy(evDesc , DESC_EVENT_RELE_SWITCH_OFF_BY_UNKNOWN_REASON , EVENT_DESC_SIZE);
				}
			}


		}
		else
		{
			if (mskOther & 0x01)
			{
				// by the UI
				memcpy(evDesc , DESC_EVENT_RELE_SWITCH_ON_BY_UI , EVENT_DESC_SIZE);
			}
			else
			{
				if ( mskRules & 0x02 )
				{
					// some property returns to normal
					if (mskPQ)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_PQ, evDesc);
					}
					else if (mskE30)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_E30, evDesc);
					}
					else if (mskEday)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Eday, evDesc);
					}
					else if (makEmnth)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Emnth, evDesc);
					}
					else if (mskUmax)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Umax, evDesc);
					}
					else if (mskUmin)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Umin, evDesc);
					}
					else if (mskImax)
					{
						COMMON_CharArrayDisjunction(DESC_EVENT_RELE_SWITCH_ON_BY_RULES , DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Imax, evDesc);
					}
					else
					{
						//unknown rule
						memcpy(evDesc , DESC_EVENT_RELE_SWITCH_ON_BY_RULES , EVENT_DESC_SIZE);
					}
				}
				else if ( mskRules & 0x04 )
				{
					// rele switched on because of button pressed
					memcpy(evDesc , DESC_EVENT_RELE_SWITCH_ON_BY_BUTTON_PRESSED , EVENT_DESC_SIZE);
				}
				else
				{
					//unknown reason
					memcpy(evDesc , DESC_EVENT_RELE_SWITCH_ON_BY_UNKNOWN_REASON , EVENT_DESC_SIZE);
				}
			}
		}

		res = OK ;
	}
	return res ;
}

//
//
//

int MAYK_UnknownCounterIdentification( unsigned char * counterType , unsigned char * ver)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if (counterType && ver)
	{
		unsigned int version = 0;
		version = ((ver[0] & 0xFF) << 24 ) | ((ver[1] & 0xFF) << 16 ) | ((ver[2] & 0xFF) << 8 ) | (ver[3] & 0xFF);

		switch(version)
		{
			case 0x00065000:
			case 0x00065001:
			case 0x00065002:
			case 0x00065003:
			case 0x00065004:
			case 0x00065005:
			case 0x00065006:
			case 0x00065007:
				res = OK ;
				(*counterType) = TYPE_MAYK_MAYK_101 ;
				break;

			case 0x003B6A00:
			case 0x003B6A01:
			case 0x003B6A02:
			case 0x003B6A03:
			case 0x003B6A04:
			case 0x003B6A05:
			case 0x003B6A06:
			case 0x003B6A07:
			case 0x003B6A08:
			case 0x003B6A09:
			case 0x003B6A0A:
			case 0x003B6A0B:
			case 0x003B6A0C:
			case 0x003B6A0D:
			case 0x003B6A0E:
			case 0x003B6A0F:				
			case 0x003B6B00:
			case 0x003B6B01:
			case 0x003B6B02:
			case 0x003B6B03:
			case 0x003B6B04:
			case 0x003B6B05:
			case 0x003B6B06:
			case 0x003B6B07:
			case 0x003B6B08:
			case 0x003B6B09:
			case 0x003B6B0A:
			case 0x003B6B0B:
			case 0x003B6B0C:
			case 0x003B6B0D:
			case 0x003B6B0E:
			case 0x003B6B0F:
			case 0x003B6B10:
			case 0x003B6B11:
			case 0x003B6B12:
			case 0x003B6B13:
			case 0x003B6B14:
			case 0x003B6B15:
			case 0x003B6B16:
			case 0x003B6B17:
			case 0x003B6B18:
			case 0x003B6B19:
			case 0x003B6B1A:
			case 0x003B6B1B:

				res = OK ;
				(*counterType) = TYPE_MAYK_MAYK_103 ;
				break;

			case 0x00012E00:
			case 0x00012E01:
			case 0x00012E02:
			case 0x00012E03:
			case 0x00012E04:
			case 0x00012E05:
			case 0x00012E06:
			case 0x00012E07:
			case 0x00012E08:
			case 0x00012E09:
			case 0x00012E0A:
			case 0x00012E0B:
			case 0x00012E0C:
			case 0x00012E0D:
			case 0x00012E0E:
			case 0x00012E0F:
			case 0x00012E10:
			case 0x00012E11:
			case 0x00012E12:
			case 0x00012E13:
			case 0x00012E14:
			case 0x00012E15:
			case 0x00012E16:
			case 0x00012E17:
			case 0x00012E18:
			case 0x00012E19:
			case 0x00012E1A:
			case 0x00012E1B:
			case 0x00012E1C:
			case 0x00012E1D:
			case 0x00012E1E:
			case 0x00012E1F:				
			case 0x00012F00:
			case 0x00012F01:
			case 0x00012F02:
			case 0x00012F03:
			case 0x00012F04:
			case 0x00012F05:
			case 0x00012F06:
			case 0x00012F07:
			case 0x00012F08:
			case 0x00012F09:
			case 0x00012F0A:
			case 0x00012F0B:
			case 0x00012F0C:
			case 0x00012F0D:
			case 0x00012F0E:
			case 0x00012F0F:
				res = OK ;
				(*counterType) = TYPE_MAYK_MAYK_302 ;
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

int MAYK_GetSTT(unsigned char * tariff , int tarifLength , maykTariffSTT_t ** stt , int * sttSize)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ( (tariff) && (tarifLength > 0) && (sttSize) && (stt) && ((*stt) == NULL) )
	{
		(*sttSize) = 0 ;
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(char *));
		if (token)
		{
			char * pch;
			int tokenNumb = 0 ;
			char * lastPtr = NULL ;

			char * tariff_ = calloc(tarifLength + 1 , sizeof(char));
			if(tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r(tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for STT's
				int sttEntryNumb = - 1 ;
				while(1)
				{
					int tokenIndex ;
					if (sttEntryNumb < 0)
						tokenIndex = 0 ;
					else
						tokenIndex = sttEntryNumb ;

					//searching for the next entry
					for( ; tokenIndex < tokenNumb ; tokenIndex++)
					{
						if (strstr(token[tokenIndex],"[STT") )
						{
							sttEntryNumb = tokenIndex ;
							break;
						}
						if (tokenIndex == (tokenNumb - 1))
						{
							sttEntryNumb = -1;
							break;
						}
					}

					if ( sttEntryNumb > 0 )
					{
						if(strcmp(token[++sttEntryNumb], "Value") == 0 )
						{
							sttEntryNumb++;
							(*stt) = realloc((*stt) , (++(*sttSize))*sizeof(maykTariffSTT_t) );
							if( (*stt) )
							{
								memset(&(*stt)[(*sttSize) - 1] , 0 , sizeof(maykTariffSTT_t));
								tokenIndex = sttEntryNumb ;

								(*stt)[(*sttSize) - 1].tpNumb = atoi(token[tokenIndex++]);

								int tpIndex = 0 ;

								for( ;tpIndex < ((*stt)[(*sttSize) - 1].tpNumb) ; tpIndex++)
								{

									if (tokenIndex < tokenNumb)
									{
										if (!strstr(token[tokenIndex] , "[") )
										{
											(*stt)[(*sttSize) - 1].tp[tpIndex].ts.s = atoi(token[tokenIndex++]);
										}
										else
											break;
									}
									else
										break;

									if (tokenIndex < tokenNumb)
									{
										if (!strstr(token[tokenIndex] , "[") )
										{
											(*stt)[(*sttSize) - 1].tp[tpIndex].ts.m = atoi(token[tokenIndex++]);
										}
										else
											break;
									}
									else
										break;

									if (tokenIndex < tokenNumb)
									{
										if (!strstr(token[tokenIndex] , "[") )
										{
											(*stt)[(*sttSize) - 1].tp[tpIndex].ts.h = atoi(token[tokenIndex++]);
										}
										else
											break;
									}
									else
										break;

									if (tokenIndex < tokenNumb)
									{
										if (!strstr(token[tokenIndex] , "[") )
										{
											(*stt)[(*sttSize) - 1].tp[tpIndex].tariffNumber = atoi(token[tokenIndex++]);
										}
										else
											break;
									}
									else
										break;

									res = OK ;

								}

							}
							else
							{
								res = ERROR_MEM_ERROR ;
								break;
							}

						}
					}
					else
						break ;
				}

				free(tariff_);
			}
			else
				res = ERROR_MEM_ERROR ;
			free(token);
		}
		else
			res = ERROR_MEM_ERROR ;
	}

	return res ;
}

//
//
//

int MAYK_GetWTT(unsigned char * tariff , int tarifLength , maykTariffWTT_t ** wtt , int * wttSize)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ((tariff) && (tarifLength >= 0) && (wttSize) && ((*wtt) == NULL) && (wtt))
	{
		(*wttSize) = 0 ;
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(char *));
		if (token)
		{
			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;
			char * tariff_ = calloc(tarifLength + 1 , sizeof(char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r(tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for WTT's
				int wttEntryNumb = -1 ;
				while(1)
				{
					int tokenIndex ;
					if (wttEntryNumb < 0)
						tokenIndex = 0 ;
					else if (wttEntryNumb > 0)
						tokenIndex = wttEntryNumb ;

					//searching the next entry
					for( ; tokenIndex<tokenNumb ; tokenIndex++ )
					{
						if(strstr(token[tokenIndex], "[WTT"))
						{
							wttEntryNumb = tokenIndex ;
							break;
						}
						if( tokenIndex == (tokenNumb - 1) )
						{
							wttEntryNumb = -1;
							break;
						}
					}

					//printf("WttEntryNumb founded [%d]\n", wttEntryNumb);

					//if WTT entry was founded
					if(wttEntryNumb > 0)
					{
						if(strcmp(token[++wttEntryNumb], "Value") == 0 )
						{
							wttEntryNumb++;
							(*wtt) = realloc((*wtt) , (++(*wttSize))*sizeof(maykTariffWTT_t) );
							//printf("wttSize [%d]\n", (*wttSize));
							if ( (*wtt) )
							{
								memset( &(*wtt)[(*wttSize) - 1 ] , 0 , sizeof(maykTariffWTT_t) );
								tokenIndex = wttEntryNumb ;

								//printf("search from [%d]\n", tokenIndex);

								if ( tokenIndex < tokenNumb )
								{
									if( !strstr(token[tokenIndex] , "[") )
									{
										(*wtt)[(*wttSize) - 1].weekDaysStamp = atoi(token[tokenIndex++]);
									}
								}
								if ( tokenIndex < tokenNumb )
								{
									if( !strstr(token[tokenIndex] , "[") )
									{
										(*wtt)[(*wttSize) - 1].weekEndStamp = atoi(token[tokenIndex++]);
									}
								}
								if ( tokenIndex < tokenNumb )
								{
									if( !strstr(token[tokenIndex] , "[") )
									{
										(*wtt)[(*wttSize) - 1].holidayStamp = atoi(token[tokenIndex++]);
									}
								}
								if ( tokenIndex < tokenNumb )
								{
									if( !strstr(token[tokenIndex] , "[") )
									{
										(*wtt)[(*wttSize) - 1].shaginMindStamp = atoi(token[tokenIndex++]);
									}
								}
								res = OK ;
							}
							else
							{
								res = ERROR_MEM_ERROR;
								break;
							}
						}

					}
					else
						break;

				}

				free(tariff_);
			}
			else
				res = ERROR_MEM_ERROR;

			free(token);
		}
		else
			res = ERROR_MEM_ERROR ;
	}


	return res ;
}

//
//
//

int MAYK_GetMTT(unsigned char * tariff , int tarifLength , maykTariffMTT_t * mtt)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ( (tariff) && (mtt) && (tarifLength != 0))
	{
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(char *));
		if (token)
		{
			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;

			char * tariff_ = calloc(tarifLength + 1 , sizeof(char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r(tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr);
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for MTT entry

				int mttEntryNumb= -1 ;

				int tokenIndex = 0 ;
				for (;tokenIndex < tokenNumb ; tokenIndex++)
				{
					if (strcmp(token[tokenIndex] , "[MTT]") == 0)
					{
						mttEntryNumb = tokenIndex;
						break;
					}
				}

				if(mttEntryNumb >= 0)
				{
					if (strcmp(token[ ++mttEntryNumb ],"Value") == 0 )
					{
						mttEntryNumb++;

						int mttIndex = 0 ;
						tokenIndex = mttEntryNumb ;
						while( (mttIndex < 12) && (tokenIndex < tokenNumb) && (!strstr(token[tokenIndex],"[")) )
						{
							mtt->wttNumb[mttIndex++] = atoi(token[tokenIndex++]);
						}

						res = OK;
					}
				}

				free(tariff_);
			}
			else
				res = ERROR_MEM_ERROR ;

			free(token);
		}
		else
			res = ERROR_MEM_ERROR ;
	}

	return res ;
}

//
//
//

int MAYK_GetLWJ(unsigned char * tariff , int tarifLength , maykTariffLWJ_t * lwj)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	if ( (tariff) && (lwj) && (tarifLength != 0))
	{
		//unsigned char * token[ACOUNTS_TARIFF_MAX_TOKENS];
		char ** token = NULL ;
		token = (char **)calloc(ACOUNTS_TARIFF_MAX_TOKENS , sizeof(char *));
		if (token)
		{
			char * pch ;
			char * lastPtr = NULL ;
			int tokenNumb = 0 ;

			char * tariff_ = calloc(tarifLength + 1 , sizeof(char));
			if (tariff_)
			{
				//parsing tariff
				memcpy(tariff_ , tariff , tarifLength);

				pch = strtok_r(tariff_ , " =,\r\n" , &lastPtr);
				while(pch)
				{
					token[tokenNumb++] = pch ;
					pch = strtok_r( NULL , " =,\r\n" , &lastPtr );
					if (tokenNumb == ACOUNTS_TARIFF_MAX_TOKENS)
						break;
				}

				//searching for LWJ entry

				int lwjEntryNumb= -1 ;

				int tokenIndex = 0 ;
				for (;tokenIndex < tokenNumb ; tokenIndex++)
				{
					if (strcmp(token[tokenIndex] , "[LWJ]") == 0)
					{
						lwjEntryNumb = tokenIndex;
						break;
					}
				}
				if(lwjEntryNumb >= 0)
				{
					if (strcmp(token[ ++lwjEntryNumb ],"Value") == 0 )
					{
						memset(lwj , 0 , sizeof(maykTariffLWJ_t) );
						lwj->mdNumb = atoi(token[ ++lwjEntryNumb ]);

						lwjEntryNumb++;

						int mdIndex = 0 ;
						tokenIndex = lwjEntryNumb ;
						while((tokenIndex < tokenNumb) && (mdIndex < (lwj->mdNumb)))
						{
							lwj->md[mdIndex].d = atoi(token[tokenIndex++]);
							lwj->md[mdIndex].m = atoi(token[tokenIndex++]);
							lwj->md[mdIndex++].t = atoi(token[tokenIndex++]);
						}
						res = OK;
					}
				}

				free(tariff_);
			}
			else
				res = ERROR_MEM_ERROR ;
			free(token);
		}
		else
			res = ERROR_MEM_ERROR ;
	}

	return res ;
}

//
//
//

int MAYK_GetIndications(unsigned char counterType , unsigned char * tariff, int tariffLength, maykIndication_t * maykIndication )
{
	int res = OK ;

	blockParsingResult_t * blockParsingResult = NULL ;
	int blockParsingResultLength = 0 ;


	if ( COMMON_SearchBlock( (char *)tariff , tariffLength , "indication", &blockParsingResult , &blockParsingResultLength ) == OK )
	{
		if ( blockParsingResult )
		{
			int counterTypeStringCounter = 0 ;

			int blockParsingResultIndex = 0 ;
			for ( ; blockParsingResultIndex < blockParsingResultLength ; ++blockParsingResultIndex )
			{
				if ( !strcmp( blockParsingResult[ blockParsingResultIndex ].variable , "CounterType" ) )
				{
					COMMON_STR_DEBUG( DEBUG_MAYK "MAYK counter type was found");
					if ( (++counterTypeStringCounter) > 1 )
					{
						COMMON_STR_DEBUG( DEBUG_MAYK "MAYK counter type was found more then 1 time. Break");
						res = ERROR_FILE_FORMAT ;
						break;
					}
					unsigned char parsedCounterType = 0 ;
					parsedCounterType = atoi( blockParsingResult[ blockParsingResultIndex ].value );
					if ( parsedCounterType != counterType )
					{
						COMMON_STR_DEBUG( DEBUG_MAYK "MAYK counter type is not equal current counter type. Break");
						res = ERROR_COUNTER_TYPE ;
					}
				}
				else if ( strstr( blockParsingResult[ blockParsingResultIndex ].variable , "Loop_" ) )
				{
					COMMON_STR_DEBUG( DEBUG_MAYK "MAYK Next loop was found");
					if ( strlen( blockParsingResult[ blockParsingResultIndex ].value ) )
					{
						BOOL lastCycleIteration = FALSE ;
						unsigned char * valuesArray = NULL ;
						int valuesArrayLength = 0 ;
						if ( COMMON_TranslateFormatStringToArray( blockParsingResult[ blockParsingResultIndex ].value , &valuesArrayLength , &valuesArray , MAYK_INDICATIONS_MAX_VALUE) == OK )
						{
							if ( (++(maykIndication->loopCounter)) == MAYK_INDICATIONS_MAX_LOOPS )
							{
								lastCycleIteration = TRUE ;
							}
							maykIndication->loop = ( maykIndicationLoop_t * )realloc( maykIndication->loop , sizeof( maykIndicationLoop_t ) * maykIndication->loopCounter ) ;
							if ( !maykIndication->loop )
							{
								fprintf( stderr , "ERROR - can not alloacate [%d] bytes\n" , sizeof( maykIndicationLoop_t ) * (maykIndication->loopCounter) ) ;
								EXIT_PATTERN;
							}
							maykIndication->loop[ maykIndication->loopCounter - 1 ].values = valuesArray ;
							maykIndication->loop[ maykIndication->loopCounter - 1 ].length = valuesArrayLength ;
						}

						if ( lastCycleIteration == TRUE )
						{
							break;
						}
					}
				}
				else if ( strstr( blockParsingResult[ blockParsingResultIndex ].variable , "Time" ) )
				{
					COMMON_STR_DEBUG( DEBUG_MAYK "MAYK loop auto switch time was found");
					if ( strlen( blockParsingResult[ blockParsingResultIndex ].value ) > 0 )
					{
						unsigned char loopAutoSwitchTime = (unsigned char)atoi( blockParsingResult[ blockParsingResultIndex ].value );
						if ( (loopAutoSwitchTime > 0) && (loopAutoSwitchTime <= 60) )
						{
							maykIndication->loopAutoSwitchTime = loopAutoSwitchTime ;
						}
					}
				}

			}//for(;;)

			if ( counterTypeStringCounter != 1 )
			{
				res = ERROR_FILE_FORMAT ;
			}

			free ( blockParsingResult );
			blockParsingResult = NULL ;
			blockParsingResultLength = 0 ;
		}
	}


	if ( res != OK )
	{
		if ( maykIndication->loop )
		{
			int indicationIndex = 0 ;
			for ( ; indicationIndex < maykIndication->loopCounter ; ++indicationIndex )
			{
				if ( maykIndication->loop[ indicationIndex ].values )
				{
					free( maykIndication->loop[ indicationIndex ].values );
					maykIndication->loop[ indicationIndex ].values = NULL ;
					maykIndication->loop[ indicationIndex ].length = 0 ;
				}
			}

			free( maykIndication->loop );
			maykIndication->loop = NULL ;
			maykIndication->loopCounter = 0 ;
		}
	}


	return res ;
}

//
//
//

unsigned char MAYK_GetJournalPqpEvType(unsigned char value)
{
	switch(value)
	{
		default:
		case 0x00: return EVENT_PQP_VOLTAGE_INCREASE_F1;
		case 0x01: return EVENT_PQP_VOLTAGE_I_NORMALISE_F1;
		case 0x02: return EVENT_PQP_VOLTAGE_INCREASE_F2;
		case 0x03: return EVENT_PQP_VOLTAGE_I_NORMALISE_F2;
		case 0x04: return EVENT_PQP_VOLTAGE_INCREASE_F3;
		case 0x05: return EVENT_PQP_VOLTAGE_I_NORMALISE_F3;

		case 0x0C: return EVENT_PQP_AVE_VOLTAGE_INCREASE_F1;
		case 0x0D: return EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F1;
		case 0x0E: return EVENT_PQP_AVE_VOLTAGE_INCREASE_F2;
		case 0x0F: return EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F2;
		case 0x10: return EVENT_PQP_AVE_VOLTAGE_INCREASE_F3;
		case 0x11: return EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F4;

		case 0x18: return EVENT_PQP_VOLTAGE_DECREASE_F1;
		case 0x19: return EVENT_PQP_VOLTAGE_D_NORMALISE_F1;
		case 0x1A: return EVENT_PQP_VOLTAGE_DECREASE_F2;
		case 0x1B: return EVENT_PQP_VOLTAGE_D_NORMALISE_F2;
		case 0x1C: return EVENT_PQP_VOLTAGE_DECREASE_F3;
		case 0x1D: return EVENT_PQP_VOLTAGE_D_NORMALISE_F3;

		case 0x24: return EVENT_PQP_AVE_VOLTAGE_DECREASE_F1;
		case 0x25: return EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F1;
		case 0x26: return EVENT_PQP_AVE_VOLTAGE_DECREASE_F2;
		case 0x27: return EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F2;
		case 0x28: return EVENT_PQP_AVE_VOLTAGE_DECREASE_F3;
		case 0x29: return EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F4;

		case 0x30: return EVENT_PQP_FREQ_DEVIATION_02;
		case 0x31: return EVENT_PQP_FREQ_NORMALISE_02;
		case 0x32: return EVENT_PQP_FREQ_DEVIATION_04;
		case 0x33: return EVENT_PQP_FREQ_NORMALISE_04;

		case 0x38: return EVENT_PQP_KNOP_INCREASE_2;
		case 0x39: return EVENT_PQP_KNOP_NORMALISE_2;
		case 0x3A: return EVENT_PQP_KNOP_INCREASE_4;
		case 0x3B: return EVENT_PQP_KNOP_NORMALISE_4;

		case 0x3C: return EVENT_PQP_KNNP_INCREASE_2;
		case 0x3D: return EVENT_PQP_KNNP_NORMALISE_2;
		case 0x3E: return EVENT_PQP_KNNP_INCREASE_4;
		case 0x3F: return EVENT_PQP_KNNP_NORMALISE_4;

		case 0x4A: return EVENT_PQP_KDF_INCREASE_F1;
		case 0x4B: return EVENT_PQP_KDF_NORMALISE_F1;
		case 0x4C: return EVENT_PQP_KDF_INCREASE_F2;
		case 0x4D: return EVENT_PQP_KDF_NORMALISE_F2;
		case 0x4E: return EVENT_PQP_KDF_INCREASE_F3;
		case 0x4F: return EVENT_PQP_KDF_NORMALISE_F3;

		case 0x50: return EVENT_PQP_DDF_INCREASE_F1;
		case 0x51: return EVENT_PQP_DDF_NORMALISE_F1;
		case 0x52: return EVENT_PQP_DDF_INCREASE_F2;
		case 0x53: return EVENT_PQP_DDF_NORMALISE_F2;
		case 0x54: return EVENT_PQP_DDF_INCREASE_F3;
		case 0x55: return EVENT_PQP_DDF_NORMALISE_F3;
	}
}

//
//
//

void MAYK_FillAsByteArray(int field, unsigned char * data, mayk_PSM_Parametres_t * psmParams){
	DEB_TRACE(DEB_IDX_MAYAK)

	switch (field){
		case MAYK_PC_FLD_MASK_OTHER:
			data[0] = 0x0;
			data[1] = psmParams->maskOther;
			break;

		case MAYK_PC_FLD_MASK_RULES:
			data[0] = 0x0;
			data[1] = psmParams->maskRules;
			break;


		case MAYK_PC_FLD_MASK_PQ:
			data[0] = (psmParams->maskPq>>24) & 0xFF;
			data[1] = (psmParams->maskPq>>16) & 0xFF;
			data[2] = (psmParams->maskPq>>8) & 0xFF;
			data[3] = (psmParams->maskPq & 0xFF);
			break;

		case MAYK_PC_FLD_MASK_E30:
			data[0] = (psmParams->maskE30>>8) & 0xFF;
			data[1] = (psmParams->maskE30 & 0xFF);
			break;

		case MAYK_PC_FLD_MASK_ED:
			data[0] = (psmParams->maskEd>>8) & 0xFF;
			data[1] = (psmParams->maskEd & 0xFF);
			break;

		case MAYK_PC_FLD_MASK_EM:
			data[0] = (psmParams->maskEm>>8) & 0xFF;
			data[1] = (psmParams->maskEm & 0xFF);
			break;

//		case MAYK_PC_FLD_MASK_UI:
//			data[0] = 0x0;
//			data[1] = (psmParams->maskUmax & 0xFF);
//			data[2] = 0x0;
//			data[3] = (psmParams->maskUmin & 0xFF);
//			data[4] = 0x0;
//			data[5] = (psmParams->maskImax & 0xFF);
//			break;

		case MAYK_PC_FLD_MASK_UMAX:
			data[0] = 0x0;
			data[1] = (psmParams->maskUmax & 0xFF);
			break;

		case MAYK_PC_FLD_MASK_UMIN:
			data[0] = 0x0;
			data[1] = (psmParams->maskUmin & 0xFF);
			break;

		case MAYK_PC_FLD_MASK_IMAX:
			data[0] = 0x0;
			data[1] = (psmParams->maskImax & 0xFF);
			break;

		case MAYK_PC_FLD_LIMIT_PQ:
			{
			int offset = 0;

			int p=0;
			int e=0;

			for (p=0;p<4;p++){
				for (e=0;e<4;e++){
					offset = (16*p) + (4*e);
					data[offset+0] = ((psmParams->limitsPq[p][e] >> 24) & 0xFF);
					data[offset+1] = ((psmParams->limitsPq[p][e] >> 16) & 0xFF);
					data[offset+2] = ((psmParams->limitsPq[p][e] >> 8) & 0xFF);
					data[offset+3] = ((psmParams->limitsPq[p][e] & 0xFF));
				}
			}
			}
			break;

		case MAYK_PC_FLD_LIMIT_E30:
		{
			int offset = 0;

			int p = 0;
			int e = 0 ;

			for (p=0;p<5;p++)
			{
				for (e=0;e<4;e++){
					offset = (16*p) + (4*e);
					data[offset+0] = ((psmParams->limitsE30[p][e] >> 24) & 0xFF);
					data[offset+1] = ((psmParams->limitsE30[p][e] >> 16) & 0xFF);
					data[offset+2] = ((psmParams->limitsE30[p][e] >> 8) & 0xFF);
					data[offset+3] = ((psmParams->limitsE30[p][e] & 0xFF));
				}
			}
		}
		break;

		case MAYK_PC_FLD_LIMIT_ED + 1:
		case MAYK_PC_FLD_LIMIT_ED + 2:
		case MAYK_PC_FLD_LIMIT_ED + 3:
		case MAYK_PC_FLD_LIMIT_ED + 4:
		case MAYK_PC_FLD_LIMIT_ED + 5:
		case MAYK_PC_FLD_LIMIT_ED + 6:
		case MAYK_PC_FLD_LIMIT_ED + 7:
		case MAYK_PC_FLD_LIMIT_ED + 8:
		case MAYK_PC_FLD_LIMIT_ED + 9:
		{
			int tIndex = field - MAYK_PC_FLD_LIMIT_ED;
			int offset = 0;
			unsigned int value = 0xFFFFFFFF;

			int p = 0 ;
			int e = 0 ;
			for ( p=0;p<5;p++)
			{
				for ( e=0;e<4;e++){
					offset = (16*p) + (4*e);

					switch (tIndex){
						case 1: value = psmParams->limitsEdT1[p][e]; break;
						case 2: value = psmParams->limitsEdT2[p][e]; break;
						case 3: value = psmParams->limitsEdT3[p][e]; break;
						case 4: value = psmParams->limitsEdT4[p][e]; break;
						case 5: value = psmParams->limitsEdT5[p][e]; break;
						case 6: value = psmParams->limitsEdT6[p][e]; break;
						case 7: value = psmParams->limitsEdT7[p][e]; break;
						case 8: value = psmParams->limitsEdT8[p][e]; break;
						case 9: value = psmParams->limitsEdTS[p][e]; break;
					}

					data[offset+0] = ((value >> 24) & 0xFF);
					data[offset+1] = ((value >> 16) & 0xFF);
					data[offset+2] = ((value >> 8) & 0xFF);
					data[offset+3] = ((value & 0xFF));
				}
			}
		}
		break;

		case MAYK_PC_FLD_LIMIT_EM + 1:
		case MAYK_PC_FLD_LIMIT_EM + 2:
		case MAYK_PC_FLD_LIMIT_EM + 3:
		case MAYK_PC_FLD_LIMIT_EM + 4:
		case MAYK_PC_FLD_LIMIT_EM + 5:
		case MAYK_PC_FLD_LIMIT_EM + 6:
		case MAYK_PC_FLD_LIMIT_EM + 7:
		case MAYK_PC_FLD_LIMIT_EM + 8:
		case MAYK_PC_FLD_LIMIT_EM + 9:
			{
			int tIndex = field - MAYK_PC_FLD_LIMIT_EM;
			int offset = 0;
			unsigned int value = 0xFFFFFFFF;

			int p = 0 ;
			int e = 0 ;
			for ( p=0;p<5;p++){
				for ( e=0;e<4;e++){
					offset = (16*p) + (4*e);

					switch (tIndex){
						case 1: value = psmParams->limitsEmT1[p][e]; break;
						case 2: value = psmParams->limitsEmT2[p][e]; break;
						case 3: value = psmParams->limitsEmT3[p][e]; break;
						case 4: value = psmParams->limitsEmT4[p][e]; break;
						case 5: value = psmParams->limitsEmT5[p][e]; break;
						case 6: value = psmParams->limitsEmT6[p][e]; break;
						case 7: value = psmParams->limitsEmT7[p][e]; break;
						case 8: value = psmParams->limitsEmT8[p][e]; break;
						case 9: value = psmParams->limitsEmTS[p][e]; break;
					}

					data[offset+0] = ((value >> 24) & 0xff);
					data[offset+1] = ((value >> 16) & 0xff);
					data[offset+2] = ((value >> 8) & 0xff);
					data[offset+3] = ((value & 0xff));
				}
			}
			}
			break;

		case MAYK_PC_FLD_LIMIT_UI:
			{
			int offset = 0;
			unsigned int value = 0xFFFFFFFF;

			int p = 0 ;
			int e = 0 ;

			for ( p=0;p<3;p++){
				for ( e=0;e<3;e++){
					offset = (12*p) + (4*e);
					switch (p){
						case 0: value = psmParams->limitsUmax[e]; break;
						case 1: value = psmParams->limitsUmin[e]; break;
						case 2: value = psmParams->limitsImax[e]; break;
					}

					data[offset+0] = ((value >> 24) & 0xff);
					data[offset+1] = ((value >> 16) & 0xff);
					data[offset+2] = ((value >> 8) & 0xff);
					data[offset+3] = ((value & 0xff));
				}
			}
			}
			break;

	}
}

//
//
//

int MAYK_SetPowerMasks1( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKOTHER , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMasks2( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKRULES , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskPq( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKPQ , mask , 4) ;
}

//
//
//

int MAYK_SetPowerMaskE30( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKE30 , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskEd( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKED , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskEm( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKEM , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskUmax( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKUMAX , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskUmin( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKUMIN , mask , 2) ;
}

//
//
//

int MAYK_SetPowerMaskImax( counter_t * counter, counterTransaction_t ** transaction , unsigned char * mask )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_MASKIMAX , mask , 2) ;
}

//
//
//

int MAYK_SetTimingsMR_P( counter_t * counter, counterTransaction_t ** transaction , unsigned int tP)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	unsigned char controls = (tP & 0xff) ;
	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_TIMING_MR_P , &controls , 1) ;
}

//
//
//

int MAYK_SetTimingsUmin( counter_t * counter, counterTransaction_t ** transaction , unsigned int tUmin)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	unsigned char controls = tUmin & 0xff ;
	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_TIMING_T_U_MIN , &controls , 1) ;
}

//
//
//

int MAYK_SetTimingsUmax( counter_t * counter, counterTransaction_t ** transaction , unsigned int tUmax)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	unsigned char controls = tUmax & 0xff ;
	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_TIMING_T_U_MAX , &controls , 1) ;
}

//
//
//

int MAYK_SetTimingsDelayOff( counter_t * counter, counterTransaction_t ** transaction , unsigned int tOff)
{
	DEB_TRACE(DEB_IDX_MAYAK)

	unsigned char controls = tOff & 0xff ;
	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_TIMING_T_DELAY_OFF , &controls , 1) ;
}

//
//
//

int MAYK_SetPowerLimitsPq( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_P , limits , 64) ;
}

//
//
//

int MAYK_SetPowerLimitsE30( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_E30 , limits , 80) ;
}

//
//
//

int MAYK_SetPowerLimitsEd( counter_t * counter, counterTransaction_t ** transaction , int tariffIndex , unsigned char * limits )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	switch( tariffIndex )
	{
		case 1: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T1 , limits , 80) ; break;
		case 2: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T2 , limits , 80) ; break;
		case 3: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T3 , limits , 80) ; break;
		case 4: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T4 , limits , 80) ; break;
		case 5: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T5 , limits , 80) ; break;
		case 6: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T6 , limits , 80) ; break;
		case 7: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T7 , limits , 80) ; break;
		case 8: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_T8 , limits , 80) ; break;
		case 9: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_ED_TS , limits , 80) ; break;
		default: break;
	}

	return res ;
}

//
//
//

int MAYK_SetPowerLimitsEm( counter_t * counter, counterTransaction_t ** transaction , int tariffIndex , unsigned char * limits )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	int res = ERROR_GENERAL ;

	switch( tariffIndex )
	{
		case 1: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T1 , limits , 80) ; break;
		case 2: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T2 , limits , 80) ; break;
		case 3: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T3 , limits , 80) ; break;
		case 4: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T4 , limits , 80) ; break;
		case 5: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T5 , limits , 80) ; break;
		case 6: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T6 , limits , 80) ; break;
		case 7: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T7 , limits , 80) ; break;
		case 8: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_T8 , limits , 80) ; break;
		case 9: res = MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_EM_TS , limits , 80) ; break;
		default: break;
	}

	return res ;
}

//
//
//

int MAYK_SetPowerLimitsUI( counter_t * counter, counterTransaction_t ** transaction , unsigned char * limits )
{
	DEB_TRACE(DEB_IDX_MAYAK)

	return MAYK_CmdSetPowerControls( counter, transaction , MAYK_MCPCR_OFFSET_LIMITS_U_MAX , limits , 36) ;
}
//EOF
