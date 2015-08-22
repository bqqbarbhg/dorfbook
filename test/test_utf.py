
def to_utf8(chars):
	return ''.join('\\U%08x' % c for c in chars).decode('unicode-escape').encode('utf-8')

utf_fixtures = [
	(range(128), 'All ascii characters'),
	(range(256), '"Extended" ascii characters'),
	(range(0xD800) + range(0xE0000, 0x10000), 'Basic multilingual plane'),
	(range(0x0700, 0x0900), 'Three byte edge'),
	(range(0xF000, 0x11000), 'Four byte edge'),
	(range(0x1F601, 0x1F64F), 'Emoji emoticons'),
]

for data, desc in utf_fixtures:
	utf8 = to_utf8(data)
	dorf_utf8 = test_call("utf8_encode", ' '.join('%d' % n for n in data))
	t.check(utf8 == dorf_utf8, "UTF-8 encoding is correct", desc)

	dorf_codepoints = [int(c) for c in test_call("utf8_decode", utf8).split()]
	t.check(data == dorf_codepoints, "UTF-8 decoding is correct", desc)

