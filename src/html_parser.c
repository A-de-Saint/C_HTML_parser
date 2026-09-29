#include "html_parser_internal.h"
#include "elements_internal.h"
#include "classes_internal.h"
#include <ctype.h>
#include <stdio.h>

//fallthrough attribute to stop gcc from complaining
#define FALLTHROUGH __attribute__((fallthrough));

//names of the tagged elements (indices match the enum number)
const char *tag_names[TAG_COUNT] = {
	[TAG_DIV] 		= "div",
	[TAG_SPAN] 		= "span",
	[TAG_A] 		= "a",
	[TAG_BR] 		= "br",
	[TAG_IMG] 		= "img",
	[TAG_LINK] 		= "link",
	[TAG_META] 		= "meta",
	[TAG_P] 		= "p",
	[TAG_BODY] 		= "body",
	[TAG_HEAD] 		= "head",
	[TAG_HTML] 		= "html",
	[TAG_SOURCE]	= "source",
	[TAG_COL] 		= "col",
	[TAG_HR]		= "hr"
};

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
	size_t i = 0;

	if (elem->properties.class_ids == NULL)
	{
		if (!element_create_classlist(elem, ELEMENT_INITIAL_CLASS_IDS_CAPACITY))
			return false;
		//going to not_found with i = 0; that's the index where the class will get added
		goto not_found;
	}

	//find class id
	for ( ; i < tree->classes.size; i++)
	{
		if (string_chararr_strcmp(class_name, tree->classes.data[i]) == 0)
			goto found;
	}

  not_found:
	//if here, class not found
	//copy classname and add to classlist
	char *classlist_entry = malloc((class_name->length + 1) * sizeof(char));
	if (!classlist_entry)
		return false;
	strncpy(classlist_entry, class_name->data, class_name->length);
	classlist_entry[class_name->length] = '\0';
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
	while (curr_child->next_sibling != NULL)
		curr_child = curr_child->next_sibling;
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

	return true;
}

/* FSM */

