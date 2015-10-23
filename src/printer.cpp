
struct Printer
{
	char *pos;
	char *end;
};

inline bool print(Printer *p, char c)
{
	if (p->pos == p->end)
		return false;
	*p->pos++ = c;
	return true;
}

inline bool print(Printer *p, String str)
{
	size_t left = p->end - p->pos;
	if (left < str.length)
		return false;
	memcpy(p->pos, str.data, str.length);
	p->pos += str.length;
	return true;
}

inline bool print(Printer *p, const char* str)
{
	return print(p, c_string(str));
}

inline bool print(Printer *p, Interned_String str)
{
	return print(p, str.string);
}

