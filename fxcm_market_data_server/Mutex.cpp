#include "Mutex.h"

Mutex::Mutex()
{
#ifdef PTHREADS_MUTEX
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_oMutex, &attr);
    pthread_mutexattr_destroy(&attr);
#else
    ::InitializeCriticalSectionAndSpinCount(&m_oCritSection, 4000);
#endif
}

Mutex::~Mutex()
{
#ifdef PTHREADS_MUTEX
    pthread_mutex_destroy(&m_oMutex);
#else
    ::DeleteCriticalSection(&m_oCritSection);
#endif
}

void Mutex::lock()
{
#ifdef PTHREADS_MUTEX
    pthread_mutex_lock(&m_oMutex);
#else
    ::EnterCriticalSection(&m_oCritSection);
#endif
}

void Mutex::unlock()
{
#ifdef PTHREADS_MUTEX
    pthread_mutex_unlock(&m_oMutex);
#else
    ::LeaveCriticalSection(&m_oCritSection);
#endif
}