//1 - NULL input
//2 - allocation failure
//3 - invalid HTML
int parse_html(RAW_HTML_TYPE raw_html, html_tree_t *dst)
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
	bool curr_elem_linked = false;
	
	//string for storing element name that is currently being read
	//does not get freed until the end of the FSM
	string_t elem_name;
	if (!string_init(&elem_name, 32))
	{
		goto f_s;
	}

	//buffers for reading attributes
	string_t attr_name;
	string_t attr_val;
	string_t other_attr;

	if (!string_init(&attr_name, 16))
		goto f_ss;
	if (!string_init(&attr_val, 32))
		goto f_sss;
	if (!string_init(&other_attr, 128))
		goto f_ssss;

	//boolean that notifies that whitespace has been read
	bool trailing_whitespace = false;
	bool end_err_reported = false;

	//boolean that notifies that a self-closing slash has been read (the '/' from "/>")
	bool self_closing_slash = false;

	//line counter
	//will be useful for error reports
	size_t line_count = 1;
	size_t col_count = 0;

	//current character that is being read in the FSM
	char currchar;

	//finally, the FSM
	while ((currchar = RAW_HTML_GETCHAR(raw_html)) != RAW_HTML_EOF)
	{
		//keep track of which line and column it is
		if (currchar == '\n')
		{
			line_count++;
			col_count = 0;
		}
		else
			col_count++;

		/* DEBUG REPORT 
		printf("Char: %c\tState: %d\n", currchar, (int)state); */

		switch (state)
		{
			/* TEXT */
			case TEXT:
				/* case '<' -> move to LT_READ */
				if (currchar == '<')
				{	
					state = LT_READ;
				}

				/* case anyting else -> write character to a text element (create if necessary) */
				else 
				{
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							//nothing new to free
							goto alloc_err;
						}
						curr_elem_linked = false;
					}
					if (!element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}
				}

				break;

			/* '<' READ */
			case LT_READ:

				/* DEBUG 
					printf("got heree\n"); */
				
				/* CASE "<!" */
				if (currchar == '!')
				{
					state = EXCLAM_READ;
				}
				/* CASE "<?" */
				else if (currchar == '?')
				{	
					//link text element, if exists
					if (curr_elem != NULL && !curr_elem_linked)
					{
						link_element(curr_elem, element_stack_peek(&stack));
					}
					state = BOGUS_COMMENT;
				}
				/* CASE "</" */
				else if (currchar == '/')
				{
					//link element if exists
					if (curr_elem != NULL && !curr_elem_linked)
					{
						link_element(curr_elem, element_stack_peek(&stack));
						//leave it on the table for "</>" case
						curr_elem_linked = true;
					}

					//set attributes for state entry
					end_err_reported = false;
					trailing_whitespace = false;

					state = ELEM_END;
				}
				/* CASE "< " */
				else if (isspace(currchar))
				{
					//TODO could be put in a func
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							//nothing to free
							goto alloc_err;
						}
						curr_elem_linked = false;
					}
					if (!element_putchar(curr_elem, '<') || !element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}

					state = TEXT;
				}
				/* CASE "<c" - reading element name */
				else
				{	
					//link TEXT element
					if (curr_elem != NULL && !curr_elem_linked)
					{
						link_element(curr_elem, element_stack_peek(&stack));	//if text was found, link
					}

					//an element name must start with an ASCII letter - if not, error and ignore whole token
					if (!(currchar  >= 'a' && currchar <= 'z') && !(currchar <= 'A' && currchar >= 'Z'))
					{
						fprintf(stderr, "html_parser: Element token not starting with an ascii letter will be ignored (treated as a bogus comment). Line: %zu Col: %zu\n", line_count, col_count);
						curr_elem = NULL;
						curr_elem_linked = false;
						state = BOGUS_COMMENT;
					}

					//create element node
					curr_elem = element_init(NODE_ELEMENT, element_stack_peek(&stack));
					if (!curr_elem)
					{
						//nothing to free here
						goto alloc_err;
					}

					curr_elem_linked = false;

					//start reading name into string dedicated to it
					state = ELEM_NAME;

					//no need to check for return value, since if alloc was good, size cannot be greater than capacity
					string_putchar(&elem_name, currchar);

					/* DEBUG 
					printf("got here\n"); */
				}

				break;

			/* CASE "<!" */
			case EXCLAM_READ:
				/* CASE "<!-" */
				if (currchar == '-')
				{
					state = START_DASH_READ;
				}
				/* CASE "<! " */
				else if (isspace(currchar))
				{
					if (!curr_elem)
					{
						curr_elem = element_init(NODE_TEXT, element_stack_peek(&stack));
						if (!curr_elem)
						{
							//nothing to free here
							goto alloc_err;
						}
						curr_elem_linked = false;
					}

					//write "<! " into TEXT element
					if (!element_putchar(curr_elem, '<')		||
						!element_putchar(curr_elem, '!')		||
						!element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}

					state = TEXT;
				}
				/* CASE "<!c" */
				else
				{
					//link element if exists
					if (curr_elem != NULL && !curr_elem_linked)
					{
						link_element(curr_elem, element_stack_peek(&stack));
					}
					state = DOCTYPE;
				}

				break;

			/* CASE "<!-" */
			case START_DASH_READ:
				/* CASE "<!--" - reading comment */
				if (currchar == '-')
				{
					//link text element
					if (curr_elem != NULL && !curr_elem_linked)
					{
						link_element(curr_elem, element_stack_peek(&stack));
					}

					//create comment node
					curr_elem = element_init(NODE_COMMENT, element_stack_peek(&stack));
					if (!curr_elem)
						goto alloc_err;

					curr_elem_linked = false;

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
							//nothing to free
							goto alloc_err;
						}
					}

					//write "<!-c" into TEXT element
					if (!element_putchar(curr_elem, '<')		||
						!element_putchar(curr_elem, '!')		||
						!element_putchar(curr_elem, '-')		||
						!element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}

					state = TEXT;
				}

				break;

			//TODO optional bogus and doctype can be put in a while loop in-place, should save performance

			/* CASE BOGUS COMMENT */
			//bogus comments are discarded for this design, as if they never existed
			case BOGUS_COMMENT:
				if (currchar == '>')
					state = TEXT;
				break;

			/* CASE DOCTYPE */
			//doctype-like tokens also get discarded
			case DOCTYPE:
				if (currchar == '>')
					state = TEXT;
				break;

			/* CASE COMMENT */
			case COMMENT:
				/* CASE '-' */
				if (currchar == '-')
				{
					state = END_DASH_READ;
				}
				/* case anything else - continue comment */
				else
				{
					if (!element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}
				}
				break;

			/* CASE "comment-" */
			case END_DASH_READ:
				/* CASE "--" */
				if (currchar == '-')
				{
					state = END_TWODASH_READ;
				}
				/* case anything else - return to comment */
				else 
				{
					if (!element_putchar(curr_elem, '-')	||
						!element_putchar(curr_elem, currchar))
					{
						goto alloc_err;
					}
					state = COMMENT;	//go back to comment state
				}

				break;

			/* CASE "comment--" */
			case END_TWODASH_READ:
				/* CASE "-->" - end of comment */
				if (currchar == '>')
				{
					//link comment, reset curr_elem
					link_element(curr_elem, element_stack_peek(&stack));
					curr_elem = NULL;
					curr_elem_linked = false;
					state = TEXT;		//switch back to TEXT
				}
				/* CASE "---" -> just continue twodash (still two dashes at the end) */
				else if (currchar == '-')
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
				if (currchar == '>')
				{
					if (self_closing_slash)
					{
						fprintf(stderr, "html_parser: warn: self-closing syntax ignored, treating as normal element declaration. Line: %zu Col: %zu\n", line_count, col_count);
						self_closing_slash = false;
					}

					//links element
					if (!finalize_element_name(curr_elem, &elem_name, &stack))
					{
						goto alloc_err;
					}
					
					//reset curr_element (nothing to add to this one)
					curr_elem = NULL;
					curr_elem_linked = false;
					
					//reset to default TEXT state	
					state = TEXT;
				}
				/* CASE unexpected '/' before */
				else if (self_closing_slash)
				{
					fprintf(stderr, "html_parser: unexpected '/' in element name tag. Ignoring until '>'. Line: %zu Col: %zu\n", line_count, col_count);
					
					//still treat the part before '/' as an element
					//links element
					if (!finalize_element_name(curr_elem, &elem_name, &stack))
					{
						goto alloc_err;
					}
					
					//reset curr_element (nothing to add to this one)
					curr_elem = NULL;
					curr_elem_linked = false;

					self_closing_slash = false;	//reset
					state = BOGUS_COMMENT;		//to ignore until '>'
				}
				/* CASE slash - potentially self-closing */
				else if (currchar == '/')
				{
					self_closing_slash = true;
					break;
				}
				/* CASE err (improperly closed tag) */
				else if (currchar == '<')
				{
					fprintf(stderr, "html_parser: parse error: invalid '<' found. Will still get parsed into element name. Line: %zu Col: %zu\n", line_count, col_count);
					goto elem_name_putchar;
				}
				/* CASE WHITESPACE - move to attribute reading */
				else if (isspace(currchar))
				{
					//links element
					if (!finalize_element_name(curr_elem, &elem_name, &stack))
					{
						goto alloc_err;
					}
					
					//finalize links element
					curr_elem_linked = true;

					//leave curr_elem still on the table
					state = ELEM_PROPERTIES;

					//go directly to elem_properties (cannot increment now, since the next char might belong into the properties inner loop)
					goto elem_properties_noincr;
				}
				/* CASE reading element name */
				else
				{
				  elem_name_putchar:
					//add char (convert to lowercase)
					if (!string_putchar(&elem_name, convert_to_lowercase(currchar)))
					{
						goto alloc_err;
					}
				}
				break;

			/* CASE END OF ELEMENT ("</") */
			case ELEM_END:
				/* CASE end of ELEM_END */
				if (currchar == '>')
				{
					trailing_whitespace = false;

					//error recovery for "</>"
					if (elem_name.length == 0)
					{
						//TODO check - element_putchar might not go to a TEXT element
						fprintf(stderr, "html_parser: warn: invalid end tag, will be treated as text. Line: %zu Col: %zu\n", line_count, col_count);
						if (!element_putchar(curr_elem, '<') ||
							!element_putchar(curr_elem, '/') ||
							!element_putchar(curr_elem, '>'))
							goto alloc_err;
						state = TEXT;
						break;
					}

					html_element_t *pop_elem = NULL;
					size_t stack_size_backup = stack.size;
					while ((pop_elem = element_stack_pop(&stack)) != NULL)
					{
						const char *pop_elem_name = pop_elem->properties.element_name;
						//case element name not explicitly said, need to get it
						if (!pop_elem_name)
						{
							pop_elem_name = tag_names[pop_elem->properties.tag];
						}

						if (string_chararr_strcmp(&elem_name, pop_elem_name) != 0)
						{
							fprintf(stderr, "html_parser: warning: expected end tag for element: %s, but instead got '</", pop_elem_name);
							for (size_t i = 0; i < elem_name.length; i++)
								fputc(elem_name.data[i], stderr);
							fprintf(stderr, ">'. Line: %zu Col: %zu\n", line_count, col_count);
							continue;
						}
						else goto elem_end_end_end;
					}

					//if pop_elem was not found, invalid element end
					if (!pop_elem)
					{
						fprintf(stderr, "html_parser: warning: element end tag wihout element start tag: ");
						//fprintf elem_name
						for (size_t i = 0; i < elem_name.length; i++)
							fputc(elem_name.data[i], stderr);
						fprintf(stderr, " Line: %zu Col: %zu\n", line_count, col_count);

						//bring the stack back (invalid element case)
						stack.size = stack_size_backup;
					}
				  elem_end_end_end:

					//reset elem_name buffer and go to TEXT
					elem_name.length = 0;
					state = TEXT;
					break;

				}
				/* CASE reading whitespace */
				else if (isspace(currchar))
				{
					//ignore whitespace but notify the FSM
					trailing_whitespace = true;
				}
				/* CASE invalid char ('<') */
				else if (currchar == '<')
				{
					fprintf(stderr, "html_parser: parse error: invalid character '<'. Will still get parsed. Line: %zu Col: %zu\n", line_count, col_count);
					goto elem_end_putchar;
				}
				/* CASE reading char */
				else
				{
				  elem_end_putchar:
					if (trailing_whitespace)
					{	
						if (!end_err_reported)
						{
							fprintf(stderr, "html_parser: parse error: element end tag attribute found and will be ignored. Line: %zu Col: %zu\n", line_count, col_count);
							end_err_reported = true;
						}
					}
					if (!string_putchar(&elem_name, currchar))
					{
						goto alloc_err;
					}
				}
				break;

			/* CASE ELEM_PROPERTIES */
			//this one is basically it's own space (moving raw_html forth without breaking)
			case ELEM_PROPERTIES:
			  elem_properties_noincr:
				
				//enum instances (states)
				elem_attr_states_t attr_state = DEFAULT;
				elem_spec_cases_t spec_case = NONE;

				char curr_quote = '\0';

				//inner loop
				while ((currchar = RAW_HTML_GETCHAR(raw_html)) != RAW_HTML_EOF)
				{
					/* DEBUG INFO 
					printf("Char: %c\tInner state: %d\n", currchar, (int)attr_state); */

					//keep track of which line and column it is
					//need to do this again inside inner loop
					if (currchar == '\n')
					{
						line_count++;
						col_count = 0;
					}
					else
						col_count++;

					//check if element is ending
					if (currchar == '>' && attr_state != READING_ATTR_Q)
					{
						if (self_closing_slash)
						{
							fprintf(stderr, "html_parser: warn: self-closing syntax ignored, treating as normal element declaration. Line: %zu Col: %zu\n", line_count, col_count);
							self_closing_slash = false;
						}

						//check current state
						if (attr_name.length > 0)
						{
							if (attr_val.length > 0)
							{
								if (spec_case == ID)
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
								//add other attribute (valueless)
								if (!add_to_other_attr(&other_attr, &attr_name, NULL))
									goto alloc_err;
							}
						}

						//finalize other attributes (if existing)
						if (other_attr.length > 0)
						{
							curr_elem->properties.other_attributes = malloc((other_attr.length + 1) * sizeof(char));
							if (!curr_elem->properties.other_attributes)
							{	
								goto alloc_err;	
							}
							strncpy(curr_elem->properties.other_attributes, other_attr.data, other_attr.length);
							curr_elem->properties.other_attributes[other_attr.length] = '\0';
						}
						
						//reset strings
						attr_name.length = 0;
						attr_val.length = 0;
						other_attr.length = 0;

						//reset attr_state
						attr_state = DEFAULT;

						//reset curr_elem
						curr_elem = NULL;
						curr_elem_linked = false;

						//return to TEXT
						state = TEXT;

						break;
					}

					
					if (currchar == '<' && attr_state != READING_ATTR_Q)
					{
						fprintf(stderr, "html_parser: parse error: invalid character: '<'. Will still get parsed into attributes. Line: %zu Col: %zu\n", line_count, col_count);
						if (attr_state == READING_ATTR_NQ || attr_state == QUOTES_OR_NOT)
						{
							if (!string_putchar(&attr_val, '<'))
								goto alloc_err;
							attr_state = READING_ATTR_NQ;
						}
						else if (attr_state == READING_NAME || attr_state == DEFAULT || attr_state == EXP_EQ)
						{
							if (!string_putchar(&attr_name, '<'))
								goto alloc_err;
							attr_state = READING_NAME;
						}
						break;
					}

					/* CASE '/' - expect slash end */
					if (currchar == '/' && attr_state != READING_ATTR_Q)
					{
						//determine next state based on current state
						if (attr_state == DEFAULT)
						{
							//DEFAULT means that '/' is an error, but error-handling suggests adding it to attr_name (if not ending)
							attr_state = READING_NAME;
						}
						else if (attr_state == EXP_EQ)
						{
							//EXP_EQ means that no '=' was found and '/' is as if a new element was starting
							goto exp_eq_no_eq;
						}
						else if (attr_state == QUOTES_OR_NOT)
						{
							//QUOTES_OR_NOT means that no quotes have been found, so '/' is as if an attribute was starting
							attr_state = READING_ATTR_NQ;
						}
						//other states stay the same

						self_closing_slash = true;
						goto after_switch;		//don't continue into the inner FSM, report (and add '/' afterward)
					}
					/* CASE other invalid characters */
					else if (currchar == '=' && attr_state != READING_ATTR_Q && attr_state != EXP_EQ && attr_state != READING_NAME)	//if reading name, '=' is a valid transition into reading attrs
						fprintf(stderr, "html_parser: parse error: invalid character '='. Will still get parsed. Line: %zu Col: %zu\n", line_count, col_count);
					else if ((currchar == '\'' || currchar == '"') && attr_state != READING_ATTR_Q && attr_state != QUOTES_OR_NOT)
						fprintf(stderr, "html_parser: parse error: invalid character '%c'. Will still get parsed. Line: %zu Col: %zu\n", currchar,line_count, col_count);
					else if (currchar == '`' && attr_state != READING_ATTR_Q)
						fprintf(stderr, "html_parser: parse error: invalid character '`'. Will still get parsed. Line: %zu Col: %zu\n", line_count, col_count);



					/* INNER FSM SWITCH */
					switch(attr_state)
					{
						/* CASE default */
						case DEFAULT:
							//reset attr_name and attr_val
							attr_name.length = 0;
							attr_val.length = 0;

							/* CASE whitespace - ignore */
							if (isspace(currchar))
							{
								break;
							}

							//else
							//no need to check  return value, since the buffer is empty
							string_putchar(&attr_name, currchar);
							attr_state = READING_NAME;
							break;

						/* CASE reading_name */
						case READING_NAME:
							/* CASE '/' has been read */
							if (self_closing_slash)
							{
								fprintf(stderr, "html_parser: parse error: invalid character '/' - will be put into the element name. Line: %zu Col: %zu\n", line_count, col_count);
								//add it to element name anyway (error handling spec)
								if (!string_putchar(&attr_name, '/'))
									goto alloc_err;
								self_closing_slash = false;
							}

							/* CASE whitespace - continue to expect '=' or new attribute */
							if (isspace(currchar))
							{
								//try to get special case (ID or CLASS)
								spec_case = determine_special_case(&attr_name);
								attr_state = EXP_EQ;
							}
							/* CASE '=' - skip EXP_EQ state */
							else if (currchar == '=')
							{
								//try to get special case
								spec_case = determine_special_case(&attr_name);
								attr_state = QUOTES_OR_NOT;
							}
							/* CASE anything else - write to name */
							else
							{
								if (!string_putchar(&attr_name, convert_to_lowercase(currchar)))
								{
									goto alloc_err;
								}
							}
							break;

						/* CASE EXP_EQ */
						case EXP_EQ:
							/* CASE '=' - what is expected */
							if (currchar == '=')
							{
								attr_state = QUOTES_OR_NOT;
							}
							//ignore whitespace
							if (isspace(currchar))
							{
								break;
							}
							/* CASE anything else - got no '=', it must be a boolean attribute */
							else
							{
							  exp_eq_no_eq:
								//otherwise just valueless element - add to other_attr
								if (!add_to_other_attr(&other_attr, &attr_name, NULL))
								{
									goto alloc_err;
								}
								
								//reset attr_name buffer
								attr_name.length = 0;

								//no need to check return bool since there will always be at least 1 allocated char
								string_putchar(&attr_name, currchar);
								attr_state = READING_NAME;
								break;
							}
							break;

						/* CASE QUOTES_OR_NOT - determine if value is quoted or not */
						case QUOTES_OR_NOT:
							/* CASE QUOTES */
							if (currchar == '"' || currchar == '\'')
							{
								//save what the quotes started with
								curr_quote = currchar;
								attr_state = READING_ATTR_Q;
							}
							//ignore whitespace
							else if (isspace(currchar))
							{
								break;
							}
							/* CASE anything else - unquoted */
							else
							{
								attr_val.length = 0;	//just to be sure

								//no need to check return val, always should be true
								string_putchar(&attr_val, currchar);
								attr_state = READING_ATTR_NQ;
							}
							break;

						/* CASE READING QUOTED VALUE */
						case READING_ATTR_Q:
							/* CASE end of read */
							if (currchar == curr_quote)
							{
								//finalize attributes and return to default
								attr_state = DEFAULT;
								goto finalize_attr;
							}
							/* CASE class and whitespace - multiple classes, write a single one */
							else if (spec_case == CLASS && attr_val.length > 0 && isspace(currchar))
							{
								//write and reset attr_val
								if (!write_class_to_element(curr_elem, &attr_val, dst))
								{
									goto alloc_err;
								}
								attr_val.length = 0;
							}
							/* CASE anything else - write to attribute value */
							else
							{
								if (!string_putchar(&attr_val, currchar))
								{
									goto alloc_err;
								}
							}
							break;
						
						/* CASE READING UNQUOTED ATTRIBUTE */
						case READING_ATTR_NQ:
							/* CASE '/' read */
							if (self_closing_slash)
							{
								fprintf(stderr, "html_parser: parse error: invalid character '/' - will be put into the element attribute. Line: %zu Col: %zu\n", line_count, col_count);
								if (!string_putchar(&attr_val, '/'))
									goto alloc_err;
								self_closing_slash = false;
							}

							/* CASE whitespace - end */
							if (isspace(currchar))
							{
								//finalize attributes and return to default
								attr_state = DEFAULT;
								goto finalize_attr;
							}
							/* CASE anything else - write to string */
							if (!string_putchar(&attr_val, currchar))
							{
								goto alloc_err;
							}
							break;

						//label for finalizing a single attribute
					  finalize_attr:
					  	if (attr_val.length == 0)
							goto after_switch;
					  	if (spec_case == ID)
						{
							if (!write_id_to_element(curr_elem, &attr_val))
							{
								goto alloc_err;
							}
						}
						else if (spec_case == CLASS)
						{
							if (!write_class_to_element(curr_elem, &attr_val, dst))
							{
								goto alloc_err;
							}
						}
						else
						{
							if (!add_to_other_attr(&other_attr, &attr_name, &attr_val))
							{
								goto alloc_err;
							}
						}
						  	

						
					}
				  after_switch:


				}
				
		}		

		//end of loop
	}

	/* EOF REACHED */
	//check which state the FSM ended in
	switch (state)
	{
		/* states that are supposed to fall through */
		case LT_READ:
			if (!element_putchar(curr_elem, '<'))
				goto alloc_err;
			goto text_case;
		case EXCLAM_READ:
			if (!element_putchar(curr_elem, '<') ||
				!element_putchar(curr_elem, '!'))
				goto alloc_err;
			goto text_case;
		case START_DASH_READ:
			if (!element_putchar(curr_elem, '<') ||
				!element_putchar(curr_elem, '!') ||
				!element_putchar(curr_elem, '-'))
				goto alloc_err;
			goto text_case;
		//TEXT (expected to end like this)
		case TEXT:
		  text_case:
			if (curr_elem != NULL && !curr_elem_linked)
			{
				link_element(curr_elem, element_stack_peek(&stack));
				curr_elem_linked = true;
			}
			break;

		//comment ending dashes (will fall through, expected)
		case END_TWODASH_READ:
			if (!element_putchar(curr_elem, '-'))
				goto alloc_err;
		FALLTHROUGH case END_DASH_READ:
			if (!element_putchar(curr_elem, '-'))
				goto alloc_err;
		//comment-like cases
		FALLTHROUGH case COMMENT:
		FALLTHROUGH case BOGUS_COMMENT:
		case DOCTYPE:
			fprintf(stderr, "html_parser: comment-like syntax not closed before reaching EOF. Will be treated as closed upon EOF.\n");
			break;
		
		//element-related cases (errors)
		case ELEM_NAME:
			fprintf(stderr, "html_parser: reading element name ended unexpectedly with EOF. Element will be discarded.\n");
			//TODO free curr_elem
			break;
		case ELEM_PROPERTIES:
			fprintf(stderr, "html_parser: reading element properties ended unexpectedly with EOF. Incomplete element will be appended\n");
			break;
		case ELEM_END:
			fprintf(stderr, "html_parser: reading element end tag ended unexpectedly with EOF. Token will be discarded.\n");
			break;
	}

	/* STACK CHECK at EOF */
	while (stack.size > 1)
	{
		html_element_t *pop_elem = element_stack_pop(&stack);

		const char *pop_elem_name = pop_elem->properties.element_name;
		//get element name
		if (!pop_elem_name)
		{
			pop_elem_name = tag_names[pop_elem->properties.tag];
		}

		fprintf(stderr, "html_parser: element not ended before EOF: <%s>. Will be treated as if it ended upon EOF.\n", pop_elem_name);
	}

	//TODO unify freeing stuff
	
	//free buffers
	string_free(&other_attr);
	string_free(&attr_val);
	string_free(&elem_name);
	string_free(&attr_name);
	element_stack_free(&stack);

	//free curr_elem (if needed)
	if (!curr_elem_linked && curr_elem != NULL)
	{
		element_free_data(curr_elem);
		free(curr_elem);
	}

	return 0;

  alloc_err:
	// malloc/realloc failed, free everything
	
	fprintf(stderr, "html_parser: parsing ended unsuccessfully at line %zu, column %zu\n", line_count, col_count);

	//if work was being done on curr_elem, free it also
	if (curr_elem != NULL && !curr_elem_linked)
	{
		element_free_data(curr_elem);
		free(curr_elem);
	}

	//free entire tree (all-or-nothing approach if allocation fails)
	html_tree_free(dst);
  	
	string_free(&other_attr);
  f_ssss:
	string_free(&attr_val);
  f_sss:
	string_free(&attr_name);
  f_ss:
	string_free(&elem_name);
  f_s:
	element_stack_free(&stack);

	fprintf(stderr, "html_parser: Allocation failed (out of memory).\n");

	return 2;
}









