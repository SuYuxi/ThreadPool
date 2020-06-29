#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <memory>

#include <iostream>

class ThreadPool {
public:
	ThreadPool(int _threadNum = 4);
	~ThreadPool();

	void pause();
	void resume();
	void shutdown();

template<class Func, class... Args>
auto add(Func&& func, Args&& ...args)
	->std::future<typename std::result_of<Func(Args...)>::type>;

private:
	int threadNum;
	bool suspend;
	bool stop;
	std::mutex taskMutex;
	std::condition_variable cv;

	std::queue<std::function<void()>> taskQueue;
	std::vector<std::thread> threadList;
};

ThreadPool::ThreadPool(int _threadNum)
	: threadNum(_threadNum), stop(false), suspend(false)
{
	std::function<void()> threadFunc = [this]() {
		while (true)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(this->taskMutex);
				this->cv.wait(lock, [this] {return !this->taskQueue.empty() || this->stop; });
				if (this->stop && this->taskQueue.empty()) return;
				task = std::move(taskQueue.front());
				this->taskQueue.pop();
			}
			task();
		}
	};
	for (int i = 0; i < threadNum; ++i)
	{
		threadList.emplace_back(threadFunc);
	}
}

ThreadPool::~ThreadPool()
{
	shutdown();
	cv.notify_all();
	for (std::thread& t : threadList)
	{
		t.join();
	}
}

template<class Func, class... Args>
auto ThreadPool::add(Func&& func, Args&& ...args)
	-> std::future<typename std::result_of<Func(Args...)>::type>
{
	using returnType = typename std::result_of<Func(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<returnType()>>(
		std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);
	std::future<returnType> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(taskMutex);
		if (stop) throw std::runtime_error("Add task to stopped threadPool");
		taskQueue.emplace([task](){ (*task)(); });
	}
  cv.notify_one();
	return res;
}

void ThreadPool::pause()
{
	suspend = true;
}

void ThreadPool::resume()
{
	suspend = true;
}

void ThreadPool::shutdown()
{
	stop = true;
}
