/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     04/08/2014 12:10:49 PM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

#ifndef PIDLIST_H_
#define PIDLIST_H_

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief  PID list item
 */
struct pidlist_item_t {
	pid_t pid;
	struct pidlist_item_t * next;
};

/**
 * @brief  Header of PID list
 */
struct pidlist_t {
	struct pidlist_item_t * first;
};


/**
 * @brief  Init PID list
 *
 * @param pidlist PID list to init
 */
static inline
void pidlist_init(struct pidlist_t * pidlist) {
	pidlist->first = NULL;
}

static inline
bool pidlist_empty(struct pidlist_t * pidlist) {
	return pidlist->first == NULL;
}

bool pidlist_insert(struct pidlist_t * pidlist, pid_t pid);
struct pidlist_item_t * pidlist_find(struct pidlist_t * pidlist, pid_t pid);
bool pidlist_remove(struct pidlist_t * pidlist, struct pidlist_item_t * item);
void pidlist_kill_free(struct pidlist_t * pidlist);

#endif // PIDLIST_H_

