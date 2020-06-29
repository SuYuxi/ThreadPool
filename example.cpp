#include "ThreadPool.h"

void func(int sum)
{
	for (int i = 0; i < 100; ++i)
	{
		for (int j = 0; j < 100; ++j)
		{
			for (int k = 0; k < 100; ++k)
			{
				sum = sqrt(i * j * k);
			}
		}
	}
}

int main() {
  int threadNum = 4;  
  int taskNum = 160;
	ThreadPool p(threadNum);
	for(int i = 0, arg = 0; i < taskNum; ++i)
	{
		p.add(func, arg);
	}
	p.pause();
	p.resume();
	p.waitAllFinish();
}
