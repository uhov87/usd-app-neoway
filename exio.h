#ifndef __ZIF__EXIO_H__
#define __ZIF__EXIO_H__

#ifdef __cplusplus
extern "C" {
#endif

int EXIO_Initialize();
int EXIO_CheckIncomingBufferForValidAnswer( unsigned char * buffer , int bufferLength , int * startMessagePosition , int * messageLength );
int EXIO_ExecutTask( unsigned char * buffer , int bufferLength );
int EXIO_HandleAsyncMessage( unsigned char * buffer , int bufferLength );

int EXIO_AddTask( unsigned char cmd , unsigned char * param , int paramLength );
void EXIO_ClearTask();
int EXIO_WaitTaskAnswer(int timeout , unsigned char * answer , int maxAnswerLength);

BOOL EXIO_CheckConfig( unsigned char * currentState );
int EXIO_SortParamArray( unsigned char * paramArray , int paramArrayLength );

int EXIO_RewriteDefault();
BOOL EXIO_CheckExioReadyToCommand();
int EXIO_CommandGetState( unsigned char * answerBuffer );
int EXIO_CommandSetMode(int discretOutputIndex , unsigned char mode , unsigned char defaultValue);
int EXIO_CommandSetState( int discretOutputIndex , unsigned char state );

#ifdef __cplusplus
}
#endif

#endif // __ZIF__EXIO_H__
