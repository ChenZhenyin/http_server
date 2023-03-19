#include "threadpool.h"

threadpool* g_threadpool = NULL;

threadpool :: threadpool(int _threadNum, int _maxRequests) :
	threadNumber(_threadNum), maxRequests(_maxRequests), isStop(false) {

	if ((_threadNum <= 0) || (_maxRequests <= 0)) {
		throw std::exception();
	}

	threadLists = new pthread_t[threadNumber];
	if (not threadLists) {
		throw std::exception();
	}

	for (int i = 0; i < threadNumber; ++i) {
		printf("Create the %dth thread!\n", i);
		if (pthread_create(threadLists + i, NULL, worker, this) != 0) {
			delete []threadLists;
			throw std::exception();
		}

		if (pthread_detach(threadLists[i])) {
			delete []threadLists;
			throw std::exception();
		}
	}

	printf("Threadpool created successfully!\n");
}

threadpool :: ~threadpool() {

	isStop = true;
	delete []threadLists;
}

bool threadpool :: append(Task* request) {

	queueLocker.lock();
	if (workQueue.size() > maxRequests) {
		queueLocker.unlock();
		return false;
	}
	workQueue.push_back(request);
	queueLocker.unlock();
	queueStat.post();
	return true;
}

void* threadpool :: worker(void* arg) {
	threadpool* pool = (threadpool*) arg;
	pool->run();
	return pool;
}

void threadpool :: run() {
	while (!isStop) {
		queueStat.wait();

		queueLocker.lock();
		if (workQueue.empty()) {
			queueLocker.unlock();
			continue;
		}
		Task* request = workQueue.front();
		workQueue.pop_front();
		queueLocker.unlock();
		if (!request) {
			continue;
		}
		request->process();
	}
}
