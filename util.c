#include "util.h"

bool string_init(string_t *str, size_t init_capacity)
{
	str->data = malloc(init_capacity * sizeof(char));
	if (!str->data)
		return false;
	str->length = 0;
	str->capacity = init_capacity;
	return true;
}

bool string_resize(string_t *str)
{
	char *tmp = realloc(str->data, str->capacity * 2);
	if (!tmp)
		return false;
	str->data = tmp;
	str->capacity *= 2;
	return true;
}

int string_chararr_strcmp(string_t *str, char *strc)
{
	size_t i = 0;
	int diff_tmp;
	while (i < str->length)
	{
		if (strc[i] == '\0')
			return 1;
		diff_tmp = strc[i] - str->data[i];
		if (diff_tmp != 0)
			return diff_tmp;
		i++;
	}
	if (strc[i] == '\0')
		return 0;
	return 1;
}

int string_string_strcmp(string_t *str1, string_t *str2)
{
	int i = 0;
	bool same_len = str1->length == str2->length;
	int diff_tmp;
	while (true)
	{
		if (i >= str1->length)
		{
			if (same_len)
				return 0;
			else
				return 1;
		}
		if (i >= str2->length)
		{
			return -1;
		}

		diff_tmp = str2->data[i] - str1->data[i];
		if (diff_tmp != 0)
			return diff_tmp;
	}
}
