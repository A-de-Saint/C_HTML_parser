#include "html_parser.h"
#include "html_parser_internal.h"
#include "elements_internal.h"
#include "classes_internal.h"
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

//states for reading attributes
typedef enum {
	DEFAULT,		//default (whitespace or in-between)
	READING_NAME,	//reading attribute name
	EXP_EQ,			//expect '='
	QUOTES_OR_NOT,	//before reading attribute value; determine if quotes or not
	READING_ATTR_Q,	//reading quoted attribute
	READING_ATTR_NQ	//reading unquoted attribute
} elem_attr_states_t;

//states for special cases when reading attributes
typedef enum {
	NONE,			//no special case (for the purposes of this parser)
	ID,				//reading id
	CLASS			//reading class(es)
} elem_spec_cases_t;

const char *special_case_strs[2] = {"id", "class"};

static inline elem_spec_cases_t determine_special_case(string_t *attr_name)
{
	if (string_chararr_strcmp(attr_name, special_case_strs[0]) == 0)
		return ID;
	if (string_chararr_strcmp(attr_name, special_case_strs[1]) == 0)
		return CLASS;
	return NONE;
}

//writes id to element
bool write_id_to_element(html_element_t *elem, string_t *id_name)
{
	elem->properties.id = malloc((id_name->length + 1) * sizeof(char));
	if (!elem->properties.id)
		return false;
	strncpy(elem->properties.id, id_name->data, id_name->length);
	elem->properties.id[id_name->length] = '\0';
	return true;
}

//adds class to classlist and writes its id to element
bool write_class_to_element(html_element_t *elem, string_t *class_name, html_tree_t *tree)
{
	//find class id
	size_t i = 0;
	for ( ; i < tree->classes.size; i++)
	{
		if (string_chararr_strcmp(class_name, tree->classes.data[i]) == 0)
			goto found;
	}

	//if here, class not found
	//copy classname and add to classlist
	char *classlist_entry = malloc((class_name->length + 1) * sizeof(char));
	if (!classlist_entry)
		return false;
	if (!class_list_add(&tree->classes, classlist_entry))
	{
		free(classlist_entry);
		return false;
	}

  found:
	//if found, add index to element
	if (elem->properties.class_count >= elem->properties.class_capacity)
	{
		//need to realloc
		size_t *tmp = realloc(elem->properties.class_ids, elem->properties.class_capacity * 2);
		if (!tmp)
			return false;
		elem->properties.class_capacity *= 2;
	}
	elem->properties.class_ids[elem->properties.class_count++] = i;
	return true;
}

//adds attr_name and attr_val to other_attr in this format:
//other_attr += attr_name="attr_val"
bool add_to_other_attr(string_t *other_attr, string_t *attr_name, string_t *attr_val)
{
	//make space from previous attributes
	if (other_attr->length > 0)
	{
		if (!string_putchar(other_attr, ' '))
			return false;
	}

	//add attribute name
	if (!string_append_string(other_attr, attr_name))
		return false;

	//if no value (bool attribute), all is okay
	if (!attr_val || attr_val->length == 0)
		return true;
	
	//add `="attr_val"`
	if (!string_putchar(other_attr, '=')			||
		!string_putchar(other_attr, '"')			||
		!string_append_string(other_attr, attr_val)	||
		!string_putchar(other_attr, '"'))
	{
		return false;
	}

	return true;
}

const char unquoted_invalid_attr_chars[] = {
	'"',
	'\'',
	'`',
	'=',
	'<'
};

//tries to find element in tagged elements and return its tag
//if not found, returns TAG_OTHER
tag_t find_element_tag(char *elem_name)
{
	for (unsigned i = 0; i < TAG_COUNT; i++)
	{
		if (strcmp(elem_name, tag_names[i]) == 0)
			return (tag_t)i;
	}
	return TAG_OTHER;
}

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

