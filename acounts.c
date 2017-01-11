/**
 *  application code v.1
 *  for uspd micron 2.0x
 *  2012 - January
 *  NPO Frunze
 *  RUSSIA NIGNY NOVGOROD
 *  OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
 *
 **/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#include "acounts.h"
#include "common.h"
#include "debug.h"

//
//
//

static pthread_mutex_t acountMux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t acountLowMux = PTHREAD_MUTEX_INITIALIZER;

void ACOUNT_Protect() {
	DEB_TRACE(DEB_IDX_ACCOUNTS)

	pthread_mutex_lock(&acountMux);
}

//
//
//

void ACOUNT_Unprotect() {
	DEB_TRACE(DEB_IDX_ACCOUNTS)

	pthread_mutex_unlock(&acountMux);
}


void ACOUNT_Protect2() {
	DEB_TRACE(DEB_IDX_ACCOUNTS)

	pthread_mutex_lock(&acountLowMux);
}

//
//
//

void ACOUNT_Unprotect2() {
	DEB_TRACE(DEB_IDX_ACCOUNTS)

	pthread_mutex_unlock(&acountLowMux);
}

//
//
//

int ACOUNT_AddNew(char * buffer , int bufferLength , acountsDbidsList_t * acountsDbidsList , int part)
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew start");

	int res = ERROR_GENERAL ;

	if ( !buffer || !bufferLength || !acountsDbidsList )
		return res ;

	ACOUNT_Protect();

	acountsMap_t * acountsMap = NULL ;
	int acountsMapLength = 0 ;
	if ( ACOUNT_GetMap( &acountsMap , &acountsMapLength , part ) == OK )
	{
		if ( acountsMap )
		{
			char fileName[ACOUNTS_FILE_NAME_SIZE];
			memset( fileName , 0 , ACOUNTS_FILE_NAME_SIZE);
			if ( ACOUNT_FindFreeName( fileName, part) == OK )
			{
				if ( COMMON_WriteFileToFS( (unsigned char* )buffer , bufferLength , (unsigned char *)fileName ) == OK )
				{
					COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew writing [%s] OK " , fileName);

					//searching for old entry for every counter
					int dbidIndex = 0 ;
					for( ; dbidIndex < acountsDbidsList->counterDbidsNumb ; ++dbidIndex )
					{
						int oldIndex = -1 ;
						int idx = 0 ;
						for( ; idx < acountsMapLength ; ++idx )
						{
							if ( acountsMap[ idx ].counterDbId == acountsDbidsList->counterDbids[ dbidIndex ] )
							{
								oldIndex = idx ;
								break;
							}
						}

						if ( oldIndex >= 0 )
						{
							// this counter was in list
							acountsMap[ oldIndex ].status = ACOUNTS_STATUS_IN_QUEUE ;
							char oldFileName[ ACOUNTS_FILE_NAME_SIZE ];
							memcpy( oldFileName                     , acountsMap[ oldIndex ].fileName , ACOUNTS_FILE_NAME_SIZE );
							memcpy( acountsMap[ oldIndex ].fileName , fileName                        , ACOUNTS_FILE_NAME_SIZE );
							if ( ACOUNT_FindOverlapInMap( acountsMap , acountsMapLength , oldFileName ) == ERROR_GENERAL )
							{
								COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew() removing [%s]" , oldFileName );
								unlink( oldFileName );
							}
						}
						else
						{
							//add new line in list
							acountsMap = (acountsMap_t *)realloc(acountsMap , (++acountsMapLength)*sizeof( acountsMap_t ) );
							if ( acountsMap )
							{
								acountsMap[ acountsMapLength - 1 ].counterDbId = acountsDbidsList->counterDbids[ dbidIndex ] ;
								acountsMap[ acountsMapLength - 1 ].status = ACOUNTS_STATUS_IN_QUEUE ;
								memset( acountsMap[ acountsMapLength - 1 ].fileName , 0 , ACOUNTS_FILE_NAME_SIZE );
								snprintf( (char *)acountsMap[ acountsMapLength - 1 ].fileName , ACOUNTS_FILE_NAME_SIZE , "%s" , fileName );
							}
							else
							{
								EXIT_PATTERN;
							}
						}
					}
					res = ACOUNT_SaveMap( acountsMap , acountsMapLength , part );

					if ( res != OK )
					{
						unlink( fileName );
					}
				}
			}
			free( acountsMap );
		}
	}
	else
	{
		COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew adding buffer in the first time");

		char fileName[ACOUNTS_FILE_NAME_SIZE];
		memset( fileName , 0 , ACOUNTS_FILE_NAME_SIZE);
		if ( ACOUNT_FindFreeName( fileName, part) == OK )
		{
			if ( COMMON_WriteFileToFS( (unsigned char* )buffer , bufferLength , (unsigned char *)fileName ) == OK )
			{
				COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew writing [%s] OK " , fileName);
				int dbidIndex = 0 ;
				for( ; dbidIndex < acountsDbidsList->counterDbidsNumb ; ++dbidIndex )
				{
					//add new line in list
					acountsMap = (acountsMap_t *)realloc(acountsMap , (++acountsMapLength)*sizeof( acountsMap_t ) );
					if ( acountsMap )
					{
						acountsMap[ acountsMapLength - 1 ].counterDbId = acountsDbidsList->counterDbids[ dbidIndex ] ;
						acountsMap[ acountsMapLength - 1 ].status = ACOUNTS_STATUS_IN_QUEUE ;
						memset( acountsMap[ acountsMapLength - 1 ].fileName , 0 , ACOUNTS_FILE_NAME_SIZE );
						snprintf( (char *)acountsMap[ acountsMapLength - 1 ].fileName , ACOUNTS_FILE_NAME_SIZE , "%s" , fileName );
					}
					else
					{
						EXIT_PATTERN;
					}
				}

				if ( acountsMap )
				{
					res = ACOUNT_SaveMap( acountsMap , acountsMapLength , part );
					if ( res != OK )
					{
						unlink( fileName );
					}
					free(acountsMap);
					acountsMap = NULL ;
				}
			}
		}
	}
	ACOUNT_Unprotect();

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_AddNew() : exit with res [%d]", res);
	return res ;
}

