#pragma once
#include <Windows.h>
class Mutex
{
private:
	HANDLE mutexHandle;
public:
	Mutex();
	Mutex(LPCWSTR mutexName);
	HANDLE getMutex();
	void lock();
	void unlock();
};