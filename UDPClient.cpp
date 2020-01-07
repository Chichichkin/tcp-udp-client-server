#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




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
int sock_err(const char* function, int s)		// вывод номера ошибки
{
	int err;
#ifdef _WIN32
	err = WSAGetLastError();
#else
	err = errno;
#endif
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}
void s_close(int s)								//Закрыть дискриптор сокета
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}

#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif 
struct msg_block
{
	char number1[13];
	char number2[13];
	char hour[2];
	char min[2];
	char sec[2];
	char *msg;
	int size_msg;
	bool delivered;
};
struct msg_block *msgs;
unsigned short quantity_of_msg = 0, quantit_of_delivered_msg = 0;

int check_inp(char *arg1, char *arg2);
void get_adrs(char *arg, char *ip, char *port);
void send_msg(int s, struct sockaddr_in *addr, int index);
void recv_msg(int s);
void decompose_file(char *FileName);

// Функция определяет IP-адрес узла по его имени.
// Адрес возвращается в сетевом порядке байтов.
unsigned int get_host_ipn(const char* name)
{
	struct addrinfo* addr = 0;
	unsigned int ip4addr = 0;
	// Функция возвращает все адреса указанного хоста
	// в виде динамического однонаправленного списка

	if (0 == getaddrinfo(name, 0, 0, &addr))
	{
		struct addrinfo* cur = addr;
		while (cur)
		{
			// Интересует только IPv4 адрес, если их несколько - то первый
			if (cur->ai_family == AF_INET)
			{
				ip4addr = ((struct sockaddr_in*) cur->ai_addr)->sin_addr.s_addr;
				break;
			}
			cur = cur->ai_next;
		}
		freeaddrinfo(addr);
	}
	return ip4addr;
}
// Отправляет http-запрос на удаленный сервер

