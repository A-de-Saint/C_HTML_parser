#include "classes_internal.h"
#include "util.h"

static inline bool class_list_resize(class_list_t *list)
{
	char **tmp = realloc(list->data, (list->capacity * 2) * sizeof(char *));
	if (!tmp)
		return false;
	list->data = tmp;
	return true;
}

bool class_list_add(class_list_t *list, char *name)
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
	return -1;
}
