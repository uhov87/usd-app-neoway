/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "common.h"
#include "debug.h"
#include "pio.h"
#include "storage.h"
#include "cli.h"
#include "gsm.h"
#include "eth.h"
#include "tic55_tokens.h"

static pthread_mutex_t lcd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lcd_cursors_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lcdErrorMutex = PTHREAD_MUTEX_INITIALIZER;

static int fd_button = -1;
static int inRound = 0;
static int menuRoundNumb = 0;
static int menuItemNumb = 0;
static int errorCodeFlag = 0;
static int currentErrorCode = 0;
static int showNextErrorCode = 0 ;


struct input_event {
	struct timeval time;
	unsigned short type;
	unsigned short code;
	unsigned int value;
};


//static int * ErrorCodeList = NULL;


int LCD_SetErrorCursor();
int LCD_RemoveErrorCursor();

// tokens constants
#define LCD_NO_TOKENS	NULL

// DOTS constants
#define LCD_DOT_NR		8
#define LCD_DOT_0		0b00000001
#define LCD_DOT_1		0b00000010
#define LCD_DOT_2		0b00000100
#define LCD_DOT_3		0b00001000
#define LCD_DOT_4		0b00010000
#define LCD_DOT_5		0b00100000
#define LCD_DOT_6		0b01000000
#define LCD_DOT_7		0b10000000
#define LCD_NO_DOTS		0b00000000
#define LCD_ALL_DOTS	0b11111111

// CURSORS constants
#define LCD_CURSOR_NR	LCD_DOT_NR
#define LCD_CURSOR_0	LCD_DOT_0
#define LCD_CURSOR_1	LCD_DOT_1
#define LCD_CURSOR_2	LCD_DOT_2
#define LCD_CURSOR_3	LCD_DOT_3
#define LCD_CURSOR_4	LCD_DOT_4
#define LCD_CURSOR_5	LCD_DOT_5
#define LCD_CURSOR_6	LCD_DOT_6
#define LCD_CURSOR_7	LCD_DOT_7
#define LCD_NO_CURSORS	LCD_NO_DOTS
#define LCD_ALL_CURSORS	LCD_ALL_DOTS

#define LCD_CHAR_NR		8

int lcd_all_clear();
int lcd_string_display( char* tokens );

int lcd_dots_display( unsigned char show_dots );
int lcd_dots_clear(unsigned char clear_dots);
int lcd_dots_all_clear();

int lcd_cursors_display( unsigned char show_cursors );
int lcd_cursors_clear( unsigned char clear_cursors );
int lcd_cursors_all_clear();

int lcd_init ();
static int lcd_setup_to_cli(char * displayBuf);
static int display_data_tic55 ();

unsigned char cursorsToSet = 0;
unsigned char cursorsToClear = 0;

// private constants
#define TIC55_TIMEOUT		500 // usec
#define SEG_NR 				72

// private data
static unsigned char SEG_bits[SEG_NR] = { 0 };
static unsigned char SEG_prev[SEG_NR] = { 0 };

//CLI
char cliErrors[CLI_ERRORS_SIZE] = { 0 };

//
//
//

int CLI_SetErrorCode(int ErrorCode, int setNotUnset)
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	if (( ErrorCode < 0) || (ErrorCode > CLI_ERRORS_SIZE))
		return res;

	pthread_mutex_lock(&lcdErrorMutex);

	switch(setNotUnset)
	{
		case CLI_CLEAR_ERROR:
		{
			cliErrors[ ErrorCode ] = 0;
			int error_flag = 0;
			int i;
			// Looking for presence of any other errorcodes
			for ( i=0 ; i < CLI_ERRORS_SIZE ; i++)
			{
				if (cliErrors[i] == 1)
				{
					error_flag = 1;
					break;
				}
			}
			if (error_flag == 0)
			{
				res = LCD_RemoveErrorCursor();
				errorCodeFlag = 0 ;
			}

		}
		break; //case 0

		case CLI_SET_ERROR:
		{
			res = LCD_SetErrorCursor();

			cliErrors[ ErrorCode ] = 1;
			errorCodeFlag = 1;
		}
		break; //case 1

		default:
			break;

	}

	pthread_mutex_unlock(&lcdErrorMutex);

	return res;
}


long timevaldiff(struct timeval *finishtime, struct timeval *starttime)
{
	DEB_TRACE(DEB_IDX_CLI)

	long msec;


	if (finishtime->tv_sec != starttime->tv_sec){
		msec=((finishtime->tv_sec-starttime->tv_sec)*1000) - 1000;

		msec+=(finishtime->tv_usec/1000);
		msec+=(((long)1000000-starttime->tv_usec)/1000);

	} else {

		msec = ((finishtime->tv_usec-starttime->tv_usec)/1000);
	}

	return msec;
}

//
//
//

int LCD_Protect()
{
	DEB_TRACE(DEB_IDX_CLI)

	pthread_mutex_lock(&lcd_mutex);
	return OK;
}

