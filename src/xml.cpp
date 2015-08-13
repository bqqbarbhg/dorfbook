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

inline bool accept_xml_whitespace(Scanner *s)
{
	for (;;) {
		accept_whitespace(s);
		if (!accept(s, to_string("<!--", 4)))
			break;
		if (!skip_accept(s, to_string("-->", 3)))
			return false;
	}

	return true;
}

bool xml_get_entity(String *value, XML *xml, Interned_String key)
{
	XML_Entity_List entities = xml->entities;
	for (size_t i = 0; i < entities.count; i++) {
		// @Cleanup: This has lots of nested members
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

	while (!scanner_end(s)) {

		Scanner es = *s;
		if (accept(&es, suffix_start)) {
			if (accept(&es, substring(suffix, 1))) {
				*text = finish_push_stream_string(&stream);
				return true;
			}
		} else if (accept(s, '&')) {

			if (accept(s, '#')) {
				int base = 10;
				if (accept(s, 'x')) {
					base = 16;
				}
				U64 code;
				if (!accept_int(&code, s, base)) return false;
				if (!accept(s, ';')) return false;

				// @Unicode(xml)
				if (code > 0xFF) return false;

				char ch = (char)code;
				STREAM_COPY(&stream, char, &ch);
			} else {
				const char *begin = s->pos;
				if (!skip_accept(s, ';')) return false;

				Interned_String entity_key = intern(&xml->string_table, to_string(begin, s->pos));
				String entity_value;
				if (!xml_get_entity(&entity_value, xml, entity_key)) return false;
				STREAM_COPY_STR(&stream, entity_value);
			}
		} else {
			char c = next_char(s);
			STREAM_COPY(&stream, char, &c);
		}
	}

	return false;
}

bool parse_xml_attributes(XML_Attribute **attrs, U32 *attr_count, XML *xml, Scanner *s)
{
	Push_Stream attr_stream = start_push_stream(&xml->attribute_alloc);

	String attr_name;
	while (accept_xml_name(&attr_name, s)) {
		if (!accept_xml_whitespace(s)) return false;
		if (!accept(s, '=')) return false;
		if (!accept_xml_whitespace(s)) return false;
		char quote = accept_any(s, "\"'", 2);
		if (!quote) return false;

		String text;
		if (!xml_text_until(&text, xml, s, char_string(quote)))
			return false;

		// xml_text_until already matched the ending quote
		scanner_skip(s, 1);

		if (!accept_xml_whitespace(s)) return false;

		XML_Attribute *attr = STREAM_ALLOC(&attr_stream, XML_Attribute);
		attr->key = intern(&xml->string_table, attr_name);
		attr->value = text;
	}

	// @Cleanup: This is too low-level code for here
	Data_Slice attr_data = finish_push_stream(&attr_stream);
	*attrs = (XML_Attribute*)attr_data.data;
	*attr_count = (U32)(attr_data.length / sizeof(XML_Attribute));

	return true;
}

bool parse_xml(XML *xml, const char *data, size_t length)
{
	Scanner scanner;
	scanner.pos = data;
	scanner.end = data + length;
	Scanner *s = &scanner;

	const XML_Entity default_entities[] = {
		{ intern(&xml->string_table, c_string("lt")), char_string('<') },
		{ intern(&xml->string_table, c_string("gt")), char_string('>') },
		{ intern(&xml->string_table, c_string("amp")), char_string('&') },
		{ intern(&xml->string_table, c_string("apos")), char_string('\'') },
		{ intern(&xml->string_table, c_string("quot")), char_string('"') },
	};

	list_push(&xml->entities, default_entities, Count(default_entities));

	XML_Node *prev = 0;
	XML_Node *parent = 0;

	while (!scanner_end(s)) {
		if (!accept_xml_whitespace(s)) return false;

		if (scanner_end(s))
			break;

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

				if (!accept_xml_whitespace(s)) return false;

				XML_Node *new_node = PUSH_ALLOC(&xml->node_alloc, XML_Node);
				new_node->tag = intern(&xml->string_table, tag_name);

				XML_Attribute *attrs;
				U32 attr_count;

				if (!parse_xml_attributes(&attrs, &attr_count, xml, s)) return false;

				new_node->attributes = attrs;
				new_node->attribute_count = attr_count;

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

