
import random
import zlib

for _ in range(5):
	length = random.randint(1, 1000)
	arr = str(random.choice(string.printable) for _ in range(length))

	dorf_crc = int(test_call("crc32", arr))
	zlib_crc = zlib.crc32(arr) & 0xFFFFFFFF

	t.check(dorf_crc == zlib_crc, "crc32 is compatible with zlib")
