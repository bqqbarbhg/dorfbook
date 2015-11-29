import re

# Lines can be either empty or indented with tabs without trailing whitespace
LINE_PATTERN = re.compile(r'^(\t*\S(.*\S)?)?$')

check_folders = ['src', 'test']

ignore_paths = ['test/fixtures']

for folder in check_folders:
	for (root, dirs, files) in os.walk(folder):
		for f in files:
			path = os.path.join(root, f)

			normpath = path.replace('\\', '/')
			if any(p in normpath for p in ignore_paths):
				continue

			with open(path) as fl:
				fail_line = -1
				for num,line in enumerate(fl):
					if not LINE_PATTERN.match(line):
						fail_line = num
						break
				message = '%s is formatted' % path
				extra = 'see line %d' % (fail_line + 1)
				t.check(fail_line == -1, message, extra)


