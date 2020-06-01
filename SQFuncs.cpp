
#define DEFAULT_BUFLEN 40960
#define PI 3.1415926
#include <stdlib.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include "SQFuncs.h"
#include <cstdio>
#include <tgmath.h>
#include <pthread.h>

extern PluginFuncs* VCMP;
extern HSQAPI sq;
extern HSQUIRRELVM v;
bool debug = false;
int id = 0;
short int PORT = 8192;
int FirstMessageLength(unsigned char* message)
{
	int payload = message[1] * 256 + message[2];
	int n;
	if (message[0] == 32 || message[0] == 96)n = 7;
	else if (message[0] == 64)n = 3;
	else if (message[0] == 0)n = 0;
	else if (message[0] == 112)n = 17; // 7 + 4 + 2 + 4
	else
	{
		return -1;
	}
	int totlen = 3 + n + payload / 8;
	return totlen;
}
int countMessage(unsigned char* rmsg, int len)
{//This is not a packet.this is raknet message.
   // which is part of packet.
	//    first 4 octects not there.

	int totlen = FirstMessageLength(rmsg);
	if (totlen == -1)
	{
		printf("Unknown message");
		return 1;
	}
	if (len == totlen)
		return 1;
	else if (totlen < len)
	{
		unsigned char* newmsg = rmsg + totlen;
		return 1 + countMessage(newmsg, len - totlen);
	}
	else
	{
		return 1;
		printf("Error\n");
	}
	//total len = 3+n+p/8
}
void leftshift(char* p, unsigned char n)
{
	//eg. 09 64 03 27 --> 96 40 32 70
	//leftshift( p, 3 );
	p[0] = p[1] << 4;
	for (int i = 0; i < n; i++)
	{
		p[i] += p[i + 1] / 16;
		p[i + 1] = p[i + 1] << 4;
	}
}
void encpld(int p, char* a, char* b)//encode payload
{
	*b = p % 256;
	*a = (p - p % 256) / 256;
}
void encodeIndex(int q, char* a, char* b, char* c)
{
	*a = q % 256;
	*b = (q - q % 256) / 256;
	*c = (q - q % 65536) / 65536;
}
void encodeCoord(float x, char* a, char* b, char* c, char* d)
{
	if (x == 0)
	{
		*a = 0, * b = 0, * c = 0, * d = 0; return;
	}
	short int c1;
	if (x > 0) c1 = 64;
	else { c1 = 192, x = -x; }

	int y = floor(log2(x));
	bool bl = false;
	if (y % 2 == 0)
	{
		bl = true;
		y -= 1;
	}
	*a = c1 + (y - 1) / 2;
	float y2;
	if (bl == false)
		y2 = (x - pow(2, y)) / pow(2, y + 1);
	else
		y2 = x / pow(2, y + 2);
	*b = floor(y2 * 256);
	*c = floor((y2 * 256 - floor(y2 * 256)) * 256);
	*d = floor(((y2 * 256 - floor(y2 * 256)) * 256 - (unsigned char)(*c)) * 256);

}
class Actor
{
public:
	char name[10];
	char file[20];
	int clientId;//player.ID
	int ConnectSocket = -1;
	int psn;
	int msi;//message sequnce index
	int rel_mes_no;//reliable message number
	int guid; //also can be used actors[guid]
	int skinId;//initial skin id. 
	bool pending = false;
	bool pending2 = false;
	float px = -232.0314, py = -442.6181, pz = 32.7944, angle = 0.0;
	unsigned char health = 100; // 0 to 255
	int cind = 2;//change index (?
	char action = 0;
	bool joined = false;
	//01 for change in pos, 11 running, 10 walking
	Actor()
	{

	}
	~Actor()
	{

	}

	void Create(int j, int k)
	{
		guid = j;
		skinId = k;
	}
	void Connect();
	void SendConnectedPong(unsigned char* packet);
	void SendACK(char* recvbuf);
	void Send93();
	void SendA6();
	void SendA7();
	void encodeAngle(char* a, char* b);
};