int LCD_Unprotect()
{
	DEB_TRACE(DEB_IDX_CLI)

	pthread_mutex_unlock(&lcd_mutex);
	return OK;
}

//
//
//

int LCD_CursorsProtect()
{
	DEB_TRACE(DEB_IDX_CLI)

	pthread_mutex_lock(&lcd_cursors_mutex);
	return OK;
}

int LCD_CursorsUnprotect()
{
	DEB_TRACE(DEB_IDX_CLI)

	pthread_mutex_unlock(&lcd_cursors_mutex);
	return OK;
}


//
//
//

void * LCD_mainMenu(void * args __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_CLI)

	time_t tDisplayTime;
	time_t tIdleTime;
	time_t tOffsetTime;

	const int bufLength = 64 ;

	int bufOffset = 0;
	char tempBuf[ bufLength ];
	char displayBuf[ bufLength ];

	time(&tIdleTime);

	COMMON_STR_DEBUG(DEBUG_CLI " CLI : cli thread started");

	while(1)
	{
		SVISOR_THREAD_SEND(THREAD_CLI);
		switch(CLI_GetButtonEvent())
		{
			case 1:
				menuRoundNumb++;
				menuItemNumb = 0;
				time(&tIdleTime);
				time(&tOffsetTime);
				bufOffset = 0;
				inRound = 1;
				break;

			case 2:
				menuItemNumb++;
				time(&tIdleTime);
				time(&tOffsetTime);
				bufOffset = 0;
				break;

			default:{
				int tDiff = tDisplayTime - tIdleTime;
				if ((tDiff > CLI_MENU_TIMEOUT) || (tDiff < 0))
				{
					menuRoundNumb = 0;
					menuItemNumb = 0;
					time(&tIdleTime);
					bufOffset = 0;
				}
				}
				break;

		}

		memset(tempBuf, 0, bufLength );
		memset(displayBuf , 0 , bufLength );

		switch(menuRoundNumb)
		{
			case CLI_IDLE:
			{
				LCD_CursorsProtect();
				cursorsToClear |= LCD_CURSOR_1 | LCD_CURSOR_0;
				LCD_CursorsUnprotect();
				//sprintf (tempBuf, "---");
				snprintf (tempBuf, bufLength , " |%%cron ");
				//runningString(tempBuf);
			}
			break;


			case ROUND_1:
			{
				//drawRoundNumber();

				switch(menuItemNumb)
				{
					case 0: //TIME
					{
						LCD_CursorsProtect();
						cursorsToSet |= LCD_CURSOR_1;
						LCD_CursorsUnprotect();

						time_t CurrentTime_t;
						struct tm * CurrentTime_tm;

						time( &CurrentTime_t );
						CurrentTime_tm = localtime( &CurrentTime_t );

						snprintf( tempBuf , bufLength , "%02i-%02i-%02i", CurrentTime_tm->tm_hour , CurrentTime_tm->tm_min , CurrentTime_tm->tm_sec);
					}
					break;

					case 1: //DATE
					{
						LCD_CursorsProtect();
						cursorsToSet |= LCD_CURSOR_0;
						cursorsToClear |= LCD_CURSOR_1;
						LCD_CursorsUnprotect();

						time_t CurrentTime_t;
						struct tm * CurrentTime_tm;

						time( &CurrentTime_t );
						CurrentTime_tm = localtime( &CurrentTime_t );

						snprintf( tempBuf , bufLength , "%02i-%02i-%02i", CurrentTime_tm->tm_mday , (CurrentTime_tm->tm_mon + 1 ) , (CurrentTime_tm->tm_year % 100));
					}
					break; //case 1

					case 2://SN USPD
					{
						LCD_CursorsProtect();
						cursorsToClear |= LCD_CURSOR_1 | LCD_CURSOR_0;
						LCD_CursorsUnprotect();

						unsigned char sn[bufLength];
						memset(sn, 0, bufLength);

						COMMON_GET_USPD_SN(sn);

						snprintf( tempBuf, bufLength ,"sn %s" , sn);

					}
					break; //case 2

					case 3://firmware version
					{
						char version[ STORAGE_VERSION_LENGTH ];
						memset( version , 0 , STORAGE_VERSION_LENGTH );
						if ( STORAGE_GetFirmwareVersion( (char *)version ) == OK )
						{
							int offset = 0 ;
							if ( strstr( version , "v" ) )
							{
								offset = 1 ;
							}
							snprintf( tempBuf, bufLength ,"fe %s" , version + offset );
//							int i = 0 ;
//							for( ; i < strlen(tempBuf) ; ++i )
//							{
//								if( tempBuf[i] == '.' )
//									tempBuf[i] = '_' ;
//							}
						}
						else
						{
							snprintf( tempBuf, bufLength ,"fe -.--");
						}
					}
					break;

					default: //RESET ROUNDS
					{
						menuItemNumb = 0;
						memset( tempBuf , 0, bufLength);
					}
					break; //default
				}

			}
			break; //ROUND_1



			case ROUND_2:
			{
				LCD_CursorsProtect();
				cursorsToClear = LCD_CURSOR_1 | LCD_CURSOR_0;
				LCD_CursorsUnprotect();

				//drawRoundNumber();

				switch(menuItemNumb)
				{
					case 0:  //RS counters
					{
						int rsCounters = STORAGE_CountOfCounters485();
						if ( rsCounters == 0 )
							snprintf( tempBuf, bufLength ,"RS  OFF");
						else if ( rsCounters > 0 )
							snprintf( tempBuf, bufLength ,"RS  %03d", rsCounters);
					}
					break; //case 0


					case 1:	//RF counters
					{

						int rfCounters = STORAGE_CountOfCountersPlc();
						if ( rfCounters == 0 )
							snprintf( tempBuf, bufLength ,"PLC OFF");
						else if ( rfCounters > 0 )
							snprintf( tempBuf, bufLength ,"PLC %03d", rfCounters);
					}
					break; //case 1


					default:
					{
						menuItemNumb = 0;
						memset( tempBuf , 0, bufLength);
					}
					break;


				}
			//	sprintf( tempBuf , "round 2");
			}
			break; //ROUND 2



			case ROUND_3:
			{
				//drawRoundNumber();

				switch(menuItemNumb)
				{
					case 0: // IMEI
					{
						//sprintf( tempBuf , "imei");
						char imei[16];
						memset(imei, 0 , 16);
						GSM_GetIMEI(imei , 16);
						snprintf(tempBuf , bufLength ,"id %s", imei);
					}
					break;

					case 1: //SIGNAL QUALITY
					{
						int quality = 0 ;
						time_t BeginTime;
						time( &BeginTime );

						quality = GSM_GetSignalQuality();
						if (quality < 0)
							//NOT READY
							snprintf( tempBuf , bufLength ,"SIG WAIt" );
						else if (quality == 0)
							//NO SIGNAL
							snprintf( tempBuf , bufLength ,"SIG   00");
						else if ( quality > 0 && quality < 33 )
							snprintf( tempBuf, bufLength ,"SIG _ %02d", quality);
						else if ( quality >= 33 && quality < 66 )
							snprintf( tempBuf, bufLength , "SIG = %02d", quality);
						else if ( quality >= 66)
						{
							if ( quality > 99 )
								quality = 99 ;
							snprintf( tempBuf, bufLength ,"SIG # %02d" , quality);
						}

					}
					break;

					case 2: // current link and it's status
					{
						//sprintf( tempBuf , "link status");

						connection_t connection;
						memset( &connection , 0 , sizeof(connection_t) );
						if (STORAGE_GetConnection(&connection) != OK )
						{
							snprintf( tempBuf, bufLength , "no param");
						}

						switch(connection.mode)
						{
							case CONNECTION_GPRS_SERVER:
							case CONNECTION_GPRS_ALWAYS:
							case CONNECTION_GPRS_CALENDAR:
							{
								if(GSM_ConnectionState() == 0)
									snprintf( tempBuf, bufLength ,"GPrS  on");
								else
									snprintf( tempBuf, bufLength ,"GPrS off");
							}
							break; //case CONNECTION_GPRS_CALENDAR

							case CONNECTION_CSD_OUTGOING:
							case CONNECTION_CSD_INCOMING:
							{
								if(GSM_ConnectionState() == 0)
									snprintf( tempBuf, bufLength ,"CSd   on");
								else
									snprintf( tempBuf, bufLength ,"CSd  off");
							}
							break; //case CONNECTION_CSD_INCOMING

							case CONNECTION_ETH_ALWAYS:
							case CONNECTION_ETH_CALENDAR:
							case CONNECTION_ETH_SERVER:
							{
								if ( ETH_ConnectionState(&connection) == OK )
								{
									snprintf( tempBuf, bufLength ,"Eth   on");
								}
								else
								{
									snprintf( tempBuf, bufLength ,"Eth  off");
								}
							}
							break; //case CONNECTION_ETH_CALENDAR

							default:
							{
								snprintf( tempBuf, bufLength ,"ErrOr connEctIon");
							}
							break;
						}
					}
					break;

					default:
					{
						menuItemNumb = 0;
					}
					break;

				}
			}
			break; //ROUND_3

			case ROUND_4:
			{
				//drawRoundNumber();

				switch(menuItemNumb)
				{
					case 0: // ETH STATUS
					{
						char cap[64];
						memset(cap, 0, 64);

						COMMON_GET_USPD_CABLE(cap);

						snprintf( tempBuf, bufLength ,"CABLE.%3s", cap);
					}
					break; //case 0

					case 1: //IP ADDRESS
					{
						int i;
						int l;
						char ip[64];
						memset(ip, 0, 64);

						COMMON_GET_USPD_IP(ip);

						i=0;
						l = strlen(ip);

//						for( i=0; i< l; i++){
//							if (ip[i] == '.') {ip[i]='_';}
//						}
						snprintf( tempBuf, bufLength ,"IP %s", ip);

					}
					break; //case 1

					case 2: //MAC ADRESS
					{
						int i;
						int l;
						char mac[64];
						memset(mac, 0, 64);

						COMMON_GET_USPD_MAC(mac);

						i=0;
						l = strlen(mac);

						for( i=0; i< l; i++){
							if (mac[i] == ':') {mac[i]='.';}
						}
						snprintf( tempBuf, bufLength ,"Hd %s", mac);
					}
					break; //case 2

					case 3: //NETMASK
					{
						int i;
						int l;
						char mask[64];
						memset(mask, 0, 64);

						COMMON_GET_USPD_MASK(mask);

						i=0;
						l = strlen(mask);

//						for( i=0; i< l; i++){
//							if (mask[i] == '.') {mask[i]='_';}
//						}
						snprintf( tempBuf, bufLength ,"Nt %s", mask);

					}
					break; //case 3

					case 4: //GATEWAY
					{
						int i;
						int l;
						char gw[64];
						memset(gw, 0, 64);

						COMMON_USPD_GET_GW(gw);

						i=0;
						l = strlen(gw);

//						for( i=0; i< l; i++){
//							if (gw[i] == '.') {gw[i]='_';}
//						}
						snprintf( tempBuf, bufLength ,"Gt %s", gw);

					}
					break; //case 4

					case 6:  // DNS
					{
						int i;
						int l;
						char dns[64];
						memset(dns, 0, 64);

						COMMON_USPD_GET_DNS(dns);

						i=0;
						l = strlen(dns);

//						for( i=0; i< l; i++){
//							if (dns[i] == '.') {dns[i]='_';}
//						}
						snprintf( tempBuf, bufLength ,"nS %s", dns);

					}
					break; //case 5

					default:
					{
						menuItemNumb = 0;
						memset( tempBuf , 0, bufLength);
					}
					break;
				}
			}
			break; //ROUND_4


			case ROUND_5: //ERROR_CODES
			{
				//drawRoundNumber();
				if(errorCodeFlag == 0) {
					snprintf (tempBuf, bufLength ,"no Error");

				} else {
					switch( menuItemNumb )
					{
						case 0:
						{
							int i;
							for( i=0 ; i < CLI_ERRORS_SIZE ; i++)
							{
								if( cliErrors[ i ] == 1 )
								{
									currentErrorCode = i;
									snprintf (tempBuf, bufLength ,"E h%02x", currentErrorCode);
									showNextErrorCode = 1;
									break; //for()
								}
							}
						}
						break; //case 0

						case 1:
						{
							if ( showNextErrorCode == 0 )
								snprintf (tempBuf, bufLength ,"E h%02x", currentErrorCode);
							if ( showNextErrorCode == 1 )
							{
								int i;
								int errorflag = 0;
								for( i=currentErrorCode + 1 ; i < CLI_ERRORS_SIZE ; i++)
								{
									if ( cliErrors[ i ] == 1)
									{
										errorflag = 1;
										showNextErrorCode = 0;
										currentErrorCode = i;
										snprintf (tempBuf, bufLength ,"E h%02x", currentErrorCode);
										break; //for()
									}
								}
								if (errorflag == 0)
									menuItemNumb = 0;
							}
						}
						break; //case 1

						default:
						{
							int i;
							int errorflag = 0 ;
							for( i = currentErrorCode + 1 ; i < CLI_ERRORS_SIZE ; i++)
							{
								if( cliErrors[i] == 1)
									errorflag = 1;
							}
							if (errorflag == 1)
							{
								menuItemNumb = 1;
								showNextErrorCode = 1;
							}
							else
								menuItemNumb = 0;
							memset( tempBuf , 0 , bufLength);
						}
					}
				}
			}
			break; //ROUND_5

			/*
			case ROUND_6:
			{
				//drawRoundNumber();
				sprintf (tempBuf, "round 6");
			}
			break; //ROUND_6
			*/

			default:
			{
				menuRoundNumb = 1;
			}
			break;
		}

		int tempBufLength = strlen(tempBuf) ;
		int dotNumber = 0 ;
		int i = 0 ;
		for ( ; i < tempBufLength ; ++i)
		{
			if ( tempBuf[ i ] == '.' )
				dotNumber++ ;
		}
		tempBufLength -= dotNumber ;


		if ( tempBufLength > 8)
		{
			int tDiff = tDisplayTime - tOffsetTime;
			if ((tDiff > 1) || (tDiff < 0))
			{
				bufOffset++;

				if ( tempBuf[bufOffset] == '.' )
					bufOffset++;

				if (bufOffset > (int)(strlen(tempBuf)-1)) {
					bufOffset = 0;
				}

				time( &tOffsetTime );
			}

		}

		snprintf( displayBuf, bufLength, "%s", &tempBuf[bufOffset]);

		if (inRound == 1){
			inRound = 0;
			char roundName[16];
			memset(roundName, 0, 16);
			snprintf (roundName, bufLength, "  [%02u]", menuRoundNumb);

			lcd_setup_to_cli(roundName);
			usleep(330000);
		}
		else
		{
			COMMON_Sleep(THREAD_SLEEPS_DELAY);
		}

		lcd_setup_to_cli(displayBuf);		

		time(&tDisplayTime);
	}

}


