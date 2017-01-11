/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/

#ifndef __ZIF_UNKNOWN_H
#define __ZIF_UNKNOWN_H

#include "boolType.h"
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

int UNKNOWN_AskForMayk(counter_t * counter, counterTransaction_t ** transaction);
int UNKNOWN_AskForUjuk(counter_t * counter, counterTransaction_t ** transaction);
int UNKNOWN_AskForGubanov(counter_t * counter, counterTransaction_t ** transaction);

int UNKNOWN_CreateCounterTask(counter_t * counter, counterTask_t ** counterTask);
int UNKNOWN_SaveTaskResults(counterTask_t * counterTask);
#ifdef __cplusplus
}
#endif

#endif

//EOF
