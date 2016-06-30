/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     04/03/2014 12:38:02 AM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "proj3.h"
#include "parse.h"
#include "pidlist.h"

typedef void * (* pthread_fun_t)(void *);

static const char * ROOT_PROMPT			= "# ";
static const char * USER_PROMPT			= "$ ";
static const char * CMD_EXIT				= "exit";

static const char * MSG_EXIT				= "\nDone. See you next time, bye!\n";
static const char * MSG_SIGCHILD			= "\r<<< child %d exited\n";
static const char * MSG_SIGTERM_CHILD	= "\r<<< some child procs exist, sending SIGTERM\n";
static const char * MSG_WAIT_CHILD		= "\r<<< waiting for children to be terminated\n";

static const char * ERR_LONG_INPUT		= "Input too long!\n";
static const char * ERR_PARSE_FAILED	= "Unable to parse command!\n";

/**
 * @brief  Stored PIDs of procs run in background
 */
struct pidlist_t pidlist;

/*
 * exit program?
 */
static volatile bool g_exit = false;

/*
 * buffer for input
 */
static char buffer[BUF_SIZE];
pthread_mutex_t buffer_mutex_read;
pthread_mutex_t buffer_mutex_exec;
pthread_cond_t  buffer_cond_read;
pthread_cond_t  buffer_cond_exec;


/**
 * @brief  Read one char from stdin
 *
 * @return   char or CHAR_EOF on end of file
 */
static
int read_char() {
	int res;
	ssize_t num_read;

	do {
		num_read = read(1, &res, 1);
	} while (num_read == EINTR);

	return num_read == 1 ? res : CHAR_EOF;
}

/**
 * @brief  Print simple help
 *
 * @param pname program name
 *
 * @return   always EXIT_FAILURE
 */
static
int print_help(const char * pname) {
	UNUSED(pname);
	static const char * MSG_HELP =
		"Simple interactive shell implementation using POSIX threads\n"
		"Fridolin Pokorny, 2014 <fridex.devel@gmail.com>\n";

	write(2, MSG_HELP, strlen(MSG_HELP));

	return EXIT_FAILURE;
}

/**
 * @brief  Print error to stderr
 *
 * @param str error message
 *
 * @return   always false
 */
static inline
bool print_error(const char * str) {
	static const char * ERR_MSG_PREFIX = "ERROR: ";

	write(2, ERR_MSG_PREFIX, strlen(ERR_MSG_PREFIX));
	write(2, str, strlen(str));

	return false;
}

/**
 * @brief  SIGCHILD signal handler
 *
 * @param sig signal number
 */
void sigchild_handler(int sig) {
	UNUSED(sig);
	struct pidlist_item_t * item;

	pid_t child_pid = waitpid(-1, NULL, WNOHANG);
	if ((item = pidlist_find(&pidlist, child_pid))) { // was it running on background?
		pidlist_remove(&pidlist, item);
		fprintf(stderr, MSG_SIGCHILD, child_pid);
	}
}

/**
 * @brief  Print prompt depending on user
 */
static inline
void print_prompt() {
	if (geteuid() == 0) {
		write(1, ROOT_PROMPT, strlen(ROOT_PROMPT));
	} else {
		write(1, USER_PROMPT, strlen(USER_PROMPT));
	}
}

/**
 * @brief  Block SIGINT signal
 */
static
void sigint_block() {
	sigset_t setint;
	sigemptyset(&setint);
	sigaddset(&setint, SIGINT);
	sigprocmask(SIG_BLOCK, &setint, NULL);
}

/**
 * @brief  UNblock SIGINT signal
 */
static
void sigint_unblock() {
	sigset_t setint;
	sigemptyset(&setint);
	sigaddset(&setint, SIGINT);
	sigprocmask(SIG_UNBLOCK, &setint, NULL);
}

/**
 * @brief  Restore signal handlers
 *
 * @return   true on success
 */
static
bool signal_handler_restore() {
	struct sigaction sigact;

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;

	sigact.sa_handler = NULL;

	if (sigaction(SIGCHLD, &sigact, NULL) < 0) {
		perror("sigaction");
		return false;
	}

	return true;
}

/**
 * @brief  Init signal handlers
 *
 * @return   true on success
 */
static
bool signal_handler_init() {
	struct sigaction sigact;

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;

	sigact.sa_handler = sigchild_handler;

	if (sigaction(SIGCHLD, &sigact, NULL) < 0) {
		perror("sigaction");
		return false;
	}

	return true;
}

/**
 * @brief  Read command from stdin
 *
 * @param buffer buffer to read command to
 *
 * @return   false on exit
 */
static
bool read_command() {
	ssize_t num_read = BUF_SIZE;
	int c;

	while (num_read == BUF_SIZE) {

		do {
			print_prompt();
			num_read = read(0, buffer, BUF_SIZE);
		} while (num_read == EINTR); // skip interrupt

		if (num_read == BUF_SIZE) {
			while (buffer[BUF_SIZE - 1] != '\n'
					&& (c = read_char()) != '\n'
					&& c != CHAR_EOF)
				;
			print_error(ERR_LONG_INPUT);
			if (c == CHAR_EOF) {
				write(1, CMD_EXIT, strlen(CMD_EXIT));
				write(1, "\n", 1);
				return false;
			}
		}
		if (num_read == 0) {
			write(1, CMD_EXIT, strlen(CMD_EXIT));
			write(1, "\n", 1);
			return false;
		}

		if (strlen(buffer) == 1 && buffer[0] == '\n')
			num_read = BUF_SIZE; // empty line, read next
	}

	buffer[num_read - 1] = '\0';

	return true;
}

