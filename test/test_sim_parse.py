
for rules_path in glob("test/fixtures/sim_parse/positive/*.md"):
	rules = read_file_string(rules_path)
	expected_json = read_file_json(rules_path.replace('.md', '.json'))

	result_json = json.loads(test_call("sim_parse", rules))
	t.check(expected_json == result_json, "Parsed rules correctly", rules_path)

for rules_path in glob("test/fixtures/sim_parse/negative/*.md"):
	rules = read_file_string(rules_path)

	result_json = json.loads(test_call("sim_parse", rules))
	t.check(result_json == None, "Failed to parse bad rules", rules_path)

