/*
	application code v.1
	for uspd micron 2.0x

	2012 - January
	NPO Frunze
	RUSSIA NIGNY NOVGOROD

	OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>



#include "common.h"
#include "connection.h"
#include "eth.h"
#include "cli.h"
#include "storage.h"
#include "debug.h"

static int ethState = CONNECTION_CLOSE;
unsigned int waitEthCount = 0;

static int listenFd = 0;
static int fd_eth0=0;
static time_t timeOfConnection;
static time_t timeOfLastConnectionAttempt;

static int ETH_ConnectionCounterIncrement(void);
void ETH_removingCommentsFromConfigFile( char ** buf , int * size );

dateTimeStamp waitingIncCallDts;

//static BOOL ethLastServiceAttempt = TRUE ;
BOOL serviceMode = FALSE ;

//
// Process eth state machine
//
int ETH_ConectionDo(connection_t * connection)
{

	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL;

	switch (ethState)
	{
		case CONNECTION_CLOSE:
		{
			#if 0
			switch (connection->mode)
			{
				case CONNECTION_ETH_ALWAYS:
				{
					COMMON_STR_DEBUG( DEBUG_ETH "ETH: Closed -> Try to open");
					ethState = CONNECTION_ASK;
					res = OK;
				}
				break;

			/*	case CONNECTION_PASSIVE:
					res = ETH_OpenListener(connection);
					if (res == OK)
						ethState = CONNECTION_READY;
					break;
			*/

				case CONNECTION_ETH_CALENDAR:
				{
					res = COMMON_CheckTimeToConnect(&connection->calendarEth);
					if (res == OK) {
						COMMON_STR_DEBUG( DEBUG_ETH "ETH: Closed -> Try to open by calendar time");
						ethState = CONNECTION_ASK;
					}
					if (res != OK) {
						sleep(TIME_BETWEEN_CHECK);
					}
				}
				break;

				default:
				{
					COMMON_Sleep( SECOND );
				}
				break;
			} //switch ( connection->mode )
			#endif

			switch( connection->mode )
			{
				case CONNECTION_ETH_ALWAYS:
				case CONNECTION_ETH_CALENDAR:
				case CONNECTION_ETH_SERVER:
				{
					if ( CONNECTION_CheckTimeToConnect(&serviceMode) == TRUE )
					{
						COMMON_STR_DEBUG( DEBUG_ETH "ETH: Closed -> Try to open. Service mode is [%d]" , serviceMode );
						ethState = CONNECTION_ASK;

						if ((connection->mode == CONNECTION_ETH_SERVER) && (serviceMode == FALSE))
						{
							memset(&waitingIncCallDts, 0  , sizeof(dateTimeStamp));
							int setListenerStatus = ETH_SetListener(connection);
							if ( setListenerStatus != OK )
							{
								ethState = CONNECTION_CLOSE ;
								COMMON_Sleep(SECOND);
							}
						}
					}
					else
						COMMON_Sleep(10);
				}
				break;

				default:
				{
					COMMON_Sleep( SECOND );
				}
			}//switch ( connection->mode )
		}
		break;

		case CONNECTION_ASK:
			COMMON_STR_DEBUG( DEBUG_ETH "ETH: Try to open");
			res = ETH_Open(connection);
			if (res == OK) {
				COMMON_STR_DEBUG( DEBUG_ETH "ETH: opened ok, connected...");

				CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_CLEAR_ERROR);

				ethState = CONNECTION_READY;

				CONNECTION_SetLastSessionProtoTrFlag(FALSE);				
			}
			else if (res == ERROR_NO_DATA_YET)
			{
				//todo: check time

				if ( COMMON_CheckDtsForValid(&waitingIncCallDts) != OK )
				{
					COMMON_GetCurrentDts(&waitingIncCallDts);
				}
				else
				{
					dateTimeStamp currentDts;
					COMMON_GetCurrentDts(&currentDts);

					int diff = COMMON_GetDtsDiff__Alt(&waitingIncCallDts, &currentDts, BY_SEC);
					if ( (diff > TIMEOUT3) || (diff < 0))
					{
						COMMON_STR_DEBUG( DEBUG_ETH "ETH: wating for accepting too long...");
						close(listenFd);
						ethState = CONNECTION_CLOSE ;
					}
				}

				COMMON_Sleep(SECOND);
			}
			else
			{

				if(res == ERROR_NO_CONNECTION_PARAMS)
				{
					CLI_SetErrorCode(CLI_INVALID_CONNECTION_PARAMAS, CLI_SET_ERROR);
				}

				COMMON_STR_DEBUG( DEBUG_ETH "ETH: open error, go wait some time");

				ethState = CONNECTION_WAIT;
			}
			break;

		case CONNECTION_READY:
			//COMMON_STR_DEBUG( DEBUG_ETH "ETH: Connected");
			res = ETH_CheckTimings(connection);
			if (res == ERROR_TIME_IS_OUT)
			{				
				STORAGE_EVENT_DISCONNECTED(0x01);


				COMMON_STR_DEBUG( DEBUG_ETH "ETH: connection timeouted, close eth connection, wait...");
				ethState = CONNECTION_NEED_TO_CLOSE;
			}
			else
			{
				COMMON_Sleep(SECOND);
			}
			break;

		case CONNECTION_NEED_TO_CLOSE:
			COMMON_STR_DEBUG( DEBUG_ETH "ETH: Need to close...");
			ethState = CONNECTION_WAIT;			

			ETH_Close(connection);
			COMMON_STR_DEBUG( DEBUG_ETH "ETH: Closed, go wait some time");
			break;

		case CONNECTION_WAIT:
			//COMMON_STR_DEBUG( DEBUG_ETH "ETH: Wait");
			COMMON_Sleep(DELAY_CONNECTION_WAIT);
			waitEthCount++;
			if (waitEthCount > TIMEOUT2){
				waitEthCount = 0;
				ethState = CONNECTION_CLOSE;
			}
			break;

		default:
			break;
	}

	return res;
}

