struct String_Table
{
	Push_Allocator alloc;
	U32 *hashes;
	U32 *lengths;
	char **datas;
	size_t count;
	size_t size;

#ifdef BUILD_DEBUG
	void *debug_table_id;
#endif
};

void string_table_free(String_Table *table)
{
	push_allocator_free(&table->alloc);
	M_FREE(table->hashes);
	M_FREE(table->lengths);
	M_FREE(table->datas);
}

struct Interned_String
{
	String string;
#ifdef BUILD_DEBUG
	void *debug_table_id;
#endif
};

U32 string_hash(String str)
{
	U32 hash = 0;
	for (size_t i = 0; i < str.length; i++) {
		hash = hash * 123 + str.data[i];
	}
	return hash;
}

#define wrap_advance(x, cap) (((x) + 1) % (cap))

void string_table_rehash(String_Table *table)
{
	// TODO: Prime table sizes?
	size_t old_size = table->size;
	size_t new_size = max(old_size * 2, 8);

	U32 *old_hashes = table->hashes;
	U32 *old_lengths = table->lengths;
	char **old_datas = table->datas;

	// TODO: Compound allocation?
	U32 *new_hashes = M_ALLOC_ZERO(U32, new_size);
	U32 *new_lengths = M_ALLOC_ZERO(U32, new_size);
	char **new_datas = M_ALLOC_ZERO(char*, new_size);

	for (size_t old_index = 0; old_index < old_size; old_index++) {
		size_t new_index = old_hashes[old_index] % new_size;
		while (new_datas[new_index]) {
			new_index = wrap_advance(new_index, new_size);
		}
		new_hashes[new_index] = old_hashes[old_index];
		new_lengths[new_index] = old_lengths[old_index];
		new_datas[new_index] = old_datas[old_index];
	}

	M_FREE(old_hashes);
	M_FREE(old_lengths);
	M_FREE(old_datas);

#ifdef BUILD_DEBUG
	if (!table->debug_table_id) {
		table->debug_table_id = new_hashes;
	}
#endif

	table->hashes = new_hashes;
	table->lengths = new_lengths;
	table->datas = new_datas;
	table->size = new_size;
}

void free_string_table(String_Table *table)
{
	push_allocator_free(&table->alloc);
	M_FREE(table->hashes);
	M_FREE(table->lengths);
	M_FREE(table->datas);
}

struct String_Table_Position
{
	size_t index;
	U32 hash;
	bool exists;
};

String_Table_Position string_table_find(String_Table *table, String str)
{
	assert(str.length <= UINT32_MAX);

	U32 hash = string_hash(str);
	size_t index = hash % table->size;

	U32 *hashes = table->hashes;
	U32 *lengths = table->lengths;
	char **datas = table->datas;
	size_t size = table->size;

	size_t length;
	while (length = lengths[index]) {
		if (hashes[index] == hash
			&& length == str.length
			&& !memcmp(datas[index], str.data, length)) {
			String_Table_Position result;
			result.index = index;
			result.hash = hash;
			result.exists = true;
			return result;
		}
		index = wrap_advance(index, size);
	}
	String_Table_Position result;
	result.index = index;
	result.hash = hash;
	result.exists = false;
	return result;
}

Interned_String intern(String_Table *table, String str)
{
	if (table->count >= table->size * 3 / 4) {
		string_table_rehash(table);
	}

	// Note: This has to be after the rehashing so that interning and empty
	// string to an empty table has the correct `debug_table_id` set up.
	if (str.length == 0) {
		Interned_String empty;
		empty.string = empty_string();
#ifdef BUILD_DEBUG
		empty.debug_table_id = table->debug_table_id;
#endif
		return empty;
	}

	String_Table_Position pos = string_table_find(table, str);
	if (!pos.exists) {
		String copy = PUSH_COPY_STR(&table->alloc, str);
		table->hashes[pos.index] = pos.hash;
		table->lengths[pos.index] = (U32)copy.length;
		table->datas[pos.index] = copy.data;
		table->count++;
	}
	Interned_String result;
	result.string = to_string(table->datas[pos.index], table->lengths[pos.index]);
#ifdef BUILD_DEBUG
	result.debug_table_id = table->debug_table_id;
#endif
	return result;
}

bool intern_if_not_new(Interned_String *result, String_Table *table, String str)
{
	String_Table_Position pos = string_table_find(table, str);
	if (!pos.exists)
		return false;

	result->string = to_string(table->datas[pos.index], table->lengths[pos.index]);
#ifdef BUILD_DEBUG
	result->debug_table_id = table->debug_table_id;
#endif
	return true;
}

bool operator==(Interned_String a, Interned_String b) {
#if BUILD_DEBUG
	assert(a.debug_table_id == b.debug_table_id);
#endif
	return a.string.data == b.string.data;
}

bool operator!=(Interned_String a, Interned_String b) {
	return !(a == b);
}

