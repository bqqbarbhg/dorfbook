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