//
//
//

int ACOUNT_Remove(acountsDbidsList_t * acountsDbidsList , int part)
{
	int res = ERROR_GENERAL ;

	ACOUNT_Protect();

	acountsMap_t * acountsMap = NULL ;
	int acountsMapLength = 0 ;
	if ( ACOUNT_GetMap( &acountsMap , &acountsMapLength , part ) == OK )
	{
		if ( acountsMap )
		{
			int changingFlag = 0 ;

			int counterIndex = 0 ;
			for ( ; counterIndex < acountsDbidsList->counterDbidsNumb ; ++counterIndex )
			{
				int entryIndex = 0 ;
				for ( ; entryIndex < acountsMapLength ; ++entryIndex )
				{
					if ( acountsMap[ entryIndex ].counterDbId == acountsDbidsList->counterDbids[ counterIndex ] )
					{
						changingFlag = 1 ;
						acountsMap[ entryIndex ].status = ACOUNTS_STATUS_REMOVE ;
						if ( ACOUNT_FindOverlapInMap( acountsMap , acountsMapLength , (char *)acountsMap[ entryIndex ].fileName) == ERROR_GENERAL )
						{
							unlink( acountsMap[ entryIndex ].fileName );
						}
					}
				}

			}
			res = ACOUNT_SaveMap( acountsMap , acountsMapLength , part );
			free( acountsMap );
		}
	}

	ACOUNT_Unprotect();

	return res ;
}

//
//
//

int ACOUNT_StatusSet( unsigned long counterDbId , int status , int part )
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_StatusSet() : start" );

	int res = ERROR_GENERAL ;

	ACOUNT_Protect();

	acountsMap_t * acountsMap = NULL ;
	int acountsMapLength = 0 ;
	if ( ACOUNT_GetMap( &acountsMap , &acountsMapLength , part ) == OK )
	{
		if (acountsMap)
		{
			int counterIndex = 0 ;
			for( ; counterIndex < acountsMapLength ; ++counterIndex )
			{
				if ( acountsMap[ counterIndex ].counterDbId == counterDbId )
				{
					acountsMap[ counterIndex ].status = status ;
					res = ACOUNT_SaveMap( acountsMap , acountsMapLength , part );
					break;
				}
			}
			free( acountsMap );
		}
	}
	ACOUNT_Unprotect();

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_StatusSet() : exit with res [%d]", res);

	return res ;
}

//
//
//

int ACOUNT_StatusGet( unsigned long counterDbId , int * status , int part )
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_StatusGet() : start" );

	int res = ERROR_GENERAL ;

	ACOUNT_Protect();

	acountsMap_t * acountsMap = NULL ;
	int acountsMapLength = 0 ;
	if ( ACOUNT_GetMap( &acountsMap , &acountsMapLength , part ) == OK )
	{
		if (acountsMap)
		{
			int counterIndex = 0 ;
			for( ; counterIndex < acountsMapLength ; ++counterIndex )
			{
				if ( acountsMap[ counterIndex ].counterDbId == counterDbId )
				{
					(*status) = acountsMap[ counterIndex ].status ;
					res = OK ;
					break ;
				}
			}
			free( acountsMap );
		}
	}
	ACOUNT_Unprotect();

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_StatusGet() : exit with res [%d]", res);

	return res ;
}

