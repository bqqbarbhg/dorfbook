struct Scanner
{
	const char *pos;
	const char *end;
};

bool is_whitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

inline bool scanner_skip(Scanner *s, size_t amount)
{
	if ((size_t)(s->end - s->pos) < amount)
		return false;
	s->pos += amount;
	return true;
}

inline bool skip_accept(Scanner *s, char c)
{
	const char *pos = s->pos;
	const char *end = s->end;
	for (; pos != end; pos++) {
		if (*pos == c) {
			s->pos = pos + 1;
			return true;
		}
	}
	return false;
}

char next_char(Scanner *s)
{
	assert(s->pos != s->end);
	return *s->pos++;
}

inline bool scanner_end(Scanner *s)
{
	return s->pos == s->end;
}

inline bool accept(Scanner *s, char c)
{
	if (s->pos != s->end && *s->pos == c) {
		s->pos++;
		return true;
	}
	return false;
}

inline bool accept(Scanner *s, String str)
{
	size_t left = s->end - s->pos;
	if (left < str.length)
		return false;
	if (memcmp(s->pos, str.data, str.length))
		return false;
	s->pos += str.length;
	return true;
}

inline char accept_any(Scanner *s, const char *chars, int char_count)
{
	for (int i = 0; i < char_count; i++) {
		if (accept(s, chars[i])) {
			return chars[i];
		}
	}
	return '\0';
}

inline bool accept_int(U64 *out_value, Scanner *s, int base)
{
	U64 value = 0;

	const char *pos = s->pos;
	const char *end = s->end;

	if (pos == end)
		return false;

	unsigned digit = char_to_digit_table[*pos];
	if (digit >= (unsigned)base)
		return false;
	value = digit;

	pos++;
	for (; pos != end; pos++) {
		digit = char_to_digit_table[*pos];
		if (digit >= (unsigned)base)
			break;
		value = value * base + digit;
	}

	s->pos = pos;
	*out_value = value;
	return true;
}

