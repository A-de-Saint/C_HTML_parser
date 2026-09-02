#include "elements_internal.h"
#include "util.h"

html_element_t *element_init(node_type_t type, html_element_t *parent)
{
	html_element_t *elem = malloc(sizeof(html_element_t));
	if (!elem)
		return NULL;
	
	//assign known values
	elem->type = type;
	elem->parent = parent;
	
	//if TEXT, alloc string
	if (type == NODE_TEXT)
	{
		if (!string_init(&elem->text, TEXT_NODE_INIT_CAPACITY))
		{
			free(elem);
			return NULL;
		}
	} //if COMMENT, also alloc string (for comment content)
	else if (type == NODE_COMMENT)
	{
		if (!string_init(&elem->text, COMMENT_NODE_INIT_CAPACITY))
		{
			free(elem);
			return NULL;
		}
	}

	return elem;
}

void element_free_data(html_element_t *elem)
{
	if (elem->type == NODE_TEXT || elem->type == NODE_COMMENT)
	{
		string_free(&elem->text);
		return;
	}
	check_and_free(elem->properties.element_name);
	check_and_free(elem->properties.id);
	check_and_free(elem->properties.class_ids);
	check_and_free(elem->properties.other_attributes);
}






