#include "html_parser.h"
#include "html_parser_internal.h"

typedef enum {
	UNDEF,
	TEXT,
	ELEMENT,
	CLASSES,
	ID,
	COMMENT
} states_t;

typedef enum {
	UNDEF,
	START_READ,
	EXCLAM_READ,
	DASH_READ,
	TWO_DASHES_READ
} help_states_t;

//1 - NULL input
//2 - invalid HTML
int parse_html(const char *raw_html, html_tree_t *dst)
{
	if (!raw_html || !dst)
		return 1;

	states_t state = UNDEF;
	help_states_t help_state = UNDEF;
	while (true)
	{
		if (*raw_html == '<')
		{
			
		}

		raw_html++;
	}	
}
