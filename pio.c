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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "common.h"
#include "pio.h"
#include "storage.h"
#include "debug.h"

//local definitions
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define PIO_BUF 64


static pthread_mutex_t pioMux = PTHREAD_MUTEX_INITIALIZER;

void PIO_Protect()
{
	pthread_mutex_lock(&pioMux);
}

void PIO_Unprotect()
{
	pthread_mutex_unlock(&pioMux);
}


// mmap gpio
// gpio.h
#if REV_COMPILE_PIO_MMAP

// includes
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

// base
#define GPIO_BASE 	0x01E26000

// page size
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// regs offsets
#define DIR0x			0x00	// 0 GPk[j] is an output.	1 GPk[j] is an input.
#define OUT_DATA0x		0x04	// 0 GPk[j] is driven low.	1 GPk[j] is driven high.
#define SET_DATA0x		0x08
#define CLR_DATA0x		0x0C
#define IN_DATA0x		0x20	// 0 GPk[j] is logic low.	1 GPk[j] is logic high.


#define SET_RIS_TRIG0x	0x24
#define CLR_RIS_TRIG0x	0x28
#define SET_FAL_TRIG0x	0x2C
#define CLR_FAL_TRIG0x	0x30
#define INTSTAT0x		0x34

// banks offsets
unsigned char banks_offset[] = {
	0x10,
	0x38,
	0x60,
	0x88,
	0xb0
};

int gpio_fd = -1;
void *map_base = NULL;
unsigned char* pBase = NULL;

int PIO_SetDirection(unsigned int gpio, unsigned int out_flag);

int PIO_Initialize() {

	DEB_TRACE(DEB_IDX_PIO)

	int res = ERROR_GENERAL;

	int gpio_fd = open( "/dev/mem", O_RDWR | O_SYNC );

	if ( gpio_fd < 0 )
	{
		printf("Cannot open /dev/mem\r\n");
		return ERROR_GENERAL;
	}

	// Map Control Module
	map_base = (unsigned long *)mmap( NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, gpio_fd, GPIO_BASE & ~MAP_MASK );
	if ( map_base == MAP_FAILED )
	{
		printf("Memory map failed.\n");
		return ERROR_GENERAL;
	}

	pBase = (unsigned char*)map_base;
/*	// Revision ID Register (REVID)
	printf( "Revision ID Register:\t%08x\r\n", *(unsigned int*)(pBase + 0) );

	// GPIO Interrupt Per-Bank Enable Register
	printf( "GPIO Interrupt Per-Bank Enable Register:\t%08x\r\n",*(unsigned int*)(pBase + 20));

	if( munmap( map_base, MAP_SIZE ) == -1) {
		printf("Memory unmap failed\r\n");
	}

	close ( gpio_fd );
*/


	//set directions of pio
	res = PIO_SetDirection(PIO_CLI_DIN, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_CLI_DCLK, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_CLI_LOAD, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_SIM_SEL, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_NOT_OE, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_POKIN, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_DSR, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_CTS, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_RTS, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_RI, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_DTR, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_PWR, PIO_OUT);
	if (res!=OK) return res;
	//res = PIO_SetDirection(PIO_485_TX_EN, PIO_OUT);
	//if (res!=OK) return res;
	res = PIO_SetDirection(PIO_PLC_RESET, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_PLC_NOT_OE, PIO_OUT);
	if (res!=OK) return res;

	//setup initial variables
	res = PIO_SetValue(PIO_CLI_DIN, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_CLI_DCLK, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_CLI_LOAD, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_SIM_SEL, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_NOT_OE, 1);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_POKIN, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_RTS, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_DTR, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_PWR, 0);
	if (res!=OK) return res;
	//res = PIO_SetValue(PIO_485_TX_EN, 0);
	//if (res!=OK) return res;
	res = PIO_SetValue(PIO_PLC_RESET, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_PLC_NOT_OE, 1);
	if (res!=OK) return res;

	COMMON_STR_DEBUG(DEBUG_PIO "PIO_Initialize() %s", getErrorCode(res));

	return res;
}