//
//
//

//
// Process eth open connection
//
int ETH_Open(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH);

	if ((connection->mode == CONNECTION_ETH_SERVER) && (serviceMode == FALSE) )
	{
		struct sockaddr_in cli_addr;
		int clilen = sizeof(cli_addr);

		fd_eth0 = accept(listenFd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);

		if ( fd_eth0 < 0 )
			return ERROR_NO_DATA_YET;
		else
		{
			COMMON_STR_DEBUG( DEBUG_ETH "ETH accept connection from %s:%u\n", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port );

			snprintf( (char *)connection->clientIp, LINK_SIZE , "%s", inet_ntoa(cli_addr.sin_addr) );
			connection->clientPort = cli_addr.sin_port ;

			unsigned char evDesc[EVENT_DESC_SIZE];
			COMMMON_CreateDescriptionForConnectEvent(evDesc , connection , serviceMode );
			STORAGE_JournalUspdEvent( EVENT_CONNECTED_TO_SERVER , evDesc );

			time(&timeOfLastConnectionAttempt);
			time(&timeOfConnection);
			return OK ;
		}
	}

	//open socket to server as client
	int res = ERROR_GENERAL;

	//creating socket
	fd_eth0=socket(AF_INET,SOCK_STREAM,0);
	if (fd_eth0==-1)
	{
		res=ERROR_MEM_ERROR;
		return res;
	}

//	fcntl(fd_eth0, F_SETFL, O_NONBLOCK);

	char ip[IP_SIZE];
	memset(ip , 0 , IP_SIZE);
	unsigned int port = 0 ;

	struct sockaddr_in serv_addr;
	struct hostent *server;

	if ( serviceMode == TRUE )
	{
		#if REV_COMPILE_STASH_NZIF_IP
			//
			// change if 212.67.9.43:10100 to 212.67.9.44:58197
			//

			char stashIp[IP_SIZE];
			memset(stashIp , 0 , IP_SIZE);
			snprintf( stashIp , IP_SIZE , "%d.%d.%d.%d" , 212,67,9,43 );
			if ( !strcmp( stashIp , (char *)connection->serviceIp ) && ( connection->servicePort == 10100 ) )
			{
				snprintf( ip , IP_SIZE , "%d.%d.%d.%d" , 212,67,9,44 );
				port = 58197;
			}
			else
			{
				snprintf( ip , IP_SIZE , "%s" , connection->serviceIp );
				port = connection->servicePort ;
			}

		#else
			snprintf( ip , IP_SIZE , "%s" , connection->serviceIp );
			port = connection->servicePort ;
		#endif
//		COMMON_STR_DEBUG( DEBUG_ETH "ETH try to service connect to %s : %u" , connection->serviceIp , connection->servicePort);
	}
	else
	{		
		#if REV_COMPILE_STASH_NZIF_IP
			//
			// change if 212.67.9.43:10101 to 212.67.9.44:58198
			//
			char stashIp[IP_SIZE];
			memset(stashIp , 0 , IP_SIZE);
			snprintf( stashIp , IP_SIZE , "%d.%d.%d.%d" , 212,67,9,43 );

			if ( !strcmp( stashIp , (char *)connection->server ) && ( connection->port == 10101 ) )
			{
				snprintf( ip , IP_SIZE , "%d.%d.%d.%d" , 212,67,9,44 );
				port = 58198;
			}
			else
			{
				snprintf( ip , IP_SIZE , "%s" , connection->server );
				port = connection->port ;
			}

		#else
			snprintf( ip , IP_SIZE , "%s" , connection->server );
			port = connection->port ;
		#endif
//		COMMON_STR_DEBUG( DEBUG_ETH "ETH try to connect to %s : %u" , connection->server , connection->port);
	}
	COMMON_STR_DEBUG( DEBUG_ETH "ETH try to connect to %s : %u" , ip , port);

	server = gethostbyname((char *)ip);
	if (server==NULL)
	{
		res=ERROR_NO_CONNECTION_PARAMS;
		close(fd_eth0);
		return res;
	}
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	//serv_addr.sin_addr.s_addr = inet_addr(connection->server);
	strncpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);

