
struct Source_Loc
{
	const char *file;
	U32 line;
};
Source_Loc make_source_loc(const char *file, U32 line)
{
	Source_Loc loc = { file, line };
	return loc;
}

#define SOURCE_LOC make_source_loc(__FILE__, __LINE__)

