#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <memory>

class ThreadPool {
public:
	ThreadPool(int _threadNum = 4);
	~ThreadPool();

	void pause();
	void resume();
	void shutdown();
	void waitAllFinish();

	template<class Func, class... Args>
	auto add(Func&& func, Args&& ...args)
		->std::future<typename std::result_of<Func(Args...)>::type>;

private:
	int threadNum;
	bool suspend;
	bool stop;
	int32_t threadState; // bit 1 means thread is in use, bit 0 means thread is idle.

	std::mutex taskMutex;
	std::condition_variable cv;

	std::queue<std::function<void()>> taskQueue;
	std::vector<std::thread> threadList;
};

ThreadPool::ThreadPool(int _threadNum)
	: threadNum(_threadNum), stop(false), suspend(false), threadState(0)
{
	std::function<void(int)> threadFunc = [this](int id) {
		while (true)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(this->taskMutex);
				this->cv.wait(lock, [this] {return this->stop || (!this->suspend && !this->taskQueue.empty()); });
				if (this->stop && this->taskQueue.empty()) return;
				task = std::move(taskQueue.front());
				this->taskQueue.pop();
			}
			threadState |= (1 << id);
			task();
			threadState &= ~(1 << id);
			cv.notify_one();
		}
	};
	for (int i = 0; i < threadNum; ++i)
	{
		threadList.emplace_back(threadFunc, i);
	}
}

ThreadPool::~ThreadPool()
{
	stop = true;
	cv.notify_all();
	for (std::thread& t : threadList)
	{
		t.join();
	}
}

inline void ThreadPool::pause()
{
	suspend = true;
}

inline void ThreadPool::resume()
{
	suspend = false;
	cv.notify_all();
}

inline void ThreadPool::shutdown()
{
	stop = true;
	cv.notify_all();
}

inline void ThreadPool::waitAllFinish()
{
	std::unique_lock<std::mutex> lock(taskMutex);
	cv.wait(lock, [this] { return !this->threadState && this->taskQueue.empty(); });
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
