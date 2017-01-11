
//includes
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//declarations
typedef struct 
{
	unsigned char d;
	unsigned char m;
	unsigned char y;
} dateStamp;

typedef struct 
{
	unsigned char h;
	unsigned char m;
	unsigned char s;
} timeStamp;

typedef struct
{
	dateStamp d;
	timeStamp t;
} dateTimeStamp;

typedef struct
{
	unsigned int e[4];
} energy_t;

typedef struct
{
	dateTimeStamp dts;
	energy_t t[4];
	unsigned char status;
	unsigned int ratio;
} meterage_t;

typedef struct
{
	dateTimeStamp dts;
	energy_t p;
	unsigned char status;
	unsigned int ratio;
} profile_t;

//
// main
//
int main(int argc, char* argv[])
{
	int id;
	int type;
	int fd;
	char fileName[64];
	
	struct stat st;
	meterage_t * m = NULL;
	profile_t * p = NULL;
	
	int sr = 0;
	int r = 0;
	int b = 0;
	int bi = 0;

	//check argc	
	if (argc < 3)
	{
		printf ("use dater <type> <id> to view dates\n");
		printf ("    where <type>:\n");
		printf ("                 0 - current \n");
		printf ("                 1 - day \n");
		printf ("                 2 - month \n");
		printf ("                 3 - profile\n");
		printf ("    where <id>:\n");
		printf ("                 dbid of counter \n");

		exit(0);
	}
	
	//get argc
	type = atoi(argv[1]);
	id = atoi(argv[2]);
	
	//create filename
	memset(fileName, 0, 64);
	switch (type)
	{
		case 0:
			sprintf (fileName, "/tmp/current_%d", id);
			break;	
		case 1:
			sprintf (fileName, "/home/root/fdb/day_%d", id);
			break;	
		case 2:
			sprintf (fileName, "/home/root/fdb/month_%d", id);
			break;
		case 3:
			sprintf (fileName, "/home/root/fdb/profile_%d", id);
			break;
		default:
			printf ("unhandled type of files storages, quit...\n");
			exit(0);
			break;
	}
	
	//open file
	fd = open(fileName, O_RDONLY);
	if (fd == -1)
	{
		printf ("Error opening file [%s], file is not exist in fdb\n", fileName);
		exit(0);
	}

	//get size of file
	fstat(fd, &st);
	sr = st.st_size;
	if (sr == 0)
	{
		printf ("File [%s] is empty\n", fileName);
		close (fd);
		exit(0);
	}
		
	//allocate struct for data in file
	if (type == 3)
	{
		p = (profile_t *)malloc(sr);	
		if (p == NULL)
		{
			printf ("error allocate mem\n");
			close(fd);
			exit(1);
		}
	}
	else
	{
		m = (meterage_t *)malloc(sr);
		if (m == NULL)
		{
			printf ("error allocate mem\n");
			close (fd);
			exit(1);
		}
	}

	//read file data
	if (type == 3)
	{
		r = read(fd, p, sr);
	        b = r / sizeof(profile_t);
	}
	else
	{
		r = read(fd, m, sr);
        	b = r / sizeof(meterage_t);
	}

	//check blocks
	if ((r == 0) || (b == 0))
	{
		printf ("Error during reading file [%s]\n", fileName);
		close(fd);
		exit (1);
	}
	
	//close file	
	close (fd);	

	//print out dates from records in loop
	for (; bi < b; bi++)
	{
		if (type ==3)
		{
			printf ("record %02d / dts %02d.%02d.%02d %02d:%02d:%02d, status %02x\n", 
					bi, 
					p[bi].dts.d.d, p[bi].dts.d.m, p[bi].dts.d.y,
					p[bi].dts.t.h, p[bi].dts.t.m, p[bi].dts.t.s, 
					p[bi].status);
		}
		else
		{
			printf ("record %02d / dts %02d.%02d.%02d %02d:%02d:%02d, status %02x\n", 	
					bi, 
					m[bi].dts.d.d, m[bi].dts.d.m, m[bi].dts.d.y,
					m[bi].dts.t.h, m[bi].dts.t.m, m[bi].dts.t.s, 
					m[bi].status);
		}
	}	
	
	
	
	//free mem
	if (type == 3)
	{
		free (p);
		p = NULL;
	}
	else
	{
		free (m);
		m = NULL;
	}	
	
	printf ("...done\n");
	exit (0);
}