int CLI_Initialize()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	if ( CLI_InitButtons() != OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "fail CLI_InitButtons()");
		return res;
	}

	if (lcd_init() != OK)
	{
		COMMON_STR_DEBUG(DEBUG_CLI "fail CLI_InitLCD()");
		return res;
	}

	if ( lcd_all_clear() != OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "fail lcd_all_clear()");
		return res;
	}

	COMMON_STR_DEBUG(DEBUG_CLI " CLI : Init done, try start thread");

	pthread_t lcdMenu;
	pthread_create( &lcdMenu , NULL , &LCD_mainMenu , NULL);

	return OK;

}

int CLI_InitializeBadNotice()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;

	if (lcd_init() != OK)
	{
		COMMON_STR_DEBUG(DEBUG_CLI "fail CLI_InitLCD()");
		return res;
	}

	if ( lcd_all_clear() != OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "fail lcd_all_clear()");
		return res;
	}


	lcd_setup_to_cli("FWError");

	return OK;

}


int CLI_InitButtons()
{
	DEB_TRACE(DEB_IDX_CLI)

	fd_button = open ( CLI_EVENT_PATH, O_RDONLY | O_NONBLOCK );
	if (fd_button < 0)
		return ERROR_GENERAL;
	else
		return OK;
}

#define TRICK_DELAY (long)500

