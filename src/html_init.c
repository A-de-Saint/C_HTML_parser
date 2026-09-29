#include <stdlib.h>
#include "html_parser_internal.h"
#include "elements_internal.h"
#include "classes_internal.h"

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

//TODO resolve (duplicate in classes_internal)
#ifdef oeqfbeibfqfenfoq
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
#endif

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

//recursive function, which frees all siblings and their children
void free_all_siblings(html_element_t *first_child)
{
	while (first_child != NULL)
	{
		//free all that was allocated
		element_free_data(first_child);
		if (first_child->first_child != NULL)
			free_all_siblings(first_child->first_child);
		html_element_t *next_tmp = first_child->next_sibling;
		free(first_child);	//free element itself
		first_child = next_tmp;
	}
}

void html_tree_free(html_tree_t *tree)
{
	if (!tree)
		return;

	//call the recursive function on first child
	free_all_siblings(tree->first_element->first_child);
	free(tree->first_element);

	//free class list
	class_list_free(&tree->classes);

	//free the tree itself
	free(tree);
}
