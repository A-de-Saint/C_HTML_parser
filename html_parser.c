#include "html_parser.h"
#include "html_parser_internal.h"
#include "elements_internal.h"
#include <ctype.h>

typedef enum {
	TEXT,
	LT_READ,
	ELEM_NAME,
	ELEM_PROPERTIES,
	ELEM_END,
	EXCLAM_READ,
	DOCTYPE,
	START_DASH_READ,
	COMMENT,
	END_DASH_READ,
	END_TWODASH_READ,
	BOGUS_COMMENT
} states_t;

bool is_void_element(html_element_t *elem)
{
	//just in case
	if (elem->type != NODE_ELEMENT)
		return false;
	
	switch(elem->properties.tag)
	{
		case TAG_OTHER:
			break;			//untagged, need to strcmp
		case TAG_BR:
		case TAG_COL:
		case TAG_HR:
		case TAG_IMG:
		case TAG_LINK:
		case TAG_META:
		case TAG_SOURCE:
			return true;	//if any of these, true
		default:
			return false;	//if tagged but not one of them, false
	}

	char *elem_name = elem->properties.element_name;
	if (strcmp(elem_name, "area") == 0 ||
		strcmp(elem_name, "base") == 0 ||
		strcmp(elem_name, "embed") == 0 ||
		strcmp(elem_name, "input") == 0 ||
		strcmp(elem_name, "track") == 0 ||
		strcmp(elem_name, "wbr") == 0)
	{
		return true;	//if any of untagged void elements, true
	}
	return false;		//else false
}

/* HELPER STACK FOR ELEMENTS */
//TODO move to separate module

#define ELEMENT_STACK_START_CAPACITY 128

typedef struct element_stack {
	html_element_t **data;
	size_t size;
	size_t capacity;
} element_stack_t;

bool element_stack_init(element_stack_t *dst)
{
	dst->data = malloc(ELEMENT_STACK_START_CAPACITY * sizeof(html_element_t *));
	if (!dst->data)
		return false;
	dst->size = 0;
	dst->capacity = ELEMENT_STACK_START_CAPACITY;
	return true;
}

bool element_stack_push(element_stack_t *stack, html_element_t *item)
{
	if (stack->size >= stack->capacity)
	{
		html_element_t **tmp = realloc(stack->data, stack->capacity * 2);
		if (!tmp)
			return false;
		stack->data = tmp;
		stack->capacity *= 2;
	}
	stack->data[stack->size++] = item;
	return true;
}

static inline html_element_t *element_stack_pop(element_stack_t *stack)
{
	if (stack->size > 0)
		return stack->data[--(stack->size)];
	else return NULL;
}

static inline html_element_t *element_stack_peek(element_stack_t *stack)
{
	if (stack->size > 0)
		return stack->data[stack->size - 1];
	else return NULL;
}

void element_stack_free(element_stack_t *stack)
{
	if (stack->data)
		free(stack->data);
	stack->data = NULL;
	stack->size = 0;
	stack->capacity = 0;
}
/////////////////

/* HELPER FUNCTIONS */

//links element to parent (either as a first child or at the end of sibling linked list)
//fills element's parent field
void link_element(html_element_t *elem, html_element_t *parent)
{
	if (!elem || !parent)
		return;

	//assign parent
	elem->parent = parent;

	//if no first child, assign as that
	if (parent->first_child == NULL)
	{
		parent->first_child = elem;
		return;
	}
	
	//go through the linked list
	html_element_t *curr_child = parent->first_child;
	while (curr_child->next_sibling != NULL);
	curr_child->next_sibling = elem;
}

/* FSM */

