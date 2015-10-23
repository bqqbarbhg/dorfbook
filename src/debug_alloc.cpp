
#ifdef BUILD_DEBUG

#define DEBUG_ALLOC_HEADER_MAGIC 0xDEDEDEDE
#define DEBUG_ALLOC_ALLOCATED_STATE 0xA1A1A1A1
#define DEBUG_ALLOC_FREED_STATE 0xAFAFAFAF

struct Debug_Alloc_Header
{
	U32 magic;
	U32 state;
	os_thread_id thread_id;
	const char *type;
	size_t type_size;
	void *alloc_trace[8];
	void *free_trace[8];
	U32 alloc_trace_length;
	U32 free_trace_length;
	Debug_Alloc_Header *prev, *next;
	Source_Loc alloc_loc;
	Source_Loc free_loc;
	U64 size;
	U64 prev_serial, next_serial;
	U64 serial;
};

Debug_Alloc_Header g_debug_log_storage[100000];

struct Debug_Memory
{
	os_mutex lock;
	U64 serial;
	Debug_Alloc_Header root;
	Debug_Alloc_Header *log;
	U32 log_index;
	U32 log_size;
};
Debug_Memory g_debug_memory;

void debug_alloc_init()
{
	os_mutex_init(&g_debug_memory.lock);
	g_debug_memory.log = g_debug_log_storage;
	g_debug_memory.log_size = Count(g_debug_log_storage);
}

Debug_Alloc_Header *debug_alloc_lock_heap()
{
	os_mutex_lock(&g_debug_memory.lock);
	return g_debug_memory.root.next;
}

void debug_alloc_unlock_heap()
{
	os_mutex_unlock(&g_debug_memory.lock);
}

struct Debug_Alloc_Snapshot
{
	Debug_Alloc_Snapshot *prev, *next;
	Debug_Alloc_Header header;
	void *pointer;
};

Debug_Alloc_Snapshot* debug_thread_memory_snapshot(os_thread_id thread_id)
{
	Debug_Alloc_Snapshot *root = 0;

	Debug_Alloc_Header *header = debug_alloc_lock_heap();
	for (; header; header = header->next) {
		if (os_thread_id_equal(header->thread_id, thread_id)) {
			Debug_Alloc_Snapshot *copy = (Debug_Alloc_Snapshot*)malloc(sizeof(Debug_Alloc_Snapshot));
			copy->header = *header;
			copy->pointer = header + 1;

			if (root) root->prev = copy;
			copy->next = root;
			copy->prev = 0;
			root = copy;
		}
	}

	debug_alloc_unlock_heap();

	return root;
}

Debug_Alloc_Snapshot *debug_snapshot_new_allocations(Debug_Alloc_Snapshot *begin, Debug_Alloc_Snapshot *end)
{
	Debug_Alloc_Snapshot *root = 0;

	for (Debug_Alloc_Snapshot *e = end; e; e = e->next) {
		bool found = false;

		for (Debug_Alloc_Snapshot *b = begin; b; b = b->next) {
			if (b->pointer == e->pointer) {
				found = true;
				break;
			}
		}

		if (!found) {
			Debug_Alloc_Snapshot *copy = (Debug_Alloc_Snapshot*)malloc(sizeof(Debug_Alloc_Snapshot));
			*copy = *e;

			if (root) root->prev = copy;
			copy->next = root;
			copy->prev = 0;
			root = copy;
		}
	}

	return root;
}

Debug_Alloc_Snapshot *debug_thread_memory_snapshot()
{
	return debug_thread_memory_snapshot(os_current_thread_id());
}

void debug_alloc_snapshot_free(Debug_Alloc_Snapshot *snapshot)
{
	while (snapshot) {
		Debug_Alloc_Snapshot *next = snapshot->next;
		free(snapshot);
		snapshot = next;
	}
}

Debug_Alloc_Header *unsafe_debug_alloc_get_serial(U64 serial)
{
	if (!g_debug_memory.log || !serial)
		return 0;

	I64 diff = (I64)(g_debug_memory.serial - serial);
	I64 index = (I64)g_debug_memory.log_index - diff;

	if (index < 0) {
		index += g_debug_memory.log_size;
	}

	if (index < 0) {
		return 0;
	}

	Debug_Alloc_Header *header = g_debug_memory.log + index;
	if (header->serial != serial)
		return 0;

	return header;
}

