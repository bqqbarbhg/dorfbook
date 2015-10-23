
struct Data_Slice
{
	void *data;
	size_t length;
};

struct Push_Page
{
	Push_Page *previous;
	char *buffer;
	size_t position;
	size_t size;
};

struct Push_Allocator
{
	Push_Page page;
};

struct Push_Stream
{
	Push_Allocator *allocator;
	size_t start;
};

inline Push_Page *push_allocator_new_page(Push_Allocator *allocator, size_t size_hint)
{
	size_t new_size = max(allocator->page.size * 2, size_hint);

	Push_Page *old_page = 0;
	if (allocator->page.size > 0) {
		old_page = M_ALLOC(Push_Page, 1);
		*old_page = allocator->page;
	}

	Push_Page *new_page = &allocator->page;
	new_page->previous = old_page;
	new_page->buffer = M_ALLOC(char, new_size);
	new_page->position = 0;
	new_page->size = new_size;

	return old_page;
}

inline void *push_allocator_push(Push_Allocator *allocator, size_t size)
{
	Push_Page *page = &allocator->page;
	if (page->size - page->position < size) {
		push_allocator_new_page(allocator, size);
	}
	void *data = page->buffer + page->position;
	page->position += size;
	return data;
}

inline void *push_allocator_copy(Push_Allocator *allocator, size_t size, void *src)
{
	void *dst = push_allocator_push(allocator, size);
	memcpy(dst, src, size);
	return dst;
}

void push_allocator_free(Push_Allocator *allocator)
{
	M_FREE(allocator->page.buffer);

	Push_Page *page = allocator->page.previous;
	while (page) {
		Push_Page *prev_page = page->previous;
		M_FREE(page->buffer);
		M_FREE(page);
		page = prev_page;
	}
}

inline Push_Stream start_push_stream(Push_Allocator *allocator)
{
	Push_Stream stream;
	stream.allocator = allocator;
	stream.start = allocator->page.position;
	return stream;
}

inline Data_Slice finish_push_stream(Push_Stream *stream)
{
	Data_Slice slice;
	slice.data = stream->allocator->page.buffer + stream->start;
	slice.length = stream->allocator->page.position - stream->start;
	return slice;
}

inline String finish_push_stream_string(Push_Stream *stream)
{
	Data_Slice slice = finish_push_stream(stream);
	return to_string((char*)slice.data, slice.length);
}

inline void *push_stream_push(Push_Stream *stream, size_t size)
{
	Push_Page *page = &stream->allocator->page;
	if (page->size - page->position < size) {
		size_t prefix_size = page->position - stream->start;
		Push_Page *old_page = push_allocator_new_page(stream->allocator, prefix_size + size);
		if (old_page) {
			memcpy(page->buffer, old_page->buffer + stream->start, prefix_size);
		}
		page->position = prefix_size;
		stream->start = 0;
	}
	void *data = page->buffer + page->position;
	page->position += size;
	return data;
}

inline void *push_stream_copy(Push_Stream *stream, size_t size, void *src)
{
	void *dst = push_stream_push(stream, size);
	memcpy(dst, src, size);
	return dst;
}

#define PUSH_ALLOC(allocator, type) ((type*)push_allocator_push((allocator), sizeof(type)))
#define PUSH_ALLOC_N(allocator, type, n) ((type*)push_allocator_push((allocator), (n) * sizeof(type)))
#define PUSH_ALLOC_STR(allocator, n) to_string(((char*)push_allocator_push((allocator), (n))), n)

#define PUSH_COPY(allocator, type, data) ((type*)push_allocator_copy((allocator), sizeof(type), (data)))
#define PUSH_COPY_N(allocator, type, n, data) ((type*)push_allocator_copy((allocator), (n) * sizeof(type), (data)))
#define PUSH_COPY_STR(allocator, str) to_string(((char*)push_allocator_copy((allocator), (str).length, (str).data)), (str).length)

#define STREAM_ALLOC(stream, type) ((type*)push_stream_push((stream), sizeof(type)))
#define STREAM_ALLOC_N(stream, type, n) ((type*)push_stream_push((stream), (n) * sizeof(type)))
#define STREAM_ALLOC_STR(stream, n) to_string(((char*)push_stream_push((stream), (n))), n)

#define STREAM_COPY(stream, type, data) ((type*)push_stream_copy((stream), sizeof(type), (data)))
#define STREAM_COPY_N(stream, type, n, data) ((type*)push_stream_copy((stream), (n) * sizeof(type), (data)))
#define STREAM_COPY_STR(stream, str) to_string(((char*)push_stream_copy((stream), (str).length, (str).data)), (str).length)

#define LIST_STRUCT(type) struct type##_List { type *data; size_t count, capacity; }; \
	void list_realloc(type##_List *list, size_t size) { \
		size_t new_capacity = max(size, list->capacity * 2); \
		list->data = M_REALLOC(list->data, type, new_capacity); \
		list->capacity = new_capacity; \
	} \
	type* list_push(type##_List *list, size_t count=1) { \
		if (list->count + count > list->capacity) list_realloc(list, list->count + count); \
		type* dst = list->data + list->count; \
		list->count += count; \
		return dst; \
	} \
	type* list_push(type##_List *list, const type *data, size_t count=1) { \
		type* dst = list_push(list, count); \
		memcpy(dst, data, count * sizeof(type)); \
		return dst; \
	} \
	void list_free(type##_List *list) { M_FREE(list->data); }

