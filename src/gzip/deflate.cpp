
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

struct Bit_Ptr
{
	uint8_t *data;
	unsigned offset;
};

Bit_Ptr bit_ptr(void *data)
{
	Bit_Ptr ptr;
	ptr.data = (uint8_t*)data;
	*ptr.data = 0;
	ptr.offset = 0;
	return ptr;
}

void bit_write_lsb(Bit_Ptr *ptr, uint32_t val, unsigned bits)
{
	// Slow basic impl

	uint32_t mask = 1 << bits;
	uint8_t out = *ptr->data;
	unsigned offset = ptr->offset;

	for (unsigned bit = 0; bit < bits; bit++) {
		if (offset >= sizeof(*ptr->data) * 8) {
			ptr->offset = offset = 0;
			*ptr->data = out;
			ptr->data++;
			*ptr->data = out = 0;
		}

		out |= ((val >> bit) & 1) << offset;
		offset++;
	}
	*ptr->data = out;
	ptr->offset = offset;
}

void bit_write_msb(Bit_Ptr *ptr, uint32_t val, unsigned bits)
{
	// Slow basic impl

	uint32_t mask = 1 << bits;
	uint8_t out = *ptr->data;
	unsigned offset = ptr->offset;

	for (unsigned bit = 0; bit < bits; bit++) {
		if (offset >= sizeof(*ptr->data) * 8) {
			ptr->offset = offset = 0;
			*ptr->data = out;
			ptr->data++;
			*ptr->data = out = 0;
		}

		out |= ((val >> (bits - bit - 1)) & 1) << offset;
		offset++;
	}
	*ptr->data = out;
	ptr->offset = offset;
}

void* bit_finish(Bit_Ptr *ptr)
{
	return ptr->data + 1;
}

struct Code_Extra_Bucket
{
	int first_value;
	int first_code;
};

// Note: 258 has it's own special encoding which is handled outside of this table
const Code_Extra_Bucket length_buckets[] = {
	{ 3, 257 }, { 11, 265 }, { 19, 269 }, { 35, 273 }, { 67, 277 }, { 131, 281 }
};

const Code_Extra_Bucket distance_buckets[] = {
	{   1,  0 }, {   5,  4 }, {    9,  6 }, {   17,  8 }, {  33,  10 }, {   65, 12 }, {   129, 14 },
	{ 257, 16 }, { 513, 18 }, { 1025, 20 }, { 2049, 22 }, { 4097, 24 }, { 8193, 26 }, { 16385, 18 }
};

struct Code_Extra
{
	int code;
	int extra;
	int extra_bits;
};

Code_Extra to_code_extra_coding(int value, const Code_Extra_Bucket *buckets, int bucket_count)
{
	for (int bits = bucket_count - 1; bits >= 0; bits--) {
		Code_Extra_Bucket bucket = buckets[bits];
		if (value >= bucket.first_value) {
			int relative = value - bucket.first_value;

			Code_Extra coding;
			coding.code = bucket.first_code + (relative >> bits);
			coding.extra = relative & ((1 << bits) - 1);
			coding.extra_bits = bits;
			return coding;
		}
	}
	Code_Extra fail = { -1 };
	return fail;
}

Code_Extra to_length_coding(int length)
{
	// Length 258 (the maximum length of a reference) has a special code with
	// 0 extra bits.
	if (length == 258) {
		Code_Extra special_258_code = { 285, 0, 0 };
		return special_258_code;
	}

	return to_code_extra_coding(length, length_buckets, Count(length_buckets));
}

Code_Extra to_distance_coding(int distance)
{
	return to_code_extra_coding(distance, distance_buckets, Count(distance_buckets));
}

void write_fixed_literal(Bit_Ptr *ptr, unsigned char c)
{
	if (c < 144) {
		bit_write_msb(ptr, 0x30 + c, 8);
	} else {
		bit_write_msb(ptr, 0x190 + (c - 144), 9);
	}
}

void write_fixed_reference(Bit_Ptr *ptr, int distance, int length)
{
	Code_Extra length_code = to_length_coding(length);
	if (length_code.code < 280) {
		bit_write_msb(ptr, length_code.code - 256, 7);
	} else {
		bit_write_msb(ptr, length_code.code - 280 + 192, 8);
	}
	if (length_code.extra_bits > 0) {
		bit_write_lsb(ptr, length_code.extra, length_code.extra_bits);
	}

	Code_Extra distance_code = to_distance_coding(distance);
	bit_write_msb(ptr, distance_code.code, 5);
	if (distance_code.extra_bits > 0) {
		bit_write_lsb(ptr, distance_code.extra, distance_code.extra_bits);
	}
}