/**
 * @brief  Run command described by string stored in buffer
 *
 * @param buffer command to be executed
 *
 * @return   true on success
 */
static
void * run_command(void * p) {
	UNUSED(p);
	char ** cmd = NULL;
	int ret;
	int i = 0;
	struct parse_list_t cmd_list;
	struct parse_litem_t * it;


	pthread_mutex_lock(&buffer_mutex_exec);
	pthread_cond_wait(&buffer_cond_exec, &buffer_mutex_exec);
	while (! g_exit) {
		parse_list_init(&cmd_list);

		if (! parse_command(&cmd_list, buffer)) {
			print_error(ERR_PARSE_FAILED);
			goto signalize;
		}

		if (cmd_list.length == 1 && ! strcmp(cmd_list.head->token, CMD_EXIT)) {
			g_exit = true;
			goto signalize;
		}
		//write(1, MSG_COMMAND, strlen(MSG_COMMAND));

		if (cmd_list.length == 0) {
			goto signalize; // nothing to do
		}

		cmd = (char **) malloc((cmd_list.length + 1) * sizeof(char *));
		if (! cmd) { parse_free(&cmd_list); goto signalize; }

		for (it = cmd_list.head, i = 0; it; it = it->next, ++i) {
			//printf("cmd[%d] == %s\n", i, it->token);
			cmd[i] = it->token;
		}
		cmd[cmd_list.length] = (char *) 0;

		ret = vfork();

		if (ret < 0) {
			perror("fork failed");
		} else if (ret == 0) { // child
			// open input
			if (cmd_list.input) {
				ret = open(cmd_list.input, O_RDONLY);
				if (ret < 0) {
					perror(cmd_list.input);
					exit(EXIT_FAILURE);
				}

				if (dup2(ret, STDIN_FILENO) < 0) {
					perror("dup2 STDIN_FILENO");
					exit(EXIT_FAILURE);
				}
			} else if (cmd_list.background) {
				close(STDIN_FILENO);
			}

			// open output
			if (cmd_list.output) {
				ret = open(cmd_list.output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (ret < 0) {
					perror(cmd_list.output);
					exit(EXIT_FAILURE);
				}

				if (dup2(ret, STDOUT_FILENO) < 0) {
					perror("dup2 STDOUT_FILENO");
					exit(EXIT_FAILURE);
				}
			}

			/*
			 * Restore signal handlers
			 */
			signal_handler_restore();

			if (cmd_list.background) {
				pidlist_insert(&pidlist, getpid());
				fprintf(stderr, "\r>>> child %d is running in background\n", getpid());
			} else {
				// run in foreground, unblock SIGINT
				sigint_unblock();
			}

			// run it! :-*
			ret = execvp(cmd_list.head->token, cmd);
			if (ret < 0) {
				perror(cmd_list.head->token);
				exit(EXIT_FAILURE);
			}
		} else { // parent
			if (! cmd_list.background)
				waitpid(ret, NULL, 0);
		}
signalize:
		if (cmd) {
			free(cmd); cmd = NULL;
		}
		parse_free(&cmd_list);
		buffer[0] = '\0'; // mark buffer as empty
		pthread_cond_signal(&buffer_cond_read);
		if (! g_exit) // wait only if no exit signaled
			pthread_cond_wait(&buffer_cond_exec, &buffer_mutex_exec);
	} // g_exit

	pthread_mutex_unlock(&buffer_mutex_exec);

	return NULL;
}

/**
 * @brief  main
 *
 * @param argc argument count
 * @param argv[] argument vector
 *
 * @return   EXIT_SUCCESS on success, otherwise EXIT_FAILURE
 */
int main(int argc, char * argv[]) {
	pthread_t run_thread;

	if (argc != 1)
		return print_help(argv[0]);

	pthread_mutex_init(&buffer_mutex_read, NULL);
	pthread_mutex_init(&buffer_mutex_exec, NULL);
	pthread_cond_init (&buffer_cond_read, NULL);
	pthread_cond_init (&buffer_cond_exec, NULL);

	sigint_block();				// block ^C
	signal_handler_init();		// print info about SIGCHILD
	pidlist_init(&pidlist);		// init PID list of background procs

	pthread_create(&run_thread, NULL, (pthread_fun_t) run_command, NULL);

	pthread_mutex_lock(&buffer_mutex_read);
	while (! g_exit) {
		if (! read_command())
			g_exit = true;

		pthread_cond_signal(&buffer_cond_exec);
		if (! g_exit) // wait only if there is something to do
			pthread_cond_wait(&buffer_cond_read, &buffer_mutex_read);
	}
	pthread_mutex_unlock(&buffer_mutex_read);

	pthread_join(run_thread, NULL);

	/*
	 * kill remaining procs
	 */
	if (! pidlist_empty(&pidlist)) {
		write(2, MSG_SIGTERM_CHILD, strlen(MSG_SIGTERM_CHILD));
		pidlist_kill_free(&pidlist);
		write(2, MSG_WAIT_CHILD, strlen(MSG_WAIT_CHILD));
		waitpid(-1, NULL, 0);
	}

	pthread_mutex_destroy(&buffer_mutex_read);
	pthread_mutex_destroy(&buffer_mutex_exec);
	pthread_cond_destroy(&buffer_cond_read);
	pthread_cond_destroy(&buffer_cond_exec);

	sigint_unblock();

	write(2, MSG_EXIT, strlen(MSG_EXIT));

	return EXIT_SUCCESS;
}

