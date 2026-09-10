#ifndef HTML_PARSER_UTIL_H
#define HTML_PARSER_UTIL_H

#include <stdlib.h>
#include <stdbool.h>

#define check_and_free(pointer)	\
	do {						\
		if(pointer) {			\
			free(pointer);		\
		}						\
	} while (0)					\

typedef struct string_view {
	char *data;
	size_t length;
	size_t capacity;
} string_t;

//allocs and preps a string
//does not NULL check
bool string_init(string_t *str, size_t init_capacity);

//doubles string capacity
//does not NULL check
bool string_resize(string_t *str);

//compares string_t to char array
int string_chararr_strcmp(string_t *str, char *strc);

//compares string_t and string_t
int string_string_strcmp(string_t *str1, string_t *str2);

//basically strcat for string_t's
bool string_append_string(string_t *dst, string_t *src);

//puts a single char into str
static inline bool string_putchar(string_t *str, char ch)
{
	if (str->length >= str->capacity)
	{
		if (!string_resize(str))
			return false;
	}
	str->data[str->length++] = ch;
	return true;
}

//.data->free()->NULL; length,capacity->0
static inline void string_free(string_t *str)
{
	if (str->data)
		free(str->data);
	str->data = NULL;
	str->length = 0;
	str->capacity = 0;
}

//returns true if strings are definitely different, else false
//false does NOT necessarily mean they're the same
static inline bool string_string_are_different(string_t *str1, string_t *str2)
{
	return str1->length == str2->length;
}

static inline char convert_to_lowercase(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
		ch += 'A' - 'a';
	return ch;
}

#endif
