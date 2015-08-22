
typedef size_t (*test_func)(char *out_buffer, const char* in_buffer, size_t length);

struct Test_Def
{
	const char *name;
	test_func func;
};

#define TEST_BUFFER_SIZE (1024*1024)

size_t test_crc32(char *out_buffer, const char* in_buffer, size_t length)
{
	uint32_t crc = crc32(in_buffer, length);
	return sprintf(out_buffer, "%u\n", crc);
}

size_t test_gzip(char *out_buffer, const char* in_buffer, size_t length)
{
	return gzip_compress(out_buffer, TEST_BUFFER_SIZE, in_buffer, length);
}

size_t test_identity(char *out_buffer, const char* in_buffer, size_t length)
{
	memcpy(out_buffer, in_buffer, length);
	return length;
}

size_t write_xml(char *buffer, XML_Node *node)
{
	char *ptr = buffer;
	ptr += sprintf(ptr, "<") + 1;
	ptr += print_string(ptr, node->tag.string) + 1;
	for (size_t i = 0; i < node->attribute_count; i++) {
		XML_Attribute *attr = &node->attributes[i];
		ptr += print_string(ptr, attr->key.string) + 1;
		ptr += print_string(ptr, attr->value) + 1;
	}
	ptr += sprintf(ptr, ">") + 1;
	if (node->text.length > 0) {
		ptr += sprintf(ptr, "#") + 1;
		ptr += print_string(ptr, node->text) + 1;
	}
	for (XML_Node *child = node->children; child; child = child->next) {
		ptr += write_xml(ptr, child);
	}
	ptr += sprintf(ptr, "/") + 1;
	return ptr - buffer;
}

size_t test_xml(char *out_buffer, const char* in_buffer, size_t length)
{
	// Copy the input to a mutable buffer for making sure that the XML doesn't
	// hold any references to the input buffer.
	char *read_buffer = (char*)malloc(length);
	memcpy(read_buffer, in_buffer, length);

	XML xml = { 0 };
	if (!parse_xml(&xml, read_buffer, length))
		return 0;

	// Overwrite the buffer with a detectable bit pattern to catch errors
	memset(read_buffer, 0xD0, length);

	if (!xml.root)
		return 0;
	size_t written = write_xml(out_buffer, xml.root);

	xml_free(&xml);
	free(read_buffer);

	return written;
}

size_t test_utf8_decode(char *out_buffer, const char* in_buffer, size_t length)
{
	const char *ptr = in_buffer;
	const char *end = ptr + length;
	char *out_ptr = out_buffer;
	while (ptr != end) {
		Utf_Result result = utf8_decode(ptr, end - ptr);
		if (!result.end) {
			return 0;
		}
		ptr = result.end;
		out_ptr += sprintf(out_ptr, "%u ", result.codepoint);
	}
	return out_ptr - out_buffer;
}

size_t test_utf8_encode(char *out_buffer, const char* in_buffer, size_t length)
{
	Scanner s;
	s.pos = in_buffer;
	s.end = in_buffer + length;

	char *out_ptr = out_buffer;
	char *out_end = out_ptr + TEST_BUFFER_SIZE;

	U64 value;
	while (accept_int(&value, &s, 10)) {
		accept_whitespace(&s);

		out_ptr += utf8_encode(out_ptr, out_end - out_ptr, (U32)value);
	}

	return out_ptr - out_buffer;
}

Test_Def test_defs[] = {
	"crc32", test_crc32,
	"gzip", test_gzip,
	"identity", test_identity,
	"xml", test_xml,
	"utf8_decode", test_utf8_decode,
	"utf8_encode", test_utf8_encode,
};

size_t test_call(const char *name, char *out_buffer,
	const char *in_buffer, size_t in_length)
{
	for (int i = 0; i < Count(test_defs); i++) {
		if (strcmp(test_defs[i].name, name))
			continue;

		size_t size = test_defs[i].func(out_buffer, in_buffer, in_length);
		return size;
	}
	return 0;
}

