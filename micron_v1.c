/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

#include "common.h"
#include "micron_v1.h"
#include "storage.h"
#include "pool.h"
#include "acounts.h"
#include "connection.h"
#include "gsm.h"
#include "eth.h"
#include "plc.h"
#include "pio.h"
#include "debug.h"


//state defines
#define MICRON_STATE_OFFLINE 0
#define MICRON_STATE_WAITING_DATA 1
#define MICRON_STATE_GETTING_DATA 2
#define MICRON_STATE_CHECK_DATA 3
#define MICRON_STATE_DESTUFF_DATA 4
#define MICRON_STATE_HANDLE_DATA 5
#define MICRON_STATE_PREPARE_ANSWER 6
#define MICRON_STATE_SEND_ANSWER 7
//other defines

//static variables
static int micronState=MICRON_STATE_OFFLINE;

static int bytesAvailToRead = 0;

static pthread_mutex_t outBufMux = PTHREAD_MUTEX_INITIALIZER;

volatile int TransparentFlag = 0;

static int transparentMode = 0 ;

//buffers
static unsigned char * IncomingGeneralBuffer = NULL;
static unsigned int IncomingGeneralBufferLength = 0;
static unsigned int IncomingGeneralBufferPos = 0;

static unsigned char * IncomingCurrentBuffer = NULL;
static unsigned int IncomingCurrentBufferLength = 0;
static unsigned int IncomingCurrentBufferPos = 0;

static unsigned char * OutgoingGeneralBuffer = NULL;
static unsigned int OutgoingGeneralBufferLength = 0;
static unsigned int OutgoingGeneralBufferPos = 0;

static unsigned int OutgoingPackageNumber = 0;
static unsigned int OutgoingPackageTotal = 0;

static unsigned int newConfigFlag = 0 ;

static int needToRebootFlag = 0 ;
static int needToSyncFlag = 0 ;

static int waitAckFlag = 0 ;
static int repeateCounter = 0 ;

static BOOL accessDenyFlag = FALSE;

//md5 alhorithm

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_s {
	md5_word_t count[2];	/* message length in bits, lsw first */
	md5_word_t abcd[4];		/* digest buffer */
	md5_byte_t buf[64];		/* accumulate block */
} md5_state_t;

void md5_init(md5_state_t *pms);										/* Initialize the algorithm. */
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);	/* Append a string to the message. */
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);				/* Finish the message and return the digest. */

int CreateOutgoingGeneralBuffer(const int length);
int ClearOutgoingGeneralBuffer();

void MICRON_INCORRECT_PACK()
{
	CONNECTION_Stop(0x03);

	CONNECTION_Start();

	micronState = MICRON_STATE_OFFLINE ;
}

void MICRON_ClearIncomingCurrentBuffer()
{
	IncomingCurrentBufferLength = 0;
	IncomingCurrentBufferPos = 0;
	COMMON_FreeMemory( &IncomingCurrentBuffer );
}

//
// main micron uspd job, v.1
//

void MICRON_GetAccessDeniedFlag()
{
	DEB_TRACE(DEB_IDX_MICRON);

	accessDenyFlag = FALSE ;

	int fd = open(STORAGE_ACC_DENY_FLG_PATH , O_RDONLY);
	if( fd >= 0 )
	{
		unsigned char ch = 0;
		if ( read( fd , &ch , 1 ) == 1 )
		{
			if ( ch != '0' )
				accessDenyFlag = TRUE ;
		}
		close(fd);
	}

}


int MICRON_V1(void)
{

	DEB_TRACE(DEB_IDX_MICRON)


	int res = OK ;


	switch(micronState)
	{

		case MICRON_STATE_OFFLINE:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			//Clear all buffers
			IncomingGeneralBufferLength = 0;
			IncomingGeneralBufferPos = 0;
			COMMON_FreeMemory( &IncomingGeneralBuffer );

			ClearOutgoingGeneralBuffer();

			MICRON_ClearIncomingCurrentBuffer();

			bytesAvailToRead = 0;
			waitAckFlag = 0 ;
			repeateCounter = 0 ;
			TransparentFlag = 0 ;
			needToRebootFlag = 0 ;
			needToSyncFlag = 0 ;

			if ( newConfigFlag > 0 )
			{
				STORAGE_ApplyNewConfig( newConfigFlag );
				newConfigFlag = 0 ;
			}

			if ( transparentMode == 1 )
			{
				POOL_TurnTransparentOffConnDie();
				transparentMode = 0 ;
			}

			if ( needToSyncFlag == 1)
			{
				needToSyncFlag = 0 ;
				COMMON_SetNewTimeAbort();
			}

			sleep( MICRON_TIME_BETWEEN_CHECKING_CONNECTION );

			if( CONNECTION_ConnectionState() == OK )
			{
				micronState = MICRON_STATE_WAITING_DATA;
				MICRON_GetAccessDeniedFlag();
			}

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: State offline");

		}
		break;

		//****************************

		case MICRON_STATE_WAITING_DATA:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON MICRON_STATE_WAITING_DATA");
			if ( repeateCounter >=  MICRON_IO_ATTEMPTS)
			{
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: Repeate counter is max. Close connection");

				MICRON_INCORRECT_PACK();
			}

			if (TransparentFlag == 0)
			{
				//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Reqest for number of available to read bytes");
				bytesAvailToRead = CONNECTION_ReadAvailable();

				if( bytesAvailToRead < 0 )
				{
					micronState = MICRON_STATE_OFFLINE;
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON CONNECTION_ReadAvailable returned [%d]. Change state to [%d]", bytesAvailToRead , micronState);
				}

				else if( bytesAvailToRead == 0 ) {
					//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Available to read 0 bytes. Sleep");
					sleep( MICRON_TIME_BETWEEN_CHECKING_BYTES_AVAILABLE_TO_READ );
				}

				else
				{

					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: available [%d] bytes ", bytesAvailToRead);

					micronState = MICRON_STATE_GETTING_DATA ;
					//Create buffer to read
					IncomingCurrentBufferLength += bytesAvailToRead;
					COMMON_AllocateMemory( &IncomingCurrentBuffer , IncomingCurrentBufferLength );
					if (IncomingCurrentBuffer == NULL){
						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: error allocating mem");
					}

				}
//				else
//				{
//					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: available [%d] bytes. Do not change state ", bytesAvailToRead);
//				}

			}
			else
			{
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON TransparentFlag = [%d]" , TransparentFlag);
				TransparentFlag=0;
				micronState =  MICRON_STATE_PREPARE_ANSWER ;
			}

			//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON MICRON_STATE_WAITING_DATA end. Change state to [%d]", micronState);

		}
		break;

		//****************************

		case MICRON_STATE_GETTING_DATA:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Try to get [%d] bytes", bytesAvailToRead);
			unsigned long int r;
			if ( CONNECTION_Read( &IncomingCurrentBuffer[ IncomingCurrentBufferPos ] , bytesAvailToRead , &r) < 0 )
			{
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Error reading. change state to OffLine");
				micronState = MICRON_STATE_OFFLINE ;
			}
			else
			{
				IncomingCurrentBufferPos += r;

				//osipov
				if (IncomingCurrentBufferPos >= 16){
					micronState = MICRON_STATE_CHECK_DATA;
				} else {
					micronState = MICRON_STATE_WAITING_DATA;
				}

				COMMON_BUF_DEBUG( DEBUG_MICRON_V1 "MICRON: rx ", IncomingCurrentBuffer, IncomingCurrentBufferPos);
			}

		}
		break;

		//****************************

		case MICRON_STATE_CHECK_DATA:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON State MICRON_STATE_CHECK_DATA");
			//check received package
			unsigned char type = 0;
			int crcFlag = OK;
			unsigned int packageNumber = 0;
			int remainingPackages = 0;
			int packageLength = 0;
			int firstPayloadBytePos = 0;

			int catchingPackageFlag =  \
					MICRON_CheckReceivedData( &type , &crcFlag, &packageNumber, &remainingPackages , &firstPayloadBytePos , &packageLength);


			if ( (catchingPackageFlag != OK ) && ( IncomingCurrentBufferLength <= MICRON_PACKAGE_LENGTH ) )
			{
				micronState = MICRON_STATE_WAITING_DATA;
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: there is no valid package in buffer ");
			}
			else if ( (catchingPackageFlag != OK ) && ( IncomingCurrentBufferLength > MICRON_PACKAGE_LENGTH ) )
			{
				MICRON_INCORRECT_PACK();

				MICRON_ClearIncomingCurrentBuffer();
			}
			else
			{
				CONNECTION_SetLastSessionProtoTrFlag(TRUE);

				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: complete packet type [%d] crc [%d] packet [%d] need packets [%d]", type, crcFlag, packageNumber, remainingPackages);
				switch(type)
				{
					case MICRON_P_DATA:
					{
						if ( waitAckFlag == 1 )
						{
							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: Waiting ACK but packet is DATA. Close connection");

							MICRON_INCORRECT_PACK();

						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: packet is DATA");

							//check CRC then destaff received package
							if (crcFlag != OK)
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: packet has wrong crc");

								if ( MICRON_SendShortMessage( packageNumber , MICRON_P_REPEAT ) != OK )
								{
									micronState = MICRON_STATE_OFFLINE ;
								}
								else
								{
									repeateCounter++;
									micronState = MICRON_STATE_WAITING_DATA;
									MICRON_ClearIncomingCurrentBuffer();

								}
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: packet ok, send ACK ");

								//CRC is OK. So we have to send acknowledge messege to server
								if ( MICRON_SendShortMessage( packageNumber , MICRON_P_ACK ) != OK )
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: error sending ACK ");

									micronState = MICRON_STATE_OFFLINE ;
								}
								else
								{
									repeateCounter = 0 ;
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: ACK send ok ");

									//copy data to general incoming buffer
									int idx ;
									IncomingGeneralBufferLength += packageLength ;
									COMMON_AllocateMemory( &IncomingGeneralBuffer , IncomingGeneralBufferLength );
									for( idx = firstPayloadBytePos ; idx < (packageLength + firstPayloadBytePos) ; idx++)
									{
										IncomingGeneralBuffer[ IncomingGeneralBufferPos ] = IncomingCurrentBuffer[ idx ] ;
										IncomingGeneralBufferPos++;
									}
									MICRON_ClearIncomingCurrentBuffer();
									//if we have to get any packages then wait them. Else, destuff general incoming buffer
									if (remainingPackages == 0)
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: complete packet go to processing data ");
										micronState = MICRON_STATE_DESTUFF_DATA ;
									}
									else if ( remainingPackages > 0 )
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: incomplete packet, need more ");
										micronState = MICRON_STATE_WAITING_DATA ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON ERROR: [%d] packets remaining. Logical error", remainingPackages);

										MICRON_INCORRECT_PACK();
									}
								}
							}

						}

					}
					break;

					case MICRON_P_REPEAT:
					{
						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: packet REPEAT");

						if ( waitAckFlag == 1 )
						{
							waitAckFlag = 0 ;

							if ( packageNumber == OutgoingPackageNumber )
							{
								//repeate sending last package
								micronState = MICRON_STATE_SEND_ANSWER ;

								MICRON_ClearIncomingCurrentBuffer();
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: We got REPEAT for package [%d] but waited for package [%d]. Close connection" , packageNumber , OutgoingPackageNumber);

								MICRON_INCORRECT_PACK();
							}

						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: unexpected REPEAT package. Close connection");

							MICRON_INCORRECT_PACK();
						}

					}
					break;

					case MICRON_P_ACK:
					{
						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: packet ACK");

						//if we have any package to send, send next. Else switch state to waiting data
						if ( waitAckFlag == 1 )
						{
							waitAckFlag = 0 ;
							if ( packageNumber == OutgoingPackageNumber )
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: ack for package [%d] was catched", packageNumber );
								if ( OutgoingPackageNumber == OutgoingPackageTotal)
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: sended last packet, go waiting...");

									//clear outgoing buffer
									ClearOutgoingGeneralBuffer();

									OutgoingPackageNumber = 0 ;
									OutgoingPackageTotal = 0 ;

									if ( newConfigFlag > 0 )
									{
										STORAGE_ApplyNewConfig( newConfigFlag );
										newConfigFlag = 0 ;
									}
									if ( needToRebootFlag == 1 )
									{
										STORAGE_JournalUspdEvent( EVENT_USPD_REBOOT , (unsigned char *)DESC_EVENT_REBOOT_BY_HLS );
										CONNECTION_Stop(0x02);
										PIO_RebootUspd();
										SHELL_CMD_REBOOT;
									}
									if ( needToSyncFlag == 1 )
									{
										COMMON_SetNewTimeStep2( DESC_EVENT_TIME_SYNCED_BY_PROTO ) ;
										needToSyncFlag = 0 ;
									}

									//cut handled package and looking for any more package in the buffers

									memmove( &IncomingCurrentBuffer[ 0 ] , &IncomingCurrentBuffer[ MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ] , IncomingCurrentBufferLength - ( MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ) );

									IncomingCurrentBufferPos -= MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ;
									IncomingCurrentBufferLength -= MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ;


									COMMON_AllocateMemory(&IncomingCurrentBuffer , IncomingCurrentBufferLength);

									micronState = MICRON_STATE_CHECK_DATA ;


									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: IncomingCurrentBufferPos = [%d] " , IncomingCurrentBufferPos );
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: IncomingCurrentBufferLength = [%d] " , IncomingCurrentBufferLength );

								}
								else
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: previos packet ACKed, send next part of answer");

									MICRON_ClearIncomingCurrentBuffer();

									micronState = MICRON_STATE_SEND_ANSWER ;
									OutgoingPackageNumber++;
								}
							}
							else
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: We got for package [%d] but waited for package [%d]. Close connection" , packageNumber , OutgoingPackageNumber);

								MICRON_INCORRECT_PACK();
							}
						}
						else
						{
							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: We got ack, but we expect for data. Close connection");

							MICRON_INCORRECT_PACK();

						}
					}
					break;

					case MICRON_P_PING:
					{
						if ( MICRON_SendShortMessage( 0 , MICRON_P_PING | MICRON_P_ACK ) != OK )
						{
							micronState = MICRON_STATE_OFFLINE ;
						}
						else
						{
							micronState = MICRON_STATE_WAITING_DATA;

							MICRON_ClearIncomingCurrentBuffer();
						}
					}
					break;

					default:
					{
						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: unknown type");

						MICRON_INCORRECT_PACK();
						MICRON_ClearIncomingCurrentBuffer();


					} //default
					break;
				} //switch
			} //else

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON State MICRON_STATE_CHECK_DATA end. Next state is [%d]", micronState);
		}
		break;

		//****************************

		case MICRON_STATE_DESTUFF_DATA:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: gniffuts packet data ");

