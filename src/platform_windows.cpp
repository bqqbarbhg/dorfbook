
#define NOMINMAX
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

typedef LARGE_INTEGER os_timer_mark;
LARGE_INTEGER os_windows_performance_counter_freq;

os_timer_mark os_get_timer()
{
	LARGE_INTEGER value;
	QueryPerformanceCounter(&value);
	return value;
}

float os_timer_delta_ms(os_timer_mark begin, os_timer_mark end)
{
	I64 diff = end.QuadPart - begin.QuadPart;
	I64 ticks = diff * 100000LL / os_windows_performance_counter_freq.QuadPart;
	float ms = (float)ticks / 100.0f;
	return ms;
}

typedef SOCKET os_socket;

enum Close_Mode
{
	Close_Read,
	Close_Write,
	Close_ReadWrite,
};

inline bool os_valid_socket(os_socket sock)
{
	return sock != INVALID_SOCKET;
}

void os_socket_format_last_error(char *buffer, U32 buffer_length)
{
	// TODO: Text errors for windows
	_snprintf(buffer, buffer_length, "%d", WSAGetLastError());
}

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
inline void os_startup()
{
	WSADATA wsadata;
	WSAStartup(0x0202, &wsadata);

	QueryPerformanceFrequency(&os_windows_performance_counter_freq);
}

inline void os_cleanup()
{
	WSACleanup();
}
