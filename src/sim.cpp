
typedef U32 Tag_Id;

struct Tag_Query
{
	Tag_Id *required_tags;
	Tag_Id *prohibited_tags;

	U32 required_tag_count;
	U32 prohibited_tag_count;
};

struct Bind
{
	Interned_String name;

	Tag_Query pre;
	Tag_Query post;
};

struct Rule
{
	String name;
	String description;

	Bind *binds;
	U32 bind_count;
};

struct Rule_Token
{
	char prefix;
	String identifier;
};

struct Tag_Info
{
	Interned_String name;
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
				return true;

			if (rule_tokens[0].prefix)
				return false;

			Interned_String bind_id = intern(string_table, rule_tokens[0].identifier);

			if (!rule_tokens[rule_token_count - 1].prefix) {
				// Relation
			} else {
				// Just tags

				Bind *bind = rule_find_bind(rule, bind_id);
				if (!bind) {
					Bind zero_bind = { 0 };
					bind = pl_push_copy(ta, rule->binds, zero_bind);
					bind->name = bind_id;
					rule->bind_count++;
				}

				for (U32 token_index = 1; token_index < rule_token_count; token_index++) {
					Rule_Token *token = rule_tokens + token_index;

					Interned_String tag_name = intern(string_table, token->identifier);

					Tag_Id tag_id = 0;

					U32 tag_info_count = pl_count(tag_infos);
					for (U32 i = 1; i < tag_info_count; i++) {
						if (tag_infos[i].name == tag_name) {
							tag_id = i;
							break;
						}
					}

					if (tag_id == 0) {
						Tag_Info *new_tag = pl_push(ta, tag_infos);
						new_tag->name = tag_name;
						tag_id = tag_info_count;
					}

					Tag_Query *query = in_post_rule_section ? &bind->post : &bind->pre;

					switch (token->prefix) {
					case '+': pl_push_copy(ta, query->required_tags, tag_id); break;
					case '-': pl_push_copy(ta, query->prohibited_tags, tag_id); break;
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
		for (U32 bind_index = 0; bind_index < bind_count; bind_index++) {
			Bind *bind = rule->binds + bind_index;

			Tag_Query *queries[] = { &bind->pre, &bind->post };
			for (U32 query_index = 0; query_index < Count(queries); query_index++) {
				Tag_Query *query = queries[query_index];

				query->required_tag_count = pl_count(query->required_tags);
				query->required_tags = PUSH_COPY_N(allocator, Tag_Id, query->required_tag_count, query->required_tags);

				query->prohibited_tag_count = pl_count(query->prohibited_tags);
				query->prohibited_tags = PUSH_COPY_N(allocator, Tag_Id, query->prohibited_tag_count, query->prohibited_tags);
			}
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

