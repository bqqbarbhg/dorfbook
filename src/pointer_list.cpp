
#define POINTER_LIST_HEADER_MAGIC 0x8A8B8C8D

struct Pointer_List_Header
{
	U32 magic;
	U32 reserved;
	U32 count;
	U32 capacity;
};

inline Pointer_List_Header *pointer_list_header(void *list)
{
	if (list == 0)
		return 0;

	Pointer_List_Header *header = (Pointer_List_Header*)list - 1;
	assert(header->magic == POINTER_LIST_HEADER_MAGIC);
	return header;
}

inline U32 pointer_list_count(void *list)
{
	Pointer_List_Header *header = pointer_list_header(list);
	return header ? header->count : 0;
}

Pointer_List_Header *pointer_list_reserve(Push_Allocator *alloc, void **list, U32 element_size, U32 capacity)
{
	Pointer_List_Header *header = pointer_list_header(*list);
	if (header) {

		if (header->capacity < capacity) {
			U32 old_size = sizeof(Pointer_List_Header) + header->capacity * element_size;
			U32 new_capacity = max(header->capacity * 2, capacity);
			U32 new_size = sizeof(Pointer_List_Header) + new_capacity * element_size;
			Pointer_List_Header *new_header = (Pointer_List_Header*)PUSH_ALLOC_N(alloc, char, new_size);

			memcpy(new_header, header, old_size);
			new_header->capacity = new_capacity;

			*list = new_header + 1;
			return new_header;
		} else {
			return header;
		}
	} else {

		U32 size = sizeof(Pointer_List_Header) + capacity * element_size;
		Pointer_List_Header* new_header = (Pointer_List_Header*)PUSH_ALLOC_N(alloc, char, size);

		new_header->magic = POINTER_LIST_HEADER_MAGIC;
		new_header->count = 0;
		new_header->capacity = capacity;

		*list = new_header + 1;
		return new_header;
	}
}

void pointer_list_push_n(Push_Allocator *alloc, void **list, U32 element_size, U32 count)
{
	U32 new_count = pointer_list_count(*list) + count;
	Pointer_List_Header *header = pointer_list_reserve(alloc, list, element_size, new_count);
	header->count = new_count;
}

#define pl_count(list) ( pointer_list_count(list) )
#define pl_reserve(alloc, list, count) ( pointer_list_reserve((alloc), (void**)&(list), sizeof(*(list)), (count)) )
#define pl_push_n(alloc, list, count) ( pointer_list_push_n((alloc), (void**)&(list), sizeof(*(list)), (count)), list + pl_count(list) - count )
#define pl_push(alloc, list) ( pl_push_n(alloc, list, 1) )
#define pl_push_copy(alloc, list, value) ( *pl_push(alloc, list) = value, list + pl_count(list) - 1 )

