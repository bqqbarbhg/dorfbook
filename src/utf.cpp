
struct Utf_Result
{
	U32 codepoint;
	const char *end;
};

enum Utf_Error
{
	Utf_No_Space,
	Utf_Invalid_Encoding,
};

Utf_Result utf_result(U32 codepoint, const char *end)
{
	Utf_Result result;
	result.codepoint = codepoint;
	result.end = end;
	return result;
}

Utf_Result utf_invalid(int error)
{
	return utf_result(0, 0);
}

inline Utf_Result utf8_decode(const char *buf, size_t bytes_left)
{
	if (!bytes_left)
		return utf_invalid(Utf_No_Space);

	// Early out
	unsigned char c = (unsigned char)*buf;
	if (c < 0x80)
		return utf_result(c, buf + 1);

	U8 code_extra = utf8_code_extra_table[c & 0x7F];

	if (!code_extra)
		return utf_invalid(Utf_Invalid_Encoding);

	U32 codepoint = code_extra & 0x1f;
	int extra_bytes = code_extra >> 5;
	if (extra_bytes + 1 > bytes_left)
		return utf_invalid(Utf_No_Space);

	const char *ptr = buf + 1;
	const char *end = ptr + extra_bytes;
	while (ptr != end) {
		unsigned char d = *ptr++;
		if ((d & 0xc0) != 0x80)
			return utf_invalid(Utf_Invalid_Encoding);
		codepoint = codepoint << 6 | d & 0x3f;
	}
	return utf_result(codepoint, end);
}

inline int utf8_encode(char *buf, size_t bytes_left, U32 codepoint)
{
	if (codepoint < 0x80) {
		if (bytes_left < 1) return 0;
		buf[0] = (U8)codepoint;
		return 1;
	} else if (codepoint < 0x800) {
		if (bytes_left < 2) return 0;
		buf[0] = (U8)(codepoint >> 6) | 0xc0;
		buf[1] = (U8)(codepoint & 0x3f) | 0x80;
		return 2;
	} else if (codepoint < 0x10000) {
		if (bytes_left < 3) return 0;
		buf[0] = (U8)(codepoint >> 12) | 0xe0;
		buf[1] = (U8)(codepoint >> 6) & 0x3f | 0x80;
		buf[2] = (U8)(codepoint & 0x3f) | 0x80;
		return 3;
	} else if (codepoint < 0x20000) {
		if (bytes_left < 4) return 0;
		buf[0] = (U8)(codepoint >> 18) | 0xf0;
		buf[1] = (U8)(codepoint >> 12) & 0x3f | 0x80;
		buf[2] = (U8)(codepoint >> 6) & 0x3f | 0x80;
		buf[3] = (U8)(codepoint & 0x3f) | 0x80;
		return 4;
	}
	return 0;
}

