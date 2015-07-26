
for data, desc in fixtures:
	ret = test_call("identity", data)
	t.check(ret == data, "test round trips", desc)
