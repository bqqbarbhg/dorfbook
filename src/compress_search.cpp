// This file implements an algorithm to find recurring strings in a stream, which
// can be used for compression. The algorithm is based on following chains of tuples.
//
// Example: The caret points at the position we are currently trying to find a
// match for.
//
// The matching word "latch-math" is a material match.
//                                              ^
//
// Check the tuple of the next three characters [mat] and retrieve the chain.
//
// The matching word "latch-math" is a material match.
//-----^--------------------^----------^--------^
//
// Then start following the chain backwards and see how many characters match.
//
//                                      ___
//                                     match
// The matching word "latch-math" is a material match.
//                                     ~~~
//
// Then select a tuple that is one character past the end of the match ([atc] in
// this case). This tuple must appear in every longer match than this. Retrieve
// its chain also and now find locations where the two tuples (begin and end)
// are the required distance away from each other.
//
// The matching word "latch-math" is a material match.
//     ||              |    |          |        ||
//-----^+--------------+----^----------^--------^|
//------^--------------^-------------------------^
//
// The only place where the two chains sync up is at the word "matching". Start
// checking how many characters match again and update the end tuple (would be
// set to [ch.] and continue with the algorithm until the chains run out.
//
// The implementation differs slightly from the above explanation. The chains
// are not guaranteed to only have one tuple. The chains are formed by hashing
// the tuple and looking up a table that is smaller than the amount of possible
// tuples. This makes the algorithm a little bit slower but because the matches
// are always scanned from the beginning, hash-collisions are detected and
// skipped over.

#define SEARCH_CHAIN_COUNT 4096
#define SEARCH_DATA_COUNT 200000
#define SEARCH_TUPLE_SIZE 3
#define MAX_SYNC_AMOUNT 10000

#define SEARCH_MAX_DIST 32000

// Contains the data to follow a series of a recurring tuple through the data.
// One chain might contain many tuples but one tuple is always sure to belong
// to the same chain. So all instances of the tuple [abc] will be in the same
// tuple.
struct Search_Chain
{
	// Absolute byte index of the latest occurrence.
	int base;

	// Number of relative distances that can be stored in this chain still
	// without having to reallocate it.
	int capacity;

	// Index to the `chain_data` buffer which contains the relative distances
	// between the occurrences of the tuple.
	uint32_t data_index;
};

// Match two series of bytes (up to max_length). Returns the number of bytes
// that were equal.
inline int strmatch(const char *a, const char *b, int max_length)
{
	int length;
	for (length = 0; length < max_length; length++) {
		if (a[length] != b[length]) {
			break;
		}
	}
	return length;
}

// This is where most of the time is spent.
// It lines up two chain iterators so that there is `target_dist` bytes between
// them. `a` and `b` are absolute byte indices and `a_it` and `b_it` are
// pointers to the chains relative advance distance lists.
// Returns true if a synced position is found.
inline bool sync_iterators(int *a, uint16_t **a_it, int *b, uint16_t **b_it,
	int target_dist, int pos)
{
	int a_pos = *a, b_pos = *b;
	uint16_t *a_iter = *a_it, *b_iter = *b_it;
	int stop = pos - SEARCH_MAX_DIST;

	// Adjust base so that negative positions are unreachable.
	a_pos -= stop;
	b_pos -= stop;

	for (int t = 0; t != MAX_SYNC_AMOUNT; t++) {
		int diff = (b_pos - a_pos) - target_dist;
		if (diff < 0) {
			a_pos -= *a_iter++;
			if (a_pos < 0) {
				return false;
			}
		} else if (diff != 0) {
			b_pos -= *b_iter++;
			if (b_pos < 0) {
				return false;
			}

		} else {
			// Adjust the base back and store result.
			*a = a_pos + stop;
			*b = b_pos + stop;
			*a_it = a_iter;
			*b_it = b_iter;

			return true;
		}
	}
	return false;
}

// This hash is the chain id for the tuple.
unsigned tuplehash(const char *data)
{
	unsigned hash = 0;
	for (int i = 0; i < SEARCH_TUPLE_SIZE; i++) {
		hash = (hash ^ data[i]) * 16777619;
	}
	return hash;
}

// Used for marking in the garbage collection.
struct Live_Chain
{
	unsigned chain_index;
	int data_index;
	int data_length;
};

int live_chain_compare(const void *a, const void *b)
{
	return ((Live_Chain*)a)->data_index - ((Live_Chain*)b)->data_index;
}

