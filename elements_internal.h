#ifndef ELEMENTS_INTERNAL_H
#define ELEMENTS_INTERNAL_H

#define TEXT_NODE_INIT_CAPACITY 128
#define COMMENT_NODE_INIT_CAPACITY 128

#include <stdlib.h>
#include <stdbool.h>
#include "html_parser.h"
#include "html_parser_internal.h"

//allocates new element and fills in only type and parent pointers
//if type == NODE_COMMENT or NODE_TEXT, allocs the text string
//does NOT allocate everything - to potentially save performance
html_element_t *element_init(node_type_t type, html_element_t *parent);

//frees element's data (properties or text)
void element_free_data(html_element_t *elem);

static inline void element_assign_first_child(html_element_t *elem, html_element_t *first_child)
{
	elem->first_child = first_child;
}

static inline void element_assign_sibling(html_element_t *elem, html_element_t *next_sibling)
{
	elem->next_sibling = next_sibling;
}

//puts a single character into the element's text
//does NOT check if an element is comment or text
static inline bool element_putchar(html_element_t *elem, char ch)
{
	if (elem->text.length >= elem->text.capacity)
	{
		if (!string_resize(&elem->text))
			return false;
	}
	elem->text.data[elem->text.length++] = ch;
	return true;
}

#endif