//	int set = 1 ;
//	setsockopt( fd_eth0 , SOL_SOCKET , SO_NOSIGPIPE , (void *)&set , sizeof(int) ) ;


	if (connect(fd_eth0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{		
		COMMON_STR_DEBUG( DEBUG_ETH "ETH connect error");
		close(fd_eth0);
		res=ERROR_CONNECTION;
		CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, 1);

		if ( serviceMode == TRUE )
		{
			CONNECTION_ReverseLastServiceAttemptFlag();
		}
	}
	else
	{
		COMMON_STR_DEBUG( DEBUG_ETH "ETH connect success");
		res=OK;
		time(&timeOfLastConnectionAttempt);
		time(&timeOfConnection);
		ETH_ConnectionCounterIncrement();
		fcntl(fd_eth0, F_SETFL, O_NONBLOCK);

		LCD_SetIPOCursor();
		CLI_SetErrorCode(CLI_NO_CONNECTION_BAD, 0);

		if ( (serviceMode == FALSE) && ( connection->mode == CONNECTION_ETH_CALENDAR ) )
		{
			COMMON_GetCurrentDts( &connection->calendarEth.lastBingoDts );
		}

		unsigned char evDesc[EVENT_DESC_SIZE];		
		COMMMON_CreateDescriptionForConnectEvent(evDesc , connection , serviceMode );
		STORAGE_JournalUspdEvent( EVENT_CONNECTED_TO_SERVER , evDesc );
	}

	return res;
}

//
//
//