int CLI_GetButtonEvent()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;

	long msecdiff = 0;
	static int prevCode = -1;
	static struct timeval prevTime;

	struct input_event ev;

	int read_bytes = read( fd_button , &ev , sizeof(ev) );
	if ( read_bytes > 0 )
	{
		if (ev.code == 0)
			return 0;

		if (ev.value == 1)
		{
			int skipButton = 0;

			if(prevCode == ev.code) {
				msecdiff = timevaldiff(&ev.time, &prevTime);
				if(msecdiff < TRICK_DELAY){
					skipButton = 1;
					//return 0;
				}
			} else {
				msecdiff = 0;
			}

			COMMON_STR_DEBUG(DEBUG_CLI "KEY code: %u, msecdiff %u, skip %u\r", ev.code, msecdiff, skipButton);

			if (skipButton == 1){
				return 0;
			}

			prevCode = ev.code;
			memcpy(&prevTime, &ev.time, sizeof(struct timeval));

			switch(ev.code)
			{
				case CLI_BUTTON_0:
					res = 1;
					break;
				case CLI_BUTTON_1:
					res = 2;
					break;
			}
		}
	}
	else if (read_bytes == 0)
	{
		res = 0;
	}

	return res;
}


int LCD_SetActCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_display(LCD_CURSOR_5);
	return res;
}

