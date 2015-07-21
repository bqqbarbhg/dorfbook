
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

inline bool os_valid_socket(os_socket sock)
{
	return sock != INVALID_SOCKET;
}

void os_socket_format_last_error(char *buffer, U32 buffer_length)
{
	// TODO: Text errors for windows
	_snprintf(buffer, buffer_length, "%d", WSAGetLastError());
}

void os_socket_stop_recv(os_socket sock)
{
	// TODO: Stop socket receiving for windows
}

void os_socket_close(os_socket sock)
{
	closesocket(sock);
}

int os_socket_send(os_socket sock, const char *data, int length)
{
	return send(sock, data, length, 0);
}

bool os_socket_set_delayed(os_socket sock, bool delayed) {
	BOOL flag = delayed ? FALSE : TRUE;
	return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(const char*)&flag, sizeof(flag)) == 0;
}

int os_socket_send_and_flush(os_socket sock, const char *data, int length)
{
	bool set = os_socket_set_delayed(sock, false);
	int ret = os_socket_send(sock, data, length);
	if (set) os_socket_set_delayed(sock, true);

	return ret;
}

bool os_socket_set_timeout(os_socket sock, int recv_sec, int send_sec)
{
	DWORD recv_time = recv_sec * 1000;
	DWORD send_time = send_sec * 1000;

	unsigned fail = 0;
	fail |= (unsigned)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
		(const char*)&recv_time, sizeof(recv_time));
	fail |= (unsigned)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		(const char*)&send_time, sizeof(send_time));
	return fail == 0;
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
