
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
	String_Table string_table;

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
					if (!accept(s, '"')) return false;
					// TODO: Parse
					while (!accept(s, '"')) {
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
			// TODO: Body
			while (!accept(s, '<')) {
				if (scanner_end(s)) return false;
			}
		}
	}

	return true;
}

