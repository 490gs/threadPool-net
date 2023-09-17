#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
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
	//登入阶段；
	std::vector<std::string>userName;
	std::vector<int>step;
	//通信阶段；
	std::vector<char*> recvBuffer;
	std::vector<char*> sendBuffer;
	std::vector<unsigned int>sendIndex;
	std::vector<int>recvLength;
	std::vector<std::shared_ptr<std::mutex>>contentsMutex;
	std::vector<std::shared_ptr<std::condition_variable>>contentsCv;
	std::thread conCli;
	std::mutex conCliMutex;
	std::condition_variable conCliCv;
public:
	netServer(unsigned long port, unsigned int numListen)
		:numClient(numListen), numCurrentClient(0), tp(numListen * 2), conCli([this]() {this->connectClient(); }),
		socketOfClient(numListen), userName(numListen), step(numListen),
		recvBuffer(numListen),sendBuffer(numListen),sendIndex(numListen),
		recvLength(numListen), contentsMutex(numListen), contentsCv(numListen)
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
			step[i] = 0;
			recvBuffer[i] = new(char[MSG_LENGTH]);
			sendBuffer[i] = new(char[MSG_LENGTH]);
			sendIndex[i] = -1;
			recvLength[i] = 0;
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
		{
			delete[] this->recvBuffer[i];
			delete[] this->sendBuffer[i];
		}
		closesocket(this->serverSocket);
		WSACleanup();
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
				std::cout << "[Sys] new client: " << this->socketOfClient[numCurrentClient] << '\n';
				std::cout << "{Sys] current num: " << numCurrentClient+1 << '\n';
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
			std::cout << "[Sys] start recv from: " << this->socketOfClient[index] << "; index: " << index << "...\n";
		}
		std::string info;
		unsigned int toIndex=0;
		int num=0;
		int indexInt=0;
		while(true)
		{
			{
				std::unique_lock<std::mutex>lock(*(this->contentsMutex[index]));
				while (this->step[index] == 0)
				{
				// step0: send msg: enter name;
				this->contentsCv[index]->wait(lock, [this, index]() {return sendIndex[index] == -1; });
				strcpy_s(this->sendBuffer[index], MSG_LENGTH, "Sys: welcome, enter your name:");
				send(this->socketOfClient[index], this->sendBuffer[index], MSG_LENGTH, 0);
				this->contentsCv[index]->notify_all();

					// step1: recv msg: name;
					this->recvLength[index] = recv(this->socketOfClient[index], this->recvBuffer[index], MSG_LENGTH, 0);
					if (this->recvLength[index] > 0 && strlen(this->recvBuffer[index]) > 1)
					{
						int len = strlen(this->recvBuffer[index]);
						this->recvBuffer[index][len - 1] = '\0';
						this->userName[index] = this->recvBuffer[index];
						this->step[index] = 2;
						{
							std::unique_lock<std::mutex>lock(mutexForPrint);
							std::cout <<"[Sys] index: "<<index << " username: " << this->userName[index] << "...\n";
						}
					}
					else
					{
						this->step[index] = 0;
					}
				}
				// step2: send msg: select chater;
				while (this->step[index] == 2)
				{
					info="Sys: current users as follow:\n";
					unsigned int num = this->numCurrentClient;
					for (unsigned int i = 0; i < num; ++i)
					{
						info = info + (char)(i + '0') + ':' + this->userName[i] + '\n';
					}
					info = info + "please select one number:\n";

					this->contentsCv[index]->wait(lock, [this, index]() {return sendIndex[index] == -1; });
					strcpy_s(this->sendBuffer[index], MSG_LENGTH, info.c_str());
					send(this->socketOfClient[index], this->sendBuffer[index], MSG_LENGTH, 0);
					this->contentsCv[index]->notify_all();
					// step3: recv msg: chater selected;
					this->recvLength[index] = recv(this->socketOfClient[index], this->recvBuffer[index], MSG_LENGTH, 0);
					if (this->recvLength[index] > 0 && strlen(this->recvBuffer[index]) == 2 && this->recvBuffer[index][1] == '\n')
					{
						indexInt = (int)(this->recvBuffer[index][0] - '0');
						if (indexInt >= 0 && indexInt < num)
						{
							this->recvLength[index] = 0;
							toIndex = indexInt;
							{
								std::unique_lock<std::mutex>lock(mutexForPrint);
								std::cout <<"[Sys] "<< this->userName[index] << " choosed " << this->userName[toIndex] << ".\n";
							}
							strcpy_s(this->recvBuffer[index], MSG_LENGTH, this->userName[toIndex].c_str());
							this->step[index] = 4;
						}
						else
						{
							this->step[index] = 2;
						}
					}
					else
					{
						this->step[index] = 2;
					}

				}
				// step4: chat start;
				if(this->step[index]==4)
				{
					info = " start chat with ";
					info = info + this->userName[toIndex];
					info = info + ":...\n";

					this->contentsCv[index]->wait(lock, [this, index]() {return sendIndex[index] == -1; });
					strcpy_s(this->sendBuffer[index], MSG_LENGTH, info.c_str());
					send(this->socketOfClient[index], this->sendBuffer[index], MSG_LENGTH, 0);
					this->contentsCv[index]->notify_all();
				}
			}
			if (this->step[index] == 5)
			{
				std::unique_lock<std::mutex>lock(*(this->contentsMutex[index]));
				this->contentsCv[index]->wait(lock, [this, index]() {return sendIndex[index] == -1; });
				info = "[from Sys]: you have quited, goodbye.\n";
				strcpy_s(this->sendBuffer[index], MSG_LENGTH, info.c_str());
				send(this->socketOfClient[index], this->sendBuffer[index], MSG_LENGTH, 0);
				this->contentsCv[index]->notify_all();
				{
					std::unique_lock<std::mutex>lock(mutexForPrint);
					std::cout << "[Sys] "<<this->userName[index]<<" quit. Close the socket...\n";
				}
				closesocket(this->socketOfClient[index]);
				break;
			}

			while (true)
			{
				this->recvLength[index] = recv(this->socketOfClient[index], this->recvBuffer[index], MSG_LENGTH, 0);
				if (this->recvLength[toIndex] > 0)
				{
					info = this->recvBuffer[index];
					if (info == "\\esc\n")
					{
						{
							std::unique_lock<std::mutex>lock(mutexForPrint);
							std::cout << "[change] from: " << this->userName[index] << ":...\n";
							std::cout << info;
						}
						this->step[index] = 2;
						break;
					}
					if (info == "\\quit\n")
					{
						this->step[index] = 5;
						break;
					}
					std::unique_lock<std::mutex>lock(*(this->contentsMutex[toIndex]));
					this->contentsCv[toIndex]->wait(lock, [this, toIndex]() {return sendIndex[toIndex] == -1; });
					{
						std::unique_lock<std::mutex>lock(mutexForPrint);
						std::cout << "[recv] from: " << this->userName[index] << ":...";
						std::cout << info;
					}
					info = "[from "+this->userName[index]+"]: "+ this->recvBuffer[index];
					strcpy_s(this->sendBuffer[toIndex], this->recvLength[index], info.c_str());
					this->sendIndex[toIndex] = index;
					this->contentsCv[toIndex]->notify_all();
				}
			}
		}
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "[Sys] Stop recv from: "<<this->userName[index]<<"...\n";
		}
	}
	void sendClient(unsigned int index)
	{
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "[Sys] start send to: " << this->socketOfClient[index] << "; index: " << index << "...\n";
		}
		std::string info;
		int sendSuccess=-1;
		int fromIndex = -1;
		while (true)
		{
			if (this->step[index] == 5)
			{
				break;
			}
			{
				std::unique_lock<std::mutex>lock(*(this->contentsMutex[index]));
				this->contentsCv[index]->wait(lock, [this, index]() {return sendIndex[index] != -1 || step[index] == 5; });
				if (this->step[index] == 5)
				{
					break;
				}
				fromIndex = this->sendIndex[index];
				if (fromIndex != -2)
				{
					info = this->userName[fromIndex] + " :";
					{
						std::unique_lock<std::mutex>lock(mutexForPrint);
						std::cout << "[send] to: " << this->userName[index] << ":...";
						std::cout << this->sendBuffer[index];
					}
					sendSuccess = send(this->socketOfClient[index], this->sendBuffer[index], this->recvLength[fromIndex], 0);
					this->sendIndex[index] = -1;
					this->contentsCv[index]->notify_all();
				}
				else
				{
					sendSuccess = send(this->socketOfClient[index], this->sendBuffer[index], MSG_LENGTH, 0);
					{
						std::unique_lock<std::mutex>lock(mutexForPrint);
						std::cout << "[send] to: " << this->userName[index] << ":...";
						std::cout << this->sendBuffer[index];
					}
					this->sendIndex[index] = -1;
					this->contentsCv[index]->notify_all();
					continue;
				}
			}
			if (sendSuccess != -1 && fromIndex != -1)
				info = " [Sys: to " + this->userName[index] + " success!]\n";
			else
				info = " [Sys: to " + this->userName[index] + " failed!]\n";
			std::unique_lock<std::mutex>subLock(*(this->contentsMutex[fromIndex]));
			this->contentsCv[index]->wait(subLock, [this, fromIndex]() {return sendIndex[fromIndex] == -1; });
			strcpy_s(this->sendBuffer[fromIndex], MSG_LENGTH, info.c_str());
			this->sendIndex[fromIndex] = -2;
			this->contentsCv[fromIndex]->notify_all();

		}
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "[Sys] stop send to: " << this->userName[index] << ":...\n";
		}
	}
};

