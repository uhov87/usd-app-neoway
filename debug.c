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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "debug.h"

#if REV_COMPILE_FAULT_HANDLE
char traceStr[DEB_TOTAL][DEB_TRACE_LEN];
pthread_mutex_t traceMux = PTHREAD_MUTEX_INITIALIZER;
#endif

#if REV_COMPILE_SVISOR_DATA_SENDING
int svisorFd = 0 ;
#endif

#if REV_COMPILE_FAULT_HANDLE
char * getTraceName(int idx){
	switch (idx){
		case DEB_IDX_PLC:
			return "         PLC";
		case DEB_IDX_GSM:
			return "         GSM";
		case DEB_IDX_POOL:
			return "        POOL";
		case DEB_IDX_MICRON:
			return "      MICRON";
		case DEB_IDX_STORAGE:
			return "     STORAGE";
		case DEB_IDX_CLI:
			return "         CLI";
		case DEB_IDX_PIO:
			return "         PIO";
		case DEB_IDX_CONNECTION:
			return "  CONNECTION";
		case DEB_IDX_ETH:
			return "         ETH";
		case DEB_IDX_SERIAL:
			return "      SERIAL";
		case DEB_IDX_MAYAK:
			return "        MAYK";
		case DEB_IDX_UJUK:
			return "        UJUK";
		case DEB_IDX_GUBANOV:
			return "     GUBANOV";
		case DEB_IDX_UNKNOWN:
			return "     UNKNOWN";
		case DEB_IDX_EXIO:
			return "        EXIO";
		case DEB_IDX_MAIN:
			return "        MAIN";
		case DEB_IDX_COMMON:
			return "      COMMON";
		case DEB_IDX_ACCOUNTS:
			return "    ACCOUNTS";
		default:
			return "NOT DEFINDED";
	}
}
#endif

//
//
//

void DEBUG_InitTrace(){

	#if REV_COMPILE_FAULT_HANDLE
	int dIndex = 0;

	signal(SIGSEGV, DEBUG_SegFaultHandle );

	for (;dIndex<DEB_TOTAL; dIndex++)
	{
		memset(traceStr[dIndex],0, DEB_TRACE_LEN);
	}

	#endif

	#if REV_COMPILE_SVISOR_DATA_SENDING
	svisorFd = open(STORAGE_SUPEVISOR_FIFO_PATH , O_WRONLY | O_NONBLOCK);
	if ( svisorFd < 0 )
	{
		perror("Svisor opening error");
		EXIT_PATTERN;
	}
	#endif
}

//
//
//

void DEBUG_PrintTraceStrs(){
#if REV_COMPILE_FAULT_HANDLE

	int dIndex = 0;

	pthread_mutex_lock(&traceMux);

	fprintf ( stderr , "-----------------------------------------------------------------------------\r\n");
	fprintf ( stderr ,"TRACE IS:\r\n");
	fprintf ( stderr ,"-----------------------------------------------------------------------------\r\n");

	for (;dIndex<DEB_TOTAL; dIndex++){
		fprintf (stderr ,"%s: %s\r\n", getTraceName(dIndex), traceStr[dIndex]);
	}

	fprintf ( stderr ,"-----------------------------------------------------------------------------\r\n");
	fflush(stdout);

	pthread_mutex_unlock(&traceMux);

#endif
}

//
//
//

void DEBUG_SegFaultHandle(int signum){
	DEBUG_PrintTraceStrs();

	signal(signum, SIG_DFL);
	kill(getpid(), signum);
}
