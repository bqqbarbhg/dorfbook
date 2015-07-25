
void make_crc32_table()
{
	uint32_t table[256];

	// This is the binary representation of the reversed gzip standard CRC-32
	// polynomial.
	uint32_t poly = 0xEDB88320;

	for (uint32_t byte = 0; byte < 256; byte++) {

		uint32_t crc = byte;

		// This is basically the long division algorithm, but in binary
		// polynomial arithmetic and reversed. So subtract (xor) if the
		// remainder (crc) is larger than the divisor (poly).
		for (int bit = 0; bit < 8; bit++) {

			// 1) The poly is actually a 33-bit number represented as a 32 bit
			// number with the most significant bit assumed to be 1.
			// 2) The notion of order in binary polynomial arithmetic is defined
			// by which number has the highest most significant bit set.
			// 3) So because this is reversed arithmetic checking for the lowest
			// bit before shifting is equivalent to testing if the 33-bit crc
			// register is greater or equal to the poly.
			if (crc & 1) {

				// crc >= poly: subtract
				// Subtract the divisor from the crc, but since this is done in
				// 32-bit registers we have to ignore the most significant bit,
				// which will be zero in any case anyway, so shift before doing
				// the xor with the 32-bit representation of the divisor.
				crc = (crc >> 1) ^ poly;
			} else {

				// crc < poly: keep shifting
				crc = crc >> 1;
			}
		}

		table[byte] = crc;
	}

	pre_create_array_u32("crc32_table", table, Count(table), SOURCE_LOC);
}
