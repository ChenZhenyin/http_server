#ifndef __TYPE_H__
#define __TYPE_H__

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem {
public:
	sem() {
		if (sem_init(&_sem, 0, 0) != 0) {
			throw std::exception();	
		}
	}
	~sem() {sem_destroy(&_sem);}
	bool wait() {return sem_wait(&_sem) == 0;}
	bool post() {return sem_post(&_sem) == 0;}
private:
	sem_t _sem;
};

class locker {
public:
	locker() {
		if (pthread_mutex_init(&mutex, NULL) != 0) {
			throw std::exception();	
		}
	}
	~locker() {pthread_mutex_destroy(&mutex);}
	bool lock() {return pthread_mutex_lock(&mutex) == 0;}
	bool unlock() {return pthread_mutex_unlock(&mutex) == 0;}
private:
	pthread_mutex_t mutex;
};

#endif
