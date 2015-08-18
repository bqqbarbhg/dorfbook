import xml.dom.minidom

xml_fixtures = [
	('<root><node arg="value">Content</node><!-- Comment --><empty /></root>', 'Simple XML'),
	("<other><inner arg='thing' second='woo' /></other>", 'Single quote arguments'),
	('<root attr="&#65;&#x61;Ao">&#65;&#x61;Ao</root>', 'Character entities'),
	('<root attr="&lt;&gt;&amp;&apos;&quot;">&lt;&gt;&amp;&apos;&quot;</root>', 'Predefined entities'),
]

class Dorf_XML_Node:
	def __init__(self, tag, ctx):
		self.tag = tag
		self.content = ''
		self.children = []
		self.attributes = []
		self.tag_id = ctx.tag_id
		ctx.tag_id += 1

class Dorf_XML_Context:
	def __init__(self):
		self.tag_id = 0

def parse_dorf_xml_node(it, ctx):
	node = Dorf_XML_Node(next(it), ctx)
	t = next(it)
	while t != '>':
		node.attributes.append((t, next(it)))
		t = next(it)
	t = next(it)
	if t == '#':
		node.content = next(it)
		t = next(it)
	while t == '<':
		node.children.append(parse_dorf_xml_node(it, ctx))
		t = next(it)
	if t == '/':
		return node
	raise IOError("Invalid Dorf XML format expected '/' got '%s'" % t)

def parse_dorf_xml(data):
	parts = data.split('\0')
	if not parts:
		return None
	it = iter(parts)
	t = next(it)
	if t == '<':
		return parse_dorf_xml_node(it, Dorf_XML_Context())
	else:
		return None

def py_children(py):
	return filter(lambda x: x.nodeType == xml.dom.Node.ELEMENT_NODE, py.childNodes)

def py_content(py):
	return filter(lambda x: x.nodeType == xml.dom.Node.TEXT_NODE, py.childNodes)[0].data

def xml_match(py, dorf, desc):
	ctx = desc + ', Node <%s> #%d' % (dorf.tag, dorf.tag_id + 1)
	t.check(py.tagName == dorf.tag, "Tag matches", ctx)
	if t.check(len(py.attributes) == len(dorf.attributes), "Correct number of attributes", ctx):
		for index, at in enumerate(dorf.attributes):
			val = py.attributes[at[0]].value
			t.check(val == at[1], "Attribute matches", ctx + ", attribute %d" % (index + 1))
	if dorf.content:
		content = py_content(py)
		t.check(content == dorf.content, "Content matches", ctx)
	children = py_children(py)
	if t.check(len(children) == len(dorf.children), "Correct number of children", ctx):
		for p,d in zip(children, dorf.children):
			xml_match(p, d, desc)

for data, desc in xml_fixtures:
	dorf_xml = parse_dorf_xml(test_call("xml", data))
	py_xml = xml.dom.minidom.parseString(data).documentElement
	if not dorf_xml:
		t.check(False, "Failed to parse XML", desc)
	else:
		xml_match(py_xml, dorf_xml, desc)


