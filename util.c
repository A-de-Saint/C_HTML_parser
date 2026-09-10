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

bool string_append_string(string_t *dst, string_t *src)
{
	size_t req_cap = dst->length + src->length;
	if (dst->capacity <= req_cap)
	{
		char *tmp = realloc(dst->data, (req_cap * 2) * sizeof(char));
		if (!tmp)
			return false;
		dst->capacity = req_cap * 2;
	}
	for (size_t i = 0; i < src->length; i++)
	{
		dst->data[dst->length++] = src->data[i];
	}
	return true;
}