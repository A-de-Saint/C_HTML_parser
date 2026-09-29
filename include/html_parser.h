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

//returns true, if the element is void (<br>, <img>...)
bool is_void_element(html_element_t *elem);

#endif
