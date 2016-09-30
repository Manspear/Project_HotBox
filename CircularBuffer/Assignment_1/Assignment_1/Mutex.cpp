#include "Mutex.h"

Mutex::Mutex()
{
}

Mutex::Mutex(LPCWSTR mutexName)
{
	mutexHandle = CreateMutex(NULL, false, mutexName);
}

HANDLE Mutex::getMutex()
{
	return mutexHandle;
}

void Mutex::lock()
{
	DWORD waitRes = WaitForSingleObject(mutexHandle, INFINITE);
}

void Mutex::unlock()
{
	ReleaseMutex(mutexHandle);
}
