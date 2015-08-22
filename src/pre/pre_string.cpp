
void make_all_chars_table()
{
	U8 table[256];

	for (int i = 0; i < 256; i++) {
		table[i] = (U8)i;
	}

	pre_create_array_u8("all_chars_table", table, Count(table), SOURCE_LOC);
}

void make_char_to_digit_table()
{
	U8 table[256];

	memset(table, 0xFF, sizeof(table));
	for (char c = '0'; c <= '9'; c++) {
		table[c] = c - '0';
	}
	for (char c = 'A'; c <= 'Z'; c++) {
		table[c] = 10 + (c - 'A');
	}
	for (char c = 'a'; c <= 'z'; c++) {
		table[c] = 10 + (c - 'a');
	}

	pre_create_array_u8("char_to_digit_table", table, Count(table), SOURCE_LOC);
}