int LCD_RemoveActCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_clear(LCD_CURSOR_5);
	return res;
}

int LCD_SetWorkCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_display(LCD_CURSOR_3);
	return res;
}

int LCD_RemoveWorkCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_clear(LCD_CURSOR_3);
	return res;
}

int LCD_SetIPOCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_display(LCD_CURSOR_6);
	return res;
}

int LCD_RemoveIPOCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_clear(LCD_CURSOR_6);
	return res;
}

int LCD_SetErrorCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_display(LCD_CURSOR_2);
	return res;
}

int LCD_RemoveErrorCursor()
{
	DEB_TRACE(DEB_IDX_CLI)

	int res = ERROR_GENERAL;
	res = lcd_cursors_clear(LCD_CURSOR_2);
	return res;
}

static int set_tic55_pin ( unsigned char gpio )
{
	DEB_TRACE(DEB_IDX_CLI)

	if ( PIO_SetValue( gpio, 1 ) != 0 )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "lcd:\terror:set_tic55_pin ( %03u )", gpio );
		return ERROR_GENERAL;
	}
	return OK;
}

static int clear_tic55_pin ( unsigned char gpio )
{
	DEB_TRACE(DEB_IDX_CLI)

	if ( PIO_SetValue( gpio, 0 ) != 0 )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "lcd:\terror: clear_tic55_pin ( %03u )", gpio );
		return ERROR_GENERAL;
	}
	return OK;
}


