
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

typedef SOCKET os_socket;

inline void os_net_startup()
{
	WSADATA wsadata;
	WSAStartup(0x0202, &wsadata);
}

inline void os_net_cleanup()
{
	WSACleanup();
}

enum Close_Mode
{
	Close_Read,
	Close_Write,
	Close_ReadWrite,
};

void os_socket_close(os_socket sock, Close_Mode mode)
{
	// TODO: Close modes for windows
	closesocket(sock);
}

typedef CRITICAL_SECTION os_mutex;

inline void os_mutex_init(os_mutex *mutex)
{
	InitializeCriticalSection(mutex);
}

inline void os_mutex_lock(os_mutex *mutex)
{
	EnterCriticalSection(mutex);
}

inline void os_mutex_unlock(os_mutex *mutex)
{
	LeaveCriticalSection(mutex);
}

inline void os_sleep_seconds(int seconds)
{
	Sleep(seconds * 1000);
}

typedef volatile DWORD os_atomic_uint32;

inline void os_atomic_increment(os_atomic_uint32 *value)
{
	InterlockedIncrement(value);
}

inline void os_atomic_decrement(os_atomic_uint32 *value)
{
	InterlockedDecrement(value);
}

#define OS_THREAD_ENTRY(function, param) DWORD WINAPI function(void *param)
#define OS_THREAD_RETURN return 0
typedef DWORD (WINAPI *os_thread_func)(void*);

struct os_thread
{
	HANDLE handle;
	DWORD id;
};

inline os_thread os_thread_do(os_thread_func func, void *param)
{
	os_thread result;
	result.handle = CreateThread(NULL, NULL, func, param, NULL, &result.id);
	return result;
}