int main(int argc, char **argv)
{
	// Проверка правильности запуска с аргументами
	int i;
	char ip[16], port[6];
	if (argc != 3)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}
	if (check_inp(argv[1], argv[2]) < 0)
		return -1;
	get_adrs(argv[1], ip, port);				//Получаем отдельно строчки ip и порта  

	int s;
	struct sockaddr_in addr;

	decompose_file(argv[2]);
	
	// Инициалиазация сетевой библиотеки
	init();

	// Создание TCP-сокета
	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s < 0)
		return sock_err("socket", s);

	// Заполнение структуры с адресом удаленного узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = get_host_ipn(ip);

	
	while (quantit_of_delivered_msg != quantity_of_msg)
	{
		for (i = 0; i < quantity_of_msg; i++)
			if (!msgs[i].delivered)
				send_msg(s, &addr, i);
		if (quantit_of_delivered_msg >= 20)
		{
			printf("Enough msg send\n");
			s_close(s);
			deinit();
			free(msgs);
			return 0;
		}

	}
	if (quantit_of_delivered_msg == quantity_of_msg && quantity_of_msg < 20)
	{
		s_close(s);
		deinit();
		free(msgs);
	}
	return 0;
}
void decompose_file(char *FileName)
{
	FILE* f;
	f = fopen(FileName, "r+");
	char TheLine[1000000];
	quantity_of_msg++;
	msgs = (struct msg_block*)malloc(sizeof(struct msg_block));
	int i = 0, num = 0,ihour = 0, imin = 0, isec = 0;
	bool badmsg = 0;
	while (fgets(TheLine, 1000000, f))
	{
		if (strlen(TheLine) < 36)
			continue;
		i = 0;
		while (i < 12)
		{
			msgs[num].number1[i] = TheLine[i];
			msgs[num].number2[i] = TheLine[13 + i];
			if (((msgs[num].number1[i] < 48 || msgs[num].number1[i] > 57) && i != 0) || ((msgs[num].number2[i] < 48 || msgs[num].number2[i] > 57) && i != 0))
				{
					badmsg = 1;
					break;
				}
			i++;
		}
		if (badmsg)
		{
			badmsg = 0;
			continue;
		}
		msgs[num].number1[i] = msgs[num].number2[i] = '\0';
		i = 0;
		ihour = 0;
		imin = 0;
		isec = 0;
		while (i < 2)
		{
			ihour *= 10;
			imin *= 10;
			isec *= 10;
			ihour += TheLine[26 + i] - 48;
			imin += TheLine[29 + i] - 48;
			isec += TheLine[32 + i] - 48;
			if ((TheLine[26 + i] < 48 || TheLine[26 + i] > 57) || (TheLine[29 + i] < 48 || TheLine[29 + i] > 57) || (TheLine[32 + i] < 48 || TheLine[32 + i] > 57))
				{
					badmsg = 1;
					break;
				}
			i++;
		}
		if (ihour > 23 || imin > 59 || isec > 59 || ihour < 0 || imin < 0 || isec < 0 || badmsg)
		{
			badmsg = 0;
			continue;
		}
		msgs[num].hour[0] = ihour;
		msgs[num].min[0] = imin;
		msgs[num].sec[0] = isec;
		msgs[num].hour[1] = msgs[num].min[1] = msgs[num].sec[1] = '\0';
		msgs[num].size_msg = strlen(TheLine) - 36;
		msgs[num].msg = (char*)malloc((msgs[num].size_msg + 1) * sizeof(char));
		i = 0;
		while (TheLine[35+i] != '\0' && TheLine[35 + i] != '\n' && i != 1000000)
		{
			msgs[num].msg[i] = TheLine[35 + i];
			i++;
		}
		msgs[num].msg[i] = '\0';
		msgs[num].delivered = false;
		
		quantity_of_msg++;
		num++;
		msgs = (struct msg_block*)realloc(msgs, quantity_of_msg * sizeof(struct msg_block));
	}
	quantity_of_msg--;
	num--;
	msgs[num].size_msg++;
	msgs[num].msg = (char*)realloc(msgs[num].msg, (msgs[num].size_msg + 1) * sizeof(char));
	msgs[num].msg[i] = '\0';

	fclose(f);
}
void send_msg(int s, struct sockaddr_in *addr, int index)
{
	char *Buffer = (char*)calloc((31 + msgs[index].size_msg + 1), sizeof(char));
	int i = 0, j = 0,res;
	int line_num = htonl(index);
	char char_line_num[5];
	memcpy(char_line_num, &line_num, 4);
	for (i = 0; i < 4; i++)
		Buffer[j++] = char_line_num[i];
	for (i = 0; i < 12; i++)
		Buffer[j++] = msgs[index].number1[i];
	for (i = 0; i < 12; i++)
		Buffer[j++] = msgs[index].number2[i];
	Buffer[j++] = msgs[index].hour[0];
	Buffer[j++] = msgs[index].min[0];
	Buffer[j++] = msgs[index].sec[0];
	for (i = 0; i < msgs[index].size_msg; i++)
		Buffer[j++] = msgs[index].msg[i];
	Buffer[j] = '\0';

	res = sendto(s, Buffer, j + 1 , flags, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
	if (!res)
	{
		printf("FAIL TO SEND. INDEX = %d\n", index);
		sock_err("sendto", s);
	}
	free(Buffer);

	recv_msg(s);
}
void recv_msg(int s)
{
	char datagram[1000];
	int res, index;
	struct timeval tv = { 0, 100 * 1000 }; // 100 ms delay ;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	// Проверка - если в сокете входящие дейтаграммы
	// (ожидание в течение tv)
	res = select(s + 1, &fds, 0, 0, &tv);
	if (res > 0) // Данные есть, считывание их
	{
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		int received = (int)recvfrom(s, (char*)datagram, sizeof(datagram), 0, (struct sockaddr *)&addr, &addrlen);
		if (received <= 0) // Ошибка считывания полученной дейтаграммы
		{
			sock_err("recvfrom", s);
			return;
		}
		else
		{
			memcpy(&index, datagram, 4);
			index = ntohl(index);
			//printf("Receved %d\n", index);
			msgs[index].delivered = true;
			quantit_of_delivered_msg++;
		}
		
	}
}
int check_inp(char *arg1, char *arg2)
{
	int i = 0, p = 0, p2 = 0;
	char txt[] = { ".txt" };
	while (arg1[i] != ' ' && arg1[i] != '\n' && arg1[i] != '\0')
	{
		if (arg1[i] == '.')
			p++;
		if (arg1[i] == ':')
			p2++;
		if ((arg1[i] > 57 || arg1[i] < 48) && arg1[i] != '.' && arg1[i] != ':')
		{
			printf("In adress wrong sumbol");
			return -1;
		}
		i++;
	}
	if (p != 3 || p2 != 1)
	{
		printf("%d %d", p, p2);
		printf("Wrong format of adress\n");
		return -1;
	}
	if (strlen(arg2) < 5)
	{
		printf("File name too short");
		return -1;
	}
	i = 0;
	while (arg2[strlen(arg2) - 4 + i] == txt[i] && i < 4)
	{
		i++;
	}
	if (i != 4)
	{
		printf("Wrong format of file shit");
		return -1;
	}
}
void get_adrs(char *arg, char *ip, char *port)
{
	int i = 0, j = 0;
	while (arg[i] != ':')
	{
		ip[i] = arg[i];
		i++;
	}
	ip[i] = '\0';
	i++;
	while (arg[i + j] != ' ' && arg[i + j] != '\0' && arg[i + j] != '\n')
	{
		port[j] = arg[i + j];
		j++;
	}
	port[j] = '\0';
}