//			//destuff incoming general bufffer
//			unsigned int BufferNewLength=IncomingGeneralBufferLength;
//			int BufferIndex=0, i;
//			while(BufferIndex < BufferNewLength)
//			{
//				if(IncomingGeneralBuffer[BufferIndex] == 0x5D)
//				{
//					for(i=BufferIndex ; i<BufferNewLength ; i++)
//						IncomingGeneralBuffer[i] = IncomingGeneralBuffer[i+1];
//					(IncomingGeneralBuffer[BufferIndex]) ^= 0x20;
//					BufferNewLength--;
//				}
//				BufferIndex++;
//			}
//			if( BufferNewLength < IncomingGeneralBufferLength )
//			{
//				COMMON_AllocateMemory( &IncomingGeneralBuffer , BufferNewLength );
//				IncomingGeneralBufferLength = BufferNewLength;
//			}

			MICRON_Gniffuts( &IncomingGeneralBuffer , (int *)&IncomingGeneralBufferLength );

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: gniffuts packet data done ");

			//egorkin kosyak number 1
			micronState = MICRON_STATE_HANDLE_DATA ;

		}
		break;

		//****************************

		case MICRON_STATE_HANDLE_DATA:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			micronState = MICRON_STATE_PREPARE_ANSWER ;

			unsigned short command = ( IncomingGeneralBuffer[0] << 8 ) | IncomingGeneralBuffer[1];  //for 2 byte command

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: recv command [%04X] ", command);

			switch(command)
			{

				///////////////////////

				case MICRON_COMM_GET_METR:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Get metrages\n");

					OutgoingGeneralBufferPos = 0 ;
					CreateOutgoingGeneralBuffer( 2 + 1 + 4) ;
					// command - 2 bytes
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = ( MICRON_COMM_GET_METR & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = MICRON_COMM_GET_METR & 0x00FF ;
					//result - 1 byte
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = MICRON_RES_OK ;
					//counter's DbId  - 4 bytes
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = IncomingGeneralBuffer[2] ;
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = IncomingGeneralBuffer[3] ;
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = IncomingGeneralBuffer[4] ;
					OutgoingGeneralBuffer[OutgoingGeneralBufferPos++] = IncomingGeneralBuffer[5] ;



					unsigned long counterDbId =   ( IncomingGeneralBuffer[2] << 24 ) + \
													( IncomingGeneralBuffer[3] << 16 ) + \
													( IncomingGeneralBuffer[4] << 8  ) + \
													IncomingGeneralBuffer[5] ;

					unsigned int gettedMask = ( IncomingGeneralBuffer[6] << 24 ) + \
												( IncomingGeneralBuffer[7] << 16 ) + \
												( IncomingGeneralBuffer[8] << 8  ) + \
												IncomingGeneralBuffer[9] ;

					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON counterDbId = [%lu] , mask = [%X]" , counterDbId , gettedMask );

					unsigned int maskIndex = 0;
					for( maskIndex = 0x01 ; maskIndex > 0 ; maskIndex <<= 1 )
					{
						//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Checking for [%u]" , maskIndex );

						if( accessDenyFlag )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						}

						if ( OutgoingGeneralBuffer[2] == MICRON_RES_OK )
						{
							if( maskIndex & gettedMask )
							{
								COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON mask [%u] was cathed" , maskIndex );
								switch( maskIndex )
								{
									case MICRON_METR_CURRENT:
									case MICRON_METR_DAY:
									case MICRON_METR_MONTH:
										MICRON_GetMeterages(maskIndex, counterDbId);
										break;
									//-----------------
									case MICRON_METR_PROFILE:
										MICRON_GetEnergies(counterDbId) ;
										break;
									//-----------------

									case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
									case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
									case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
									case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
									case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
									case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
									case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
									case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
									case MICRON_METR_COUNTER_JOURNAL_PQP:
									{
										if ( counterDbId != 0xFFFFFFFF )
											MICRON_GetJournal( maskIndex , counterDbId );
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
									break;

									case MICRON_METR_USPD_JOURNAL_GENERAL:
									case MICRON_METR_USPD_JOURNAL_TARIFF:
									case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
									case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
									case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
									case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
									case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
									{
										if ( counterDbId == 0xFFFFFFFF )
											MICRON_GetJournal( maskIndex , counterDbId );
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
									break;

									//-----------------

									case MICRON_METR_PQP:
										MICRON_GetPQP( maskIndex , counterDbId );
										break;
									//-----------------

									case MICRON_METR_TARIFF_WRITE_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetTariffState() ;
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										break;
									//-----------------

									case MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetPSMState() ;
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										break;

									case MICRON_METR_DISCRET_OUTPUT_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetDiscretOutputState();
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										break;
									//-----------------

									case MICRON_METR_PLC_NETWORK_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetPlcState();
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										//TODO
										break;
									//-----------------

									case MICRON_METR_RF_NETWORK_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetRfState();
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										break;
									//-----------------

									case MICRON_METR_DEVICE_SYNC_STATE:
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetSyncStateWord();
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;

										break;
									//-----------------

//									case MICRON_METR_DEVICES_UPDATING_FIRMWARE_STATE:
//									{
//										if ( counterDbId == 0xFFFFFFFF )
//										{
//											MICRON_GetFirmwareState();
//										}
//										else
//											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
//									}
//									break;
									//-----------------

									case MICRON_METR_DEVICES_TRANSACTION_STATE:
									{
										if ( counterDbId == 0xFFFFFFFF )
										{
											MICRON_GetTransactionStatistic();
										}
										else
											OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
									break;
									//-----------------



									default:
										OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
										break;
								}
							}
						}
						else
						{
							OutgoingGeneralBufferLength = 3 ;
							COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength ) ;
							break;
						}
					}




				}
				break;

				///////////////////////

				case MICRON_COMM_SET_M_DB:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set metrages are placed to DB\n");
					CreateOutgoingGeneralBuffer( 2 + 1 ) ;
					// command - 2 bytes
					OutgoingGeneralBuffer[0] = ( MICRON_COMM_SET_M_DB & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[1] = MICRON_COMM_SET_M_DB & 0x00FF ;
					//result - 1 byte
					OutgoingGeneralBuffer[2] = MICRON_RES_OK ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					unsigned long \
					counterDbId = ( IncomingGeneralBuffer[ 2 ] << 24 ) + \
								  ( IncomingGeneralBuffer[ 3 ] << 16) + \
								  ( IncomingGeneralBuffer[ 4 ] << 8) + \
									IncomingGeneralBuffer[ 5 ] ;

					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON counter DbId is [%ld]" , counterDbId );

					IncomingGeneralBufferPos = 6;
					while(( IncomingGeneralBufferPos < IncomingGeneralBufferLength ) && (OutgoingGeneralBuffer[2] == MICRON_RES_OK))
					{
						//osipov
						//if ( OutgoingGeneralBuffer[2] == MICRON_RES_OK )
						//{



							unsigned int mask = ( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
												( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
												( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
												  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

							IncomingGeneralBufferPos += 4;

							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON mask is [%d]" , mask );


							dateTimeStamp dts;
							memset( &dts , 0 , sizeof(dateTimeStamp));

							dts.t.s = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.t.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.t.h = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.d = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.y = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;


							switch (mask)
							{

								//-----------------
								case MICRON_METR_DAY:
								{
									if ( STORAGE_MarkMeteragesAsPlacedToDb( STORAGE_DAY , counterDbId , dts) != OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_MONTH:
								{
									if ( STORAGE_MarkMeteragesAsPlacedToDb( STORAGE_MONTH , counterDbId , dts)!= OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_PROFILE:
								{
									if ( STORAGE_MarkProfileAsPlacedToDb(counterDbId , dts)!= OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
								case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
								case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
								case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
								case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
								case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
								case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
								case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
								case MICRON_METR_COUNTER_JOURNAL_PQP:
								{
									int journalNumber = 0 ;
									switch (mask) {
										case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_BOX ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_CONTACT ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_DEVICE_SWITCH ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_HARD ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_SOFT ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SET_TARIFF ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_POWER_SWITCH ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_STATE_WORD ;
											break;
										//-----------------
										case MICRON_METR_COUNTER_JOURNAL_PQP:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_PQP ;
											break;
										//-----------------

										default:
											break;
									}

									if ( counterDbId != 0xFFFFFFFF )
									{
										if (STORAGE_MarkJournalAsPlacedToDb(dts , counterDbId , journalNumber) !=  OK )
										{
											OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
										}
									}
									else
									{
										OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
								}
								break;

								case MICRON_METR_USPD_JOURNAL_GENERAL:
								case MICRON_METR_USPD_JOURNAL_TARIFF:
								case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
								case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
								case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
								case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
								case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
								{
									int journalNumber = 0 ;
									switch (mask) {
										case MICRON_METR_USPD_JOURNAL_GENERAL:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_GENERAL ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_TARIFF:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_TARIFF ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_POWER_SWITCH_MODULE ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_RF_EVENTS ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_PLC_EVENTS ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_EXTENDED_IO_CARD ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_FIRMWARE ;
											break;

										default:
											break;
									}

									if ( counterDbId == 0xFFFFFFFF ){
										if ( STORAGE_MarkJournalAsPlacedToDb(dts , counterDbId , journalNumber) != OK )
										{
											OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
										}
									} else {
										OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
								}
								break;

								default:
									OutgoingGeneralBuffer[2] = MICRON_RES_WRONG_PARAM ;
									break;
							}

						//}

						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON pckres is [%d]" , OutgoingGeneralBuffer[2] );

					}

				}
				break;


				///////////////////////

				case MICRON_RECOV_M_DB:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Recovery PlacedToDb-Flag in metrages\n");

					CreateOutgoingGeneralBuffer( 2 + 1 ) ;
					// command - 2 bytes
					OutgoingGeneralBuffer[0] = ( MICRON_RECOV_M_DB & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[1] = MICRON_RECOV_M_DB & 0x00FF ;
					//result - 1 byte
					OutgoingGeneralBuffer[2] = MICRON_RES_OK ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					unsigned long \
					counterDbId = ( IncomingGeneralBuffer[ 2 ] << 24 ) | \
								  ( IncomingGeneralBuffer[ 3 ] << 16) | \
								  ( IncomingGeneralBuffer[ 4 ] << 8) | \
									IncomingGeneralBuffer[ 5 ] ;

					IncomingGeneralBufferPos = 6;
					while(( IncomingGeneralBufferPos < IncomingGeneralBufferLength ) && ( OutgoingGeneralBuffer[2] == MICRON_RES_OK ))
					{
						//osipov
						//if ( OutgoingGeneralBuffer[2] == MICRON_RES_OK )
						//{
							unsigned int mask = ( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
												( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
												( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
												  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

							IncomingGeneralBufferPos += 4;


							dateTimeStamp dts;
							memset( &dts , 0 , sizeof(dateTimeStamp));

							dts.t.s = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.t.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.t.h = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.d = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;
							dts.d.y = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ] ;


							switch (mask)
							{

								//-----------------
								case MICRON_METR_DAY:
								{
									if ( STORAGE_RecoveryMetragesPlasedToDbFlag( STORAGE_DAY , counterDbId , dts)!= OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_MONTH:
								{
									if ( STORAGE_RecoveryMetragesPlasedToDbFlag( STORAGE_MONTH , counterDbId , dts)!= OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_PROFILE:
								{
									if ( STORAGE_RecoveryProfilePlacedToDbFlag(counterDbId , dts)!= OK )
										OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
								}
								break;
								//-----------------
								case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
								case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
								case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
								case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
								case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
								case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
								case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
								case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
								case MICRON_METR_COUNTER_JOURNAL_PQP:
								{
									int journalNumber = 0 ;
									switch (mask) {
										case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_BOX ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_CONTACT ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_DEVICE_SWITCH ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_HARD ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_SOFT ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SET_TARIFF ;
											break;
										//-----------------

										case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_POWER_SWITCH ;
											break;
										//-----------------
										case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_STATE_WORD ;
											break;
										//-----------------
										case MICRON_METR_COUNTER_JOURNAL_PQP:
											journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_PQP ;
											break;
										//-----------------
										default:
											break;
									}

									if ( counterDbId != 0xFFFFFFFF )
									{
										if (STORAGE_RecoveryJournalPlacedToDbFlag(dts , counterDbId , journalNumber) !=  OK )
										{
											OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
										}
									}
									else
									{
										OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
								}
								break;

								case MICRON_METR_USPD_JOURNAL_GENERAL:
								case MICRON_METR_USPD_JOURNAL_TARIFF:
								case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
								case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
								case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
								case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
								case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
								{
									int journalNumber = 0 ;
									switch (mask) {
										case MICRON_METR_USPD_JOURNAL_GENERAL:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_GENERAL ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_TARIFF:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_TARIFF ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_POWER_SWITCH_MODULE ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_RF_EVENTS ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_PLC_EVENTS ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_EXTENDED_IO_CARD ;
											break;
										//-----------------

										case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
											journalNumber = EVENT_USPD_JOURNAL_NUMBER_FIRMWARE ;
											break;

										default:
											break;
									}

									if ( counterDbId == 0xFFFFFFFF ){
										if ( STORAGE_RecoveryJournalPlacedToDbFlag(dts , counterDbId , journalNumber) != OK )
										{
											OutgoingGeneralBuffer[2] = MICRON_RES_GEN_ERR ;
										}
									} else {
										OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
									}
								}
								break;

								default:
									OutgoingGeneralBuffer[2] = MICRON_RES_WRONG_PARAM ;
									break;
							}

						//}

					}

				}
				break;

				///////////////////////

				case MICRON_COMM_GET_SN:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON: ask about serial number, name and place\n");

					unsigned char return_code = MICRON_RES_OK ;

					unsigned char uspd_sn[ (SIZE_OF_SERIAL + 1) ];
					memset(uspd_sn, 0 , (SIZE_OF_SERIAL + 1) );
					COMMON_GET_USPD_SN( uspd_sn );

					if( strstr( (char *)uspd_sn , "err" ) != NULL)
					{
						memset(uspd_sn, 0 , (SIZE_OF_SERIAL + 1 ) );
						return_code = MICRON_RES_ID_NOT_FOUND ;
					}

					uspd_sn[ SIZE_OF_SERIAL ] = '\0';

					unsigned char uspd_obj_name[STORAGE_PLACE_NAME_LENGTH ] ;
					memset( uspd_obj_name , 0 , STORAGE_PLACE_NAME_LENGTH );
					COMMON_GET_USPD_OBJ_NAME( uspd_obj_name );

					unsigned char uspd_location[STORAGE_PLACE_NAME_LENGTH];
					memset( uspd_location , 0 , STORAGE_PLACE_NAME_LENGTH);
					COMMON_GET_USPD_LOCATION( uspd_location );

					//Create outgoing package according rules
					CreateOutgoingGeneralBuffer( 2 + 1 + strlen((char *)uspd_sn) + 1 + strlen((char *)uspd_obj_name) + 1 + strlen((char *)uspd_location) + 1 ) ;

					OutgoingGeneralBufferPos = 0;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_GET_SN & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_GET_SN & 0x00FF;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = return_code;

					memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , uspd_sn , strlen((char *)uspd_sn) );
					OutgoingGeneralBufferPos += strlen((char *)uspd_sn) ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0;

					memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , uspd_obj_name , strlen((char *)uspd_obj_name) );
					OutgoingGeneralBufferPos += strlen((char *)uspd_obj_name) ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0;

					memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , uspd_location , strlen((char *)uspd_location) );
					OutgoingGeneralBufferPos += strlen((char *)uspd_location) ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0;

					//proto v.0.16
					char version[ STORAGE_VERSION_LENGTH ];
					memset( version , 0 , STORAGE_VERSION_LENGTH );
					unsigned int sw = 0xFFFF;
					if ( STORAGE_GetFirmwareVersion( (char *)version ) == OK)
					{
						 STORAGE_TranslateFirmwareVersion( (char *)version , &sw ) ;
					}

					#if ((REV_COMPILE_PLC == 0) && (REV_COMPILE_RF == 1) && (REV_COMPILE_EXIO == 0))
						unsigned char type = 0x01 ;
					#elif ((REV_COMPILE_PLC == 1) && (REV_COMPILE_RF == 0) && (REV_COMPILE_EXIO == 0))
						unsigned char type = 0x02 ;
					#elif ((REV_COMPILE_PLC == 0) && (REV_COMPILE_RF == 0) && (REV_COMPILE_EXIO == 1))
						unsigned char type = 0x03 ;
					#elif ((REV_COMPILE_PLC == 0) && (REV_COMPILE_RF == 1) && (REV_COMPILE_EXIO == 1))
						unsigned char type = 0x04 ;
					#elif ((REV_COMPILE_PLC == 1) && (REV_COMPILE_RF == 0) && (REV_COMPILE_EXIO == 1))
						unsigned char type = 0x05 ;
					#else
						unsigned char type = 0x06 ;
					#endif

					unsigned int protoVer = MICRON_PROTO_VER ;
					OutgoingGeneralBufferLength += 5 ;
					OutgoingGeneralBuffer = (unsigned char *)realloc( OutgoingGeneralBuffer , OutgoingGeneralBufferLength * sizeof(unsigned char) );
					if ( OutgoingGeneralBuffer == NULL )
					{
						EXIT_PATTERN;
					}

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = type;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)(( sw & 0xFF00 ) >> 8);
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)(sw  & 0xFF);
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)( ( protoVer & 0xFF00 ) >> 8);
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)(protoVer  & 0xFF);

				}
				break;

				///////////////////////

				case MICRON_COMM_SET_PROP:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set properties\n");
					CreateOutgoingGeneralBuffer( 2 + 1 + 2);
					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_SET_PROP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] =  MICRON_COMM_SET_PROP & 0x00FF ;
					if (IncomingGeneralBufferLength < (2 + 2 + 16 + 1) )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
						break;
					}

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						OutgoingGeneralBuffer[ 3 ] = IncomingGeneralBuffer[2];
						OutgoingGeneralBuffer[ 4 ] = IncomingGeneralBuffer[3];
						break;
					}

					OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK;

					if ( MICRON_CheckMD5( &IncomingGeneralBuffer[4] , &IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 )) != OK )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM;
						OutgoingGeneralBuffer[ 3 ] = IncomingGeneralBuffer[2];
						OutgoingGeneralBuffer[ 4 ] = IncomingGeneralBuffer[3];
						break;
					}

					unsigned int mask = ( IncomingGeneralBuffer[2] << 8 ) + IncomingGeneralBuffer[3] ;
					int firstRetValue = 0 ;
					int secRetValue = 0 ;


					if (( mask == MICRON_PROP_DEV_RS) || ( mask == MICRON_PROP_DEV_PLC ))
					{
						int countersNumb = STORAGE_GetCountersNumberInRawConfig((char *)&IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 ));
						BOOL tooManyCounters = FALSE ;

						switch(mask)
						{
							case MICRON_PROP_DEV_RS:

								if ( (countersNumb + STORAGE_CountOfCountersPlc()) > MAX_DEVICE_COUNT_TOTAL )
									tooManyCounters = TRUE ;

								break;

							case MICRON_PROP_DEV_PLC:

								if ( (countersNumb + STORAGE_CountOfCounters485()) > MAX_DEVICE_COUNT_TOTAL )
									tooManyCounters = TRUE ;

								break;
						}

						if (tooManyCounters)
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
							OutgoingGeneralBuffer[ 3 ] = IncomingGeneralBuffer[2];
							OutgoingGeneralBuffer[ 4 ] = IncomingGeneralBuffer[3];

							unsigned char desc[EVENT_DESC_SIZE] = { 0 };
							desc[0] = 0x03;
							desc[1] = IncomingGeneralBuffer[2];
							desc[2] = IncomingGeneralBuffer[3];

							STORAGE_JournalUspdEvent( EVENT_CONFIG_ERROR , desc );

							break;
						}
					}

					if ( STORAGE_CheckNewConfig( mask , &IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 ) , &firstRetValue , &secRetValue ) == OK )
					{
						if (STORAGE_SaveNewConfig( mask , &IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 ) , DESC_EVENT_CFG_UPDATE_BY_HLS  ) == OK )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK;
							OutgoingGeneralBuffer[ 3 ] = (mask >> 8) & 0xFF;
							OutgoingGeneralBuffer[ 4 ] = mask & 0xFF;

							//
							// Appliing new config will be in state MICRON_STATE_OFF or MICRON_WAIT_PACKAGE
							//

							newConfigFlag = mask ;

						}
						else
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR ;
							OutgoingGeneralBuffer[ 3 ] = (mask >> 8) & 0xFF;
							OutgoingGeneralBuffer[ 4 ] = mask & 0xFF;
						}
					}
					else
					{
						switch(mask)
						{
							case MICRON_PROP_DEV_PLC:
							{
								#if REV_COMPILE_PLC
									OutgoingGeneralBuffer[ 2 ] = MICRON_RES_PLC_COUNTERS_CONFIG_ERROR;

									unsigned char desc[EVENT_DESC_SIZE] = { 0 };
									desc[0] = 0x03;
									desc[1] = (firstRetValue >> 24 ) & 0xFF ;
									desc[2] = (firstRetValue >> 16 ) & 0xFF ;
									desc[3] = (firstRetValue >> 8 ) & 0xFF ;
									desc[4] = firstRetValue  & 0xFF ;

									desc[5] = (secRetValue >> 24 ) & 0xFF ;
									desc[6] = (secRetValue >> 16 ) & 0xFF ;
									desc[7] = (secRetValue >> 8 ) & 0xFF ;
									desc[8] = secRetValue  & 0xFF ;
									STORAGE_JournalUspdEvent( EVENT_ERROR_CONFIG_PlC_COUNTERS , desc );
								#else
									OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
								#endif
							}
							break;

							case MICRON_PROP_COORD_PLC:
							{
								#if REV_COMPILE_PLC
									OutgoingGeneralBuffer[ 2 ] = MICRON_RES_PLC_COORD_CONFIG_ERROR;

									unsigned char desc[EVENT_DESC_SIZE] = { 0 };
									desc[0] = 0x03;
									desc[1] = (firstRetValue >> 24 ) & 0xFF ;
									desc[2] = (firstRetValue >> 16 ) & 0xFF ;
									desc[3] = (firstRetValue >> 8 ) & 0xFF ;
									desc[4] = firstRetValue  & 0xFF ;

									desc[5] = (secRetValue >> 24 ) & 0xFF ;
									desc[6] = (secRetValue >> 16 ) & 0xFF ;
									desc[7] = (secRetValue >> 8 ) & 0xFF ;
									desc[8] = secRetValue  & 0xFF ;
									STORAGE_JournalUspdEvent( EVENT_ERROR_CONFIG_PlC_COORD , desc );
								#else
									OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
								#endif
							}
							break;

							default:
							{
								OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
							}
							break;
						}

						OutgoingGeneralBuffer[ 3 ] = (mask >> 8) & 0xFF;
						OutgoingGeneralBuffer[ 4 ] = mask & 0xFF;
					}

				}
				break;

				///////////////////////

				case MICRON_COMM_NZIF_SET_PROP:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Setting properties without checking\n");
					CreateOutgoingGeneralBuffer( 2 + 1 + 2);
					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_SET_PROP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] =  MICRON_COMM_SET_PROP & 0x00FF ;
					if (IncomingGeneralBufferLength < (2 + 2 + 16 + 1) )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
						break;
					}

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK;

					if ( MICRON_CheckMD5( &IncomingGeneralBuffer[4] , &IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 )) != OK )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM;
						break;
					}

					unsigned int mask = ( IncomingGeneralBuffer[2] << 8 ) + IncomingGeneralBuffer[3] ;
					if (STORAGE_SaveNewConfig( mask , &IncomingGeneralBuffer[4+16] , ( IncomingGeneralBufferLength - 4 - 16 ), DESC_EVENT_CFG_UPDATE_BY_HLS  ) == OK )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK;
						OutgoingGeneralBuffer[ 3 ] = (mask >> 8) & 0xFF;
						OutgoingGeneralBuffer[ 4 ] = mask & 0xFF;

						//
						// Appliing new config will be in state MICRON_STATE_OFF or MICRON_WAIT_PACKAGE
						//

						newConfigFlag = mask ;

					}
					else
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR ;
						OutgoingGeneralBuffer[ 3 ] = (mask >> 8) & 0xFF;
						OutgoingGeneralBuffer[ 4 ] = mask & 0xFF;
					}
				}
					break;

				//////////////////////

				case MICRON_COMM_GET_PROP:
				{
					CreateOutgoingGeneralBuffer( 2 + 1 + 2 );
					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_GET_PROP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] =  MICRON_COMM_GET_PROP & 0x00FF ;

					unsigned char * cfgFile = NULL;
					int cfgLength = 0;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					if ( IncomingGeneralBufferLength == 4 )
					{
						unsigned int mask = ( IncomingGeneralBuffer[2] << 8 ) + IncomingGeneralBuffer[3] ;

						OutgoingGeneralBuffer[ 2 ]  = STORAGE_GetCfgFile( mask , &cfgFile , &cfgLength );

						if ( (cfgLength != 0 ) && (OutgoingGeneralBuffer[ 2 ] == MICRON_RES_OK) )
						{
							if ( cfgFile )
							{
								OutgoingGeneralBuffer[ 3 ] = ( mask & 0xFF00 ) >> 8;
								OutgoingGeneralBuffer[ 4 ] =  mask & 0x00FF ;

								OutgoingGeneralBufferLength += ( cfgLength + 16 );
								COMMON_AllocateMemory( &OutgoingGeneralBuffer, OutgoingGeneralBufferLength );

								//calculating hash
								unsigned char hash[16];
								memset(hash, 0 , 16 );

								md5_state_t md5handler;
								md5_init(&md5handler);
								md5_append( &md5handler , (const md5_byte_t *)cfgFile, cfgLength);
								md5_finish( &md5handler , hash );

								//coping hash to OutgoingGeneralBuffer
								memcpy( &OutgoingGeneralBuffer[ 5 ] , hash , 16);

								//coping config file to OutgoingGeneralBuffer
								memcpy( &OutgoingGeneralBuffer[ 21 ] , cfgFile , cfgLength );

								free(cfgFile);
							}
							else
							{
								OutgoingGeneralBuffer[ 2 ]  = MICRON_RES_GEN_ERR ;
							}
						}

					}
					else
					{
						OutgoingGeneralBuffer[ 2 ]  = MICRON_RES_GEN_ERR ;
					}


				}
				break;

				//////////////////////

				case MICRON_COMM_GET_AUTODETECTED_COUNTERS:
				{
					CreateOutgoingGeneralBuffer( 2 + 1 );

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_GET_AUTODETECTED_COUNTERS & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_GET_AUTODETECTED_COUNTERS & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					counterAutodetectionStatistic_t * counterAutodetectionStatistic = NULL ;
					int counterAutodetectionStatisticLength = 0 ;
					if ( STORAGE_GetCounterAutodetectionStatistic( &counterAutodetectionStatistic , &counterAutodetectionStatisticLength) == OK )
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;

						OutgoingGeneralBufferLength += 4 + ( 5 * counterAutodetectionStatisticLength ) ;

						COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatisticLength & 0xFF000000) >> 24 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatisticLength & 0xFF0000 ) >> 16 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatisticLength & 0xFF00 ) >> 8 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterAutodetectionStatisticLength & 0xFF ;

						if ( (counterAutodetectionStatisticLength > 0) && ( counterAutodetectionStatistic ) )
						{
							int idx = 0 ;
							for ( ; idx < counterAutodetectionStatisticLength ; ++idx )
							{
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatistic[ idx ].dbId & 0xFF000000) >> 24 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatistic[ idx ].dbId & 0xFF0000 ) >> 16 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterAutodetectionStatistic[ idx ].dbId & 0xFF00 ) >> 8 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterAutodetectionStatistic[ idx ].dbId & 0xFF ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterAutodetectionStatistic[ idx ].type & 0xFF ;
							}
							free( counterAutodetectionStatistic );
							counterAutodetectionStatistic = NULL ;
							counterAutodetectionStatisticLength = 0 ;
						}

					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_GEN_ERR ;
					}
				}
				break;

