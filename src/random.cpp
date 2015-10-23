
struct Random_Series
{
	U32 x, y, z, w;
};

// Create a random series with 32 bits of seed
Random_Series series_from_seed32(U32 seed)
{
	Random_Series series = { 0 };
	series.x = seed;
	series.y = seed;
	series.z = seed;
	series.w = seed;
	return series;
}

// Returns an uniform distribution in [0, 2^32)
U32 next32(Random_Series *series)
{
	// XorShift algorithm
	// Note: This may not be the best option, consider PCG
	Random_Series s = *series;
	U32 t = s.x ^ (s.x << 11);
	s.x = s.y; s.y = s.z; s.z = s.w;
	s.w = s.w ^ (s.w >> 19) ^ t ^ (t >> 8);
	*series = s;
	return s.w;
}

U32 next(Random_Series *series, U32 limit)
{
	// Note: This is slightly-nonuniform
	U32 value = next32(series);
	return value % limit;
}

// Returns true with the chance of 1 out of `inverse`
// eg. next_one_in(series, 4) returns true 25% of the time.
bool next_one_in(Random_Series *series, U32 inverse)
{
	U32 val = next32(series);
	return val < UINT32_MAX / inverse;
}

