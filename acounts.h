#ifndef __ZIF_ACOUNTS_H
#define __ZIF_ACOUNTS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

int ACOUNT_AddNew(char * buffer , int bufferLength , acountsDbidsList_t * acountsDbidsList , int part);
int ACOUNT_Remove(acountsDbidsList_t * acountsDbidsList , int part);
int ACOUNT_StatusSet( unsigned long counterDbId , int status , int part );
int ACOUNT_StatusGet( unsigned long counterDbId , int * status , int part );
int ACOUNT_FindFreeName( char * fileName , int part );
int ACOUNT_Get( unsigned long counterDbId , unsigned char ** buffer, int * bufferLength , int part );
int ACOUNT_GetMap( acountsMap_t ** acountsMap , int * acountsMapLength , int part);
int ACOUNT_SaveMap( acountsMap_t * acountsMap , int acountsMapLength , int part);
int ACOUNT_GetOnceMapCrc( acountsMap_t * acountsMap , unsigned int * crc );
int ACOUNT_FindOverlapInMap( acountsMap_t * acountMap , int acountMapLength , char * fileName );

#ifdef __cplusplus
}
#endif

#endif

//EOF
