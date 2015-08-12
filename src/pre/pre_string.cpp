
void make_all_chars_table()
{
	U8 table[256];

	for (int i = 0; i < 256; i++) {
		table[i] = (U8)i;
	}

	pre_create_array_u8("all_chars_table", table, Count(table), SOURCE_LOC);
}

