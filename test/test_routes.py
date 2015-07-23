#!/usr/bin/env python

import sys
import requests
from collections import namedtuple

Fail = namedtuple("Fail", ("filename", "line", "message"))
def format_fail(fail):
	loc = '%s:%d' % (fail.filename, fail.line)
	return '%30s  %s' % (loc, fail.message)

class Tester:
	def __init__(self):
		self.fail_list = []
		self.num_total = 0

	def check(self, condition, message):
		if not condition:
			frame = sys._getframe(1)
			filename = frame.f_code.co_filename
			line = frame.f_lineno

			fail = Fail(filename, line, message)
			self.fail_list.append(fail)
		self.num_total += 1

t = Tester()

r = requests.get('http://127.0.0.1:3500/')
t.check(r.status_code == 200, 'Can get root')

r = requests.get('http://127.0.0.1:3500/sdoijfiosdjf')
t.check(r.status_code == 404, 'Random route gives 404')

print '%d tests %d passed' % (t.num_total, t.num_total - len(t.fail_list))
if t.fail_list:
	print 'Failed tests:'
	print '\n'.join('%s' % format_fail(fail) for fail in t.fail_list)

sys.exit(len(t.fail_list))

