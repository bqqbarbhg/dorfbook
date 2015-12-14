
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

Bind *rule_find_bind(Rule *rule, Interned_String bind_id)
{
	for (U32 i = 0; i < rule->bind_count; i++) {
		Bind *bind = rule->binds + i;
		if (bind->name == bind_id)
			return bind;
	}
	return 0;
}

struct Rule_Token
{
	char prefix;
	String identifier;
};


bool parse_rule_token(Rule_Token *token, Scanner *s)
{
	const char *pos = s->pos;
	const char *end = s->end;

	if (pos == end)
		return false;

	char prefix = *pos;
	if (prefix == '+' || prefix == '-') {
		pos++;
		token->prefix = prefix;
	} else {
		token->prefix = '\0';
	}

	const char *start = pos;

	for (; pos != end; pos++) {
		char c = *pos;
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '_'))
			break;
	}

	String identifier = to_string(start, pos);
	if (identifier.length == 0)
		return false;

	s->pos = pos;
	token->identifier = identifier;
	return true;
}

void finalize_tags_state(Sim_Info *sim_info, Tags_State *state)
{
	state->included.count = pl_count(state->included.tags);
	state->included.tags = PUSH_COPY_N(&sim_info->allocator, Tag_Id,
			state->included.count, state->included.tags);

	state->excluded.count = pl_count(state->excluded.tags);
	state->excluded.tags = PUSH_COPY_N(&sim_info->allocator, Tag_Id,
			state->excluded.count, state->excluded.tags);
}

Bind *find_or_create_bind(Push_Allocator *ta, Rule *rule, Interned_String name)
{
	Bind *old_bind = rule_find_bind(rule, name);
	if (old_bind) return old_bind;

	Bind zero_bind = { 0 };
	Bind *bind = pl_push_copy(ta, rule->binds, zero_bind);
	bind->name = name;

	Tags_State zero_state = { 0 };
	pl_push_copy(ta, rule->pre_state.bind_states, zero_state);
	pl_push_copy(ta, rule->post_state.bind_states, zero_state);

	rule->bind_count++;

	return bind;
}

Tag_Id find_or_create_tag(Push_Allocator *ta, Tag_Info **tag_infos, Interned_String name)
{
	Tag_Info *infos = *tag_infos;

	U32 tag_info_count = pl_count(infos);
	for (U32 i = 1; i < tag_info_count; i++) {
		if (infos[i].name == name) {
			return i;
		}
	}

	Tag_Info *new_tag = pl_push(ta, infos);
	new_tag->name = name;

	*tag_infos = infos;
	return tag_info_count;
}