//
// gpio_set_dir
//
int PIO_SetDirection(unsigned int gpio, unsigned int out_flag) {
	DEB_TRACE(DEB_IDX_PIO)

	if ( ( gpio > 144 ) || ( gpio < 1 ) )  {
		printf( "Pio [%d] is invalid. Valid pio num is 1 ... 144\r\n", gpio );
		return ERROR_GENERAL;
	}

	gpio++;

	int pin = gpio%32;
	int bank = ( gpio - pin )/32;

	printf( "Bank = [%d] , Pin %d\t gpio %d\r\n", bank , pin, gpio );
	printf( "Dir before = %u\r\n", *((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) );

	if (out_flag == PIO_OUT)
		*((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) &=~(0x01 << ( pin  ));
	else
		*((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) |= 0x01 << ( pin );

	printf( "Dir after = %u\r\n", *((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) );

	//usleep(500000);

	return OK;
}

//
// gpio_set_value
//
int PIO_SetValue(unsigned int gpio, unsigned int value) {
	DEB_TRACE(DEB_IDX_PIO)

	if ( ( gpio > 144 ) || ( gpio < 1 ) )  {
		printf( "Pio [%d] is invalid. Valid pio are 1 ... 144\r\n", gpio );
		return ERROR_GENERAL;
	}

	gpio++;

	int pin = gpio%32;
	int bank = ( gpio - pin )/32;

	printf( "Out data before = %u\r\n", *((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) );
	if (value == 1)
		*((unsigned int*)(pBase + banks_offset[ bank ] + OUT_DATA0x )) |= 0x01 << ( pin );
	else
		*((unsigned int*)(pBase + banks_offset[ bank ] + OUT_DATA0x )) &=~(0x01 << ( pin ));
	printf( "Out data after = %u\r\n", *((unsigned int*)(pBase + banks_offset[ bank ] + DIR0x )) );

	return OK;
}

//
// gpio_get_value
//
int PIO_GetValue(unsigned int gpio, unsigned int *value) {
	DEB_TRACE(DEB_IDX_PIO)

	if ( ( gpio > 144 ) || ( gpio < 1 ) )  {
		printf( "Pio [%d] is invalid. Valid pio are 1 ... 144\r\n", gpio );
		return ERROR_GENERAL;
	}

	gpio++;

	int pin = gpio%32;
	int bank = ( gpio - pin )/32;

	printf( "Bank = [%d] , Pin %d\t gpio %d \t lav: %u \r\n", bank , pin, gpio,*((unsigned int*)(pBase + banks_offset[ bank ] + IN_DATA0x )) );

	*value = *((unsigned int*)(pBase + banks_offset[ bank ] + IN_DATA0x )) & ((unsigned int)(0x01 << ( pin ) ));

	return OK;
}

#else

//
// General input output initialization
//

int PIO_Initialize() {

	DEB_TRACE(DEB_IDX_PIO)

	int res = ERROR_GENERAL;

	//un-export pio pins
	res = PIO_Unexport(PIO_CLI_DIN);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_CLI_DCLK);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_CLI_LOAD);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_SIM_SEL);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_NOT_OE);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_POKIN);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_DSR);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_CTS);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_RTS);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_RI);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_DTR);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_GSM_PWR);
	if (res!=OK) return res;
	//res = PIO_Unexport(PIO_485_TX_EN);
	//if (res!=OK) return res;
	res = PIO_Unexport(PIO_PLC_RESET);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_PLC_NOT_OE);
	if (res!=OK) return res;
	res = PIO_Unexport(PIO_PLC_POWER);
	if (res!=OK) return res;

	//export pio pins
	res = PIO_Export(PIO_CLI_DIN);
	if (res!=OK) return res;
	res = PIO_Export(PIO_CLI_DCLK);
	if (res!=OK) return res;
	res = PIO_Export(PIO_CLI_LOAD);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_SIM_SEL);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_NOT_OE);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_POKIN);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_DSR);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_CTS);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_RTS);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_RI);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_DTR);
	if (res!=OK) return res;
	res = PIO_Export(PIO_GSM_PWR);
	if (res!=OK) return res;
	//res = PIO_Export(PIO_485_TX_EN);
	//if (res!=OK) return res;
	res = PIO_Export(PIO_PLC_RESET);
	if (res!=OK) return res;
	res = PIO_Export(PIO_PLC_NOT_OE);
	if (res!=OK) return res;
	res = PIO_Export(PIO_PLC_POWER);
	if (res!=OK) return res;

	//set directions of pio
	res = PIO_SetDirection(PIO_CLI_DIN, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_CLI_DCLK, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_CLI_LOAD, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_SIM_SEL, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_NOT_OE, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_POKIN, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_DSR, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_CTS, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_RTS, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_RI, PIO_IN);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_DTR, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_GSM_PWR, PIO_OUT);
	if (res!=OK) return res;
	//res = PIO_SetDirection(PIO_485_TX_EN, PIO_OUT);
	//if (res!=OK) return res;
	res = PIO_SetDirection(PIO_PLC_RESET, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_PLC_NOT_OE, PIO_OUT);
	if (res!=OK) return res;
	res = PIO_SetDirection(PIO_PLC_POWER, PIO_OUT);
	if (res!=OK) return res;

	//setup initial variables
	res = PIO_SetValue(PIO_CLI_DIN, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_CLI_DCLK, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_CLI_LOAD, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_SIM_SEL, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_NOT_OE, 1);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_POKIN, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_RTS, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_DTR, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_GSM_PWR, 0);
	if (res!=OK) return res;
	//res = PIO_SetValue(PIO_485_TX_EN, 0);
	//if (res!=OK) return res;
	res = PIO_SetValue(PIO_PLC_RESET, 0);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_PLC_NOT_OE, 1);
	if (res!=OK) return res;
	res = PIO_SetValue(PIO_PLC_POWER, 0);
	if (res!=OK) return res;

	COMMON_STR_DEBUG(DEBUG_PIO "PIO_Initialize() %s", getErrorCode(res));

	return res;
}

