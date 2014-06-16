#include "exceptional.h"
#include <stdlib.h>

// Literal bstring
#define BL(m) (& (struct tagbstring) bsStatic(m))

#define NULL_REPLACEMENT '_'

void exceptional_dump_fn(FILE *file, const char *fn, const char *tag, const char *extra) {
	if (tag) {
		if (extra)
			fprintf(file, ANSI_COLOR_BRIGHT_CYAN "%s %s \"%s\":\n" ANSI_COLOR_RESET, fn, tag, extra);
		else
			fprintf(file, ANSI_COLOR_BRIGHT_CYAN "%s %s:\n" ANSI_COLOR_RESET, fn, tag);
	}
	else
		fprintf(file, ANSI_COLOR_BRIGHT_CYAN "%s:\n" ANSI_COLOR_RESET, fn);
}

char *exceptional_bstring_to_string(bstring string_b) {
	return bstr2cstr(string_b, NULL_REPLACEMENT);
}

char *exceptional_strdup(const char *string) {
	bstring dup_b = bfromcstr(string);
	char *dup = exceptional_bstring_to_string(dup_b);
	bdestroy(dup_b);
	return dup;
}

char *exceptional_sprintf(size_t max_size, const char *format, va_list args) {
	char *string;
	bstring string_b = bfromcstr("");
	int r = bvcformata(string_b, max_size, format, args);
	if (r > BSTR_ERR)
		string = exceptional_bstring_to_string(string_b);
	else
		string = exceptional_strdup(format);
	bdestroy(string_b);
	return string;
}

char *exceptional_escape_spaces(const char *string) {
	bstring SPACE = BL(" ");
	bstring ESCAPED_SPACE = BL("\\ ");

	bstring string_b = bfromcstr(string);
	bfindreplace(string_b, SPACE, ESCAPED_SPACE, 0);
	char *escaped = exceptional_bstring_to_string(string_b);
	bdestroy(string_b);
	return escaped;
}

bool exceptional_list_initialized(list_t *list) {
	return list && list->head_sentinel;
}

void exceptional_list_move(list_t *source, list_t *destination) {
	// Move all exceptions from our context to the relay context
	exceptional_list_for_each (source, void, element)
		list_append(destination, element);
	list_clear(source);
}

bool exceptional_list_destroy_with_elements(list_t *list, exceptional_list_destroy_element_fn destroy_element) {
	if (exceptional_list_initialized(list)) {
		exceptional_list_for_each (list, void, element) {
			if (destroy_element)
				destroy_element(element);
			free(element);
		}
		return true;
	}
	return false;
}
