#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN 
#include <windows.h> 
#include <winsock2.h> 
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif 
struct msg_elem
{
	char number1[13];
	char number2[13];
	char hour[3];
	char min[3];
	char sec[3];
	char * msg;
	char*ip;
	int port;
} ;

struct msg_elem * msgs = (struct msg_elem *)malloc(sizeof(struct msg_elem));

int  sock_arr[256];
int num_of_msg = 0,cur_msg;
int answer;
bool exittime = false;

int init();
void deinit();
int sock_err(const char* function, int s);
int set_non_block_mode(int s);
void parsing_msg(char *buf, char* ip, int port, int size);
void output();

int main(int argc, char *argv[])
{
	int port_1 = atoi(argv[1]), port_2 = atoi(argv[2]);
	int port_arr[256], recv, addrlen, i;
	char buffer[1000000], buf_recv[4];
	bool stop = false;
	init();
	struct sockaddr_in addr;
	for (i = 0; i <= port_2 - port_1; i++)
	{
		sock_arr[i] = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock_arr[i] < 0)
			return sock_err("socket", sock_arr[i]);
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		port_arr[i] = port_1 + i;
		addr.sin_port = htons(port_arr[i]);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(sock_arr[i], (struct sockaddr*) &addr, sizeof(addr)) < 0)
			return sock_err("bind", sock_arr[i]);
		set_non_block_mode(sock_arr[i]);	
	}

	fd_set rfd;
	fd_set wfd;
	int nfds = sock_arr[0];
	struct timeval tv = { 10, 10 };
	printf("Waiting for msg\n");
	while (!exittime)
	{
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		for (i = 0; i <= port_2 - port_1; i++)
		{
			FD_SET(sock_arr[i], &rfd);
			FD_SET(sock_arr[i], &wfd);
			if (nfds < sock_arr[i])
				nfds = sock_arr[i];
		}
		if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
		{
			for (i = 0; i <= port_2 - port_1; i++)
			{
				if (FD_ISSET(sock_arr[i], &rfd))
				{
					printf("Got msg\n");
					memset(buffer, 0, 1000000);
					addrlen = sizeof(addr);
					recv = recvfrom(sock_arr[i], buffer, sizeof(buffer), flags, (struct sockaddr*) &addr, &addrlen);
					char* ip = inet_ntoa(addr.sin_addr);
					parsing_msg(buffer, ip, port_arr[i],recv);

					answer = htonl(answer);
					memcpy(buf_recv, &answer, 4);
					int send_len = sendto(sock_arr[i], buf_recv, 4, flags, (struct sockaddr*) &addr, addrlen);
					if (exittime)
						break;
				}
			}
		}

	}
	for (int i = 0; i <= port_2 - port_1; i++)
		closesocket(sock_arr[i]);
	deinit();
	output();
	return 0;
}




int init() // Для Windows следует вызвать WSAStartup перед началом использования сокетов 
{
	WSADATA wsa_data; 
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data)); 
}

void deinit() // Для Windows следует вызвать WSACleanup в конце работы 
{
	WSACleanup(); 
}

int sock_err(const char* function, int s) // Return socket error
{
	int err = WSAGetLastError(); 
	fprintf(stderr, "%s: socket error: %d\n", function, err); 
	return -1;
}

int set_non_block_mode(int s) // translate socket into non-block mode
{ 
	unsigned long mode = 1; 
	return ioctlsocket(s, FIONBIO, &mode); 
}

void parsing_msg(char *buf, char* ip, int port,int size)
{
	cur_msg = num_of_msg;
	msgs = (struct msg_elem *)realloc(msgs, (num_of_msg++ + 1) * sizeof(struct msg_elem ));
	
	char buf_msg[4] = {buf[0], buf[1], buf[2], buf[3]};
	unsigned int idx,i;
	memcpy(&idx, buf_msg, 4);
	idx = ntohl(idx);
	answer = idx;
	
	msgs[cur_msg].port = port;
	msgs[cur_msg].ip = (char*)malloc(strlen(ip) * sizeof(char));
	strcpy(msgs[cur_msg].ip, ip);

	for (i = 0; i < 12; i++)
		msgs[cur_msg].number1[i] = buf[4 + i];
	msgs[cur_msg].number1[12] = '\0';
	
	for (i = 0; i < 12; i++)
		msgs[cur_msg].number2[i] = buf[16 + i];
	msgs[cur_msg].number2[12] = '\0';

	i = buf[28];
	msgs[cur_msg].hour[1] = i % 10 + 48;
	msgs[cur_msg].hour[0] = i / 10 + 48;

	i = buf[29];
	msgs[cur_msg].min[1] = i % 10 + 48;
	msgs[cur_msg].min[0] = i / 10 + 48;

	i = buf[30];
	msgs[cur_msg].sec[1] = i % 10 + 48;
	msgs[cur_msg].sec[0] = i / 10 + 48;

	msgs[cur_msg].sec[2] = msgs[cur_msg].min[2] = msgs[cur_msg].hour[2] = '\0';

	msgs[cur_msg].msg = (char*)malloc((size - 31) * sizeof(char));

	for (i = 0; i < size - 32; i++)
		msgs[cur_msg].msg[i] = buf[31+i];
	msgs[cur_msg].msg[size - 32] = '\0';

	if (strcmp("stop", msgs[cur_msg].msg) == 0)
		exittime = true;
	
}

void output()
{
	FILE *fp = fopen("msg.txt", "a+");
	int i;
	if (!fp)
	{
		printf("Can`t open msg.txt\n");
		return;
	}
	for ( i = 0; i < num_of_msg; i++)
	{
		fprintf(fp, "%s:%u %s %s %s:%s:%s %s\n", msgs[i].ip, msgs[i].port, msgs[i].number1, msgs[i].number2,
			msgs[i].hour, msgs[i].min, msgs[i].sec, msgs[i].msg);
	}
	//fprintf(fp, "Number of messeges = %d\n", num_of_msg);
	fclose(fp);
}