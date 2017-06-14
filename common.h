/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_COMMON_H
#define __ZIF_COMMON_H

//global includes
#include <time.h>
#include <termios.h>
#include <stdarg.h>
#include "boolType.h"
#include "debug.h"
#include "release_flag.h"

#ifdef __cplusplus
extern "C" {
#endif

//to restore pakets of mayak with wery bad answer due to fw meter release version 8
#define WERY_BAD_MAYK_ANSWER 0
#define MAYAK_KALUJNIY_RECCOMEND 1

//select preferred service for transparent mode
#define PLC_TRANSPARENT_MODE_USE_SHORT_INTERNETWORKING_SERVICE 1

//exported definitions
#define DTS_PATTERN "%02hhu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu"
#define DTS_GET_BY_PTR(___D___) (___D___)->d.d, (___D___)->d.m, (___D___)->d.y, (___D___)->t.h, (___D___)->t.m, (___D___)->t.s
#define DTS_GET_BY_VAL(___D___) (___D___).d.d,  (___D___).d.m,  (___D___).d.y,  (___D___).t.h,  (___D___).t.m,  (___D___).t.s

#define EXIT_PATTERN fprintf( stderr , "%s exit\n" , __FUNCTION__ ); exit(1);

#define H_PATTERN "%d h"
#define M_PATTERN "%d min"
#define S_PATTERN "%d sec"
#define HM_PATTERN "%d h %d min"
#define HMS_PATTERN "%d h %d min %d sec"

#define H_FROM_S(___S___) (___S___/60/60)
#define M_FROM_S(___S___) (___S___/60)
#define HM_FROM_S(___S___) (___S___/60/60), ((___S___/60)%60)
#define HMS_FROM_S(___S___) (___S___/60/60), ((___S___/60)%60), (___S___%60)

#define DEF_TO_XSTR(__A__) DEF_TO_STR(__A__)
#define DEF_TO_STR(__A__) #__A__

// -------------------------------------------
// embodiment part inclusion
// -------------------------------------------

#ifndef REV_COMPILE_RELEASE
#define REV_COMPILE_RELEASE						0 // 0 - for debug , 1 - for release(disable debug output)
#endif

#define REV_COMPILE_EXIO						0 //extension input-output board compilation
#define REV_COMPILE_PLC							1 //PLC_pim compilation
#define REV_COMPILE_485							1 //485 compilation
#define REV_COMPILE_RF							0 //RF_pim compilation

#define REV_COMPILE_PIO_MMAP					0 // 1 - over mmap , 0 - over sysfs
#define REV_COMPILE_FULL_QUIZ_MASK				0 // if 1 - tariff and energies mask are always 0x0F

#define REV_COMPILE_EVENT_METER_FAIL			0 //use journal event 0x10 or not
#define REV_COMPILE_ASK_MAYK_FRM_VER			1

#define REV_COMPILE_STASH_NZIF_IP				1

#define MICRON_PROTO_VER						(unsigned int)0x001d  //version is v00.29

#define MAX_DEVICE_COUNT_TOTAL					350

//#define MAX_DEVICE_COUNT_PLC					350
//#define MAX_DEVICE_COUNT_485					100

// -------------------------------------------
// CLI part inclusion
// -------------------------------------------

#define CLI_EVENT_PATH "/dev/input/event0"
#define CLI_BUTTON_0	66
#define CLI_BUTTON_1	65
#define CLI_MENU_TIMEOUT 30 //seconds
#define CLI_IDLE 0
#define ROUND_1 1
#define ROUND_2 2
#define ROUND_3 3
#define ROUND_4 4
#define ROUND_5 5
#define ROUND_6 6


#define CLI_SET_ERROR 1
#define CLI_CLEAR_ERROR 0

//errros on cli
#define CLI_NO_COUNTERS						0
#define CLI_NO_PLC_MODEMS					3
#define CLI_NO_TIME_SYNCED					4
#define CLI_ERROR_SERIAL_TR                 0xA1
#define CLI_ERROR_CONN_TR                   0xA2
#define CLI_ERROR_POOL_PLC_TR               0xA3
#define CLI_ERROR_POOL_485_TR               0xA4
#define CLI_NO_CONFIG_PLC                   0xC1
#define CLI_NO_CONFIG_NETPLC                0xC2
#define CLI_NO_CONFIG_RS                    0xC3
#define CLI_NO_CONFIG_CONN                  0xC4
#define CLI_NO_CONFIG_ETH                   0xC5
#define CLI_NO_CONFIG_GSM                   0xC6
#define CLI_NO_CONFIG_MAC                   0xC7
#define CLI_NO_CONFIG_SERIAL                0xC8
#define CLI_INVALID_CONNECTION_PARAMAS      0xCF
#define CLI_ERROR_INIT_GSM_MODULE           0xD1
#define CLI_ERROR_INIT_PIM                  0xD2
#define CLI_NO_SIM_REG	                    0x51
#define CLI_NO_CONNECTION_BAD               0xF1

#define CLI_ERRORS_SIZE                     0x0100

// -------------------------------------------
// COMMON part inclusion
// -------------------------------------------
//debug options
#define ZERO 0
#define HALF 30
#define READY 0xEC
	
#define SHELL_CMD_REBOOT system ("reboot")
#define DEBUG_INCLUDE_TIME_TO_OUTPUT 1

#define DEBUG_BUFFER_SIZE       8192
#define DEBUG_TO_CONSOLE        "Y"
#define DEBUG_TO_FILE           "N"

//debug levels
#define DEBUG_ALL_LEVELS        "N"
#define DEBUG_CLI               "N"
#define DEBUG_MAIN              "N"
#define DEBUG_COMMON            "N"
#define DEBUG_CONNECTION        "N"
#define DEBUG_UJUK              "N"
#define DEBUG_GUBANOV           "N"
#define DEBUG_MAYK              "N"
#define DEBUG_UNKNOWN           "N"
#define DEBUG_ETH               "N"
#define DEBUG_TASK_GSM          "N"
#define DEBUG_GSM               "N"
#define DEBUG_MICRON_V1         "N"
#define DEBUG_PIO               "N"
#define DEBUG_POOL_485          "N"
#define DEBUG_POOL_PLC          "N"
#define DEBUG_PLC               "N"
#define DEBUG_SHAMANING_PLC     "N"
#define DEBUG_IT700_APPROVE     "N"
#define DEBUG_IT700_PROTO       "N"
#define DEBUG_SERIAL            "N"
#define DEBUG_STORAGE_ALL       "N"
#define DEBUG_STORAGE_PROFILE   "N"
#define DEBUG_STORAGE_485       "N"
#define DEBUG_STORAGE_PLC       "N"
#define DEBUG_STORAGE_WEB       "N"
#define DEBUG_ACOUNTS           "N"
#define DEBUG_EXIO              "N"
#define DEBUG_NEWPROFILE        "N"

//time definitions
#define SECOND 1000
#define MINUTE (60 * SECOND)
#define HOUR   (60 * MINUTE)

//timeouts
#define TIMEOUT_READING_ON_RS485 2   //in seconds
#define TIMEOUT_READING_ON_PLC   20  //in seconds
#define TIMEOUT_LEAVE_RF_NET     190 //in seconds
#define TIMEOUT_TRANSPARENT_MODE 300 //in seconds // 5 minutes
#define TIMEOUT_TRANSPARENT_TIME 20  //in seconds
#define TIMEOUT_TRANSPARENT_OPEN 23  //in seconds
#define TIMEOUT_TRANSPARENT_RS   1   //in seconds
#define TIMEOUT_EMPTY_PLC_POLLS  (12*60*60) // 12 hours
#define TIMEOUT_BETWEENS_LEAVES  (20*60) // 20 minutes

//counter type definitions
#define TYPE_COUNTER_UNKNOWN	 	255 //0xFF - is unknonw type of counter
#define TYPE_COUNTER_REPEATER       254 //for plc-repeater

//ujuk
#define TYPE_UJUK_SEO_1_16				25 //for SEO-1-16

#define TYPE_UJUK_SEB_1TM_01A			17 //for SEB-1TM-01
#define TYPE_UJUK_SEB_1TM_02A			18 //for SEB-1TM-02
#define TYPE_UJUK_SEB_1TM_02M			34 //for SEB-1TM-02 M
#define TYPE_UJUK_SEB_1TM_02D			29 //for SEB-1TM-02 D
#define TYPE_UJUK_SEB_1TM_03A			19 //for SEB-1TM-03

#define TYPE_UJUK_SET_1M_01			33 //for SET-1M-01

#define TYPE_UJUK_SET_4TM_01A			20 //for SET-4TM-01
#define TYPE_UJUK_SET_4TM_02A			21 //for SET-4TM-02 (SET-1TM-01)
#define TYPE_UJUK_SET_4TM_02MB			28 //for SET-4TM-02 M
#define TYPE_UJUK_SET_4TM_03A			22 //for SET-4TM-03
#define TYPE_UJUK_SET_4TM_03MB			35 //for SET-4TM-03 M

#define TYPE_UJUK_PSH_3TM_05A			23 //for PSH-3TM-05
#define TYPE_UJUK_PSH_3TM_05MB			27 //for PSH-3TM-05 M
#define TYPE_UJUK_PSH_3TM_05D			30 //for PSH-3TM-05 D

#define TYPE_UJUK_PSH_4TM_05A			24 //for PSH-4TM-05
#define TYPE_UJUK_PSH_4TM_05MB			26 //for PSH-4TM-05 M
#define TYPE_UJUK_PSH_4TM_05D			31 //for PSH-4TM-05 D
#define TYPE_UJUK_PSH_4TM_05MK			32 //for PSH-4TM-05 M K
#define TYPE_UJUK_PSH_4TM_05MD			36 //for PSH-3TM-05 M D

//gubanov
#define TYPE_GUBANOV_SEB_2A05           103 //for SEB-2A-05
#define TYPE_GUBANOV_SEB_2A07           104 //for SEB-2A-07
#define TYPE_GUBANOV_SEB_2A08           105 //for SEB-2A-08

#define TYPE_GUBANOV_PSH_3TA07A         102 //for PSH-3TA-07
#define TYPE_GUBANOV_PSH_3TA07_DOT_1    100 //for PSH-3TA-07.1
#define TYPE_GUBANOV_PSH_3TA07_DOT_2    101 //for PSH-3TA-07.2

#define TYPE_GUBANOV_PSH_3ART           106 //for PSH-3ART
#define TYPE_GUBANOV_PSH_3ART_1         107 //for PSH-3ART....-01
#define TYPE_GUBANOV_PSH_3ART_D         108 //for PSH-3ART....-02 , 03 , 04

#define TYPE_GUBANOV_MAYK_101           200 //for MAYK-101 (SEB-2A-08 no modem)
#define TYPE_GUBANOV_MAYK_102           202 //for MAYK-102 (SEB-2A-08 with modem)
#define TYPE_GUBANOV_MAYK_301           230 //for MAYK-302 (PSH-3ART)

//mayak
#define TYPE_MAYK_MAYK_101              203 //for MAYK-101-ARTD
#define TYPE_MAYK_MAYK_103              201 //for MAYK-103 any
#define TYPE_MAYK_MAYK_302              231 //for MAYK-302 any

//errors definitions
#define OK 0
#define ERROR_GENERAL -1
#define ERROR_PLC -2
#define ERROR_REPEATER -222
#define ERROR_SERIAL_GLOBAL -3
#define ERROR_SERIAL_IOCTL -30
#define ERROR_SERIAL_OVERLAPPED -31
#define ERROR_STORAGE -4
#define ERROR_CONNECTION -5
#define ERROR_NO_CONNECTION_PARAMS -51
#define ERROR_NO_COUNTERS -6
#define ERROR_NO_DATA_YET -61
#define ERROR_NO_DATA_FOUND -62
#define ERROR_NO_ANSWER - 63
#define ERROR_POOL_INIT -7
#define ERROR_POOL_PLC_SHAMANING -71
#define ERROR_TIME_IS_OUT -8
#define ERROR_SIZE_WRITE -9
#define ERROR_SIZE_READ -10
#define ERROR_IS_NOT_FOUND -11
#define ERROR_MEM_ERROR -12
#define ERROR_FILE_OPEN_ERROR -13
#define ERROR_CONFIG_FORMAT_ERROR -14
#define ERROR_ARRAY_INDEX -15
#define ERROR_COUNTER_NOT_OPEN -16
#define ERROR_PIO -17
#define ERROR_TRANSPARENT_INTERRUPT -18
#define ERROR_NEED_REPEAT -99
#define ERROR_MAX_LEN_REACHED -19
#define ERROR_FILE_IS_EMPTY -20
#define ERROR_FILE_FORMAT -201
#define ERROR_COUNTER_TYPE -202
#define ERROR_SYNC_IS_IMPOSSIBLE -203

#define THREAD_SLEEPS_DELAY 5
#define DELAY_PLC_INIT SECOND // 1s
#define DELAY_PLC_ANSWER (SECOND/100) //10 ms
#define DELAY_POOL_INIT SECOND // 1s
#define DELAY_CONNECTION_INIT SECOND // 1s
#define DELAY_CONNECTION_WAIT SECOND // 1s
#define DELAY_BEETWEN_CHECK_TASK_STATE (SECOND/10) // 100ms
#define DELAY_BEETWEN_SERIAL_READ_WRITE 15 // 15 ms


#define MAX_PLC_INIT_TRIES 10
#define MAX_PLC_REINIT_COUNT 30
#define MAX_PLC_ERROR_COUNT 20
#define MAX_COUNT_PLC_INIT_WAIT 40 // 40 seconds by 1 s
#define MAX_COUNT_EXIO_INIT_WAIT 40 // 40 seconds by 1 s
#define MAX_COUNT_POOL_INIT_WAIT 20 // 20 seconds by 1 s
#define MAX_COUNT_CONNECTION_INIT_WAIT 120 // 120 seconds by 1 s
#define MAX_COUNT_PLC_ANSWER_WAIT 10000 // 10 seconds by 10 ms

#define MAX_POLL_DELAY_FOR_UJUK (60*60*23) //in seconds (23 hours)

#define IP_SIZE 16
#define LINK_SIZE 128
#define FIELD_SIZE 64
#define PASSWORD_SIZE 6

//task results
#define TRANSACTION_NO_RESULT (unsigned char)0x0
#define TRANSACTION_DONE_OK (unsigned char)0xFD
#define TRANSACTION_DONE_ERROR_A (unsigned char)0xFC
#define TRANSACTION_DONE_ERROR_SIZE (unsigned char)0xFE
#define TRANSACTION_DONE_ERROR_TIMEOUT (unsigned char)0xFB
#define TRANSACTION_DONE_ERROR_NO_ROUTE (unsigned char)0xFA
#define TRANSACTION_DONE_ERROR_RESPONSE (unsigned char)0xF9
#define TRANSACTION_DONE_ERROR_ADDRESS (unsigned char)0xF8
#define TRANSACTION_DONE_ERROR_CRC (unsigned char)0xF7

//task results for statistic
#define TRANSACTION_COUNTER_NOT_OPEN (unsigned char)0xE0
#define TRANSACTION_COUNTER_POOL_DONE (unsigned char)0xA0

//pool status for statistic
#define STATISTIC_IN_POOL (unsigned char)0x1
#define STATISTIC_NOT_IN_POOL (unsigned char)0x02
#define STATISTIC_NOT_IN_POOL_YET (unsigned char)0x03
#define STATISTIC_NOT_IN_POOL_ERROR (unsigned char)0x04

//operations for date time stamps
#define INC 1
#define DEC 2

#define CURRENT_DTS 0
#define DEFINED_DTS 1

#define BY_SEC 1
#define BY_MIN 2
#define BY_HOUR 3
#define BY_HALF_HOUR 31
#define BY_DAY 4
#define BY_MONTH 5
#define BY_YEAR 6

//for days
#define ASK_EMPTY (unsigned char)0xFF
#define ASK_DAY_TODAY (unsigned char)0x0
#define ASK_DAY_ARCHIVE (unsigned char)0x2
#define ASK_DAY_YESTERDAY (unsigned char)0x1

// -------------------------------------------
// GLOBAL FOR ALL COUNTERS
// -------------------------------------------
//transcations defines
#define TRANSACTION_COUNTER_OPEN							(unsigned char)0x1
#define TRANSACTION_GET_RATIO								(unsigned char)0x2
#define TRANSACTION_PROFILE_POINTER							(unsigned char)0x3
#define TRANSACTION_PROFILE_INTERVAL						(unsigned char)0x4
#define	TRANSACTION_PROFILE_INIT							(unsigned char)0x5
#define TRANSACTION_DATE_TIME_STAMP							(unsigned char)0x6
#define TRANSACTION_UNKNOWN_ASK_FOR_MAYK					(unsigned char)0x7
#define TRANSACTION_UNKNOWN_ASK_FOR_UJUK					(unsigned char)0x8
#define TRANSACTION_UNKNOWN_ASK_FOR_GUBANOV					(unsigned char)0x9
#define TRANSACTION_METERAGE_CURRENT						(unsigned char)0x10 // 11, 12, 13 or 14 depending on tarifs, i.e. 10+tIndex
#define TRANSACTION_METERAGE_MONTH							(unsigned char)0x20 // 21, 22, 23 or 24 depending on tarifs, i.e. 20+tIndex
#define TRANSACTION_METERAGE_DAY							(unsigned char)0x30 // 31, 32, 33 or 34 depending on tarifs, i.e. 30+tIndex
#define TRANSACTION_UJUK_WRITE_TARIFF_ZONE					(unsigned char)0x40 //+0...10 depending on command identificator
#define TRANSACTION_UJUK_CURR_PPTR							(unsigned char)0x4B
															//(unsigned char)0x4C
															//(unsigned char)0x4D
															//(unsigned char)0x4E
															//(unsigned char)0x4F
#define TRANSACTION_POWER_MONTH								(unsigned char)0x50 // 51 , 52 , 53 , 54 depending on tarifs
#define TRANSACTION_POWER_TODAY								(unsigned char)0x60 // 61 , 62 , 63 , 64 depending on tarifs
#define TRANSACTION_POWER_YESTERDAY							(unsigned char)0x70 // 71 , 72 , 73 , 74 depending on tarifs


#define TRANSACTION_READ_DEVICE_SW_VERSION					(unsigned char)0x80
#define TRANSACTION_SYNC_HARDY								(unsigned char)0x81
#define TRANSACTION_SYNC_SOFT								(unsigned char)0x82

#define TRANSACTION_GET_EVENTS_NUMBER						(unsigned char)0x83
#define TRANSACTION_GET_EVENT								(unsigned char)0x84

#define TRANSACTION_GET_PQP_FREQUENCY						(unsigned char)0x85

#define TRANSACTION_GET_PQP_VOLTAGE_PHASE1					(unsigned char)0x86
#define TRANSACTION_GET_PQP_VOLTAGE_PHASE2					(unsigned char)0x87
#define TRANSACTION_GET_PQP_VOLTAGE_PHASE3					(unsigned char)0x88
#define TRANSACTION_GET_PQP_VOLTAGE_GROUP					(unsigned char)0x89

#define TRANSACTION_GET_PQP_AMPERAGE_PHASE1					(unsigned char)0x8A
#define TRANSACTION_GET_PQP_AMPERAGE_PHASE2					(unsigned char)0x8B
#define TRANSACTION_GET_PQP_AMPERAGE_PHASE3					(unsigned char)0x8C
#define TRANSACTION_GET_PQP_AMPERAGE_GROUP					(unsigned char)0x8D

#define TRANSACTION_GET_PQP_POWER_P_PHASE1					(unsigned char)0x8E
#define TRANSACTION_GET_PQP_POWER_P_PHASE2					(unsigned char)0x8F
#define TRANSACTION_GET_PQP_POWER_P_PHASE3					(unsigned char)0x90
#define TRANSACTION_GET_PQP_POWER_P_PHASE_SIGMA				(unsigned char)0x91
#define TRANSACTION_GET_PQP_POWER_P_GROUP					(unsigned char)0x92

#define TRANSACTION_GET_PQP_POWER_Q_PHASE1					(unsigned char)0x93
#define TRANSACTION_GET_PQP_POWER_Q_PHASE2					(unsigned char)0x94
#define TRANSACTION_GET_PQP_POWER_Q_PHASE3					(unsigned char)0x95
#define TRANSACTION_GET_PQP_POWER_Q_PHASE_SIGMA				(unsigned char)0x96
#define TRANSACTION_GET_PQP_POWER_Q_GROUP					(unsigned char)0x97

#define TRANSACTION_GET_PQP_POWER_S_PHASE1					(unsigned char)0x98
#define TRANSACTION_GET_PQP_POWER_S_PHASE2					(unsigned char)0x99
#define TRANSACTION_GET_PQP_POWER_S_PHASE3					(unsigned char)0x9A
#define TRANSACTION_GET_PQP_POWER_S_PHASE_SIGMA				(unsigned char)0x9B
#define TRANSACTION_GET_PQP_POWER_S_GROUP					(unsigned char)0x9C

#define TRANSACTION_GET_PQP_RATIO_COS_PHASE1				(unsigned char)0x9D
#define TRANSACTION_GET_PQP_RATIO_COS_PHASE2				(unsigned char)0x9E
#define TRANSACTION_GET_PQP_RATIO_COS_PHASE3				(unsigned char)0x9F
#define TRANSACTION_GET_PQP_RATIO_COS_GROUP					(unsigned char)0xA0

#define TRANSACTION_GET_PQP_RATIO_TAN_PHASE1				(unsigned char)0xA1
#define TRANSACTION_GET_PQP_RATIO_TAN_PHASE2				(unsigned char)0xA2
#define TRANSACTION_GET_PQP_RATIO_TAN_PHASE3				(unsigned char)0xA3
#define TRANSACTION_GET_PQP_RATIO_TAN_GROUP					(unsigned char)0xA4

#define TRANSACTION_GET_PQP_POWER_ALL						TRANSACTION_GET_PQP_POWER_P_GROUP
#define TRANSACTION_GET_PQP_RATIO							TRANSACTION_GET_PQP_RATIO_COS_GROUP

#define TRANSACTION_GUBANOV_SET_DATE						(unsigned char)0xA5
#define TRANSACTION_GUBANOV_SET_TIME						(unsigned char)0xA6

#define TRANSACTION_BEGIN_WRITING_TARIFF_MAP				(unsigned char)0xA7
#define TRANSACTION_END_WRITING_TARIFF_MAP					(unsigned char)0xA8
#define TRANSACTION_WRITING_TARIFF_STT						(unsigned char)0xA9
#define TRANSACTION_WRITING_TARIFF_WTT						(unsigned char)0xAA
#define TRANSACTION_WRITING_TARIFF_MTT						(unsigned char)0xAB
#define TRANSACTION_WRITING_TARIFF_LWJ						(unsigned char)0xAC

#define TRANSACTION_PROFILE_GET_ALL_HOUR					(unsigned char)0xAD
#define TRANSACTION_PROFILE_GET_VALUE						(unsigned char)0xAE
#define TRANSACTION_PROFILE_GET_SEARCHING_STATE				(unsigned char)0xAF

#define TRANSACTION_WRITING_TARIFF_HOLIDAYAYS_TO_PH_MEMORY	(unsigned char)0xB0
#define TRANSACTION_WRITING_TARIFF_MAP_TO_PH_MEMORY			(unsigned char)0xB1
#define TRANSACTION_WRITING_TARIFF_MOVED_DAYS_TO_PH_MEMMORY	(unsigned char)0xB2

#define TRANSACTION_MAYK_SET_POWER_CONTROLS					(unsigned char)0xB3
#define TRANSACTION_MAYK_SET_POWER_CONTROLS_DEFAULT			(unsigned char)0xB4
#define TRANSACTION_MAYK_SET_POWER_CONTROLS_DONE			(unsigned char)0xB5
#define TRANSACTION_MAYK_POWER_SWITCHER_STATE_SET			(unsigned char)0xB6

#define TRANSACTION_UJUK_SET_POWER_CONTROLS					(unsigned char)0xB7
#define TRANSACTION_UJUK_SET_PROGRAM_FLAGS					(unsigned char)0xB8
#define TRANSACTION_UJUK_SET_POWER_LIMIT_CONTROL			(unsigned char)0xB9

#define TRANSACTION_UJUK_GET_COUNTER_STATE_WORD				(unsigned char)0xBA

#define TRANSACTION_GUBANOV_RELE_CONTROL					(unsigned char)0xBB
#define TRANSACTION_GUBANOV_SET_POWER_LIMIT					(unsigned char)0xBC
#define TRANSACTION_GUBANOV_SET_CONSUMER_CATEGORY			(unsigned char)0xBD
#define TRANSACTION_GUBANOV_SET_MONTH_LIMIT					(unsigned char)0xBE

#define TRANSACTION_GUBANOV_SET_DAY_TEMPLATE				(unsigned char)0xBF
#define TRANSACTION_GUBANOV_SET_1_TARIFF_START_TIME			(unsigned char)0xC0
#define TRANSACTION_GUBANOV_SET_2_TARIFF_START_TIME			(unsigned char)0xC1
#define TRANSACTION_GUBANOV_SET_3_TARIFF_START_TIME			(unsigned char)0xC2
#define TRANSACTION_GUBANOV_SET_HOLIDAY						(unsigned char)0xC3

#define TRANSACTION_START_WRITING_INDICATIONS				(unsigned char)0xC4
#define TRANSACTION_WRITING_INDICATIONS						(unsigned char)0xC5
#define TRANSACTION_WRITING_LOOP_DURATION					(unsigned char)0xC6

#define TRANSACTION_USER_DEFINED_COMMAND					(unsigned char)0xC7

#define TRANSACTION_SET_CURRENT_SEASON						(unsigned char)0xC8
#define TRANSACTION_ALLOW_SEASON_CHANGING					(unsigned char)0xC9

//tarifs mask
#define COUNTER_MASK_T1 (unsigned char)0x1
#define COUNTER_MASK_T2 (unsigned char)0x2
#define COUNTER_MASK_T3 (unsigned char)0x4
#define COUNTER_MASK_T4 (unsigned char)0x8
#define COUNTER_MASK_T5 (unsigned char)0x10
#define COUNTER_MASK_T6 (unsigned char)0x20
#define COUNTER_MASK_T7 (unsigned char)0x40
#define COUNTER_MASK_T8 (unsigned char)0x80

//energy mask
#define COUNTER_MASK_AP (unsigned char)0x1
#define COUNTER_MASK_AM (unsigned char)0x2
#define COUNTER_MASK_RP (unsigned char)0x4
#define COUNTER_MASK_RM (unsigned char)0x8

//other difines
#define POWER_PROFILE_READ (unsigned char)0x1
#define INTERVAL_EMPTY (unsigned char)0xFF
#define POINTER_EMPTY 0x0000FFFF
#define ALL_ENERGY (unsigned char) 0xF
#define ONLY_ENERGY_A_DIRECT (unsigned char)0x1
#define ONLY_ENERGY_A_R_DIRECT (unsigned char)0x5
#define ONLY_ENERGY_A_DIRECT_R_BOTH (unsigned char)0xD
#define RATIO_MASK (unsigned char) 0xF

//tarifs indexes
#define TARIFF_1 (unsigned char)0x1
#define TARIFF_2 (unsigned char)0x2
#define TARIFF_3 (unsigned char)0x3
#define TARIFF_4 (unsigned char)0x4
#define TARIFF_5 (unsigned char)0x4
#define TARIFF_6 (unsigned char)0x4
#define TARIFF_7 (unsigned char)0x4
#define TARIFF_8 (unsigned char)0x4

//energies indexes
#define ENERGY_AP (unsigned char)1
#define ENERGY_AM (unsigned char)2
#define ENERGY_RP (unsigned char)3
#define ENERGY_RM (unsigned char)4

//flags
#define TARIFS_READY (unsigned char)0x0
#define TARIFS_UPDATE (unsigned char)0x1
#define TARIFS_IN_PROGRESS (unsigned char)0x2

// -------------------------------------------
// CONNECTION part inclusion
// -------------------------------------------
#define CONNECTION_CLOSE 1
#define CONNECTION_ASK 2
#define CONNECTION_READY 3
#define CONNECTION_WAIT 4
#define CONNECTION_NEED_TO_CLOSE 5
#define CONNECTION_GSM_EMPTY 6
#define CONNECTION_GSM_WAIT_SMS 7

//connection state
#define CONNECTION_STATE_OFF 0
#define CONNECTION_STATE_ON 1

#define CONNECTION_GPRS_SERVER	0
#define CONNECTION_GPRS_ALWAYS 1
#define CONNECTION_GPRS_CALENDAR 2
#define CONNECTION_CSD_INCOMING 3
#define CONNECTION_CSD_OUTGOING 4
#define CONNECTION_ETH_ALWAYS 5
#define CONNECTION_ETH_CALENDAR 6
#define CONNECTION_ETH_SERVER 7

//for calendar
#define CONNECTION_INTERVAL_HOUR 1
#define CONNECTION_INTERVAL_DAY 2
#define CONNECTION_INTERVAL_WEEK 3
//for checking connection time from calendar
#define TIME_BETWEEN_CHECK 10 //seconds
#define CONNECTION_TIME_INTERVAL 900 //seconds


// -------------------------------------------
// UJUK COUNTERS part inclusion
// -------------------------------------------
//parameters
#define UJUK_LONG_ADDRESS_START_BYTE (unsigned char)0xFC
#define UJUK_CURRENT (unsigned char)0x0
#define UJUK_MONTH (unsigned char)0x83
#define UJUK_TODAY (unsigned char)0x84
#define UJUK_YESTERDAY (unsigned char)0x85
#define UJUK_ARCHIVE_DAY (unsigned char)0x86
#define UJUK_RATIO (unsigned char)0x12
#define UJUK_CURRNET_TIME (unsigned char)0x0
#define UJUK_PROFILE_1 (unsigned char)0x3
#define UJUK_PROFILE_POINTER (unsigned char)0x4
#define UJUK_PROFILE_INTERVAL (unsigned char)0x6
#define UJUK_8_BYTES (unsigned char)0x8
#define UJUK_SYNC_TIME_HARDY (unsigned char)0x0C
#define UJUK_SYNC_TIME_SOFT (unsigned char)0x0D

//max bytes number for writing to phisical memory
#define UJUK_MAX_PH_MEM_BYTES	16

//commands
#define UJUK_CMD_OPEN_COUNTER (unsigned char)0x1
#define UJUK_CMD_GET_METERAGES_FROM_ARCHIVE (unsigned char)0xA
#define UJUK_CMD_SIMPLE_GET_METERAGES (unsigned char)0x05
#define UJUK_CMD_GET_MONTH_METERAGES (unsigned char)0xA
#define UJUK_CMD_GET_PROFILE_SETTINGS (unsigned char)0x8
#define UJUK_CMD_GET_PARAMS (unsigned char)0x8
#define UJUK_CMD_GET_JOURNAL_TIME (unsigned char)0x4
#define UJUK_CMD_GET_READ_MEM (unsigned char)0x6
#define UJUK_CMD_SET_PROPERTY (unsigned char)0x03
#define UJUK_CMD_GET_ENERGY_BY_BINARY_MASK (unsigned char)0x0B

#define UJUK_CMD_GET_EVENT (unsigned char)0x09

#define UJUK_RES_ERR_PWD	0x03

#define UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_CURRENT				(unsigned char)0x00
#define UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_MONTH				(unsigned char)0x83
#define UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_DAY					(unsigned char)0x86
#define UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_TODAY				(unsigned char)0x84
#define UJUK_BINARY_MASK_METERAGE_ARCH_TYPE_YESTERDAY			(unsigned char)0x85


//lenghts
#define UJUK_SIZE_OF_CRC 2
#define UJUK_SIZE_OF_PASSWORD 6
#define UJUK_SIZE_OF_LONG_ADDRESS 4

//lenght of answers

//general
#define UJUK_ANSWER_ONLY_ERRORCODE_SHORT					4
#define UJUK_ANSWER_ONLY_ERRORCODE_LONG						(UJUK_ANSWER_ONLY_ERRORCODE_SHORT + 4)

#define UJUK_ANSWER_OPEN_COUNTER_SHORT						UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_OPEN_COUNTER_LONG						(UJUK_ANSWER_OPEN_COUNTER_SHORT + 4)

#define UJUK_ANSWER_COUNTER_TIME_SHORT						11
#define UJUK_ANSWER_COUNTER_TIME_LONG						(UJUK_ANSWER_COUNTER_TIME_SHORT + 4)

#define UJUK_ANSWER_PARAMS_COUNTER_SHORT					6
#define UJUK_ANSWER_PARAMS_COUNTER_LONG						(UJUK_ANSWER_PARAMS_COUNTER_SHORT + 4)

#define UJUK_ANSWER_METERAGE_SHORT							19
#define UJUK_ANSWER_METERAGE_LONG							(UJUK_ANSWER_METERAGE_SHORT + 4)

#define UJUK_ANSWER_METERAGE_1_TM_02_SHORT					7
#define UJUK_ANSWER_METERAGE_1_TM_02_LONG					(UJUK_ANSWER_METERAGE_1_TM_02_SHORT + 4)

#define UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_SHORT			UJUK_ANSWER_METERAGE_SHORT
#define UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_LONG			(UJUK_ANSWER_SIMPLE_METERAGE_REQUEST_SHORT + 4)

#define UJUK_ANSWER_METERAGE_1_TM_02M_SHORT					15
#define UJUK_ANSWER_METERAGE_1_TM_02M_LONG					(UJUK_ANSWER_METERAGE_1_TM_02M_SHORT + 4)

#define UJUK_ANSWER_PROFILE_POINTER_SHORT					10
#define UJUK_ANSWER_PROFILE_POINTER_LONG					(UJUK_ANSWER_PROFILE_POINTER_SHORT + 4)

#define UJUK_ANSWER_PROFILE_INTERVAL_SHORT					5
#define UJUK_ANSWER_PROFILE_INTERVAL_LONG					(UJUK_ANSWER_PROFILE_INTERVAL_SHORT + 4)

#define UJUK_ANSWER_PROFILE_VALUE_SHORT						11
#define UJUK_ANSWER_PROFILE_VALUE_LONG						(UJUK_ANSWER_PROFILE_VALUE_SHORT + 4)

#define UJUK_ANSWER_SYNC_TIME_HARDY_SHORT					UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_SYNC_TIME_HARDY_LONG					(UJUK_ANSWER_SYNC_TIME_HARDY_SHORT + 4)

#define UJUK_ANSWER_PROFILE_INTERVAL_VALUE_SHORT			5
#define UJUK_ANSWER_PROFILE_INTERVAL_VALUE_LONG				(UJUK_ANSWER_PROFILE_INTERVAL_VALUE_SHORT + 4)

#define UJUK_ANSWER_GET_PROFILE_POINTER_SHORT				UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_GET_PROFILE_POINTER_LONG				(UJUK_ANSWER_GET_PROFILE_POINTER_SHORT + 4)

#define UJUK_ANSWER_GET_PROFILE_SEARCHING_STATE_SHORT		8
#define UJUK_ANSWER_GET_PROFILE_SEARCHING_STATE_LONG		(UJUK_ANSWER_GET_PROFILE_SEARCHING_STATE_SHORT + 4)

#define UJUK_ANSWER_GET_PROFILE_VALUE_SHORT					3 // plus value length
#define UJUK_ANSWER_GET_PROFILE_VALUE_LONG					(UJUK_ANSWER_GET_PROFILE_VALUE_SHORT + 4)

#define UJUK_ANSWER_WRITING_DATA_TO_PH_MEMORY_SHORT			UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_WRITING_DATA_TO_PH_MEMORY_LONG			(UJUK_ANSWER_WRITING_DATA_TO_PH_MEMORY_SHORT + 4)

#define UJUK_ANSWER_WRITING_POWER_CTRL_RULES_SHORT			UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_WRITING_POWER_CTRL_RULES_LONG			(UJUK_ANSWER_WRITING_POWER_CTRL_RULES_SHORT + 4)

#define UJUK_ANSWER_WRITING_PROGRAM_FLAGS_SHORT				UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_WRITING_PROGRAM_FLAGS_LONG				(UJUK_ANSWER_WRITING_PROGRAM_FLAGS_SHORT + 4)

#define UJUK_ANSWER_WRITING_POWER_LIMIT_CONTROL_SHORT		UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_WRITING_POWER_LIMIT_CONTROL_LONG		(UJUK_ANSWER_WRITING_POWER_LIMIT_CONTROL_SHORT + 4)

#define UJUK_ANSWER_GET_COUNTER_STATE_WORD_SHORT			8
#define UJUK_ANSWER_GET_COUNTER_STATE_WORD_LONG				(UJUK_ANSWER_GET_COUNTER_STATE_WORD_SHORT + 4)

#define UJUK_ANSWER_SET_TARIFF_ZONE_SHORT					5
#define UJUK_ANSWER_SET_TARIFF_ZONE_LONG					(UJUK_ANSWER_SET_TARIFF_ZONE_SHORT + 4)

#define UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT			UJUK_ANSWER_ONLY_ERRORCODE_SHORT
#define UJUK_ANSWER_SET_INDICATION_GENERAL_LONG				(UJUK_ANSWER_SET_INDICATION_GENERAL_SHORT + 4)

#define UJUK_ANSWER_GET_METERAGES_BY_BINARY_MASK_SHORT		4 //plus value length
#define UJUK_ANSWER_GET_METERAGES_BY_BINARY_MASK_LONG		(UJUK_ANSWER_GET_METERAGES_BY_BINARY_MASK_SHORT + 4)


#define UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE        48
#define UJUK_TARIFF_MOVED_DAYS_LIST_MAX_SIZE    31

#define UJUK_TARIFF_DAY_MAP_SIZE                72
#define UJUK_TARIFF_DAY_TYPE_NUMB               8

#define UJUK_SOFT_HARD_SYNC_SW					120 //seconds

//ujuk definitions
#define UJUK_PQP_RATIO_AM_5			0
#define UJUK_PQP_RATIO_AM_1			1
#define UJUK_PQP_RATIO_AM_10		2
#define UJUK_PQP_V_57				0
#define UJUK_PQP_V_230				1
#define UJUK_PQP_TRANSF_CONN		0
#define UJUK_PQP_DIRECT_CONN		1

// -------------------------------------------
// GUBANOV COUNTERS part inclusion
// -------------------------------------------

#define GCMSK_PROFILE		(BYTE)0x1
#define GCMSK_EXTENDED_CMD	(BYTE)0x2
//#define GCMSK_PQP			(BYTE)0x4
#define GCMSK_REACTIVE		(BYTE)0x10

enum GUBANOV_D_QUIZ_STRATEGY
{
	GDQS_FIRST_CURRENT_MTR = 0,
	GDQS_BY_DAY_DIFF,
	GDQS_REACT_BY_DAY_DIFF
};

#define GUBANOV_COMMAND_ANSWER_POS 4
#define GUBANOV_METERAGES_RATIO_VALUE (unsigned char)0xAA
#define GUBANOV_START_MARKER_ADDRESS (unsigned char) '#'
#define GUBANOV_GROUP_MARKER_ADDRESS (unsigned char) '@'
#define GUBANOV_SOFT_TIME_CORRECTION_DIFF 20
#define GUBANOV_SIZE_OF_CRC 2
#define GUBANOV_SIZE_OF_ADDRESS 3
#define GUBANOV_SIZE_OF_PASSWORD 5

#define GUBANOV_SIMPLE_YES_NO_ANSWER						9
#define GUBANOV_IMPROVED_YES_NO_ANSWER						10



#define GUBANOV_ANSWER_LENGTH_DAY_MAYK_102					20

#define GUBANOV_ANSWER_LENGTH_DATE_TIME_STAMP				21
#define GUBANOV_ANSWER_LENGTH_RATIO							10

#define GUBANOV_ANSWER_LENGTH_PROFILE						19
#define GUBANOV_ANSWER_ART_LENGTH_PROFILE					30


#define GUBANOV_ANSWER_ACTIVE_POWER							12


#define GUBANOV_ANSWER_SIMPLE_JOURNAL_BOX_OPENED			21
#define GUBANOV_ANSWER_SIMPLE_JOURNAL_POWER_SWITCH_ON		21
#define GUBANOV_ANSWER_SIMPLE_JOURNAL_POWER_SWITCH_OFF		21
#define GUBANOV_ANSWER_SIMPLE_JOURNAL_TIME_SINCED			21

#define GUBANOV_ANSWER_SOFT_SYNC_TIME						11

#define GUBANOV_ANSWER_RELE_CONTROL							GUBANOV_SIMPLE_YES_NO_ANSWER
#define GUBANOV_ANSWER_SET_MONTH_LIMIT						GUBANOV_SIMPLE_YES_NO_ANSWER
#define GUBANOV_ANSWER_SET_CONSUMER_CATEGORY				GUBANOV_SIMPLE_YES_NO_ANSWER

#define GUBANOV_ANSWER_SET_DAY_TEMPLATE						GUBANOV_IMPROVED_YES_NO_ANSWER

#define GUBANOV_ANSWER_SET_1_TARIFF_START_TIME				GUBANOV_IMPROVED_YES_NO_ANSWER
#define GUBANOV_ANSWER_SET_2_TARIFF_START_TIME				GUBANOV_IMPROVED_YES_NO_ANSWER
#define GUBANOV_ANSWER_SET_3_TARIFF_START_TIME				GUBANOV_IMPROVED_YES_NO_ANSWER

#define GUBANOV_ANSWER_SHORT_SET_HOLIDAY					GUBANOV_SIMPLE_YES_NO_ANSWER
#define GUBANOV_ANSWER_LONG_SET_HOLIDAY						GUBANOV_IMPROVED_YES_NO_ANSWER

#define GUBANOV_ANSWER_INDICATIONS_SIMPLE					GUBANOV_SIMPLE_YES_NO_ANSWER
#define GUBANOV_ANSWER_INDICATIONS_IMPROVED					GUBANOV_IMPROVED_YES_NO_ANSWER
#define GUBANOV_ANSWER_LOOP_DURATION						GUBANOV_SIMPLE_YES_NO_ANSWER

#define GUBANOV_ANSWER_GROUP_CMD							0



#define GUBANOV_SIMPLE_JOURNAL_EVENT_OPEN					1
#define GUBANOV_SIMPLE_JOURNAL_EVENT_POWER_ON				2
#define GUBANOV_SIMPLE_JOURNAL_EVENT_POWER_OFF				3
#define GUBANOV_SIMPLE_JOURNAL_EVENT_TIME_SYNC				4

#define GUBANOV_CONSUMER_CATERORY							95

// -------------------------------------------
// MAYK COUNTERS part inclusion
// -------------------------------------------
//commands
#define MAYK_CMD_GET_DATE		(unsigned short)0x0013
//+

#define MAYK_CMD_GET_TYPE		(unsigned short)0x0011

#define MAYK_CMD_GET_PROFILE	(unsigned short)0x0024

#define MAYK_CMD_GET_CURRENT	(unsigned short)0x0022
//+

#define MAYK_CMD_GET_METERAGE	(unsigned short)0x0023

#define MAYK_CMD_GET_RATIO		(unsigned short)0x0201
//+

#define MAYK_CMD_SEASON_CHANGE	(unsigned short)0x0033

#define MAYK_CMD_SYNC_HARDY		(unsigned short)0x0015

#define MAYK_CMD_SYNC_SOFT		(unsigned short)0x0014

#define MAYK_CMD_GET_PQP        (unsigned short)0x002D

#define MAYK_CMD_SET_POWER_CTRL	(unsigned short)0x002C

#define MAYK_CMD_SET_POWER_SWITCHER_STATE (unsigned short)0x0202

#define MAYK_CMD_BEGIN_TARIFF_WRITING	(unsigned short)0x001C
#define MAYK_CMD_END_TARIFF_WRITING		(unsigned short)0x001D
#define MAYK_CMD_TARIFF_WRITING_STT		(unsigned short)0x001E
#define MAYK_CMD_TARIFF_WRITING_WTT		(unsigned short)0x001F
#define MAYK_CMD_TARIFF_WRITING_MTT		(unsigned short)0x0020
#define MAYK_CMD_TARIFF_WRITING_LWJ		(unsigned short)0x0021

#define MAYK_CMD_WRITING_INDICATIONS	(unsigned short)0x0027

#define MAYK_CMD_GET_VERSION			(unsigned short)0x0011

#define MAYK_CMD_GET_EVENTS_NUMB		(unsigned short)0x0025
#define MAYK_CMD_GET_EVENT				(unsigned short)0x0026

#define MAYK_RESULT_OK					(unsigned char)0x00
#define MAYK_RESULT_ERR_PWD				(unsigned char)0x04
#define MAYK_RESULT_NOT_FOUND			(unsigned char)0x03
#define MAYK_RESULT_TIME_DIFF_TO_LARGE  (unsigned char)0x07
#define MAYK_RESULT_TIME_IN_PREV_SYNC	(unsigned char)0x06

#define MAYK_INDICATIONS_MAX_VALUE	46
#define MAYK_INDICATIONS_MAX_LOOPS	5

#define MAYK_SOFT_HARD_SYNC_SW	(unsigned int)300 // seconds.  //if this time sync worth higher then this define then use hardy sync time. else use soft sync

//codes' positions in answer
#define MAYK_RESULT_CODE_POS    	6  //position in answer without 7E-quotes and stuffing
#define MAYK_COMMAND_CODE_FIRST_POS	4  //position in answer without 7E-quotes and stuffing
#define MAYK_COMMAND_CODE_SEC_POS	5  //position in answer without 7E-quotes and stuffing

//other defines
#define MAYK_MAX_PROFILE_REQ		2	//maximum energy profiles in 1 request because of RF buffer size
#define MAYK_MAX_METRAGE_REQ		2   //maximum metrages in one request because of RF buffer size


#define MAYK_PC_FLD_MASK_OTHER		0
#define MAYK_PC_FLD_MASK_RULES		1
#define MAYK_PC_FLD_MASK_PQ			2
#define MAYK_PC_FLD_MASK_E30		3
#define MAYK_PC_FLD_MASK_ED			4
#define MAYK_PC_FLD_MASK_EM			5
//#define MAYK_PC_FLD_MASK_UI			6
#define MAYK_PC_FLD_MASK_UMAX			6
#define MAYK_PC_FLD_MASK_UMIN			7
#define MAYK_PC_FLD_MASK_IMAX			8

#define MAYK_PC_FLD_LIMIT_PQ		10
#define MAYK_PC_FLD_LIMIT_E30		20
#define MAYK_PC_FLD_LIMIT_ED		40
#define MAYK_PC_FLD_LIMIT_EM		60
#define MAYK_PC_FLD_LIMIT_UI		80


//
//power control offsets
//
#define MAYK_MCPCR_OFFSET_MASKOTHER				0x0   //0    //+ 2 bytes
#define MAYK_MCPCR_OFFSET_MASKPQ				0x2   //2    //+ 4 bytes
#define MAYK_MCPCR_OFFSET_MASKE30				0x6   //6    //+ 2 bytes
#define MAYK_MCPCR_OFFSET_MASKED				0x8   //8    //+ 2 bytes
#define MAYK_MCPCR_OFFSET_MASKEM				0xA   //10   //+ 2 bytes
//#define MAYK_MCPCR_OFFSET_MASKUI				0xC   //12   //+ 6 (3x2 bytes)
#define MAYK_MCPCR_OFFSET_MASKUMAX				0xC
#define MAYK_MCPCR_OFFSET_MASKUMIN				0xE
#define MAYK_MCPCR_OFFSET_MASKIMAX				0x10
#define MAYK_MCPCR_OFFSET_MASKRULES				0x12  //18   //+ 2 bytes

#define MAYK_MCPCR_OFFSET_LIMITS_P				0x14  //20   //+4xMR(4x4) 64 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_E30			0x54  //84   //+5xMR(4x4) 80 bytes long

#define MAYK_MCPCR_OFFSET_LIMITS_ED_T1			0xA4  //164  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T2			0xF4  //244  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T3			0x144 //324  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T4			0x194 //404  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T5			0x1e4 //484  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T6			0x234 //564  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T7			0x284 //644  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_T8			0x2d4 //724  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_ED_TS			0x324 //804  //+5xMR(4x4) 80 bytes long

#define MAYK_MCPCR_OFFSET_LIMITS_EM_T1			0x374 //884  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T2			0x3c4 //964  //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T3			0x414 //1044 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T4			0x464 //1124 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T5			0x4b4 //1204 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T6			0x504 //1284 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T7			0x554 //1364 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_T8			0x5a4 //1444 //+5xMR(4x4) 80 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_EM_TS			0x5f4 //1524 //+5xMR(4x4) 80 bytes long

#define MAYK_MCPCR_OFFSET_LIMITS_U_MAX			0x644 //1604 //+3x4(int)  12 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_U_MIN			0x650 //1616 //+3x4(int)  12 bytes long
#define MAYK_MCPCR_OFFSET_LIMITS_I_MAX			0x65c //1628 //+3x4(int)  12 bytes long

//#define MAYK_MCPCR_OFFSET_TIMINGS				0x668 //1640 //+4x4(int)  16 bytes long
#define MAYK_MCPCR_OFFSET_TIMING_MR_P			0x668
#define MAYK_MCPCR_OFFSET_TIMING_T_U_MAX		0x669
#define MAYK_MCPCR_OFFSET_TIMING_T_U_MIN		0x66A
#define MAYK_MCPCR_OFFSET_TIMING_T_DELAY_OFF	0x66B

#define MAYK_DATA_TO_SET_SIZE 128

#define MAYK_MAX_HALF_HOURS_DIFF 6

// -------------------------------------------
// UNKNOWN part inclusion
// -------------------------------------------

#define UNKNOWN_ASK_MAYK        (unsigned short)0x0201
#define UNKNOWN_ASK_GUBANOV     (unsigned char)'R'
#define UNKNOWN_ASK_UJUK        (unsigned char)0x12

#define UNKNOWN_ID_MAYK         1
#define UNKNOWN_ID_GUBANOV      2
#define UNKNOWN_ID_UJUK         3

// -------------------------------------------
// ETH part inclusion
// -------------------------------------------

#define ETH_STATIC 0
#define ETH_DHCP 1
#define ETH_INTERFACES "/etc/network/interfaces"
#define ETH_RESOLV "/etc/resolv.conf"
#define ETH_CONNECTION_COUNTER "/home/root/ethcounter"

#define ETH_HW_SIZE 17
#define ETH_MAC_CFG_PATH "/home/root/cfg/mac.cfg"

// -------------------------------------------
// GSM part inclusion
// -------------------------------------------

#define GSM_IMEI_LENGTH 15
#define GSM_OPERATOR_NAME_LENGTH 20
#define GSM_CONNECTION_COUNTER "/home/root/gsmcounter"

enum simIdx_e
{
	SIM2,
	SIM1
};

#define GSM_POWERON_TIMEOUT 20
#define GSM_NETWORK_REG_TIMEOUT 120
#define PHONE_MAX_SIZE 45

#define TIMEOUT1 ( 30 + 15 ) //inactivity traffic timeout
#define TIMEOUT2 60 //Time between two connection attempts  // was 60
#define TIMEOUT3 (60*60*2) //waiting incoming call timeout

#define GSM_SMALL_ANSWER_TIMEOUT 20
#define GSM_LARGE_ANSWER_TIMEOUT 120

#define MAX_MESSAGE_LENGTH 1460

#define GSM_INPUT_BUF_SIZE 4096
#define GSM_TASK_RX_SIZE 1024

enum simPrinciple_e
{
	SIM_FIRST = 1 ,
	SIM_SECOND = 2 ,
	SIM_FIRST_THEN_SECOND = 3 ,
	SIM_SECOND_THEN_FIRST = 4
};

#define SMS_TEXT_LENGTH 160

enum gsmDialState_e
{
	GDS_OFFLINE,
	GDS_WAIT,
	GDS_NEED_TO_ANSW,
	GDS_NEED_TO_HANG,
	GDS_ONLINE
};

/*

#define GSM_TASK_EMPTY 0
#define GSM_TASK_WAIT 1
#define GSM_TASK_READY 2


#define GSM_SMS_INDEX_NO_NEW_MESSAGES					-1
#define GSM_MORE_THEN_ONE_MESSAGE_WAS_RECIEVED			-2
#define GSM_SMS_INDEX_CHECK_AFTER_OPEN					-3
//#define GSM_SMS_INDEX_CHECK_AFTER_LONG_TIMEOUT			-4

#define GSM_CMD_ANSWER_BANK_STANDART	{	(unsigned char *)"\r\nOK\r\n" , \
											(unsigned char *)"\r\nERROR\r\n" , \
											(unsigned char *)"\r\n+CME ERROR" , \
											NULL 	}

#define GSM_CMD_ANSWER_BANK_CMGS		{	(unsigned char *)">" , \
											(unsigned char *)"\r\nOK\r\n" , \
											(unsigned char *)"\r\nERROR\r\n" , \
											(unsigned char *)"\r\n+CME ERROR" , \
											(unsigned char *)"\r\n+CMS ERROR" , \
											NULL 	}

#define GSM_CMD_ANSWER_BANK_SISW		{	(unsigned char *)"\r\n^SISW:" , \
											(unsigned char *)"\r\nOK\r\n" , \
											(unsigned char *)"\r\nERROR\r\n" , \
											(unsigned char *)"\r\n+CME ERROR" , \
											NULL 	}

#define GSM_CMD_ANSWER_BANK_DIALER		{	(unsigned char *)"\r\nNO CARRIER\r\n" , \
											(unsigned char *)"\r\nBUSY\r\n" , \
											(unsigned char *)"\r\nCONNECT" , \
											(unsigned char *)"\r\nOK\r\n" , \
											NULL	}

#define GSM_CMD_ASYNC_MESSAGES			{	(unsigned char *)"\r\n^SISR:" , \
											(unsigned char *)"\r\n^SISW:" , \
											(unsigned char *)"\r\n^SIS:" , \
											(unsigned char *)"\r\n^SYSSTART\r\n" , \
											(unsigned char *)"\r\n+CMTI:" , \
											(unsigned char *)"\r\n+CLIP:" , \
											(unsigned char *)"\r\nRING\r\n" , \
											(unsigned char *)"\r\n+CRING:" , \
											(unsigned char *)"\r\n+CREG:" , \
											(unsigned char *)"\r\n+CGREG:" , \
											(unsigned char *)"\r\n+CMT:" , \
											(unsigned char *)"\r\n^SHUTDOWN\r\n" , \
											(unsigned char *)"\r\n^ORIG:" , \
											(unsigned char *)"\r\n^CONF:" , \
											(unsigned char *)"\r\n^CONN:" , \
											(unsigned char *)"\r\n^CEND:" , \
											(unsigned char *)"\r\n^DDTMF:" , \
											(unsigned char *)"\r\n^STIN:" , \
											(unsigned char *)"\r\n^AUDEND:" , \
											(unsigned char *)"\r\n^THERM:" , \
											(unsigned char *)"\r\n+CIEV:" , \
											(unsigned char *)"\r\n+CIEV:" , \
											(unsigned char *)"\r\n+CGEV:" , \
											(unsigned char *)"\r\n+CGEV:" , \
											(unsigned char *)"\r\n+CBM:" , \
											(unsigned char *)"\r\n+CDS:" , \
											(unsigned char *)"\r\n+CUSD:" , \
											(unsigned char *)"\r\n+CALA:" , \
											(unsigned char *)"\r\n+CGEV:" , \
											(unsigned char *)"\r\nOK" , \
											(unsigned char *)"\r\nERROR" , \
											NULL	  }
#define GSM_AVALANCHE_COMMANDS			{	(unsigned char *)"AT+CMGL=\"ALL\"\r" , \
											NULL }

enum GSM_INCOMING_CALL_STATE
{
	GSM_ICS_NO_CALL = 0,
	GSM_ICS_NEED_TO_ANSWER,
	GSM_ICS_NEED_TO_HANG
};

enum
{
	GSM_PART_USER ,
	GSM_PART_PASS ,
	GSM_PART_APN
}gsmApnPart;
*/

// -------------------------------------------
// MICRON V1 part inclusion
// -------------------------------------------


//uspd commands

//#define MICRON_COMM_APP_PROP	(unsigned short)0x0012

//#define MICRON_COMM_TAR_STAT_GET		(unsigned short)0x000D
//#define MICRON_COMM_TAR_MAP_START_WRITE (unsigned short)0x000E
//#define MICRON_COMM_TAR_MAP_STOP_WRITE	(unsigned short)0x000F


#define MICRON_COMM_GET_METR		(unsigned short)0x0001
#define MICRON_COMM_SET_M_DB		(unsigned short)0x0002

#define MICRON_COMM_SET_TIME	 	(unsigned short)0x0003
#define MICRON_COMM_GET_TIME		(unsigned short)0x0004

#define MICRON_COMM_SET_ST_PIPE		(unsigned short)0x0006
#define MICRON_COMM_SEND_PIPE		(unsigned short)0x0007

#define MICRON_COMM_REBOOT			(unsigned short)0x0008
#define MICRON_COMM_GET_SN			(unsigned short)0x0009
#define MICRON_COMM_SET_PROP		(unsigned short)0x000A
#define MICRON_COMM_GET_PROP		(unsigned short)0x000B

#define MICRON_COMM_TAR_MAP_SET		(unsigned short)0x000C
#define MICRON_COMM_PSM_RULES_SET	(unsigned short)0x000D
#define MICRON_COMM_FIRMWARE_SET	(unsigned short)0x000E
#define MICRON_COMM_USERCMNDS_SET	(unsigned short)0x000F

#define MICRON_COMM_GET_AUTODETECTED_COUNTERS (unsigned short)0x001B

#define MICRON_COMM_TARIFF_STOP		(unsigned short)0x001C
#define MICRON_COMM_PSM_RULES_STOP	(unsigned short)0x001D
#define MICRON_COMM_FIRMWARE_STOP	(unsigned short)0x001E
#define MICRON_COMM_USERCMNDS_STOP	(unsigned short)0x001F

#define MICRON_COMM_TARIFF_CLEAR	(unsigned short)0x002C
#define MICRON_COMM_PSM_RULES_CLEAR	(unsigned short)0x002D
#define MICRON_COMM_FIRMWARE_CLEAR  (unsigned short)0x002E
#define MICRON_COMM_USERCMNDS_CLEAR (unsigned short)0x002F

#define MICRON_RECOV_M_DB				(unsigned short)0x0012

#define MICRON_COMM_USD_FS		(unsigned short)0xFFCF
#define MICRON_COMM_SHELL_CMD	(unsigned short)0xFFCE
#define MICRON_COMM_NZIF_SET_PROP		(unsigned short)0xFF0A

#define MICRON_COMM_SEASON_FLG_SET	(unsigned short)0x0013
#define MICRON_COMM_SEASON_FLG_GET	(unsigned short)0x0014

#define MICRON_COMM_GET_LAST_METERAGES_DATES	(unsigned short)0xFFDB


// metrages masks
#define MICRON_METR_CURRENT								(unsigned int)0x1
#define MICRON_METR_DAY									(unsigned int)0x2
#define MICRON_METR_MONTH								(unsigned int)0x4
#define MICRON_METR_PROFILE								(unsigned int)0x8
#define MICRON_METR_COUNTER_JOURNAL_OPEN_BOX			(unsigned int)0x10
#define MICRON_METR_COUNTER_JOURNAL_OPEN_CONTACT		(unsigned int)0x20
#define MICRON_METR_COUNTER_JOURNAL_DEVICE_SWITCH		(unsigned int)0x40
#define MICRON_METR_COUNTER_JOURNAL_SYNC_HARD			(unsigned int)0x80
#define MICRON_METR_COUNTER_JOURNAL_SYNC_SOFT			(unsigned int)0x100
#define MICRON_METR_COUNTER_JOURNAL_SET_TARIFF_MAP		(unsigned int)0x200
#define MICRON_METR_COUNTER_JOURNAL_POWER_SWITCH		(unsigned int)0x400
#define MICRON_METR_COUNTER_JOURNAL_PQP					(unsigned int)0x1000000
#define MICRON_METR_USPD_JOURNAL_GENERAL				(unsigned int)0x800
#define MICRON_METR_USPD_JOURNAL_TARIFF					(unsigned int)0x1000
#define MICRON_METR_USPD_JOURNAL_POWER_SWITCH_MODULE	(unsigned int)0x2000
#define MICRON_METR_USPD_JOURNAL_RF_EVENTS				(unsigned int)0x4000
#define MICRON_METR_USPD_JOURNAL_PLC_EVENTS				(unsigned int)0x8000
#define MICRON_METR_USPD_JOURNAL_EIO_CARD_EVENTS		(unsigned int)0x10000
#define MICRON_METR_PQP									(unsigned int)0x20000
#define MICRON_METR_TARIFF_WRITE_STATE					(unsigned int)0x40000
#define MICRON_METR_POWER_SWITCH_MODULE_WRITE_STATE		(unsigned int)0x80000
#define MICRON_METR_DISCRET_OUTPUT_STATE				(unsigned int)0x100000
#define MICRON_METR_PLC_NETWORK_STATE					(unsigned int)0x200000
#define MICRON_METR_RF_NETWORK_STATE					(unsigned int)0x400000
#define MICRON_METR_DEVICE_SYNC_STATE					(unsigned int)0x800000
#define MICRON_METR_DEVICES_TRANSACTION_STATE			(unsigned int)0x2000000
#define MICRON_METR_USPD_JOURNAL_FIRMWARE_UPDATE		(unsigned int)0x4000000
#define MICRON_METR_COUNTER_JOURNAL_STATE_WORD_CHANGED	(unsigned int)0x8000000





//result codes
#define MICRON_RES_OK						(unsigned char)0x00
#define MICRON_RES_GEN_ERR					(unsigned char)0x01
#define MICRON_RES_ID_NOT_FOUND				(unsigned char)0x02
#define MICRON_RES_WRONG_PARAM				(unsigned char)0x03
#define MICRON_RES_UNKNOWN_COMMAND			(unsigned char)0x04
#define MICRON_RES_FILE_EMPTY				(unsigned char)0x05
#define MICRON_RES_FS_FAILURE				(unsigned char)0x06
#define MICRON_RES_TRANSPARENT_MODE_ERR		(unsigned char)0x07
#define MICRON_RES_SYNC_IS_ALREADY_STARTED	(unsigned char)0x08
#define MICRON_RES_PLC_COUNTERS_CONFIG_ERROR	(unsigned char)0x09
#define MICRON_RES_PLC_COORD_CONFIG_ERROR	(unsigned char)0x0A
#define MICRON_RES_DENY						(unsigned char)0x0D

// masks for properties
//#define MICRON_PROP_DEV_RS (unsigned int)0x01
//#define MICRON_PROP_DEV_PLC (unsigned int)0x02
//#define MICRON_PROP_COORD_PLC (unsigned int)0x04
//#define MICRON_PROP_GSM (unsigned int)0x08
//#define MICRON_PROP_ETH (unsigned int)0x10
//#define MICRON_PROP_USD (unsigned int)0x20


// masks for properties
#define MICRON_PROP_DEV_RS			(unsigned int)0x01
#define MICRON_PROP_DEV_RF			(unsigned int)0x02
#define MICRON_PROP_COORD_RF		(unsigned int)0x04
#define MICRON_PROP_GSM				(unsigned int)0x08
#define MICRON_PROP_ETH				(unsigned int)0x10
#define MICRON_PROP_USD				(unsigned int)0x20
#define MICRON_PROP_AUTH			(unsigned int)0x40
#define MICRON_PROP_DEV_PLC			(unsigned int)0x80
#define MICRON_PROP_COORD_PLC		(unsigned int)0x100
#define MICRON_PROP_EXIO			(unsigned int)0x200
#define MICRON_PROP_PIN				(unsigned int)0x400
#define MICRON_PROP_COUNTERS_BILL	(unsigned int)0x800

#define MICRON_MALLOC_ATTEMPTS 5
#define MICRON_IO_ATTEMPTS 3

#define MICRON_TIME_BETWEEN_CHECKING_CONNECTION 1 //seconds
#define MICRON_TIME_BETWEEN_CHECKING_BYTES_AVAILABLE_TO_READ 1 //seconds

#define MICRON_P_WRONG_TYPE 0xFF
#define MICRON_P_DATA_ANSW 0x10
#define MICRON_P_PING 0x08
#define MICRON_P_REPEAT 0x04
#define MICRON_P_ACK 0x02
#define MICRON_P_DATA 0x01

#define MICRON_PACKAGE_LENGTH 1024
#define MICRON_PACKAGE_HEAD_LENGTH 8
#define MICRON_PACKAGE_CHAIN_LENGTH 8  //length of 0x7E chain

#define	MICRON_PACKAGE_TYPE_POS (MICRON_PACKAGE_CHAIN_LENGTH)
#define MICRON_PACKAGE_LENGTH_FIRST_POS (MICRON_PACKAGE_CHAIN_LENGTH+1)
#define MICRON_PACKAGE_LENGTH_SEC_POS (MICRON_PACKAGE_CHAIN_LENGTH+2)
#define MICRON_PACKAGE_NUMBER_FIRST_POS (MICRON_PACKAGE_CHAIN_LENGTH+3)
#define MICRON_PACKAGE_NUMBER_SEC_POS (MICRON_PACKAGE_CHAIN_LENGTH+4)
#define MICRON_PACKAGE_TOTAL_FIRST_POS (MICRON_PACKAGE_CHAIN_LENGTH+5)
#define MICRON_PACKAGE_TOTAL_SEC_POS (MICRON_PACKAGE_CHAIN_LENGTH+6)
#define MICRON_PACKAGE_CRC_POS (MICRON_PACKAGE_CHAIN_LENGTH+7)

#define MICRON_SHELL_OUTPUT_MAX_PACKAGES 40
// -------------------------------------------
// PIO part inclusion
// -------------------------------------------

#define PIO_IN 					0 //osipov was 1
#define PIO_OUT 				1 //osipov was 2

#if REV_COMPILE_PIO_MMAP

#define PIO_CLI_DIN             ( 16*4 + 5 )  // GP4[5]
#define PIO_CLI_DCLK            ( 16*4 + 10 ) // GP4[10]
#define PIO_CLI_LOAD            ( 16*4 + 4  ) // GP4[4]
#define PIO_GSM_SIM_SEL         ( 16*3 + 12 ) // GP3[12]
#define PIO_GSM_NOT_OE          ( 16*4 + 14 ) // GP4[14]
#define PIO_GSM_POKIN           ( 16*2 + 15 ) // GP2[15]
#define PIO_GSM_DSR             ( 16*2 + 12 ) // GP2[12]
#define PIO_GSM_CTS             ( 16*5 + 3  ) // GP5[3]
#define PIO_GSM_RTS             ( 16*4 + 3 )  // GP4[3]
#define PIO_GSM_RI              ( 16*3 + 15 ) // GP3[15]
#define PIO_GSM_DTR             ( 16*3 + 14 ) // GP3[14]
#define PIO_GSM_PWR             ( 16*4 + 13 ) // GP4[13]//77
//#define PIO_485_TX_EN         ( 16*4 + 8  ) // GP4[8] //by kernel
#define PIO_PLC_RESET           ( 16*4 + 6 )  // GP4[6]
#define PIO_PLC_NOT_OE          ( 16*4 + 7  ) // GP4[7]
#define PIO_REBOOT				( 16*5 + 11 ) // GP5[11]

#else

#define PIO_CLI_DIN             ( 16*4 + 5 )  // GP4[5]
#define PIO_CLI_DCLK            ( 16*4 + 10 ) // GP4[10]
#define PIO_CLI_LOAD            ( 16*4 + 4  ) // GP4[4]
#define PIO_GSM_SIM_SEL         ( 16*3 + 12 ) // GP3[12]
#define PIO_GSM_NOT_OE          ( 16*4 + 14 ) // GP4[14]
#define PIO_GSM_POKIN           ( 16*2 + 15 ) // GP2[15]
#define PIO_GSM_DSR             ( 16*2 + 12 ) // GP2[12]
#define PIO_GSM_CTS             ( 16*5 + 3  ) // GP5[3]
#define PIO_GSM_RTS             ( 16*4 + 3 )  // GP4[3]
#define PIO_GSM_RI              ( 16*3 + 15 ) // GP3[15]
#define PIO_GSM_DTR             ( 16*3 + 14 ) // GP3[14]
#define PIO_GSM_PWR             ( 16*4 + 13 ) // GP4[13]
//#define PIO_485_TX_EN         ( 16*4 + 8  ) // GP4[8] //by kernel
#define PIO_PLC_RESET           ( 16*4 + 6 )  // GP4[6]
#define PIO_PLC_NOT_OE          ( 16*4 + 7  ) // GP4[7]
#define PIO_REBOOT				( 16*5 + 11 ) // GP5[11]
#define PIO_PLC_POWER			( 16*7 + 14 )

#endif

#define PIO_ERROR_COUNTER		"/home/root/pioerrorcounter"
#define PIO_MAX_ERROR_COUNTER_VALUE		20

// -------------------------------------------
// POOL part inclusion
// -------------------------------------------
#define POOL_STRATEGY_LINEAR_POOL 0
#define POOL_STRATEGY_PLC_PARALLEL_POOL_BY_MAP 2
#define POOL_STRATEGY_PLC_PARALLEL_POOL_BY_CONFIG 1

#define POOL_MAX_PLC_PARALLEL_NODES 3

#define POOL_MAX_TASK_TIME_REPEATE_FOR_ONE_COUNTER_PLC 60 //min
#define POOL_MIN_TASK_TIME_REPEATE_FOR_ONE_COUNTER_PLC 5 //min
#define POOL_MAX_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 60 //min
#define POOL_MIN_TASK_TIME_REPEATE_FOR_ONE_COUNTER_485 5 //min

#define POOL_ALTER_NOT_SET 			0
#define POOL_ALTER_NEED_00_LEAVE	1
#define POOL_ALTER_NEED_52_LEAVE	2
#define POOL_ALTER_NEED_57_LEAVE	3
#define POOL_ALTER_NEED_78_LEAVE	4


#define MAX_POOLS_PER_DAY 7
#define MAX_APPEND_TIMEOUT (20*60)

#define MAX_PLC_POLL_TIME_FOR_SINGLR_COUNTER (3*60) //3 minutes

#define POLL_WEIGHT_UNKNOWN (double)0.0
#define POLL_WEIGHT_MIN		 (double)0.01

// -------------------------------------------
// PLC part inclusion
// -------------------------------------------
#define PROTOCOL_PLC_CA_BYTE    1
#define PROTOCOL_PLC_LEN_LO     2
#define PROTOCOL_PLC_LEN_HI     3
#define PROTOCOL_PLC_TYPE       4
#define PROTOCOL_PLC_OPCODE     5
#define PROTOCOL_PLC_PAYLOAD    6
#define PROTOCOL_PLC_CRC        7

#define PLC_SEQ_BYTE_RUN (unsigned char)0xCA

#define PLC_INPUT_BUFFER_SIZE 1500
#define PLC_OUTPUT_BUFFER_SIZE 1500

#define PLC_MAX_PACK_TAG 999

//5 paramteres fo rf base


//8 parameters for other

//total params count


//some else parameter


//mode


//sizes
#define SIZE_OF_SERIAL 10

//device state

//define admission modes
#define PLC_ADMISSION_AUTO 0
#define PLC_ADMISSION_APPLICATION 1
#define PLC_ADMISSION_WITH_DETECT 2

//device errors

//info table update interval
#if REV_COMPILE_RELEASE
#define TIMEOUT_TABLE_UPDATE_ON_PLC 120 //every 2 minute
#define MAX_POOL_ITERATION_BEFORE_REREAD_PLC_TABLE 2 //every 3 ok counters polls
#else
#define TIMEOUT_TABLE_UPDATE_ON_PLC 60 //every 1 minute
#define MAX_POOL_ITERATION_BEFORE_REREAD_PLC_TABLE 1 //every 2 ok counters pollss
#endif

// -------------------------------------------
// SERIAL part inclusion
// -------------------------------------------

//local definitions
#define SERIAL_LINK_TO_PLC "/dev/ttyS1"
#define SERIAL_LINK_TO_EXIO "/dev/ttyS1"
#define SERIAL_LINK_TO_RS485 "/dev/ttyS2"
#define SERIAL_LINK_TO_GSM "/dev/ttyS0"

#define SERIAL_BAUD_PLC B38400
//#define SERIAL_BAUD_485 B9600
#define SERIAL_BAUD_EXIO B9600
#define SERIAL_BAUD_GSM B115200

#define SERIAL_RS_BAUD_300		B300
#define SERIAL_RS_BAUD_600		B600
#define SERIAL_RS_BAUD_1200		B1200
#define SERIAL_RS_BAUD_1800		B1800
#define SERIAL_RS_BAUD_2400		B2400
#define SERIAL_RS_BAUD_4800		B4800
#define SERIAL_RS_BAUD_9600		B9600
#define SERIAL_RS_BAUD_19200	B19200
#define SERIAL_RS_BAUD_38400	B38400
#define SERIAL_RS_BAUD_57600	B57600
#define SERIAL_RS_BAUD_115200	B115200

enum SERIAL_RS_BITS
{
	SERIAL_RS_BITS_7,
	SERIAL_RS_BITS_8
};

enum SERIAL_RS_STOP_BIT
{
	SERIAL_RS_ONCE_STOP_BIT,
	SERIAL_RS_DOUBLE_STOP_BIT,
};

enum SERIAL_RS_PARITY
{
	SERIAL_RS_PARITY_NONE,
	SERIAL_RS_PARITY_ODD,
	SERIAL_RS_PARITY_EVEN,
	SERIAL_RS_PARITY_MARK,
	SERIAL_RS_PARITY_SPACE
};

#define PORT_GSM 0  //uart 0 // dev - ttyS0
#define PORT_PLC 1  //uart 1 // dev - ttyS1
#define PORT_RS  2  //uart 2 // dev - ttyS2
#define PORT_EXIO 3

// -------------------------------------------
// STORAGE part inclusion
// -------------------------------------------
#define MAX_TARIFF_NUMBER 4

#if ( MAX_TARIFF_NUMBER == 8 )
	#define MAX_TARIFF_MASK	0xFF
#elif ( MAX_TARIFF_NUMBER == 4 )
	#define MAX_TARIFF_MASK	0x0F
#endif

#define CONFIG_PATH_PLC			"/home/root/cfg/plc.cfg"
#define CONFIG_PATH_RF			"/home/root/cfg/rf.cfg"
#define CONFIG_PATH_485			"/home/root/cfg/rs485.cfg"
#define CONFIG_PATH_NET_PLC		"/home/root/cfg/plcNet.cfg"
#define CONFIG_PATH_NET_RF		"/home/root/cfg/rfNet.cfg"
#define CONFIG_PATH_GSM			"/home/root/cfg/gsm.cfg"
#define CONFIG_PATH_ETH			"/home/root/cfg/ethernet.cfg"
#define CONFIG_PATH_CONNECTION	"/home/root/cfg/connection.cfg"
#define CONFIG_PATH_CPIN		"/home/root/cfg/cpin.cfg"
#define CONFIG_PATH_WUI			"/home/root/cfg/aut.cfg"
#define CONFIG_PATH_SERVICE		"/home/root/cfg/service.cfg"
#define CONFIG_PATH_SEASON		"/home/root/cfg/season.cfg"
#define CONFIG_PATH_COUNTERS_BILL "/home/root/countersbill/bill.xls"
#define STORAGE_SUPEVISOR_FIFO_PATH	"/home/root/svisorPipe"

#define STORAGE_PLACE_NAME_LENGTH 128

#define STORAGE_COUNTER_STRING "[counter]"

#define STORAGE_MAX_DAY 200
#define STORAGE_MAX_DAY_RS485 STORAGE_MAX_DAY
#define STORAGE_MAX_DAY_PLC STORAGE_MAX_DAY

#define STORAGE_MAX_MONTH 36
#define STORAGE_MAX_MONTH_RS485 STORAGE_MAX_MONTH
#define STORAGE_MAX_MONTH_PLC STORAGE_MAX_MONTH

#define STORAGE_MAX_PROFILE (48*200)
#define STORAGE_MAX_PROFILE_RS485 STORAGE_MAX_PROFILE
#define STORAGE_MAX_PROFILE_PLC STORAGE_MAX_PROFILE

#define STORAGE_CURRENT 0
#define STORAGE_DAY 1
#define STORAGE_MONTH 2
#define STORAGE_PROFILE 3

#define STORAGE_UNKNOWN_PROFILE_INTERVAL 255

#define STORAGE_PATH_CURRENT				"/home/root/fdb/current"
#define STORAGE_PATH_MONTH					"/home/root/fdb/month"
#define STORAGE_PATH_DAY					"/home/root/fdb/day"
#define STORAGE_PATH_PROFILE				"/home/root/fdb/profile"
#define STORAGE_PATH_EVENTS					"/home/root/fdb/events_uspd"
#define STORAGE_PATH_JOURNAL				"/home/root/fdb/events"
#define STORAGE_PATH_PQ						"/home/root/fdb/pqp"
#define STORAGE_PATH_STATUS					"/home/root/fdb/status"
#define STORAGE_UNIX_DOMAIN_LISTENER		"/home/root/webPipe"
#define STORAGE_PATH_CURRENT_SEASON			"/home/root/season"
#define STORAGE_PATH_IMSI					"/home/root/imsi"

#define STORAGE_PATH_USPD_SN				"/home/root/serial"
#define STORAGE_PATH_USPD_VERSION			"/home/root/version.info"
#define STORAGE_PATH_SERVICE				"/home/root/fdb/service"

#define STORAGE_ACC_DENY_FLG_PATH			"/etc/discard_output"

#define STORAGE_PATH_HARDWARE_WRITING_CTRL	"/home/root/hdwrctrl"
#define STORAGE_DEFPSWD	"psy6yg7n1705"

#define SFNL 64
#define SET_RES_AND_BREAK_WITH_MEM_ERROR {res = ERROR_MEM_ERROR; break;}
#define GET_STORAGE_FILE_NAME(__A__, __B__, __C__) memset(__A__,0,SFNL); sprintf(__A__, "%s_%ld", __B__, __C__);

// __DJ__ is a journal number
#define GET_JOURNAL_FILE_NAME(__AJ__, __BJ__, __CJ__, __DJ__) memset(__AJ__,0,SFNL); sprintf(__AJ__, "%s%d_%ld",__BJ__, __DJ__, __CJ__);
#define GET_USPD_JOURNAL_FILE_NAME(__OUT__ , __JN__) memset(__OUT__,0,SFNL); sprintf(__OUT__,"%s%d", STORAGE_PATH_EVENTS , __JN__);

#define STORAGE_USPD_DBID 0xFFFFFFFF

#define STORAGE_SEASON_CHANGE_DISABLE			(unsigned char)0
#define STORAGE_SEASON_CHANGE_ENABLE			(unsigned char)1
#define STORAGE_SEASON_WINTER					(unsigned char)0
#define STORAGE_SEASON_SUMMER					(unsigned char)1


#define METERAGE_VALID						(unsigned char)0x01
#define METERAGE_PARTIAL					(unsigned char)0x02
#define METERAGE_NOT_RIGHT					(unsigned char)0x03
#define METERAGE_NOT_PRESET					(unsigned char)0x04
#define METERAGE_EMPTY						(unsigned char)0x06
#define METERAGE_OUT_OF_ARCHIVE_BOUNDS		(unsigned char)0x07
#define METERAGE_WORTH_DTS					(unsigned char)0x08
#define METERAGE_UNEXPECTED_ANSWER			(unsigned char)0x0B
#define METERAGE_TIME_WAS_SYNCED			(unsigned char)0x0A
#define METERAGE_CALCULATED_VALUE			(unsigned char)0x0C
#define METERAGE_DUBLICATED					(unsigned char)0x40
#define METERAGE_IN_DB_MASK					(unsigned char)0x80

#define STORAGE_MAX_EVENTS_NUMBER 1000
#define STORAGE_MIN_EVENTS_NUMBER 150
#define STORAGE_EVENT_IN_DB 0x80

#define STORAGE_MAX_WORTH 60 //seconds

//time synchronisation of counters
#define STORAGE_FLAG_EMPTY		(unsigned char)0x01
#define STORAGE_WORT_ACQ		(unsigned char)0x02
#define STORAGE_NEED_TO_SYNC	(unsigned char)0x04
#define STORAGE_SYNC_PROCESS	(unsigned char)0x08
#define STORAGE_SYNC_DONE		(unsigned char)0x10
#define STORAGE_SYNC_FAIL		(unsigned char)0x20

#define STORAGE_SYNC_DATE_SET	(unsigned char)0x40
#define STORAGE_SYNC_TIME_SET	(unsigned char)0x80


#define STORAGE_MAX_TOKEN_NUMB 50
#define STORAGE_VERSION_LENGTH 32
#define STORAGE_COUNTER_STATE_WORD 5

//profile reading states
#define STORAGE_PROFILE_STATE_NOT_PROCESS   (unsigned char)0x01
#define STORAGE_PROFILE_STATE_WAITING_SEARCH_RES       (unsigned char)0x02
#define STORAGE_PROFILE_STATE_READ_BY_PTR         (unsigned char)0x03

//journal reading state
#define STORAGE_JOURNAL_R_STATE_GET_EV_NUMBER   (unsigned char)0x00
#define STORAGE_JOURNAL_R_STATE_GET_NEXT_EVENT  (unsigned char)0x01

// -------------------------------------------
// EVENTS part inclusion
// -------------------------------------------

#define EVENT_DESC_SIZE 10

//events
#define EVENT_USPD_POWER_ON							0x01
#define EVENT_APP_POWER_ON							0x02
#define EVENT_USPD_REBOOT							0x03
			#define DESC_EVENT_REBOOT_BY_SMS "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_REBOOT_BY_WUI "\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_REBOOT_BY_HLS "\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define EVENT_TIME_SYNCED							0x04
			#define DESC_EVENT_TIME_SYNCED_BY_UI			0x02
			#define DESC_EVENT_TIME_SYNCED_BY_PROTO			0x03
			#define DESC_EVENT_TIME_SYNCED_BY_SEASON		0xFE
#define EVENT_CFG_UPDATE							0x05
			#define DESC_EVENT_CFG_UPDATE_BY_SMS    "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_CFG_UPDATE_BY_WUI    "\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_CFG_UPDATE_BY_HLS    "\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_CFG_UPDATE_BY_LOCAL  "\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00"

			#define DESC_EVENT_UPDATE_CFG_DEV_RS    "\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_DEV_RF    "\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_COORD_RF  "\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_GSM       "\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_ETH       "\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_CONN      "\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_AUT       "\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_DEV_PLC   "\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_COORD_PLC "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_EXIO		"\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_CPIN		"\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_COUNTERS_BILL	"\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_UPDATE_CFG_SERVICE   "\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00"   // TODO - add to proto

#define EVENT_PLC_TABLE_START_CLEARING				0x06
#define EVENT_PLC_TABLE_STOP_CLEARING				0x07
#define EVENT_PLC_TABLE_NEW_REMOTE					0x08
#define EVENT_PLC_TABLE_DEL_REMOTE					0x09
#define EVENT_RF_TABLE_DEL_REMOTE					0x0A	//not use yet
#define EVENT_PLC_ISM_START_CODE					0x0B
#define EVENT_PLC_ISM_ERROR_CODE					0x0C
#define EVENT_PIM_INITIALIZE_ERROR					0x0D

#define EVENT_TARIFF_MAP_RECIEVED					0x0E
#define EVENT_TARIFF_MAP_WRITING					0x0F
			#define DESC_EVENT_TARIFF_WRITING_START		"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_OK		"\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_ERR_GEN	"\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_ERR_PASS	"\x00\x00\x00\x00\x31\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_ERR_TYPE	"\x00\x00\x00\x00\x32\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_ERR_FORM	"\x00\x00\x00\x00\x33\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TARIFF_WRITING_CANCELED	"\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00"

#define EVENT_COUNTER_METERAGES_FAIL				0x10
			#define DESC_EVENT_METERAGES_FAIL_DOUBLE	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_METERAGES_FAIL_SIZE		"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
			#define DESC_EVENT_METERAGES_FAIL_TIMEOUT	"\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
			#define DESC_EVENT_METERAGES_FAIL_RATIO		"\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
#define EVENT_COUNTER_METERAGES_CLEAR				0x11
#define EVENT_COUNTER_ADDED_TO_PLC_CONFIG			0x12
#define EVENT_PIO_ERROR								0x13
			#define DESC_EVENT_PIO_ERROR_GET_STATE      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01"
			#define DESC_EVENT_PIO_ERROR_SET_STATE      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02"
			#define DESC_EVENT_PIO_ERROR_EXPORT         "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03"
			#define DESC_EVENT_PIO_ERROR_UNEXPORT       "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04"
			#define DESC_EVENT_PIO_ERROR_SET_DIRECTION  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05"

#define EVENT_PSM_RULES_RECIEVED					0x14
#define EVENT_PSM_RULES_WRITING						0x15
			#define DESC_EVENT_PSM_RULES_WRITING_START		"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_OK			"\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_ERR_GEN	"\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_ERR_PASS	"\x00\x00\x00\x00\x31\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_ERR_TYPE	"\x00\x00\x00\x00\x32\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_ERR_FORM	"\x00\x00\x00\x00\x33\x00\x00\x00\x00\x00"
			#define DESC_EVENT_PSM_RULES_WRITING_CANCELED	"\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00"

#define EVENT_USR_CMNDS_RECIEVED					0x16
#define EVENT_USR_CMNDS_WRITING						0x17
			#define DESC_EVENT_USR_CMNDS_WRITING_START		"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_OK			"\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_ERR_GEN	"\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_ERR_PASS	"\x00\x00\x00\x00\x31\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_ERR_TYPE	"\x00\x00\x00\x00\x32\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_ERR_FORM	"\x00\x00\x00\x00\x33\x00\x00\x00\x00\x00"
			#define DESC_EVENT_USR_CMNDS_WRITING_CANCELED	"\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00"

#define EVENT_GSM_MODULE_INIT_ERROR					0x18
#define EVENT_GSM_MODULE_NETWORK_REGISTER_ERR		0x19
#define EVENT_GSM_MODULE_SIM_CHANGED				0x1A
#define EVENT_GSM_MODULE_PIN_INVALID				0x1B
#define EVENT_CONNECTED_TO_SERVER					0x1C
#define EVENT_DISCONNECTED_FROM_SERVER				0x1D

#define EVENT_CONFIG_ERROR							0x1F

#define EVENT_FIRMAWARE_RECIEVED					0x20	//not use yet
#define EVENT_FIRMAWARE_WRITING						0x21	//not use yet
			#define DESC_EVENT_FIRMAWARE_WRITING_START		"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_WRITING_OK			"\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_WRITING_ERR		"\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_WRITING_CANCELED	"\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_TYPE_COUNTER		"\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_TYPE_PIM			"\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00"
			#define DESC_EVENT_FIRMAWARE_TYPE_TERMINAL		"\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00"

#define EVENT_ERROR_CONFIG_PlC_COUNTERS				0x22
#define EVENT_ERROR_CONFIG_PlC_COORD				0x23

#define EVENT_EXIO_STATE_CHANGED					0x24	//not use yet
			//
			//	TODO - add event description
			//
#define EVENT_WEB_UI_LOGIN							0x25	//not use yet
			#define DESC_EVENT_LOGIN_SUCCESS				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_LOGIN_FAILURE				"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define EVENT_TRANSPARENT_MODE_ENABLE				0x26
			#define DESC_EVENT_TRANSPARENT_OPEN_OK			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TRANSPARENT_OPEN_ERR			"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00"
#define EVENT_TRANSPARENT_MODE_DISABLE				0x27
			#define DESC_EVENT_TRANSPARENT_CLOSE_PROTO		"\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TRANSPARENT_CLOSE_TIMEOUT	"\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			#define DESC_EVENT_TRANSPARENT_CLOSE_CONNECT	"\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define EVENT_RF_INITIALIZE_ERROR					0x28	//not use yet
#define EVENT_RF_PIM_START_CODE						0x29	//not use yet
			//
			//	TODO - add event description
			//
#define EVENT_RF_TABLE_NEW_REMOTE					0x2A	//not use yet
#define EVENT_RF_TABLE_START_REMOVING_REMOTE		0x2B	//not use yet
#define EVENT_PLC_TABLE_ALL_LEAVE_NET				0x2C
			#define DESC_EVENT_PLC_LEAVE_NET_ALL_COUNTERS	"\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00"
#define EVENT_PLC_PIM_CONFIG						0x2D	//not use yet
			//
			//	TODO - add event description
			//
#define EVENT_RF_INIT_ERROR							0x2E	//not use yet
			//
			//	TODO - add event description
			//
#define EVENT_UNABLE_TO_SET_ALLOW_SEASON_FLG            0x2F
#define EVENT_GETTED_SEASON_FLG				0x30

#define USPD_BOOT_FLAG "/var/log/uspdbootflag"

//counter's events
#define EVENT_OPENING_BOX						50
#define EVENT_CLOSING_BOX						51
#define EVENT_OPENING_CONTACT					52
#define EVENT_CLOSING_CONTACT					53
#define EVENT_COUNTER_SWITCH_ON					54
#define EVENT_COUNTER_SWITCH_OFF				55
#define EVENT_SYNC_TIME_HARD					56
#define EVENT_SYNC_TIME_SOFT					57
#define EVENT_CHANGE_TARIF_MAP					58
#define EVENT_SWITCH_POWER_ON					59
#define EVENT_SWITCH_POWER_OFF					60
#define EVENT_SYNC_STATE_WORD_CH				61

#define EVENT_PQP_VOLTAGE_INCREASE_F1			62	//0x00
#define EVENT_PQP_VOLTAGE_I_NORMALISE_F1		63	//0x01
#define EVENT_PQP_VOLTAGE_INCREASE_F2			64	//0x02
#define EVENT_PQP_VOLTAGE_I_NORMALISE_F2		65	//0x03
#define EVENT_PQP_VOLTAGE_INCREASE_F3			66	//0x04
#define EVENT_PQP_VOLTAGE_I_NORMALISE_F3		67	//0x05

#define EVENT_PQP_AVE_VOLTAGE_INCREASE_F1		68	//0x0C
#define EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F1	69	//0x0D
#define EVENT_PQP_AVE_VOLTAGE_INCREASE_F2		70	//0x0E
#define EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F2	71	//0x0F
#define EVENT_PQP_AVE_VOLTAGE_INCREASE_F3		72	//0x10
#define EVENT_PQP_AVE_VOLTAGE_I_NORMALISE_F4	73	//0x11

#define EVENT_PQP_VOLTAGE_DECREASE_F1			74	//0x18
#define EVENT_PQP_VOLTAGE_D_NORMALISE_F1		75	//0x19
#define EVENT_PQP_VOLTAGE_DECREASE_F2			76	//0x1A
#define EVENT_PQP_VOLTAGE_D_NORMALISE_F2		77	//0x1B
#define EVENT_PQP_VOLTAGE_DECREASE_F3			78	//0x1C
#define EVENT_PQP_VOLTAGE_D_NORMALISE_F3		79	//0x1D

#define EVENT_PQP_AVE_VOLTAGE_DECREASE_F1		80	//0x24
#define EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F1	81	//0x25
#define EVENT_PQP_AVE_VOLTAGE_DECREASE_F2		82	//0x26
#define EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F2	83	//0x27
#define EVENT_PQP_AVE_VOLTAGE_DECREASE_F3		84	//0x28
#define EVENT_PQP_AVE_VOLTAGE_D_NORMALISE_F4	85	//0x29

#define EVENT_PQP_FREQ_DEVIATION_02				86	//0x30
#define EVENT_PQP_FREQ_NORMALISE_02				87	//0x31
#define EVENT_PQP_FREQ_DEVIATION_04				88	//0x32
#define EVENT_PQP_FREQ_NORMALISE_04				89	//0x33

#define EVENT_PQP_KNOP_INCREASE_2				90	//0x38
#define EVENT_PQP_KNOP_NORMALISE_2				91	//0x39
#define EVENT_PQP_KNOP_INCREASE_4				92	//0x3A
#define EVENT_PQP_KNOP_NORMALISE_4				93	//0x3B

#define EVENT_PQP_KNNP_INCREASE_2				94	//0x3C
#define EVENT_PQP_KNNP_NORMALISE_2				95	//0x3D
#define EVENT_PQP_KNNP_INCREASE_4				96	//0x3E
#define EVENT_PQP_KNNP_NORMALISE_4				97	//0x3F

#define EVENT_PQP_KDF_INCREASE_F1				98	//0x4A
#define EVENT_PQP_KDF_NORMALISE_F1				99	//0x4B
#define EVENT_PQP_KDF_INCREASE_F2				100	//0x4C
#define EVENT_PQP_KDF_NORMALISE_F2				101	//0x4D
#define EVENT_PQP_KDF_INCREASE_F3				102	//0x4E
#define EVENT_PQP_KDF_NORMALISE_F3				103	//0x4F

#define EVENT_PQP_DDF_INCREASE_F1				104	//0x50
#define EVENT_PQP_DDF_NORMALISE_F1				105	//0x51
#define EVENT_PQP_DDF_INCREASE_F2				106	//0x52
#define EVENT_PQP_DDF_NORMALISE_F2				107	//0x53
#define EVENT_PQP_DDF_INCREASE_F3				108	//0x54
#define EVENT_PQP_DDF_NORMALISE_F3				109	//0x55




//descriptions
#define DESC_EVENT_RELE_SWITCH_OFF_BY_UI                "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_OPEN_CONTACT      "\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_OPEN_BOX          "\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES             "\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_PRE_PAY_SYS_RULES "\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_TEMPERATURE       "\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_SCEDULE           "\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_ENERGY_DAY_LIMIT  "\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define DESC_EVENT_RELE_SWITCH_OFF_BY_UNKNOWN_REASON    "\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_PQ          "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_E30         "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Eday        "\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Emnth       "\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umax        "\x00\x05\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Umin        "\x00\x06\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_OFF_BY_RULES_Imax        "\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00"

#define DESC_EVENT_RELE_SWITCH_ON_BY_UNKNOWN_REASON     "\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES              "\x84\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_UI			        "\x81\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_BUTTON_PRESSED     "\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00"

#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_PQ           "\x00\x81\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_E30          "\x00\x82\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Eday         "\x00\x83\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Emnth        "\x00\x84\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Umax         "\x00\x85\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Umin         "\x00\x86\x00\x00\x00\x00\x00\x00\x00\x00"
#define DESC_EVENT_RELE_SWITCH_ON_BY_RULES_Imax         "\x00\x87\x00\x00\x00\x00\x00\x00\x00\x00"


// journal numbers
#define EVENT_USPD_JOURNAL_NUMBER_GENERAL 0
#define EVENT_USPD_JOURNAL_NUMBER_TARIFF 1
#define EVENT_USPD_JOURNAL_NUMBER_POWER_SWITCH_MODULE 2
#define EVENT_USPD_JOURNAL_NUMBER_RF_EVENTS 3
#define EVENT_USPD_JOURNAL_NUMBER_PLC_EVENTS 4
#define EVENT_USPD_JOURNAL_NUMBER_EXTENDED_IO_CARD 5
#define EVENT_USPD_JOURNAL_NUMBER_FIRMWARE 6


#define EVENT_COUNTER_JOURNAL_NUMBER_OPEN_BOX 0
#define EVENT_COUNTER_JOURNAL_NUMBER_OPEN_CONTACT 1
#define EVENT_COUNTER_JOURNAL_NUMBER_DEVICE_SWITCH 2
#define EVENT_COUNTER_JOURNAL_NUMBER_SYNC_HARD 3
#define EVENT_COUNTER_JOURNAL_NUMBER_SYNC_SOFT 4
#define EVENT_COUNTER_JOURNAL_NUMBER_SET_TARIFF 5
#define EVENT_COUNTER_JOURNAL_NUMBER_POWER_SWITCH 6
#define EVENT_COUNTER_JOURNAL_NUMBER_STATE_WORD 7
#define EVENT_COUNTER_JOURNAL_NUMBER_PQP 8
#define EVENT_COUNTER_JOURNAL_NUMBER_UNKNOWN_JOURNAL 9

//  ------------------
//  PQP part inclusion
//  --------------------

#define PQP_STATE_READING_VOLTAGE       0
#define PQP_STATE_READING_AMPERAGE      1
#define PQP_STATE_READING_POWER         2
#define PQP_STATE_READING_POWER_P       21
#define PQP_STATE_READING_POWER_Q       22
#define PQP_STATE_READING_POWER_S       23
#define PQP_STATE_READING_FREQUENCY     3
#define PQP_STATE_READING_RATIO         4
#define PQP_STATE_READING_RATIO_COS     41
#define PQP_STATE_READING_RATIO_TAN     42


#define PQP_FLAG_VOLTAGE    (unsigned char)0x01
#define PQP_FLAG_AMPERAGE   (unsigned char)0x02
#define PQP_FLAG_POWER      (unsigned char)0x04
#define PQP_FLAG_FREQUENCY  (unsigned char)0x08
#define PQP_FLAG_RATIO      (unsigned char)0x10

// -------------------------
//   Accounts part defines
// -------------------------

#define ACOUNTS_STATUS_IN_QUEUE						0
#define ACOUNTS_STATUS_IN_PROCESS					1
#define ACOUNTS_STATUS_WRITTEN_OK					2
#define ACOUNTS_STATUS_WRITTEN_ERROR_GENERAL		3
#define ACOUNTS_STATUS_WRITTEN_ERROR_PASSWORD		31
#define ACOUNTS_STATUS_WRITTEN_ERROR_COUNTER_TYPE	32
#define ACOUNTS_STATUS_WRITTEN_ERROR_FILE_FORMAT	33
#define ACOUNTS_STATUS_WRITTEN_CANCELED				4
#define ACOUNTS_STATUS_REMOVE						99

//#define ACOUNTS_MAP_PATH "/home/root/fdb/tariff.map"
//#define ACOUNTS_TARIF_NAME_SIZE 64


//#define ACOUNTS_PSM_NAME_SIZE 64

#define ACOUNTS_TARIFF_MAP_PATH "/home/root/fdb/tariff.map"
#define ACOUNTS_TARIFF_NAME_TEAMPLATE "/home/root/fdb/tariff_"

#define ACOUNTS_PWRCTRL_MAP_PATH "/home/root/fdb/power_control_rules.map"
#define ACOUNTS_PWRCTRL_NAME_TEAMPLATE "/home/root/fdb/power_control_rules_"

#define ACOUNTS_FIRMWARE_MAP_PATH "/home/root/fdb/firmware.map"
#define ACOUNTS_FIRMWARE_NAME_TEAMPLATE "/home/root/fdb/firmware_"

#define ACOUNTS_USER_COMMANDS_MAP_PATH "/home/root/fdb/user_commands.map"
#define ACOUNTS_USER_COMMANDS_NAME_TEAMPLATE "/home/root/fdb/user_commands_"

//#define ACOUNTS_TARIFF_MAX_TOKENS   1280
#define ACOUNTS_TARIFF_MAX_TOKENS   8000
#define ACOUNTS_FILE_NAME_SIZE 64
#define ACOUNTS_MAX_FILES_NUMB 512

#define ACOUNTS_SEARCHING_BLOCK_TOKENS_NUMB 128
#define ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE 256

#define ACOUNTS_PART_TARIFF			1
#define ACOUNTS_PART_POWR_CTRL		2
#define ACOUNTS_PART_FIRMWARE		3
#define ACOUNTS_PART_USER_COMMANDS	4

//--------------------
//ExIO part defines
//--------------------

#define EXIO_DISCRET_OUTPUT_NUMB 10

#define EXIO_TASK_STATE_EMPTY			0
#define EXIO_TASK_STATE_WAIT_ANSWER		1
#define EXIO_TASK_STATE_READY			2

#define EXIO_INCOMING_BUF_LENGTH 64
#define EXIO_TIMEOUT 10

#define EXIO_CMD_DISCRET_SET_MODE			(unsigned char)0x01
#define EXIO_CMD_DISCRET_SET_STATE			(unsigned char)0x02
#define EXIO_CMD_DISCRET_GET_STATE			(unsigned char)0x03

#define EXIO_MES_DISCRET_CHANGE				(unsigned char)0x8E

#define EXIO_DISCRET_CURRENT_STATE_OFF		(unsigned char)0x00
#define EXIO_DISCRET_CURRENT_STATE_ON		(unsigned char)0x01

#define EXIO_DISCRET_DEFAULT_OFF			(unsigned char)0x00
#define EXIO_DISCRET_DEFAULT_ON				(unsigned char)0x02

#define EXIO_DISCRET_ERRORCODE_OK			(unsigned char)0x00
#define EXIO_DISCRET_ERRORCODE_ERROR		(unsigned char)0x04

#define EXIO_DISCRET_MODE_OUTPUT			(unsigned char)0x00
#define EXIO_DISCRET_MODE_INPUT				(unsigned char)0x08

#define EXIO_DISCRET_PIN_1					0x10
#define EXIO_DISCRET_PIN_2					0x20
#define EXIO_DISCRET_PIN_3					0x30
#define EXIO_DISCRET_PIN_4					0x40
#define EXIO_DISCRET_PIN_5					0x50
#define EXIO_DISCRET_PIN_6					0x60
#define EXIO_DISCRET_PIN_7					0x70
#define EXIO_DISCRET_PIN_8					0x80
#define EXIO_DISCRET_PIN_9					0x90
#define EXIO_DISCRET_PIN_10					0xA0

#define EXIO_CFG_PATH "/home/root/cfg/exio.cfg"

typedef struct dateStamp_s{
	unsigned char d;
	unsigned char m;
	unsigned char y;
} dateStamp;

typedef struct timeStamp_s{
	unsigned char h;
	unsigned char m;
	unsigned char s;
} timeStamp;

typedef struct dateTimeStamp_s{
	dateStamp d;
	timeStamp t;
} dateTimeStamp;

//exported types
typedef struct calendar_s{
	//begin of connection
	int start_hour;
	int start_min;
	int start_year;
	int start_month;
	int start_mday;
	//interval
	char ratio; // CONNECTION_INTERVAL_HOUR or CONNECTION_INTERVAL_DAY or CONNECTION_INTERVAL_WEEK
	long int interval;
	//end of connection
	BOOL ifstoptime;
	int stop_hour;
	int stop_min;

	//time of last success checking
	dateTimeStamp lastBingoDts;
} calendar_t;

typedef struct energy_s{
	unsigned int e[4];
} energy_t;

typedef struct meterage_s{
	unsigned int crc;
	dateTimeStamp dts;
	energy_t t[ MAX_TARIFF_NUMBER ];
	unsigned char status;
	unsigned int ratio;
	unsigned char delimeter;
} meterage_t;

typedef struct profile_old_s{
	unsigned int crc;
	dateTimeStamp dts;
	energy_t p;
	unsigned char status;
	unsigned int ratio;
} profile_old_t;

typedef struct profile_s{
	unsigned int crc;
	dateTimeStamp dts;
	unsigned int id;
	energy_t p;
	unsigned char status;
	unsigned int ratio;
} profile_t;

typedef struct meteregesArray_s{
	meterage_t ** meterages;
	unsigned int meteragesCount;
}meteregesArray_t;

typedef struct energiesArray_s{
	profile_t ** energies;
	unsigned int energiesCount;
}energiesArray_t;

/*----------*/

typedef struct uspdEvent_s{
	unsigned int crc;
	unsigned char evType;
	unsigned char evDesc[EVENT_DESC_SIZE];
	unsigned char status;
	dateTimeStamp dts;
}uspdEvent_t;

typedef struct uspdEventsArray_s{
	uspdEvent_t ** events;
	int eventsCount;
}uspdEventsArray_t;

/*--------*/

typedef struct serialAttributes_s
{
	unsigned int speed;
	//unsigned char parity;
	//unsigned char stopBits;
	//unsigned char bits ;
	enum SERIAL_RS_PARITY parity;
	enum SERIAL_RS_STOP_BIT stopBits;
	enum SERIAL_RS_BITS bits;
}serialAttributes_t ;

typedef struct counter_s{
	//parameters from configuration
	unsigned char type;
	BOOL autodetectionType;
	unsigned long serial;
	unsigned char serialRemote[SIZE_OF_SERIAL+1];
	unsigned char password1[PASSWORD_SIZE];
	unsigned char password2[PASSWORD_SIZE];
	unsigned char maskEnergy;
	unsigned char maskTarifs;
	unsigned char profile;
	unsigned char profileInterval;
	unsigned long dbId;
	unsigned char clear;

	unsigned char stampReread;
	BOOL useDefaultPass ;

	dateStamp mountD; //to investigates

	//parameters from readings
	unsigned char ratioIndex;
	unsigned char transformationRatios;
	//unsigned int lastGettedProfilePointer;
	//unsigned int lastRequestedProfilePointer;
	unsigned int profilePointer ;
	unsigned int profileCurrPtr ;
	char profilePtrCurrOverflow;
	unsigned int tmpProfilePointer ;
	unsigned char profileReadingState;
	unsigned char profileState;
	unsigned char clocksState;

	//meterages reading properties
	dateStamp dayMetaragesLastRequestDts;
	dateStamp monthMetaragesLastRequestDts;
	dateTimeStamp profileLastRequestDts;

	//journal reading properties
	unsigned int journalNumber;
	unsigned int eventNumber;
	unsigned int eventsTotal;
	unsigned char journalReadingState;

	//properties of dts
	dateTimeStamp currentDts; //current time and date of the counter
	int syncWorth;
	int transactionTime;
	unsigned char syncFlag;

	dateTimeStamp lastCounterTaskRequest;

	//int tariffWritingAttemptsCounter ;

	int unknownId;

	//property for writing tariff map
	//BOOL tariffMapWritingFlag;

	//flag of last task repeate needing
	int lastTaskRepeateFlag;

	//ulutchaizings for plc poll
	unsigned char pollAlternativeAlgo;
	unsigned char shamaningTryes;
	BOOL wasAged;
	time_t lastPollTime; 
	time_t lastLeaveCmd;

	serialAttributes_t serial485Attributes ;

	unsigned int lastProfileId;

	char swver1; //only for GUBANOV
	char swver2; //only for GUBANOV
} counter_t;

typedef struct counterStateWord_s
{
	unsigned char word[ STORAGE_COUNTER_STATE_WORD ];
} counterStateWord_t ;

typedef struct counterStatus_s
{
	unsigned char type;
	unsigned char profileInterval;
	unsigned int lastProfilePtr; //only for UJUK-counters
	unsigned char ratio ;

	#if REV_COMPILE_ASK_MAYK_FRM_VER
		counterStateWord_t counterStateWord ; //counter state word for UJUK-counters and TK-version for MAYAK-counters
	#else
		counterStateWord_t counterStateWord ; //only for UJUK-counters
	#endif

	unsigned char allowSeasonChange ;
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	unsigned char reserved5;
	unsigned char reserved6;
	unsigned char reserved7;
	unsigned char reserved8;
	unsigned char reserved9;
	unsigned char reserved10;
	unsigned int crc ;
} counterStatus_t ;



typedef struct countersArray_s{
	counter_t **counter;
	int countersSize;
	int currentCounterIndex;
} countersArray_t;

typedef struct gsmCpinProperties_s
{
	BOOL usage;
	unsigned int pin;
} gsmCpinProperties_t;

typedef struct neowayCmdWA_s
{
	unsigned char * cmd ;
	unsigned char * waitAnsw ;
	unsigned char ** answerBank ;
	int timeout ;
}neowayCmdWA_t;

typedef struct neowayBuffer_s
{
	unsigned char buf[GSM_INPUT_BUF_SIZE] ;
	int size ;
}neowayBuffer_t;

typedef struct neowayTask_s
{
	unsigned char * tx;
	unsigned char rx[GSM_TASK_RX_SIZE];
	int status ;
	//uint timeout ;
	unsigned char ** possibleAnswerBank ;
}neowayTask_t;

typedef struct neowayTcpReadBuf_s
{
	unsigned char buf[GSM_INPUT_BUF_SIZE];
	unsigned int size;
}neowayTcpReadBuf_t;

typedef struct neowayStatus_s
{
	BOOL power;
	BOOL simPresence ;
	int dialState;
	unsigned char currentSim ;
	unsigned int smsStorSize ;	
	BOOL serviceMode ;

	unsigned int currSock;
	unsigned int hangSock;

	neowayTask_t task ;
	neowayBuffer_t neowayBuffer ;

	//dts
	dateTimeStamp onlineDts;
	dateTimeStamp lastDataDts;
	dateTimeStamp powerOnDts;

	//statistic
	char opName[GSM_OPERATOR_NAME_LENGTH];
	char imei[GSM_IMEI_LENGTH + 1];	
	char ip[LINK_SIZE];
	int sq;
}neowayStatus_t;

typedef struct connection_s{
	//how many tries we can do with one sim before long sleep or sim change
	unsigned int simTryes;

	//server port and ip adress for connections over GPRS and ETH

	//uhov: it is possible to use domain name instead of IP-address.
	//unsigned char server[IP_SIZE];
	unsigned char server[LINK_SIZE];
	unsigned int port;

	//only for GPRS_SERVER connection mode
	unsigned char clientIp[LINK_SIZE];
	unsigned int clientPort;

	//for setting GPRS
	unsigned char apn1[LINK_SIZE];
	unsigned char apn1_pass[FIELD_SIZE];
	unsigned char apn1_user[FIELD_SIZE];
	unsigned char apn2[LINK_SIZE];
	unsigned char apn2_pass[FIELD_SIZE];
	unsigned char apn2_user[FIELD_SIZE];

	//for incoming calls in CSD
	unsigned char allowedIncomingNum1Sim1[PHONE_MAX_SIZE];
	unsigned char allowedIncomingNum2Sim1[PHONE_MAX_SIZE];
	unsigned char allowedIncomingNum1Sim2[PHONE_MAX_SIZE];
	unsigned char allowedIncomingNum2Sim2[PHONE_MAX_SIZE];

	//for outgoing calls  in CSD
	unsigned char serverPhoneSim1[PHONE_MAX_SIZE];
	unsigned char serverPhoneSim2[PHONE_MAX_SIZE];

	//priority and principle of sim usage
	unsigned char simPriniciple; //1 - only 1, 2 - only 2, 3 - use 1 then 2 , 4 - use 2 then 1

	//type of connection
	unsigned int mode; //1 - GPRS always,  2 - GPRS by calendar,
					   //3 - CSD incoming, 4 - CSD outcall by calendar,
					   //5 - ETH always,   6 - ETH by calendar

	unsigned int protocol; //always = 1

	//calendars for connections
	calendar_t calendarGsm;
	calendar_t calendarEth;

	//only for settings of eth0
	unsigned char myIp[IP_SIZE];
	unsigned char gateway[IP_SIZE];
	unsigned char mask[IP_SIZE];
	unsigned char dns1[IP_SIZE];
	unsigned char dns2[IP_SIZE];
	unsigned char eth0mode; //static or dynamic ip

	unsigned char hwaddress[ ETH_HW_SIZE + 1 ];

	unsigned char ethUpdateFlag ;
	unsigned char gsmUpdateFlag ;
	unsigned char connUpdateFlag ;
	unsigned char servUpdateFlag ;
	unsigned char servSmsUpdateFlag;

	//service properties
	unsigned char serviceIp[IP_SIZE];
	unsigned char servicePhone[PHONE_MAX_SIZE];
	unsigned int servicePort;
	unsigned int serviceInterval;
	int serviceRatio;
	unsigned char smsReportPhone[PHONE_MAX_SIZE];
	BOOL smsReportAllow;

	gsmCpinProperties_t gsmCpinProperties[2];
}connection_t;


typedef struct counterTransaction_s{
	unsigned char transactionType;
	unsigned char * command;
	unsigned char * answer;
	unsigned char result;
	unsigned int commandSize;
	unsigned int answerSize;
	unsigned int waitSize;
	time_t tStart;
	time_t tStop;
	BOOL shortAnswerIsPossible ;
	BOOL shortAnswerFlag ;

	//only for UJUK SET-4TM-01/02
	int monthEnergyRequest;
} counterTransaction_t;

typedef struct counterTask_s{
	counterTransaction_t ** transactions;
	unsigned long counterDbId;
	unsigned char counterType;

	unsigned int transactionsCount;
	unsigned int currentTransaction;
	unsigned char associatedSerial[SIZE_OF_SERIAL+1];
	unsigned int associatedId;
	int taskTag;
	unsigned char responsesCout;

	unsigned char resultCurrentReady;
	meterage_t resultCurrent;

	unsigned char resultDayReady;
	meterage_t resultDay;

	unsigned char resultMonthReady;
	meterage_t resultMonth;

	unsigned char resultProfileReady;
	profile_t resultProfile;

	time_t lastAction;

	int tariffMovedDaysTransactionsCounter;
	int tariffHolidaysTransactionCounter ;
	int tariffMapTransactionCounter ;
	int powerControlTransactionCounter ;
	int indicationWritingTransactionCounter ;

	int userCommandsTransactionCounter;

	dateTimeStamp dtsStart ;
	BOOL needToShuffle ;

	//only for UJUK SET-4TM-01/02
	int monthRequestCount;

	#if WERY_BAD_MAYK_ANSWER
	BOOL prevAnswerBad;
	#endif

} counterTask_t;

typedef struct plcNode_s{
	unsigned char serial[SIZE_OF_SERIAL+1]; //used 11 bytes
	unsigned int pid;       // 2 bytes
	unsigned int did;       // 2 bytes
	unsigned char age;
} plcNode_t;

typedef struct it700params_s{
	unsigned char strategy;
	int netSizePhisycal;
	int netSizeLogical;
	unsigned char admissionMode;
	unsigned char nodeKey[8];
} it700params_t;

typedef struct plcTask_s{
	counterTask_t ** tasks;
	unsigned int tasksCount;
} plcTask_t;

typedef struct plcBaseStation_s{
	//nodes
	plcNode_t * nodes;
	int nodesCount;
	time_t lastTableUpdate;

	//for tasks
	plcTask_t plcTask;

	//network Id
	unsigned int networkId;
	unsigned int networkIdForced;

	//settings
	it700params_t settings;

	//for stack usage
	unsigned char paramResponce;
	int paramValueI;
	unsigned char paramValueV[32];

	//for reset and online
	BOOL externalReset;
	BOOL needToReset;
	unsigned char stackResponce;

	//for sn table
	unsigned char snTableResponce;
	unsigned int entryNodeDid;
	unsigned int entryNodePid;
	unsigned char entryNodeAge;
	unsigned long entryNodeSn;

	//responces
	unsigned char responce1;
	unsigned char responce2;
	int responceTag;

}plcBaseStation_t;

typedef struct plcModem_s{
	counter_t ** countersOfModem;
	unsigned int countersCount;
	unsigned int nextCounterIndex;
	unsigned char serial[SIZE_OF_SERIAL+1];
} plcModem_t;

typedef struct counterStat_s{
	unsigned long dbid;
	unsigned long serial;
	unsigned char serialRemote[SIZE_OF_SERIAL+1];
	dateTimeStamp poolRun;
	dateTimeStamp poolEnd;
	unsigned char poolStatus;
	unsigned char pollErrorTransactionIndex;
	unsigned char pollErrorTransactionResult;
}counterStat_t;

typedef struct statisticMap_s{
	counterStat_t ** countersStat;
	unsigned int countersStatCount;
} statisticMap_t;

typedef struct smsBuffer_s {
	unsigned char phoneNumber[PHONE_MAX_SIZE];
	unsigned char textMessage[SMS_TEXT_LENGTH];

	unsigned char incomingText[SMS_TEXT_LENGTH];
	int smsStorageNumber;
}smsBuffer_t;


typedef struct objNameUndPlace_s{
	char objName[ STORAGE_PLACE_NAME_LENGTH ];
	char objPlace[ STORAGE_PLACE_NAME_LENGTH ];

} objNameUndPlace_t;

typedef struct maykTariffMD_s{
	unsigned char d;
	unsigned char m;
	unsigned char t;
}maykTariffMD_t;

typedef struct maykTariffTP_s{
	timeStamp ts;
	unsigned char tariffNumber;
}
maykTariffTP_t;

typedef struct maykTariffSTT_s{
	maykTariffTP_t tp[16];
	unsigned char tpNumb;
} maykTariffSTT_t;

typedef struct maykTariffWTT_s{
	unsigned char weekDaysStamp;
	unsigned char weekEndStamp;
	unsigned char holidayStamp;
	unsigned char shaginMindStamp;
}maykTariffWTT_t;

typedef struct maykTariffMTT_s{
	unsigned char wttNumb[12];
}maykTariffMTT_t;

typedef struct maykTariffLWJ_s{
	maykTariffMD_t md[40];
	unsigned char mdNumb;
}maykTariffLWJ_t;

typedef struct ujukHolidayArr_s{
	unsigned char data[UJUK_TARIFF_HOLIDAY_ARR_MAX_SIZE];
	unsigned char crc ;
}ujukHolidayArr_t;

typedef struct ujukMovedDaysList_s{
	unsigned int data[UJUK_TARIFF_MOVED_DAYS_LIST_MAX_SIZE];
	int dataSize ;
	unsigned char crc;
}ujukMovedDaysList_t ;


typedef struct ujukDayTariffMap_s{
	unsigned char data[UJUK_TARIFF_DAY_MAP_SIZE];
	int dataSize;
}ujukDayTariffMap_t;

typedef struct ujukMonthTariffMap_s{
	ujukDayTariffMap_t day[UJUK_TARIFF_DAY_TYPE_NUMB];
	int dayNumb;
}ujukMonthTariffMap_t;

typedef struct ujukTariffMap_s{
	ujukMonthTariffMap_t monthMap[12];
	unsigned char yearCrc;
}ujukTariffMap_t;


//typedef struct{
//	unsigned long counterDbId;
//	unsigned char status;
//	unsigned char fileName[ACOUNTS_TARIF_NAME_SIZE];
//	unsigned short crc;
//} accTarifMap_t ;

//typedef struct{
//		unsigned long counterDbId;
//		unsigned char status;
//		unsigned char fileName[ACOUNTS_PSM_NAME_SIZE];
//		unsigned short crc;
//}accPsmMap_t;

typedef struct pqpPower_s{
	int a;
	int b;
	int c;
	int sigma;
} pqpPower_t ;

typedef struct pqpRatio_s{
	int cos_ab;
	int cos_bc;
	int cos_ac;
	int tan_ab;
	int tan_bc;
	int tan_ac;
}pqpRatio_t ;


typedef struct pqpVoltage_s{
	int a;
	int b;
	int c;
}pqpVoltage_t;

typedef struct pqpAmperage_s{
	int a;
	int b;
	int c;
}pqpAmperage_t;

typedef struct powerQualityParametres_s{
	dateTimeStamp dts ;
	pqpVoltage_t U;
	pqpAmperage_t I;
	pqpPower_t P ;
	pqpPower_t Q ;
	pqpPower_t S ;
	int frequency ;
	pqpRatio_t coeff ;
} powerQualityParametres_t ;


typedef struct syncStateWord_s{
	unsigned long counterDbId ;
	int worth ;
	int transactionTime ;
	unsigned char status ;
} syncStateWord_t;

typedef struct blockParsingResult_s{
	char variable[ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE];
	char value[ACOUNTS_SEARCHING_BLOCK_FIELD_SIZE] ;
} blockParsingResult_t;

typedef struct acountsDbidsList_s
{
	unsigned long * counterDbids;
	int counterDbidsNumb;
}acountsDbidsList_t;

typedef struct acountsMap_s{
		unsigned long counterDbId;
		unsigned char status;
		char fileName[ACOUNTS_FILE_NAME_SIZE];
		unsigned int crc;
}acountsMap_t;

typedef struct mayk_PSM_Parametres_s{
		int maskOther;
		int maskPq;
		int maskE30;
		int maskEd;
		int maskEm;
		int maskUmin;
		int maskUmax;
		int maskImax;
		int maskRules;

		unsigned int limitsPq[4][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ) //second - energy (A+, A-, R+, R-);

		unsigned int limitsE30[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);

		unsigned int limitsEdT1[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT2[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT3[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT4[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT5[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT6[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT7[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdT8[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEdTS[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);

		unsigned int limitsEmT1[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT2[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT3[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT4[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT5[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT6[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT7[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmT8[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);
		unsigned int limitsEmTS[5][4]; //first index - phase (0-A, 1-B, 2-C, 3-Summ, 4-Summ for |A| & |R|) //second - energy (A+, A-, R+, R-);

		unsigned int limitsUmax[3];
		unsigned int limitsUmin[3];
		unsigned int limitsImax[3];

		int tP;
		int tMax;
		int tMin;
		int tOff;
} mayk_PSM_Parametres_t;

typedef struct maykIndicationLoop_s
{
		int length;
		unsigned char * values ;
}maykIndicationLoop_t;

typedef struct maykIndication_s
{
		int loopCounter ;
		maykIndicationLoop_t * loop ;

		unsigned char loopAutoSwitchTime ;
}maykIndication_t;

typedef struct discretCfg_s{
		int index;
		unsigned char mode;
		unsigned char defaultValue;
} discretCfg_t;

typedef struct exioCfg_s{
	discretCfg_t discretCfg[ EXIO_DISCRET_OUTPUT_NUMB ];
} exioCfg_t;

typedef struct exioTask_s{
	unsigned char cmd;
	unsigned char param[EXIO_DISCRET_OUTPUT_NUMB];
	int state;
	unsigned char answer[EXIO_DISCRET_OUTPUT_NUMB];

	//int queueFlag;
	//int intitiateCheckingFlag ;
} exioTask_t;

typedef struct quizStatistic_s
{
	unsigned long counterDbId ;
	dateTimeStamp lastCurrentDts;
	dateTimeStamp lastDayDts;
	dateTimeStamp lastMonthDts;
	dateTimeStamp lastProfileDts;

	unsigned long counterSerial;
	unsigned char counterType;
}quizStatistic_t;

typedef struct counterAutodetectionStatistic_s
{
	unsigned long dbId ;
	unsigned char type ;
}counterAutodetectionStatistic_t;

typedef struct uspdService_s
{
	dateTimeStamp lastServiceConnectDts ;
	unsigned int crc ;
}uspdService_t;

/*
typedef struct gsmTask_s
{
	unsigned char cmd[ GSM_R_BUFFER_LENGTH ];
	unsigned char * answer ;
	int answerLength;
	int status ;
	BOOL avalancheOutput;
	unsigned char ** possibleAnswerBank;
} gsmTask_t;
*/
typedef struct gubanovTariff_s
{
	int mm ;
	int hh ;
	int dd ;
	int MM ;
	int duration ;
	int realTariffIdx ;
	int timeZone ;
}gubanovTariff_t;

typedef struct gubanovHoliday_s
{
	int index ;
	int dd ;
	int MM ;
	int type ;
}gubanovHoliday_t;

typedef struct ujukTariffZone_s
{
	unsigned char zoneIndex ;
	unsigned int seasonMask ;

	timeStamp weekDayStart;
	timeStamp weekDayEnd;

	timeStamp saturdayStart;
	timeStamp saturdayEnd;

	timeStamp sundayStart;
	timeStamp sundayEnd;

	timeStamp holidayStart;
	timeStamp holidayEnd;
}ujukTariffZone_t;

typedef struct gubanov_types_s
{
	unsigned char ratioIndex;
	unsigned char profileRatioIndex;
	unsigned char counterSkillMsk;
	unsigned char waitMeterageSize;
	char swver1;
	char swver2;
	unsigned char counterType; //for unknown counters identification
}gubanov_types_t;

typedef struct gubanovIndication_s
{
	BOOL T1; //for SEB-2A.07 etc
	BOOL T2; //for SEB-2A.07 etc
	BOOL T3; //for SEB-2A.07 etc
	BOOL T4; //for SEB-2A.07 etc
	BOOL TS; //for SEB-2A.07 etc
	BOOL date; //for SEB-2A.07 etc
	BOOL time; //for SEB-2A.07 etc
	unsigned int loop0; //for PSH-3ART
	unsigned int loop1; //for PSH-3ART
						//loop2 reserved
	unsigned int loop3; //for PSH-3ART
	unsigned int loop4; //for PSH-3ART
	int duration;
}gubanovIndication_t;

typedef struct hardwareWritingCtrl_s
{
	dateTimeStamp initDts ;
	dateTimeStamp lastWritingDts ;
	unsigned int counter ;
	unsigned int crc;
}hardwareWritingCtrl_t;

typedef struct seasonProperties_s
{
	unsigned char currentSeason ;
	unsigned char allowChanging ;
}seasonProperties_t;

//exported variables
extern objNameUndPlace_t objNameUndPlace;

#if REV_COMPILE_RELEASE
	#define COMMON_STR_DEBUG(...)
	#define COMMON_BUF_DEBUG(...)
	#define COMMON_ASCII_DEBUG(...)
#else
	void COMMON_STR_DEBUG(char * str, ...);
	void COMMON_BUF_DEBUG(char * prefix, unsigned  char * buf, unsigned  int size);
	void COMMON_ASCII_DEBUG(char * prefix, unsigned char * buf, unsigned int size);
#endif



int COMMON_Initialize();

void COMMON_Sleep(long millisecs);

counterTransaction_t ** COMMON_AllocTransaction( counterTask_t ** counterTask );
int COMMON_FreeCounterTask(counterTask_t ** counterTask);
int COMMON_FreeMeteragesArray(meteregesArray_t ** mArray);
int COMMON_FreeEnergiesArray(energiesArray_t ** mArray);
int COMMON_FreeUspdEventsArray(uspdEventsArray_t ** mArray);

int COMMON_GetCurrentDts(dateTimeStamp * dts);
int COMMON_GetLastHalfHourDts(dateTimeStamp * dts, int  mode);
int COMMON_GetCurrentTs(timeStamp * ts);
int COMMON_GetDtsWorth(dateTimeStamp * dts);
int COMMON_GetDtsDiff(dateTimeStamp * dStart, dateTimeStamp * dStop, int step);
int COMMON_GetDtsDiff__Alt(dateTimeStamp * dStart, dateTimeStamp * dStop, int step);
void COMMON_ChangeDts(dateTimeStamp * dts, int operation, int step);
BOOL COMMON_DtsInRange(dateTimeStamp * dts, dateTimeStamp * from, dateTimeStamp * to);
BOOL COMMON_CheckDts( dateTimeStamp * firstDts , dateTimeStamp * secDts );
int COMMON_GetDtsByTime_t(dateTimeStamp * dts , time_t t);
int COMMON_CheckDtsForCorrect( int dd_int , int mm_int , int yyyy_int , int hh_int , int min_int);
int COMMON_CheckDtsForValid(dateTimeStamp * dts);
int COMMON_GetWeekDayByDts( dateTimeStamp * dts );
BOOL COMMON_DtsAreEqual(dateTimeStamp * da, dateTimeStamp * db);
BOOL COMMON_DtsIsLower(dateTimeStamp * from, dateTimeStamp * to);
void COMMON_GetSeasonChangingDts(unsigned char year , unsigned char season, dateTimeStamp * springDts , dateTimeStamp * autumnDts );

BOOL IS_DTS_ZERO(dateTimeStamp * dts);
BOOL IS_DTS_VALID(dateTimeStamp * dts);

unsigned char COMMON_HexToDec(unsigned char value);
unsigned int COMMON_BufToInt(unsigned char * value);
int COMMON_AsciiToInt(unsigned char * buf , int bufLength, int base);
void COMMON_DecToAscii( int value , int size , unsigned char * buf );
unsigned int COMMON_BufToShort(unsigned char * value);

int COMMON_GetRatioValueMayak(int ratioByte);
int COMMON_GetRatioValueUjuk(int ratioByte);
BOOL COMMON_CheckRatioIndexForValid(unsigned char counterType , unsigned char ratioIndex);

char * getTransactionResult(unsigned char result);
char * getErrorCode(int ErrorCode);
char * BOOL_VAL(BOOL val);
char COMMON_GetMayakRatio(int ratio);

void COMMON_GET_USPD_SN(unsigned char *p);
void COMMON_GET_USPD_SN_UL(unsigned long *retSn);
void COMMON_GET_USPD_OBJ_NAME(unsigned char *p);
void COMMON_GET_USPD_LOCATION(unsigned char *p);
void COMMON_GET_USPD_IP(char * p);
void COMMON_GET_USPD_MAC(char * p);
void COMMON_GET_USPD_MASK(char * p);
void COMMON_USPD_GET_GW(char * p);
void COMMON_USPD_GET_DNS(char * p);

void COMMON_ShiftTextDown( char * buf , int length );
void COMMON_ShiftTextUp( char * buf , int length);

void COMMON_GetWebUIPass( unsigned char * pass );
int COMMON_SetWebUIPass(unsigned char * pass );

void COMMON_CharArrayDisjunction(char * firstString , char * secondString, unsigned char * result);
void COMMMON_CreateDescriptionForConnectEvent(unsigned char * evDesc , connection_t * connection, BOOL serviceMode);
void COMMON_TranslateLongToChar(unsigned long counterDbID, unsigned char * result);

int COMMON_Translate10to2_10(int dec);
int COMMON_Translate2_10to10(int hex);
int COMMON_TranslateHexPwdToAnsi( unsigned char * startString , unsigned char * password );

int COMMON_GetDtsFromUIString(char * uiString , dateTimeStamp * dts);
int COMMON_SetTime(dateTimeStamp * dtsToSet , int * diff , unsigned int reason);

int COMMON_SetNewTimeStep1(dateTimeStamp * dtsToSet, int * timeDiff);
int COMMON_SetNewTimeStep2(unsigned char reason);
void COMMON_SetNewTimeAbort();

int COMMON_SearchBlock(char * text , int textLength, char * blockName , blockParsingResult_t ** searchResult , int * searchResultSize) ;
int COMMON_RemoveComments(char * text , int * textLength , char startCharacter ) ;
BOOL COMMON_CheckPhrasePresence( unsigned char ** searchPhrases , unsigned char * buf , int bufLength , unsigned char * foundedMessage );

int COMMON_WriteFileToFS(unsigned char * buffer , int bufferLength , unsigned char * fileName);
int COMMON_GetFileFromFS(unsigned char ** buffer , int * bufferLength ,unsigned char * fileName);
int COMMON_RemoveFileFromFS(unsigned  char * fileName );

int COMMON_ReadLn(int fd , char * buffer , int max, int * r  );
void COMMON_GET_USPD_CABLE(char *p);

int COMMON_AllocateMemory(unsigned char ** pointer, int new_size);
int COMMON_FreeMemory(unsigned char ** pointer);

int COMMON_CheckSawInMeterage(meterage_t * meterage);
int COMMON_GetStorageDepth( counter_t * counter , int storage );
int COMMON_CheckProfileIntervalForValid( unsigned char profileInterval );

#if 0
int COMMON_TranslateSpecialSmsEncodingToAscii( unsigned char * text , int * length );
#endif
void COMMON_DecodeSmsSpecialChars(char * str);
void COMMON_RemoveExcessBlanks( char * str );
int COMMON_TranslateFormatStringToArray(char * formatString , int * arrLength , unsigned char ** arr , int maxLength);
#ifdef __cplusplus
}
#endif

#endif

//EOF