class netClient
{
private:
	SOCKET clientSocket;
	char sendBuffer[MSG_LENGTH];
	char recvBuffer[MSG_LENGTH];
	std::mutex startSubThreadMutex;
	std::atomic<int> step;
	std::condition_variable startSubThreadCv;
	std::thread recvThread;
	std::thread sendThread;
public:
	netClient(const char* serverIp, unsigned long port) 
		:step(0), recvThread([this]() {this->recvServer(); }), sendThread([this]() {this->sendServer(); })
	{
		WSADATA wsaData;
		int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (error)
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "init fail.\n";
		}
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
		step = 1;
		startSubThreadCv.notify_all();

	}
	netClient(const netClient&) = delete;
	netClient(netClient&&) = delete;
	netClient& operator=(const netClient&) = delete;
	netClient& operator=(netClient&&) = delete;
	~netClient()
	{
		this->recvThread.join();
		this->sendThread.join();
		closesocket(this->clientSocket);
		WSACleanup();
	}
	void recvServer()
	{
		{
			std::unique_lock<std::mutex>lock(this->startSubThreadMutex);
			this->startSubThreadCv.wait(lock, [this]() {return step == 1; });
			std::cout << "start recv from server...\n";
		}
		int recvLength = 0;
		while (true)
		{
			recvLength = recv(this->clientSocket, this->recvBuffer, MSG_LENGTH,0);
			if (recvLength >0)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << this->recvBuffer <<std::endl;
			}
			if (recvLength == 0)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "the connection have broken.\n";
				break;
			}
			if (recvLength == -1)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
			}
		}
		{
			step = 2;
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "enter something to quit.▲\nstop recv from server.\n";
			}
		}
	}
	void sendServer()
	{
		{
			std::unique_lock<std::mutex>lock(this->startSubThreadMutex);
			this->startSubThreadCv.wait(lock, [this]() {return step == 1; });
			std::cout << "start send to server...\n";
		}
		int sendLength = 0;
		while (true)
		{
			fgets(sendBuffer, sizeof(sendBuffer), stdin);
			if (step == 2)
			{
				break;
			}
			sendLength = send(this->clientSocket, this->sendBuffer, MSG_LENGTH, 0);
			if (sendLength == 0)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "the connection have broken.\n\n";
				break;
			}
			if (sendLength == -1)
			{
				std::unique_lock<std::mutex>lock(mutexForPrint);
				std::cout << "error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
			}
		}
		{
			std::unique_lock<std::mutex>lock(mutexForPrint);
			std::cout << "stop send to server.\n";
		}
	}
};