bool parse_rules_inner(Sim_Info *sim_info, Push_Allocator *ta, Scanner *s)
{
	String_Table *string_table = &sim_info->string_table;
	Push_Allocator *allocator = &sim_info->allocator;

	Tag_Info *tag_infos = 0;

	Rule *rules = 0;
	Rule *rule = 0;

	Tag_Info *null_tag = pl_push(ta, tag_infos);
	null_tag->name = intern(string_table, c_string("(null)"));

	bool in_post_rule_section = false;

	Scanner line;
	while (scanner_accept_line(&line, s)) {
		if (accept(&line, c_string("###"))) {
			Rule empty_rule = { 0 };
			rule = pl_push_copy(ta, rules, empty_rule);

			in_post_rule_section = false;

			accept_whitespace(&line);
			rule->name = PUSH_COPY_STR(allocator, to_string(line));

		} else if (accept(&line, '>')) {
			if (!rule) return false;
			accept_whitespace(&line);
			rule->description = PUSH_COPY_STR(allocator, to_string(line));

		} else if (accept(&line, c_string("    ")) || accept(&line, '\t')) {
			if (!rule) return false;
			accept_whitespace(&line);

			if (accept(&line, c_string("->"))) {
				in_post_rule_section = true;
				continue;
			}

			Rule_Token rule_tokens[64];
			U32 rule_token_count = 0;

			while (!scanner_end(&line)) {
				if (!parse_rule_token(rule_tokens + rule_token_count, &line)) return false;
				if (++rule_token_count == Count(rule_tokens)) return false;
				accept_whitespace(&line);
			}

			if (rule_token_count < 2)
				return false;

			if (rule_tokens[0].prefix)
				return false;

			Rule_State *current_state = in_post_rule_section ? &rule->post_state : &rule->pre_state;

			if (!rule_tokens[rule_token_count - 1].prefix) {
				// Relation

				if (rule_token_count < 3)
					return false;

				Rel_State zero_rel = { 0 };

				Interned_String first_name = intern(string_table, rule_tokens[0].identifier);
				Bind *first = find_or_create_bind(ta, rule, first_name);
				U32 first_index = (U32)(first - rule->binds);

				Interned_String second_name = intern(string_table, rule_tokens[rule_token_count - 1].identifier);
				Bind *second = find_or_create_bind(ta, rule, second_name);
				U32 second_index = (U32)(second - rule->binds);

				Rel_State *rel_state = 0;

				U32 rel_state_count = current_state->rel_state_count;
				for (U32 rel_index = 0; rel_index < rel_state_count; rel_index++) {
					Rel_State *rel = current_state->rel_states + rel_index;
					if (rel->first_bind_id == first_index && rel->second_bind_id == second_index) {
						rel_state = rel;
						break;
					}
				}

				if (!rel_state) {
					Rel_State rel = { 0 };
					rel.first_bind_id = first_index;
					rel.second_bind_id = second_index;

					rel_state = pl_push_copy(ta, current_state->rel_states, rel);
					current_state->rel_state_count++;
				}

				for (U32 token_index = 1; token_index < rule_token_count - 1; token_index++) {
					Rule_Token *token = rule_tokens + token_index;

					Interned_String tag_name = intern(string_table, token->identifier);
					Tag_Id tag_id = find_or_create_tag(ta, &tag_infos, tag_name);

					switch (token->prefix) {
					case '+': pl_push_copy(ta, rel_state->tags.included.tags, tag_id); break;
					case '-': pl_push_copy(ta, rel_state->tags.excluded.tags, tag_id); break;
					default: return false;
					}
				}

			} else {
				// Just tags

				Interned_String bind_name = intern(string_table, rule_tokens[0].identifier);
				Bind *bind = find_or_create_bind(ta, rule, bind_name);
				size_t bind_index = bind - rule->binds;
				Tags_State *tags = current_state->bind_states + bind_index;

				for (U32 token_index = 1; token_index < rule_token_count; token_index++) {
					Rule_Token *token = rule_tokens + token_index;

					Interned_String tag_name = intern(string_table, token->identifier);
					Tag_Id tag_id = find_or_create_tag(ta, &tag_infos, tag_name);

					switch (token->prefix) {
					case '+': pl_push_copy(ta, tags->included.tags, tag_id); break;
					case '-': pl_push_copy(ta, tags->excluded.tags, tag_id); break;
					default: return false;
					}
				}
			}
		}
	}

	U32 rule_count = pl_count(rules);

	for (U32 rule_index = 0; rule_index < rule_count; rule_index++) {
		Rule *rule = rules + rule_index;

		U32 bind_count = pl_count(rule->binds);
		rule->bind_count = bind_count;

		Rule_State *states[] = { &rule->pre_state, &rule->post_state };
		for (U32 state_index = 0; state_index < 2; state_index++) {
			Rule_State *state = states[state_index];

			for (U32 bind_index = 0; bind_index < bind_count; bind_index++) {
				finalize_tags_state(sim_info, &state->bind_states[bind_index]);
			}

			state->bind_states = PUSH_COPY_N(allocator, Tags_State, bind_count, state->bind_states);

			U32 rel_state_count = pl_count(state->rel_states);
			for (U32 rel_index = 0; rel_index < rel_state_count; rel_index++) {
				finalize_tags_state(sim_info, &state->rel_states[rel_index].tags);
			}

			state->rel_state_count = rel_state_count;
			state->rel_states = PUSH_COPY_N(allocator, Rel_State, state->rel_state_count, state->rel_states);
		}

		rule->binds = PUSH_COPY_N(allocator, Bind, bind_count, rule->binds);
		rule->bind_count = bind_count;
	}

	sim_info->rules = PUSH_COPY_N(allocator, Rule, rule_count, rules);
	sim_info->rule_count = rule_count;

	U32 tag_info_count = pl_count(tag_infos);
	sim_info->tag_infos = PUSH_COPY_N(allocator, Tag_Info, tag_info_count, tag_infos);
	sim_info->tag_info_count = tag_info_count;

	return true;
}

bool parse_rules(Sim_Info *sim_info, Scanner *s)
{
	Push_Allocator temp_allocator = { 0 };
	bool ret = parse_rules_inner(sim_info, &temp_allocator, s);
	push_allocator_free(&temp_allocator);

	if (!ret) {
		sim_info_free(sim_info);
	}

	return ret;
}