//
// gpio_set_dir
//
int PIO_SetDirection(unsigned int gpio, unsigned int out_flag) {
	DEB_TRACE(DEB_IDX_PIO)

	PIO_Protect();

	int fd, len;
	char buf[PIO_BUF];

	len = snprintf(buf, sizeof (buf), SYSFS_GPIO_DIR "/gpio%d/direction", gpio);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");

		char desc[ EVENT_DESC_SIZE ];
		memset(desc , 0 , EVENT_DESC_SIZE);

		desc[0] = (char)( ( gpio & 0xFF000000) >> 24 );
		desc[1] = (char)( ( gpio & 0xFF0000) >> 16 );
		desc[2] = (char)( ( gpio & 0xFF00) >> 8 );
		desc[3] = (char) ( gpio & 0xFF) ;

		unsigned char evDesc[EVENT_DESC_SIZE];
		COMMON_CharArrayDisjunction( DESC_EVENT_PIO_ERROR_SET_DIRECTION , desc, evDesc);
		STORAGE_JournalUspdEvent( EVENT_PIO_ERROR , evDesc );

		PIO_Unprotect();
		PIO_ErrorHandler();
		return ERROR_PIO;
	}

	if (out_flag == PIO_OUT)
		write(fd, "out", 4);
	else
		write(fd, "in", 3);

	close(fd);

	usleep(500000);

	PIO_Unprotect();

	return OK;
}

