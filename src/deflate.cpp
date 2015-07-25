
// A slow gzip compliant CRC-32 implementation
uint32_t crc32(const void* data, size_t length)
{
	const uint8_t *bytes = (const uint8_t*)data;

	// The gzip CRC-32 expects the register to be inverted in the beginning
	// and the end of the algorithm.
	uint32_t crc = ~0U;

	for (size_t byte = 0; byte < length; byte++) {

		// See pre/pre_deflate.cpp
		crc = crc32_table[(crc ^ bytes[byte]) & 0xFF] ^ (crc >> 8);
	}

	// Invert again (per gzip standard)
	return ~crc;
}

size_t deflate_no_compress(void *dst, size_t dst_length,
		const void *src, size_t src_length)
{
	const unsigned char *in = (const unsigned char*)src;
	unsigned char *out_start = (unsigned char*)dst;
	unsigned char *out = out_start;

	// Send the data us uncompressed DEFLATE blocks.
	size_t max_block_size = 0xffff;
	size_t length_left = src_length;
	while (length_left > 0) {
		size_t block_size = min(length_left, max_block_size);

		// Check that there is enough space to fit the block
		if ((out - out_start) + block_size + 5 >= dst_length)
			return 0;

		// The uncompressed blocks begin with a 3-bit header of format [e00]
		// where the first bit indicates if this is the final block in the
		// stream. The 3-bit header could be anywhere in the byte but since
		// currently this outputs only uncompressed blocks it will always be
		// byte aligned.
		// TODO: Handle unaligned header.
		*out++ = length_left > max_block_size ? 0 : 1;

		// The last bits of the current byte are ignored and the next four
		// bytes contain the 16-bit length of the uncompressed block and the
		// length's complement.
		*out++ = (unsigned char)(block_size);
		*out++ = (unsigned char)(block_size >> 8);
		*out++ = (unsigned char)(~block_size);
		*out++ = (unsigned char)(~(block_size >> 8));

		// Then just the plain raw data byte aligned.
		memcpy(out, in, block_size);
		out += block_size;
		in += block_size;

		length_left -= block_size;
	}

	return out - out_start;
}

size_t gzip_no_compress(void *dst, size_t dst_length,
		const void *src, size_t src_length)
{
	unsigned char *out_start = (unsigned char*)dst;
	unsigned char *out = out_start;

	// Check that there is space for the header
	if (dst_length < 10)
		return 0;

	// Magic number.
	*out++ = 0x1f;
	*out++ = 0x8b;

	// Compression method: deflate
	*out++ = 0x08;

	// No flags for now (`src` is assumed to be text, no extra CRC-16, no
	// extra headers, no file name, no comment)
	*out++ = 0x00;

	time_t tm = time(NULL);
	uint32_t time32 = (uint32_t)tm;

	// 4-byte UNIX timestamp.
	*out++ = (unsigned char)(time32 >> 0);
	*out++ = (unsigned char)(time32 >> 8);
	*out++ = (unsigned char)(time32 >>16);
	*out++ = (unsigned char)(time32 >>24);

	// Extra flags: none.
	*out++ = 0x00;

	// Operating system: unknown.
	*out++ = 0xff;

	// Append the DEFLATE compressed data.
	size_t len = deflate_no_compress(out, dst_length - 1, src, src_length);
	if (len == 0)
		return 0;

	out += len;

	// Check that there is space for the footer
	if ((size_t)(out - out_start) + 8 >= dst_length)
		return 0;

	uint32_t crc = crc32(src, src_length);

	// 4-byte CRC-32
	*out++ = (unsigned char)(crc >> 0);
	*out++ = (unsigned char)(crc >> 8);
	*out++ = (unsigned char)(crc >>16);
	*out++ = (unsigned char)(crc >>24);

	// 4-byte original length
	*out++ = (unsigned char)(src_length >> 0);
	*out++ = (unsigned char)(src_length >> 8);
	*out++ = (unsigned char)(src_length >>16);
	*out++ = (unsigned char)(src_length >>24);

	return out - out_start;
}

