#ifndef HTML_PARSER_INTERNAL_H
#define HTML_PARSER_INTERNAL_H

#include "html_parser.h"
#include "util.h"

typedef struct class_list {
	char **data;
	size_t size;
	size_t capacity;
} class_list_t;

//types of nodes
typedef enum {
	NODE_ELEMENT,	//element
	NODE_TEXT,		//text (tree leaf)
	NODE_COMMENT	//comment (same properties as text)
} node_type_t;

//enum for known elements (saves space and speed)
//ordered by most usual
typedef enum {
	//usual elements
	TAG_DIV = 0,
	TAG_SPAN,
	TAG_A,
	TAG_BR,
	TAG_IMG,
	TAG_LINK,
	TAG_META,
	TAG_P,
	TAG_BODY,
	TAG_HEAD,
	TAG_HTML,
	TAG_SOURCE,
	TAG_COL,
	TAG_HR,

	TAG_COUNT,
	TAG_OTHER
} tag_t;

//names of the tagged elements (indices match the enum number)
const char *tag_names[TAG_COUNT] = {
	[TAG_DIV] 		= "div",
	[TAG_SPAN] 		= "span",
	[TAG_A] 		= "a",
	[TAG_BR] 		= "br",
	[TAG_IMG] 		= "img",
	[TAG_LINK] 		= "link",
	[TAG_META] 		= "meta",
	[TAG_P] 		= "p",
	[TAG_BODY] 		= "body",
	[TAG_HEAD] 		= "head",
	[TAG_HTML] 		= "html",
	[TAG_SOURCE]	= "source",
	[TAG_COL] 		= "col",
	[TAG_HR]		= "hr"
};

//representation of an html element
struct html_element {
	node_type_t type;				//node type
	html_element_t *parent;			//reference to parent
	html_element_t *first_child;	//reference to first child
	html_element_t *next_sibling;	//reference to next sibling (like a linked list)

	//union, because either text or info is expected, never both
	union {
		struct {
			tag_t tag;					//enum number representing usual element types
			char *element_name;			//name for unusual element types - NULL if tag is not 'TAG_OTHER'
			char *id;					//id of the element
			unsigned class_count;		//number of classes
			unsigned class_capacity;	//capacity of class array
			unsigned *class_ids;		//ids of all classes this element is a member of
			char *other_attributes;		//other attributes in a single string
		} properties;

		string_t text;			//text of a text type node
	};
};

struct html_tree {
	class_list_t classes;			//list of classes - indices are class IDs
	html_element_t *first_element;	//pointer to first element
};

#endif
