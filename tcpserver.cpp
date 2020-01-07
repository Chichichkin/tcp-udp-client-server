#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN 
	#include <windows.h> 
	#include <winsock2.h>  // Директива линковщику: использовать библиотеку сокетов   
	#pragma comment(lib, "ws2_32.lib")
#else // LINUX
	#include <fstream>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
	#include <poll.h>
	#include <netdb.h>
	#include <errno.h>
	#include <unistd.h>
	#include <ifaddrs.h>
	#include <fcntl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct clients
{
	int s;
	unsigned int ip;
};
#define MAXEVENTS_PER_CALL (16)
#define MAX_CLIENTS (256)
bool exittime = false,end = false;
void s_close(int s) 
{ 
	#ifdef _WIN32  
		closesocket(s); 
	#else
		close(s);
	#endif 
}
int set_non_block_mode(int s)
{
	#ifdef _WIN32  
		unsigned long mode = 1;  
		return ioctlsocket(s, FIONBIO, &mode); 
	#else  
		int fl = fcntl(s, F_GETFL, 0);
		return fcntl(s, F_SETFL, fl | O_NONBLOCK); 
	#endif 
}
int init() 
{
	#ifdef _WIN32  
		// Для Windows следует вызвать WSAStartup перед началом использования сокетов  
		WSADATA wsa_data;  
		return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data)); 
	#else   
		return 1; // Для других ОС действий не требуется 
	#endif 
} 
void deinit() 
{
	#ifdef _WIN32  
		// Для Windows следует вызвать WSACleanup в конце работы   
		WSACleanup();  
	#else  
		// Для других ОС действий не требуется 
	#endif 
}
#ifdef _WIN32  
	int flags = 0; 
#else  
	int flags = MSG_NOSIGNAL; 
#endif


int put_mod(int socket, unsigned int ip, int port,int *check);
void get_mod(int s);


struct client_ctx
{
	int socket; // ���������� ������
	unsigned char in_buf[4096]; // ����� �������� ������
	int received; // ������� ������
	unsigned char out_buf[4096]; // ����� ������������ ������
	int out_total; // ������ ������������ ������
	int out_sent; // ������ ����������
};
// ������ ��������, �������� ���������� � ������������ ��������
struct client_ctx g_ctxs[1 + MAX_CLIENTS];

int sock_err(const char* function,int s)
{
	int err = errno;
	fprintf(stderr,"%s: socket error: %d\n",function,err);
	return -1;
}



int main(int argc, char* argv[])
{
	const int N = 256;
	if (argc != 2)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}
	int port = atoi(argv[1]);
	struct sockaddr_in addr;
	init();
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock <= 0)
		return sock_err("socket",sock);
	set_non_block_mode(sock);
	printf("socket\n");
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0)
	{
		return sock_err("bind",sock);
	}
	printf("bind\n");
	// ������ �������������
	if (listen(sock, 1) < 0)
	{
		return sock_err("listen",sock);
	}
	printf("listen\n");

	printf("non block\n");
	
	fd_set rfd;
	int nfds = sock;
	int i, num_sock, num_lines = 0;
	int check = 1;
	struct timeval tv = { 1, 0 };
	int res_select;
	struct sockaddr_in addr_cs;
	memset(&addr_cs, 0, sizeof(addr_cs));
	char msg[2] = { 'o','k' };
	char *ip = inet_ntoa(addr.sin_addr);
	char buf[4];
	int rcv,snd;
	int gear = 0;
	bool mod = false;
	struct clients cs[16];
	
	num_lines = 0;
	
	while (!exittime)
	{
		end = false;
		FD_ZERO(&rfd);
		FD_SET(sock, &rfd);
		for (i = 0; i < gear; i++)
		{
			FD_SET(cs[i].s, &rfd);
			if (nfds < cs[i].s)
				nfds = cs[i].s;
		}
		res_select  = select(nfds + 1, &rfd, 0, 0, &tv);
		if (FD_ISSET(sock, &rfd))
		{
			#ifdef _WIN32  
				int len = sizeof(addr);
			#else  
				unsigned int len = sizeof(addr);
			#endif

			cs[gear].s = accept(sock, (struct sockaddr*)&addr, &len);
			if (cs[gear].s < 0)
			{
				return sock_err("accept", sock);
			}
			set_non_block_mode(cs[gear].s);
			cs[gear].ip = ntohl(addr.sin_addr.s_addr);
			fprintf(stdout, "Peer connected : %d.%d.%d.%d:%i\n", (cs[gear].ip >> 24) & 0xFF, (cs[gear].ip >> 16) & 0xFF, (cs[gear].ip >> 8) & 0xFF, (cs[gear].ip) & 0xFF, port);
			gear++;
		}
		for (i = 0; i < gear; i++)
		{
			if (FD_ISSET(cs[i].s, &rfd))
			{
				if (!mod)
				{
					rcv = recv(cs[i].s, buf, 3, flags);
					if (rcv < 0)
						printf("Error recv\n");
					buf[3] = '\0';
					if (!strncmp("put", buf, 4))
					{
						printf("Put mode\n");
						mod = true;
					}
					else
					{
						break;
					}
				}
				printf("reciving msg\n");
				num_lines += put_mod(cs[i].s, cs[i].ip, port, &check);
				//printf("hi-hi bitch\n");
				if (end || exittime)
				{
					printf("break free gear = %d\n",gear);
					printf("Sending OK. num_lines = %d\n", num_lines);
					snd = send(cs[i].s, msg, 2, flags);
					gear--;
					mod = false;
					num_lines = 0;
					break;
				}
				printf("Sending OK. num_lines = %d\n", num_lines);
				snd = send(cs[i].s, msg, 2, flags);
				//printf("fuck it %d snd = %d\n",i,snd);
			}
		}
	}
	s_close(sock);
	deinit();
	printf("Client disconnect: function successuflly ended\n");
}

