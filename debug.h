/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_DEBUG_H
#define __ZIF_DEBUG_H

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REV_COMPILE_FAULT_HANDLE	 1 //Enable to check segmentation fault
#define REV_COMPILE_SVISOR_DATA_SENDING			1

#define DEB_IDX_PLC			0
#define DEB_IDX_GSM			1
#define DEB_IDX_POOL		2
#define DEB_IDX_MICRON		3
#define DEB_IDX_STORAGE		4
#define DEB_IDX_CLI			5
#define DEB_IDX_PIO			6
#define DEB_IDX_CONNECTION	7
#define DEB_IDX_ETH			8
#define DEB_IDX_SERIAL		9
#define DEB_IDX_MAYAK		10
#define DEB_IDX_UJUK		11
#define DEB_IDX_GUBANOV		12
#define DEB_IDX_UNKNOWN		13
#define DEB_IDX_EXIO		14
#define DEB_IDX_MAIN		15
#define DEB_IDX_COMMON		16
#define DEB_IDX_ACCOUNTS	17
#define DEB_TOTAL			18

#define DEB_TRACE_LEN 256


#if REV_COMPILE_FAULT_HANDLE
extern char traceStr[DEB_TOTAL][DEB_TRACE_LEN];
extern pthread_mutex_t traceMux;
#endif

#if REV_COMPILE_SVISOR_DATA_SENDING
extern int svisorFd ;
enum THREADS_IDX{
	THREAD_MAIN,
	THREAD_CLI,
	THREAD_GSM_LISTENER,
	THREAD_GSM,
	THREAD_ETH,
	THREAD_CONN_MANAGER,
	THREAD_EXIO,
	THREAD_EXIO_LISTENER,
	THREAD_PLC,
	THREAD_POOL_PLC,
	THREAD_POOL_485
	//THREAD_UD,			// do not use
	//THREAD_UD_CLIENT		// do not use
};
#define SVISOR_THREAD_SEND(___T___) unsigned char _tmpbuf=(unsigned char)___T___; write( svisorFd , &_tmpbuf , 1 );
#else
#define SVISOR_THREAD_SEND(___T___)
#endif

#if REV_COMPILE_FAULT_HANDLE
	#define DEB_TRACE(___N___) pthread_mutex_lock(&traceMux); sprintf(traceStr[___N___],"line:%d:%s",__LINE__,__FUNCTION__); pthread_mutex_unlock(&traceMux);
#else
	#define DEB_TRACE( ___N___ )
#endif

void DEBUG_InitTrace();
void DEBUG_PrintTraceStrs();
void DEBUG_SegFaultHandle(int signum);

#ifdef __cplusplus
}
#endif

#endif // __ZIF_DEBUG_H
