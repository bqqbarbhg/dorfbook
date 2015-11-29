### Three objects
> {a}, {b} and {c}

	a +a_pos
	a +ab_pos +ab_pos_2 -ab_neg b
	a -a_neg

	b +b_pos -b_neg
	b +ba_pos a

	a +ac_pos c
	a -ab_neg_2 b

	b -ba_neg a

	->

	a +a_add
	a +ab_add -ab_remove b
	c +c_add -c_remove
	a -a_remove