// Since the chains are dynamically allocated in a buffer it can run out of space.
// In this case we run a garbage collection, which frees all the unreachable
// chain nodes and unused memory.
// Returns the amount of the used space of the `chain_data` buffer.
unsigned chain_data_gc(Search_Chain *chains, unsigned chain_count, uint16_t *chain_data, unsigned chain_data_count, int pos)
{
	Live_Chain *live_chains = (Live_Chain*)malloc(sizeof(Live_Chain) * chain_count);
	unsigned live_chain_count = 0;

	int drop_at = pos - SEARCH_MAX_DIST;
	for (unsigned i = 0; i < chain_count; i++) {
		Search_Chain *chain = &chains[i];
		chain->capacity = 0;

		int chain_pos = chain->base;
		int chain_data_index = chain->data_index;

		uint16_t *start = &chain_data[chain_data_index];
		uint16_t *iter = start;

		// Follow the chain as far as possible
		while (chain_pos >= drop_at) {
			chain_pos -= *iter++;
		}

		// Length is one shorter since the last one is always the terminator.
		int length = (int)(iter - start - 1);

		// If there are not enough dynamic nodes we can point it at the null chain.
		if (length <= 0) {
			chain->data_index = 0;
			continue;
		}

		// Keep the chain
		Live_Chain *live_chain = &live_chains[live_chain_count++];
		live_chain->chain_index = i;
		live_chain->data_index = chain_data_index;
		live_chain->data_length = length;
	}

	// Leave null index at 0.
	unsigned data_index = 1;

	// If the chains are sorted then when copying the live ones there is no need
	// for an extra buffer since the elements always shrink there is never the
	// danger of overwriting other live chains.
	qsort(live_chains, live_chain_count, sizeof(Live_Chain), &live_chain_compare);

	for (unsigned i = 0; i < live_chain_count; i++) {
		Live_Chain *live = &live_chains[i];
		Search_Chain *chain = &chains[live->chain_index];

		memcpy(&chain_data[data_index], &chain_data[live->data_index],
				live->data_length * sizeof(uint16_t));

		// The chains always end with a terminator, but it's not counted in length.
		chain_data[live->data_index + live->data_length] = 0xFFFF;

		chain->data_index = data_index;
		data_index += live->data_length + 1;
	}
	return data_index;
}

struct Search_Match
{
	int position;
	int offset;
	int length;
};

struct Search_Context
{
	int position;
	const char *data;
	int length;

	Search_Chain *chains;
	uint16_t *chain_data;
	unsigned chain_data_head;

	Search_Chain *last_end_chain;
	int last_match_pos;
	int last_match_length;
	bool last_match_was_best;
};

void init_search_context(Search_Context *context, const void *data, int length)
{
	context->position = 0;
	context->data = (const char*)data;
	context->length = length;

	context->chains = (Search_Chain*)malloc(SEARCH_CHAIN_COUNT * sizeof(Search_Chain));
	context->chain_data = (uint16_t*)malloc(SEARCH_DATA_COUNT * sizeof(uint16_t));

	// TODO: Free list optimization (less GC)

	// Initialize the null-chain
	context->chain_data[0] = 0xFFFF;
	context->chain_data_head = 1;

	// Initialize all the chains to be too far away to reference (not seen yet)
	Search_Chain *chains = context->chains;
	for (unsigned i = 0; i < SEARCH_CHAIN_COUNT; i++) {
		chains[i].base = -SEARCH_MAX_DIST;
	}

	context->last_end_chain = 0;
	context->last_match_length = 0;
	context->last_match_was_best = 0;
}

int search_position(const Search_Context *context)
{
	return context->position;
}

bool search_done(const Search_Context *context)
{
	return context->position >= context->length;
}

