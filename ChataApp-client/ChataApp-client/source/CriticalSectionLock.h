#pragma once

#ifdef __linux__
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

class CriticalSectionLock
{
public:
    CriticalSectionLock();
    ~CriticalSectionLock();
    bool TryEnter();
    void Enter();
    void Leave();

private:
#ifdef __linux__
    pthread_mutex_t _mutex;
    pthread_mutexattr_t _attr;
#elif defined(_WIN32)
    CRITICAL_SECTION _cs;
#endif
};

