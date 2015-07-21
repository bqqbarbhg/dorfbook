
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/tcp.h>

typedef timespec os_timer_mark;

os_timer_mark os_get_timer()
{
	timespec value;
	clock_gettime(CLOCK_MONOTONIC, &value);
	return value;
}

float os_timer_delta_ms(os_timer_mark begin, os_timer_mark end)
{
	time_t sec_diff = end.tv_sec - begin.tv_sec;
	int nano_diff = end.tv_nsec - begin.tv_nsec;
	float ms = sec_diff * 1000.0f + nano_diff / 1000000.0f;
}

typedef int os_socket;

inline bool os_valid_socket(os_socket sock)
{
	return sock != -1;
}

void os_socket_format_last_error(char *buffer, U32 buffer_length)
{
	// TODO: Text errors for linux
	snprintf(buffer, buffer_length, "%d", errno);
}

void os_socket_stop_recv(os_socket sock)
{
	shutdown(sock, SHUT_RD);
}

void os_socket_close(os_socket sock)
{
	close(sock);
}

int os_socket_send(os_socket sock, const char *data, int length)
{
	return send(sock, data, length, MSG_NOSIGNAL);
}

bool os_socket_set_delayed(os_socket sock, bool delayed) {
	int flag = delayed ? 0 : 1;
	return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(char*)&flag, sizeof(int)) == 0;
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
	timeval recv_time;
	recv_time.tv_sec = recv_sec;
	recv_time.tv_usec = 0;
	timeval send_time;
	send_time.tv_sec = send_sec;
	send_time.tv_usec = 0;

	unsigned fail = 0;
	fail |= (unsigned)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
		(const char*)&recv_time, sizeof(recv_time));
	fail |= (unsigned)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		(const char*)&send_time, sizeof(send_time));
	return fail == 0;
}

typedef pthread_mutex_t os_mutex;

inline void os_mutex_init(os_mutex *mutex)
{
	pthread_mutex_init(mutex, 0);
}

inline void os_mutex_lock(os_mutex *mutex)
{
	pthread_mutex_lock(mutex);
}

inline void os_mutex_unlock(os_mutex *mutex)
{
	pthread_mutex_unlock(mutex);
}

inline void os_sleep_seconds(int seconds)
{
	sleep(seconds);
}

typedef volatile U32 os_atomic_uint32;

inline void os_atomic_increment(os_atomic_uint32 *value)
{
	__sync_fetch_and_add(value, 1);
}

inline void os_atomic_decrement(os_atomic_uint32 *value)
{
	__sync_fetch_and_sub(value, 1);
}

#define OS_THREAD_ENTRY(function, param) void* function(void *param)
#define OS_THREAD_RETURN return 0

typedef pthread_t os_thread;
typedef void* (*os_thread_func)(void*);

inline os_thread os_thread_do(os_thread_func func, void *param)
{
	os_thread result;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, 1024*1024);
	pthread_create(&result, &attr, func, param);
	return result;
}

inline void os_startup()
{
}

inline void os_cleanup()
{
}