Actor actors[512];
void Actor::SendACK(char* recvbuf)
{
	unsigned char* ack;
	ack = new unsigned char[8];
	ack[0] = '\xC0';
	ack[1] = '\x00';
	ack[2] = '\x01';
	ack[3] = '\x01'; // min == max
	ack[4] = (unsigned char)recvbuf[1];// packet sequence number
	ack[5] = (unsigned char)recvbuf[2];// packet sequence number
	ack[6] = (unsigned char)recvbuf[3];// packet sequence number
	ack[7] = '\x00';
	send(ConnectSocket, (char*)ack, 8, 0);
	delete[] ack;  // When done, free memory pointed to by a.
	ack = NULL;
}
long gettickcount()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
void Actor::SendConnectedPong(unsigned char* packet)
{
	//Going to send Connected Pong
	int k = 0;
	if (packet[0] == 64)k = 3;
	unsigned char* d;
	d = new unsigned char[24];
	d[0] = '\x84';
	d[1] = psn % 256;
	d[2] = (psn % 65536 - d[1]) / 256;
	d[3] = (psn - psn % 65536) / 65536;
	d[4] = '\x00';
	d[5] = '\x00';
	d[6] = '\x88';
	d[7] = '\x03';//connected pong
	d[8] = packet[4 + k];
	d[9] = packet[5 + k];
	d[10] = packet[6 + k];
	d[11] = packet[7 + k];
	d[12] = packet[8 + k];
	d[13] = packet[9 + k];
	d[14] = packet[10 + k];
	d[15] = packet[11 + k];
	long long time = gettickcount();
	d[23] = time % 256;
	int y;
	y = d[23];
	d[22] = ((time - y) % 65536) / 256;
	y = y + 256 * d[22];
	d[21] = ((time - y) % (int)pow(16, 6)) / 65536;
	y = y + 65536 * d[21];
	d[20] = ((time - y) % (int)pow(16, 8)) / pow(16, 6);
	y = y + pow(16, 6) * d[20];
	d[19] = ((time - y) % (int)pow(16, 10)) / pow(16, 8);
	y = y + pow(16, 8) * d[19];
	if (y == psn)//by this time, it might have happened.
	{
		d[18] = 0;
		d[17] = 0;
		d[16] = 0;
	}
	else
	{
		d[18] = ((time - y) % (int)pow(16, 12)) / pow(16, 10);
		y = y + pow(16, 10) * d[18];
		d[17] = ((time - y) % (int)pow(16, 14)) / pow(16, 12);
		y = y + pow(16, 12) * d[17];
		d[16] = ((time - y) % (int)pow(16, 16)) / pow(16, 14);
	}
	send(ConnectSocket, (char*)d, 24, 0);
	delete[] d;
	d = NULL;
	psn += 1;
}
void Actor::SendA6()
{
	char pkt[15];
	pkt[0] = 132;
	encodeIndex(psn, &pkt[1], &pkt[2], &pkt[3]);
	psn++;
	pkt[4] = 96;//reliability;
	encpld(8, &pkt[5], &pkt[6]);//PAYLOAD
	encodeIndex(rel_mes_no, &pkt[7], &pkt[8], &pkt[9]);//message seq. indx.
	rel_mes_no++;
	encodeIndex(1, &pkt[10], &pkt[11], &pkt[12]);//ordering index
	pkt[13] = 5;//ordering channel
	pkt[14] = 166;//0xa6
	int iResult = send(ConnectSocket, pkt, sizeof(pkt), 0);
}
void Actor::SendA7()
{
	char pkt[11];
	pkt[0] = 132;
	encodeIndex(psn, &pkt[1], &pkt[2], &pkt[3]);
	psn++;
	pkt[4] = 64;//reliability;
	encpld(8, &pkt[5], &pkt[6]);//PAYLOAD
	encodeIndex(rel_mes_no, &pkt[7], &pkt[8], &pkt[9]);//message seq. indx.
	rel_mes_no++;
	pkt[10] = 167;//0xa7
	int iResult = send(ConnectSocket, pkt, sizeof(pkt), 0);
}
void Actor::Send93()
{
	char pkt[36];
	pkt[0] = 132;
	encodeIndex(psn, &pkt[1], &pkt[2], &pkt[3]);
	psn++;
	pkt[4] = 32;//reliability;
	encpld(22 * 8, &pkt[5], &pkt[6]);//PAYLOAD
	encodeIndex(msi, &pkt[7], &pkt[8], &pkt[9]);//message seq. indx.
	msi++;
	encodeIndex(2, &pkt[10], &pkt[11], &pkt[12]);//ordering index
	pkt[13] = 0;//ordering channel
	pkt[14] = 147;//0x93
	pkt[15] = 0;
	encodeIndex(cind, &pkt[18], &pkt[17], &pkt[16]);
	pkt[19] = action;
	encodeCoord(px, &pkt[20], &pkt[21], &pkt[22], &pkt[23]);
	encodeCoord(py, &pkt[24], &pkt[25], &pkt[26], &pkt[27]);
	encodeCoord(pz, &pkt[28], &pkt[29], &pkt[30], &pkt[31]);
	encodeAngle(&pkt[32], &pkt[33]);
	pkt[34] = health;
	pkt[35] = 3;
	int iResult = send(ConnectSocket, pkt, sizeof(pkt), 0);
}