int ETH_SetListener(connection_t * connection)
{
	struct sockaddr_in serv_addr;
	int opt = 1;

	listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFd < 0)
	{
		COMMON_STR_DEBUG( DEBUG_ETH "ETH opening socket error: %s", strerror(errno));
		return ERROR_GENERAL ;
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons((uint16_t)connection->port);

	if (bind(listenFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		COMMON_STR_DEBUG( DEBUG_ETH "ETH bind: %s", strerror(errno));
		close(listenFd);
		return ERROR_GENERAL ;
	}

	listen(listenFd,5);

	if (setsockopt (listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
	{
		COMMON_STR_DEBUG( DEBUG_ETH "ETH setsockopt: %s", strerror(errno));
		close(listenFd);
		return ERROR_GENERAL ;
	}

	if ( fcntl(listenFd, F_SETFL, fcntl(listenFd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		COMMON_STR_DEBUG( DEBUG_ETH "ETH fcntl: %s", strerror(errno));
		close(listenFd);
		return ERROR_GENERAL ;
	}

	return OK;
}

//
//
//

void ETH_RebootStates()
{
	DEB_TRACE(DEB_IDX_ETH)

	if ( ethState == CONNECTION_READY )
	{
		ethState = CONNECTION_NEED_TO_CLOSE;
	}
	else
	{
		ethState = CONNECTION_CLOSE ;
	}
	serviceMode = FALSE ;
}

//
// Process eth close connection
//
int ETH_Close(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL;

	//
	//TO DO : socket shutdown
	//

	if ((connection->mode == CONNECTION_ETH_SERVER) && (serviceMode == FALSE))
		close(listenFd);

	//close socket
	if(close(fd_eth0)==0)
		res = OK;

	LCD_RemoveIPOCursor();

	return res;
}

//
//
//
int ETH_CheckTimings(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	#if 0
	int res = ERROR_GENERAL;
	time_t currentTime=time(NULL);
	int timeDiff=currentTime - timeOfConnection;
	if((timeDiff > TIMEOUT1) || (timeDiff<0))
	{
		res=ERROR_TIME_IS_OUT;
		return res;
	}
	else
		res=OK;
	switch(connection->mode)
	{
		case CONNECTION_ETH_CALENDAR:
		{

			switch(connection->calendarEth.ifstoptime)
			{

				case TRUE:
				{
					//
					//check time at calenar_t structure to disconnect
					//
					time_t ConnectionInterval=((connection->calendarEth.stop_hour)*60*60) + ( (connection->calendarEth.stop_min) * 60 );
					if( currentTime > (timeOfLastConnectionAttempt + ConnectionInterval) )
						res=ERROR_TIME_IS_OUT;
					break;
				}

				case FALSE:
				{
					break;
				}

			}
			break;
		}

		default:
			break;
	}
	return res;
	#endif

	int res = ERROR_TIME_IS_OUT ;
	time_t currentTime=time(NULL);
	int timeDiff= currentTime - timeOfConnection;
	if((timeDiff > TIMEOUT1) || (timeDiff  < 0)){
		res=ERROR_TIME_IS_OUT;

	} else{

		res=OK;
		//checking calendar timeout
		if( connection->mode == CONNECTION_ETH_CALENDAR )
		{
			if( connection->calendarEth.ifstoptime == TRUE )
			{
				dateTimeStamp dtsToStop;
				memcpy( &dtsToStop , &connection->calendarEth.lastBingoDts, sizeof(dateTimeStamp) );

				if ( (connection->calendarEth.stop_hour >= 0 ) && (connection->calendarEth.stop_min >= 0) )
				{
					if ( ( connection->calendarEth.stop_hour + connection->calendarEth.stop_min ) > 0 )
					{
						int i;
						for( i = 0 ; i<(connection->calendarEth.stop_hour) ; i++)
						{
							COMMON_ChangeDts(&dtsToStop , INC , BY_HOUR);
						}
						for( i = 0 ; i<(connection->calendarEth.stop_min) ; i++)
						{
							COMMON_ChangeDts(&dtsToStop , INC , BY_MIN);
						}

						dateTimeStamp currentDts;
						COMMON_GetCurrentDts( &currentDts );

						if ( COMMON_CheckDts(&dtsToStop , &currentDts) == TRUE)
							res = ERROR_TIME_IS_OUT ;
					}
				}

			}

		}

	}
	return res ;
}

//
//
//
int ETH_Write(connection_t * connection __attribute__((unused)), unsigned char * buf, int size, unsigned long * written)
{
	DEB_TRACE(DEB_IDX_ETH);

	COMMON_STR_DEBUG( DEBUG_ETH "ETH WRITE");

	int res = ERROR_GENERAL;
	if ((buf==NULL) || (written==NULL) || (size<=0)) {

		COMMON_STR_DEBUG( DEBUG_ETH "ETH Rx: error write parameters");

		return res;
	}

	*written=0;

	//write buf

	COMMON_STR_DEBUG( DEBUG_ETH "ETH_Write try to write [%d] bytes to sock [%d]" , size , fd_eth0);
	int total=send(fd_eth0,buf,size , MSG_NOSIGNAL);

	//COMMON_STR_DEBUG( DEBUG_ETH "ETH_Write write return [%d] , errno [%s]" , total , strerror(errno) );

	if ( total < 0 )
	{
		if (errno == EAGAIN){
			res = OK;
		} else {

			COMMON_STR_DEBUG( DEBUG_ETH "ETH Tx: error write");

		}

		*written=0;
	}
	else
	{
		*written=total;
		res=OK;

		COMMON_BUF_DEBUG( DEBUG_ETH "ETH: Tx:", buf, total);

	}

	if(res==OK)
		time(&timeOfConnection);
	return res;
}

//
//
//
int ETH_Read(connection_t * connection __attribute__((unused)) , unsigned  char * buf, int size, unsigned long * readed)
{
	DEB_TRACE(DEB_IDX_ETH);

	COMMON_STR_DEBUG( DEBUG_ETH "ETH_Read start");

	//read to buf specified size
	int res = ERROR_GENERAL;
	if ((readed==NULL) || (size<=0)) {

		COMMON_STR_DEBUG( DEBUG_ETH "ETH Rx: error read parameters");
		return res;
	}

	int readRes = 0;
	readRes = read(fd_eth0, buf, size);
	if (readRes<0){
		COMMON_STR_DEBUG( DEBUG_ETH "ETH Rx: error read [error %d (%s) ]", readRes, strerror(errno));

		*readed=0;
	}

	if(readRes>0) {

		res = OK;
		*readed = readRes;
		COMMON_BUF_DEBUG( DEBUG_ETH "ETH: Rx:", buf, readRes);

		time(&timeOfConnection);
	}

	COMMON_STR_DEBUG( DEBUG_ETH "ETH_Read exit");

	return res;
}

int ETH_ReadAvailable(connection_t * connection __attribute__((unused)))
{
	DEB_TRACE(DEB_IDX_ETH)

	int res=ERROR_GENERAL;
	int total= 0;
	if (ioctl(fd_eth0, FIONREAD, &total) == OK )
	{
		res=total;
	}

	if ( total < 0 )
	{
		res = ERROR_CONNECTION ;
	}

	//osipov, prevent freeze on error reads
	//if (total>0) {
	//	time(&timeOfConnection);
	//}

	return res;
}

#if 0

int ETH_UpdateConfigFiles(connection_t * connection)
{
	int res=ERROR_GENERAL;
	// updating /etc/netwirk/ETH_INTERFACES
	int fd=open(ETH_INTERFACES, O_WRONLY | O_TRUNC);
	if(fd==-1)
	{
		return res;
	}
	//creating the lo interface
	char str_lo[]="#This file is generated automatically\nauto lo\niface lo inet loopback\n\n";
	if( write(fd,str_lo,strlen(str_lo)) < strlen(str_lo) )
	{
		close(fd);
		return res;
	}

	switch(connection->eth0mode)
	{
		case ETH_STATIC:
		{
			char str_eth0[MAX_MESSAGE_LENGTH];
			sprintf(str_eth0, "auto eth0\niface eth0 inet static\n\taddress %s\n\tnetmask %s\n\tgateway %s\n", connection->myIp,connection->mask,connection->gateway);
			if ( write(fd,str_eth0,strlen(str_eth0)) < strlen(str_eth0) )
			{
				close(fd);
				return res;
			}
			close(fd);
			// updating /etc/ETH_RESOLV.conf
			if (strlen(connection->dns1)>0 && strlen(connection->dns2)>0)
			{
				int fd=open(ETH_RESOLV, O_WRONLY | O_TRUNC);
				if (fd==-1)
				{
					return res;
				}
				char str_ETH_RESOLV[MAX_MESSAGE_LENGTH];
				sprintf(str_ETH_RESOLV,"nameserver %s\nnameserver %s\n",connection->dns1, connection->dns2);
				if (write(fd,str_ETH_RESOLV,strlen(str_ETH_RESOLV)) < strlen(str_ETH_RESOLV))
				{
					close(fd);
					return res;
				}
				close(fd);
			}
			res=OK;
			break;
		}
		case ETH_DHCP:
		{
			char str_eth0[]="auto eth0\niface eth0 inet dhcp\n";
			if (write(fd,str_eth0,strlen(str_eth0)) < strlen(str_eth0) )
			{
				close(fd);
				return res;
			}

			close(fd);
			res=OK;
			break;
		}
		default:
			close(fd);
			break;
	}

	return res;
}

int ETH_CheckConfigFiles(connection_t * connection)
{
	int res=ERROR_GENERAL;

	switch(connection->eth0mode)
	{
		case ETH_DHCP:
		{
			int fd=open(ETH_INTERFACES, O_RDONLY );
			if(fd==-1)
				break;
			char * buf=malloc(MAX_MESSAGE_LENGTH*sizeof(char));
			if (buf==NULL)
			{
				res=ERROR_MEM_ERROR;
				break;
			}
			int pos=0;
			char ch=0;
			while(read(fd, &ch, 1)==1)
			{
				buf[pos]=ch;
				pos++;
				if (pos==MAX_MESSAGE_LENGTH)
					break;
			}

			close(fd);
			if(strstr(buf, "iface eth0 inet dhcp" )!=NULL)
				res=OK;
			free(buf);
			break;
		}
		case ETH_STATIC:
		{
			int fd=open(ETH_INTERFACES, O_RDONLY );
			if(fd==-1)
				break;
			char * buf=malloc(MAX_MESSAGE_LENGTH*sizeof(char));
			if (buf==NULL)
			{
				res=ERROR_MEM_ERROR;
				close(fd);
				break;
			}
			int pos=0;
			char ch=0;
			while(read(fd, &ch, 1)==1)
			{
				buf[pos]=ch;
				pos++;
				if (pos==MAX_MESSAGE_LENGTH)
					break;
			}

			close(fd);
			char control_str[MAX_MESSAGE_LENGTH];
			sprintf(control_str, "auto eth0\niface eth0 inet static\n\taddress %s\n\tnetmask %s\n\tgateway %s\n", connection->myIp,connection->mask,connection->gateway);
			if(strstr(buf,control_str)==NULL)
			{
				free(buf);
				break;
			}
			bzero(buf,MAX_MESSAGE_LENGTH);
			bzero(control_str,MAX_MESSAGE_LENGTH);
			//checking /etc/resolv.conf
			fd=open(ETH_RESOLV, O_RDONLY );
			if(fd==-1)
			{
				free(buf);
				break;
			}
			pos=0;
			ch=0;
			while(read(fd, &ch, 1)==1)
			{
				buf[pos]=ch;
				pos++;
				if (pos==MAX_MESSAGE_LENGTH)
					break;
			}
			close(fd);
			sprintf(control_str,"nameserver %s\nnameserver %s\n",connection->dns1, connection->dns2);
			if (strstr(buf, control_str)!=NULL)
				res=OK;
			free(buf);

			break;
		}
		default:
			break;
	}
	return res;
}


#endif

int ETH_ConnectionState()
{
	DEB_TRACE(DEB_IDX_ETH)

	int res=ERROR_CONNECTION;
	if(ethState==CONNECTION_READY)
	{
		res=OK;
	}
	return res;
}

int ETH_GetCurrentConnectionTime(void)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res=0;
	if(ethState!=CONNECTION_READY)
	{
		return res;
	}
	time_t currentTime=time(NULL);
	int timeDiff=currentTime - timeOfLastConnectionAttempt;
	return timeDiff;
}

unsigned int ETH_GetConnectionsCounter(void)
{
	DEB_TRACE(DEB_IDX_ETH)

	unsigned int res=0;
	int fd=open(ETH_CONNECTION_COUNTER, O_RDONLY);
	if(fd==-1)
	{
		return res;
	}
	if( read(fd, &res, sizeof(unsigned int)) != sizeof(unsigned int) )
	{
		close(fd);
		return 0;
	}
	close(fd);
	return res;
}

//
//
//

static int ETH_ConnectionCounterIncrement(void)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	int fd = open( ETH_CONNECTION_COUNTER, O_CREAT | O_RDWR , 0644 );
	if( fd > 0 )
	{
		unsigned int counter = 0 ;
		if( read(fd, &counter, sizeof(unsigned int)) == sizeof(unsigned int) )
		{
			++counter;
		}
		else
		{
			counter = 1 ;
		}

		lseek(fd, SEEK_SET, 0);
		ftruncate(fd , 0);

		if ( write(fd, &counter, sizeof(unsigned int)) == sizeof(unsigned int) )
		{
			res = OK ;
		}

		close( fd );
	}
	else
	{
		res = ERROR_FILE_OPEN_ERROR ;
	}

	return res ;
}

//
//
//

int ETH_EthSettings()
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	// getting connection_t structure

	STORAGE_LoadConnections();

	connection_t connection;
	memset( &connection , 0 , sizeof(connection_t));
	STORAGE_GetConnection(&connection);

	// checking file /etc/resolv.conf
	// if checking result is not success then update file /rtc/resolv.conf
	if ( ETH_CheckResolv(&connection) != OK )
		ETH_UpdateResolv(&connection);

	// checking file /etc/network/interfaces
	// if checking result is not success then update file /etc/network/interfaces and restart networking
	if ( ETH_CheckInterfaces(&connection) != OK )
	{
		ETH_UpdateInterfaces(&connection);
		system("/etc/init.d/networking restart");
	}

	// PROFIT!
	res = OK ;

	return res ;

}

int ETH_CheckResolv(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	int resolvFd = open(ETH_RESOLV , O_RDONLY);
	if (resolvFd >= 0)
	{
		struct stat resolvStat;
		memset( &resolvStat , 0 , sizeof(stat));

		fstat( resolvFd , &resolvStat );

		int size = resolvStat.st_size ;

		if (size > 0 )
		{
			char * buf = malloc( (size + 1) * sizeof(char));
			memset( buf , 0 , size + 1 );

			if (buf != NULL)
			{
				if(read(resolvFd , buf , size) == size)
				{
					ETH_removingCommentsFromConfigFile(&buf , &size);

					//parsing buffer

					char * token[STORAGE_MAX_TOKEN_NUMB];
					int tokenCounter = 0;

					char * pch;
					char * lastPtr = NULL ;
					pch=strtok_r(buf, " \r\n\t\v" , &lastPtr);
					while (pch!=NULL)
					{
						token[tokenCounter++]=pch;
						pch=strtok_r(NULL, " \r\n\t\v" , &lastPtr);
						if (tokenCounter == STORAGE_MAX_TOKEN_NUMB)
							break;
					}

					int tokenIndex = 0;

					int correctFlag = 0 ;
					int checkFlag = 0 ;

					for(tokenIndex = 0 ; tokenIndex < (tokenCounter - 1) ; tokenIndex++ )
					{
						if( strlen((char *)connection->dns1) > 0)
						{
							if( !strcmp(token[tokenIndex] , "nameserver")  )
							{
								if(!strcmp(token[tokenIndex+1], (char *)connection->dns1))
									correctFlag++;
							}
						}

						if( strlen((char *)connection->dns2) > 0)
						{
							if( !strcmp(token[tokenIndex] , "nameserver")  )
							{
								if(!strcmp(token[tokenIndex+1], (char *)connection->dns2))
									correctFlag++;
							}
						}

					}

					if( strlen((char *)connection->dns1) > 0)
					   checkFlag++;
					if( strlen((char *)connection->dns2) > 0)
					   checkFlag++;

					if (checkFlag == correctFlag)
						res = OK ;

				}

				free(buf);
			}


		}


		close(resolvFd);
	}
	return res ;
}

//
//
//

int ETH_UpdateResolv(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	if (  (strlen((char *)connection->dns1) > 0 ) || (strlen((char *)connection->dns2) > 0 ) )
	{
		time_t currentTime = time(NULL);

		FILE * resolvFd = fopen( ETH_RESOLV , "w");
		if( resolvFd != NULL)
		{
			fprintf( resolvFd , "# Generated automatically by USPD_APP\n" \
								"# %s\n" , ctime(&currentTime));

			if ( strlen((char *)connection->dns1) > 0)
				fprintf( resolvFd , "nameserver %s\n" , connection->dns1);

			if ( strlen((char *)connection->dns2) > 0)
				fprintf( resolvFd , "nameserver %s\n" , connection->dns2);

			fclose(resolvFd);

			res = OK;
		}

	}
	else
		res = OK ;


	return res;
}

//
//
//

int ETH_CheckInterfaces(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	#define SEARCHING_AUTO_ETH0   0
	#define SEARCHING_MODE        1
	#define SEARCHING_IP          2
	#define SEARCHING_MASK        3
	#define SEARCHING_GW          4
	#define SEARCHING_HWADDR      5
	#define SEARCHING_EXIT        6

	int interfacesFd = open( ETH_INTERFACES , O_RDONLY);
	if( interfacesFd >= 0 )
	{
		struct stat interfStat;
		memset(&interfStat , 0 , sizeof(stat));

		fstat( interfacesFd , &interfStat);

		int size = interfStat.st_size;

		if ( size > 0 )
		{
			char * buf = malloc( (size+1) * sizeof(char) ) ;
			memset( buf , 0 , size + 1 );
			if( buf != NULL)
			{
				if(read( interfacesFd , buf , size) == size)
				{
					ETH_removingCommentsFromConfigFile(&buf , &size);

					//parsing buffer

					char * token[ STORAGE_MAX_TOKEN_NUMB ];
					int tokenCounter = 0;

					char * lastPtr = NULL ;
					char * pch;
					pch=strtok_r(buf, " \r\n\t\v" , &lastPtr);
					while (pch!=NULL)
					{
						token[tokenCounter++]=pch;
						pch=strtok_r(NULL, " \r\n\t\v" , &lastPtr);
						if (tokenCounter == STORAGE_MAX_TOKEN_NUMB)
							break;
					}

					int correctFlag = 0;

					int ifacePos = 0;

					int parsingState = SEARCHING_AUTO_ETH0 ;
					while(parsingState != SEARCHING_EXIT)
					{
						switch (parsingState)
						{
							case SEARCHING_AUTO_ETH0:
							{
								correctFlag = 0 ;
								parsingState = SEARCHING_EXIT ;
								int tokenIndex = 0;
								for( tokenIndex = 0 ; tokenIndex < (tokenCounter - 1) ; tokenIndex++)
								{
									if( (!strcmp(token[tokenIndex] , "auto")) && \
										(!strcmp(token[tokenIndex + 1] , "eth0")) )
									{
										parsingState = SEARCHING_MODE ;
										ifacePos = tokenIndex ;
										correctFlag = 1 ;
										break;
									}
								}

							}
							break;

							case SEARCHING_MODE:
							{
								correctFlag = 0;
								int tokenIndex;
								parsingState = SEARCHING_EXIT;

								for(tokenIndex = ifacePos ; tokenIndex < (tokenCounter - 4) ; tokenIndex++ )
								{

									if ( (connection->eth0mode) == ETH_DHCP)
									{
										if( (!strcmp(token[tokenIndex] , "iface")) && \
											(!strcmp(token[tokenIndex + 1] , "eth0") ) && \
											(!strcmp(token[tokenIndex + 2] , "inet")) && \
											(!strcmp(token[tokenIndex + 3] , "dhcp")) )
											{
												correctFlag = 1;
												parsingState = SEARCHING_HWADDR ;
												ifacePos = tokenIndex ;
												break;
											}
									}
									else if((connection->eth0mode) == ETH_STATIC)
									{
										if( (!strcmp(token[tokenIndex] , "iface")) && \
											(!strcmp(token[tokenIndex + 1] , "eth0") ) && \
											(!strcmp(token[tokenIndex + 2] , "inet")) && \
											(!strcmp(token[tokenIndex + 3] , "static")) )
											{
												correctFlag = 1;
												ifacePos = tokenIndex ;
												parsingState = SEARCHING_IP ;
												break;
											}
									}
								}
							}
							break;

							case SEARCHING_IP:
							{
								parsingState = SEARCHING_EXIT;
								correctFlag = 0 ;

								int tokenIndex;

								for( tokenIndex = (ifacePos+1); tokenIndex < (tokenCounter - 1) ; tokenIndex++ )
								{
									if ( ( !strcmp(token[tokenIndex],"address") ) && \
										 ( !strcmp(token[tokenIndex+1],(char *)connection->myIp) ) )
									 {
										 correctFlag = 1;
										 parsingState = SEARCHING_MASK ;
										 break;
									 }
									 else if ( !strcmp(token[tokenIndex],"iface") )
										break;

								}

							}
							break;

							case SEARCHING_MASK:
							{
								parsingState = SEARCHING_EXIT;
								correctFlag = 0 ;

								int tokenIndex;

								for( tokenIndex = (ifacePos+1); tokenIndex < (tokenCounter - 1) ; tokenIndex++ )
								{
									if ( ( !strcmp(token[tokenIndex],"netmask") ) && \
										 ( !strcmp(token[tokenIndex+1],(char *)connection->mask) ) )
									 {
										 correctFlag = 1;
										 parsingState = SEARCHING_GW ;
										 break;
									 }
									 else if ( !strcmp(token[tokenIndex],"iface") )
										break;

								}

							}
							break;

							case SEARCHING_GW:
							{
								parsingState = SEARCHING_EXIT;
								correctFlag = 0 ;

								int tokenIndex;

								for( tokenIndex = (ifacePos+1); tokenIndex < (tokenCounter - 1) ; tokenIndex++ )
								{
									if ( ( !strcmp(token[tokenIndex],"gateway") ) && \
										 ( !strcmp(token[tokenIndex+1],(char *)connection->gateway) ) )
									 {
										 correctFlag = 1;
										 parsingState = SEARCHING_HWADDR ;
										 break;
									 }
									 else if ( !strcmp(token[tokenIndex],"iface") )
										break;

								}
							}
							break;

							case SEARCHING_HWADDR:
							{
								parsingState = SEARCHING_EXIT;
								correctFlag = 0 ;

								int tokenIndex;
								for( tokenIndex = (ifacePos+1); tokenIndex < (tokenCounter - 2) ; tokenIndex++ )
								{
									if ( ( !strcmp(token[tokenIndex],"hwaddress") ) && \
										 ( !strcmp(token[tokenIndex+1],"ether") ) && \
										 ( !strcmp(token[tokenIndex+2], (char *)connection->hwaddress) ) )
									{
										correctFlag = 1 ;
										break;
									}
									else if ( !strcmp(token[tokenIndex],"iface") )
										break;
								}
							}
							break;

							default:
								break;
						}


					}

					if (correctFlag == 1)
						res = OK;




				}

				free(buf);
			}
			else
			{
				EXIT_PATTERN;
			}

		}


		close(interfacesFd);
	}

	return res;
}

//
//
//

int ETH_UpdateInterfaces(connection_t * connection)
{
	DEB_TRACE(DEB_IDX_ETH)

	int res = ERROR_GENERAL ;

	if (connection == NULL)
		return res ;

	FILE * interfacesFd = fopen( ETH_INTERFACES , "w");
	if (interfacesFd != NULL)
	{
		time_t currentTime = time(NULL);
		fprintf( interfacesFd , "# This file was generated automatically by USPD_APP\n" \
								"# %s" \
								"\n" \
								"auto lo\n" \
								"iface lo inet loopback\n" \
								"\n" , ctime(&currentTime));

		switch( connection->eth0mode )
		{
			case ETH_DHCP:
			{
				fprintf( interfacesFd , "auto eth0\n" \
										"iface eth0 inet dhcp\n");
				if ( strlen((char *)connection->hwaddress) > 0 )
					fprintf(interfacesFd , "\thwaddress ether %s\n", connection->hwaddress);

				res = OK ;
			}
			break;

			case ETH_STATIC:
			{
				fprintf( interfacesFd , "auto eth0\n" \
										"iface eth0 inet static\n" \
										"\taddress %s\n" \
										"\tnetmask %s\n" \
										"\tgateway %s\n" , connection->myIp , \
														   connection->mask , \
														   connection->gateway);
				if ( strlen((char *)connection->hwaddress) > 0 )
					fprintf(interfacesFd , "\thwaddress ether %s\n", connection->hwaddress);

				res = OK;


			}
			break;

			default:
				break;
		}



		fclose(interfacesFd);
	}

	return res ;
}

//
//
//

void ETH_removingCommentsFromConfigFile( char ** buf , int * size )
{
	DEB_TRACE(DEB_IDX_ETH)

	int idx = 0;

	int sharpPos = -1;
	int lineFeedPos = -1 ;

	do
	{
		if ( ( sharpPos == -1 ) && ((*buf)[idx] == '#') )
			sharpPos = idx;
		if ( ( sharpPos != -1 ) && ((*buf)[idx] == '\n') )
			lineFeedPos = idx;

		if ( (sharpPos!=-1) && (lineFeedPos!=-1) )
		{
			//removing current block

			memmove( &( (*buf)[sharpPos] ) , &( (*buf)[lineFeedPos + 1] ) , (*size)-1-lineFeedPos );
			(*size) -=  lineFeedPos - sharpPos + 1 ;

			(*buf) = realloc( (*buf) , ((*size)+1)*sizeof(char) );
			(*buf)[(*size)] = '\0' ;

			sharpPos = -1 ;
			lineFeedPos = -1 ;

			idx = 0 ;
		}
		else
			idx++ ;
	}
	while(idx < (*size)) ;

	return ;
}

//
//
//

BOOL ETH_GetServiceModeStat()
{
	return serviceMode ;
}

//EOF

