/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     04/03/2014 02:31:50 AM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

#ifndef PARSE_H_
#define PARSE_H_

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Parse list item
 */
struct parse_litem_t {
	char * token;
	struct parse_litem_t * next;
	struct parse_litem_t * prev;
};

/**
 * @brief  Parse list
 */
struct parse_list_t {
	char * input;
	char * output;
	bool background;
	size_t length;

	struct parse_litem_t * head;
	struct parse_litem_t * tail;
};

/**
 * @brief  Init parse list
 *
 * @param cmd_list list to init
 */
static inline
void parse_list_init(struct parse_list_t * cmd_list) {
	cmd_list->input = NULL;
	cmd_list->output = NULL;
	cmd_list->background = false;
	cmd_list->head = NULL;
	cmd_list->tail = NULL;
	cmd_list->length = 0;
}

void parse_free(struct parse_list_t * cmd_list);
bool parse_command(struct parse_list_t * cmd_list, const char * cmd);

#endif // PARSE_H_

