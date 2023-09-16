#pragma once
# define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<winsock.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")

#include"threadPool.h"

#define MSG_LENGTH 100

static std::mutex mutexForPrint;


class netServer
{
private:
	SOCKET serverSocket;
	unsigned int numClient;
	std::atomic<unsigned int>numCurrentClient;
	threadPool tp;
	std::vector<SOCKET>socketOfClient;
	std::vector<char*> contents;
	std::vector<std::shared_ptr<std::mutex>>contentsMutex;
	std::vector<std::shared_ptr<std::condition_variable>>contentsCv;
	std::thread conCli;
	std::mutex conCliMutex;
	std::condition_variable conCliCv;
public:
	netServer(unsigned long port, unsigned int numListen)
		:numClient(numListen), numCurrentClient(0), tp(numListen * 2), conCli([this]() {this->connectClient(); }),
		socketOfClient(numListen), contents(numListen), contentsMutex(numListen), contentsCv(numListen)
	{
		int success;
		WSADATA wsaData;
		success = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (success == -1)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "init fail.\n";
			WSACleanup();
		}
		serverSocket = socket(AF_INET, SOCK_STREAM, 0);//地址类型：ipv4，套接字类型：流，协议类型：自动tcp；返回-1表示失败；
		if (serverSocket != -1)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "server socket created: " << serverSocket << '\n';
		}
		else
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "server socket not created.\n";
			WSACleanup();
		}
		SOCKADDR_IN serverAddr;
		serverAddr.sin_family = AF_INET;						//ipv4协议；
		serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);	//本机地址；
		serverAddr.sin_port = htons(port);						//端口号；
		success = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
		if (success == -1)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "bind fail.\n";
			WSACleanup();
		}
		success = listen(serverSocket, numListen);
		if (success == -1)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "listen fail.\n";
			WSACleanup();
		}
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "waiting for client...\n";
		}
		for (unsigned int i = 0; i < numClient; ++i)
		{
			contents[i] = new(char[100]);
			contentsMutex[i] = std::make_shared<std::mutex>();
			contentsCv[i] = std::make_shared<std::condition_variable>();
		}
		conCliCv.notify_one();
	}
	netServer(const netServer&) = delete;
	netServer(netServer&&) = delete;
	netServer& operator=(const netServer&) = delete;
	netServer&& operator=(netServer&&) = delete;
	~netServer()
	{
		this->conCli.join();
		this->tp.shutdown();
		for (unsigned int i = 0; i < this->numClient; ++i)
			delete[] this->contents[i];
	}

	void connectClient()
	{
		std::unique_lock<std::mutex>lock(conCliMutex);
		conCliCv.wait(lock);
		tp.init();
		while (this->numCurrentClient < numClient)
		{
			SOCKADDR_IN clientSocket;
			int length = sizeof(SOCKADDR);
			this->socketOfClient[numCurrentClient] = accept(serverSocket, (SOCKADDR*)&clientSocket, &length);
			if (this->socketOfClient[numCurrentClient]!=-1)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "new client: " << this->socketOfClient[numCurrentClient] << '\n';
				std::cout << "current num: " << numCurrentClient << '\n';
				unsigned int index = numCurrentClient.load();
				this->tp.submit([this,index]() {this->recvClient(index); });
				this->tp.submit([this,index]() {this->sendClient(index); });
				++numCurrentClient;
			}
			else
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "accept fail: " << this->socketOfClient[numCurrentClient] << '\n';
			}
		}
	}

	void recvClient(unsigned int index)
	{
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "index: " << index << "...\n";
			std::cout << "start recv from: " << this->socketOfClient[index] << "...\n";
		}
		while (true)
		{
			unsigned int toIndex = index;
			std::unique_lock<std::mutex>lock(*(this->contentsMutex[toIndex]));
			int recvLen = recv(this->socketOfClient[toIndex], this->contents[toIndex], MSG_LENGTH, 0);
			if (recvLen != -1)
			{
				this->contentsCv[toIndex]->notify_one();
				{
					std::unique_lock<std::mutex>lock(mutexForPrint);
					std::cout << "recv from: " << this->socketOfClient[index] << "...";
					std::cout << this->contents[toIndex];
				}
			}
		}
	}
	void sendClient(unsigned int index)
	{
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "index: " << index << "...\n";
			std::cout << "start send to: " << this->socketOfClient[index] << "...\n";
		}
		while (true)
		{
			std::unique_lock<std::mutex>lock(*(this->contentsMutex[index]));
			this->contentsCv[index]->wait(lock);
			std::cout << "send " << this->socketOfClient[index] << "...\n";
			send(this->socketOfClient[index], this->contents[index], MSG_LENGTH + 1, 0);
		}
	}
};

class netClient
{
private:
	SOCKET clientSocket;
	char sendBuffer[MSG_LENGTH];
	char recvBuffer[MSG_LENGTH];
	std::thread recvThread;
	std::thread sendThread;
public:
	netClient(const char* serverIp, unsigned long port) 
		:recvThread([this]() {this->recvServer(); }), sendThread([this]() {this->sendServer(); })
	{
		WSADATA wsaData;
		int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (error)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "init fail.\n";
		}
		std::cout << error;
		clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		SOCKADDR_IN clientAddr;
		clientAddr.sin_addr.S_un.S_addr = inet_addr(serverIp);
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_port = htons(port);
		error = connect(clientSocket, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR));
		if (error == -1)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "connect fail.\n";
		}
		else
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "connect success.\n";
		}
	}
	netClient(const netClient&) = delete;
	netClient(netClient&&) = delete;
	netClient& operator=(const netClient&) = delete;
	netClient& operator=(netClient&&) = delete;
	~netClient()
	{
		this->recvThread.join();
		this->sendThread.join();
	}
	void recvServer()
	{
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "recv from server...\n";
		}
		while (true)
		{
			int recvLength = recv(this->clientSocket, this->recvBuffer, MSG_LENGTH,0);
			if (recvLength != -1)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << recvBuffer;
			}
		}
	}
	void sendServer()
	{
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "send to server...\n";
		}
		while (true)
		{
			fgets(sendBuffer, sizeof(sendBuffer), stdin);
			send(this->clientSocket, this->sendBuffer, MSG_LENGTH + 1, 0);
		}
	}
};