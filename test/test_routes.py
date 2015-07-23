#!/usr/bin/env python

import sys
import requests

fail_list = []
num_total = 0

def test_assert(x, msg):
	if not x:
		fail_list.append(msg)
	num_total += 1

r = requests.get('http://localhost:3500/')
test_assert(r.status_code == 200, 'Can get root')

r = requests.get('http://localhost:3500/sdoijfiosdjf')
test_assert(r.status_code == 404, 'Random route gives 404')

print '%d tests %d passed' % (num_total, len(fail_list))
print '\n'.join('Fail: %s' % fail for fail in fail_list)
sys.exit(len(fail_list))

