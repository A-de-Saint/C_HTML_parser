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

	//initialize child and sibling to NULL
	elem->first_child = NULL;
	elem->next_sibling = NULL;
	
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
	else //dealing with NODE_ELEMENT
	{
		//init values
		elem->properties.tag = TAG_OTHER;
		elem->properties.class_ids = NULL;
		elem->properties.class_count = 0;
		elem->properties.class_capacity = 0;
		elem->properties.id = NULL;
		elem->properties.element_name = NULL;
		elem->properties.other_attributes = NULL;
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

bool element_create_classlist(html_element_t *elem, size_t initial_capacity)
{
	elem->properties.class_ids = malloc(initial_capacity * sizeof(size_t));
	if (elem->properties.class_ids == NULL)
		return false;
	elem->properties.class_capacity = initial_capacity;
	elem->properties.class_count = 0;
	return true;
}