bool debug_alloc_get_serial(U64 serial, Debug_Alloc_Header *out)
{
	os_mutex_lock(&g_debug_memory.lock);

	Debug_Alloc_Header *header = unsafe_debug_alloc_get_serial(serial);
	if (header) {
		*out = *header;
	}

	os_mutex_unlock(&g_debug_memory.lock);

	return header != 0;
}

void *debug_allocate(size_t size, const char *type, size_t type_size, Source_Loc loc)
{
	Debug_Alloc_Header *header_and_data = (Debug_Alloc_Header*)
		malloc(sizeof(Debug_Alloc_Header) + size);

	if (!header_and_data)
		return 0;

	header_and_data->magic = DEBUG_ALLOC_HEADER_MAGIC;
	header_and_data->state = DEBUG_ALLOC_ALLOCATED_STATE;
	header_and_data->type = type;
	header_and_data->type_size = type_size;
	header_and_data->size = size;
	header_and_data->alloc_loc = loc;
	header_and_data->free_loc.file = 0;
	header_and_data->free_loc.line = 0;
	header_and_data->free_trace_length = 0;
	header_and_data->thread_id = os_current_thread_id();
	header_and_data->prev_serial = 0;
	header_and_data->next_serial = 0;

	header_and_data->alloc_trace_length = os_capture_stack_trace(
			header_and_data->alloc_trace,
			Count(header_and_data->alloc_trace));

	os_mutex_lock(&g_debug_memory.lock);

	header_and_data->serial = ++g_debug_memory.serial;
	header_and_data->prev = &g_debug_memory.root;
	header_and_data->next = g_debug_memory.root.next;
	if (g_debug_memory.root.next)
		g_debug_memory.root.next->prev = header_and_data;
	g_debug_memory.root.next = header_and_data;

	if (g_debug_memory.log) {
		g_debug_memory.log_index = (g_debug_memory.log_index + 1) % g_debug_memory.log_size;
		g_debug_memory.log[g_debug_memory.log_index] = *header_and_data;
	}

#ifdef BREAK_AT_SERIAL
	if (header_and_data->serial == BREAK_AT_SERIAL)
		os_debug_break();
#endif

	os_mutex_unlock(&g_debug_memory.lock);

	return header_and_data + 1;
}

void *debug_allocate_nonzero(size_t size, const char *type, size_t type_size, Source_Loc loc)
{
	void *mem = debug_allocate(size, type, type_size, loc);
	if (mem) memset(mem, 0xAA, size);
	return mem;
}

void *debug_allocate_zero(size_t size, const char *type, size_t type_size, Source_Loc loc)
{
	void *mem = debug_allocate(size, type, type_size, loc);
	if (mem) memset(mem, 0x00, size);
	return mem;
}

Debug_Alloc_Header *debug_get_header(void *ptr) {
	Debug_Alloc_Header *header = (Debug_Alloc_Header*)((char*)ptr - sizeof(Debug_Alloc_Header));
	assert(header->magic == DEBUG_ALLOC_HEADER_MAGIC);
	return header;
}

void debug_free(void *ptr, Source_Loc loc)
{
	if (!ptr) return;

	Debug_Alloc_Header *header = debug_get_header(ptr);

	memset(ptr, 0xFE, header->size);

	os_mutex_lock(&g_debug_memory.lock);

	if (header->state != DEBUG_ALLOC_ALLOCATED_STATE) {
		// Double free
		assert(0);
	}
	header->state = DEBUG_ALLOC_FREED_STATE;
	header->free_loc = loc;

	header->free_trace_length = os_capture_stack_trace(
			header->free_trace,
			Count(header->free_trace));

	if (header->prev) {
		header->prev->next = header->next;
	}
	if (header->next) {
		header->next->prev = header->prev;
	}

	Debug_Alloc_Header *logged = unsafe_debug_alloc_get_serial(header->serial);
	if (logged) {
		*logged = *header;
	}

	os_mutex_unlock(&g_debug_memory.lock);

	free(header);
}

