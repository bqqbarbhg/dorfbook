typedef U32 Tag_Id;

struct Tag_List
{
	Tag_Id *tags;
	U32 count;
};

struct Tags_State
{
	Tag_List included;
	Tag_List excluded;
};

struct Bind
{
	Interned_String name;
};

struct Rel_State
{
	U32 first_bind_id;
	U32 second_bind_id;

	Tags_State tags;
};

struct Rule_State
{
	Tags_State *bind_states;

	Rel_State *rel_states;
	U32 rel_state_count;
};

struct Tag_Info
{
	Interned_String name;
};

struct Rule
{
	String name;
	String description;

	Bind *binds;
	U32 bind_count;

	Rule_State pre_state;
	Rule_State post_state;
};

struct Sim_Info
{
	Push_Allocator allocator;
	String_Table string_table;

	Tag_Info *tag_infos;
	U32 tag_info_count;

	Rule *rules;
	U32 rule_count;
};

void sim_info_free(Sim_Info *sim_info)
{
	push_allocator_free(&sim_info->allocator);
	string_table_free(&sim_info->string_table);
}

Tag_Id find_tag(Sim_Info *info, String str)
{
	Interned_String interned;
	if (!intern_if_not_new(&interned, &info->string_table, str)) return 0;
	U32 count = info->tag_info_count;
	for (U32 i = 1; i < count; i++) {
		if (info->tag_infos[i].name == interned)
			return i;
	}
	return 0;
}

Tag_Id find_tag(Sim_Info *info, const char *str)
{
	return find_tag(info, c_string(str));
}