//
// gpio_set_value
//
int PIO_SetValue(unsigned int gpio, unsigned int value) {
	DEB_TRACE(DEB_IDX_PIO)

	PIO_Protect();
	int fd, len;
	char buf[PIO_BUF];

	len = snprintf(buf, sizeof (buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-value");

		char desc[ EVENT_DESC_SIZE ];
		memset(desc , 0 , EVENT_DESC_SIZE);

		desc[0] = (char)( ( gpio & 0xFF000000) >> 24 );
		desc[1] = (char)( ( gpio & 0xFF0000) >> 16 );
		desc[2] = (char)( ( gpio & 0xFF00) >> 8 );
		desc[3] = (char) ( gpio & 0xFF) ;

		unsigned char evDesc[EVENT_DESC_SIZE];
		COMMON_CharArrayDisjunction( DESC_EVENT_PIO_ERROR_SET_STATE , desc, evDesc );
		STORAGE_JournalUspdEvent( EVENT_PIO_ERROR , evDesc );

		PIO_Unprotect();
		PIO_ErrorHandler();

		return ERROR_PIO;
	}

	if (value == 1)
		write(fd, "1\0", 2);
	else
		write(fd, "0\0", 2);

	close(fd);

	PIO_Unprotect();
	return OK;
}

//
// gpio_get_value
//
int PIO_GetValue(unsigned int gpio, unsigned int *value) {
	DEB_TRACE(DEB_IDX_PIO)

	PIO_Protect();
	int fd, len;
	char buf[PIO_BUF];
	char ch;

	len = snprintf(buf, sizeof (buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("gpio/get-value");

		char desc[ EVENT_DESC_SIZE ];
		memset(desc , 0 , EVENT_DESC_SIZE);

		desc[0] = (char)( ( gpio & 0xFF000000) >> 24 );
		desc[1] = (char)( ( gpio & 0xFF0000) >> 16 );
		desc[2] = (char)( ( gpio & 0xFF00) >> 8 );
		desc[3] = (char) ( gpio & 0xFF) ;

		unsigned char evDesc[EVENT_DESC_SIZE];
		COMMON_CharArrayDisjunction( DESC_EVENT_PIO_ERROR_GET_STATE , desc , evDesc);
		STORAGE_JournalUspdEvent( EVENT_PIO_ERROR , evDesc );

		PIO_Unprotect();
		PIO_ErrorHandler();

		return ERROR_PIO;
	}

	read(fd, &ch, 1);

	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}

	close(fd);

	PIO_Unprotect();
	return OK;
}

#endif



//
// Handler for key 1
//

int PIO_Hanlde_Key1() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for key 1 signal

	return OK;
}

//
// Handler for key 2
//

int PIO_Hanlde_Key2() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for key 2 signal

	return OK;
}

//
// Handler for key 2
//

int PIO_Hanlde_Key3() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for key 3 signal

	return OK;
}

//
// Handler for supervisor LLO
//

int PIO_Hanlde_LLO() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for llo signal

	return OK;
}

//
// Handler for supervisor PFO
//

int PIO_Hanlde_PFO() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for llo signal

	return OK;
}

//
// Handler for RTC IRQ
//

int PIO_Hanlde_RTC() {
	DEB_TRACE(DEB_IDX_PIO)

	//handler for rtc irq signal

	return OK;
}

//
// gpio_export
//
int PIO_Export(unsigned int gpio) {
	DEB_TRACE(DEB_IDX_PIO)

	PIO_Protect();

	int fd, len;
	char buf[PIO_BUF];

	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");

		char desc[ EVENT_DESC_SIZE ];
		memset(desc , 0 , EVENT_DESC_SIZE);

		desc[0] = (char)( ( gpio & 0xFF000000) >> 24 );
		desc[1] = (char)( ( gpio & 0xFF0000) >> 16 );
		desc[2] = (char)( ( gpio & 0xFF00) >> 8 );
		desc[3] = (char) ( gpio & 0xFF) ;

		unsigned char evDesc[EVENT_DESC_SIZE];
		COMMON_CharArrayDisjunction( DESC_EVENT_PIO_ERROR_EXPORT , desc, evDesc );
		STORAGE_JournalUspdEvent( EVENT_PIO_ERROR , evDesc);

		PIO_Unprotect();
		PIO_ErrorHandler();

		return ERROR_PIO;
	}

	len = snprintf(buf, sizeof (buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);

	usleep(500000);

	PIO_Unprotect();

	return OK;
}