void *debug_reallocate(void *ptr, size_t size, const char *type, size_t type_size, Source_Loc loc)
{
	if (!ptr) {
		return debug_allocate(size, type, type_size, loc);
	} else if (!size) {
		debug_free(ptr, loc);
		return 0;
	}

	os_mutex_lock(&g_debug_memory.lock);

	Debug_Alloc_Header *header = debug_get_header(ptr);

	if (header->state != DEBUG_ALLOC_ALLOCATED_STATE) {
		// Already freed
		assert(0);
	}

	if (header->prev) {
		header->prev->next = header->next;
	}
	if (header->next) {
		header->next->prev = header->prev;
	}
	header->free_loc = loc;

	header->free_trace_length = os_capture_stack_trace(
			header->free_trace,
			Count(header->free_trace));

	U64 old_serial = header->serial;
	Debug_Alloc_Header *logged = unsafe_debug_alloc_get_serial(old_serial);
	if (logged) {
		*logged = *header;
	}

	os_mutex_unlock(&g_debug_memory.lock);

	size_t old_type_size = header->type_size;
	Debug_Alloc_Header *new_header = (Debug_Alloc_Header*)realloc(header, size + sizeof(Debug_Alloc_Header));

	new_header->size = size;
	new_header->alloc_loc = loc;
	new_header->thread_id = os_current_thread_id();
	new_header->prev_serial = old_serial;

	new_header->alloc_trace_length = os_capture_stack_trace(
			new_header->alloc_trace,
			Count(new_header->alloc_trace));

	assert(new_header->type_size == old_type_size);

	os_mutex_lock(&g_debug_memory.lock);

	new_header->serial = ++g_debug_memory.serial;
	new_header->prev = &g_debug_memory.root;
	new_header->next = g_debug_memory.root.next;
	if (g_debug_memory.root.next)
		g_debug_memory.root.next->prev = new_header;
	g_debug_memory.root.next = new_header;

	if (g_debug_memory.log) {
		g_debug_memory.log_index = (g_debug_memory.log_index + 1) % g_debug_memory.log_size;
		g_debug_memory.log[g_debug_memory.log_index] = *new_header;
	}

	Debug_Alloc_Header *old_logged = unsafe_debug_alloc_get_serial(old_serial);
	if (old_logged) {
		old_logged->next_serial = new_header->serial;
	}

#ifdef BREAK_AT_SERIAL
	if (new_header->serial == BREAK_AT_SERIAL)
		os_debug_break();
#endif

	os_mutex_unlock(&g_debug_memory.lock);
	return new_header + 1;
}

#endif

#ifdef BUILD_DEBUG

#define M_ALLOC_RAW(size) (debug_allocate_nonzero((size), "void", 1, SOURCE_LOC))
#define M_ALLOC_RAW_ZERO(size) (debug_allocate_zero((size), "void", 1, SOURCE_LOC))
#define M_ALLOC(type, count) ((type*)debug_allocate_nonzero(sizeof(type) * (count), #type, sizeof(type), SOURCE_LOC))
#define M_ALLOC_ZERO(type, count) ((type*)debug_allocate_zero(sizeof(type) * (count), #type, sizeof(type), SOURCE_LOC))

#define M_REALLOC_RAW(ptr, size) (debug_reallocate((ptr), (size), "void", 1, SOURCE_LOC))
#define M_REALLOC(ptr, type, count) ((type*)debug_reallocate((ptr), sizeof(type) * (count), #type, sizeof(type), SOURCE_LOC))

#define M_FREE(ptr) (debug_free((ptr), SOURCE_LOC))

#else

#define M_ALLOC_RAW(size) malloc((size))
#define M_ALLOC_RAW_ZERO(size) calloc((size), 1)
#define M_ALLOC(type, count) (type*)malloc(sizeof(type) * (count))
#define M_ALLOC_ZERO(type, count) (type*)calloc(sizeof(type), (count))

#define M_REALLOC_RAW(ptr, size) realloc((ptr), (size))
#define M_REALLOC(ptr, type, count) (type*)realloc(ptr, sizeof(type) * (count))

#define M_FREE(ptr) free(ptr)

#endif

