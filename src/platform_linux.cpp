
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

typedef int os_socket;

inline void os_net_startup()
{
	// Not needed on linux.
}

inline void os_net_cleanup()
{
	// Not needed on linux.
}

enum Close_Mode
{
	Close_Read = SHUT_RD,
	Close_Write = SHUT_WR,
	Close_ReadWrite = SHUT_RDWR,
};

void os_socket_close(os_socket sock, Close_Mode mode)
{
	shutdown((int)mode);
}

typedef pthread_mutex_t os_mutex;

inline void os_mutex_init(os_mutex *mutex)
{
	pthread_mutex_init(mutex);
}

inline void os_mutex_lock(os_mutex *mutex)
{
	pthread_mutex_lock(mutex);
}

inline void os_mutex_unlock(os_mutex *mutex)
{
	pthread_mutex_unlock(mutex);
}

typedef volatile U32 os_atomic_uint32;

inline void os_atomic_increment(os_atomic_int32 *value)
{
	__sync_fetch_and_add(value, 1);
}

inline void os_atomic_decrement(os_atomic_int32 *value)
{
	__sync_fetch_and_sub(value, 1);
}

#define OS_THREAD_ENTRY(function, param) void* function(void *param)
#define OS_THREAD_RETURN return 0

typedef pthread_t os_thread;

inline os_thread os_thread_do(os_thread_func func, void *param)
{
	os_thread result;
	pthread_create(&result, 0, func, param);
	return result;
}

