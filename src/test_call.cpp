
typedef size_t (*test_func)(char *out_buffer, const char* in_buffer, size_t length);

struct Test_Def
{
	const char *name;
	test_func func;
};

#define TEST_BUFFER_SIZE (1024*1024)

size_t test_crc32(char *out_buffer, const char* in_buffer, size_t length)
{
	uint32_t crc = crc32(in_buffer, length);
	return sprintf(out_buffer, "%u\n", crc);
}

size_t test_gzip(char *out_buffer, const char* in_buffer, size_t length)
{
	return gzip_compress(out_buffer, TEST_BUFFER_SIZE, in_buffer, length);
}

size_t test_identity(char *out_buffer, const char* in_buffer, size_t length)
{
	memcpy(out_buffer, in_buffer, length);
	return length;
}

size_t write_xml(char *buffer, XML_Node *node)
{
	char *ptr = buffer;
	ptr += sprintf(ptr, "<") + 1;
	ptr += print_string(ptr, node->tag.string) + 1;
	for (size_t i = 0; i < node->attribute_count; i++) {
		XML_Attribute *attr = &node->attributes[i];
		ptr += print_string(ptr, attr->key.string) + 1;
		ptr += print_string(ptr, attr->value) + 1;
	}
	ptr += sprintf(ptr, ">") + 1;
	if (node->text.length > 0) {
		ptr += sprintf(ptr, "#") + 1;
		ptr += print_string(ptr, node->text) + 1;
	}
	for (XML_Node *child = node->children; child; child = child->next) {
		ptr += write_xml(ptr, child);
	}
	ptr += sprintf(ptr, "/") + 1;
	return ptr - buffer;
}

size_t test_xml(char *out_buffer, const char* in_buffer, size_t length)
{
	// Copy the input to a mutable buffer for making sure that the XML doesn't
	// hold any references to the input buffer.
	char *read_buffer = M_ALLOC(char, length);
	memcpy(read_buffer, in_buffer, length);

	XML xml = { 0 };
	if (!parse_xml(&xml, read_buffer, length))
		return 0;

	// Overwrite the buffer with a detectable bit pattern to catch errors
	memset(read_buffer, 0xD0, length);

	if (!xml.root)
		return 0;
	size_t written = write_xml(out_buffer, xml.root);

	xml_free(&xml);
	M_FREE(read_buffer);

	return written;
}

size_t test_utf8_decode(char *out_buffer, const char* in_buffer, size_t length)
{
	const char *ptr = in_buffer;
	const char *end = ptr + length;
	char *out_ptr = out_buffer;
	while (ptr != end) {
		Utf_Result result = utf8_decode(ptr, end - ptr);
		if (!result.end) {
			return 0;
		}
		ptr = result.end;
		out_ptr += sprintf(out_ptr, "%u ", result.codepoint);
	}
	return out_ptr - out_buffer;
}

size_t test_utf8_encode(char *out_buffer, const char* in_buffer, size_t length)
{
	Scanner s;
	s.pos = in_buffer;
	s.end = in_buffer + length;

	char *out_ptr = out_buffer;
	char *out_end = out_ptr + TEST_BUFFER_SIZE;

	U64 value;
	while (accept_int(&value, &s, 10)) {
		accept_whitespace(&s);

		out_ptr += utf8_encode(out_ptr, out_end - out_ptr, (U32)value);
	}

	return out_ptr - out_buffer;
}

void json_tags_state(Json_Writer *j, Sim_Info *sim_info, Tags_State *state)
{
	json_begin_object(j);

	json_key_begin_array(j, "included");
	for (U32 i = 0; i < state->included.count; i++) {
		Tag_Info *tag = &sim_info->tag_infos[state->included.tags[i]];
		json_string(j, tag->name.string);
	}
	json_end_array(j);

	json_key_begin_array(j, "excluded");
	for (U32 i = 0; i < state->excluded.count; i++) {
		Tag_Info *tag = &sim_info->tag_infos[state->excluded.tags[i]];
		json_string(j, tag->name.string);
	}
	json_end_array(j);

	json_end_object(j);
}