static int tokens_write_tic55( char * tokens )
{
	DEB_TRACE(DEB_IDX_CLI)

	unsigned int tokens_len = 0;
	int i, j;
	int do_i;

	if ( tokens == NULL )
	{
		//COMMON_STR_DEBUG(DEBUG_CLI "lcd: warning: display NULL string");
		return OK;
	}

	tokens_len = strlen(tokens);

	if (tokens_len == 0 ) {
		//COMMON_STR_DEBUG(DEBUG_CLI "lcd: warning: display empty string");
		return OK;
	}

	LCD_Protect();

	// clear only the text
	for ( i = 0; i < 8; i++ )
	{
		for (j = 0; j < 7; j++){
			SEG_bits[(i*9) + j] = 0;
		}
	}

	if( tokens_len > 8 ) {
		//COMMON_STR_DEBUG(DEBUG_CLI "lcd: warning: string more then 8 characters [ %s ]", tokens);
		do_i = 8;
	} else {
		do_i = tokens_len;
	}

	unsigned int token_offset = 0;

	for( i = 0; i < do_i; i++ )
	{		
		unsigned char tmp;

		switch( tokens[i] )
		{
#if 0
			case 'A': tmp = token_A ; break;
			case 'a': tmp = token_a ; break;

			case 'B': tmp = token_B ; break;
			case 'b': tmp = token_b ; break;

			case 'C': tmp = token_C ; break;
			case 'c': tmp = token_c ; break;

			case 'D': tmp = token_D ; break;
			case 'd': tmp = token_d ; break;

			case 'E': tmp = token_E ; break;
			case 'e': tmp = token_e ; break;

			case 'F': tmp = token_F ; break;
			case 'f': tmp = token_f ; break;

			case 'G': tmp = token_G ; break;
			case 'g': tmp = token_g ; break;

			case 'H': tmp = token_H ; break;
			case 'h': tmp = token_h ; break;

			case 'I': tmp = token_I ; break;
			case 'i': tmp = token_i ; break;

			case 'J': tmp = token_J ; break;
			case 'j': tmp = token_j ; break;

			case 'K': tmp = token_K ; break;
			case 'k': tmp = token_k ; break;

			case 'L': tmp = token_L ; break;
			case 'l': tmp = token_l ; break;

			case 'M': tmp = token_M ; break;
			case 'm': tmp = token_m ; break;

			case 'N': tmp = token_N ; break;
			case 'n': tmp = token_n ; break;

			case 'O': tmp = token_O ; break;
			case 'o': tmp = token_o ; break;

			case 'P': tmp = token_P ; break;
			case 'p': tmp = token_p ; break;

			case 'Q': tmp = token_Q ; break;
			case 'q': tmp = token_q ; break;

			case 'R': tmp = token_R ; break;
			case 'r': tmp = token_r ; break;

			case 'S': tmp = token_S ; break;
			case 's': tmp = token_s ; break;

			case 'T': tmp = token_T ; break;
			case 't': tmp = token_t ; break;

			case 'U': tmp = token_U ; break;
			case 'u': tmp = token_u ; break;

			case 'V': tmp = token_V ; break;
			case 'v': tmp = token_v ; break;

			case 'W': tmp = token_W ; break;
			case 'w': tmp = token_w ; break;

			case 'X': tmp = token_X ; break;
			case 'x': tmp = token_x ; break;

			case 'Y': tmp = token_Y ; break;
			case 'y': tmp = token_y ; break;

			case 'Z': tmp = token_Z ; break;
			case 'z': tmp = token_z ; break;


			case '0': tmp = token_0 ; break;
			case '1': tmp = token_1 ; break;
			case '2': tmp = token_2 ; break;
			case '3': tmp = token_3 ; break;
			case '4': tmp = token_4 ; break;
			case '5': tmp = token_5 ; break;
			case '6': tmp = token_6 ; break;
			case '7': tmp = token_7 ; break;
			case '8': tmp = token_8 ; break;
			case '9': tmp = token_9 ; break;

			case '-': tmp = token_dash ; break;
			case ' ': tmp = token_space ; break;

			case '%': tmp = token_zju; break;

			case ':': tmp = token_colon; break;
			case '_': tmp = token_gsm_33; break;
			case '=': tmp = token_gsm_66; break;
			case '#': tmp = token_gsm_100; break;

			case '[': tmp = token_open_bracket; break;
			case ']': tmp = token_close_bracket; break;
#endif
			case 'A':
			case 'a': tmp = token_A ; break;

			case 'B':
			case 'b': tmp = token_b ; break;

			case 'C': tmp = token_C ; break;
			case 'c': tmp = token_c ; break;

			case 'D':
			case 'd': tmp = token_d ; break;

			case 'E':
			case 'e': tmp = token_E ; break;

			case 'F':
			case 'f': tmp = token_F ; break;

			case 'G':
			case 'g': tmp = token_G ; break;

			case 'H':
			case 'h': tmp = token_H ; break;

			case 'I':
			case 'i': tmp = token_i ; break;

			case 'J':
			case 'j': tmp = token_J ; break;

			case 'K':
			case 'k': tmp = token_K ; break;

			case 'L':
			case 'l': tmp = token_L ; break;

			case 'M':
			case 'm': tmp = token_M ; break;

			case 'N':
			case 'n': tmp = token_n ; break;

			case 'O':
			case 'o': tmp = token_o ; break;

			case 'P':
			case 'p': tmp = token_P ; break;

			case 'Q':
			case 'q': tmp = token_q ; break;

			case 'R':
			case 'r': tmp = token_r ; break;

			case 'S':
			case 's': tmp = token_S ; break;

			case 'T':
			case 't': tmp = token_t ; break;

			case 'U':
			case 'u': tmp = token_U ; break;

			case 'V':
			case 'v': tmp = token_v ; break;

			case 'W':
			case 'w': tmp = token_W ; break;

			case 'X':
			case 'x': tmp = token_x ; break;

			case 'Y':
			case 'y': tmp = token_y ; break;

			case 'Z':
			case 'z': tmp = token_z ; break;


			case '0': tmp = token_0 ; break;
			case '1': tmp = token_1 ; break;
			case '2': tmp = token_2 ; break;
			case '3': tmp = token_3 ; break;
			case '4': tmp = token_4 ; break;
			case '5': tmp = token_5 ; break;
			case '6': tmp = token_6 ; break;
			case '7': tmp = token_7 ; break;
			case '8': tmp = token_8 ; break;
			case '9': tmp = token_9 ; break;

			case '-': tmp = token_dash ; break;
			case ' ': tmp = token_space ; break;

			case '%': tmp = token_zju; break;
			case '|': tmp = token_N; break;



			case ':': tmp = token_colon; break;
			case '_': tmp = token_gsm_33; break;
			case '=': tmp = token_gsm_66; break;
			case '#': tmp = token_gsm_100; break;

			case '[': tmp = token_open_bracket; break;
			case ']': tmp = token_close_bracket; break;


			default:
				COMMON_STR_DEBUG(DEBUG_CLI "lcd: warning: unsupported tokens [ %c hex: %x]",tokens[i], tokens[i] );
				tmp = token_NULL ;
		}

		for( j = 0; j < 7; j++)
		{
			SEG_bits[ token_offset + j ] = ( tmp & 0x01 );
			tmp >>= 1;
		}

		token_offset += 9;
	}

	LCD_Unprotect();

	return OK;
}

