#include "html_parser.h"
#include "html_parser_internal.h"

typedef enum {
	TEXT,
	LT_READ,
	ELEM_NAME,
	ELEM_PROPERTIES,
	EXCLAM_READ,
	DOCTYPE,
	START_DASH_READ,
	COMMENT,
	END_DASH_READ,
	END_TWODASH_READ,
	BOGUS_COMMENT
} states_t;

bool is_void_element(html_element_t *elem)
{
	//just in case
	if (elem->type != NODE_ELEMENT)
		return false;
	
	switch(elem->properties.tag)
	{
		case TAG_OTHER:
			break;			//untagged, need to strcmp
		case TAG_BR:
		case TAG_COL:
		case TAG_HR:
		case TAG_IMG:
		case TAG_LINK:
		case TAG_META:
		case TAG_SOURCE:
			return true;	//if any of these, true
		default:
			return false;	//if tagged but not one of them, false
	}

	char *elem_name = elem->properties.element_name;
	if (strcmp(elem_name, "area") == 0 ||
		strcmp(elem_name, "base") == 0 ||
		strcmp(elem_name, "embed") == 0 ||
		strcmp(elem_name, "input") == 0 ||
		strcmp(elem_name, "track") == 0 ||
		strcmp(elem_name, "wbr") == 0)
	{
		return true;	//if any of untagged void elements, true
	}
	return false;		//else false
}

//1 - NULL input
//2 - invalid HTML
int parse_html(const char *raw_html, html_tree_t *dst)
{
	if (!raw_html || !dst)
		return 1;
	//TODO
}