//				case MICRON_COMM_APP_PROP:
//				{
//					CreateOutgoingGeneralBuffer( 2 + 1 );
//					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_APP_PROP & 0xFF00 ) >> 8;
//					OutgoingGeneralBuffer[ 1 ] =  MICRON_COMM_APP_PROP & 0x00FF ;
//					if (IncomingGeneralBufferLength < 2 + 1 )
//						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR;
//					else
//						OutgoingGeneralBuffer[ 2 ]  = STORAGE_ApplyNewConfig( IncomingGeneralBuffer[2] ) ;

//				}
//				break;


				//////////////////////


				case MICRON_COMM_TAR_MAP_SET:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set new tariff map\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_TAR_MAP_SET & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_TAR_MAP_SET & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_RECIEVED ,  NULL );

					IncomingGeneralBufferPos = 2 ;

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if ( acountsDbidsList.counterDbidsNumb > 0 )
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;

							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						if ( MICRON_CheckMD5( &IncomingGeneralBuffer[ IncomingGeneralBufferPos ] , &IncomingGeneralBuffer[IncomingGeneralBufferPos+16] , ( IncomingGeneralBufferLength - IncomingGeneralBufferPos - 16 )) != OK )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM;
						}
						else
						{
							IncomingGeneralBufferPos += 16 ;
							if ( ACOUNT_AddNew( (char *)&IncomingGeneralBuffer[IncomingGeneralBufferPos] , IncomingGeneralBufferLength - IncomingGeneralBufferPos , &acountsDbidsList , ACOUNTS_PART_TARIFF  ) == OK )
							{
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
							}
							else
							{
								OutgoingGeneralBuffer[ 2 ] = MICRON_RES_FS_FAILURE ;
							}
						}

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}

				}
				break;


				/////////////////////


				case MICRON_COMM_PSM_RULES_SET:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set new power control map\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_PSM_RULES_SET & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_PSM_RULES_SET & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					STORAGE_JournalUspdEvent( EVENT_PSM_RULES_RECIEVED ,  NULL );

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if ( acountsDbidsList.counterDbidsNumb > 0 )
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						if ( MICRON_CheckMD5( &IncomingGeneralBuffer[ IncomingGeneralBufferPos ] , &IncomingGeneralBuffer[IncomingGeneralBufferPos+16] , ( IncomingGeneralBufferLength - IncomingGeneralBufferPos - 16 )) != OK )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM;
						}
						else
						{
							IncomingGeneralBufferPos += 16 ;
							if ( ACOUNT_AddNew( (char *)&IncomingGeneralBuffer[IncomingGeneralBufferPos] , IncomingGeneralBufferLength - IncomingGeneralBufferPos , &acountsDbidsList , ACOUNTS_PART_POWR_CTRL ) == OK )
							{
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
							}
							else
							{
								OutgoingGeneralBuffer[ 2 ] = MICRON_RES_FS_FAILURE ;
							}
						}

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}

				}
				break;



				/////////////////////


				case MICRON_COMM_USERCMNDS_SET:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set new user commands\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_USERCMNDS_SET & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_USERCMNDS_SET & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_RECIEVED ,  NULL );

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if ( acountsDbidsList.counterDbidsNumb > 0 )
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						if ( MICRON_CheckMD5( &IncomingGeneralBuffer[ IncomingGeneralBufferPos ] , &IncomingGeneralBuffer[IncomingGeneralBufferPos+16] , ( IncomingGeneralBufferLength - IncomingGeneralBufferPos - 16 )) != OK )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM;
						}
						else
						{
							IncomingGeneralBufferPos += 16 ;
							if ( ACOUNT_AddNew( (char *)&IncomingGeneralBuffer[IncomingGeneralBufferPos] , IncomingGeneralBufferLength - IncomingGeneralBufferPos , &acountsDbidsList , ACOUNTS_PART_USER_COMMANDS ) == OK )
							{
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
							}
							else
							{
								OutgoingGeneralBuffer[ 2 ] = MICRON_RES_FS_FAILURE ;
							}
						}

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}

				}
				break;


				/////////////////////


				case MICRON_COMM_TARIFF_STOP:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON STOP TARIFF\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_TARIFF_STOP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_TARIFF_STOP & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if (acountsDbidsList.counterDbidsNumb > 0)
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						int counterIndex = 0 ;
						for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
						{
							ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_TARIFF );
							unsigned char evDesc[EVENT_DESC_SIZE];
							COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
							COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_TARIFF_WRITING_CANCELED, evDesc);
							STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
						}
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}
				}
				break;


				/////////////////////


				case MICRON_COMM_PSM_RULES_STOP:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON STOP PSM RULES\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_PSM_RULES_STOP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_PSM_RULES_STOP & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if (acountsDbidsList.counterDbidsNumb > 0)
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						int counterIndex = 0 ;
						for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
						{
							ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_POWR_CTRL );
							unsigned char evDesc[EVENT_DESC_SIZE];
							COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
							COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_PSM_RULES_WRITING_CANCELED, evDesc);
							STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
						}

						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}
				}
				break;


				/////////////////////


				case MICRON_COMM_USERCMNDS_STOP:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON STOP USER COMMANDS\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_USERCMNDS_STOP & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_USERCMNDS_STOP & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if (acountsDbidsList.counterDbidsNumb > 0)
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						int counterIndex = 0 ;
						for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
						{
							ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_USER_COMMANDS );
							unsigned char evDesc[EVENT_DESC_SIZE];
							COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
							COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_USR_CMNDS_WRITING_CANCELED, evDesc);
							STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
						}

						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}
				}
				break;



				/////////////////////



				case MICRON_COMM_TARIFF_CLEAR:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON CLEAR TARIFF\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_TARIFF_CLEAR & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_TARIFF_CLEAR & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					if (acountsDbidsList.counterDbidsNumb > 0)
					{
						acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
						if ( !acountsDbidsList.counterDbids )
						{
							EXIT_PATTERN;
						}

						int counterDbIdIndex =  0 ;
						for( ; counterDbIdIndex < acountsDbidsList.counterDbidsNumb ; ++counterDbIdIndex )
						{
							acountsDbidsList.counterDbids[ counterDbIdIndex ] =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
																				( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
																				  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;
							IncomingGeneralBufferPos += 4 ;
							if ( IncomingGeneralBufferPos >= IncomingGeneralBufferLength )
								break;
						}

						int counterIndex = 0 ;
						for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
						{
							int status = 0;
							if ( ACOUNT_StatusGet(acountsDbidsList.counterDbids[ counterIndex ] , &status , ACOUNTS_PART_TARIFF) == OK )
							{
								switch( status )
								{
									case ACOUNTS_STATUS_IN_PROCESS:
									case ACOUNTS_STATUS_IN_QUEUE:
									{
										ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_TARIFF );
										unsigned char evDesc[EVENT_DESC_SIZE];
										COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
										COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_TARIFF_WRITING_CANCELED, evDesc);
										STORAGE_JournalUspdEvent( EVENT_TARIFF_MAP_WRITING , evDesc );
									}
									break;

									default:
										break;
								}
							}
						}
						if ( ACOUNT_Remove( &acountsDbidsList , ACOUNTS_PART_TARIFF ) == OK )
						{
							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
						}
						else
						{
							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_GEN_ERR ;
						}

						free( acountsDbidsList.counterDbids );
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_WRONG_PARAM ;
					}
				}
				break;


				/////////////////////


				case MICRON_COMM_PSM_RULES_CLEAR:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON CLEAR PSM RULES\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_PSM_RULES_CLEAR & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_PSM_RULES_CLEAR & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
					if ( !acountsDbidsList.counterDbids )
					{
						EXIT_PATTERN;
					}

					int counterIndex = 0 ;
					for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
					{
						int status = 0;
						if ( ACOUNT_StatusGet(acountsDbidsList.counterDbids[ counterIndex ] , &status , ACOUNTS_PART_POWR_CTRL) == OK )
						{
							switch( status )
							{
								case ACOUNTS_STATUS_IN_PROCESS:
								case ACOUNTS_STATUS_IN_QUEUE:
								{
									ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_POWR_CTRL );
									unsigned char evDesc[EVENT_DESC_SIZE];
									COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
									COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_PSM_RULES_WRITING_CANCELED, evDesc);
									STORAGE_JournalUspdEvent( EVENT_PSM_RULES_WRITING , evDesc );
								}
								break;

								default:
									break;
							}
						}
					}
					if ( ACOUNT_Remove( &acountsDbidsList , ACOUNTS_PART_POWR_CTRL ) == OK )
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_GEN_ERR ;
					}

					free( acountsDbidsList.counterDbids );
				}
				break;


				/////////////////////


				case MICRON_COMM_USERCMNDS_CLEAR:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON USER COMMANDS RULES\n");
					CreateOutgoingGeneralBuffer( 2 + 1 );
					OutgoingGeneralBufferPos = 0 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_USERCMNDS_CLEAR & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_COMM_USERCMNDS_CLEAR & 0x00FF ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					acountsDbidsList_t acountsDbidsList;
					memset( &acountsDbidsList , 0 , sizeof(acountsDbidsList_t) );

					IncomingGeneralBufferPos = 2 ;
					acountsDbidsList.counterDbidsNumb =	( IncomingGeneralBuffer[ IncomingGeneralBufferPos ] << 24 ) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 1 ] << 16) + \
														( IncomingGeneralBuffer[ IncomingGeneralBufferPos + 2 ] << 8) + \
														  IncomingGeneralBuffer[ IncomingGeneralBufferPos + 3 ] ;

					IncomingGeneralBufferPos += 4 ;

					acountsDbidsList.counterDbids = (unsigned long *)calloc( acountsDbidsList.counterDbidsNumb , sizeof(unsigned long) );
					if ( !acountsDbidsList.counterDbids )
					{
						EXIT_PATTERN;
					}

					int counterIndex = 0 ;
					for( ; counterIndex < acountsDbidsList.counterDbidsNumb ; ++counterIndex )
					{
						int status = 0;
						if ( ACOUNT_StatusGet(acountsDbidsList.counterDbids[ counterIndex ] , &status , ACOUNTS_PART_USER_COMMANDS) == OK )
						{
							switch( status )
							{
								case ACOUNTS_STATUS_IN_PROCESS:
								case ACOUNTS_STATUS_IN_QUEUE:
								{
									ACOUNT_StatusSet( acountsDbidsList.counterDbids[ counterIndex ] , ACOUNTS_STATUS_WRITTEN_CANCELED , ACOUNTS_PART_USER_COMMANDS );
									unsigned char evDesc[EVENT_DESC_SIZE];
									COMMON_TranslateLongToChar(acountsDbidsList.counterDbids[ counterIndex ], evDesc);
									COMMON_CharArrayDisjunction( (char *)evDesc , DESC_EVENT_USR_CMNDS_WRITING_CANCELED, evDesc);
									STORAGE_JournalUspdEvent( EVENT_USR_CMNDS_WRITING , evDesc );
								}
								break;

								default:
									break;
							}
						}
					}
					if ( ACOUNT_Remove( &acountsDbidsList , ACOUNTS_PART_USER_COMMANDS ) == OK )
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_OK ;
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] =  MICRON_RES_GEN_ERR ;
					}

					free( acountsDbidsList.counterDbids );
				}
				break;


				/////////////////////


				case MICRON_COMM_SET_TIME:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set new date and time\n");
					dateTimeStamp dtsToSet;
					memset(&dtsToSet , 0 , sizeof(dateTimeStamp));

					if( accessDenyFlag )
					{
						CreateOutgoingGeneralBuffer(2+1);

						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_SET_TIME & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_SET_TIME & 0x00FF;
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					IncomingGeneralBufferPos = 2 ;

					dtsToSet.t.s = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];
					dtsToSet.t.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];
					dtsToSet.t.h = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];

					dtsToSet.d.d = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];
					dtsToSet.d.m = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];
					dtsToSet.d.y = IncomingGeneralBuffer[ IncomingGeneralBufferPos++ ];

					int diff = 0 ;

