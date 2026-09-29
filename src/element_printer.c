#include "html_parser_internal.h"
#include <stdio.h>

void print_other_element_whereabouts(html_element_t *elem, const char *family_relationship)
{
	printf("%s: ", family_relationship);
	if (elem->type != NODE_ELEMENT)
	{
		if (elem->type == NODE_COMMENT)
			printf("COMMENT");
		else
			printf("TEXT");

		if (elem->text.length > 0)
		{
			printf(" - ");
			//determine how many characters to print
			bool longer = elem->text.length <= 10;
			size_t to_print = longer ?
				elem->text.length :
				10;

			if (elem->type == NODE_COMMENT)
				printf("<--");
			else
				putchar('"');

			for (size_t i = 0; i < to_print; i++)
			{
				putchar(elem->text.data[i]);
			}

			if (longer)
				printf("...");
			else
			{
				if (elem->type == NODE_COMMENT)
					printf("-->");
			}

			if (elem->type == NODE_TEXT)
				putchar('"');
		}
		putchar('\n');
	}
	else
	{
		if (elem->properties.element_name == NULL && elem->properties.tag == TAG_OTHER)
		{
			printf("DOCUMENT\n");
			return;
		}
		if (elem->properties.element_name != NULL || elem->properties.tag != TAG_OTHER)
		{
			putchar('<');
			if (elem->properties.tag == TAG_OTHER)
					printf("%s", elem->properties.element_name);
			else
				printf("%s", tag_names[elem->properties.tag]);

			if (elem->properties.id != NULL)
				printf(" id=\"%s\"", elem->properties.id);

			putchar('>');
		}
	}
	putchar('\n');
}

void print_element(html_element_t *elem, html_tree_t *tree, bool print_family_info)
{
	if (!elem)
		return;

	printf("----------------\n");

	if (elem->parent == NULL)
	{
		printf("Element: DOCUMENT\n");
		goto print_child;
	}

	if (print_family_info)
	{
		print_other_element_whereabouts(elem->parent, "Child of");
	}

	printf("Type: ");
	if (elem->type == NODE_ELEMENT)
		printf("ELEMENT");
	else if (elem->type == NODE_TEXT)
		printf("TEXT");
	else
		printf("COMMENT");
	putchar('\n');

	if (elem->type == NODE_TEXT || elem->type == NODE_COMMENT)
	{
		printf("Content: ");
		if (elem->type == NODE_COMMENT)
			printf("<!--");
		else putchar('"');
		for (size_t i = 0; i < elem->text.length; i++)
			putchar(elem->text.data[i]);
		if (elem->type == NODE_COMMENT)
			printf("-->");
		else putchar('"');
		putchar('\n');
	}
	else 
	{
		printf("Tag: <");
		if (elem->properties.tag == TAG_OTHER && elem->properties.element_name != NULL)
			printf("%s", elem->properties.element_name);
		else if (elem->properties.element_name != NULL)
			printf("%s", tag_names[elem->properties.tag]);
		putchar('>');
		putchar('\n');

		if (elem->properties.id != NULL)
			printf("id: %s\n", elem->properties.id);

		if (elem->properties.class_count > 0 && tree != NULL)
		{
			printf("classes: ");
			size_t class_count = 0;
			const char **classes = get_element_classes(elem, tree, &class_count);
			if (classes != NULL)
			{
				for (size_t i = 0; i < class_count; i++)
				{
					if (i > 0)
						putchar(',');
					printf(" %s", classes[i]);
				}
			}
			putchar('\n');
		}

		if (elem->properties.other_attributes != NULL)
		{
			printf("other_attributes: %s\n", elem->properties.other_attributes);
		}
	}

	if (elem->next_sibling != NULL && print_family_info)
	{
		print_other_element_whereabouts(elem->next_sibling, "next_sibling");
	}

  print_child:
  	if (elem->first_child != NULL && print_family_info)
	{
		print_other_element_whereabouts(elem->first_child, "first_child:");
	}

	printf("----------------\n\n");
}

//recursive function
void print_self_and_siblings(html_element_t *curr_elem, html_tree_t *tree)
{
	while (curr_elem != NULL)
	{
		print_element(curr_elem, tree, true);
		if (curr_elem->first_child != NULL)
			print_self_and_siblings(curr_elem->first_child, tree);
		curr_elem = curr_elem->next_sibling;
	}
}

void print_html_tree(html_tree_t *tree)
{
	if (!tree)
		return;
	print_self_and_siblings(tree->first_element, tree);
}