static int lcd_setup_to_cli(char * displayBuf){

	DEB_TRACE(DEB_IDX_CLI)

	int digit = 0;

	LCD_CursorsProtect();

	for (digit = 0; digit < 8; digit++){
		if (cursorsToClear & (1 << digit)){
			SEG_bits[(9 * digit) + 7 ] = 0;
		}
	}
	cursorsToClear = 0;

	for (digit = 0; digit < 8; digit++){
		if (cursorsToSet & (1 << digit)){
			SEG_bits[(9 * digit) + 7 ] = 1;
		}
	}
	cursorsToSet = 0;

#if 1
	int length = strlen(displayBuf) ;
	unsigned char dots = 0 ;
	int i = 0 ;
	for ( ; i < (int)strlen( displayBuf ) ; ++i )
	{
		if ( displayBuf[ i ] == '.' )
		{
			memcpy( &displayBuf[i] , &displayBuf[ i + 1 ] , length - i - 1 );
			if ( ( i > 0 ) && ( i < 8))
			{
				dots |= 1 << (i - 1) ;
			}
			displayBuf[ --length ] = 0 ;
		}
	}
	//setting dots
	for (digit = 0; digit < 8; digit++){
		SEG_bits[(9 * digit) + 8 ] = 0;
		if (dots & (1 << digit)){
			SEG_bits[(9 * digit) + 8 ] = 1;
		}
	}
#endif

	LCD_CursorsUnprotect();

	if ( tokens_write_tic55( displayBuf ) !=  OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "lcd:\terror: tokens_write_tic55()");
		return ERROR_GENERAL;
	}

	return display_data_tic55();
}