//					if ( COMMON_SetTime(&dtsToSet, &diff , DESC_EVENT_TIME_SYNCED_BY_PROTO ) == OK )
//					{
//						STORAGE_SetSyncFlagForCounters(diff);
//						CreateOutgoingGeneralBuffer(2+1+4);
//						OutgoingGeneralBufferPos = 0 ;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_SET_TIME & 0xFF00 ) >> 8;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_SET_TIME & 0x00FF;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;

//						diff = abs( diff );
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0xFF000000 ) >> 24 ;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x00FF0000 ) >> 16 ;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x0000FF00 ) >> 8 ;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x000000FF ) ;
//					}
//					else
//					{
//						//create package with error code = 1
//						CreateOutgoingGeneralBuffer(2+1);
//						OutgoingGeneralBufferPos = 0 ;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_SET_TIME & 0xFF00 ) >> 8;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_SET_TIME & 0x00FF;
//						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_GEN_ERR ;
//					}

					if (COMMON_SetNewTimeStep1(&dtsToSet , &diff) == OK)
					{
						needToSyncFlag = 1 ;
						CreateOutgoingGeneralBuffer(2+1+4);
						OutgoingGeneralBufferPos = 0 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_SET_TIME & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_SET_TIME & 0x00FF;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;

						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0xFF000000 ) >> 24 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x00FF0000 ) >> 16 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x0000FF00 ) >> 8 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( diff & 0x000000FF ) ;
					}
					else
					{
						//create package with error code = 1
						CreateOutgoingGeneralBuffer(2+1);
						OutgoingGeneralBufferPos = 0 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_SET_TIME & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_SET_TIME & 0x00FF;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_SYNC_IS_ALREADY_STARTED ;
					}

				}
				break;

				//////////////////////

				case MICRON_COMM_GET_TIME:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Get date and time\n");
					CreateOutgoingGeneralBuffer(2+1+6);

					// first 2 bytes contain current comand
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_GET_TIME & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_GET_TIME & 0x00FF;
					//next byte is result code
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;
					//next 6 bytes are DateTimeStamp
					dateTimeStamp CurrentTime_dts;
					COMMON_GetCurrentDts( &CurrentTime_dts );
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.t.s ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.t.m ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.t.h ;

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.d.d ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.d.m ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = CurrentTime_dts.d.y ;


				}
				break;

				//////////////////////

				case MICRON_COMM_SET_ST_PIPE:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Set transparent mode\n");
					CreateOutgoingGeneralBuffer(2+1);
					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_SET_ST_PIPE & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] = MICRON_COMM_SET_ST_PIPE & 0x00FF;

					if ( IncomingGeneralBuffer[2] == 1)
					{
						unsigned long dbid = ( IncomingGeneralBuffer[3] << 24 ) | \
									( IncomingGeneralBuffer[4] << 16 ) | \
									( IncomingGeneralBuffer[5] << 8 ) | \
									( IncomingGeneralBuffer[6] ) ;
						if ( POOL_TurnTransparentOn( dbid ) < 0 )
						{
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_TRANSPARENT_MODE_ERR ;
						}
						else
						{
							transparentMode = 1 ;
							OutgoingGeneralBufferLength += 5 ;
							COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );
							OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK ;
							OutgoingGeneralBuffer[ 3 ] = 1 ;

							OutgoingGeneralBuffer[ 4 ] = ( dbid & 0xFF000000 ) >> 24 ;
							OutgoingGeneralBuffer[ 5 ] = ( dbid & 0x00FF0000 ) >> 16 ;
							OutgoingGeneralBuffer[ 6 ] = ( dbid & 0x0000FF00 ) >> 8 ;
							OutgoingGeneralBuffer[ 7 ] = ( dbid & 0x000000FF ) ;
						}
					}
					else if ( IncomingGeneralBuffer[2] == 2)
					{
						transparentMode = 0 ;
						POOL_TurnTransparentOff();
						OutgoingGeneralBufferLength += 1 ;
						COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK ;
						OutgoingGeneralBuffer[ 3 ] = 2 ;						
					}
					else if ( IncomingGeneralBuffer[2] == 3)
					{
						OutgoingGeneralBufferLength += 5 ;
						COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK ;

						unsigned long transparentDbid = 0 ;
						if ( POOL_GetTransparent(&transparentDbid) == TRUE )
						{
							OutgoingGeneralBuffer[ 3 ] = 1 ;
						}
						else
						{
							OutgoingGeneralBuffer[ 3 ] = 2 ;
						}

						OutgoingGeneralBuffer[ 4 ] = ( transparentDbid & 0xFF000000 ) >> 24 ;
						OutgoingGeneralBuffer[ 5 ] = ( transparentDbid & 0x00FF0000 ) >> 16 ;
						OutgoingGeneralBuffer[ 6 ] = ( transparentDbid & 0x0000FF00 ) >> 8 ;
						OutgoingGeneralBuffer[ 7 ] = ( transparentDbid & 0x000000FF ) ;
					}
					else
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_WRONG_PARAM ;
					}
				}
				break;

				//////////////////////

				case MICRON_COMM_SEND_PIPE:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Send data by transparent pipe\n");
					if ( POOL_InTransparent() == TRUE )
					{
						micronState = MICRON_STATE_WAITING_DATA ;
						POOL_WriteTransparent( &IncomingGeneralBuffer[2] , IncomingGeneralBufferLength - 2 );

					}
					else
					{
						CreateOutgoingGeneralBuffer(2+1);
						OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_SEND_PIPE & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ 1 ] = MICRON_COMM_SEND_PIPE & 0x00FF;
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR ;
					}
				}
				break;

				//////////////////////

				case MICRON_COMM_GET_LAST_METERAGES_DATES:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Get last meterages dates\n");
					CreateOutgoingGeneralBuffer(2+1);

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_COMM_GET_LAST_METERAGES_DATES & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_COMM_GET_LAST_METERAGES_DATES & 0x00FF;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;

					quizStatistic_t * quizStatistic = NULL ;
					int quizStatisticLength = 0 ;

					if ( STORAGE_GetQuizStatistic(&quizStatistic , &quizStatisticLength) == OK )
					{
						if ( quizStatistic )
						{
							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Get statisctic for [%d] counters" , quizStatisticLength);
							OutgoingGeneralBufferLength += (4 + (quizStatisticLength * ( 4 + 6 + 6 + 6 + 6 + 5))) ;
							COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength);

							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatisticLength & 0xFF000000 ) >> 24 ;
							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatisticLength & 0x00FF0000 ) >> 16 ;
							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatisticLength & 0x0000FF00 ) >> 8 ;
							OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatisticLength & 0x000000FF ) ;

							int counterIndex = 0 ;
							for( ; counterIndex < quizStatisticLength ; ++counterIndex)
							{
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterDbId & 0xFF000000 ) >> 24 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterDbId & 0x00FF0000 ) >> 16 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterDbId & 0x0000FF00 ) >> 8 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterDbId & 0x000000FF ) ;

								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.t.s;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.t.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.t.h;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.d.d;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.d.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastCurrentDts.d.y;

								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.t.s;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.t.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.t.h;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.d.d;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.d.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastDayDts.d.y;

								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.t.s;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.t.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.t.h;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.d.d;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.d.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastMonthDts.d.y;

								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.t.s;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.t.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.t.h;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.d.d;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.d.m;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].lastProfileDts.d.y;

								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = quizStatistic[ counterIndex ].counterType ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterSerial & 0xFF000000 ) >> 24 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterSerial & 0x00FF0000 ) >> 16 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterSerial & 0x0000FF00 ) >> 8 ;
								OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( quizStatistic[ counterIndex ].counterSerial & 0x000000FF ) ;


							}
							free(quizStatistic);
						}
						else
						{
							OutgoingGeneralBufferLength += 4 ;
							COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength);

							memset( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
							OutgoingGeneralBufferPos += 4 ;
						}
					}
					else
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_ID_NOT_FOUND ;
					}
				}
				break;

				//////////////////////

				case MICRON_COMM_USD_FS:
				{

					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON file system job\n" );
					char * fileName = NULL;

					int fileNameLastPos = -1;
					unsigned int idx = 2 ;
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Searching for fileName terminate \\0 position");
					for( ; idx < IncomingGeneralBufferLength; ++idx)
					{
						//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON check [%02X]." , IncomingGeneralBuffer[ index ] ) ;
						if ( IncomingGeneralBuffer[ idx ] == 0 )
						{
							fileNameLastPos = idx;
							break;
						}
					}

					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON position of fileName terminate \\0 is [%d]" , fileNameLastPos ) ;

					if ( fileNameLastPos > 0 )
					{
						int fileNameLength = fileNameLastPos - 2 ;

						fileName = malloc( fileNameLength + 1);
						if (fileName)
						{

							memset(fileName , 0 , fileNameLength + 1);
							memcpy(fileName , &IncomingGeneralBuffer[2] , fileNameLength );

							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON File name is [%s]" , fileName);

							//action flag is the  next character after '\0'

							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Action flag is [%X]" ,IncomingGeneralBuffer [fileNameLastPos + 1] );

							switch( IncomingGeneralBuffer [fileNameLastPos + 1] )
							{
								case 1:
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Try to get [%s] from FS", fileName ) ;
									unsigned char * buffer = NULL;
									int bufferLength = 0 ;

									int res_i = COMMON_GetFileFromFS(&buffer , &bufferLength , (unsigned char *)fileName ) ;

									if ( res_i == OK )
									{
										CreateOutgoingGeneralBuffer(2+1+1+bufferLength+16);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;
										//calculating md5
										unsigned char hash[16];
										memset(hash, 0 , 16 );
										md5_state_t md5handler;
										md5_init(&md5handler);
										md5_append( &md5handler , (const md5_byte_t *)buffer, bufferLength);
										md5_finish( &md5handler , hash );

										COMMON_BUF_DEBUG( DEBUG_MICRON_V1 "MICRON Getting file OK. Md5 hash is: " , hash , 16);

										memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , hash , 16 );
										OutgoingGeneralBufferPos += 16 ;

										memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , buffer , bufferLength);
										OutgoingGeneralBufferPos += bufferLength ;

										free(buffer);
									}
									else if ( res_i == ERROR_FILE_IS_EMPTY )
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Can not get file from FS - file is empty");
										CreateOutgoingGeneralBuffer(2+1);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_FILE_EMPTY ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Can not get file from FS");
										CreateOutgoingGeneralBuffer(2+1);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_GEN_ERR ;
									}
								}
								break;

								case 2:
								{
//									char md5Tocheck[17];
//									memcpy(&md5Tocheck, &IncomingGeneralBuffer[fileNameLastPos + 2], 16);
//									md5Tocheck[16] = 0;



									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Try to write [%s] to FS.", fileName ) ;
									//COMMON_BUF_DEBUG( DEBUG_MICRON_V1 "MICRON Checking Md5 for: " , &IncomingGeneralBuffer[fileNameLastPos + 1 + 1 + 16]  , IncomingGeneralBufferLength - (fileNameLastPos + 1 + 1 + 16) );

									if ( MICRON_CheckMD5( &IncomingGeneralBuffer[fileNameLastPos + 1 + 1] , \
											&IncomingGeneralBuffer[fileNameLastPos + 1 + 1 + 16] ,
											IncomingGeneralBufferLength - (fileNameLastPos + 1 + 1 + 16) ) == OK )
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Md5 OK. Writing...");

										if ( COMMON_WriteFileToFS( &IncomingGeneralBuffer[fileNameLastPos + 1 + 1 + 16] , \
												IncomingGeneralBufferLength - (fileNameLastPos + 1 + 1 + 16) , \
												(unsigned char *)fileName) == OK )
										{
											COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Writing OK.");
											CreateOutgoingGeneralBuffer(2+1+1);
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 2 ;
										}
										else
										{
											COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON writing file ERROR");
											CreateOutgoingGeneralBuffer(2+1);
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
											OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_GEN_ERR ;
										}

									}
									else
									{
										//debug

										#if (REV_COMPILE_RELEASE==0)
											//calculating md5
											unsigned char hash[16];
											memset(hash, 0 , 16 );
											md5_state_t md5handler;
											md5_init(&md5handler);
											md5_append( &md5handler , (const md5_byte_t *)&IncomingGeneralBuffer[fileNameLastPos + 1 + 1 + 16], IncomingCurrentBufferLength - (fileNameLastPos + 1 + 1 + 16) - 1 );
											md5_finish( &md5handler , hash );

											COMMON_BUF_DEBUG( DEBUG_MICRON_V1 "MICRON md5 ERROR. Calculated Md5 hash is: " , hash , 16);
										#endif


										CreateOutgoingGeneralBuffer(2+1);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_WRONG_PARAM ;
									}

								}
								break;

								case 3:
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Removing [%s] from FS..." , fileName );
									if ( COMMON_RemoveFileFromFS( (unsigned char *)fileName ) == OK )
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Removing file OK");
										CreateOutgoingGeneralBuffer(2+1+1);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 3 ;
									}
									else
									{
										COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Removing file ERROR");
										CreateOutgoingGeneralBuffer(2+1);
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
										OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_GEN_ERR ;
									}
								}
								break;

								default:
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON unknown action. Clearing buffer and send error code 1" );
									CreateOutgoingGeneralBuffer(2+1);
									OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
									OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
									OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_WRONG_PARAM ;
								}
								break;
							}

							free(fileName);
						}
						else
						{
							//memory error
							EXIT_PATTERN;
						}


					}
					else
					{
						COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Can not parse file name");
						CreateOutgoingGeneralBuffer(2+1);
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_WRONG_PARAM ;
					}

				}
				break;

				//////////////////////

				case MICRON_COMM_SHELL_CMD:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON Execut shell command\n");
					int cmdLength = IncomingGeneralBufferLength - 2 ;
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: cmdLen = [%d]" , cmdLength);

					char * cmd = malloc( cmdLength + 32 );
					if ( cmd )
					{
						memset( cmd , 0 , cmdLength + 32 );
						//&IncomingGeneralBuffer[2+cmdLength-1] = 0x0;
						sprintf (cmd, "%s", &IncomingGeneralBuffer[2]);

						unsigned char * outputBuffer = NULL ;
						int outputBufferLength = 0 ;
						int outputBufferPos = 0 ;
						char buf[1000];

						//
						// TO DO : check time of execution of command sended to PIPE
						//         make read from pipe in non-blocking mode or check size to read from pipe
						//

						FILE *pp;
						pp = popen(cmd, "r");
						if (pp != NULL)
						{

							COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: is open");

							while (1)
							{
								memset(buf , 0, 1000);
								char * line = fgets(buf, sizeof (buf), pp);
								if ( (line == NULL) || \
									 ( outputBufferLength >= ( MICRON_SHELL_OUTPUT_MAX_PACKAGES * ( MICRON_PACKAGE_LENGTH - MICRON_PACKAGE_HEAD_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH) ) ) )
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "PIPE END");

									outputBufferLength += 1 ;
									outputBuffer = realloc( outputBuffer , outputBufferLength );
									if ( outputBuffer )
									{
										outputBuffer[ outputBufferPos++ ] = 0 ;
									}
									else
									{
										EXIT_PATTERN;
									}

									break;
								}
								else
								{
									COMMON_STR_DEBUG( DEBUG_MICRON_V1 "PIPE > %s", line);

									outputBufferLength += strlen(line);
									outputBuffer = realloc( outputBuffer , outputBufferLength );
									if ( outputBuffer )
									{
										memcpy( &outputBuffer[ outputBufferPos ] , line,  strlen(line));
										outputBufferPos += strlen(line) ;
									}
									else
									{
										EXIT_PATTERN;
									}


								}



							}
							pclose(pp);
						}

						CreateOutgoingGeneralBuffer(2+1+outputBufferLength);
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;

						memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] , outputBuffer , outputBufferLength );

						if ( outputBuffer )
							free( outputBuffer );


						free(cmd);
					}
					else
					{
						EXIT_PATTERN;
					}
				}
				break;

				//////////////////////

				case MICRON_COMM_SEASON_FLG_SET:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON_COMM_SEASON_FLG_SET\n");

					CreateOutgoingGeneralBuffer(2+1);
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;

					unsigned char cmdRes = MICRON_RES_GEN_ERR ;

					if( accessDenyFlag )
					{
						OutgoingGeneralBuffer[ 2 ] = MICRON_RES_DENY ;
						break;
					}

					if ( IncomingGeneralBufferLength == 4 )
					{
						unsigned char seasonAllowFlg = IncomingGeneralBuffer[2];
						unsigned char seasonToSet = IncomingGeneralBuffer[3];

						if ( (STORAGE_UpdateSeasonAllowFlag( seasonAllowFlg ) == OK ) && ( STORAGE_UpdateCurrentSeason(seasonToSet) == OK ) )
						{
							cmdRes = MICRON_RES_OK ;

							unsigned char evDesc[EVENT_DESC_SIZE];
							memset(evDesc , 0 , EVENT_DESC_SIZE );
							evDesc[0] = seasonAllowFlg ;
							evDesc[1] = seasonToSet ;
							STORAGE_JournalUspdEvent( EVENT_GETTED_SEASON_FLG , evDesc );
						}
					}
					else
					{
						cmdRes = MICRON_RES_WRONG_PARAM ;
					}

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = cmdRes ;
				}
				break;

				//////////////////////

				case MICRON_COMM_SEASON_FLG_GET:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON_COMM_SEASON_FLG_GET\n");
					unsigned char seasonAllowFlg = STORAGE_GetSeasonAllowFlag() ;
					unsigned char seasonToSet = STORAGE_GetCurrentSeason() ;

					CreateOutgoingGeneralBuffer(2+1+2);
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( command & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = command & 0x00FF;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_RES_OK ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = seasonAllowFlg ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = seasonToSet ;
				}
				break;

				//////////////////////

				case MICRON_COMM_REBOOT:
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "\nMICRON reboot\n");
					//POOL_TurnTransparentOff();
					needToRebootFlag = 1 ;
					CreateOutgoingGeneralBuffer(2+1);
					OutgoingGeneralBuffer[ 0 ] = ( MICRON_COMM_REBOOT & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] = MICRON_COMM_REBOOT & 0x00FF;
					OutgoingGeneralBuffer[ 2 ] = MICRON_RES_OK ;
				}
				break;

				//////////////////////

				default: //unknown command
				{
					CreateOutgoingGeneralBuffer(2+1);
					OutgoingGeneralBuffer[ 0 ] = ( command & 0xFF00 ) >> 8;
					OutgoingGeneralBuffer[ 1 ] = command & 0x00FF;
					OutgoingGeneralBuffer[ 2 ] = MICRON_RES_UNKNOWN_COMMAND ;
				}
				break;

			}
			IncomingGeneralBufferLength = 0;
			IncomingGeneralBufferPos = 0;
			COMMON_FreeMemory( &IncomingGeneralBuffer );

		} // case MICRON_STATE_HANDLE_DATA
		break;

		//****************************

		case MICRON_STATE_PREPARE_ANSWER:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: prepeare answer ");

			//stuffing data etc
