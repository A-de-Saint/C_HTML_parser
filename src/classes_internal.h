#ifndef CLASSES_INTERNAL_H
#define CLASSES_INTERNAL_H

#include "html_parser_internal.h"
#include "util.h"

//adds an entry to class list
bool class_list_add(class_list_t *list, char *name);

//returns class ID number, <0 if not found
int find_class_num(class_list_t *list, const char *name);

//frees class list
static inline void class_list_free(class_list_t *list)
{
    check_and_free(list->data);
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

#endif
