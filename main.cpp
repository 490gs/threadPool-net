#include<iostream>
#include<random>
#include"threadPool.h"


/*void fun1()
{
	std::cout << "working in thread " << std::this_thread::get_id() << std::endl;
}

void fun2(int x)
{
	std::cout << "task " << x << " working in thread " << std::this_thread::get_id() << std::endl;
}

int main(int argc, char* argv[])
{
	ThreadPool thread_pool(80);
	thread_pool.start();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for (int i = 0; i < 6; i++)
	{
		//thread_pool.appendTask(fun1);
		thread_pool.appendTask(std::bind(fun2, i));
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	thread_pool.stop();

	getchar();
	return 0;
}*/


std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<int>dist(-1000, 1000);
auto rnd = std::bind(dist, mt);

void f(int a)
{
	std::this_thread::sleep_for(std::chrono::seconds(1)); 
	std::cout << std::this_thread::get_id() << ':'<<a<<'\n';
}
int g(int i, int j)
{
	//std::this_thread::sleep_for(std::chrono::seconds(1));
	std::cout << std::this_thread::get_id() << ' ' << i<<' '<< j << '\n';
	return i * j;
}
void main()
{
	//for (int i = 0; i < 100; ++i)
	{
		//std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << i;
		threadPool tp(20);
		tp.init();
		for (int i = 0; i < 3; i++)
		{
			tp.submit(f,i);
			for (int j = 5; j < 10; j++)
			{
				std::future<int>r = tp.submit(g, i, j);
				std::cout << r.get()<<'\n';
			}
		}
		tp.shutdown();

	}
	std::cout << std::thread::hardware_concurrency();
}