//			int IndexOutgoingBuffer=0;
//			while( IndexOutgoingBuffer < OutgoingGeneralBufferLength )
//			{
//				if( OutgoingGeneralBuffer[IndexOutgoingBuffer] == 0x7E || OutgoingGeneralBuffer[IndexOutgoingBuffer] == 0x5D)
//				{
//					OutgoingGeneralBufferLength++;
//					COMMON_AllocateMemory( &OutgoingGeneralBuffer, OutgoingGeneralBufferLength );
//					//shift elements of array
//					int i=OutgoingGeneralBufferLength-1;
//					for( ; i > IndexOutgoingBuffer ; i--)
//						OutgoingGeneralBuffer[i]=OutgoingGeneralBuffer[i-1];
//					//make stuffing
//					OutgoingGeneralBuffer[IndexOutgoingBuffer] = 0x5D;
//					OutgoingGeneralBuffer[IndexOutgoingBuffer+1] ^= 0x20;
//				}
//				IndexOutgoingBuffer++;
//			}


			MICRON_Stuffing( &OutgoingGeneralBuffer , (int *)&OutgoingGeneralBufferLength );

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: stuffing done ");

			//calculate number of total packages
			OutgoingPackageNumber = 1;
			OutgoingPackageTotal = OutgoingGeneralBufferLength / (MICRON_PACKAGE_LENGTH - MICRON_PACKAGE_HEAD_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH);

			if( (OutgoingPackageTotal * (MICRON_PACKAGE_LENGTH - MICRON_PACKAGE_HEAD_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH)) < \
				OutgoingGeneralBufferLength )
			{
				OutgoingPackageTotal++;
			}

			micronState = MICRON_STATE_SEND_ANSWER ;
		}
		break;

		//****************************

		case MICRON_STATE_SEND_ANSWER:
		{
			DEB_TRACE(DEB_IDX_MICRON)

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: send answer ");

			//TODO
			unsigned int FirstByte = (OutgoingPackageNumber -1)  * (MICRON_PACKAGE_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH - MICRON_PACKAGE_HEAD_LENGTH);
			unsigned int TailByte = 0;
			if( (OutgoingPackageNumber) == OutgoingPackageTotal )
			{
				TailByte = OutgoingGeneralBufferLength;
			}
			else
			{
				TailByte = FirstByte + (MICRON_PACKAGE_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH - MICRON_PACKAGE_HEAD_LENGTH);
			}

			unsigned char * IntermediateBuffer = NULL ;
			unsigned int IntermediateBufferLength=TailByte - FirstByte + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH;

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON try to allocate memory for intermediate buffer");
			COMMON_AllocateMemory( &IntermediateBuffer , IntermediateBufferLength );
			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON memory allocated successful");

			memset( IntermediateBuffer , 0, IntermediateBufferLength );
			unsigned int IntermediateBufferPos = MICRON_PACKAGE_HEAD_LENGTH + MICRON_PACKAGE_CHAIN_LENGTH;

			unsigned char CRC = 0;

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: copy part of data");

			unsigned int Index = FirstByte;
			while( Index < TailByte )
			{
				IntermediateBuffer[ IntermediateBufferPos ] = OutgoingGeneralBuffer[ Index ];
				CRC ^= IntermediateBuffer[ IntermediateBufferPos ];
				IntermediateBufferPos++;
				Index++;
			}

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: done getting crc (crc is [%02x])", CRC);

			//creating h7E chain of the package
			Index = 0;
			for( ; Index < MICRON_PACKAGE_CHAIN_LENGTH ; Index++)
				IntermediateBuffer[Index] = 0x7E;

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: done packet bounding");

			//creating head of the package
			IntermediateBuffer[MICRON_PACKAGE_TYPE_POS]=MICRON_P_DATA_ANSW;
			IntermediateBuffer[MICRON_PACKAGE_LENGTH_FIRST_POS] = ((IntermediateBufferLength - MICRON_PACKAGE_HEAD_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH) & 0xFF00) >> 8;
			IntermediateBuffer[MICRON_PACKAGE_LENGTH_SEC_POS] = (IntermediateBufferLength - MICRON_PACKAGE_HEAD_LENGTH - MICRON_PACKAGE_CHAIN_LENGTH) & 0xFF;
			IntermediateBuffer[MICRON_PACKAGE_NUMBER_FIRST_POS] = (OutgoingPackageNumber & 0xFF00) >> 8;
			IntermediateBuffer[MICRON_PACKAGE_NUMBER_SEC_POS] = OutgoingPackageNumber & 0xFF;
			IntermediateBuffer[MICRON_PACKAGE_TOTAL_FIRST_POS] = (OutgoingPackageTotal & 0xFF00) >> 8;
			IntermediateBuffer[MICRON_PACKAGE_TOTAL_SEC_POS] = OutgoingPackageTotal & 0xFF;
			IntermediateBuffer[MICRON_PACKAGE_CRC_POS] = CRC;

			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: done paket heading");

			int error_counter = 0 ;
			int bytesRemaining = IntermediateBufferLength;

			COMMON_BUF_DEBUG( DEBUG_MICRON_V1 "MICRON: tx ", IntermediateBuffer, IntermediateBufferLength);

			error_counter = 0 ;
			while( 1 )
			{
				unsigned long bytesWritten = 0;
				if ( CONNECTION_Write( &IntermediateBuffer[ IntermediateBufferLength - bytesRemaining ] , bytesRemaining, &bytesWritten) < 0)
				{
					COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: send packet error");
					micronState = MICRON_STATE_OFFLINE ;
					break;
				}

				if (bytesWritten == 0)
				{
					++error_counter;
				}
				else
				{
					bytesRemaining -= bytesWritten ;
					error_counter = 0;
				}


				if ( (bytesRemaining == 0) || ( error_counter > MICRON_IO_ATTEMPTS ) )
				{
					break;
				}
			}

			if (bytesRemaining == 0) {
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: send packet done, need recv ACK");
				waitAckFlag = 1 ;
				micronState = MICRON_STATE_WAITING_DATA ;

			} else {

				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: send packet end error");
				micronState = MICRON_STATE_OFFLINE ;
			}

			COMMON_FreeMemory( &IntermediateBuffer );

		}
		break;

		//****************************

		default:
		{
			micronState = MICRON_STATE_OFFLINE ;
		}
		break;
	}
	return res;
}


//
//
//

