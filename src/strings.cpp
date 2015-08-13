struct String
{
	char *data;
	size_t length;
};

inline String empty_string()
{
	String s = { 0 };
	return s;
}

inline String to_string(const char *data, size_t length)
{
	String s;
	s.data = (char*)data;
	s.length = length;
	return s;
}

inline String to_string(const char *begin, const char *end)
{
	return to_string((char*)begin, (char*)end - (char*)begin);
}

inline String c_string(const char *str)
{
	return to_string((char*)str, strlen(str));
}

inline String char_string(char c)
{
	return to_string((char*)&all_chars_table[(U8)c], 1);
}

inline String substring(String s, size_t begin)
{
	return to_string(s.data + begin, s.length - begin);
}

inline String substring(String s, size_t begin, size_t length)
{
	return to_string(s.data + begin, length);
}

