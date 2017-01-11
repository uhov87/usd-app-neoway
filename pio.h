/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_PIO_H
#define __ZIF_PIO_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

//exported definitions
//all in "common.h"

//exported types
//all in "common.h"

//exported variables

//exported functions


#if REV_COMPILE_PIO_MMAP
int PIO_Initialize();
int PIO_SetValue(unsigned int gpio, unsigned int value);
int PIO_GetValue(unsigned int gpio, unsigned int *value);
int PIO_RebootUspd();
#else

int PIO_Initialize();
int PIO_Unexport(unsigned int gpio);
int PIO_Export(unsigned int gpio);
int PIO_SetDirection(unsigned int gpio, unsigned int out_flag);
int PIO_SetValue(unsigned int gpio, unsigned int value);
int PIO_GetValue(unsigned int gpio, unsigned int *value);
int Pio_SetEdge(unsigned int gpio, char *edge);
int Pio_Open(unsigned int gpio);
int Pio_Close(int fd);
int PIO_RebootUspd();
void PIO_ErrorHandler();

#endif

#ifdef __cplusplus
}
#endif

#endif

//EOF

