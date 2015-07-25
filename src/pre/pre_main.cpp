
int main(int argc, char** argv)
{
	if (argc < 2)
		return 1;

	pre_out = fopen(argv[1], "w");

	make_crc32_table();

	fclose(pre_out);
	return 0;
}

