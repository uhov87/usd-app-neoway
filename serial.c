/*
		application code v.1
		for uspd micron 2.0x

		2012 - January
		NPO Frunze
		RUSSIA NIGNY NOVGOROD

		OSIPOV V, SHASHUNKIN D, OSKOLKOVA O, UHOV E
 */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "common.h"
#include "serial.h"
#include "debug.h"

//local variables
int fdPlc;
int fdRs485;
int fdGsm;
int fdExio;
static pthread_mutex_t serialMuxPlc = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t serialMuxRs485 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t serialMuxGsm = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t serialMuxExio = PTHREAD_MUTEX_INITIALIZER;

//
//
//

void SERIAL_ProtectExio() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_lock(&serialMuxExio);
}

//
//
//

void SERIAL_UnprotectExio() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_unlock(&serialMuxExio);
}

//
//
//

void SERIAL_ProtectPlc() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_lock(&serialMuxPlc);
}

//
//
//

void SERIAL_UnprotectPlc() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_unlock(&serialMuxPlc);
}

//
//
//

void SERIAL_ProtectRs485() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_lock(&serialMuxRs485);
}

//
//
//

void SERIAL_UnprotectRs485() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_unlock(&serialMuxRs485);
}

//
//
//

void SERIAL_ProtectGsm() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_lock(&serialMuxGsm);
}

//
//
//

void SERIAL_UnprotectGsm() {
	DEB_TRACE(DEB_IDX_SERIAL)

	pthread_mutex_unlock(&serialMuxGsm);
}

//
// Serial part initialization
//

int SERIAL_Initialize() {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = OK;

	//
	//to do - check ready of all 3 ttySx
	//

	//open GSM terminal
	if (res == OK) {
		res = SERIAL_OPEN_GSM();
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open GSM terminal %s", getErrorCode(res));
	}


	#if ( (REV_COMPILE_EXIO == 1) && (REV_COMPILE_PLC == 1) )
		#error Flags ExIO and RF are both ON
	#else //if ( (REV_COMPILE_EXIO == 1) && (REV_COMPILE_PLC == 1) )

		#if (REV_COMPILE_PLC == 1)
	// open PLC terminal
	if (res == OK) {
				res = SERIAL_OPEN_PLC();
				COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open plc terminal %s", getErrorCode(res));
			}

		#elif (REV_COMPILE_EXIO == 1)

			//open Extension IO terminal
			if (res == OK) {
				res = SERIAL_OPEN_EXIO();
				COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open ExIO terminal %s", getErrorCode(res));
	}

		#endif //if (REV_COMPILE_EXIO == 1)
	#endif //if ( (REV_COMPILE_EXIO == 1) && (REV_COMPILE_PLC == 1) )



	//open rs485 link
	if (res == OK) {
		res = SERIAL_OPEN_RS485();
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open 485 terminal %s", getErrorCode(res));
	}

	//debug
	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL_Initialize() %s", getErrorCode(res));

	return res;
}

//
//
//

int SERIAL_OPEN_PLC() {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;
	int speed;
	struct termios options;

	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL Try to open PLC serial");

	fdPlc = open(SERIAL_LINK_TO_PLC, O_RDWR | O_SYNC | O_NOCTTY | O_NDELAY | O_NONBLOCK ) ;
	if (fdPlc == -1) {
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open PLC error on open");
		return res;
	}

	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL Try to update PLC serial");

	memset(&options, 0, sizeof(struct termios));
	tcgetattr(fdPlc, &options); //get curretn attributes

	options.c_iflag &= ~(INPCK | ISTRIP); // Disableparity checking

	//input
	options.c_iflag |= IGNPAR; // BUt ignore parity
	options.c_iflag &= ~INLCR; // Don't convert linefeeds
	options.c_iflag &= ~ICRNL; // Don't convert linefeeds

	//output
	options.c_oflag |= IGNPAR; // BUt ignore parity
	options.c_oflag &= ~INLCR; // Don't convert linefeeds
	options.c_oflag &= ~ICRNL; // Don't convert linefeeds

	options.c_cflag |= CBAUDEX;                         //??
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag |= HUPCL;

	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;                 //no stop and others
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	options.c_cflag &= ~CRTSCTS;

	options.c_oflag &= ~(IXON | IXOFF | IXANY); // no flow control on output
	options.c_iflag &= ~(IXON | IXOFF | IXANY); // no flow control on input

	options.c_oflag &= ~OPOST;                  //ignore processing
	options.c_iflag &= ~OPOST;                  //ignore processing

	options.c_cc[VMIN] = 0;                     //recieve at least minimum 1 byte
	options.c_cc[VTIME] = 0;                    //blocking inread on 300 ms if no any bytes recieved

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	cfsetispeed(&options, SERIAL_BAUD_PLC);
	speed = cfgetispeed(&options);

	cfsetospeed(&options, SERIAL_BAUD_PLC);
	speed = cfgetospeed(&options);

	tcsetattr(fdPlc, TCSANOW, &options); //set attributes

	SERIAL_PLC_FLUSH();

	res = OK;

	return res;
}

