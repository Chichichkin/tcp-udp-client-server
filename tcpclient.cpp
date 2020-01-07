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
#include <netdb.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>


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
struct msg_block
{
	char number1[13];
	char number2[13];
	char hour[2];
	char min[2];
	char sec[2];
	char *msg;
	int size_msg;
};
struct msg_block *msgs;
int num_of_msg = 0;
int check_inp(char *arg1, char *arg2);
void get_adrs(char *arg, char *ip, char *port);
void send_msg(int s, int idx);
void parsing_file(char *fname);


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
	if (argc != 3)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}
	if (check_inp(argv[1], argv[2]) < 0)
		return - 1;
	char ip[16], port[6];
	get_adrs(argv[1], ip, port);				//Получаем отдельно строчки ip и порта  
	

	int s;
	struct sockaddr_in addr;
	
	// Инициалиазация сетевой библиотеки
	init();
	// Создание TCP-сокета
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	// Заполнение структуры с адресом удаленного узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = get_host_ipn(ip);
	// Установка соединения с удаленным хостом

	int num_try = 0;
	while (num_try < 10)
	{
		printf("Conection try No.%d\n", num_try+1);
		if (connect(s, (struct sockaddr*) &addr, sizeof(addr)) == 0)
		{
			printf("Connection established\n");
			if (send(s, "put", sizeof(char) * 3, 0) != -1)
			{
				printf("Send\n");
				break;
			}
			else
			{
				printf("problem with sending put");
				s_close(s);
				deinit();
				return -1;
			}
		}
		else
			printf("problem with conection\n");
		num_try++;
		Sleep(100);
	}
	if (num_try == 10)
	{
		printf("No connection established\n");
		s_close(s);
		deinit();
		return -1;
	}
	parsing_file(argv[2]);
	// Прием результата
	for (i = 0; i < num_of_msg; i++)
	{
		send_msg(s, i);
	}
	s_close(s);
	deinit();
	return 0;
}
void parsing_file(char *fname)
{
	FILE* f;
	f = fopen(fname, "r");
	char TheLine[1000000];
	num_of_msg++;
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
		
		num_of_msg++;
		num++;
		msgs = (struct msg_block*)realloc(msgs, num_of_msg * sizeof(struct msg_block));
	}
	num_of_msg--;
	num--;
	msgs[num].size_msg++;
	msgs[num].msg = (char*)realloc(msgs[num].msg, (msgs[num].size_msg + 1) * sizeof(char));
	msgs[num].msg[i] = '\0';

	fclose(f);
}
void send_msg(int s, int idx)
{
	char res[2];
	int check;
	unsigned int line_num = htons(idx);
	char char_line_num[4];

	char_line_num[0] = (line_num >> 24) & 0xFF;
	char_line_num[1] = (line_num >> 16) & 0xFF;
	char_line_num[2] = (line_num >> 8) & 0xFF;
	char_line_num[3] = line_num & 0xFF;

	check = send(s, char_line_num, 4, 0);
	check = send(s, msgs[idx].number1, 12, 0);
	check = send(s, msgs[idx].number2, 12, 0);
	check = send(s, msgs[idx].hour, 1, 0);
	check = send(s, msgs[idx].min, 1, 0);
	check = send(s, msgs[idx].sec, 1, 0);
	check = send(s, msgs[idx].msg, msgs[idx].size_msg+1, 0);

	check = recv(s, res, 2, 0);
	if (check == 1)
	{
		check += recv(s, res, 1, 0);
	}
}
int check_inp(char *arg1, char *arg2)
{
	int i = 0, p = 0, p2 = 0;
	char txt[] = { ".txt" };
	while(arg1[i] != ' ' && arg1[i] != '\n' && arg1[i] != '\0')
	{
		if (arg1[i] == '.')
			p++;
		if (arg1[i] == ':')
			p2++;
		if((arg1[i] > 57 || arg1[i] < 48) && arg1[i] != '.' && arg1[i] != ':')
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
	int i = 0,j = 0;
	while (arg[i] != ':')
	{
		ip[i] = arg[i];
		i++;
	}
	ip[i] = '\0';
	i++;
	while (arg[i + j] != ' ' && arg[i + j] != '\0' && arg[i + j] != '\n')
	{
		port[j] = arg[i+j];
		j++;
	}
	port[j] = '\0';
}