size_t deflate_compress_fixed_block(void *dst, size_t dst_length,
	const void *src, size_t src_length)
{
	const unsigned char *in = (const unsigned char*)src;

	uint8_t *out_start = (uint8_t*)dst;
	Bit_Ptr out = bit_ptr(out_start);

	bit_write_lsb(&out, 3, 3);

	Search_Context context;
	init_search_context(&context, in, (int)src_length);

	int pos = 0;
	while (!search_done(&context)) {
		Search_Match matches[128];
		int match_count = search_next_matches(&context, matches, Count(matches));

		for (int i = 0; i < match_count; i++) {
			Search_Match match = matches[i];
			if (pos > match.position)
				continue;

			while (pos < match.position) {
				write_fixed_literal(&out, in[pos]);
				pos++;
			}

			if (match.length <= 258) {
				// Default case
				write_fixed_reference(&out, match.offset, match.length);
				pos += match.length;

			} else {
				// Splice longer matches into shorter ones
				int length_left = match.length;
				while (length_left > 3) {
					int length = min(length_left, 258);

					write_fixed_reference(&out, match.offset, length);
					pos += length;
					length_left -= length;
				}
			}
		}
	}
	while (pos < src_length) {
		write_fixed_literal(&out, in[pos]);
		pos++;
	}

	free_search_context(&context);

	bit_write_msb(&out, 0x0, 7);

	uint8_t *out_end = (uint8_t*)bit_finish(&out);
	return out_end - out_start;
}

int write_length_code_zeroes(Code_Extra *length_codes,
	int *length_code_counts, int zero_count)
{
	int length_code_count = 0;

	while (zero_count > 138) {
		length_code_counts[18]++;
		length_codes[length_code_count].code = 18;
		length_codes[length_code_count].extra = 138 - 11;
		length_codes[length_code_count].extra_bits = 7;
		length_code_count++;
	}

	if (zero_count < 1) {
		// Pass.
	} else if (zero_count < 2) {
		length_code_counts[0]++;
		length_codes[length_code_count].code = 0;
		length_codes[length_code_count].extra_bits = 0;
		length_code_count++;
	} else if (zero_count < 3) {
		length_code_counts[0] += 2;
		length_codes[length_code_count].code = 0;
		length_codes[length_code_count].extra_bits = 0;
		length_code_count++;
		length_codes[length_code_count].code = 0;
		length_codes[length_code_count].extra_bits = 0;
		length_code_count++;
	} else if (zero_count < 11) {
		length_code_counts[17]++;
		length_codes[length_code_count].code = 17;
		length_codes[length_code_count].extra = zero_count - 3;
		length_codes[length_code_count].extra_bits = 3;
		length_code_count++;
	} else {
		length_code_counts[18]++;
		length_codes[length_code_count].code = 18;
		length_codes[length_code_count].extra = zero_count - 11;
		length_codes[length_code_count].extra_bits = 7;
		length_code_count++;
	}

	return length_code_count;
}

int create_length_codes(Code_Extra *length_codes, int *length_code_counts,
	Huffman_Code *codes, int length, int min_count)
{
	int length_code_count = 0;
	int zero_stride = 0;

	for (int i = 0; i < length; i++) {
		int length = codes[i].bits;
		if (length == 0) {
			zero_stride++;
			continue;
		} else {
			length_code_count += write_length_code_zeroes(
				length_codes + length_code_count, length_code_counts, zero_stride);
			zero_stride = 0;
		}

		length_code_counts[length]++;
		length_codes[length_code_count].code = length;
		length_codes[length_code_count].extra_bits = 0;
		length_code_count++;

		// Use the symbol 15 for repeating the previous symbol 3-10 times
		int j;
		for (j = i + 1; j < min(length, j + 6); j++) {
			if (length != codes[j].bits) {
				break;
			}
		}
		int repeat = j - i - 1;
		
		if (repeat >= 3) {
			length_code_counts[16]++;
			length_codes[length_code_count].code = 16;
			length_codes[length_code_count].extra = repeat - 3;
			length_codes[length_code_count].extra_bits = 2;
			length_code_count++;
			i += repeat;
		}
	}

	if (length_code_count < min_count) {
		length_code_count += write_length_code_zeroes(
			length_codes + length_code_count, length_code_counts,
			min_count - length_code_count);
	}

	// We might be left with some zero_stride but we can assume the rest are
	// 0 if we don't specify them.

	return length_code_count;
}