void SERIAL_PLC_FLUSH(){
	tcflush(fdPlc, TCIOFLUSH);
}

//
//
//

int SERIAL_OPEN_EXIO()
{
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;
	int speed;
	struct termios options;

	fdExio = open(SERIAL_LINK_TO_EXIO, O_RDWR | O_NDELAY | O_NOCTTY | O_NONBLOCK);
	if (fdExio == -1) {
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open ExIO error on open");
		return res;
	}

	options.c_iflag &= ~INPCK;                          //disable input parity
	options.c_iflag |= IGNPAR;                          //set to ignore input parity
	options.c_iflag &= ~INLCR;                          //don't convert linefeeds
	options.c_iflag &= ~ICRNL;                          //don't convert linefeeds
	options.c_cflag |= CBAUDEX;                         //speed is higner than 57600, so set
	options.c_cflag &= ~CRTSCTS;                        //no hw flow control
	options.c_cflag |= (CLOCAL | CREAD);                //enable input reciever, discard line control
	options.c_cflag |= HUPCL;                           //drop dtr signal on last close
	options.c_cflag &= ~PARENB;                         //disable output parity
	options.c_cflag &= ~CSTOPB;                         //1
	options.c_cflag &= ~CSIZE;                          //N
	options.c_cflag |= CS8;                             //8
	options.c_oflag &= ~(IXON | IXOFF | IXANY);         // no flow control
	options.c_iflag &= ~(IXON | IXOFF | IXANY);
	options.c_oflag &= ~OPOST;                          //select raw output
	options.c_cc[VMIN] = 0;                             //discarded by ndelay
	options.c_cc[VTIME] = 5;                           //discarded by ndelay, wait till 0.5 second
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //select raw input

	cfsetispeed(&options, SERIAL_BAUD_EXIO);
	speed = cfgetispeed(&options);

	cfsetospeed(&options, SERIAL_BAUD_EXIO);
	speed = cfgetospeed(&options);

	int saved_flags = fcntl(fdExio, F_GETFL);
	fcntl(fdExio, F_SETFL, saved_flags | O_NONBLOCK);

	tcsetattr(fdExio, TCSANOW, &options);
	tcflush(fdExio, TCIOFLUSH);

	res = OK;

	return res;
}

//
//
//

int SERIAL_OPEN_RS485() {
	DEB_TRACE(DEB_IDX_SERIAL)


	int res = ERROR_SERIAL_GLOBAL;

	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL Try to open 485 serial");

	fdRs485 = open(SERIAL_LINK_TO_RS485, O_RDWR | O_SYNC | O_NOCTTY | O_NDELAY | O_NONBLOCK ) ;
	if (fdRs485 == -1) {
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open 485 error on open");
		return res;
	}

	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL Try to update 485 serial");	

	res = SERIAL_SetupAttributesRs485( NULL );

	return res;
}

//
//
//

