
struct Huffman_Code
{
	int code;
	int bits;
};

struct Huffman_Tree_Node
{
	int uses;
	union {
		struct {
			int left;
			int right;
		} branch;
		struct {
			int tag;
			int code_index;
		} leaf;
	};
};

inline Huffman_Tree_Node huffman_leaf(int uses, int code_index)
{
	Huffman_Tree_Node node;
	node.uses = uses;
	node.leaf.tag = -1;
	node.leaf.code_index = code_index;
	return node;
}

inline Huffman_Tree_Node huffman_branch(int uses, int left, int right)
{
	Huffman_Tree_Node node;
	node.uses = uses;
	node.branch.left = left;
	node.branch.right = right;
	return node;
}

inline bool huffman_is_leaf(Huffman_Tree_Node *node)
{
	return node->leaf.tag < 0;
}

inline int compare_huffman_tree_nodes(const void *a, const void *b)
{
	return ((Huffman_Tree_Node*)a)->uses - ((Huffman_Tree_Node*)b)->uses;
}

int canonical_huffman(Huffman_Code *out_codes, int *code_uses, int code_count, int max_length)
{
	Huffman_Tree_Node *tree_storage = (Huffman_Tree_Node*)malloc(code_count * 2 * sizeof(Huffman_Tree_Node));
	Huffman_Tree_Node *leaf_nodes = tree_storage;

	int last_code = 0;

	int leaf_count = 0;
	for (int i = 0; i < code_count; i++) {
		int uses = code_uses[i];
		if (uses > 0) {
			leaf_nodes[leaf_count++] = huffman_leaf(uses, i);
			last_code = i;
		}
	}

	// Zero out all the codes (since we walk only the ones that have uses)
	memset(out_codes, 0, sizeof(Huffman_Code) * code_count);

	if (leaf_count == 0) {
		return 0;
	}

	qsort(leaf_nodes, leaf_count, sizeof(*leaf_nodes), compare_huffman_tree_nodes);

	Huffman_Tree_Node *branch_nodes = tree_storage + leaf_count;
	int branch_count = 0;

	Huffman_Tree_Node *work_nodes = leaf_nodes;
	int work_set_count = leaf_count;
	while (work_set_count > 1) {

		int left_index = branch_count++;
		int right_index = branch_count++;

		Huffman_Tree_Node *left = &work_nodes[work_set_count - 2];
		Huffman_Tree_Node *right = &work_nodes[work_set_count - 1];

		int uses = left->uses + right->uses;

		branch_nodes[left_index] = *left;
		branch_nodes[right_index] = *right;

		// Pop the two last nodes, but add the new one
		work_set_count = work_set_count - 2 + 1;

		// Find the right position to insert so that the array stays sorted
		int insert_index = work_set_count;
		for (; insert_index > 0; insert_index--) {
			if (work_nodes[insert_index - 1].uses >= uses) {
				break;
			}
			work_nodes[insert_index] = work_nodes[insert_index - 1];
		}

		work_nodes[insert_index] = huffman_branch(uses, left_index, right_index);
	}

	int root_index = branch_count++;
	branch_nodes[root_index] = work_nodes[0];

	// Since the nodes are inserted in a way that newer ones always follow their
	// children we can just walk all the nodes backwards and set the children's
	// depths before we reach them.
	
	int *node_depth = (int*)malloc(sizeof(int) * branch_count);
	node_depth[root_index] = 1;

	for (int i = root_index; i >= 0; --i) {
		Huffman_Tree_Node *node = &branch_nodes[i];
		int depth = node_depth[i];

		if (huffman_is_leaf(node)) {
			out_codes[node->leaf.code_index].bits = depth;
		} else {
			node_depth[node->branch.left] = depth + 1;
			node_depth[node->branch.right] = depth + 1;
		}
	}

	free(node_depth);

	int max_code_bits = min(max_length + 1, code_count);

	int *bits_count = (int*)calloc(max_code_bits, sizeof(int));
	int *bits_code = (int*)malloc(max_code_bits * sizeof(int));

	for (int i = 0; i < code_count; i++) {
		int length = out_codes[i].bits;
		if (length > max_length) {
			// TODO: Deal with this.
			return -1;
		}
		if (length > 0) {
			bits_count[length]++;
		}
	}
	
	int code = 0;

	bits_code[0] = code;
	for (int i = 1; i < max_code_bits; i++) {
		code = (code + bits_count[i - 1]) << 1;
		bits_code[i] = code;
	}

	for (int i = 0; i < code_count; i++) {
		int bits = out_codes[i].bits;
		out_codes[i].code = bits_code[i];

		// Increment for next code of the same length
		bits_code[i]++;
	}

	free(bits_count);
	free(bits_code);
	free(tree_storage);

	return last_code + 1;
}