void json_test_sim_info(Json_Writer *j, Sim_Info *sim_info)
{
	json_begin_object(j);

	json_key_begin_array(j, "rules");
	for (U32 rule_index = 0; rule_index < sim_info->rule_count; rule_index++) {
		Rule *rule = sim_info->rules + rule_index;

		json_begin_object(j);

		json_key_string(j, "title", rule->name);
		json_key_string(j, "description", rule->description);

		json_key_begin_object(j, "binds");

		for (U32 bind_index = 0; bind_index < rule->bind_count; bind_index++) {
			Bind *bind = rule->binds + bind_index;

			json_key_begin_object(j, bind->name.string);

			json_key(j, "pre");
			json_tags_state(j, sim_info, &rule->pre_state.bind_states[bind_index]);

			json_key(j, "post");
			json_tags_state(j, sim_info, &rule->post_state.bind_states[bind_index]);

			json_end_object(j);
		}

		json_end_object(j);

		json_key_begin_object(j, "rels");

		String rel_keys[] = { c_string("pre"), c_string("post") };
		Rule_State *states[] = { &rule->pre_state, &rule->post_state };

		for (U32 state_index = 0; state_index < 2; state_index++) {
			Rule_State *state = states[state_index];

			json_key_begin_array(j, rel_keys[state_index]);

			for (U32 rel_index = 0; rel_index < state->rel_state_count; rel_index++) {
				Rel_State *rel = state->rel_states + rel_index;

				json_begin_object(j);

				json_key_begin_array(j, "binds");
				json_string(j, rule->binds[rel->first_bind_id].name.string);
				json_string(j, rule->binds[rel->second_bind_id].name.string);
				json_end_array(j);

				json_key(j, "tags");
				json_tags_state(j, sim_info, &rel->tags);

				json_end_object(j);
			}

			json_end_array(j);
		}

		json_end_object(j);

		json_end_object(j);
	}
	json_end_array(j);

	json_end_object(j);
}

size_t test_sim_parse(char *out_buffer, const char* in_buffer, size_t length)
{
	Scanner s;
	s.pos = in_buffer;
	s.end = in_buffer + length;

	Sim_Info sim_info = { 0 };
	bool success = parse_rules(&sim_info, &s);

	char *json_buffer = M_ALLOC(char, TEST_BUFFER_SIZE);

	Json_Writer json;
	json_init(&json, json_buffer, TEST_BUFFER_SIZE);

	if (success) {
		json_test_sim_info(&json, &sim_info);
		sim_info_free(&sim_info);
	} else {
		json_null(&json);
	}

	size_t js_length = json_length(&json);
	size_t formatted_length = format_json(out_buffer, TEST_BUFFER_SIZE, json_buffer, js_length);

	M_FREE(json_buffer);

	return formatted_length;
}

Test_Def test_defs[] = {
	"crc32", test_crc32,
	"gzip", test_gzip,
	"identity", test_identity,
	"xml", test_xml,
	"utf8_decode", test_utf8_decode,
	"utf8_encode", test_utf8_encode,
	"sim_parse", test_sim_parse,
};

size_t test_call(const char *name, char *out_buffer,
	const char *in_buffer, size_t in_length, size_t *leak_amount)
{
	for (int i = 0; i < Count(test_defs); i++) {
		if (strcmp(test_defs[i].name, name))
			continue;

#if BUILD_DEBUG
		Debug_Alloc_Snapshot *begin = debug_thread_memory_snapshot();
#endif

		size_t size = test_defs[i].func(out_buffer, in_buffer, in_length);

#if BUILD_DEBUG
		Debug_Alloc_Snapshot *end = debug_thread_memory_snapshot();
		Debug_Alloc_Snapshot *leaked = debug_snapshot_new_allocations(begin, end);

		if (leak_amount) {
			if (leaked) {
				size_t amount = 0;
				for (Debug_Alloc_Snapshot *alloc = leaked; alloc; alloc = alloc->next) {
					amount += (size_t)alloc->header.size;
				}
				*leak_amount = amount;
			} else {
				*leak_amount = 0;
			}
		}

		debug_alloc_snapshot_free(begin);
		debug_alloc_snapshot_free(end);
		debug_alloc_snapshot_free(leaked);
#endif

		return size;
	}
	return 0;
}

