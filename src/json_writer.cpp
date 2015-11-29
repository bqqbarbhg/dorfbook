
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

inline bool json_comma(Json_Writer *writer)
{
	if (writer->end - writer->ptr < 1) {
		writer->fail = true;
		return false;
	}
	if (!writer->is_first_element) *writer->ptr++ = ',';
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

	if (!json_comma(writer))
		return;

	if (!json_write_quoted_string(writer, key.data, key.length)) {
		writer->fail = true;
		return;
	}

	if (!json_needs_bytes(writer, 1)) {
		writer->fail = true;
		return;
	}
	*writer->ptr++ = ':';

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
		return json_comma(writer);

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

struct Json_Format_Settings {
	int indent_amount;
	char indent_char;

	String newline;

	U32 max_inline_no_wrap;
};

struct Json_Formatter {
	char *out_buffer;
	char *out;
	char *out_end;

	int indent;

	Json_Format_Settings settings;
};

const Json_Format_Settings json_default_format_settings = {
	// Indent using 2 spaces
	2, ' ',

	c_string("\n"),

	// Wrap at 40 characters
	40,
};

const char *format_json_skip(const char *pos, const char *end)
{
	if (pos == end)
		return 0;

	switch (*pos) {
	case '"':
		// String literal: skip to matching "

		pos++; // Skip past "

		for (; pos < end; pos++) {
			switch (*pos) {
			case '\\':
				// Escape sequence: the end of the string literal cannot follow an escape sequence.
				// The escape sequences are either simple 2-character sequences such as \n or \" or
				// 6-character unicode sequences such as \u1234. Since the unicode escape alphabet
				// is very limited the two first characters of the escape can be skipped continuing
				// normally without getting out of sync.
				pos++;
				continue;

			case '"':
				// Found end
				return pos + 1;
			}
		}
		return 0;

	case '[':
		// Array: skip elements until ]

		pos++; // Skip past [

		if (pos < end && *pos == ']')
			return pos + 1;

		while (pos < end) {
			// Skip value
			pos = format_json_skip(pos, end);
			if (!pos || pos >= end) return 0;

			switch (*pos) {
			case ',':
				// Has next value
				pos++; // Skip past ,
				break;

			case ']':
			// Found end
				return pos + 1;

			default:
				assert(0 && "Unexpected character in JSON");
				break;
			}
		}
		return 0;

	case '{':
		// Object: skip key-value pairs until }

		pos++; // Skip past {

		if (pos < end && *pos == '}')
			return pos + 1;

		while (pos < end) {

			const char *old_pos = pos;

			// Skip key
			pos = format_json_skip(pos, end);
			if (!pos || pos >= end) return 0;

			assert(*pos == ':');
			pos++;

			// Skip value
			pos = format_json_skip(pos, end);
			if (!pos || pos >= end) return 0;

			switch (*pos) {
			case ',':
				// Has next value
				pos++; // Skip past ,
				break;

			case '}':
				// Found end
				return pos + 1;

			default:
				assert(0 && "Unexpected character in JSON");
				break;
			}
		}
		return 0;

	default:
		// Unknown, skip to next terminator character
		for (; pos < end; pos++) {
			switch (*pos) {
			case ',': case ':': case ']': case '}':
				// Terminator character, return there
				return pos;
			}
		}
		return pos;
	}
}

bool format_json_output(Json_Formatter *formatter, const char *data, size_t length)
{
	if ((size_t)(formatter->out_end - formatter->out) < length)
		return false;

	memcpy(formatter->out, data, length);
	formatter->out += length;
	return true;
}

bool format_json_output(Json_Formatter *formatter, char c)
{
	if (formatter->out_end - formatter->out < 1)
		return false;

	*formatter->out++ = c;
	return true;
}

bool format_json_output_newline(Json_Formatter *formatter)
{
	return format_json_output(formatter,
			formatter->settings.newline.data,
			formatter->settings.newline.length);
}

bool format_json_indent(Json_Formatter *formatter)
{
	int amount = formatter->settings.indent_amount * formatter->indent;
	if (formatter->out_end - formatter->out < amount) return false;

	char c = formatter->settings.indent_char;
	char *out = formatter->out;
	for (int i = 0; i < amount; i++) {
		*out++ = c;
	}
	formatter->out = out;
	return true;
}

const char* format_json(Json_Formatter *formatter, const char *in, const char *in_end)
{
	if (in == in_end) return 0;

	char c = *in;

	if (c == '{') {
		format_json_output(formatter, '{');
		formatter->indent++;

		const char *end = format_json_skip(in, in_end);
		if (!end) return 0;
		in++;

		if (in < in_end && *in == '}') {
			formatter->indent--;
			format_json_output(formatter, '}');
			return in + 1;
		}

		bool is_inline = end - in < formatter->settings.max_inline_no_wrap;
		if (is_inline) {
			format_json_output(formatter, ' ');
		} else {
			format_json_output_newline(formatter);
			format_json_indent(formatter);
		}

		while (in < in_end) {

			const char *key_start = in;
			const char *key_end = format_json_skip(key_start, in_end);
			if (!key_end) return 0;

			assert(key_end < in_end && *key_end == ':');

			format_json_output(formatter, key_start, key_end - key_start);
			format_json_output(formatter, ": ", 2);

			in = format_json(formatter, key_end + 1, in_end);
			if (!in || in == in_end) return 0;

			switch (*in) {
			case ',':
				in++;
				format_json_output(formatter, ',');
				if (is_inline) {
					format_json_output(formatter, ' ');
				} else {
					format_json_output_newline(formatter);
					format_json_indent(formatter);
				}
				break;

			case '}':
				formatter->indent--;
				if (is_inline) {
					format_json_output(formatter, " }", 2);
				} else {
					format_json_output_newline(formatter);
					format_json_indent(formatter);
					format_json_output(formatter, '}');
				}
				return in + 1;

			default:
				assert(0 && "Unknown character in JSON");
				return 0;
			}

		}

		return 0;
	} else if (c == '[') {

		format_json_output(formatter, '[');
		formatter->indent++;

		const char *end = format_json_skip(in, in_end);
		if (!end) return 0;
		in++;

		if (in < in_end && *in == ']') {
			formatter->indent--;
			format_json_output(formatter, ']');
			return in + 1;
		}

		bool is_inline = end - in < formatter->settings.max_inline_no_wrap;
		if (is_inline) {
			// No space between [ and value
		} else {
			format_json_output_newline(formatter);
			format_json_indent(formatter);
		}

		while (in < in_end) {
			in = format_json(formatter, in, in_end);
			if (!in || in == in_end) return 0;

			switch (*in) {
			case ',':
				in++;
				format_json_output(formatter, ',');
				if (is_inline) {
					format_json_output(formatter, ' ');
				} else {
					format_json_output_newline(formatter);
					format_json_indent(formatter);
				}
				break;

			case ']':
				formatter->indent--;
				if (is_inline) {
					format_json_output(formatter, ']');
				} else {
					format_json_output_newline(formatter);
					format_json_indent(formatter);
					format_json_output(formatter, ']');
				}
				return in + 1;

			default:
				assert(0 && "Unknown character in JSON");
				return 0;
			}
		}

		return 0;
	} else {

		const char *end = format_json_skip(in, in_end);
		if (!end) return 0;

		format_json_output(formatter, in, end - in);
		return end;
	}
}

size_t format_json(char *output, size_t output_size,
		const char *input, size_t input_size, const Json_Format_Settings *settings)
{
	Json_Formatter formatter;
	formatter.out_buffer = output;
	formatter.out = output;
	formatter.out_end = output + output_size;
	formatter.indent = 0;
	formatter.settings = *settings;

	if (!format_json(&formatter, input, input + input_size))
		return 0;

	return formatter.out - formatter.out_buffer;
}

size_t format_json(char *output, size_t output_size, const char *input, size_t input_size)
{
	return format_json(output, output_size, input, input_size, &json_default_format_settings);
}