int search_next_matches(Search_Context *context, Search_Match *matches, int max_matches)
{
	unsigned chain_data_size = SEARCH_DATA_COUNT;
	int match_count = 0;

	// Restore the state from context to the stack, this should help compilers
	// optimize the variables better.
	const char *data = context->data;
	int length = context->length;

	Search_Chain *chains = context->chains;
	uint16_t *chain_data = context->chain_data;
	unsigned chain_data_head = context->chain_data_head;

	Search_Chain *last_end_chain = context->last_end_chain;
	int last_match_pos = context->last_match_pos;
	int last_match_length = context->last_match_length;
	bool last_match_was_best = context->last_match_was_best;

	// TODO: Free list optimization (less GC)

	int pos = context->position;
	for (; pos < length; pos++) {
		if (match_count >= max_matches)
			break;

		if (pos + SEARCH_TUPLE_SIZE > length) {
			// This algorithm can't find matches that are less than the tuple
			// size in length. But in practice that's not a problem because there
			// is no need for that short matches anyway.
			continue;
		}

		const char *pos_str = &data[pos];

		// Find the chain the beginning tuple of this match belongs to.
		unsigned hash = tuplehash(pos_str) % SEARCH_CHAIN_COUNT;
		Search_Chain *chain = &chains[hash];

		int begin_pos = chain->base;
		int pos_diff = pos - begin_pos;

		// Set as the new base of the chain.
		chain->base = pos;

		if (pos_diff >= SEARCH_MAX_DIST) {
			// Never seen or too far to reference: Orphan the chain.
			chain->capacity = 0;
			chain->data_index = 0;
			continue;
		}

		// If there is not enough space in the chain, its storage needs to be reallocated
		if (chain->capacity == 0) {
			uint16_t *begin = &chain_data[chain->data_index];
			uint16_t *iter = begin;

			int chain_diff = pos_diff;
			do {
				chain_diff += *iter++;
			} while (chain_diff < SEARCH_MAX_DIST);

			unsigned length = (int)(iter - begin);

			// Grow the allocation geometrically to reduce the amount of reallocations needed.
			unsigned alloc_amount = length * 2;

			// If there is no space try to do a garbage collection, if that doesn't help give up.
			if (chain_data_head + alloc_amount > chain_data_size) {

				chain_data_head = chain_data_gc(chains, SEARCH_CHAIN_COUNT,
					chain_data, chain_data_size, pos);

				if (chain_data_head + alloc_amount > chain_data_size) {
					return false;
				}
			}

			unsigned capacity = alloc_amount - length;
			int data_begin = chain_data_head + capacity;

			// Copy the old data.
			memcpy(chain_data + data_begin, begin, length * sizeof(uint16_t));

			chain->data_index = data_begin;
			chain->capacity = capacity;

			chain_data_head += alloc_amount;
		}

		// Add the distance to the previous occurrence to the chain.
		chain->capacity--;
		chain_data[--chain->data_index] = (uint16_t)pos_diff;

		uint16_t *begin_iter = chain_data + chain->data_index + 1;

		// Info about the current match candidate.
		int match_pos = 0;
		int match_length = 0;
		bool best_match = false;

		// The chain and iterator for the tuple that is required for a match
		// that is longer than the previous matches.
		Search_Chain *end_chain = 0;
		int end_pos;
		uint16_t *end_iter;

		int target_match_length = SEARCH_TUPLE_SIZE;

		// Can't match the buffer but don't limit the match lengths since it
		// breaks the buffering of the matches. Let the calling code splice
		// the too long matches into smaller ones (easy)
		int input_left = length - pos;
		int max_match_length = input_left;

		// If this match is a sub-match of the previous one we have already
		// found a match which is one byte shorter than the last one.
		int submatch_length = last_match_length - 1;
		if (submatch_length > SEARCH_TUPLE_SIZE) {
			// Set as new minimum goal
			match_pos = last_match_pos + 1;
			match_length = submatch_length;

			if (last_match_was_best) {
				// If the last match was the best possible match just accept it.
				last_match_was_best = true;
				last_match_pos = match_pos;
				last_match_length = match_length;
				continue;
			}

			// Re-use the previous end tuple, since it must be included in any
			// longer matches than the current submatch of the previous one.
			end_chain = last_end_chain;
			end_pos = end_chain->base;
			end_iter = chain_data + end_chain->data_index;
			target_match_length = last_match_length;
		}

		do {
			// If we have an end chain make sure that we only check places where
			// the begin and end iterators are separated by the target distance
			int target_dist = target_match_length - SEARCH_TUPLE_SIZE;
			if (end_chain && !sync_iterators(&begin_pos, &begin_iter, &end_pos, &end_iter, target_dist, pos)) {
				break;
			}

			// Compare the reference byte-by-byte as far as they match.
			int length = strmatch(pos_str, &data[begin_pos], max_match_length);
			int mpos = begin_pos;

			if (length >= target_match_length) {

				// Found a new longest match!
				match_length = length;
				match_pos = mpos;

				if (match_length == max_match_length) {
					// Accept as the best match.
					best_match = true;
					break;
				}

				// We MUST have additional characters left that could match, create the tuple
				// of the source string that contains one character past of the current best
				// match.

				const int end_offset = match_length - SEARCH_TUPLE_SIZE + 1;
				unsigned end_hash = tuplehash(pos_str + end_offset) % SEARCH_CHAIN_COUNT;

				Search_Chain *chain = &chains[end_hash];

				if (pos - chain->base >= SEARCH_MAX_DIST) {
					// There is no instance of a tuple that is required to continue the match
					// therefore this is the best match.
					best_match = true;
					break;
				}

				end_chain = chain;
				end_pos = end_chain->base;
				end_iter = chain_data + end_chain->data_index;

				target_match_length = match_length + 1;
			}

			// Advance.
			begin_pos -= *begin_iter++;
		} while (pos - begin_pos < SEARCH_MAX_DIST);

		// If there was a match and it's not _completely_ included in the last one
		// write it to the buffer.
		if (match_length >= SEARCH_TUPLE_SIZE && match_length >= last_match_length) {
			Search_Match *match = &matches[match_count++];
			match->position = pos;
			match->offset = pos - match_pos;
			match->length = match_length;
		}

		last_end_chain = end_chain;
		last_match_length = match_length;
		last_match_was_best = best_match;
	}

	// Store the cached data back to the context.
	context->chain_data_head = chain_data_head;
	context->last_end_chain = last_end_chain;
	context->last_match_pos = last_match_pos;
	context->last_match_length = last_match_length;
	context->last_match_was_best = last_match_was_best;
	context->position = pos;

	return match_count;
}

void free_search_context(Search_Context *context)
{
	free(context->chains);
	free(context->chain_data);
}