int SERIAL_SetupAttributesRs485( serialAttributes_t * serialAttributes)
{
	int res = ERROR_GENERAL ;

	if ( serialAttributes )
	{
		//setup attributes

		int speed;
		struct termios options;
		memset(&options, 0, sizeof(struct termios));
		tcgetattr(fdRs485, &options); //get curretn attributes

		//input
		options.c_iflag &= ~INLCR; // Don't convert linefeeds
		options.c_iflag &= ~ICRNL; // Don't convert linefeeds

		//output
		//options.c_oflag |= IGNPAR; // BUt ignore parity
		options.c_oflag &= ~INLCR; // Don't convert linefeeds
		options.c_oflag &= ~ICRNL; // Don't convert linefeeds

		options.c_cflag |= CBAUDEX;                         //??
		options.c_cflag |= CLOCAL;							//local line (not modem)
		options.c_cflag |= CREAD;							//enable reading
		options.c_cflag |= HUPCL;

		//parity
		switch( serialAttributes->parity )
		{
			default:
			case SERIAL_RS_PARITY_NONE:
			{
				options.c_iflag &= ~(INPCK | ISTRIP); // Disableparity checking
				options.c_iflag |= IGNPAR; // BUt ignore parity

				options.c_cflag &=~ PARENB;
				options.c_cflag &=~ PARODD;
			}
			break;

			case SERIAL_RS_PARITY_EVEN:
			{
				options.c_iflag |= (INPCK);
				options.c_iflag &= ~(IGNPAR | ISTRIP | PARMRK);

				options.c_cflag |= PARENB;
				options.c_cflag &=~ PARODD;
			}
			break;

			case SERIAL_RS_PARITY_ODD:
			{
				options.c_iflag |= (INPCK);
				options.c_iflag &= ~(IGNPAR | ISTRIP | PARMRK);

				options.c_cflag |= PARENB;
				options.c_cflag |= PARODD;
			}
			break;

			case SERIAL_RS_PARITY_MARK:
			{
				options.c_iflag |= (INPCK);
				options.c_iflag &= ~(IGNPAR | ISTRIP | PARMRK);

				options.c_cflag |= ( PARENB | CMSPAR );
				options.c_cflag |= PARODD;
			}
			break;

			case SERIAL_RS_PARITY_SPACE:
			{
				options.c_iflag |= (INPCK);
				options.c_iflag &= ~(IGNPAR | ISTRIP | PARMRK);

				options.c_cflag |= ( PARENB | CMSPAR );
				options.c_cflag &=~ PARODD;
			}
			break;
		}

		//stop bits
		switch( serialAttributes->stopBits )
		{
			default:
			case SERIAL_RS_ONCE_STOP_BIT:
			{
				options.c_cflag &= ~CSTOPB;
			}
			break;

			case SERIAL_RS_DOUBLE_STOP_BIT:
			{
				options.c_cflag |= CSTOPB;
			}
			break;
		}

		//bits
		options.c_cflag &= ~CSIZE;
		switch( serialAttributes->bits )
		{
			case SERIAL_RS_BITS_7:
			{
				options.c_cflag |= CS7;
			}
			break;

			default:
			case SERIAL_RS_BITS_8:
			{
				options.c_cflag |= CS8;
			}
			break;
		}

		options.c_oflag &= ~(IXON | IXOFF | IXANY); // no flow control on output
		options.c_iflag &= ~(IXON | IXOFF | IXANY); // no flow control on input

		options.c_oflag &= ~OPOST;                  //ignore processing
		options.c_iflag &= ~OPOST;                  //ignore processing

		options.c_cc[VMIN] = 0;                     //recieve at least minimum 1 byte
		options.c_cc[VTIME] = 0;                    //blocking inread on 300 ms if no any bytes recieved

		options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

		cfsetispeed(&options, serialAttributes->speed );
		speed = cfgetispeed(&options);

		cfsetospeed(&options, serialAttributes->speed);
		speed = cfgetospeed(&options);

		tcsetattr(fdRs485, TCSANOW, &options); //set attributes
		tcflush(fdRs485, TCIOFLUSH);


		//set to zero
		if(ioctl(fdRs485, 0x542E, NULL) < 0)
		{
			COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL WRITE ioctl 0 error");
		}
		COMMON_Sleep(2);

		res = OK ;
	}
	else
	{
		//setup attributes by default

		int speed;
		struct termios options;
		memset(&options, 0, sizeof(struct termios));
		tcgetattr(fdRs485, &options); //get curretn attributes

		options.c_iflag &= ~(INPCK | ISTRIP); // Disableparity checking

		//input
		options.c_iflag |= IGNPAR; // BUt ignore parity
		options.c_iflag &= ~INLCR; // Don't convert linefeeds
		options.c_iflag &= ~ICRNL; // Don't convert linefeeds

		//output
		options.c_oflag |= IGNPAR; // BUt ignore parity
		options.c_oflag &= ~INLCR; // Don't convert linefeeds
		options.c_oflag &= ~ICRNL; // Don't convert linefeeds

		options.c_cflag |= CBAUDEX;                         //??
		options.c_cflag |= (CLOCAL | CREAD);
		options.c_cflag |= HUPCL;

		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;                 //no stop and others
		options.c_cflag &= ~CSIZE;
		options.c_cflag |= CS8;

		options.c_cflag &= ~CRTSCTS;

		options.c_oflag &= ~(IXON | IXOFF | IXANY); // no flow control on output
		options.c_iflag &= ~(IXON | IXOFF | IXANY); // no flow control on input

		options.c_oflag &= ~OPOST;                  //ignore processing
		options.c_iflag &= ~OPOST;                  //ignore processing

		options.c_cc[VMIN] = 0;                     //recieve at least minimum 1 byte
		options.c_cc[VTIME] = 0;                    //blocking inread on 300 ms if no any bytes recieved

		options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

		cfsetispeed(&options, SERIAL_RS_BAUD_9600);
		speed = cfgetispeed(&options);

		cfsetospeed(&options, SERIAL_RS_BAUD_9600);
		speed = cfgetospeed(&options);

		tcsetattr(fdRs485, TCSANOW, &options); //set attributes
		tcflush(fdRs485, TCIOFLUSH);


		//set to zero
		if(ioctl(fdRs485, 0x542E, NULL) < 0)
		{
			COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL WRITE ioctl 0 error");
		}
		COMMON_Sleep(2);

		res = OK ;
	}

	return res;
}

