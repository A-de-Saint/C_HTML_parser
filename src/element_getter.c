#include "html_parser_internal.h"

html_element_t *get_document_element(html_tree_t *tree)
{
	return tree != NULL ?
		tree->first_element
		: NULL;
}

element_type_t get_element_type(html_element_t *elem)
{
	return (element_type_t)elem->type;
}

const char *get_element_name(html_element_t *elem)
{
	if (elem->type != NODE_ELEMENT)
		return NULL;
	if (elem->properties.tag == TAG_OTHER)
		return elem->properties.element_name;
	return tag_names[elem->properties.tag];
}

const char *get_element_id(html_element_t *elem)
{
	return elem->type == NODE_ELEMENT ?
		elem->properties.id :
		NULL;
}

const char **get_element_classes(html_element_t *elem, html_tree_t *tree, size_t *class_count)
{
	if (class_count)
		*class_count = elem->properties.class_count;

	if (elem->type != NODE_ELEMENT || elem->properties.class_count == 0)
		return NULL;
	
	const char **classes = malloc(elem->properties.class_count * sizeof(char *));
	if (!classes)
		return NULL;
	
	for (size_t i = 0; i < elem->properties.class_count; i++)
	{
		classes[i] = tree->classes.data[elem->properties.class_ids[i]];
	}

	return classes;
}

const char *get_element_other_attributes(html_element_t *elem)
{
	return elem->type == NODE_ELEMENT ?
		elem->properties.other_attributes :
		NULL;
}

html_element_t *get_element_parent(html_element_t *elem)
{
	return elem->parent;
}

html_element_t *get_element_first_child(html_element_t *elem)
{
	return elem->first_child;
}

html_element_t *get_element_next_sibling(html_element_t *elem)
{
	return elem->next_sibling;
}

