
#define JSON_MAX_NEST 128

enum Json_Context {
	Json_Context_Unknown = 0,
	Json_Context_Root = 1,
	Json_Context_Array = 2,
	Json_Context_Object = 3,
};

struct Json_Writer
{
	char *buffer, *ptr, *end;
	bool is_first_element;

	// Nested contexts are always object or array, so store them in a bitfield
	// 0: array
	// 1: object
	U32 nested_context_is_object[JSON_MAX_NEST];
	int nest_depth;

	U32 context;
	bool fail;
	bool has_key;
};

inline void json_init(Json_Writer *writer, char *buffer, size_t size)
{
	writer->buffer = buffer;
	writer->ptr = buffer;
	writer->end = buffer + size;
	writer->context = Json_Context_Root;
	writer->fail = false;
	writer->has_key = false;
	writer->nest_depth = 0;
}

void json_push_context(Json_Writer *writer, Json_Context context)
{
	if (writer->nest_depth > 0) {
		if (writer->nest_depth >= JSON_MAX_NEST) {
			writer->fail = true;
			return;
		}

		int nested_count = writer->nest_depth - 1;

		U32 *field = writer->nested_context_is_object + nested_count / 32;
		U32 bit = 1 << (nested_count % 32);

		if (writer->context == Json_Context_Object) {
			*field |= bit;
		} else {
			*field &= ~bit;
		}
	}

	writer->context = context;
	writer->nest_depth++;
}

void json_pop_context(Json_Writer *writer)
{
	writer->nest_depth--;

	int nested_count = writer->nest_depth - 1;
	if (nested_count < 0) {
		writer->context = Json_Context_Root;
	} else {
		U32 *cell = writer->nested_context_is_object + nested_count / 32;
		U32 bit = 1 << (nested_count % 32);

		writer->context = (*cell & bit) ? Json_Context_Object : Json_Context_Array;
	}
}

inline size_t json_length(Json_Writer *writer)
{
	return writer->ptr - writer->buffer;
}

inline bool json_needs_bytes(Json_Writer *writer, size_t bytes)
{
	if ((size_t)(writer->end - writer->ptr) < bytes) {
		writer->fail = true;
		return false;
	}
	return true;
}

inline void json_indent(Json_Writer *writer, U32 indent)
{
	if (writer->end - writer->ptr < indent) {
		writer->fail = true;
		return;
	}

	char *end = writer->ptr + indent;
	for (char *ptr = writer->ptr; ptr != end; ptr++)
		*ptr = ' ';
	writer->ptr += indent;
}

inline void json_indent(Json_Writer *writer)
{
	json_indent(writer, writer->nest_depth * 2);
}

inline bool json_comma_newline(Json_Writer *writer)
{
	if (writer->end - writer->ptr < 2) {
		writer->fail = true;
		return false;
	}
	if (!writer->is_first_element) *writer->ptr++ = ',';
	*writer->ptr++ = '\n';
	json_indent(writer);
	writer->is_first_element = false;
	return true;
}

inline bool json_write_quoted_string(Json_Writer *writer, const char *data, size_t length)
{

	char *ptr = writer->ptr;
	char *end = writer->end;

	if ((size_t)(end - ptr) < length + 2)
		return false;

	// Leave space for quote or possible \-escape
	char *write_end = end - 1;

	const char *in = data;

	*ptr++ = '"';

	for (size_t i = 0; i < length; i++) {
		if (ptr >= write_end) return false;

		char c = *in++;

		switch (c) {
		case '"': *ptr++ = '\\'; *ptr++ = '"'; break;
		case '\\': *ptr++ = '\\'; *ptr++ = '\\'; break;
		case '\b': *ptr++ = '\\'; *ptr++ = '\b'; break;
		case '\f': *ptr++ = '\\'; *ptr++ = '\f'; break;
		case '\n': *ptr++ = '\\'; *ptr++ = '\n'; break;
		case '\r': *ptr++ = '\\'; *ptr++ = '\r'; break;
		case '\t': *ptr++ = '\\'; *ptr++ = '\t'; break;
		default: *ptr++ = c; break;
		}
	}

	// If the write ended with an escape sequence, the buffer may be out of space.
	if (ptr == end) return false;
	*ptr++ = '"';

	writer->ptr = ptr;
	return true;
}