//
//
//

int SERIAL_OPEN_GSM() {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;
	int speed;
	struct termios options;

	//open
	fdGsm = open(SERIAL_LINK_TO_GSM, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if (fdGsm == -1) {
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open GSM error on open");
		return res;
	}

	if (fcntl(fdGsm, F_SETFL, 0) == -1) {
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL open GSM error on fctl");
		return res;
	}

	tcgetattr(fdGsm, &options);

	options.c_iflag &= ~(INPCK | ISTRIP); // Disableparity checking
	options.c_iflag |= IGNPAR; // BUt ignore parity
	options.c_iflag &= ~INLCR; // Don't convert linefeeds
	options.c_iflag &= ~ICRNL; // Don't convert linefeeds
	options.c_cflag |= CBAUDEX;                         //??
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag |= HUPCL;
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;                 //no stop and others
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag &= ~CRTSCTS;
	options.c_oflag &= ~(IXON | IXOFF | IXANY); // no flow control on output
	options.c_iflag &= ~(IXON | IXOFF | IXANY); // no flow control on input
	options.c_oflag &= ~OPOST;                  //ignore processing
	options.c_cc[VMIN] = 0;                     //recieve at least minimum 1 byte
	options.c_cc[VTIME] = 10;                    //blocking inread on 300 ms if no any bytes recieved
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	cfsetispeed(&options, SERIAL_BAUD_GSM);
	speed = cfgetispeed(&options);

	cfsetospeed(&options, SERIAL_BAUD_GSM);
	speed = cfgetospeed(&options);

	tcsetattr(fdGsm, TCSANOW, &options);
	tcflush(fdGsm, TCIOFLUSH);

	res = OK;

	return res;
}

//
//
//

int SERIAL_CLOSE_GSM()
{
	DEB_TRACE(DEB_IDX_SERIAL)

	return close(fdGsm);
}

//
//
//

/**
  * Bytes counter we can read from serial
  */

int SERIAL_Check(int port, int * len) {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;

	switch (port) {
		case PORT_GSM:
			//enter cs
			SERIAL_ProtectGsm();

			ioctl(fdGsm, FIONREAD, len);

			//leave cs
			SERIAL_UnprotectGsm();
			break;

		case PORT_PLC:
			//enter cs
			SERIAL_ProtectPlc();

			#if (REV_COMPILE_PLC == 1)
			ioctl(fdPlc, FIONREAD, len);
			#endif

			//leave cs
			SERIAL_UnprotectPlc();
			break;

		case PORT_RS:
			//enter cs
			SERIAL_ProtectRs485();


			ioctl(fdRs485, FIONREAD, len);

			//leave cs
			SERIAL_UnprotectRs485();
			break;

		case PORT_EXIO:
			//enter cs
			SERIAL_ProtectExio();

			#if (REV_COMPILE_EXIO == 1)
			ioctl(fdExio, FIONREAD, len);
			#endif

			//leave cs
			SERIAL_UnprotectExio();
			break;

		default:
			break;
	}

	//debug
	if ( (*len) > 0 )
	{
		COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL READY: %d", *len);
	}
	else if ( (*len) < 0 )
		*len = 0 ;

	return res;
}


//
// Read available bytes from specified port limited by size
//

int SERIAL_Read(int port, unsigned char * buf, int size, int * r) {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;
	int total = 0;

	switch (port) {
		case PORT_GSM:
			//enter cs
			SERIAL_ProtectGsm();

			//read
			total = read(fdGsm, buf, size);
			if (total < 0) {
				if (errno == EAGAIN){
					res = OK;
				}

				*r = 0;
			} else {
				*r = total;
				res = OK;
			}

			//leave cs
			SERIAL_UnprotectGsm();

			//COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL GSM READ: ", buf, *r);

			break;

		case PORT_PLC:
			//enter cs
			SERIAL_ProtectPlc();

			//read
			total = read(fdPlc, buf, size);
			if (total < 0) {
				if (errno == EAGAIN)
					res = OK;

				*r = 0;
			} else {
				*r = total;
				res = OK;
			}

			//leave cs
			SERIAL_UnprotectPlc();
			if ((*r) > 0)
			{
				COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL PLC READ: ", buf, *r);
			}
			break;

		case PORT_EXIO:
			//enter cs
			SERIAL_ProtectExio();

			#if (REV_COMPILE_EXIO == 1)
			//read
			total = read(fdExio, buf, size);
			if (total < 0) {
				if (errno == EAGAIN)
					res = OK;

				*r = 0;
			} else {
				*r = total;
				res = OK;
			}
			#endif

			//leave cs
			SERIAL_UnprotectExio();
			//if ((*r) > 0)
			//	COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL EXIO READ: ", buf, *r);
			break;

		case PORT_RS:
			//enter cs
			SERIAL_ProtectRs485();

			//read
			total = read(fdRs485, buf, size);
			if (total < 0) {
				*r = 0;
				if (errno == EAGAIN){
					//COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL READ return EAGAIN error");
					res = OK;
				}
			} else {
				*r = total;
				res = OK;
			}

			//leave cs
			SERIAL_UnprotectRs485();
			//COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL RS READ: ", buf, *r);
			break;

		default:
			break;
	}

	//debug
   // COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL READ: ", buf, *r);

	return res;
}

//
// Write to specified port
//

int SERIAL_Write(int port, unsigned char * buf, int size, int * w) {
	DEB_TRACE(DEB_IDX_SERIAL)

	int res = ERROR_SERIAL_GLOBAL;
	int total = 0;
	int i=0;
	int result;

	//debug
	COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL TX on port [%d]", port);
	COMMON_BUF_DEBUG(DEBUG_SERIAL "SERIAL TX", buf, size);

	switch (port) {
		case PORT_GSM:
			//enter cs
			SERIAL_ProtectGsm();

			//write
			total = write(fdGsm, buf, size);
			if (total < 0) {
				*w = 0;
			} else {
				*w = total;
				res = OK;
			}

			fsync(fdGsm);
			//leave cs
			SERIAL_UnprotectGsm();
			break;

		case PORT_PLC:
			//enter cs
			SERIAL_ProtectPlc();

			//write
			total = write(fdPlc, buf, size);
			if (total < 0) {
				*w = 0;
			} else {
				*w = total;
				res = OK;
			}

			//leave cs
			SERIAL_UnprotectPlc();
			break;

		case PORT_EXIO:
			//enter cs
			SERIAL_ProtectExio();

			#if (REV_COMPILE_EXIO == 1)
			//write
			total = write(fdExio, buf, size);
			if (total < 0) {
				*w = 0;
			} else {
				*w = total;
				res = OK;
			}
			#endif

			//leave cs
			SERIAL_UnprotectExio();
			break;

		case PORT_RS:
			//enter cs
			SERIAL_ProtectRs485();


			//set to 1
			if (ioctl(fdRs485, 0x542F, NULL) < 0) //set pio to hight
			{
				COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL WRITE ioctl 1 error");
			}
			COMMON_Sleep(2);

			//write byte to byte
			for (i=0; i<size; i++){
				result = write(fdRs485, &buf[i], 1);
				if (result < 0) {
					*w = 0;
					res = ERROR_SERIAL_GLOBAL;
					break;
				} else {
					//fsync(fdRs485);
					total+=result;
					*w = total;
					res = OK;
					COMMON_Sleep(2);
				}
			}

			//set to 1
			if (ioctl(fdRs485, 0x542E, NULL) < 0) //set pio to low
			{
				COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL WRITE ioctl 0 error");
			}
			COMMON_Sleep(2);

			//leave cs
			SERIAL_UnprotectRs485();
			break;

		default:
			break;
	}

	//debug
	//COMMON_STR_DEBUG(DEBUG_SERIAL "SERIAL WRITE: %d", total);

	return res;
}

//EOF