int MICRON_CheckReceivedData(unsigned char * type , int * crcFlag, unsigned int * packageNumber, int * remainingPackages, int * firstPayloadBytePos, int * packageLength)
{
	DEB_TRACE(DEB_IDX_MICRON)

//	if ( IncomingCurrentBufferLength < (MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH) )
//	{
//		COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON: total package length [%d] is less then 16" , IncomingCurrentBufferLength );
//		return ERROR_GENERAL ;
//	}

	int FirstPos = (-1);
	unsigned int idx = 0;
	if ( IncomingCurrentBufferLength >= (MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH) )
	{
		for ( idx = 0 ; idx < (IncomingCurrentBufferLength - MICRON_PACKAGE_CHAIN_LENGTH) ; idx++)
		{
			int ChainPresenseFlag = 1;
			unsigned int chainCounter;
			for ( chainCounter = idx ; chainCounter < (idx + MICRON_PACKAGE_CHAIN_LENGTH) ; chainCounter++)
			{
				if( IncomingCurrentBuffer[chainCounter] != 0x7E)
				{
					ChainPresenseFlag = 0;
					break;
				}
			}
			if (ChainPresenseFlag == 1)
			{
				if ( ( idx + MICRON_PACKAGE_CHAIN_LENGTH ) < IncomingCurrentBufferLength )
				{
					if ( IncomingCurrentBuffer[ idx + MICRON_PACKAGE_CHAIN_LENGTH ] != 0x7E )
					{
						FirstPos = idx;
						break;
					}
				}
			}
		}
	}

	if (FirstPos < 0)
		return ERROR_GENERAL;

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON First package position is [%d]" , FirstPos);

	if 	( (FirstPos + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH) > (int)IncomingCurrentBufferLength )
	{
		COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON First position + head length + chain length is higher then incoming buffer length");
		return ERROR_GENERAL;
	}

	(*packageLength) =  ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_LENGTH_FIRST_POS ] <<8 ) | \
									IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_LENGTH_SEC_POS ] ;
	(*type) = (unsigned char)IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_TYPE_POS ] ;

	if ( (*type) == MICRON_P_DATA )
	{
		if ( (FirstPos + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH + (*packageLength) ) > \
				(int)IncomingCurrentBufferLength )
		{
			return ERROR_GENERAL;
		}

		//check number of package and number of remaining packages
		(*packageNumber) = ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_NUMBER_FIRST_POS ] ) << 8 | \
							( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_NUMBER_SEC_POS ] );

		int packageTotal = ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_TOTAL_FIRST_POS ] ) << 8 | \
							( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_TOTAL_SEC_POS ] );

		//osipov, delete +1 to correct hanfling of recieved packets if its count start from 1 (not 0)
		(*remainingPackages) = packageTotal - (*packageNumber);

		//check CRC
		unsigned char ControlCRC = IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_CRC_POS ] ;
		unsigned char CalculatedCRC=0;
		for (idx = ( FirstPos + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ) ; \
			 idx < (unsigned int)( FirstPos + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH + (*packageLength) ) ; \
			 idx++ )
		{
			CalculatedCRC ^= IncomingCurrentBuffer[ idx ] ;
		}

		COMMON_STR_DEBUG( DEBUG_MICRON_V1 "CalculatedCRC = [%02X] , ControlCRC = [%02X] " , CalculatedCRC , ControlCRC );

		if ( CalculatedCRC == ControlCRC )
			*crcFlag = OK ;
		else
			*crcFlag = ERROR_GENERAL;

		//check position of first byte of paylod
		(*firstPayloadBytePos) = FirstPos + MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH ;
		return OK;
	}
	else if ( (*type == MICRON_P_ACK) || (*type == MICRON_P_REPEAT) )
	{
		(*packageNumber) = ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_NUMBER_FIRST_POS ] ) << 8 | \
							( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_NUMBER_SEC_POS ] );
		if ( (*packageNumber) == 0x00 )
		{
			//error type
			(*type) = MICRON_P_WRONG_TYPE ;
		}

		if ( (*packageLength) != 0 )
		{
			//package length is not equal zero
			(*type) = MICRON_P_WRONG_TYPE ;
		}

		int packageTotal = ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_TOTAL_FIRST_POS ] ) << 8 | \
							( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_TOTAL_SEC_POS ] );

		//package total should be equal 0

		if ( packageTotal != 0x00 )
		{
			//error type
			(*type) = MICRON_P_WRONG_TYPE ;
		}

		//CRC should be equal 0
		if ( ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_CRC_POS ] ) != 0x00 )
		{
			//error type
			(*type) = MICRON_P_WRONG_TYPE ;
		}
		return OK;
	}
	else
	{
		//CRC should be equal 0
		if ( ( IncomingCurrentBuffer[ FirstPos + MICRON_PACKAGE_CRC_POS ] ) != 0x00 )
		{
			//error type
			(*type) = MICRON_P_WRONG_TYPE ;
		}
		return OK;
	}

}
//
//
//

int MICRON_SendShortMessage(unsigned int PartNumber, const unsigned char mask)
{
	DEB_TRACE(DEB_IDX_MICRON)

	int res=ERROR_GENERAL;
	if(mask==MICRON_P_PING)
	{
		PartNumber=0;
	}
	const int MessageLength = MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH;
	unsigned char Message[ MessageLength ];

	memset(Message, 0 , (MICRON_PACKAGE_CHAIN_LENGTH + MICRON_PACKAGE_HEAD_LENGTH));
	int i=0;
	for( ; i < MICRON_PACKAGE_CHAIN_LENGTH; i++ )
		Message[i]=0x7E;
	Message[MICRON_PACKAGE_TYPE_POS]=mask;
	Message[MICRON_PACKAGE_NUMBER_FIRST_POS]=(PartNumber & 0xFF00) >> 8;
	Message[MICRON_PACKAGE_NUMBER_SEC_POS]=PartNumber & ~0xFF00;
	//short message is ready. trying to send it

	int bytesRemaining = MessageLength;
	//for ( i=0 ; i < MICRON_IO_ATTEMPTS ; i++)
	int error_counter = 0;
	while (error_counter < MICRON_IO_ATTEMPTS)
	{
		unsigned long int w = 0 ;
		res = CONNECTION_Write( &Message[ ( MessageLength - bytesRemaining ) ] , bytesRemaining , &w);
		if ( res != OK)
			return res;
		if (w == 0)
			error_counter++;
		bytesRemaining -= w;
		if (bytesRemaining == 0)
		{
			break;
		}
	}
	if (bytesRemaining == 0)
		return OK;
	else
		return ERROR_GENERAL;
}

//
//
//


int MICRON_OutgoingBufferProtect()
{
	DEB_TRACE(DEB_IDX_MICRON)

	pthread_mutex_lock(&outBufMux);
	return OK;
}

//
//
//

int MICRON_OutgoingBufferUnprotect()
{
	DEB_TRACE(DEB_IDX_MICRON)

	pthread_mutex_unlock(&outBufMux);
	return OK;
}

//
//
//

int CreateOutgoingGeneralBuffer(const int length)
{
	DEB_TRACE(DEB_IDX_MICRON);

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON CreateOutgoingGeneralBuffer" );
	MICRON_OutgoingBufferProtect();
	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON outgoing buffer protected successful" );

	OutgoingGeneralBufferLength = length;
	OutgoingGeneralBufferPos = 0;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer, OutgoingGeneralBufferLength );

	return OK;
}

int ClearOutgoingGeneralBuffer()
{
	DEB_TRACE(DEB_IDX_MICRON)

	if (OutgoingGeneralBuffer != NULL)
	{
		COMMON_FreeMemory( &OutgoingGeneralBuffer );
		OutgoingGeneralBufferLength = 0;
		OutgoingGeneralBufferPos = 0;

		MICRON_OutgoingBufferUnprotect();

	}
	return OK;
}

//
//
//

void MICRON_SendTransparent(unsigned char * sendBuf, int sendSize){
	DEB_TRACE(DEB_IDX_MICRON);

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON_SendTransparent for [%d] bytes" , sendSize);

	//send recieved buffer to server over connection with code 00 13
	//free pointer

	CreateOutgoingGeneralBuffer( sendSize + 2 + 1);

	OutgoingGeneralBuffer[0] = ( MICRON_COMM_SEND_PIPE & 0xFF00 ) >> 8;
	OutgoingGeneralBuffer[1] = MICRON_COMM_SEND_PIPE & (~0xFF00) ;
	OutgoingGeneralBuffer[2] = MICRON_RES_OK ;
	OutgoingGeneralBufferPos = 3 ;

	if ( sendBuf )
	{
		if ( sendSize > 0 )
		{
			memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , sendBuf , sendSize * sizeof( unsigned char ) );
			OutgoingGeneralBufferPos += sendSize ;
		}
	}

	//COMMON_FreeMemory( &sendBuf );
	TransparentFlag = 1;
	//OutgoingBufferUnprotect();

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON_SendTransparent exit");
}