static int display_data_tic55( )
{
	DEB_TRACE(DEB_IDX_CLI)

	int i;
	int needUpdate = 0;

	LCD_Protect();

	//check we need to handle PIO
	for (i = 0; i < SEG_NR; i++){
		if (SEG_bits[ i ] != SEG_prev[ i ]){
			SEG_prev[ i ] = SEG_bits[ i ];
			needUpdate = 1;
		}
	}

	//quit if CLI content the same as previous
	if (needUpdate == 0){
		LCD_Unprotect();
		return OK;
	}

	//update CLI content
	if ( clear_tic55_pin( PIO_CLI_DIN ) != OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "LCD: clear_tic55_pin ERROR");
		LCD_Unprotect();
		return ERROR_PIO;
	}

	for ( i = 0 ; i < 8; i++ )
	{
		if ( set_tic55_pin( PIO_CLI_DCLK  ) != OK )
		{
			COMMON_STR_DEBUG(DEBUG_CLI "LCD: set_tic55_pin ERROR");
			LCD_Unprotect();
			return ERROR_PIO;
		}


		if ( clear_tic55_pin( PIO_CLI_DCLK  ) != OK )
		{
			COMMON_STR_DEBUG(DEBUG_CLI "LCD: clear_tic55_pin ERROR");
			LCD_Unprotect();
			return ERROR_PIO;
		}
	}

	for ( i = 71 ; i >= 0 ; i-- )
	{
		if ( SEG_bits[ i ] == 1 )
		{
			if ( set_tic55_pin( PIO_CLI_DIN ) != OK )
			{
				LCD_Unprotect();
				return ERROR_PIO;
			}
		}

		if ( clear_tic55_pin( PIO_CLI_DCLK  ) != OK )
		{
			LCD_Unprotect();
			return ERROR_PIO;
		}

		if ( set_tic55_pin( PIO_CLI_DCLK  ) != OK )
		{
			LCD_Unprotect();
			return ERROR_PIO;
		}

		if ( SEG_bits[ i ] == 1 )
		{
			if ( clear_tic55_pin( PIO_CLI_DIN ) != OK )
			{
				LCD_Unprotect();
				return ERROR_PIO;
			}
		}
	}

	// displey loaded segments on tic55
	// set tic55 LOAD pin for display loaded segments
	// clear PIO_CLI_DCLK , PIO_CLI_LOAD, PIO_CLI_DIN pins

	if ( set_tic55_pin( PIO_CLI_LOAD ) != OK )
	{
		LCD_Unprotect();
		return ERROR_GENERAL;
	}

	if ( clear_tic55_pin( PIO_CLI_LOAD ) != OK )
	{
		LCD_Unprotect();
		return ERROR_GENERAL;
	}

	LCD_Unprotect();

	return OK;
}


int lcd_init ( )
{
	DEB_TRACE(DEB_IDX_CLI)

	if ( clear_tic55_pin( PIO_CLI_DIN ) != OK )
	{
		return ERROR_PIO;
	}

	return OK;
}

int lcd_string_display( char* tokens )
{
	DEB_TRACE(DEB_IDX_CLI)

	if ( tokens_write_tic55( tokens ) !=  OK )
	{
		COMMON_STR_DEBUG(DEBUG_CLI "lcd:\terror: tokens_write_tic55()");
		return ERROR_GENERAL;
	}

	return display_data_tic55();
}

int lcd_all_clear()
{
	DEB_TRACE(DEB_IDX_CLI)

	LCD_Protect();

	COMMON_STR_DEBUG(DEBUG_CLI "LCD: All clear");
	memset( SEG_bits, 0, sizeof ( SEG_bits ) );

	LCD_Unprotect();

	return display_data_tic55();
}

int lcd_dots_display( unsigned char show_dots )
{
	DEB_TRACE(DEB_IDX_CLI)

	int digit = 0;

	LCD_Protect();

	for (digit = 0; digit < 8; digit++){
		if (show_dots & (1 << digit)){
			SEG_bits[(9 * digit) + 8 ] = 1;
		}
	}

	LCD_Unprotect();

	return display_data_tic55();
}

int lcd_dots_clear(unsigned char clear_dots)
{
	DEB_TRACE(DEB_IDX_CLI)

	int digit = 0;

	LCD_Protect();

	for (digit = 0; digit < 8; digit++){
		if (clear_dots & (1 << digit)){
			SEG_bits[(9 * digit) + 8 ] = 0;
		}
	}

	LCD_Unprotect();

	return display_data_tic55();
}

int lcd_dots_all_clear()
{
	DEB_TRACE(DEB_IDX_CLI)

	int digit = 0;

	LCD_Protect();

	for (digit = 0; digit < 8; digit++){
		SEG_bits[(9 * digit) + 8 ] = 0;
	}

	LCD_Unprotect();

	return display_data_tic55();
}



int lcd_cursors_display( unsigned char show_cursors )
{
	DEB_TRACE(DEB_IDX_CLI)

	LCD_CursorsProtect();

	cursorsToSet |= show_cursors;

	LCD_CursorsUnprotect();

	return OK;
}

int lcd_cursors_clear( unsigned char clear_cursors )
{
	DEB_TRACE(DEB_IDX_CLI)


	LCD_CursorsProtect();

	cursorsToClear |= clear_cursors;

	LCD_CursorsUnprotect();

	return OK;
}

int lcd_cursors_all_clear()
{
	DEB_TRACE(DEB_IDX_CLI)

	LCD_Protect();

	int digit = 0;

	for (digit = 0; digit < 8; digit++){
		SEG_bits[(9 * digit) + 7 ] = 0;
	}

	LCD_Unprotect();

	return display_data_tic55();
}

//EOF