//assigns element name or element tag
//resets elem_name string
//if not void, adds element to stack
//if false, alloc_error (cleans own mess)
bool finalize_element_name(html_element_t *curr_elem, string_t *elem_name, element_stack_t *stack)
{
	//end string
	if (!string_putchar(elem_name, '\0'))
	{
		return false;
	}

	//try to find element's tag
	curr_elem->properties.tag = find_element_tag(elem_name->data);
	if (curr_elem->properties.tag == TAG_OTHER)
	{
		//allocate new string						//no need for +1, because '\0' is a part of the string
		curr_elem->properties.element_name = malloc(elem_name->length * sizeof(char));
		if (!curr_elem->properties.element_name)
		{
			return false;
		}

		//copy the name
		strncpy(curr_elem->properties.element_name, elem_name->data, elem_name->length);
	}
	else
		curr_elem->properties.element_name = NULL;

	//reset element name string
	elem_name->length = 0;		//no need to allocate new string, since the old one was copied

	//link element (add to tree)
	link_element(curr_elem, element_stack_peek(stack));

	//only add to stack if not a void element (<br>, <img>...)
	if (!is_void_element(curr_elem))
	{
		if (!element_stack_push(stack, curr_elem))
		{
			return false;
		}
	}
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
	
	//string for storing element name that is currently being read
	//does not get freed until the end of the FSM
	string_t elem_name;
	if (!string_init(&elem_name, 32))
	{
		element_stack_free(&stack);
		return 2;
	}

	//boolean that notifies that whitespace has been read
	bool trailing_whitespace = false;

	//line counter
	//will be useful for error reports
	size_t line_count = 1;

	//finally, the FSM
	while (*raw_html != '\0')
	{
		//keep track of which line it is
		if (*raw_html == '\n')
			line_count++;

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
				/* CASE "<c" - reading element name */
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
					state = DOCTYPE;
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
			//doctype-like tokens also get discarded
			case DOCTYPE:
				if (*raw_html == '>')
					state = TEXT;
				break;

			/* CASE COMMENT */
			case COMMENT:
				/* CASE '-' */
				if (*raw_html == '-')
				{
					state = END_DASH_READ;
				}
				/* case anything else - continue comment */
				else
				{
					if (!element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}
				}
				break;

			/* CASE "comment-" */
			case END_DASH_READ:
				/* CASE "--" */
				if (*raw_html == '-')
				{
					state = END_TWODASH_READ;
				}
				/* case anything else - return to comment */
				else 
				{
					if (!element_putchar(curr_elem, '-')	||
						!element_putchar(curr_elem, *raw_html))
					{
						goto alloc_err;
					}
					state = COMMENT;	//go back to comment state
				}

				break;

			/* CASE "comment--" */
			case END_TWODASH_READ:
				/* CASE "-->" - end of comment */
				if (*raw_html == '>')
				{
					//link comment, reset curr_elem
					link_element(curr_elem, element_stack_peek(&stack));
					curr_elem = NULL;
					state = TEXT;		//switch back to TEXT
				}
				/* CASE "---" -> just continue twodash (still two dashes at the end) */
				else if (*raw_html == '-')
				{
					//one '-' needs to be added, since only the two "--" at the end count
					if (!element_putchar(curr_elem, '-'))
					{
						goto alloc_err;
					}
				}
				/* case anything else - go back to reading comment */
				else 
				{
					if (!element_putchar(curr_elem, '-') ||
						!element_putchar(curr_elem, '-'))
					{
						goto alloc_err;
					}
					state = COMMENT;
				}
				break;

			/* CASE ELEMENT_READ - reading element name */
			case ELEM_NAME:
				/* CASE END OF ELEMENT ('>') */
				if (*raw_html == '>')
				{
					if (!finalize_element_name(curr_elem, &elem_name, &stack))
						goto alloc_err;
					
					//reset curr_element (nothing to add to this one)
					curr_elem = NULL;
					
					//reset to default TEXT state	
					state = TEXT;
				}
				/* CASE err (improperly closed tag) */
				else if (*raw_html == '<')
				{
					//TODO error recovery
				}
				/* CASE WHITESPACE - move to attribute reading */
				else if (isspace(*raw_html))
				{
					if (!finalize_element_name(curr_elem, &elem_name, &stack))
						goto alloc_err;

					//leave curr_elem still on the table
					state = ELEM_PROPERTIES;
				}
				/* CASE reading element name */
				else
				{
					//add char (convert to lowercase)
					if (!string_putchar(&elem_name, convert_to_lowercase(*raw_html)))
						goto alloc_err;
				}
				break;

			/* CASE END OF ELEMENT ("</") */
			case ELEM_END:
				/* CASE end of ELEM_END */
				if (*raw_html == '>')
				{
					trailing_whitespace = false;

					if (elem_name.length == 0)
					{
						//TODO some error recovery for "</>"
						break;
					}

					html_element_t *pop_elem = NULL;
					while ((pop_elem = element_stack_pop(&stack)) != NULL)
					{
						if (string_charrar_strcmp(&elem_name, pop_elem->properties.element_name) != 0)
						{
							//TODO some error recovery for popping element that does not match the element name
							continue;
						}
					}

					//if pop_elem was not found, invalid element end
					if (!pop_elem)
					{
						//TODO some error report
						//I also have to do some error recovery, since the stack is now empty
						//since the stack does not erase the data, maybe just backup the stack size and reset it
						//such as stack_size_backup = stack.size -> if err, stack.size = stack_size_backup
						break;
					}
				}
				/* CASE reading whitespace */
				else if (isspace(*raw_html))
				{
					//ignore whitespace but notify
					trailing_whitespace = true;
				}
				/* CASE invalid char ('<') */
				else if (*raw_html == '<')
				{
					//TODO some error recovery
				}
				/* CASE reading char */
				else
				{
					if (trailing_whitespace)
					{
						//TODO some error recovery for stuff like </div smth
					}
					if (!string_putchar(&elem_name, *raw_html))
					{
						goto alloc_err;
					}
				}
				break;

			/* CASE ELEM_PROPERTIES */
			//this one is basically it's own space (moving raw_html forth without breaking)
			case ELEM_PROPERTIES:
				//TODO maybe just alloc the buffers once, not everytime attributes are read
				string_t attr_name;
				string_t attr_val;
				string_t other_attr;

				//alloc strings
				if (!string_init(&attr_name, 16) ||
					!string_init(&attr_val, 32)  ||
					!string_init(&other_attr, 128));
				{
					//TODO
					goto alloc_err;
				}
				
				//enum instances (states)
				elem_attr_states_t attr_state = DEFAULT;
				elem_spec_cases_t spec_case = NONE;

				char curr_quote = '\0';

				//inner loop
				while (*raw_html != '\0')
				{	
					//check if element is ending
					if (*raw_html == '>')
					{
						//check current state
						if (attr_name.length > 0)
						{
							if (attr_val.length > 0)
							{
								if (attr_state == READING_ATTR_Q)
								{
									//TODO report error - unproperly ended quotes
								}
								//TODO
								else if (spec_case == ID)
								{
									if (!write_id_to_element(curr_elem, &attr_val))
										goto alloc_err;
								}
								else if (spec_case == CLASS)
								{
									if (!write_class_to_element(curr_elem, &attr_val, dst))
										goto alloc_err;
								}
								else
								{
									if (!add_to_other_attr(&other_attr, &attr_name, &attr_val))
										goto alloc_err;
								}
							}
							else
							{
								spec_case = determine_special_case(&attr_name);
								if (spec_case != NONE)
								{
									//TODO report error - valueless id or class attribute
								}
								//add other attribute (valueless)
								else if (!add_to_other_attr(&other_attr, &attr_name, NULL))
									goto alloc_err;
							}
						}

						//TODO add other attributes (if existing) to element
						
						string_free(&attr_name);
						string_free(&attr_val);
						string_free(&other_attr);

						break;
					}


				}
				
		}		

		//end of loop
		raw_html++;
	}

  alloc_err:
	//TODO

}









