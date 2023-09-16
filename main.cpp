#include<iostream>
#include<random>
#include"threadPool.h"
#include"net.h"


void main()
{
	netServer ns(12345, 2);
	netClient nc("127.0.0.1", 12345);
	
}