//
// gpio_unexport
//
int PIO_Unexport(unsigned int gpio) {
	DEB_TRACE(DEB_IDX_PIO)

	PIO_Protect();

	int fd, len;
	char buf[PIO_BUF];

	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");

		char desc[ EVENT_DESC_SIZE ];
		memset(desc , 0 , EVENT_DESC_SIZE);

		desc[0] = (char)( ( gpio & 0xFF000000) >> 24 );
		desc[1] = (char)( ( gpio & 0xFF0000) >> 16 );
		desc[2] = (char)( ( gpio & 0xFF00) >> 8 );
		desc[3] = (char) ( gpio & 0xFF) ;

		unsigned char evDesc[EVENT_DESC_SIZE];
		COMMON_CharArrayDisjunction( DESC_EVENT_PIO_ERROR_UNEXPORT , desc , evDesc );
		STORAGE_JournalUspdEvent( EVENT_PIO_ERROR , evDesc );

		PIO_Unprotect();
		PIO_ErrorHandler();

		return ERROR_PIO;
	}

	len = snprintf(buf, sizeof (buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);

	usleep(500000);

	PIO_Unprotect();

	return OK;
}


//
// gpio_set_edge
//

int Pio_SetEdge(unsigned int gpio, char *edge) {
	DEB_TRACE(DEB_IDX_PIO)

	int fd, len;
	char buf[PIO_BUF];

	len = snprintf(buf, sizeof (buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);

	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-edge");

		PIO_ErrorHandler();

		return ERROR_PIO;
	}

	write(fd, edge, strlen(edge) + 1);
	close(fd);
	return OK;
}

//
// gpio_fd_open
//

int Pio_Open(unsigned int gpio) {
	DEB_TRACE(DEB_IDX_PIO)

	int fd, len;
	char buf[PIO_BUF];

	len = snprintf(buf, sizeof (buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

	fd = open(buf, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {

		perror("gpio/fd_open");

		PIO_ErrorHandler();
	}
	return fd;
}

//
// gpio_fd_close
//

int Pio_Close(int fd) {
	DEB_TRACE(DEB_IDX_PIO)

	return close(fd);
}

//
//
//

int PIO_RebootUspd()
{
	DEB_TRACE(DEB_IDX_PIO)

	int res = ERROR_GENERAL;
	printf(" >>>>>>REBOOT TRY TO UNEXPORT PIO\n");

	COMMON_Sleep( 10 * SECOND );

	//push pio[5]11 down for reboot
	if (PIO_Unexport(PIO_REBOOT) != OK )
	{
		printf("REBOOT ERROR -  Can't unexport\n");
		return res;
	}
	if (PIO_Export(PIO_REBOOT) != OK )
	{
		printf("REBOOT ERROR -  Can't export\n");
		return res;
	}

	if(PIO_SetDirection(PIO_REBOOT, PIO_OUT) !=OK )
	{
		printf("REBOOT ERROR -  Can't set direction\n");
		return res;
	}

	if (PIO_SetValue(PIO_REBOOT, 0) != OK )
	{
		printf("REBOOT ERROR -  can't reboot\n");
		return res;
	}

	res = OK ;
	printf("REBOOT OK\n");


	return res ;
}

//
//
//

void PIO_ErrorHandler()
{
	unsigned int counterValue = 0 ;

	int fd = open(PIO_ERROR_COUNTER , O_RDWR);
	if ( fd >= 0 )
	{
		//counter is exist
		//increment it and check

		if ( read( fd , &counterValue , sizeof( unsigned int ) ) == sizeof( unsigned int ) )
		{
			lseek( fd , 0 , SEEK_SET );
			++counterValue ;
			if ( counterValue >= PIO_MAX_ERROR_COUNTER_VALUE )
			{
				unlink( PIO_ERROR_COUNTER );
				system("reboot");
			}
			else
			{
				write( fd , &counterValue , sizeof( unsigned int ) );
			}
		}
		else
		{
			lseek( fd , 0 , SEEK_SET );
			counterValue = 0 ;
			write( fd , &counterValue , sizeof( unsigned int ) );
		}

		close( fd );
	}
	else
	{
		counterValue = 1 ;
		// counter is not exist
		fd = open( PIO_ERROR_COUNTER , O_CREAT | O_WRONLY , 0777);
		if ( fd >= 0 )
		{
			write( fd , &counterValue , sizeof( unsigned int ) );
			close( fd );
		}
	}

	return ;
}

//EOF
