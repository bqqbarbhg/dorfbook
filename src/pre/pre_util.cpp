
FILE *pre_out;

void pre_create_loc_comment(Source_Loc loc)
{
	fprintf(pre_out, "// Generated from %s:%d\n", loc.file, loc.line);
}

void pre_create_array_u8(const char *name, U8 *values, U32 count, Source_Loc loc)
{
	pre_create_loc_comment(loc);

	fprintf(pre_out, "const uint8_t %s[%u] = {\n", name, count);
	U32 index = 0;
	while (index < count) {
		fprintf(pre_out, "\t");
		for (uint32_t i = index; i < min(index + 16, count); i++) {
			fprintf(pre_out, "0x%02x, ", values[i]);
		}
		index += 16;
		fprintf(pre_out, "\n");
	}
	fprintf(pre_out, "};\n\n");
}

void pre_create_array_u32(const char *name, U32 *values, U32 count, Source_Loc loc)
{
	pre_create_loc_comment(loc);

	fprintf(pre_out, "const uint32_t %s[%u] = {\n", name, count);
	U32 index = 0;
	while (index < count) {
		fprintf(pre_out, "\t");
		for (uint32_t i = index; i < min(index + 8, count); i++) {
			fprintf(pre_out, "0x%08x, ", values[i]);
		}
		index += 8;
		fprintf(pre_out, "\n");
	}
	fprintf(pre_out, "};\n\n");
}

