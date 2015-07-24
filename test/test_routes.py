

expect_routes = ['/', '/dwarves', '/locations', '/feed', '/stats']
for route in expect_routes:
	r = dorf_get(route)
	t.check(r.status_code == 200, "Can get '%s'" % route)

r = dorf_get('/sdoijfiosdjf')
t.check(r.status_code == 404, 'Random route gives 404')
