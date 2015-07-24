#!/usr/bin/env python

import sys
import os.path
import requests
from collections import namedtuple
from glob import glob
import subprocess
import signal
from bs4 import BeautifulSoup

def flush_write(text):
	sys.stdout.write(text)
	sys.stdout.flush()

def dorf_get(route):
	return requests.get('http://127.0.0.1:3500' + route)

# Search for the correct binary name
binary_names = ['bin/dorfbook.exe', 'bin/dorfbook']
binary = next((n for n in binary_names if os.path.isfile(n)), None)
if not binary:
	print 'Could not find dorfbook binary'
	sys.exit(-1)

print 'Starting server'

# TODO: Support opening on some free port, communicate port to tests
test_out_log = open('bin/test_out.log', 'w')
test_err_log = open('bin/test_err.log', 'w')
server = subprocess.Popen([binary], stdout=test_out_log, stderr=test_err_log)

# Wait for the server to respond
for t in range(10):
	try:
		requests.get('http://127.0.0.1:3500/', timeout=0.2)
		break
	except requests.ConnectionError:
		flush_write('-')
else:
	flush_write('\n')
	print 'Could not connect to the server'
	server.terminate()
	sys.exit(-2)

flush_write('\n')

# The mini testing framework

Fail = namedtuple("Fail", ("filename", "line", "description", "extra"))
def format_fail(fail):
	loc = '%s:%d' % (fail.filename, fail.line)
	line = '%30s  %s' % (loc, fail.description)
	if fail.extra:
		line += '  (%s)' % fail.extra
	return line

class Tester:
	def __init__(self):
		self.fail_list = []
		self.num_total = 0

	def check(self, condition, description, extra=None):
		self.num_total += 1

		if not condition:
			frame = sys._getframe(1)
			filename = frame.f_code.co_filename
			line = frame.f_lineno
			fail = Fail(filename, line, description, extra)

			self.fail_list.append(fail)

		flush_write('.' if condition else 'F')
		return condition

t = Tester()

# Run all the python files under test/
for testfile in glob('test/*.py'):
	flush_write('Running %s: ' % testfile)
	execfile(testfile)
	flush_write('\n')

flush_write('\n')

print 'Closing server'
server.terminate()

flush_write('\n')

# Print the test results

print 'Tests passed: %d/%d' % (t.num_total - len(t.fail_list), t.num_total)
if t.fail_list:
	print 'Failed tests:'
	print '\n'.join('%s' % format_fail(fail) for fail in t.fail_list)

flush_write('\n')

sys.exit(len(t.fail_list))

