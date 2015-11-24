
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
		parsed = test_call("sim_parse", data)
		lines = iter(parsed.splitlines())

		fail_extra = "Should succeed" if bool(data) else "Should fault"
		t.check(bool(fixture) == bool(int(next(lines))), "Parses as expected", desc)

		if not fixture:
			continue

		rules = fixture['rules']
		if t.check(len(rules) == int(next(lines)), "Parsed expected number of rules", desc):
			for rule in rules:
				t.check(rule['title'] == next(lines), "Parsed title", desc)
				t.check(rule['description'] == next(lines), "Parsed description", desc)
				bind_count = int(next(lines))
				parsed_binds = { }
				for i in range(bind_count):
					bind_id = next(lines)
					parsed_tags = tuple(set(next(lines).split()) for ln in range(4))
					parsed_binds[bind_id] = parsed_tags

				binds = { k: tuple(set(t.split()) for t in v) for k,v in rule['binds'].items() }

				t.check(parsed_binds == binds, "Parsed binds", desc)
	except StopIteration:
		t.check(False, "Got correct output", desc)

