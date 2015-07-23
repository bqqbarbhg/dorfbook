#!/usr/bin/env python

import sys
import requests
from collections import namedtuple
from glob import glob

Fail = namedtuple("Fail", ("filename", "line", "message"))
def format_fail(fail):
	loc = '%s:%d' % (fail.filename, fail.line)
	return '%30s  %s' % (loc, fail.message)

def flush_write(text):
	sys.stdout.write(text)
	sys.stdout.flush()

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
		flush_write('.' if condition else 'F')
		sys.stdout.flush()

t = Tester()

for testfile in glob('test/*.py'):
	flush_write('Running %s: ' % testfile)
	execfile(testfile)
	flush_write('\n')

print 'Tests passed: %d/%d' % (t.num_total - len(t.fail_list), t.num_total)
if t.fail_list:
	print 'Failed tests:'
	print '\n'.join('%s' % format_fail(fail) for fail in t.fail_list)

sys.exit(len(t.fail_list))