inline bool json_write_raw_string(Json_Writer *writer, const char *data, size_t length)
{
	if (!json_needs_bytes(writer, length)) return false;
	memcpy(writer->ptr, data, length);
	writer->ptr += length;
	return true;
}

inline void json_key(Json_Writer *writer, String key)
{
	assert(writer->context == Json_Context_Object);
	assert(!writer->has_key);

	if (!json_comma_newline(writer))
		return;

	if (!json_write_quoted_string(writer, key.data, key.length)) {
		writer->fail = true;
		return;
	}

	if (!json_needs_bytes(writer, 2)) {
		writer->fail = true;
		return;
	}
	*writer->ptr++ = ':';
	*writer->ptr++ = ' ';

	writer->has_key = true;
}

inline void json_key(Json_Writer *writer, const char *key)
{
	json_key(writer, c_string(key));
}

inline bool json_value_prefix(Json_Writer *writer)
{
	switch (writer->context) {
	case Json_Context_Root:
		return true;

	case Json_Context_Array:
		return json_comma_newline(writer);

	case Json_Context_Object:
		assert(writer->has_key);
		writer->has_key = false;
		return true;

	default:
		writer->fail = true;
		return false;
	}

}

inline void json_string(Json_Writer *writer, String string)
{
	if (!json_value_prefix(writer))
		return;

	if (!json_needs_bytes(writer, 2 + string.length)) {
		writer->fail = true;
		return;
	}

	if (!json_write_quoted_string(writer, string.data, string.length)) {
		writer->fail = true;
		return;
	}
}

inline void json_null(Json_Writer *writer)
{
	if (!json_value_prefix(writer))
		return;

	json_write_raw_string(writer, "null", 4);
}

inline void json_bool(Json_Writer *writer, bool value)
{
	if (!json_value_prefix(writer))
		return;

	if (value) {
		json_write_raw_string(writer, "true", 4);
	} else {
		json_write_raw_string(writer, "false", 5);
	}
}

inline void json_begin_array(Json_Writer *writer)
{
	if (!json_value_prefix(writer)) return;
	if (!json_needs_bytes(writer, 1)) return;
	*writer->ptr++ = '[';

	json_push_context(writer, Json_Context_Array);
	writer->is_first_element = true;
}

inline void json_end_array(Json_Writer *writer)
{
	json_pop_context(writer);
	if (!writer->is_first_element) {
		if (!json_needs_bytes(writer, 1)) return;
		*writer->ptr++ = '\n';
		json_indent(writer);
	}
	if (!json_needs_bytes(writer, 1)) return;
	*writer->ptr++ = ']';
	writer->is_first_element = false;
}

inline void json_begin_object(Json_Writer *writer)
{
	if (!json_value_prefix(writer)) return;
	if (!json_needs_bytes(writer, 1)) return;
	*writer->ptr++ = '{';

	json_push_context(writer, Json_Context_Object);
	writer->is_first_element = true;
	writer->has_key = false;
}

inline void json_end_object(Json_Writer *writer)
{
	json_pop_context(writer);
	if (!writer->is_first_element) {
		if (!json_needs_bytes(writer, 1)) return;
		*writer->ptr++ = '\n';
		json_indent(writer);
	}
	if (!json_needs_bytes(writer, 1)) return;
	*writer->ptr++ = '}';
	writer->is_first_element = false;
}

inline void json_key_string(Json_Writer *writer, String key, String string)
{
	json_key(writer, key);
	json_string(writer, string);
}

inline void json_key_begin_array(Json_Writer *writer, String key)
{
	json_key(writer, key);
	json_begin_array(writer);
}

inline void json_key_begin_object(Json_Writer *writer, String key)
{
	json_key(writer, key);
	json_begin_object(writer);
}


inline void json_key_string(Json_Writer *writer, const char *key, String string)
{
	json_key(writer, key);
	json_string(writer, string);
}

inline void json_key_begin_array(Json_Writer *writer, const char *key)
{
	json_key(writer, key);
	json_begin_array(writer);
}

inline void json_key_begin_object(Json_Writer *writer, const char *key)
{
	json_key(writer, key);
	json_begin_object(writer);
}

inline void json_key_null(Json_Writer *writer, const char *key)
{
	json_key(writer, key);
	json_null(writer);
}

inline void json_key_bool(Json_Writer *writer, const char *key, bool value)
{
	json_key(writer, key);
	json_bool(writer, value);
}