//1 - NULL input
//2 - allocation failure
//3 - invalid HTML
int parse_html(const char *raw_html, html_tree_t *dst)
{
	//NULL checks
	if (!raw_html || !dst)
		return 1;
	
	//initial state
	states_t state = TEXT;

	//stack for parents
	element_stack_t stack;
	if (!element_stack_init(&stack))
		return 2;

	//push first parent (document)
	//no need to check for return value, since it can't fail now (no way of exceeding capacity)
	element_stack_push(&stack, dst->first_element);

	//the element that is currently being worked on
	html_element_t *curr_elem = NULL;
	
	
	string_t elem_name;
	if (!string_init(&elem_name, 32))
	{
		element_stack_free(&stack);
		return 2;
	}

	//finally, the FSM
	while (*raw_html != '\0')
	{
		switch (state)
		{
			/* TEXT */
			case TEXT:
				/* case '<' -> move to LT_READ */
				if (*raw_html == '<')
				{	
					state = LT_READ;

					//TODO remove (for error handling)
					//check for TEXT read before
					if (curr_elem)
					{
						link_element(curr_elem, element_stack_peek(&stack));	//if text was found, link
						curr_elem = NULL;
					}
				}

				/* case anyting else -> write character to a text element (create if necessary) */
				else 
				{
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							goto alloc_err;
						}
					}
					if (!element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}
				}

				break;

			/* '<' READ */
			case LT_READ:
				/* CASE "<!" */
				if (*raw_html == '!')
				{
					state = EXCLAM_READ;
				}
				/* CASE "<?" */
				else if (*raw_html == '?')
				{	
					//will be discarded
					state = BOGUS_COMMENT;
				}
				/* CASE "</" */
				else if (*raw_html == "/")
				{
					state = ELEM_END;
				}
				/* CASE "< " */
				else if (isspace(*raw_html))
				{
					//TODO could be put in a func
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							goto alloc_err;
						}
					}
					if (!element_putchar(curr_elem, '<') || !element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}

					state = TEXT;
				}
				/* CASE "<c" */
				else
				{	
					//link TEXT element
					if (curr_elem)
					{
						link_element(curr_elem, element_stack_peek(&stack));	//if text was found, link
					}

					//create element node
					curr_elem = element_init(NODE_ELEMENT, element_stack_peek(&stack));
					if (!curr_elem)
					{
						goto alloc_err;
					}

					//start reading name into string dedicated to it
					state = ELEM_NAME;
					if (!string_putchar(&elem_name, *raw_html))
						goto alloc_err;
				}

				break;

			/* CASE "<!" */
			case EXCLAM_READ:
				/* CASE "<!-" */
				if (*raw_html == '-')
				{
					state = START_DASH_READ;
				}
				/* CASE "<! " */
				else if (isspace(*raw_html))
				{
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							goto alloc_err;
						}
					}

					//write "<! " into TEXT element
					if (!element_putchar(curr_elem, '<')		||
						!element_putchar(curr_elem, '!')		||
						!element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}

					state = TEXT;
				}
				/* CASE "<!c" */
				else
				{
					state = DOCTYPE; //TODO
				}

				break;

			/* CASE "<!-" */
			case START_DASH_READ:
				/* CASE "<!--" */
				if (*raw_html == '-')
				{
					//link text element
					if (curr_elem)
					{
						link_element(curr_elem, element_stack_peek(&stack));
					}

					//create comment node
					curr_elem = element_init(NODE_COMMENT, element_stack_peek(&stack));
					if (!curr_elem)
						goto alloc_err;

					state = COMMENT;
				}
				/* CASE "<!-c" */
				else //return to TEXT
				{
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							goto alloc_err;
						}
					}

					//write "<!-c" into TEXT element
					if (!element_putchar(curr_elem, '<')		||
						!element_putchar(curr_elem, '!')		||
						!element_putchar(curr_elem, '-')		||
						!element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}

					state = TEXT;
				}

				break;

			/* CASE BOGUS COMMENT */
			//bogus comments are discarded for this design, as if they never existed
			case BOGUS_COMMENT:
				if (*raw_html == '>')
					state = TEXT;
				break;

			/* CASE DOCTYPE */
			//doctype-like tokes also get discarded
			case DOCTYPE:
				if (*raw_html == '>')
					state = TEXT;
				break;

			//TODO
		}

		//end of loop
		raw_html++;
	}

  alloc_err:
	//TODO

}









