
struct Scanner
{
	const char *pos;
	const char *end;
};

struct XML_Attribute
{
	Interned_String key;
	String value;
};

struct XML_Entity
{
	Interned_String key;
	String value;
};
LIST_STRUCT(XML_Entity);

struct XML_Node
{
	Interned_String tag;

	XML_Attribute *attributes;
	U32 attribute_count;

	XML_Node *parent;
	XML_Node *children;
	XML_Node *prev;
	XML_Node *next;

	String text;
};

struct XML
{
	Push_Allocator attribute_alloc;
	Push_Allocator node_alloc;
	Push_Allocator text_alloc;

	String_Table string_table;

	XML_Entity_List entities;

	XML_Node *root;
};

void xml_free(XML *xml)
{
	push_allocator_free(&xml->attribute_alloc);
	push_allocator_free(&xml->node_alloc);
	string_table_free(&xml->string_table);
}

inline bool xml_name_start_char(char c)
{
	return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == ':' || c == '_';
}

inline bool xml_name_char(char c)
{
	return xml_name_start_char(c) || c >= '0' && c <= '9' || c == '-' || c == '.';
}

bool accept_xml_name(String *name, Scanner *s)
{
	const char *pos = s->pos;

	const char *start = pos;
	const char *end = s->end;

	if (pos == end || !xml_name_start_char(*pos))
		return false;
	pos++;
	while (pos != end && xml_name_char(*pos))
		pos++;

	s->pos = pos;

	name->data = (char*)start;
	name->length = pos - start;

	return true;
}

bool is_whitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

inline void accept_whitespace(Scanner *s)
{
	const char *end = s->end;
	const char *pos = s->pos;

	while (pos != end && is_whitespace(*pos)) {
		*pos++;
	}

	s->pos = pos;
}

inline bool accept(Scanner *s, char c)
{
	if (s->pos != s->end && *s->pos == c) {
		s->pos++;
		return true;
	}
	return false;
}

inline bool accept(Scanner *s, String str)
{
	size_t left = s->end - s->pos;
	if (left < str.length)
		return false;
	if (memcmp(s->pos, str.data, str.length))
		return false;
	s->pos += str.length;
	return true;
}

inline char accept_any(Scanner *s, const char *chars, int char_count)
{
	for (int i = 0; i < char_count; i++) {
		if (accept(s, chars[i])) {
			return chars[i];
		}
	}
	return '\0';
}

bool xml_get_entity(String *value, XML *xml, Interned_String key)
{
	XML_Entity_List entities = xml->entities;
	for (size_t i = 0; i < entities.count; i++) {
		// TODO: This is ugly
		if (entities.data[i].key.string.data == key.string.data) {
			*value = entities.data[i].value;
			return true;
		}
	}

	return false;
}

bool xml_text_until(String *text, XML *xml, Scanner *s, String suffix)
{
	if (suffix.length == 0) return false;
	char suffix_start = suffix.data[0];

	Push_Stream stream = start_push_stream(&xml->text_alloc);

	const char *pos = s->pos;
	const char *end = s->end;

	if ((size_t)(end - pos) < suffix.length)
		return false;
	end -= suffix.length;

	for (; pos != end; pos++) {

		char c = *pos;
		if (c == suffix_start) {
			if (!memcmp(pos + 1, suffix.data + 1, suffix.length - 1)) {
				s->pos = pos;
				*text = finish_push_stream_string(&stream);
				return true;
			}
		} else if (c == '&') {
			pos++;
			const char *start = pos;
			for (; pos != end; pos++) {
				if (*pos == ';')
					break;
			}
			if (pos == end) return false;

			// TODO: &#123; &#xAB; forms

			Interned_String entity_key = intern(&xml->string_table, to_string(start, pos));
			String entity_value;
			if (!xml_get_entity(&entity_value, xml, entity_key)) return false;
			STREAM_COPY_STR(&stream, entity_value);
		} else {
			STREAM_COPY(&stream, char, &c);
		}
	}

	return false;
}

inline bool scanner_end(Scanner *s)
{
	return s->pos == s->end;
}

bool parse_xml(XML *xml, const char *data, size_t length)
{
	Scanner scanner;
	scanner.pos = data;
	scanner.end = data + length;
	Scanner *s = &scanner;

	const XML_Entity default_entities[] = {
		{ intern(&xml->string_table, c_string("lt")), c_string("<") },
		{ intern(&xml->string_table, c_string("gt")), c_string(">") },
		{ intern(&xml->string_table, c_string("amp")), c_string("&") },
		{ intern(&xml->string_table, c_string("apos")), c_string("'") },
		{ intern(&xml->string_table, c_string("quot")), c_string("\"") },
	};

	list_push(&xml->entities, default_entities, Count(default_entities));

	XML_Node *prev = 0;
	XML_Node *parent = 0;

	while (!scanner_end(s)) {
		accept_whitespace(s);
		if (accept(s, '<')) {
			if (accept(s, '/')) {
				if (!parent) {
					return false;
				}
				if (!accept(s, parent->tag.string)) return false;
				if (!accept(s, '>')) return false;
				prev = parent;
				parent = parent->parent;
			} else {
				String tag_name;
				if (!accept_xml_name(&tag_name, s)) {
					return false;
				}

				accept_whitespace(s);

				// TODO: Attributes
				String attr_name;
				while (accept_xml_name(&attr_name, s)) {
					accept_whitespace(s);
					if (!accept(s, '=')) return false;
					accept_whitespace(s);
					char quote = accept_any(s, "\"'", 2);
					if (!quote) return false;
					// TODO: Parse
					while (!accept(s, quote)) {
						if (scanner_end(s)) return false;
					}
					accept_whitespace(s);
				}

				XML_Node *new_node = PUSH_ALLOC(&xml->node_alloc, XML_Node);
				new_node->tag = intern(&xml->string_table, tag_name);
				new_node->attribute_count = 0;

				new_node->parent = parent;
				new_node->prev = prev;

				new_node->children = 0;
				new_node->next = 0;

				new_node->text = empty_string();

				if (parent && !parent->children)
					parent->children = new_node;
				if (prev) prev->next = new_node;
				if (!parent && !prev) {
					xml->root = new_node;
				}

				if (accept(s, '/')) {
					if (!accept(s, '>')) return false;
					prev = new_node;
				} else if (accept(s, '>')) {
					prev = 0;
					parent = new_node;
				}
			}
		} else {
			String text;
			if (!xml_text_until(&text, xml, s, to_string("<", 1)))
				return false;
			parent->text = text;
		}
	}

	return true;
}

