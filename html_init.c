#include <stdlib.h>
#include "html_parser.h"
#include "html_parser_internal.h"
#include "elements_internal.h"

//initial size of class list array
#define CLASS_LIST_INIT_SIZE 128

//initializes class list
bool class_list_init(class_list_t *dst)
{
	dst->data = malloc(CLASS_LIST_INIT_SIZE * sizeof(char*));
	if (!dst->data)
		return false;
	dst->size = 0;
	dst->capacity = CLASS_LIST_INIT_SIZE;
	return true;
}

//resizes class list to double its previous capacity
bool class_list_resize(class_list_t *list)
{
	char **tmp = realloc(list->data, list->capacity * 2 * sizeof(char *));
	if (!tmp)
		return false;
	list->data = tmp;
	list->capacity *= 2;
	return true;
}

//frees class list and nullifies it
void class_list_free(class_list_t *list)
{
	if (list->data)
	{
		for (unsigned i = 0; i < list->size; i++)
		{
			if (list->data[i])
				free(list->data[i]);
		}
		free(list->data);	//SUS
	}
	list->data = NULL;
	list->capacity = 0;
	list->size = 0;
}

//html tree initialization
//allocs class list and creates a document element (fist element, parent to all)
//returns NULL upon failure
html_tree_t *html_tree_init(void)
{	
	//alloc tree itself
	html_tree_t *tree = malloc(sizeof(html_tree_t));
	if (!tree)
		return NULL;

	//alloc class_list
	if (!class_list_init(&tree->classes))
	{
		free(tree);
		return NULL;
	}

	html_element_t *doc_elem = element_init(NODE_ELEMENT, NULL);
	if (!doc_elem)
	{
		class_list_free(&tree->classes);
		free(tree);
		return NULL;
	}
	tree->first_element = doc_elem;
	return tree;
}
