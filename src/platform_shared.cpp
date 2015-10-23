
struct os_symbol_info
{
	char *function;
	char *filename;

	int function_length;
	int filename_length;

	int line;
};

struct os_symbol_info_writer
{
	os_symbol_info *symbols;
	char *strings;
	int symbol_count;
	int string_pos, string_capacity;
};

bool os_symbol_writer_begin(os_symbol_info_writer *w, int symbol_count)
{
	int capacity = symbol_count * 128;

	void *memory = malloc(sizeof(os_symbol_info) * symbol_count + capacity);
	if (!memory)
		return false;

	w->symbols = (os_symbol_info*)memory;
	w->strings = (char*)memory + sizeof(os_symbol_info) * symbol_count;
	w->symbol_count = symbol_count;
	w->string_pos = 0;
	w->string_capacity = capacity;

	memset(w->symbols, 0, sizeof(os_symbol_info) * symbol_count);

	return true;
}

bool os_symbol_writer_push_string(os_symbol_info_writer *w, const char *str, int length)
{
	int copy_length = length + 1;

	if (w->string_capacity - w->string_pos < copy_length) {
		void *memory = w->symbols;
		int new_capacity = w->string_capacity * 2 + copy_length;
		memory = realloc(memory, sizeof(os_symbol_info) * w->symbol_count + new_capacity);
		if (!memory) {
			free(w->symbols);
			return false;
		}

		w->symbols = (os_symbol_info*)memory;
		w->strings = (char*)memory + sizeof(os_symbol_info) * w->symbol_count;
		w->string_capacity = new_capacity;
	}

	memcpy(w->strings + w->string_pos, str, length);
	w->strings[w->string_pos + length] = '\0';
	w->string_pos += copy_length;

	return true;
}

bool os_symbol_writer_set_location(os_symbol_info_writer *w, int index, const char *str, int length, int line)
{
	w->symbols[index].filename = (char*)0 + w->string_pos;
	w->symbols[index].filename_length = length;
	w->symbols[index].line = line;

	return os_symbol_writer_push_string(w, str, length);
}

bool os_symbol_writer_set_function(os_symbol_info_writer *w, int index, const char *str, int length)
{
	w->symbols[index].function = (char*)0 + w->string_pos;
	w->symbols[index].function_length = length;

	return os_symbol_writer_push_string(w, str, length);
}

os_symbol_info *os_symbol_writer_finish(os_symbol_info_writer *w)
{
	int count = w->symbol_count;

	for (int i = 0; i < count; i++) {
		os_symbol_info *symbol = w->symbols + i;

		if (symbol->filename_length > 0) {
			symbol->filename = w->strings + (symbol->filename - (char*)0);
		}
		if (symbol->function_length > 0) {
			symbol->function = w->strings + (symbol->function - (char*)0);
		}
	}

	return w->symbols;
}


