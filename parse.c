/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     04/03/2014 02:31:38 AM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

#include "parse.h"
#include "proj3.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Suppress unused variable warning
 */
#ifndef UNUSED
# define UNUSED(X)			((void) X)
#endif // UNUSED(X)

/*
 * Special tokens on input, which have to be handled
 */
#define TKN_BACKGROUND			(1<<31)
#define TKN_INPUT					(1<<30)
#define TKN_OUTPUT				(1<<29)

/*
 * Special tokens predicates
 */
#define IS_TKN_BG(X)				((X) & TKN_BACKGROUND)
#define IS_TKN_IN(X)				((X) & TKN_INPUT)
#define IS_TKN_OUT(X)			((X) & TKN_OUTPUT)

#define IS_TKN(X)					(IS_TKN_BG(X) || IS_TKN_IN(X) || IS_TKN_OUT(X))

/*
 * Get displacement when special token signaled
 */
#define GET_DISP_TKN_BG(X)		(((X) ^ TKN_BACKGROUND) + 1)
#define GET_DISP_TKN_IN(X)		(((X) ^ TKN_INPUT) + 1)
#define GET_DISP_TKN_OUT(X)	(((X) ^ TKN_OUTPUT) + 1)

static const char * ERR_PARSE_OUTPUT			= "PARSE: Syntax error using '>'\n";
static const char * ERR_PARSE_INPUT				= "PARSE: Syntax error using '<'\n";
static const char * ERR_PARSE_BACKGROUND		= "PARSE: Syntax error using '&'\n";
static const char * ERR_PARSE_UNEXPECTED		= "PARSE: Syntax error in input!\n";

/**
 * @brief  Print parsed line - for debug purpose only!
 *
 * @param l list to be printed
 */
#ifdef DEBUG
static
void dbg_print(struct parse_list_t * l) {
	if (l->input)
		fprintf(stderr, "\tINPUT: %s\n", l->input);
	else
		fprintf(stderr, "\tINPUT: stdin\n");

	if (l->output)
		fprintf(stderr, "\tOUTPUT: %s\n", l->output);
	else
		fprintf(stderr, "\tOUTPUT: stdout\n");

	if (l->background)
		fprintf(stderr, "\tBACKGROUND: true\n");
	else
		fprintf(stderr, "\tBACKGROUND: false\n");

	for (struct parse_litem_t * it = l->head; it; it = it->next) {
		fprintf(stderr, "\tTOKEN: '%s'\n", it->token);
	}

}
#endif // DEBUG

/**
 * @brief  Add token to a list
 *
 * @param cmd_list commad list
 * @param token token to be added
 *
 * @return   true on success, otherwise false
 */
static
bool add_item(struct parse_list_t * cmd_list, char * token) {
	struct parse_litem_t * item;

	item = malloc(sizeof(struct parse_litem_t));
	if (! item) return false;

	item->token = token;

	item->next = NULL;
	item->prev = cmd_list->tail;

	if (cmd_list->tail) {
		cmd_list->tail->next = item;
	}

	cmd_list->tail = item;

	if (! cmd_list->head)
		cmd_list->head = item;

	cmd_list->length++;

	return true;
}

/**
 * @brief  Remove item from list
 *
 * @param cmd_list list to be used
 * @param item item to be removed
 *
 * @return removed item
 */
#if 0
static
struct parse_litem_t * remove_item(struct parse_list_t * cmd_list,
												struct parse_litem_t * item) {
	if (item == cmd_list->head) {
		cmd_list->head = item->next;
		item->next->prev = NULL;
	}

	if (item == cmd_list->tail) {
		cmd_list->tail = item->prev;
		item->prev->next = NULL;
	}

	if (item->next) {
		item->next->prev = item->prev;
	}

	if (item->prev) {
		item->prev->next = item->next;
	}

	cmd_list->length--;

	return item;
}
#endif // 0

/**
 * @brief  Get token from buffer
 *
 * @param token where to put parsed token
 * @param cmd used buffer
 *
 * @return   displacement for next token
 */
static
int get_token(char ** token, const char * cmd) {
	int start = 0;
	int end = 0;

	while (cmd[start] == ' ' || cmd[start] == '\t')
		start++;

	if (cmd[start] == '\0')
		return 0;
	else if (cmd[start] == '>')
		return start + TKN_OUTPUT;
	else if (cmd[start] == '<')
		return start + TKN_INPUT;
	else if (cmd[start] == '&')
		return start + TKN_BACKGROUND;

	for (end = start + 1; true; end++) {
		if (cmd[end] == ' '
				|| cmd[end] == '\t'
				|| cmd[end] == '>'
				|| cmd[end] == '<'
				|| cmd[end] == '&'
				|| cmd[end] == '\0') {
			(*token) = (char *) malloc(sizeof(char) * (end - start + 1));
			strncpy((*token), &cmd[start], end - start);
			(*token)[end-start] = '\0';

			return end;
		}
	}

	return 0;
}

/**
 * @brief  Free command list
 *
 * @param cmd_list list to be freed
 */
void parse_free(struct parse_list_t * cmd_list) {
	struct parse_litem_t *tmp;

	for (struct parse_litem_t * it = cmd_list->head; it; /**/) {
		tmp = it;
		it = it->next;

		if (tmp->token)
			free(tmp->token);
		free(tmp);
	}

	if (cmd_list->input)
		free(cmd_list->input);

	if (cmd_list->output)
		free(cmd_list->output);

	parse_list_init(cmd_list);
}

/**
 * @brief  Parse command from buffer
 *
 * @param cmd_list list to place parsed command to
 * @param cmd buffer to be used
 *
 * @return   true on success otherwise false
 */
bool parse_command(struct parse_list_t * cmd_list, const char * cmd) {
	parse_list_init(cmd_list);

	char * token;
	int start = 0;
	int disp = 0;

	while ((disp = get_token(&token, &cmd[start]))) {
		if (IS_TKN_BG(disp)) {
			if (cmd_list->background) { // only once allowed
				write(2, ERR_PARSE_BACKGROUND, strlen(ERR_PARSE_BACKGROUND));
				parse_free(cmd_list);
				return false;
			}
			start += GET_DISP_TKN_BG(disp);
			cmd_list->background = true;
		} else if (IS_TKN_IN(disp)) {
			start += GET_DISP_TKN_IN(disp);
			disp = get_token(&token, &cmd[start]);

			if (disp <= 0 || cmd_list->input || IS_TKN(disp)) { // only once!
				write(2, ERR_PARSE_INPUT, strlen(ERR_PARSE_INPUT));
				parse_free(cmd_list);
				return false;
			}

			cmd_list->input = token;
			start += disp;
		} else if (IS_TKN_OUT(disp)) {
			start += GET_DISP_TKN_OUT(disp);
			disp = get_token(&token, &cmd[start]);

			if (disp <= 0 || cmd_list->output || IS_TKN(disp)) { // only once!
				write(2, ERR_PARSE_OUTPUT, strlen(ERR_PARSE_OUTPUT));
				parse_free(cmd_list);
				return false;
			}

			cmd_list->output = token;
			start += disp;
		} else if (! cmd_list->input
						&& ! cmd_list->output
						&& ! cmd_list->background) {
			// regular token of command (i.e. not redirect/&)
			add_item(cmd_list, token);
			start += disp;
		} else {
			write(2, ERR_PARSE_UNEXPECTED, strlen(ERR_PARSE_UNEXPECTED));
			parse_free(cmd_list);
			return false;
		}
	}

#ifdef DEBUG
	dbg_print(cmd_list);
#endif // DEBUG

	return true;
}

