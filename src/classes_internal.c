#include "classes_internal.h"
#include "util.h"

bool class_list_add(class_list_t *list, const char *name)
{
	if (list->size >= list->capacity)
		if (!class_list_resize(list))
			return false;

	list->data[list->size++] = name;
	return true;
}

int find_class_num(class_list_t *list, const char *name)
{
	for (size_t i = 0; i < list->size; i++)
	{
		if (strcmp(list->data[i], name) != 0)
			continue;
		return i;
	}
}
