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
		serverSocket = socket(AF_INET, SOCK_STREAM, 0);//��ַ���ͣ�ipv4���׽������ͣ�����Э�����ͣ��Զ�tcp������-1��ʾʧ�ܣ�
		if (serverSocket != -1)
			std::cout << "server socket created: " << serverSocket << '\n';

	}
};