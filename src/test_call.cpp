
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
	return gzip_no_compress(out_buffer, TEST_BUFFER_SIZE, in_buffer, length);
}

Test_Def test_defs[] = {
	"crc32", test_crc32,
	"gzip", test_gzip,
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