void Actor::encodeAngle(char* a, char* b)
{
	int val = floor(angle * 4096 * 4 / PI + 32767);
	encpld(val, a, b);
}
void Actor::Connect()
{
	int recvbuflen = DEFAULT_BUFLEN;
	char recvbuf[DEFAULT_BUFLEN]; 
	
	int iResult;
	

	int dummy = 8192;
	//beginning..
	const char sendbuf1[] = "\x56\x43\x4d\x50\xcf\xb4\xd4\x61\x00\x20\x69";

	//open connection request 1
	const char sendbuf2[] = "\x05\x00\xff\xff\x00\xfe\xfe\xfe\xfe\xfd\xfd\xfd\xfd\x12\x34\x56\x78\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

	//open connection request 2
	char sendbuf3[] = "\x07\x00\xff\xff\x00\xfe\xfe\xfe\xfe\xfd\xfd\xfd\xfd\x12\x34\x56\x78\x04\x30\x4b\x2b\x9e\x20\x00\x05\xd4\x0d\xd0\x00\x0c\xa8\x42\x9a\x00";

	sendbuf3[33] = (char)(guid % 256);
	sendbuf3[32] = (char)((guid - guid % 256) / 256);
	sendbuf3[23] = dummy % 256;// non essential port data
	sendbuf3[22] = (dummy - sendbuf3[23]) / 256;//future

	//connection request
	const char sendbuf4[] = "\x84\x00\x00\x00\x40\x00\x98\x00\x00\x00\x09\x0d\xd0\x00\x0c\xa8\x42\x9a\xd8\x00\x00\x00\x00\x03\x3d\x85\xcc\x00\x00";

	const char sendbuf5[] = "\x84\x01\x00\x00\x60\x02\xf0\x01\x00\x00\x00\x00\x00\x00\x13\x04\x30\x4b\x2b\x9e\x20\x00\x04\x56\x01\x1e\x25\xdc\x07\x04\x3f\x57\xd4\xf9\xdc\x07\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x04\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x04\xa5\x27\x00\x00\x00\x00\x03\x3d\x87\x0e\x00\x00\x48\x00\x00\x00\x00\x00\x03\x3d\x87\x0e";
	const char sendbuf6[] = "\x84\x02\x00\x00\x00\x00\x48\x00\x00\x00\x00\x00\x03\x3d\x87\x0e";
	const char sendbuf7[] = "\xc0\x00\x01\x01\x00\x00\x00";

	const char sendbuf8[] = "\x84\x03\x00\x00\x40\x03\x10\x02\x00\x00\x98\x03\x0d\xc5\x32\x00\x01\x07\x48\x04\x68\x61\x62\x69\x00\x94\x1c\x1a\x98\xb1\xb0\x99\x18\x19\x1b\x9a\x31\x99\x32\x32\x1c\xb2\x1c\x31\x33\x18\x99\x99\x33\x18\x9a\xb1\xb0\x9a\x9b\x9a\x32\x98\x9a\x19\x32\x1c\xb3\x19\x98\x9c\xca\x18\x4e\x0c\x59\x4c\x19\x4c\xcd\x19\x4d\x8e\x4d\x59\x4c\x0d\x0d\xcc\x8e\x0d\x4e\x0d\x0e\x0c\xcc\x19\x0e\x58\xcd\x99\x59\x19\x19\x19\x19\x0c\xcc\x4c\xcd\x0e\x4c\x40";

	unsigned char* namepacket;
	namepacket = new unsigned char[104 + strlen(name)];
	int pload = (94 + strlen(name)) * 8;

	for (int i = 0; i <= 4; i++)
	{
		namepacket[i] = (unsigned char)sendbuf8[i];
	}

	namepacket[6] = pload % 256;
	namepacket[5] = (pload - namepacket[6]) / 256;

	for (int i = 7; i <= 18; i++)
	{
		namepacket[i] = (unsigned char)sendbuf8[i];
	}
	namepacket[19] = strlen(name);
	for (int i = 0; i < strlen(name); i++)
	{
		namepacket[20 + i] = (unsigned char)name[i];
	}
	for (int i = 0; i <= 83; i++)
	{
		namepacket[20 + strlen(name) + i] = (unsigned char)sendbuf8[20 + 4 + i];
	}


	const char sendbuf9[] = "\x84\x04\x00\x00\x00\x00\x88\x03\x00\x00\x00\x00\x00\x04\xa6\x71\x00\x00\x00\x00\x03\x3d\x87\xe2\x00\x00\x88\x03\x00\x00\x00\x00\x00\x04\xa6\x71\x00\x00\x00\x00\x03\x3d\x87\xe2";
	const char sendbuf10[] = "\xc0\x00\x01\x00\x01\x00\x00\x02\x00\x00";
	const char sendbuf11[] = "\xc0\x00\x01\x01\x03\x00\x00";
	const char sendbuf12[] = "\x84\x05\x00\x00\x60\x00\x20\x03\x00\x00\x01\x00\x00\x00\xb9\x00\x00\x00\x60\x00\x10\x04\x00\x00\x00\x00\x00\x05\xa5\x00";
	const char sendbuf13[] = "\xc0\x00\x01\x00\x04\x00\x00\x05\x00\x00";
	const char sendbuf14[] = "\x84\x06\x00\x00\x40\x00\x48\x05\x00\x00\xba\x40\x32\x34\x31\xb5\x6f\xd8\x3c";
	const char sendbuf15[] = "\x84\x07\x00\x00\x60\x00\x08\x06\x00\x00\x01\x00\x00\x05\xa6";
	const char sendbuf16[] = "\xc0\x00\x01\x01\x06\x00\x00";
	const char sendbuf17[] = "\x84\x08\x00\x00\x40\x00\x08\x07\x00\x00\xa7";
	char sendbuf18[] = "\x84\x09\x00\x00\x20\x00\xb0\x00\x00\x00\x02\x00\x00\x00\x93\x00\x00\x00\x01\x00\xc3\x68\x08\x0a\xc3\xdd\x4f\x1e\x42\x03\x2d\x75\x7f\xff\x64\x03";
	
	struct sockaddr_in     servaddr;
	// Creating socket file descriptor 
	if ((ConnectSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// Filling server information 
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = INADDR_ANY;

	int n;
	unsigned int len;
	iResult = connect(ConnectSocket, (const struct sockaddr*) & servaddr,
		sizeof(servaddr));
	if (iResult == -1)
	{
		printf("connect failed.");
		close(ConnectSocket);
		return;
	}
	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf1, sizeof(sendbuf1) - 1, 0);
	if (iResult == -1) {
		printf("send failed with error: %s\n", strerror(errno));
		close(ConnectSocket);
	}
	send(ConnectSocket, sendbuf2, sizeof(sendbuf2) - 1, 0);
	send(ConnectSocket, sendbuf3, sizeof(sendbuf3) - 1, 0);
	send(ConnectSocket, sendbuf4, sizeof(sendbuf4) - 1, 0);

	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			unsigned char* packet = NULL;   // Pointer to int, initialize to nothing.

			packet = new unsigned char[iResult];  // Allocate n ints and save ptr in a.


			//printf("No:of bytes received is %d\n", iResult);
			for (int i = 0; i < iResult; i++)
			{
				int c = recvbuf[i];
				c = c < 0 ? 128 + 128 + c : c;
				packet[i] = c;
			}
			if (packet[0] == 132 && packet[4] == 96 && packet[14] == 16)
			{
				//packet is connection request accepted. msg id 0x10
				clientId = packet[23] + packet[22] * 256;
				//printf("Raknet system index is %d", clientId);
				break;
			}
		}
	} while (iResult > 0);

	send(ConnectSocket, sendbuf5, sizeof(sendbuf5) - 1, 0);
	send(ConnectSocket, sendbuf6, sizeof(sendbuf6) - 1, 0);
	send(ConnectSocket, sendbuf7, sizeof(sendbuf7) - 1, 0);

	send(ConnectSocket, (char*)namepacket, 104 + strlen(name), 0);
	delete[] namepacket;  // When done, free memory pointed to by a.
	namepacket = NULL;
	send(ConnectSocket, sendbuf9, sizeof(sendbuf9) - 1, 0);
	send(ConnectSocket, sendbuf10, sizeof(sendbuf10) - 1, 0);
	send(ConnectSocket, sendbuf11, sizeof(sendbuf11) - 1, 0);
	send(ConnectSocket, sendbuf12, sizeof(sendbuf12) - 1, 0);

	send(ConnectSocket, sendbuf13, sizeof(sendbuf13) - 1, 0);
	send(ConnectSocket, sendbuf14, sizeof(sendbuf14) - 1, 0);
	send(ConnectSocket, sendbuf15, sizeof(sendbuf15) - 1, 0);
	send(ConnectSocket, sendbuf16, sizeof(sendbuf16) - 1, 0);
	send(ConnectSocket, sendbuf17, sizeof(sendbuf17) - 1, 0);
	send(ConnectSocket, sendbuf18, sizeof(sendbuf18), 0);
	psn = 10;
	msi = 1;
	rel_mes_no = 8;
	if (pending == true)
	{
		Send93();
		pending = false;
	}


	// Receive until the peer closes the connection
	do {
		if (pending == true)
		{
			Send93();
			pending = false;
		}
		if (pending2 == true)
		{
			SendA6();
			SendA7();
			pending2 = false;
			pending = true;
		}
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			int i = 0;
			unsigned char* packet = NULL;   // Pointer to int, initialize to nothing.
			packet = new unsigned char[iResult];  // Allocate n ints and save ptr in a.

			//printf("No:of bytes received is %d\n", iResult);
			for (i = 0; i < iResult; i++)
			{
				int c = recvbuf[i];
				c = c < 0 ? 128 + 128 + c : c;
				packet[i] = c;
			}
			if (packet[0] == 192)//acknowledgement
			{
				if (joined == false)
				{
					if (packet[3] == 1 && packet[4] >= 9
						|| packet[3] == 0 && packet[7] >= 9)
					{
						if (skinId != -1)
						{
							VCMP->SetPlayerSkin(clientId, skinId);
							if (VCMP->GetPlayerSkin(clientId) == skinId)
							{
								joined = true;
							}
							else printf("Setting skin failed\n");
						}

						else joined = true;

					}
				}

			}
			else if (packet[0] == 132 || packet[0] == 140
				|| packet[0] == 136)
			{
				//0x8c continuously send packet
				//0x88 also there
				if (packet[4] != 0 && packet[4] != 32 && packet[4] != 64
					&& packet[4] != 96 && packet[4] != 112)continue;
				unsigned char* message = packet + 4;
				char t = countMessage(message, iResult - 4);
				bool ackSend;
				for (int i = 0; i < t; i++)
				{
					ackSend = false;
					if (message[0] == 64 || message[0] == 96
						|| message[0] == 112)
					{
						if (ackSend == false)
						{
							SendACK(recvbuf);
							ackSend = true;
						}
					}

					if ((message[0] == 0 && message[3] == 0) ||
						(message[0] == 64 && message[6] == 0))//reliable ping
					{
						//Reliable Ping or Unreliable Ping. who knows?
						SendConnectedPong(message);
					}
					message = message + FirstMessageLength(message);

				}

			}
			else
			{
				//printf("Well, it says it is %d\n", packet[0]);
			}

			delete[] packet;  // When done, free memory pointed to by a.
			packet = NULL;     // Clear a to prevent using invalid memory reference.
		}
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %s\n", strerror(errno));

	} while (iResult > 0);

	// cleanup
	close(ConnectSocket);
}
void *connect(void * ptr)
{
	Actor* t = (Actor*)ptr;
	t->Connect();
}

