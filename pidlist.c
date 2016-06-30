/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     04/08/2014 12:10:46 PM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

#include "pidlist.h"

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

/**
 * @brief  Insert PID into PID list
 *
 * @param pidlist PID list to use
 * @param pid pid to be inserted
 *
 * @return true on success
 */
bool pidlist_insert(struct pidlist_t * pidlist, pid_t pid) {
	struct pidlist_item_t * item;

	item = (struct pidlist_item_t *) malloc(sizeof(struct pidlist_item_t));
	if (! item) return false;

	item->pid = pid;
	item->next = pidlist->first;
	pidlist->first = item;

	return true;
}

/**
 * @brief  Find pid in PID list
 *
 * @param pidlist PID list to use
 * @param pid PID to be inserted
 *
 * @return true on success
 */
struct pidlist_item_t * pidlist_find(struct pidlist_t * pidlist, pid_t pid) {
	struct pidlist_item_t * it;

	if (pid < 0)
		return NULL;

	for (it = pidlist->first; it; it = it->next)
		if (it->pid == pid)
			return it;

	return NULL;
}

/**
 * @brief  Remove item from PID list
 *
 * @param pidlist PID list to use
 * @param item item to be removed
 *
 * @return  true on success
 */
bool pidlist_remove(struct pidlist_t * pidlist, struct pidlist_item_t * item) {
	struct pidlist_item_t * it;
	struct pidlist_item_t * prev = NULL;

	for (it = pidlist->first; it; prev = it, it = it->next) {
		if (it == item) {
			if (prev == NULL) // head of the list
				pidlist->first = it->next;
			else
				prev->next = it->next;

			return true;
		}
	}

	return false;
}

/**
 * @brief  Free allocated memory for PID list
 *
 * @param pidlist PID list to use
 */
void pidlist_kill_free(struct pidlist_t * pidlist) {
	struct pidlist_item_t * it;
	struct pidlist_item_t * tmp;

	for (it = pidlist->first; it; /**/) {
		tmp = it;
		it = it->next;
		kill(tmp->pid, SIGTERM);
		free(tmp);
	}
}

