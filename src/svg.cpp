
struct Id_To_XML_Node
{
	Interned_String id;
	XML_Node *node;
};
LIST_STRUCT(Id_To_XML_Node);

int compare_id_to_xml(const void *av, const void *bv)
{
	Id_To_XML_Node *a = (Id_To_XML_Node*)av;
	Id_To_XML_Node *b = (Id_To_XML_Node*)bv;

	ptrdiff_t diff = a->id.string.data - b->id.string.data;
	if (diff < 0) return -1;
	if (diff > 0) return 1;
	return 0;
}

int compare_id_key_to_xml(const void *keyv, const void *elemv)
{
	Interned_String *key = (Interned_String*)keyv;
	Id_To_XML_Node *elem = (Id_To_XML_Node*)elemv;

	ptrdiff_t diff = key->string.data - elem->id.string.data;
	if (diff < 0) return -1;
	if (diff > 0) return 1;
	return 0;
}

struct SVG_XML
{
	String_Table id_table;
	XML xml;
	Id_To_XML_Node_List ids;
};

void svg_walk_xml(SVG_XML *svg, XML_Node *node, Interned_String id_key)
{
	for (U32 i = 0; i < node->attribute_count; i++) {
		XML_Attribute attrib = node->attributes[i];

		if (attrib.key == id_key) {
			Id_To_XML_Node id_to_xml;
			id_to_xml.id = intern(&svg->id_table, attrib.value);
			id_to_xml.node = node;

			list_push(&svg->ids, &id_to_xml);
			break;
		}
	}

	XML_Node *child = node->children;
	while (child) {
		svg_walk_xml(svg, child, id_key);
		child = child->next;
	}
}

void initialize_id_list(SVG_XML *svg)
{
	Interned_String id_key = intern(&svg->xml.string_table, c_string("id"));
	svg_walk_xml(svg, svg->xml.root, id_key);

	qsort(svg->ids.data, svg->ids.count, sizeof(*svg->ids.data), compare_id_to_xml);
}

XML_Node *svg_find_by_id(SVG_XML *svg, String id)
{
	Interned_String id_key;
	if (!intern_if_not_new(&id_key, &svg->id_table, id))
		return 0;

	Id_To_XML_Node *id_to_xml = (Id_To_XML_Node*)bsearch(&id_key,
		svg->ids.data, svg->ids.count, sizeof(*svg->ids.data),
		compare_id_key_to_xml);

	assert(id_to_xml && id_to_xml->node);

	return id_to_xml->node;
}

