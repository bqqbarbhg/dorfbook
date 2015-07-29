#define SEARCH_CHAIN_COUNT 4096
#define SEARCH_DATA_COUNT 200000
#define SEARCH_TUPLE_SIZE 3
#define MAX_SYNC_AMOUNT 10000

#define SEARCH_MAX_DIST 32000

struct Search_Chain
{
	int base;
	int capacity;
	uint32_t data_index;
};

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

inline bool sync_iterators(int *a, uint16_t **a_it, int *b, uint16_t **b_it,
	int target_dist, int pos)
{
	int a_pos = *a, b_pos = *b;
	uint16_t *a_iter = *a_it, *b_iter = *b_it;
	int stop = pos - SEARCH_MAX_DIST;

	// Adjust base
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
			*a = a_pos + stop;
			*b = b_pos + stop;
			*a_it = a_iter;
			*b_it = b_iter;

			return true;
		}
	}
	return false;
}

unsigned tuplehash(const char *data)
{
	unsigned hash = 0;
	for (int i = 0; i < SEARCH_TUPLE_SIZE; i++) {
		hash = (hash ^ data[i]) * 16777619;
	}
	return hash;
}

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

		while (chain_pos >= drop_at) {
			chain_pos -= *iter++;
		}

		int length = (int)(iter - start - 1);
		if (length <= 0) {
			if (length < 0) {
				chain->base = -SEARCH_MAX_DIST;
			}
			chain->data_index = 0;
			continue;
		}
		Live_Chain *live_chain = &live_chains[live_chain_count++];
		live_chain->chain_index = i;
		live_chain->data_index = chain_data_index;
		live_chain->data_length = length;
	}

	// Leave null index at 0.
	unsigned data_index = 1;

	qsort(live_chains, live_chain_count, sizeof(Live_Chain), &live_chain_compare);

	for (unsigned i = 0; i < live_chain_count; i++) {
		Live_Chain *live = &live_chains[i];

		Search_Chain *chain = &chains[live->chain_index];
		memcpy(&chain_data[data_index], &chain_data[live->data_index],
				live->data_length * sizeof(uint16_t));
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

		if (pos + SEARCH_TUPLE_SIZE <= length) {
			const char *pos_str = &data[pos];

			unsigned hash = tuplehash(pos_str) % SEARCH_CHAIN_COUNT;
			Search_Chain *chain = &chains[hash];

			int begin_pos = chain->base;
			int pos_diff = pos - begin_pos;

			// This is the new base for the chain
			chain->base = pos;

			if (pos_diff >= SEARCH_MAX_DIST) {
				// Never seen or too far to reference: Orphan the chain.
				chain->capacity = 0;
				chain->data_index = 0;
				continue;
			}

			if (chain->capacity == 0) {
				uint16_t *begin = &chain_data[chain->data_index];
				uint16_t *iter = begin;
				int chain_diff = pos_diff;
				do {
					chain_diff += *iter++;
				} while (chain_diff < SEARCH_MAX_DIST);
				unsigned length = (int)(iter - begin);
				unsigned alloc_amount = length * 2;
				unsigned capacity = alloc_amount - length;

				if (chain_data_head + alloc_amount > chain_data_size) {
					chain_data_head = chain_data_gc(chains, SEARCH_CHAIN_COUNT,
						chain_data, chain_data_size, pos);
					if (chain_data_head + alloc_amount > chain_data_size) {
						return -1;
					}
				}

				int data_begin = chain_data_head + capacity;
				memcpy(chain_data + data_begin, begin, length * sizeof(uint16_t));
				chain->data_index = data_begin;
				chain->capacity = capacity;

				chain_data_head += alloc_amount;
			}

			chain->capacity--;
			chain_data[--chain->data_index] = (uint16_t)pos_diff;

			uint16_t *begin_iter = chain_data + chain->data_index + 1;

			int match_pos = 0;
			int match_length = 0;
			bool best_match = false;

			Search_Chain *end_chain = 0;
			int end_pos;
			uint16_t *end_iter;

			int target_match_length = SEARCH_TUPLE_SIZE;

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

				begin_pos -= *begin_iter++;

				if (length >= target_match_length) {

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

					/*
					int stop = begin_pos + target_match_length - SEARCH_TUPLE_SIZE;
					while (end_pos > stop) {
						end_pos -= *end_iter++;
					}

					if (pos - end_pos >= SEARCH_MAX_DIST)
						break;
						*/
				}

			} while (pos - begin_pos < SEARCH_MAX_DIST);

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
	}

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

