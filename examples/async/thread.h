#ifndef THREAD_H
#define THREAD_H

#include <assert.h>

#ifdef NDEBUG
	#define ensureThat(x) (void)(x)
#else
	#define ensureThat(x) assert(x)
#endif

#ifdef WIN32
	#include <Windows.h>
	typedef HANDLE Thread;
	typedef CRITICAL_SECTION Mutex;
	typedef CONDITION_VARIABLE ConditionVariable;
	typedef DWORD (*ThreadFunc)(LPVOID);

	static __inline int threadMutexInit(Mutex *mutex) {
		InitializeCriticalSection(mutex);
		return 0;
	}
	static __inline void threadMutexLock(Mutex *mutex) {
		EnterCriticalSection(mutex);
	}
	static __inline void threadMutexUnlock(Mutex *mutex) {
		LeaveCriticalSection(mutex);
	}
	static __inline void threadMutexDestroy(Mutex *mutex) {
		DeleteCriticalSection(mutex);
	}
	static __inline int threadCondInit(ConditionVariable *cond) {
		InitializeConditionVariable(cond);
		return 0;
	}
	static __inline void threadCondWait(ConditionVariable *cond, Mutex *mutex) {
		SleepConditionVariableCS(cond, mutex, INFINITE);
	}
	static __inline void threadCondSignal(ConditionVariable *cond) {
		WakeConditionVariable(cond);
	}
	static __inline void threadCondDestroy(ConditionVariable *cond) {
		(void)cond; // no need to destroy Windows condition variables
	}
	static __inline int threadCreate(Thread *thread, ThreadFunc function, void *argument) {
		*thread = CreateThread(NULL, 0, function, argument, 0, NULL);
		if ( *thread ) {
			return 0;
		} else {
			return (int)GetLastError();
		}
	}
	static __inline void threadJoin(Thread thread) {
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
	}
#else
	#include <pthread.h>
	typedef pthread_t Thread;
	typedef pthread_mutex_t Mutex;
	typedef pthread_cond_t ConditionVariable;
	typedef void* (*ThreadFunc)(void*);

	static inline int threadMutexInit(Mutex *mutex) {
		return pthread_mutex_init(mutex, NULL); // can fail with ENOMEM or EAGAIN
	}
	static inline void threadMutexLock(Mutex *mutex) {
		const int status = pthread_mutex_lock(mutex);  // failure modes are all static
		ensureThat(status == 0);
	}
	static inline void threadMutexUnlock(Mutex *mutex) {
		const int status = pthread_mutex_unlock(mutex);  // failure modes are all static
		ensureThat(status == 0);
	}
	static inline void threadMutexDestroy(Mutex *mutex) {
		const int status = pthread_mutex_destroy(mutex);  // failure modes are all static
		ensureThat(status == 0);
	}
	static inline int threadCondInit(ConditionVariable *cond) {
		return pthread_cond_init(cond, NULL); // can fail with ENOMEM or EAGAIN
	}
	static inline void threadCondWait(ConditionVariable *cond, Mutex *mutex) {
		const int status = pthread_cond_wait(cond, mutex);
		ensureThat(status == 0);
	}
	static inline void threadCondSignal(ConditionVariable *cond) {
		const int status = pthread_cond_signal(cond);
		ensureThat(status == 0);
	}
	static inline void threadCondDestroy(ConditionVariable *cond) {
		const int status = pthread_cond_destroy(cond);
		ensureThat(status == 0);
	}
	static inline int threadCreate(Thread *thread, ThreadFunc function, void *argument) {
		return pthread_create(thread, NULL, function, argument);
	}
	static inline void threadJoin(Thread thread) {
		const int status = pthread_join(thread, NULL);
		ensureThat(status == 0);
	}
#endif

#endif
