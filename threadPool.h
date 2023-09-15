#pragma once


#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<future>
#include<functional>
#include<atomic>


template<typename T>
class safeQueue
{
private:
	std::queue<T>t_queue;
	std::mutex t_mutex;
public:
	bool isEmpty()
	{
		std::unique_lock<std::mutex>lock(this->t_mutex);
		return this->t_queue.empty();
	}
	unsigned int size()
	{
		std::unique_lock<std::mutex>lock(this->t_mutex);
		return this->t_queue.size();
	}
	void emplace(T& t)
	{
		//std::cout << "get a lv,size:" << this->size() << '\n';
		std::unique_lock<std::mutex>lock(this->t_mutex);
		this->t_queue.emplace(t);
	}
	void emplace(T&& t)
	{
		//std::cout << "get a rv,size:" << this->size() << '\n';
		std::unique_lock<std::mutex>lock(this->t_mutex);
		this->t_queue.emplace(std::move(t));
	}
	bool pop(T& t)
	{
		std::unique_lock<std::mutex>lock(this->t_mutex);
		if (this->t_queue.empty())
			return false;
		t = std::move(this->t_queue.front());
		this->t_queue.pop();
		return true;
	}
};

std::mutex numtex;
int num = 0;

class threadPool
{
private:
	std::atomic<bool> isShutdown;
	std::vector<std::thread>threads;
	safeQueue<std::function<void()>>t_queue;
	std::mutex t_mutex;
	std::condition_variable t_cv;

	class threadWorker
	{
	private:
		int t_id;
		threadPool* t_pool;
	public:
		threadWorker(const int id,threadPool*pool) :t_id(id),t_pool(pool) {}
		void operator()()
		{
			//std::cout << "new thread: " << std::this_thread::get_id() << '\n';
			while (true)
			{
				std::function<void()>func;
				bool isPop = false;
				{
					std::unique_lock<std::mutex>lock(this->t_pool->t_mutex);
					this->t_pool->t_cv.wait(lock, [this]() {return !(this->t_pool->t_queue.isEmpty()) || this->t_pool->isShutdown; });//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!实时，不要使用中间变量！！！！！！！！！！！！！！
					if (this->t_pool->isShutdown && this->t_pool->t_queue.isEmpty())
						return;
					isPop = this->t_pool->t_queue.pop(func);
				}
				if (isPop)
					func();
			}

		}
	};


public:
	threadPool(const unsigned int numThreads = 8) :threads(std::vector<std::thread>(numThreads)), isShutdown(false) {}
	threadPool(const threadPool&) = delete;
	threadPool(threadPool&&) = delete;
	threadPool& operator=(const threadPool&) = delete;
	threadPool&& operator=(threadPool&&) = delete;
	~threadPool()
	{
		if (!this->isShutdown)
			this->shutdown();
	}
	void init()
	{
		for (unsigned int i = 0; i < this->threads.size(); ++i)
		{
			threads.at(i) = std::thread(threadWorker(i, this));
		}
	}
	void shutdown()
	{
		this->isShutdown = true;
		this->t_cv.notify_all();
		int numClosed = 0;
		for (unsigned int i = 0; i < this->threads.size(); ++i)
		{
			if (this->threads.at(i).joinable())
				this->threads.at(i).join();
		}
	}
	template<typename F,typename... Args>
	auto submit(F&& f, Args&& ...args) -> std::future<decltype(f(args...))>
	{
		using returnType = std::invoke_result<F, Args...>::type;
		std::function<decltype(f(args...))()>func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
		std::function<void()>task = [task_ptr]() {(*task_ptr)(); };
		this->t_queue.emplace(task);
		this->t_cv.notify_one();
		return task_ptr->get_future();
	}
	void subVoid(std::function<void()>& f)
	{
		this->t_queue.emplace(f);
		this->t_cv.notify_one();

	}
	void subVoid(std::function<void()>&& f)
	{
		this->t_queue.emplace(std::move(f));
		this->t_cv.notify_one();
	}
};

