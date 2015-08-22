
void make_utf8_code_extra_table()
{
	U8 table[128];

	for (int i = 0; i < 128; i++) {
		U8 value;

		U8 ch = (U8)i | 0x80;
		if ((ch & 0xe0) == 0xc0) {
			value = 1 << 5 | (ch & 0x1f);
		} else if ((ch & 0xf0) == 0xe0) {
			value = 2 << 5 | (ch & 0x0f);
		} else if ((ch & 0xf8) == 0xf0) {
			value = 3 << 5 | (ch & 0x07);
		} else if ((ch & 0xfc) == 0xf8) {
			value = 4 << 5 | (ch & 0x03);
		} else if ((ch & 0xfe) == 0xfc) {
			value = 5 << 5 | (ch & 0x01);
		} else {
			value = 0;
		}

		table[i] = value;
	}

	pre_create_array_u8("utf8_code_extra_table", table, Count(table), SOURCE_LOC);
}