//
//
//

int ACOUNT_FindFreeName( char * fileName , int part )
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	int res = ERROR_GENERAL ;

	if (!fileName)	{
		return res ;
	}

	int idx = 0 ;
	for( ; idx < ACOUNTS_MAX_FILES_NUMB ; ++idx )
	{
		char fileTmpName[ACOUNTS_FILE_NAME_SIZE];
		memset(fileTmpName, 0 , ACOUNTS_FILE_NAME_SIZE * sizeof(unsigned char));
		switch( part )
		{
			case ACOUNTS_PART_TARIFF:
				snprintf(fileTmpName , ACOUNTS_FILE_NAME_SIZE, "%s%d" , ACOUNTS_TARIFF_NAME_TEAMPLATE , idx) ;
				break;

			case ACOUNTS_PART_POWR_CTRL:
				snprintf(fileTmpName , ACOUNTS_FILE_NAME_SIZE, "%s%d" , ACOUNTS_PWRCTRL_NAME_TEAMPLATE , idx) ;
				break;

			case ACOUNTS_PART_FIRMWARE:
				snprintf(fileTmpName , ACOUNTS_FILE_NAME_SIZE, "%s%d" , ACOUNTS_FIRMWARE_NAME_TEAMPLATE , idx) ;
				break;

			case ACOUNTS_PART_USER_COMMANDS:
				snprintf(fileTmpName , ACOUNTS_FILE_NAME_SIZE, "%s%d" , ACOUNTS_USER_COMMANDS_NAME_TEAMPLATE , idx) ;
				break;

			default:
				return res ;
				break;
		}
		int fd = open((char *)fileTmpName , O_RDONLY);
		if ( fd >=0 )
		{
			close(fd);
		}
		else
		{
			memcpy(fileName , fileTmpName , strlen((char *)fileTmpName) + 1);
			res = OK ;
			break;
		}
	}
	return res ;
}

//
//
//

int ACOUNT_Get( unsigned long counterDbId , unsigned char ** buffer, int * bufferLength , int part )
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	int res = ERROR_GENERAL ;

	ACOUNT_Protect();

	acountsMap_t * acountsMap = NULL ;
	int acountsMapLength = 0 ;
	if ( ACOUNT_GetMap( &acountsMap , &acountsMapLength , part ) == OK )
	{
		if ( acountsMap )
		{
			int entryIndex = 0 ;
			for( ; entryIndex < acountsMapLength ; ++entryIndex )
			{
				if ( acountsMap[ entryIndex ].counterDbId == counterDbId )
				{
					if (( acountsMap[ entryIndex ].status == ACOUNTS_STATUS_IN_QUEUE ) || ( acountsMap[ entryIndex ].status == ACOUNTS_STATUS_IN_PROCESS ) )
					{
						res = COMMON_GetFileFromFS( buffer , bufferLength , (unsigned char *)acountsMap[ entryIndex ].fileName);
						break;
					}
				}
			}
			free(acountsMap);
		}
	}
	ACOUNT_Unprotect();

	return res ;
}

//
//
//

int ACOUNT_GetMap( acountsMap_t ** acountsMap , int * acountsMapLength , int part)
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_GetMap start");

	int res = ERROR_GENERAL ;

	ACOUNT_Protect2();

	char mapFileName[ ACOUNTS_FILE_NAME_SIZE ];
	memset( mapFileName , 0 , ACOUNTS_FILE_NAME_SIZE );
	switch( part )
	{
		case ACOUNTS_PART_TARIFF:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_TARIFF_MAP_PATH );
			break;

		case ACOUNTS_PART_POWR_CTRL:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_PWRCTRL_MAP_PATH );
			break;

		case ACOUNTS_PART_FIRMWARE:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_FIRMWARE_MAP_PATH );
			break;

		case ACOUNTS_PART_USER_COMMANDS:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_USER_COMMANDS_MAP_PATH );
			break;

		default:
			break;
	}

	int fd = open( mapFileName , O_RDONLY );
	if ( fd >= 0 )
	{
		int blockSize = sizeof(acountsMap_t) ;
		struct stat fd_stat;
		memset(&fd_stat , 0 , sizeof(stat));
		fstat(fd , &fd_stat);
		int totalSize = fd_stat.st_size ;
		int blockNumb = totalSize / blockSize ;
		COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_GetMap: total blocks number [%d]", blockNumb);

		(*acountsMap) = (acountsMap_t *)malloc( totalSize );
		if ( (*acountsMap) == NULL )
		{
			EXIT_PATTERN;
		}

		res = OK ;
		memset( (*acountsMap) , 0 , totalSize );

		int idx = 0 ;
		for( ; idx < blockNumb ; ++idx )
		{
			acountsMap_t temp ;
			memset( &temp , 0 , blockSize );
			if ( read(fd , &temp , blockSize) == blockSize )
			{
				unsigned int crc = 0 ;
				ACOUNT_GetOnceMapCrc( &temp , &crc );
				if( crc == temp.crc)
				{
					COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_GetMap: CRC is OK");
					COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_GetMap: stuff elem N [%d]", (*acountsMapLength));
					memcpy(&(*acountsMap)[(*acountsMapLength)++] , &temp , blockSize);
				}
			}
			else
			{
				COMMON_STR_DEBUG(DEBUG_ACOUNTS "Error reading. Try to exit from ACOUNT_StuffPsmfArray()");
				free( (*acountsMap) );
				res = ERROR_GENERAL ;
				break;
			}
		}

		close(fd);
	}
	ACOUNT_Unprotect2();

	return res ;
}