int put_mod(int socket, unsigned int ip,int port,int *check)
{
	
	char msg[500000],num_msg[4],phn1[13],phn2[13],hour[3],min[3],sec[3];
	char simb[2];
	int temp;
	memset(&msg, 0, sizeof(msg));
	int rcv = recv(socket, num_msg, 4, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}

	//phone 1
	rcv = recv(socket, phn1, 12, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	phn1[12]='\0';
	//printf("ph1 = %s\n",phn1);
	//phone 2
	rcv = recv(socket, phn2, 12, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	phn2[12]='\0';
	//printf("ph2 = %s\n", phn2);
	//hour
	rcv = recv(socket, simb, 1, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	temp = simb[0];
	hour[1] = temp % 10 + 48;
	hour[0] = temp / 10 + 48;
	hour[2]='\0';
	//printf("hour = %s\n", hour);
	

	//min
	rcv = recv(socket, simb, 1, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	temp = simb[0];
	min[1] = temp % 10 + 48;
	min[0] = temp / 10 + 48;
	min[2] = '\0';
	//printf("min = %s\n", min);

	//sec
	rcv = recv(socket, simb, 1, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	temp = simb[0];
	sec[1] = temp % 10 + 48;
	sec[0] = temp / 10 + 48;
	sec[2] = '\0';
	//printf("sec = %s\n", sec);

	//msg
	rcv = recv(socket, msg, 1, flags);
	if (rcv == 0)
	{
		end = true;
		return 0;
	}
	while (msg[rcv - 1] != '\0'&& rcv < 1000000)
	{
		rcv += recv(socket, msg + rcv, 1, flags);
		if (rcv == 0)
		{
			end = true;
			return 0;
		}
	}
	msg[rcv] = '\0';
	//printf("%d.%d.%d.%d:%u %s %s %s:%s:%s %s\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port, phn1, phn2, hour, min, sec, msg);
	FILE* f = fopen("msg.txt", "a+");;
	if (!f)
	{
		printf("Problem with opening file");
		check = 0;
		return 0;
	}
	fprintf(f, "%d.%d.%d.%d:%u %s %s %s:%s:%s %s\n",(ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port, phn1, phn2, hour, min, sec, msg);
	fclose(f);
	//printf("end\n");
	if (strncmp("stop", msg, 4) == 0 && strlen(msg) == 4)
	{
		printf("Stop msg\n");
		*check = 0;
		exittime = true;
		//end = true;
	}
	return 1;
}
void get_mod(int s)
{
	FILE* f = fopen("msg.txt", "r");
	char buff[4096];
	int NumLine = 0;
	char num1[12], num2[12], hour[1], min[1], sec[1], msg[4061];
	int i = 0, ihour = 0, imin = 0, isec = 0, flag;
	char char_line_num[4];
	while (fgets(buff, 4096, f))
	{

		NumLine = htons(NumLine);
		char_line_num[0] = (NumLine >> 24) & 0xFF;
		char_line_num[1] = (NumLine >> 16) & 0xFF;
		char_line_num[2] = (NumLine >> 8) & 0xFF;
		char_line_num[3] = NumLine & 0xFF;
		while (i < 12)
		{
			num1[i] = buff[18];
			num2[i] = buff[31 + i];
			if (((num1[i] < 48 || num1[i] > 57) && i != 0) || ((num1[i] < 48 || num1[i] > 57) && i != 0))
				NumLine++;
			i++;
		}

		i = 0;
		while (i < 2)
		{
			ihour *= 10;
			imin *= 10;
			isec *= 10;
			ihour += buff[44 + i] - 48;
			imin += buff[47 + i] - 48;
			isec += buff[50 + i] - 48;
			if ((buff[44 + i] < 48 || buff[44 + i] > 57) || (buff[47 + i] < 48 || buff[47 + i] > 57) || (buff[50 + i] < 48 || buff[50 + i] > 57))
				NumLine++;
			i++;
		}
		if (ihour > 23 || imin > 59 || isec > 59)
			NumLine++;
		hour[0] = ihour;
		min[0] = imin;
		sec[0] = isec;


		i = 0;
		while (buff[53 + i] != '\0' || buff[5 + i] != '\n')
		{
			msg[i] = buff[35 + i];
			i++;
		}
		msg[i] = '\n';
		i++;
		while (flag < 4)
			flag += send(s, char_line_num + flag, 4 - flag, 0);
		flag = 0;
		while (flag < 12)
			flag += send(s, num1 + flag, 12 - flag, 0);
		flag = 0;
		while (flag < 12)
			flag += send(s, num2 + flag, 12 - flag, 0);
		flag = 0;
		while (flag < 1)
			flag += send(s, hour + flag, 1 - flag, 0);
		flag = 0;
		while (flag < 1)
			flag += send(s, min + flag, 1 - flag, 0);
		flag = 0;
		while (flag < 1)
			flag += send(s, sec + flag, 1 - flag, 0);
		flag = 0;
		while (flag < i)
			flag += send(s, msg + flag, i - flag, 0);
		if (strncmp("stop", msg, 4) == 0 && strlen(msg) == 4)		
		{// � ����� �� ����� ��?
			break;
		}
		NumLine++;
	}
	fclose(f);
	s_close(s);
}