_SQUIRRELDEF(SQ_create_actor) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 2) {
		const SQChar* name;
		sq->getstring(v, 2, &name);
		int n = id; //global id

		for (int i = 0; i < strlen(name); i++)
		{
			actors[n].name[i] = name[i];
		}
		for (int i = strlen(name); i < 10; i++)
		{
			actors[n].name[i] = 0;
		}
		actors[n].Create(id, -1);
		id += 1;
		sq->pushinteger(v, n);
		pthread_t thread1;
		pthread_create(&thread1, NULL, connect, (void*)&actors[n]);
		return 1;
	}
	else if (iArgCount == 3) {
		const SQChar* name;
		sq->getstring(v, 2, &name);
		SQInteger skinId;
		sq->getinteger(v, 3, &skinId);
		int n = id; //global id
		for (int i = 0; i < strlen(name); i++)
		{
			actors[n].name[i] = name[i];
		}
		for (int i = strlen(name); i < 10; i++)
		{
			actors[n].name[i] = 0;
		}
		actors[n].Create(id, skinId);
		id += 1;
		sq->pushinteger(v, n);
		pthread_t thread1;
		pthread_create(&thread1, NULL, connect, (void*)&actors[n]);
		return 1;
	}
	else if (iArgCount == 7) {
		const SQChar* name;
		sq->getstring(v, 2, &name);
		SQInteger skinId;
		sq->getinteger(v, 3, &skinId);
		int n = id; //global id
		SQFloat x, y, z, angle;
		sq->getfloat(v, 4, &x);
		sq->getfloat(v, 5, &y);
		sq->getfloat(v, 6, &z);
		sq->getfloat(v, 7, &angle);
		for (int i = 0; i < strlen(name); i++)
		{
			actors[n].name[i] = name[i];
		}
		for (int i = strlen(name); i < 10; i++)
		{
			actors[n].name[i] = 0;
		}
		actors[n].Create(id, skinId);
		actors[n].px = x, actors[n].py = y, actors[n].pz = z;
		actors[n].angle = angle;
		actors[n].pending = true;//need to set position angle later.
		id += 1;
		sq->pushinteger(v, n);
		pthread_t thread1;
		pthread_create(&thread1, NULL, connect, (void*)&actors[n]);
		return 1;
	}
	return 0;

}
_SQUIRRELDEF(SQ_set_actor_angle) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 3) {
		SQInteger a;
		sq->getinteger(v, 2, &a);//actor id
		SQFloat b;
		sq->getfloat(v, 3, &b);//angle
		actors[a].angle = b;
		actors[a].pending = true;
		//actors[a].Send93();
		sq->pushinteger(v, 1);
		return 1;
	}
	sq->pushbool(v, SQFalse);
	return 1;
}
_SQUIRRELDEF(SQ_spawn_actor) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 2) {
		SQInteger a;
		sq->getinteger(v, 2, &a);//actor id
		actors[a].pending2 = true;
		sq->pushinteger(v, 1);
		return 1;
	}
	sq->pushbool(v, SQFalse);
	return 1;
}
_SQUIRRELDEF(SQ_send_cmd) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 3) {
		SQInteger n;
		const SQChar* cmd;
		sq->getinteger(v, 2, &n);//actor id
		sq->getstring(v, 3, &cmd);
		unsigned char len = strlen(cmd);
		char* pkt;
		pkt = new char[20 + len];
		pkt[0] = 132;
		encodeIndex(actors[n].psn, pkt + 1, pkt + 2, pkt + 3);
		actors[n].psn++;
		pkt[4] = 64;//reliability;
		encpld((len + 3) * 8, pkt + 5, pkt + 6);//PAYLOAD
		encodeIndex(actors[n].rel_mes_no, pkt + 7, pkt + 8, pkt + 9);//message seq. indx.
		actors[n].rel_mes_no++;
		pkt[10] = 164;//0xa4
		pkt[11] = 0;  pkt[12] = len;
		for (int i = 0; i < len; i++)
		{
			pkt[13 + i] = cmd[i];
		}
		//next message 0x9c
		pkt[13 + len] = 64;
		encpld(8, pkt + 13 + len + 1, pkt + 13 + len + 2);//PAYLOAD
		encodeIndex(actors[n].rel_mes_no, pkt + 13 + len + 3, pkt + 13 + len + 4, pkt + 13 + len + 5);//message seq. indx.
		actors[n].rel_mes_no++;
		pkt[13 + len + 6] = 156;//0x9c
		int iResult = send(actors[n].ConnectSocket, pkt, 20 + len, 0);
		sq->pushinteger(v, iResult);
		return 1;
	}
	sq->pushbool(v, SQFalse);
	return 1;
}
_SQUIRRELDEF(SQ_set_port) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 2) {
		SQInteger port;
		sq->getinteger(v, 2, &port);
		if (port>=0 and port<=65536)
		{
			PORT = port;
			printf("Port set to %d\n", PORT);
			sq->pushinteger(v, 1);
			return 1;
		}
		else
			printf("Error. Usage:set_port(8193)\n");

	}
	sq->pushbool(v, SQFalse);
	return 1;
}
_SQUIRRELDEF(SQ_debug) {
	SQInteger iArgCount = sq->gettop(v);
	if (iArgCount == 2) {
		SQBool b;
		sq->getbool(v, 2, &b);
		if (b == true)debug = true;
		else debug = false;
		sq->pushbool(v, SQTrue);
		return 1;
	}
	return 0;
}
SQInteger RegisterSquirrelFunc(HSQUIRRELVM v, SQFUNCTION f, const SQChar* fname, unsigned char ucParams, const SQChar* szParams) {
	char szNewParams[32];

	sq->pushroottable(v);
	sq->pushstring(v, fname, -1);
	sq->newclosure(v, f, 0); /* create a new function */

	if (ucParams > 0) {
		ucParams++; /* This is to compensate for the root table */
		sprintf(szNewParams, "t%s", szParams);
		sq->setparamscheck(v, ucParams, szNewParams); /* Add a param type check */
	}

	sq->setnativeclosurename(v, -1, fname);
	sq->newslot(v, -3, SQFalse);
	sq->pop(v, 1);

	return 0;
}
void RegisterMyFuncs(HSQUIRRELVM v) {
	RegisterSquirrelFunc(v, SQ_create_actor, "create_actor", 0, 0);
	RegisterSquirrelFunc(v, SQ_set_actor_angle, "set_actor_angle", 0, 0);
	RegisterSquirrelFunc(v, SQ_spawn_actor, "spawn_actor", 0, 0);
	RegisterSquirrelFunc(v, SQ_set_port, "set_port", 0, 0);
	RegisterSquirrelFunc(v, SQ_send_cmd, "send_cmd", 0, 0);
	RegisterSquirrelFunc(v, SQ_debug, "debug", 0, 0);
}