//
//
//

int ACOUNT_SaveMap( acountsMap_t * acountsMap , int acountsMapLength , int part)
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	int res = ERROR_GENERAL ;


	ACOUNT_Protect2();

	char mapFileName[ ACOUNTS_FILE_NAME_SIZE ];
	memset( mapFileName , 0 , ACOUNTS_FILE_NAME_SIZE );
	switch( part )
	{
		case ACOUNTS_PART_TARIFF:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_TARIFF_MAP_PATH );
			break;

		case ACOUNTS_PART_POWR_CTRL:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_PWRCTRL_MAP_PATH );
			break;

		case ACOUNTS_PART_FIRMWARE:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_FIRMWARE_MAP_PATH );
			break;

		case ACOUNTS_PART_USER_COMMANDS:
			snprintf( mapFileName , ACOUNTS_FILE_NAME_SIZE , "%s" , ACOUNTS_USER_COMMANDS_MAP_PATH );
			break;

		default:
			break;
	}

	int fd = open(mapFileName , O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if ( fd >= 0 )
	{
		if ( acountsMapLength > 0)
		{
			if ( acountsMap )
			{
				int idx = 0 ;
				for( ; idx < acountsMapLength ; idx++)
				{
					if ( acountsMap[idx].status != ACOUNTS_STATUS_REMOVE )
					{
						if ( ACOUNT_GetOnceMapCrc( &acountsMap[idx] , &acountsMap[idx].crc ) == OK )
						{
							if ( write(fd , &acountsMap[idx] , sizeof(acountsMap_t)) == sizeof(acountsMap_t) )
							{
								res = OK ;
							}
							else
							{
								res = ERROR_GENERAL ;
								break;
							}
						}
					}
				}
			}
		}
		else
		{
			res = OK ;
		}
		close(fd);
	}

	ACOUNT_Unprotect2();

	return res ;
}

//
//
//

int ACOUNT_GetOnceMapCrc( acountsMap_t * acountsMap , unsigned int * crc )
{
	DEB_TRACE(DEB_IDX_ACCOUNTS);

	int res = ERROR_GENERAL  ;

	if (crc && acountsMap)
	{
		unsigned char * buf = (unsigned char *)malloc( sizeof(unsigned char) * sizeof(acountsMap_t) );

		if (buf)
		{
			memset(buf , 0 , sizeof(acountsMap_t));

			int bufPos = 0 ;
			memcpy(&buf[bufPos] , (unsigned char *)&acountsMap->counterDbId , sizeof(unsigned long));
			bufPos += sizeof(unsigned long) ;

			memcpy(&buf[bufPos] , (unsigned char *)&acountsMap->status , sizeof(unsigned char));
			bufPos += sizeof(unsigned char) ;

			memcpy(&buf[bufPos] , acountsMap->fileName , sizeof(unsigned char) * ACOUNTS_FILE_NAME_SIZE);
			bufPos += ACOUNTS_FILE_NAME_SIZE ;

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
			res = OK ;
		}
		else
			res = ERROR_MEM_ERROR ;
	}
	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_GetOnceMapCrc() : exit with res [%d]", res);
	return res ;
}

//
//
//

int ACOUNT_FindOverlapInMap( acountsMap_t * acountMap , int acountMapLength , char * fileName )
{
	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_FindOverlapInMap() start" );

	int res = ERROR_GENERAL ;

	if ( !acountMap || !fileName || !acountMapLength )
		return res ;

	int idx = 0 ;
	for ( ; idx < acountMapLength ; ++idx )
	{
		if ( acountMap[ idx ].status != ACOUNTS_STATUS_REMOVE )
		{
			if ( !strcmp(acountMap[ idx ].fileName , fileName ) )
			{
				return OK ;
			}
		}
	}

	COMMON_STR_DEBUG(DEBUG_ACOUNTS "ACOUNT_FindOverlapInMap() : exit with res [%d]", res);
	return res ;
}

//EOF
