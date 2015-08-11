
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

inline Push_Page *push_allocator_new_page(Push_Allocator *allocator, size_t size_hint)
{
	size_t new_size = max(allocator->page.size * 2, size_hint);

	Push_Page *old_page = 0;
	if (allocator->page.size > 0) {
		old_page = (Push_Page*)malloc(sizeof(Push_Page));
		*old_page = allocator->page;
	}

	Push_Page *new_page = &allocator->page;
	new_page->previous = old_page;
	new_page->buffer = (char*)malloc(new_size);
	new_page->position = 0;
	new_page->size = new_size;

	return new_page;
}

inline void *push_allocator_push(Push_Allocator *allocator, size_t size)
{
	Push_Page *page = &allocator->page;
	if (page->size - page->position < size) {
		page = push_allocator_new_page(allocator, size);
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
	free(allocator->page.buffer);

	Push_Page *page = allocator->page.previous;
	while (page) {
		Push_Page *prev_page = page->previous;
		free(page->buffer);
		free(page);
		page = prev_page;
	}
}

#define PUSH_ALLOC(allocator, type) ((type*)push_allocator_push((allocator), sizeof(type)))
#define PUSH_ALLOC_N(allocator, type, n) ((type*)push_allocator_push((allocator), (n) * sizeof(type)))
#define PUSH_ALLOC_STR(allocator, n) to_string(((char*)push_allocator_push((allocator), (n))), n)

#define PUSH_COPY(allocator, type, data) ((type*)push_allocator_copy((allocator), sizeof(type), (data)))
#define PUSH_COPY_N(allocator, type, n, data) ((type*)push_allocator_copy((allocator), (n) * sizeof(type), (data)))
#define PUSH_COPY_STR(allocator, str) to_string(((char*)push_allocator_copy((allocator), (str).length, (str).data)), (str).length)

