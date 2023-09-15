#pragma once
# define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<winsock.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")

#include"threadPool.h"

class netServer
{
private:
	SOCKET serverSocket;
public:
	netServer()
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		serverSocket = socket(AF_INET, SOCK_STREAM, 0);//地址类型：ipv4，套接字类型：流，协议类型：自动tcp；返回-1表示失败；
		if (serverSocket != -1)
			std::cout << "server socket created: " << serverSocket << '\n';

	}
};