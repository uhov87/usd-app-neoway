/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_SERIAL_H
#define __ZIF_SERIAL_H

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
int SERIAL_Initialize();
int SERIAL_OPEN_PLC();
int SERIAL_OPEN_RS485();
int SERIAL_OPEN_GSM();
int SERIAL_CLOSE_GSM();
int SERIAL_OPEN_EXIO();

int SERIAL_SetupAttributesRs485( serialAttributes_t * serialAttributes);

int SERIAL_Check(int port, int * len);
int SERIAL_Write(int port, unsigned char * buf, int size, int * write);
int SERIAL_Read(int port, unsigned char * buf, int size, int * read);

void SERIAL_PLC_FLUSH();


#ifdef __cplusplus
}
#endif

#endif

//EOF

