/* TEST OF BASIC FUNCTIONALITY */
//tests if tree init works
//tests if a tree can be built
//tests if a tree can be freed
//
//USE VALGRIND for full functionality
//

#include "../include/html_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//argv[1] should be a .html file
int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "html_parser_test: No arguments given. First argument must be a .html file\n");
		return 1;
	}

	if (strcasestr(argv[1], ".html") == NULL)
	{
		fprintf(stderr, "html_parser_test: '.html' not found in first argument.\n");
		return 1;
	}

	FILE *html_file = fopen(argv[1], "r");
	if (!html_file)
	{
		fprintf(stderr, "html_parser_test: could not open '%s'\n", argv[1]);
		return 1;
	}

	//init tree
	html_tree_t *tree = html_tree_init();
	if (!tree)
	{
		fprintf(stderr, "html_parser_test: failed to init html_tree.\n");
		return 2;
	}

	//read
	int res = parse_html(html_file, tree);

	if (res != 0)
	{
		fprintf(stderr, "html_parser_test: Parsing returned %d, which means there were issues.\n", res);
	}

	//free
	html_tree_free(tree);

	fclose(html_file);

	return 0;
}


