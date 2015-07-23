#!/usr/bin/env python

import sys
import os.path
import requests
from collections import namedtuple
from glob import glob
import subprocess
import signal

binary_names = ['bin/dorfbook.exe', 'bin/dorfbook']
binary = next((n for n in binary_names if os.path.isfile(n)), None)

if not binary:
	print 'Could not find dorfbook binary'
	sys.exit(-1)

def flush_write(text):
	sys.stdout.write(text)
	sys.stdout.flush()

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
		flush_write('.' if condition else 'F')
		sys.stdout.flush()

t = Tester()

for testfile in glob('test/*.py'):
	flush_write('Running %s: ' % testfile)
	execfile(testfile)
	flush_write('\n')

flush_write('\n')

print 'Closing server'
server.terminate()

flush_write('\n')

print 'Tests passed: %d/%d' % (t.num_total - len(t.fail_list), t.num_total)
if t.fail_list:
	print 'Failed tests:'
	print '\n'.join('%s' % format_fail(fail) for fail in t.fail_list)

flush_write('\n')

sys.exit(len(t.fail_list))

