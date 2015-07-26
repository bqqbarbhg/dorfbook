
import random
import zlib
import gzip

for _ in range(5):
	length = random.randint(1, 1000)
	arr = str(random.choice(string.printable) for _ in range(length))

	dorf_crc = int(test_call("crc32", arr))
	zlib_crc = zlib.crc32(arr) & 0xFFFFFFFF

	t.check(dorf_crc == zlib_crc, "crc32 is compatible with zlib")


for _ in range(5):
	length = random.randint(1, 10000)
	content = str(random.choice(string.printable) for _ in range(length))

	dorf_gzip = test_call("gzip", content)
	success = False
	extra = None
	try:
		with gzip.GzipFile(fileobj=StringIO(dorf_gzip)) as gzip_file:
			content_round_trip = gzip_file.read()
			if content_round_trip == content:
				success = True
	except zlib.error as err:
		extra = 'zlib: %s' % str(err.message)
	except IOError as err:
		extra = 'io: %s' % str(err.message)
	except:
		extra = 'other'

	t.check(success, "gzip round trips", extra)