void MICRON_GetMeterages(unsigned char mask, unsigned long counterDbId)
{
	DEB_TRACE(DEB_IDX_MICRON)

	int storage;
	switch (mask)
	{
		case MICRON_METR_CURRENT:
			storage = STORAGE_CURRENT;
			break;

		case MICRON_METR_DAY:
			storage = STORAGE_DAY;
			break;

		case MICRON_METR_MONTH:
			storage = STORAGE_MONTH;
			break;

		default:
			return ;
			break;
	}

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (mask & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = mask & 0xFF ;

	meteregesArray_t * meterages = NULL;

	if ( STORAGE_GetMeteragesForDb( storage , counterDbId , &meterages) == OK )
	{
		if ( meterages )
		{
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( meterages->meteragesCount & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( meterages->meteragesCount & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (meterages->meteragesCount & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meteragesCount & 0xFF ;

			OutgoingGeneralBufferLength += ( (4*4*4) /*A+A-R+R- for 4 tariffs*/ + 1 /*ratio*/ + 1/*status*/ + 1 /*delim*/ + 6 /*dts*/ ) * meterages->meteragesCount ;
			COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

			unsigned int meteragesIndex = 0;
			for( meteragesIndex = 0 ; meteragesIndex < meterages->meteragesCount ; ++meteragesIndex  )
			{
				int tariffIndex = 0 ;
				for( ; tariffIndex < 4 ; ++tariffIndex)
				{
					int eIndex = 0 ;
					for( ;eIndex < 4 ; ++eIndex )
					{
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( meterages->meterages[meteragesIndex]->t[ tariffIndex ].e[ eIndex ] & 0xFF000000) >> 24 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( meterages->meterages[meteragesIndex]->t[ tariffIndex ].e[ eIndex ] & 0xFF0000 ) >> 16 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (meterages->meterages[meteragesIndex]->t[ tariffIndex ].e[ eIndex ] & 0xFF00 ) >> 8 ;
						OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->t[ tariffIndex ].e[ eIndex ] & 0xFF ;
					}
				}

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (meterages->meterages[meteragesIndex]->ratio & 0xFF);
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->status ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->delimeter ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.t.s ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.t.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.t.h ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.d.d ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.d.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = meterages->meterages[meteragesIndex]->dts.d.y ;

			}

			//osipov 29-11-13
			/*
			for(meteragesIndex = 0 ; meteragesIndex < meterages->meteragesCount ; ++meteragesIndex){
				free(meterages->meterages[ meteragesIndex ]);
			}
			free(meterages->meterages);
			free(meterages);
			*/
			COMMON_FreeMeteragesArray (&meterages);
		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		if( STORAGE_CheckCounterDbIdPresenceInConfiguration( counterDbId ) == FALSE )
		{
			OutgoingGeneralBuffer[ 2 ] = MICRON_RES_ID_NOT_FOUND ;
		}

		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;

		//debug
		//EXIT_PATTERN;
	}
	return ;
}

//
//
//

void MICRON_GetEnergies( unsigned long counterDbId )
{
	DEB_TRACE(DEB_IDX_MICRON)

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON MICRON_GetEnergies() started");
	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PROFILE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PROFILE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_PROFILE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_PROFILE & 0xFF ;

	energiesArray_t * energies = NULL;

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON MICRON_GetEnergies() try to get profiles to DB");

	if ( STORAGE_GetProfileForDb(counterDbId, &energies) == OK )
	{
		COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON MICRON_GetEnergies() geting profiles to DB");
		if ( energies )
		{
			COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON total [%d] metrages" , energies->energiesCount);

			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( energies->energiesCount & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( energies->energiesCount & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (energies->energiesCount & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energiesCount & 0xFF ;

			OutgoingGeneralBufferLength += (4*4 /*A+A-R+R-*/ + 1 /*ratio*/ + 1/*status*/ + 6 /*dts*/) * energies->energiesCount ;
			COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

			//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON date is %u-%u-%u" , energies->energies[1]->dts.d.d , energies->energies[1]->dts.d.m , energies->energies[1]->dts.d.y);

			unsigned int energiesIndex = 0 ;
			for ( ; energiesIndex < energies->energiesCount ; ++energiesIndex )
			{
				//COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON adding [%d] metrage to bytearray" , energiesIndex);

				int eIndex = 0 ;
				for( ; eIndex < 4 ; ++eIndex )
				{
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( energies->energies[ energiesIndex ]->p.e[ eIndex ] & 0xFF000000) >> 24 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( energies->energies[ energiesIndex ]->p.e[ eIndex ] & 0xFF0000 ) >> 16 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (energies->energies[ energiesIndex ]->p.e[ eIndex ] & 0xFF00 ) >> 8 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->p.e[ eIndex ] & 0xFF ;
				}

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->ratio & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->status ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.t.s ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.t.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.t.h ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.d.d ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.d.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = energies->energies[ energiesIndex ]->dts.d.y ;
			}

			//osipov 29-11-13
			/*
			for( energiesIndex = 0 ; energiesIndex < energies->energiesCount ; ++energiesIndex )
			{
				free ( energies->energies[ energiesIndex ] ) ;
			}
			free (energies->energies) ;
			free ( energies ) ;
			*/
			COMMON_FreeEnergiesArray(&energies);
		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		if( STORAGE_CheckCounterDbIdPresenceInConfiguration( counterDbId ) == FALSE )
		{
			OutgoingGeneralBuffer[ 2 ] = MICRON_RES_ID_NOT_FOUND ;
		}

		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;

	}

	return ;
}

//
//
//

void MICRON_GetJournal( unsigned int mask , unsigned long counterDbId )
{
	DEB_TRACE(DEB_IDX_MICRON);

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON_GetJournal start for mask [%08X]" , mask);

	uspdEventsArray_t * uspdEventsArray = NULL ;

	int journalNumber = 0 ;

	switch(mask)
	{
		case MICRON_METR_COUNTER_JOURNAL_OPEN_BOX:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_BOX ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_OPEN_CONTACT ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_DEVICE_SWITCH ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_SYNC_HARD:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_HARD ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SYNC_SOFT ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_SET_TARIFF ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_POWER_SWITCH ;
			break;
		//-----------------

		case MICRON_METR_COUNTER_JOURNAL_PQP:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_PQP ;
			break;

		case MICRON_METR_USPD_JOURNAL_GENERAL:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_GENERAL ;
			break;
		//-----------------

		case MICRON_METR_USPD_JOURNAL_TARIFF:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_TARIFF ;
			break;
		//-----------------

		case MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_POWER_SWITCH_MODULE ;
			break;
		//-----------------

		case MICRON_METR_USPD_JOURNAL_RF_EVENTS:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_RF_EVENTS ;
			break;
		//-----------------

		case MICRON_METR_USPD_JOURNAL_PLC_EVENTS:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_PLC_EVENTS ;
			break;
		//-----------------

		case MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_EXTENDED_IO_CARD ;
			break;

		case MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE:
			journalNumber = EVENT_USPD_JOURNAL_NUMBER_FIRMWARE ;
			break;

		case MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED:
			journalNumber = EVENT_COUNTER_JOURNAL_NUMBER_STATE_WORD;
			break;

		default:
			break;
	}

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON_GetJournal OutgoingGeneralBufferPos = [%d] OutgoingGeneralBufferLength=[%d]" , OutgoingGeneralBufferPos , OutgoingGeneralBufferLength);

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (mask & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = mask & 0xFF ;



	if ( STORAGE_GetJournalForDb( &uspdEventsArray , counterDbId , journalNumber ) == OK )
	{
		COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON_GetJournal ptr uspdEventsArray = [%p]", uspdEventsArray );
		if ( uspdEventsArray )
		{
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( uspdEventsArray->eventsCount & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( uspdEventsArray->eventsCount & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (uspdEventsArray->eventsCount & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->eventsCount & 0xFF ;

			OutgoingGeneralBufferLength += ( 6/*dts*/ + 1 /*evType*/ + 10 /*evDesc*/) * uspdEventsArray->eventsCount;
			COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

			int eventIndex = 0 ;
			for( ; eventIndex < uspdEventsArray->eventsCount ; ++eventIndex )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.t.s ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.t.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.t.h ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.d.d ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.d.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->dts.d.y ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = uspdEventsArray->events[ eventIndex ]->evType;

				memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos] , uspdEventsArray->events[ eventIndex ]->evDesc , EVENT_DESC_SIZE ) ;
				OutgoingGeneralBufferPos += EVENT_DESC_SIZE ;
			}



			//free events array

			//osipov 29-11-13
			/*
			for( ; eventIndex < uspdEventsArray->eventsCount ; ++eventIndex )
			{
				free( uspdEventsArray->events[ eventIndex ] );
			}
			free(uspdEventsArray->events);
			free(uspdEventsArray);
			*/
			COMMON_FreeUspdEventsArray(&uspdEventsArray);
		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		if ( counterDbId != STORAGE_USPD_DBID )
		{
			if( STORAGE_CheckCounterDbIdPresenceInConfiguration( counterDbId ) == FALSE )
			{
				OutgoingGeneralBuffer[ 2 ] = MICRON_RES_ID_NOT_FOUND ;
			}
		}

		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;


	}


	return ;


}


//
//
//

void MICRON_GetPQP( unsigned int mask , unsigned long counterDbId )
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( mask & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (mask & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = mask & 0xFF ;

	powerQualityParametres_t pqp;
	memset(&pqp , 0 , sizeof( powerQualityParametres_t ) );
	if ( STORAGE_GetPQPforDB(&pqp , counterDbId) == OK )
	{
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)0x00 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)0x00 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)0x00 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (unsigned char)0x01 ;

		OutgoingGeneralBufferLength += 12 /*U*/ + 12 /*I*/ + 16/*P*/ + 16/*Q*/ + 16 /*S*/ + 12 /*cos*/ + 12/*tg*/ + 4 /*F*/ + 6 /*dts*/;
		COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

		//
		// U
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.a & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.a & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.U.a & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.U.a & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.b & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.b & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.U.b & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.U.b & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.c & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.U.c & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.U.c & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.U.c & 0xFF ;

		//
		// I
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.a & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.a & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.I.a & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.I.a & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.b & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.b & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.I.b & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.I.b & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.c & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.I.c & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.I.c & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.I.c & 0xFF ;

		//
		// P
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.a & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.a & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.P.a & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.P.a & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.b & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.b & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.P.b & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.P.b & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.c & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.c & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.P.c & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.P.c & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.sigma & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.P.sigma & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.P.sigma & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.P.sigma & 0xFF ;

		//
		// Q
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.a & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.a & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.Q.a & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.Q.a & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.b & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.b & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.Q.b & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.Q.b & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.c & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.c & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.Q.c & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.Q.c & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.sigma & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.Q.sigma & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.Q.sigma & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.Q.sigma & 0xFF ;

		//
		// S
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.a & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.a & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.S.a & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.S.a & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.b & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.b & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.S.b & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.S.b & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.c & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.c & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.S.c & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.S.c & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.sigma & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.S.sigma & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.S.sigma & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.S.sigma & 0xFF ;

		//
		// cos
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_ab & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_ab & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.cos_ab & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.cos_ab & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_bc & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_bc & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.cos_bc & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.cos_bc & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_ac & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.cos_ac & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.cos_ac & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.cos_ac & 0xFF ;

		//
		// tan
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_ab & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_ab & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.tan_ab & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.tan_ab & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_bc & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_bc & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.tan_bc & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.tan_bc & 0xFF ;

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_ac & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.coeff.tan_ac & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.coeff.tan_ac & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.coeff.tan_ac & 0xFF ;

		//
		// frequency
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.frequency & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( pqp.frequency & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (pqp.frequency & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.frequency & 0xFF ;

		//
		// dts
		//

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.t.s ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.t.m ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.t.h ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.d.d ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.d.m ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = pqp.dts.d.y ;

	}
	else
	{
		if( STORAGE_CheckCounterDbIdPresenceInConfiguration( counterDbId ) == FALSE )
		{
		OutgoingGeneralBuffer[ 2 ] = MICRON_RES_ID_NOT_FOUND ;
		}

		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;
	}

	return ;
}

//
//
//

void MICRON_GetTariffState()
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_TARIFF_WRITE_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_TARIFF_WRITE_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_TARIFF_WRITE_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_TARIFF_WRITE_STATE & 0xFF ;

	acountsMap_t * accTarifMap = NULL ;
	int accTarifMapSize = 0 ;

	if ( ACOUNT_GetMap( &accTarifMap , &accTarifMapSize , ACOUNTS_PART_TARIFF ) == OK )
	{
		if ( accTarifMap )
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
			OutgoingGeneralBufferPos+=3;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;

			OutgoingGeneralBufferLength += 4 + ( (4 + 1) * accTarifMapSize ) ;
			COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;

			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMapSize & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMapSize & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accTarifMapSize & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMapSize & 0xFF ;

			int counterIndex = 0 ;

			for( ; counterIndex < accTarifMapSize ; ++counterIndex )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMap[ counterIndex ].counterDbId & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMap[ counterIndex ].counterDbId & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accTarifMap[ counterIndex ].counterDbId & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMap[ counterIndex ].counterDbId & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMap[ counterIndex ].status ;
			}

			free( accTarifMap );

		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		//OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR ;
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;
	}
}

//
//
//

void MICRON_GetPSMState()
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE & 0xFF ;

	acountsMap_t * accPsmMap = NULL ;
	int accPsmMapSize = 0 ;

	if ( ACOUNT_GetMap( &accPsmMap , &accPsmMapSize , ACOUNTS_PART_POWR_CTRL ) == OK )
	{
		if ( accPsmMap )
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
			OutgoingGeneralBufferPos+=3;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;

			OutgoingGeneralBufferLength += 4 + ( (4 + 1) * accPsmMapSize ) ;
			COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;

			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accPsmMapSize & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accPsmMapSize & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accPsmMapSize & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accPsmMapSize & 0xFF ;

			int counterIndex = 0 ;

			for( ; counterIndex < accPsmMapSize ; ++counterIndex )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accPsmMap[ counterIndex ].counterDbId & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accPsmMap[ counterIndex ].counterDbId & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accPsmMap[ counterIndex ].counterDbId & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accPsmMap[ counterIndex ].counterDbId & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accPsmMap[ counterIndex ].status ;
			}

			free( accPsmMap );

		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;
	}
}

//
//
//
/*
void MICRON_GetFirmwareState()
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICES_UPDATING_FIRMWARE_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICES_UPDATING_FIRMWARE_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_DEVICES_UPDATING_FIRMWARE_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_DEVICES_UPDATING_FIRMWARE_STATE & 0xFF ;

	acountsMap_t * accTarifMap = NULL ;
	int accTarifMapSize = 0 ;

	if ( ACOUNT_GetMap( &accTarifMap , &accTarifMapSize , ACOUNTS_PART_FIRMWARE ) == OK )
	{
		if ( accTarifMap )
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
			OutgoingGeneralBufferPos+=3;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;

			OutgoingGeneralBufferLength += 4 + ( (4 + 1) * accTarifMapSize ) ;
			COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;

			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMapSize & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMapSize & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accTarifMapSize & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMapSize & 0xFF ;

			int counterIndex = 0 ;

			for( ; counterIndex < accTarifMapSize ; ++counterIndex )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMap[ counterIndex ].counterDbId & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( accTarifMap[ counterIndex ].counterDbId & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (accTarifMap[ counterIndex ].counterDbId & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMap[ counterIndex ].counterDbId & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = accTarifMap[ counterIndex ].status ;
			}

			free( accTarifMap );

		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		//OutgoingGeneralBuffer[ 2 ] = MICRON_RES_GEN_ERR ;
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;
	}
}
*/
//
//
//

void MICRON_GetTransactionStatistic()
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICES_TRANSACTION_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICES_TRANSACTION_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_DEVICES_TRANSACTION_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_DEVICES_TRANSACTION_STATE & 0xFF ;

	counterStat_t * counterStat = NULL ;
	int counterStatLength = 0 ;
	if ( STORAGE_GetCounterStatistic(&counterStat , &counterStatLength) == OK )
	{
		if ( counterStat )
		{
			OutgoingGeneralBufferLength += (counterStatLength * 33) ;
			COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;

			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStatLength & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStatLength & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterStatLength & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStatLength & 0xFF ;

			int idx = 0 ;
			for( ; idx < counterStatLength ; ++idx )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStat[idx].dbid & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStat[idx].dbid & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterStat[idx].dbid & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].dbid & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStat[idx].serial & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( counterStat[idx].serial & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (counterStat[idx].serial & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].serial & 0xFF ;

				memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , counterStat[idx].serialRemote , SIZE_OF_SERIAL );
				OutgoingGeneralBufferPos += SIZE_OF_SERIAL ;
				//OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.t.s ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.t.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.t.h ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.d.d ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.d.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolRun.d.y ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.t.s ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.t.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.t.h ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.d.d ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.d.m ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolEnd.d.y ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].poolStatus ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].pollErrorTransactionIndex ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = counterStat[idx].pollErrorTransactionResult ;
			}

			free(counterStat);
			counterStat = NULL ;
		}
		else
		{
//			OutgoingGeneralBufferLength += 4 ;
//			COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos += 4;
		}
	}
	else
	{
//		OutgoingGeneralBufferLength += 4 ;
//		COMMON_AllocateMemory(&OutgoingGeneralBuffer , OutgoingGeneralBufferLength) ;
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos += 4;
	}
}

//
//
//

void MICRON_GetDiscretOutputState()
{
		DEB_TRACE(DEB_IDX_MICRON)


#if REV_COMPILE_EXIO

	OutgoingGeneralBufferLength += 4 + 4 + 10;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	//mask
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF000000) >> 24 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF0000 ) >> 16 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF ;

	//blocks number
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
		OutgoingGeneralBufferPos+=3;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;

	//discret output state
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 0 ;

	// TODO
#else
	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_DISCRET_OUTPUT_STATE & 0xFF ;

	memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
	OutgoingGeneralBufferPos+=4;
#endif

}

//
//
//

void MICRON_GetPlcState()
{
		DEB_TRACE(DEB_IDX_MICRON)


#if REV_COMPILE_PLC

	  OutgoingGeneralBufferLength += 4 + 4    + 4 /*NetworkID*/ + 1/*networkMode*/ + 4/*nodesCounter*/;
	  COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF000000) >> 24 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF0000 ) >> 16 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_PLC_NETWORK_STATE & 0xFF00 ) >> 8 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_PLC_NETWORK_STATE & 0xFF ;

	  memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
	  OutgoingGeneralBufferPos+=3;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;


	  unsigned int plcNetworkId = (unsigned int)PLC_GetNetowrkId();
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( plcNetworkId & 0xFF000000) >> 24 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( plcNetworkId & 0xFF0000 ) >> 16 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( plcNetworkId & 0xFF00 ) >> 8 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = plcNetworkId & 0xFF ;

	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = PLC_GetNetworkMode();

	  unsigned int nodesCounterOutgoingGeneralBufferPos = OutgoingGeneralBufferPos ;
	  unsigned int nodesCounter = (unsigned int)PLC_INFO_TABLE_GetRemotesCount();
	  unsigned int newNodesCounter = nodesCounter ;

	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesCounter & 0xFF000000) >> 24 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesCounter & 0xFF0000 ) >> 16 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesCounter & 0xFF00 ) >> 8 ;
	  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = nodesCounter & 0xFF ;

	  if ( nodesCounter > 0 )
	  {
		  OutgoingGeneralBufferLength += ( nodesCounter * ( 4 + 2 + 2 + 1) ) ;
		  COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );
		  plcNode_t tempNode;
		  unsigned int nodeIndex = 0 ;
		  for( ; nodeIndex < nodesCounter ; ++nodeIndex )
		  {
			  memset(&tempNode , 0 , sizeof(plcNode_t) );
			  if ( PLC_INFO_TABLE_GetRemoteNodeByIndex( nodeIndex , &tempNode ) == OK )
			  {
				  unsigned int serial = 0 ;
				  sscanf( (char *)tempNode.serial , "%u" , &serial );

				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( serial & 0xFF000000) >> 24 ;
				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( serial & 0xFF0000 ) >> 16 ;
				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( serial & 0xFF00 ) >> 8 ;
				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = serial & 0xFF ;

				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( tempNode.did & 0xFF00 ) >> 8 ;
				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = tempNode.did & 0xFF ;

				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( tempNode.pid & 0xFF00 ) >> 8 ;
				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = tempNode.pid & 0xFF ;

				  OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = tempNode.age ;
			  }
			  else
			  {
				  newNodesCounter = nodeIndex + 1;
				  break;
			  }

		  }

		  if ( newNodesCounter != nodesCounter )
		  {
			  OutgoingGeneralBuffer[ nodesCounterOutgoingGeneralBufferPos++ ] = ( newNodesCounter & 0xFF000000) >> 24 ;
			  OutgoingGeneralBuffer[ nodesCounterOutgoingGeneralBufferPos++ ] = ( newNodesCounter & 0xFF0000 ) >> 16 ;
			  OutgoingGeneralBuffer[ nodesCounterOutgoingGeneralBufferPos++ ] = ( newNodesCounter & 0xFF00 ) >> 8 ;
			  OutgoingGeneralBuffer[ nodesCounterOutgoingGeneralBufferPos++ ] = newNodesCounter & 0xFF ;
		  }

	  }

#else

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_PLC_NETWORK_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_PLC_NETWORK_STATE & 0xFF ;

	memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
	OutgoingGeneralBufferPos+=4;

#endif

}

//
//
//

void MICRON_GetRfState()
{
	DEB_TRACE(DEB_IDX_MICRON)

#if REV_COMPILE_RF
	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_RF_NETWORK_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_RF_NETWORK_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_RF_NETWORK_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_RF_NETWORK_STATE & 0xFF ;

	memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 3 );
	OutgoingGeneralBufferPos+=3;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = 1 ;


	int panId = 0 ;
	if ( RF_GetPanId(&panId) == OK )
	{
		OutgoingGeneralBufferLength += 2 + 1 + 4 ;
		COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (panId & 0xFF00 ) >> 8 ;
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = panId & 0xFF ;

		int channels = 0 ;
		RF_GetChannels(&channels);
		OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = channels & 0xFF ;

		int nodesNumb = RF_INFO_TABLE_GetRemotesCount() ;






		rfNode_t * nodes = NULL;
		nodes = (rfNode_t*)calloc( nodesNumb , sizeof(rfNode_t) );
		if ( nodes )
		{
			int succesNodesCounter;
			int nodesIndex = 0 ;
			for( ; nodesIndex < nodesNumb ; ++nodesIndex )
			{
				rfNode_t * currentNode = &nodes[nodesIndex];
				if (RF_INFO_TABLE_GetRemoteNodeByIndex(nodesIndex , &currentNode) == OK )
				{
					succesNodesCounter++ ;
				}
			}

			if ( succesNodesCounter == nodesIndex )
			{
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesNumb & 0xFF000000) >> 24 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesNumb & 0xFF0000 ) >> 16 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodesNumb & 0xFF00 ) >> 8 ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = nodesNumb & 0xFF ;

				OutgoingGeneralBufferLength += (SIZE_OF_SERIAL+2+2) * nodesNumb ;
				COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

				for( nodesIndex = 0 ; nodesIndex < nodesNumb ; ++nodesIndex )
				{
					memcpy( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , nodes[ nodesIndex ].serial , SIZE_OF_SERIAL );
					OutgoingGeneralBufferPos += SIZE_OF_SERIAL;

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodes[ nodesIndex ].did & 0xFF00 ) >> 8 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = nodes[ nodesIndex ].did & 0xFF ;

					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( nodes[ nodesIndex ].pid & 0xFF00 ) >> 8 ;
					OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = nodes[ nodesIndex ].pid & 0xFF ;

				}

			}
			else
			{
				memset( &OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
				OutgoingGeneralBufferPos += 4 ;
			}


			free( nodes );
		}
		else
		{
			EXIT_PATTERN;
		}

	}
