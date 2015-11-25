
sim_parse_fixtures = [
('''
### Thing and other
> {thing} does {other}
	thing +thing -nothing
	other +othing +many -nother -woo
	->
	thing +adds -removes
	other +yes +also -no -nope
''',

{
	'rules': [{
		'title': 'Thing and other',
		'description': '{thing} does {other}',
		'binds': {
			'thing': ('thing', 'nothing', 'adds', 'removes'),
			'other': ('othing many', 'nother woo', 'yes also', 'no nope'),
		}
	}]
},

'Simple success test'
),

(
'''
### Title
> desc
	sdffd@sdf
]
''',

None,

'Simple fail'
),

(
'''
### Title
> desc
	entity +tag entity -tag
]
''',

None,

'Unprefixed tag fail'
),

(
'''
### First rule
> first desc

	first_entity +first_yes -first_no
	shared_entity +first_yes -first_no
	->
	first_entity +first_add -first_remove
	shared_entity +first_add -first_remove

### Second rule
> second desc

	second_entity +second_yes -second_no
	shared_entity +second_yes -second_no
	->
	second_entity +second_add -second_remove
	shared_entity +second_add -second_remove

### Third rule
> third desc

### Fourth rule
> fourth desc

]
''',

{
	'rules': [
	{
		'title': 'First rule',
		'description': 'first desc',
		'binds': {
			'first_entity': ('first_yes', 'first_no', 'first_add', 'first_remove'),
			'shared_entity': ('first_yes', 'first_no', 'first_add', 'first_remove'),
		}
	},
	{
		'title': 'Second rule',
		'description': 'second desc',
		'binds': {
			'second_entity': ('second_yes', 'second_no', 'second_add', 'second_remove'),
			'shared_entity': ('second_yes', 'second_no', 'second_add', 'second_remove'),
		}
	},
	{
		'title': 'Third rule',
		'description': 'third desc',
		'binds': {
		}
	},
	{
		'title': 'Fourth rule',
		'description': 'fourth desc',
		'binds': {
		}
	},
	]
},

'Multiple rules positive'
),

(
'''
### Prefix
> prefix

	good +good
	->
	good -good

### Failing
> failing

	dw@rf +dwarf

### Suffix
> suffix

	bad +bad
	->
	bad -bad

''',

None,

'Multiple rules negative'
),

(
'''
> non rule description
''',

None,

'Non-rule description negative'
),

(
'''
	non rule rules
''',

None,

'Non-rule rules negative'
),
]

for data, fixture, desc in sim_parse_fixtures:
	try:
		parsed = json.loads(test_call("sim_parse", data))
		t.check(bool(fixture) == bool(parsed), "Parses as expected", desc)

		if not fixture:
			continue

		parsed_rules = parsed['rules']

		rules = fixture['rules']
		if t.check(len(rules) == len(parsed_rules), "Parsed expected number of rules", desc):
			for rule, parsed_rule in zip(rules, parsed_rules):
				t.check(rule['title'] == parsed_rule['title'], "Parsed title", desc)
				t.check(rule['description'] == parsed_rule['description'], "Parsed description", desc)

				binds = { k: tuple(set(t.split()) for t in v) for k,v in rule['binds'].items() }

				parsed_binds = { }
				for bind_id, parsed_bind in parsed_rule['binds'].items():
					parsed_tags = tuple(set(parsed_bind[key]) for key in ('required', 'prohibited', 'adds', 'removes'))
					parsed_binds[bind_id] = parsed_tags

				t.check(parsed_binds == binds, "Parsed binds", desc)
	except StopIteration:
		t.check(False, "Got correct output", desc)

