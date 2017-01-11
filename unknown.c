/**
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

#include "unknown.h"
#include "storage.h"
#include "common.h"
#include "mayk.h"
#include "ujuk.h"
#include "gubanov.h"
#include "debug.h"

//
//
//

int UNKNOWN_AskForMayk(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UNKNOWN)

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForMayk() started" );

	int res = ERROR_GENERAL ;

	unsigned int counterAddress = 0;
	unsigned char tempCmd[64];
	memset( tempCmd , 0 , 64 );
	unsigned int tempCmdLength = 0;
	MAYK_GetCounterAddress( counter, &counterAddress );

	//set default password
	if (counter->useDefaultPass == TRUE)
	{
		memset( counter->password1 , 0 , PASSWORD_SIZE );
		memset( counter->password2 , 0 , PASSWORD_SIZE );
	}

	//set counter address
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 24)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 16)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) ((counterAddress >> 8)& 0xFF);
	tempCmd[ tempCmdLength++ ] = (unsigned char) (counterAddress & 0xFF);

	//set command index

	tempCmd[ tempCmdLength++ ] = (unsigned char)((UNKNOWN_ASK_MAYK >> 8) & 0xFF) ;
	tempCmd[ tempCmdLength++ ] = (unsigned char) (UNKNOWN_ASK_MAYK & 0xFF) ;

	unsigned char crc = MAYK_GetCRC( tempCmd , tempCmdLength );
	tempCmd[ tempCmdLength++ ] = crc ;


	(*transaction)->transactionType = TRANSACTION_UNKNOWN_ASK_FOR_MAYK;
	(*transaction)->waitSize = 0 ;
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
		COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN_AskForMayk request buf: " , (*transaction)->command , (*transaction)->commandSize );
		res = OK ;
	}
	else
		res = ERROR_MEM_ERROR ;

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForMayk() exit with errorcode %d" , res );

	return res;
}

//
//
//

int UNKNOWN_AskForGubanov(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UNKNOWN)

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForGubanov() started" );

	int res = ERROR_GENERAL ;

	unsigned char tempCommand[64];
	memset( tempCommand , 0 , 64 );
	int commandSize = 0 ;

	tempCommand[ commandSize++ ] = GUBANOV_GetStartCommandSymbol( counter , FALSE );

	GUBANOV_GetCounterAddress( counter , &tempCommand[ commandSize ] );
	commandSize += GUBANOV_SIZE_OF_ADDRESS ;

	//set default password
	if (counter->useDefaultPass == TRUE)
	{
		memset( counter->password1 , 0 , PASSWORD_SIZE );
		memset( counter->password1 , 0x30 , GUBANOV_SIZE_OF_PASSWORD );
		memset( counter->password2 , 0 , PASSWORD_SIZE );
		memset( counter->password2 , 0x30 , GUBANOV_SIZE_OF_PASSWORD );
	}

	memcpy( &tempCommand[ commandSize ] , counter->password1 , GUBANOV_SIZE_OF_PASSWORD ) ;
	commandSize += GUBANOV_SIZE_OF_PASSWORD ;

	//command
	tempCommand[ commandSize++ ] = 'R' ;

	//crc
	GUBANOV_GetCRC( tempCommand , commandSize , &tempCommand[ commandSize ] );
	commandSize += GUBANOV_SIZE_OF_CRC ;

	tempCommand[ commandSize++ ] = '\r' ;

	COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN_AskForGubanov request buf: " , tempCommand , commandSize );

	(*transaction)->transactionType = TRANSACTION_UNKNOWN_ASK_FOR_GUBANOV ;
	(*transaction)->waitSize = 10 ;
	(*transaction)->answerSize = 0;
	(*transaction)->commandSize = commandSize;
	(*transaction)->answer = NULL;
	(*transaction)->result = (unsigned char) 0;
	(*transaction)->command = (unsigned char *) malloc( sizeof(unsigned char) * commandSize );
	if ((*transaction)->command != NULL)
	{
		memcpy((*transaction)->command, tempCommand, commandSize);
		res = OK ;
	}
	else
		res = ERROR_MEM_ERROR ;

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForGubanov() exit with errorcode %d" , res );

	return res;
}

//
//
//

int UNKNOWN_AskForUjuk(counter_t * counter, counterTransaction_t ** transaction)
{
	DEB_TRACE(DEB_IDX_UNKNOWN)

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForUjuk() started" );

	int res = ERROR_GENERAL ;

	unsigned int commandSize = 0;
	unsigned int answerLen = 0;
	unsigned int counterCrc;
	unsigned int counterAddress;
	unsigned char tempCommand[64];

	memset(tempCommand, 0, 64);

	if ( UJUK_GetCounterAddress(counter, &counterAddress) == TRUE )
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
	tempCommand[commandSize++] = 0x08 ;
	tempCommand[commandSize++] = UNKNOWN_ASK_UJUK ;

	//set password
//	memcpy(&tempCommand[commandSize], counter->password1, UJUK_SIZE_OF_PASSWORD);
//	commandSize += UJUK_SIZE_OF_PASSWORD;

	//set default password
	if (counter->useDefaultPass == TRUE)
	{
		memset( counter->password1 , 0x30 , PASSWORD_SIZE );
		memset( counter->password2 , 0x30 , PASSWORD_SIZE );
	}

	//get and set crc
	counterCrc = UJUK_GetCRC(tempCommand, commandSize);
	memcpy(&tempCommand[commandSize], &counterCrc, UJUK_SIZE_OF_CRC);
	commandSize += UJUK_SIZE_OF_CRC;

	COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN_AskForUjuk request buf: " , tempCommand , commandSize );

	(*transaction)->transactionType = TRANSACTION_UNKNOWN_ASK_FOR_UJUK;
	(*transaction)->waitSize = answerLen;
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

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_AskForUjuk() exit with errorcode %d" , res );

	return res;
}



//
//
//

int UNKNOWN_CreateCounterTask(counter_t * counter, counterTask_t ** counterTask)
{
	DEB_TRACE(DEB_IDX_UNKNOWN)

	int res = ERROR_GENERAL ;

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() started for [%lu]" , counter->dbId );

	//allocate task
	(*counterTask) = (counterTask_t *) malloc(sizeof (counterTask_t));

	if ( (*counterTask) == NULL )
	{
		//ACHTUNG!!! Can not alloacate memory!
		res = ERROR_MEM_ERROR;
		COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode=%d", res);
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

	//free statuses of result
	(*counterTask)->resultCurrentReady = 0;
	(*counterTask)->resultDayReady = 0;
	(*counterTask)->resultMonthReady = 0;
	(*counterTask)->resultProfileReady = 0;

	//free results
	memset(&(*counterTask)->resultCurrent, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultDay, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultMonth, 0, sizeof (meterage_t));
	memset(&(*counterTask)->resultProfile, 0, sizeof (profile_t));


	switch( counter->unknownId )
	{
		case 0:
		case UNKNOWN_ID_UJUK:
		{
			(*counterTask)->transactions = (counterTransaction_t **) realloc((*counterTask)->transactions, \
																	(++(*counterTask)->transactionsCount) * sizeof(counterTransaction_t *) );
			if ( (*counterTask)->transactions != NULL)
			{
				(*counterTask)->transactions[(*counterTask)->transactionsCount - 1] = (counterTransaction_t *) malloc(sizeof (counterTransaction_t));
				if (((*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) != NULL)
					res = UNKNOWN_AskForMayk(counter, &(*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) ;
				else
				{
					res = ERROR_MEM_ERROR;
					COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
					return res;
				}
			}
			else
			{
				res = ERROR_MEM_ERROR;
				COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
				return res;
			}
			counter->unknownId = UNKNOWN_ID_MAYK ;
		}
		break;

		case UNKNOWN_ID_MAYK:
		{
			(*counterTask)->transactions = (counterTransaction_t **) realloc((*counterTask)->transactions, \
																	(++(*counterTask)->transactionsCount) * sizeof(counterTransaction_t *) );
			if ( (*counterTask)->transactions != NULL)
			{
				(*counterTask)->transactions[(*counterTask)->transactionsCount - 1] = (counterTransaction_t *) malloc(sizeof (counterTransaction_t));
				if (((*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) != NULL)
					res = UNKNOWN_AskForGubanov(counter, &(*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) ;
				else
				{
					res = ERROR_MEM_ERROR;
					COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
					return res;
				}
			}
			else
			{
				res = ERROR_MEM_ERROR;
				COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
				return res;
			}

			counter->unknownId = UNKNOWN_ID_GUBANOV ;
		}
		break;

		case UNKNOWN_ID_GUBANOV:
		{
			(*counterTask)->transactions = (counterTransaction_t **) realloc((*counterTask)->transactions, \
																	(++(*counterTask)->transactionsCount) * sizeof(counterTransaction_t *) );
			if ( (*counterTask)->transactions != NULL)
			{
				(*counterTask)->transactions[(*counterTask)->transactionsCount - 1] = (counterTransaction_t *) malloc(sizeof (counterTransaction_t));
				if (((*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) != NULL)
					res = UNKNOWN_AskForUjuk(counter, &(*counterTask)->transactions[(*counterTask)->transactionsCount - 1]) ;
				else
				{
					res = ERROR_MEM_ERROR;
					COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
					return res;
				}
			}
			else
			{
				res = ERROR_MEM_ERROR;
				COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );
				return res;
			}

			counter->unknownId = UNKNOWN_ID_UJUK ;
		}
		break;

		default:
			break;

	}

	COMMON_STR_DEBUG(DEBUG_UNKNOWN "UNKNOWN_CreateCounterTask() exit with errorcode %d" , res );

	return res ;


}

//
//
//

int UNKNOWN_SaveTaskResults(counterTask_t * counterTask)
{
	DEB_TRACE(DEB_IDX_UNKNOWN)

	int res = ERROR_GENERAL;
	unsigned char transactionType;
	unsigned char transactionResult = TRANSACTION_DONE_OK;

	counter_t * counter = NULL;
	if (STORAGE_FoundCounter(counterTask->counterDbId , &counter) == OK )
	{
		if ( counter )
		{

			if ( ( counter->unknownId == UNKNOWN_ID_MAYK) || ( counter->unknownId == UNKNOWN_ID_GUBANOV ) )
			{
				counter->lastTaskRepeateFlag = 1 ;
			}
			else
			{
				counter->lastTaskRepeateFlag = 0 ;
			}



			unsigned int transactionIndex = 0 ;
			for
			(
				transactionIndex = 0 ; \
				((transactionIndex < counterTask->transactionsCount) && (transactionResult == TRANSACTION_DONE_OK)); \
				transactionIndex++
			)
			{
				COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN answer " , counterTask->transactions[transactionIndex]->answer , counterTask->transactions[transactionIndex]->answerSize );

				transactionType = counterTask->transactions[transactionIndex]->transactionType;
				transactionResult = counterTask->transactions[transactionIndex]->result;
				if ( transactionResult == TRANSACTION_DONE_OK )
				{

					unsigned char * answer = NULL;
					unsigned int answerSize = counterTask->transactions[transactionIndex]->answerSize ;
					COMMON_AllocateMemory( &answer  , answerSize );
					memcpy( answer , counterTask->transactions[transactionIndex]->answer , answerSize*sizeof(unsigned char)  );

					COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN answer: " , answer , answerSize);

					switch( transactionType )
					{
						case TRANSACTION_UNKNOWN_ASK_FOR_MAYK:
						{
							if (answerSize >= 10)
							{
								//waiting for something like  7e 00 5b 8d 93 82 01 00 00 01 2e 07 1a 25 0f 05 0b 07 0d 00 00 03 e8 00 31 7e
								//remove 7E-quotes and make backstuffing according to the MAYK protocol
								if ( MAYK_RemoveQuotes( &answer , &answerSize) == OK )
								{
									COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN answer for MAYK-request catched");
									MAYK_Gniffuts( &answer , &answerSize ) ;
									//check crc
									unsigned char crc = MAYK_GetCRC(&answer[0] , answerSize - 1 );
									if ( crc == answer[ answerSize - 1 ] )
									{
										COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN crc OK ");

										unsigned char counterType = 0 ;
										unsigned char ver[4];
										memset(ver , 0 , 4);

										if (answerSize > 11){
											memcpy(ver , &answer[7] , 4);
										}

										COMMON_BUF_DEBUG( DEBUG_UNKNOWN "UNKNOWN mayk version :" , ver , 4 );

										if (MAYK_UnknownCounterIdentification(&counterType, ver) == OK )
										{
											COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN counter type is [%d]", counterType );
											counter->type =counterType ;
											res = OK ;
											counter->lastTaskRepeateFlag = 1 ;
											STORAGE_UpdateCounterStatus_CounterType( counter , counterType );

											//need to check this is really PLC counter
											if (strlen((char *)counter->serialRemote) > 0){
												STORAGE_SavePlcCounterToConfiguration(counter);
											}
										}
										else
										{
											COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN MAYK_UnknownCounterIdentification return ERROR");
										}
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN crc ERROR. Wait for crc = [%02X], but real crc is [%02X]", crc , answer[ answerSize - 1 ] );
									}
								}
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN MAYK_UnknownCounterIdentification return ERROR, too small answer");
							}
						}
						break;

						case TRANSACTION_UNKNOWN_ASK_FOR_GUBANOV:
						{
							//waiting for something like  7e 38 33 37 52 50 41 30 33 0d
							COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN answer for GUBANOV-request catched");

							//check for answer length
							if ( answerSize >= 10  )
							{
								//check crc
								//unsigned char crc[3] ;
								unsigned char crc[ GUBANOV_SIZE_OF_CRC ] ;
								memset( crc , 0 , GUBANOV_SIZE_OF_CRC );

								unsigned char * pCrc = &crc[0];

								GUBANOV_GetCRC(answer , 7 , pCrc);
								if( !memcmp( &answer[7] , crc , GUBANOV_SIZE_OF_CRC ) )
								{
									unsigned char ver[3];
									memset( ver , 0 , 3 );
									memcpy( ver , &answer[5] , 2 );

									COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN counter software version is [%s] " , ver);

									unsigned char counterType =  0 ;

									//search for counter type
									if ( GUBANOV_UnknownCounterIdentification( &counterType, ver) == OK )
									{
										counter->type =counterType ;
										res = OK ;
										counter->lastTaskRepeateFlag = 1 ;
										STORAGE_UpdateCounterStatus_CounterType( counter , counterType );

										//need to check this is really PLC counter
										if (strlen((char *)counter->serialRemote) > 0){
											STORAGE_SavePlcCounterToConfiguration(counter);
										}
									}
								}
							}
						}
						break;

						case TRANSACTION_UNKNOWN_ASK_FOR_UJUK:
						{
							COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN answer for UJUK-request catched");


							int answerPos = 1 ;
							int crcPos = 4 ;
							if (counterTask->transactions[transactionIndex]->answer[0] == UJUK_LONG_ADDRESS_START_BYTE)
							{
								answerPos += 4;
								crcPos += 4 ;
							}

							unsigned int crc = UJUK_GetCRC(answer , crcPos) ;

							//check crc
							if ( memcmp(&answer[ crcPos ] , &crc , UJUK_SIZE_OF_CRC) == 0 )
							{
								unsigned char counterType ;
								if ( UJUK_UnknownCounterIdentification(&counterType , &answer[answerPos] ) == OK )
								{
									COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN type is [%d]", counterType);
									counter->type = counterType ;
									res = OK ;
									counter->lastTaskRepeateFlag = 1 ;
									STORAGE_UpdateCounterStatus_CounterType( counter , counterType );

									//need to check this is really PLC counter
									if (strlen((char *)counter->serialRemote) > 0){
										STORAGE_SavePlcCounterToConfiguration(counter);
									}
								}
							}
						}
						break;

						default:
							break;
					}
					COMMON_FreeMemory(&answer) ;
				} //if ( transactionResult == TRANSACTION_DONE_OK )
				else
				{
					COMMON_STR_DEBUG( DEBUG_UNKNOWN "UNKNOWN transaction result [%02X]", transactionResult );
				}

			}
		}
	}

	return res ;
}