#else
	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_PLC_NETWORK_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_PLC_NETWORK_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_PLC_NETWORK_STATE & 0xFF ;

	memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
	OutgoingGeneralBufferPos+=4;

#endif
}

//
//
//

void MICRON_GetSyncStateWord()
{
	DEB_TRACE(DEB_IDX_MICRON)

	OutgoingGeneralBufferLength += 4 + 4 ;
	COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICE_SYNC_STATE & 0xFF000000) >> 24 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( MICRON_METR_DEVICE_SYNC_STATE & 0xFF0000 ) >> 16 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (MICRON_METR_DEVICE_SYNC_STATE & 0xFF00 ) >> 8 ;
	OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = MICRON_METR_DEVICE_SYNC_STATE & 0xFF ;

	syncStateWord_t * syncStateWord = NULL ;
	int syncStateWordSize = 0 ;
	if ( STORAGE_GetSyncStateWord(&syncStateWord , &syncStateWordSize ) == OK )
	{
		if ( syncStateWord )
		{
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWordSize & 0xFF000000) >> 24 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWordSize & 0xFF0000 ) >> 16 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (syncStateWordSize & 0xFF00 ) >> 8 ;
			OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = syncStateWordSize & 0xFF ;

			OutgoingGeneralBufferLength += syncStateWordSize * ( 4 + 4 + 4 + 1 ) ;
			COMMON_AllocateMemory( &OutgoingGeneralBuffer , OutgoingGeneralBufferLength );

			int counterIndex = 0 ;
			for( ; counterIndex < syncStateWordSize ; ++counterIndex )
			{
				COMMON_STR_DEBUG( DEBUG_MICRON_V1 "MICRON Copy syncStateWord for counter [%lu]" , syncStateWord[ counterIndex ].counterDbId );
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].counterDbId >> 24 ) & 0xFF ;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].counterDbId >> 16 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (syncStateWord[ counterIndex ].counterDbId >> 8 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = syncStateWord[ counterIndex ].counterDbId & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].worth >> 24 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].worth >> 16 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (syncStateWord[ counterIndex ].worth >> 8 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = syncStateWord[ counterIndex ].worth & 0xFF ;

				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].transactionTime >> 24 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = ( syncStateWord[ counterIndex ].transactionTime >> 16 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = (syncStateWord[ counterIndex ].transactionTime >> 8 ) & 0xFF;
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = syncStateWord[ counterIndex ].transactionTime & 0xFF ;

				//show only 6 bits
				OutgoingGeneralBuffer[ OutgoingGeneralBufferPos++ ] = syncStateWord[ counterIndex ].status & 0x3F ;
			}

			free( syncStateWord );
		}
		else
		{
			memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
			OutgoingGeneralBufferPos+=4;
		}
	}
	else
	{
		memset(&OutgoingGeneralBuffer[ OutgoingGeneralBufferPos ] , 0 , 4 );
		OutgoingGeneralBufferPos+=4;
	}

}

//
//
//

int MICRON_CheckMD5( unsigned char * hashControl , unsigned char * str , unsigned long strLength)
// the function calculte hash code from the *str with strLength. If hash code equal control hash then function returns OK result. Else ERROR_GENERAL
{
	DEB_TRACE(DEB_IDX_MICRON)

	int res = OK;

	//calculating hash
	unsigned char hash[16];
	memset(hash, 0 , 16 );

	md5_state_t md5handler;
	md5_init(&md5handler);
	md5_append( &md5handler , (const md5_byte_t *)str, strLength);
	md5_finish( &md5handler , hash );

	//checking

	if ( memcmp( hash , hashControl , 16 ) != 0 )
		res = ERROR_GENERAL ;

	return res;
}

//
//
//

int MICRON_Stuffing( unsigned char ** buffer , int * bufferLength )
{
	DEB_TRACE(DEB_IDX_MICRON)

	int stuffCounter = 0 ;
	int bufIndex = 0;

	// calc bytes to stuffing number

	for( ; bufIndex < (*bufferLength) ; ++bufIndex )
	{
		if ( ( (*buffer)[ bufIndex ] == 0x7E ) || ( (*buffer)[ bufIndex ] == 0x5D ) )
		{
			++stuffCounter ;
		}
	}

	//allocate memory
	if( stuffCounter > 0 )
	{
		int oldIndex = (*bufferLength) - 1 ;
		int newIndex = oldIndex + stuffCounter ;

		(*bufferLength) += stuffCounter ;


		(*buffer) = realloc( (*buffer) , (*bufferLength) );
		if( !(*buffer) )
		{
			printf("STUFFING ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}

		while( oldIndex >= 0 )
		{
			//printf("oldIndex = [%d] , newIndex = [%d] , stuffConter = [%d]. " , oldIndex , newIndex  , stuffCounter);
			if ( ( (*buffer)[ oldIndex ] == 0x7E ) || ( (*buffer)[ oldIndex ] == 0x5D ) )
			{
				//printf("making stuffing\n");
				(*buffer)[newIndex--] = (*buffer)[ oldIndex-- ] ^ 0x20  ;
				(*buffer)[newIndex--] = 0x5D ;

				if( (--stuffCounter) == 0 )
				{
					break;
				}

			}
			else
			{
				//printf("Simple copy\n");
				(*buffer)[ newIndex-- ] = (*buffer)[ oldIndex-- ] ;
			}
		}


	}

	return OK ;
}

//
//
//

int MICRON_Gniffuts( unsigned char ** buffer , int * bufferLength )
{
	DEB_TRACE(DEB_IDX_MICRON)

	if ( ( (*buffer) == NULL ) || ( bufferLength == NULL ) )
		return -1;

	int oldIndex = 0 ;
	int newIndex = 0 ;
	int stuffCounter = 0 ;

	while( oldIndex < (*bufferLength) )
	{
		if( (*buffer)[ oldIndex ] == 0x5D )
		{
			++oldIndex ;
			++stuffCounter;
			(*buffer)[ newIndex++ ] = (*buffer)[ oldIndex++ ] ^ 0x20 ;
		}
		else
		{
			(*buffer)[ newIndex++ ] = (*buffer)[ oldIndex++ ];
		}
	}

	if ( stuffCounter > 0 )
	{
		(*bufferLength) -= stuffCounter ;
		(*buffer) = realloc( (*buffer) , (*bufferLength) );
		if( (*buffer) == NULL)
		{
			printf("GNUFFITS ERROR: Can not allocate memory\n");
			EXIT_PATTERN;
		}

	}

	return OK ;
}

//
//
//
//
//

//*****************************************************************************
//*** 					Functions for calculating MD5 						***
//*****************************************************************************
#define T1 0xd76aa478
#define T2 0xe8c7b756
#define T3 0x242070db
#define T4 0xc1bdceee
#define T5 0xf57c0faf
#define T6 0x4787c62a
#define T7 0xa8304613
#define T8 0xfd469501
#define T9 0x698098d8
#define T10 0x8b44f7af
#define T11 0xffff5bb1
#define T12 0x895cd7be
#define T13 0x6b901122
#define T14 0xfd987193
#define T15 0xa679438e
#define T16 0x49b40821
#define T17 0xf61e2562
#define T18 0xc040b340
#define T19 0x265e5a51
#define T20 0xe9b6c7aa
#define T21 0xd62f105d
#define T22 0x02441453
#define T23 0xd8a1e681
#define T24 0xe7d3fbc8
#define T25 0x21e1cde6
#define T26 0xc33707d6
#define T27 0xf4d50d87
#define T28 0x455a14ed
#define T29 0xa9e3e905
#define T30 0xfcefa3f8
#define T31 0x676f02d9
#define T32 0x8d2a4c8a
#define T33 0xfffa3942
#define T34 0x8771f681
#define T35 0x6d9d6122
#define T36 0xfde5380c
#define T37 0xa4beea44
#define T38 0x4bdecfa9
#define T39 0xf6bb4b60
#define T40 0xbebfbc70
#define T41 0x289b7ec6
#define T42 0xeaa127fa
#define T43 0xd4ef3085
#define T44 0x04881d05
#define T45 0xd9d4d039
#define T46 0xe6db99e5
#define T47 0x1fa27cf8
#define T48 0xc4ac5665
#define T49 0xf4292244
#define T50 0x432aff97
#define T51 0xab9423a7
#define T52 0xfc93a039
#define T53 0x655b59c3
#define T54 0x8f0ccc92
#define T55 0xffeff47d
#define T56 0x85845dd1
#define T57 0x6fa87e4f
#define T58 0xfe2ce6e0
#define T59 0xa3014314
#define T60 0x4e0811a1
#define T61 0xf7537e82
#define T62 0xbd3af235
#define T63 0x2ad7d2bb
#define T64 0xeb86d391

static void
md5_process(md5_state_t *pms, const md5_byte_t *data /*[64]*/)
{
	DEB_TRACE(DEB_IDX_MICRON)

	md5_word_t
	a = pms->abcd[0], b = pms->abcd[1],
	c = pms->abcd[2], d = pms->abcd[3];
	md5_word_t t;

#ifndef ARCH_IS_BIG_ENDIAN
# define ARCH_IS_BIG_ENDIAN 1	/* slower, default implementation */
#endif
#if ARCH_IS_BIG_ENDIAN

	/*
	 * On big-endian machines, we must arrange the bytes in the right
	 * order.  (This also works on machines of unknown byte order.)
	 */
	md5_word_t X[16];
	const md5_byte_t *xp = data;
	int i;

	for (i = 0; i < 16; ++i, xp += 4)
	X[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);

#else  /* !ARCH_IS_BIG_ENDIAN */

	/*
	 * On little-endian machines, we can process properly aligned data
	 * without copying it.
	 */
	md5_word_t xbuf[16];
	const md5_word_t *X;

	if (!((data - (const md5_byte_t *)0) & 3)) {
	/* data are properly aligned */
	X = (const md5_word_t *)data;
	} else {
	/* not aligned */
	memcpy(xbuf, data, 64);
	X = xbuf;
	}
#endif

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

	/* Round 1. */
	/* Let [abcd k s i] denote the operation
	   a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
		/* Do the following 16 operations. */
		SET(a, b, c, d,  0,  7,  T1);
		SET(d, a, b, c,  1, 12,  T2);
		SET(c, d, a, b,  2, 17,  T3);
		SET(b, c, d, a,  3, 22,  T4);
		SET(a, b, c, d,  4,  7,  T5);
		SET(d, a, b, c,  5, 12,  T6);
		SET(c, d, a, b,  6, 17,  T7);
		SET(b, c, d, a,  7, 22,  T8);
		SET(a, b, c, d,  8,  7,  T9);
		SET(d, a, b, c,  9, 12, T10);
		SET(c, d, a, b, 10, 17, T11);
		SET(b, c, d, a, 11, 22, T12);
		SET(a, b, c, d, 12,  7, T13);
		SET(d, a, b, c, 13, 12, T14);
		SET(c, d, a, b, 14, 17, T15);
		SET(b, c, d, a, 15, 22, T16);
#undef SET

	 /* Round 2. */
	 /* Let [abcd k s i] denote the operation
		  a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
		 /* Do the following 16 operations. */
		SET(a, b, c, d,  1,  5, T17);
		SET(d, a, b, c,  6,  9, T18);
		SET(c, d, a, b, 11, 14, T19);
		SET(b, c, d, a,  0, 20, T20);
		SET(a, b, c, d,  5,  5, T21);
		SET(d, a, b, c, 10,  9, T22);
		SET(c, d, a, b, 15, 14, T23);
		SET(b, c, d, a,  4, 20, T24);
		SET(a, b, c, d,  9,  5, T25);
		SET(d, a, b, c, 14,  9, T26);
		SET(c, d, a, b,  3, 14, T27);
		SET(b, c, d, a,  8, 20, T28);
		SET(a, b, c, d, 13,  5, T29);
		SET(d, a, b, c,  2,  9, T30);
		SET(c, d, a, b,  7, 14, T31);
		SET(b, c, d, a, 12, 20, T32);
#undef SET

	 /* Round 3. */
	 /* Let [abcd k s t] denote the operation
		  a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
		 /* Do the following 16 operations. */
		SET(a, b, c, d,  5,  4, T33);
		SET(d, a, b, c,  8, 11, T34);
		SET(c, d, a, b, 11, 16, T35);
		SET(b, c, d, a, 14, 23, T36);
		SET(a, b, c, d,  1,  4, T37);
		SET(d, a, b, c,  4, 11, T38);
		SET(c, d, a, b,  7, 16, T39);
		SET(b, c, d, a, 10, 23, T40);
		SET(a, b, c, d, 13,  4, T41);
		SET(d, a, b, c,  0, 11, T42);
		SET(c, d, a, b,  3, 16, T43);
		SET(b, c, d, a,  6, 23, T44);
		SET(a, b, c, d,  9,  4, T45);
		SET(d, a, b, c, 12, 11, T46);
		SET(c, d, a, b, 15, 16, T47);
		SET(b, c, d, a,  2, 23, T48);
#undef SET

	 /* Round 4. */
	 /* Let [abcd k s t] denote the operation
		  a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
		 /* Do the following 16 operations. */
		SET(a, b, c, d,  0,  6, T49);
		SET(d, a, b, c,  7, 10, T50);
		SET(c, d, a, b, 14, 15, T51);
		SET(b, c, d, a,  5, 21, T52);
		SET(a, b, c, d, 12,  6, T53);
		SET(d, a, b, c,  3, 10, T54);
		SET(c, d, a, b, 10, 15, T55);
		SET(b, c, d, a,  1, 21, T56);
		SET(a, b, c, d,  8,  6, T57);
		SET(d, a, b, c, 15, 10, T58);
		SET(c, d, a, b,  6, 15, T59);
		SET(b, c, d, a, 13, 21, T60);
		SET(a, b, c, d,  4,  6, T61);
		SET(d, a, b, c, 11, 10, T62);
		SET(c, d, a, b,  2, 15, T63);
		SET(b, c, d, a,  9, 21, T64);
#undef SET

	 /* Then perform the following additions. (That is increment each
		of the four registers by the value it had before this block
		was started.) */
	pms->abcd[0] += a;
	pms->abcd[1] += b;
	pms->abcd[2] += c;
	pms->abcd[3] += d;
}

void
md5_init(md5_state_t *pms)
{
	DEB_TRACE(DEB_IDX_MICRON)

	pms->count[0] = pms->count[1] = 0;
	pms->abcd[0] = 0x67452301;
	pms->abcd[1] = 0xefcdab89;
	pms->abcd[2] = 0x98badcfe;
	pms->abcd[3] = 0x10325476;
}

void
md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes)
{
	DEB_TRACE(DEB_IDX_MICRON)

	const md5_byte_t *p = data;
	int left = nbytes;
	int offset = (pms->count[0] >> 3) & 63;
	md5_word_t nbits = (md5_word_t)(nbytes << 3);

	if (nbytes <= 0)
	return;

	/* Update the message length. */
	pms->count[1] += nbytes >> 29;
	pms->count[0] += nbits;
	if (pms->count[0] < nbits)
	pms->count[1]++;

	/* Process an initial partial block. */
	if (offset) {
	int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

	memcpy(pms->buf + offset, p, copy);
	if (offset + copy < 64)
		return;
	p += copy;
	left -= copy;
	md5_process(pms, pms->buf);
	}

	/* Process full blocks. */
	for (; left >= 64; p += 64, left -= 64)
	md5_process(pms, p);

	/* Process a final partial block. */
	if (left)
	memcpy(pms->buf, p, left);
}

void
md5_finish(md5_state_t *pms, md5_byte_t digest[16])
{
	DEB_TRACE(DEB_IDX_MICRON)

	static const md5_byte_t pad[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	md5_byte_t data[8];
	int i;

	/* Save the length before padding. */
	for (i = 0; i < 8; ++i)
	data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
	/* Pad to 56 bytes mod 64. */
	md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
	/* Append the length. */
	md5_append(pms, data, 8);
	for (i = 0; i < 16; ++i)
	digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}
//EOF
