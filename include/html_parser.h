#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <string.h>
#include <stdbool.h>

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
int parse_html(const char *raw_html, html_tree_t *dst);

//frees html tree
void html_tree_free(html_tree_t *tree);

//returns true, if the element is void (<br>, <img>...)
bool is_void_element(html_element_t *elem);

#endif