inline void write_huffman_code(Bit_Ptr *ptr, Huffman_Code *table, Code_Extra code)
{
	Huffman_Code symbol = table[code.code];

	bit_write_msb(ptr, symbol.code, symbol.bits);

	if (code.extra_bits > 0) {
		bit_write_lsb(ptr, code.extra, code.extra_bits);
	}
}

size_t deflate_compress_dynamic_block(void *dst, size_t dst_length,
	const void *src, size_t src_length)
{
	const unsigned char *in = (const unsigned char*)src;

	// Collect all the matches
	unsigned match_size = 32;
	Search_Match *matches = 0;

	int match_count = 0;

	Search_Context context;
	init_search_context(&context, in, (int)src_length);
	while (!search_done(&context)) {
		matches = (Search_Match*)realloc(matches, match_size * sizeof(Search_Match));

		match_count += search_next_matches(&context, 
			matches + match_count, match_size - match_count);

		match_size *= 2;
	}
	free_search_context(&context);

	// Count how many times each symbol is used
	int literal_length_count[286] = { 0 };
	int distance_count[32] = { 0 };

	// There is one end symbol.
	literal_length_count[256] = 1;

	int pos = 0;
	for (int i = 0; i < match_count; i++) {
		Search_Match match = matches[i];

		while (pos < match.position) {
			literal_length_count[in[pos]]++;
		}
		
		Code_Extra length_code = to_length_coding(match.length);
		Code_Extra distance_code = to_distance_coding(match.offset);

		literal_length_count[length_code.code]++;
		distance_count[distance_code.code]++;

		pos += matches[i].length;
	}
	while (pos < src_length) {
		literal_length_count[in[pos]]++;
	}

	// Create the two Huffman codings for the literal-length and distance
	// alphabets.
	Huffman_Code literal_length_codes[286];
	Huffman_Code distance_codes[32];

	int literal_length_code_amount =
		canonical_huffman(literal_length_codes, literal_length_count, 288, 15);
	int distance_code_amount =
		canonical_huffman(distance_codes, distance_count, 32, 15);

	// Create the Huffman coding for the lengths of the symbols of the two
	// other Huffman codings.
	Code_Extra length_codes[286 + 32];
	int length_code_counts[19] = { 0 };

	int litlen_length_count = create_length_codes(
		length_codes, length_code_counts, literal_length_codes, 286, 257);

	int dist_length_count = create_length_codes(
		length_codes + litlen_length_count, length_code_counts,
		distance_codes, 32, 1);

	Huffman_Code length_code_codes[19];

	int length_code_code_amount =
		canonical_huffman(length_code_codes, length_code_counts, 19, 7);

	if (length_code_code_amount < 4)
		length_code_code_amount = 4;

	uint8_t *out_start = (uint8_t*)dst;
	Bit_Ptr out = bit_ptr(out_start);

	// Write the header.
	bit_write_lsb(&out, 2, 3);

	// Write the code length counts.
	bit_write_lsb(&out, max(literal_length_code_amount, 257) - 257, 5);
	bit_write_lsb(&out, max(distance_code_amount, 1) - 1, 5);
	bit_write_lsb(&out, length_code_code_amount - 4, 5);

	// Write the code lengths of the code length codes.
	for (int i = 0; i < length_code_code_amount; i++) {
		bit_write_lsb(&out, length_code_counts[i], 3);
	}

	// Write the literal/length and distance code lengths.
	for (int i = 0; i < litlen_length_count + dist_length_count; i++) {
		write_huffman_code(&out, length_code_codes, length_codes[i]);
	}

	pos = 0;
	for (int i = 0; i < match_count; i++) {
		Search_Match match = matches[i];

		while (pos < match.position) {
			Code_Extra literal_code;
			literal_code.code = in[pos];
			literal_code.extra_bits = 0;
			write_huffman_code(&out, literal_length_codes, literal_code);
		}
		
		Code_Extra length_code = to_length_coding(match.length);
		Code_Extra distance_code = to_distance_coding(match.offset);

		write_huffman_code(&out, literal_length_codes, length_code);
		write_huffman_code(&out, distance_codes, distance_code);

		pos += matches[i].length;
	}
	while (pos < src_length) {
		literal_length_count[in[pos]]++;
	}


	// Write the terminating symbol.
	Code_Extra final_code;
	final_code.code = 256;
	final_code.extra_bits = 0;
	write_huffman_code(&out, literal_length_codes, final_code);

	free(matches);

	uint8_t *out_end = (uint8_t*)bit_finish(&out);
	return out_end - out_start;
}

size_t gzip_compress(void *dst, size_t dst_length,
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
	size_t len = deflate_compress_dynamic_block(out, dst_length - 1, src, src_length);
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

