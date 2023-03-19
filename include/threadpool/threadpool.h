#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <cstdio>
#include <list>

#include "Type.h"
#include "Task.h"

using std::list;

class threadpool {
public:
	threadpool(int _threadNum = 10, int _maxRequests = 10000);
	~threadpool();
	bool append(Task* request);
private:
	static void* worker(void* arg);
	void run();
private:
	int threadNumber;
	int maxRequests;
	pthread_t* threadLists;
	list<Task*> workQueue;
	locker queueLocker;
	sem queueStat;
	bool isStop;
};

extern threadpool* g_threadpool;

#endif
