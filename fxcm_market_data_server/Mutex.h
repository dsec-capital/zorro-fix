#pragma once

#include <Windows.h>

/**
    The class implements a recursive mutex object for Windows/Linux/MacOS platform.
*/
class Mutex
{
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();

    class Lock
    {
    public:
        Lock(Mutex& m) : mutex(&m)
        {
            mutex->lock();
        }

        Lock(Mutex* m) : mutex(m)
        {
            mutex->lock();
        }

        ~Lock()
        {
            mutex->unlock();
        }
    private:
        Mutex* mutex;
    };

private:
#ifdef PTHREADS_MUTEX
    pthread_mutex_t m_oMutex;
#else
    CRITICAL_SECTION m_oCritSection;
#endif
};

