struct String
{
	char *data;
	size_t length;
};

String to_string(char *data, size_t length)
{
	String s;
	s.data = data;
	s.length = length;
	return s;
}

