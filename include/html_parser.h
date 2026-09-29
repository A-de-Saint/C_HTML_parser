#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

//compile using -DUSE_STRING_SOURCE to read from char *raw_html
//default settings are reading from a file
#ifdef USE_STRING_SOURCE
	#define RAW_HTML_TYPE const char *
	#define RAW_HTML_GETCHAR(raw_html) (char)(*(raw_html++))
	#define RAW_HTML_EOF '\0'
#else
    #include <stdlib.h>
	#define RAW_HTML_TYPE FILE *
	#define RAW_HTML_GETCHAR(raw_html) (char)fgetc(raw_html)
	#define RAW_HTML_EOF EOF
#endif

typedef enum {
    ELEMENT_TYPE = 0,
    TEXT_TYPE,
    COMMENT_TYPE
} element_type_t;

//html tree representation
struct html_tree;
typedef struct html_tree html_tree_t;

//html element representation
struct html_element;
typedef struct html_element html_element_t;

//initializes the tree
//returns NULL upon failure
html_tree_t *html_tree_init(void);

//parses raw_html into dst
int parse_html(RAW_HTML_TYPE raw_html, html_tree_t *dst);

//frees html tree
void html_tree_free(html_tree_t *tree);

//returns tree's first element (document element)
//the document element contains no data other than first_child
html_element_t *get_document_element(html_tree_t *tree);

//returns true, if the element is void (<br>, <img>...)
bool is_void_element(html_element_t *elem);

//returns element type
//ELEMENT, COMMENT or TEXT
element_type_t get_element_type(html_element_t *elem);

//returns element name
//returns NULL for COMMENT and TEXT element types
const char *get_element_name(html_element_t *elem);

//returns element id
//returns NULL if element does not have an id
const char *get_element_id(html_element_t *elem);

//returns element class, fills number of classes into class_count
//returns NULL if element does not have any classes, class_count = 0
//tree is needed as a parameter since it contains the classes table
const char **get_element_classes(html_element_t *elem, html_tree_t *tree, size_t *class_count);

//returns element's other attributes (non-id, non-class attributes)
//other attributes are a single string in the format: attr1="val1 val2" attr2="val3"
const char *get_element_other_attributes(html_element_t *elem);

//returns element's parent
html_element_t *get_element_parent(html_element_t *elem);

//returns element's first child
//returns NULL for childless elements
html_element_t *get_element_first_child(html_element_t *elem);

//returns element's next sibling (in the linked list)
//returns NULL if element is the last of the parent's children
html_element_t *get_element_next_sibling(html_element_t *elem);

//prints element's type, content or attributes
//if print_family_info, also prints parent's, first_child's and next_sibling's type and optionally name and/or id
void print_element(html_element_t *elem, html_tree_t *tree, bool print_family_info);

//prints entire tree, with family info
void print_html_tree(html_tree_t *tree);

#endif
