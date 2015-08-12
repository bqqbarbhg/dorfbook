
char *combine_path(const char *root, const char *path)
{
	size_t root_len = strlen(root);
	size_t path_len = strlen(path);

	char *result_buffer = (char*)malloc(root_len + path_len + 2);
	char *result_ptr = result_buffer;

	memcpy(result_ptr, root, root_len);
	result_ptr += root_len;

	if (path_len != 0 && result_ptr[-1] == '/') {
		*result_ptr++ = '/';
	}

	memcpy(result_ptr, path, path_len);
	result_ptr += path_len;

	*result_ptr = '\0';

	return result_buffer;
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return 1;

	char *path = combine_path(argv[1], "pre_output.cpp");
	pre_out = fopen(path, "w");

	make_crc32_table();
	make_all_chars_table();

	fclose(pre_out);
	free(path);

	return 0;
}

