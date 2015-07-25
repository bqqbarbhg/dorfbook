
typedef U32 (*test_func)(char *out_buffer, const char* in_buffer, size_t length);

struct Test_Def
{
	const char *name;
	test_func func;
};

U32 test_crc32(char *out_buffer, const char* in_buffer, size_t length)
{
	uint32_t crc = crc32(in_buffer, length);
	return sprintf(out_buffer, "%u\n", crc);
}

Test_Def test_defs[] = {
	"crc32", test_crc32,
};

void test_call(const char *name)
{
	for (int i = 0; i < Count(test_defs); i++) {
		if (strcmp(test_defs[i].name, name))
			continue;

		char *in_buffer = (char*)malloc(1024*1024);
		char *out_buffer = (char*)malloc(1024*1024);

		size_t amount = 0;
		while (!feof(stdin)) {
			size_t num = fread(&in_buffer[amount], 1, 1024, stdin);
			amount += num;
		}
		size_t size = test_defs[i].func(out_buffer, in_buffer, amount);

		fwrite(out_buffer, 1, size, stdout);

		free(in_buffer);
		free(out_buffer);

		break;
	}
}

