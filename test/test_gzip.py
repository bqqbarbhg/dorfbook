
import random
import zlib
import gzip

fixtures = [
	(bytearray([]), "Empty"),
	(bytearray([0]), "Single zero"),
	(bytearray(list(range(256))), "All sequential bytes"),
	(bytearray([0xAB, 0xCD, 0xEF, 0x99] * 2000), "2000x ABCDEF99"),
]

for data, desc in fixtures:

	dorf_crc = int(test_call("crc32", data))
	zlib_crc = zlib.crc32(str(data)) & 0xFFFFFFFF

	t.check(dorf_crc == zlib_crc, "crc32 is compatible with zlib", desc)


for data, desc in fixtures:

	dorf_gzip = test_call("gzip", data)
	success = False
	extra = None
	try:
		with gzip.GzipFile(fileobj=StringIO(dorf_gzip)) as gzip_file:
			content_round_trip = gzip_file.read()
			if content_round_trip == data:
				success = True
			else:
				extra = 'mismatch'
	except zlib.error as err:
		extra = 'zlib: %s' % str(err.message)
	except IOError as err:
		extra = 'io: %s' % str(err.message)
	except:
		extra = 'other'

	t.check(success, "gzip round trips", '%s, %s' % (extra, desc))



