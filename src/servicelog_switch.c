/*
 * servicelog_switch.c: the servicelog commands' front end.
 * Execs either the v0.2.9 version of servicelog or the v1+ version,
 * depending on which command-line options are specified.
 *
 * Copyright (C) 2009  IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "config.h"
#include "platform.h"

static struct option long_options[] = {
/* v0.2.9 options: */
	{"id",		    required_argument,  NULL, 'i'},
	{"type",	    required_argument,  NULL, 't'},
	{"start_time",	    required_argument,  NULL, 's'},
	{"end_time",	    required_argument,  NULL, 'e'},
	{"severity",	    required_argument,  NULL, 'E'},
	{"serviceable",	    required_argument,  NULL, 'S'},
	{"repair_action",   required_argument,  NULL, 'R'},
	{"event_repaired",  required_argument,  NULL, 'r'},
	// {"location",        required_argument,  NULL, 'l'},

/* v1 options: */
	{"query",	    required_argument, NULL, 'q'},
	{"dump",	    no_argument,       NULL, 'd'},

/* common options */
	{"help",	    no_argument,       NULL, 'h'},
	{"verbose",	    no_argument,       NULL, 'v'},
	{"version",	    no_argument,       NULL, 'V'},
	{0,0,0,0}
};

static char *cmd;
static char v29_cmd[PATH_MAX];
static char v1_cmd[PATH_MAX];
static char *v29_usage[] = { v29_cmd, "-h", NULL };
static char *v1_usage[] = { v1_cmd, "-h", NULL };

static void
setup_failed(const char *why)
{
	fprintf(stderr, "%s: cannot find v1_servicelog and/or v29_servicelog\n",								cmd);
	fprintf(stderr, "%s\n", why);
}

/*
 * Set up the pathnames for the v0.2.9 and v1+ executables.  Wherever
 * they are, they should be in the same directory as this one, whose
 * pathname is referenced by the /proc/self/exe symlink.
 */
static void
set_up_commands(void)
{
	char self_dir[PATH_MAX];
	ssize_t pathlen;
	char *last_slash;

	pathlen = readlink("/proc/self/exe", self_dir, PATH_MAX);
	/* Leave room because "v29_servicelog" is longer than "servicelog". */
	if (pathlen < 0 || pathlen > PATH_MAX-10) {
		setup_failed("readlink of /proc/self/exe failed");
		perror("/proc/self/exe");
		exit(2);
	}
	self_dir[pathlen] = '\0';	/* since readlink doesn't provide it */
	last_slash = strrchr(self_dir, '/');
	if (!last_slash) {
		setup_failed("pathname lacks /");
		exit(2);
	}
	pathlen -= (strlen(last_slash) - 1); /* account for trailing / */
	last_slash[1] = '\0';

	(void) strncpy(v1_cmd, self_dir, PATH_MAX - 1);
	v1_cmd[pathlen] = '\0';
	(void) strncat(v1_cmd, "v1_servicelog", (PATH_MAX - pathlen - 1 ));
	(void) strncpy(v29_cmd, self_dir, PATH_MAX - 1);
	v29_cmd[pathlen] = '\0';
	(void) strncat(v29_cmd, "v29_servicelog", (PATH_MAX - pathlen - 1));
}

static void
exec_command(int argc, char **argv)
{
	execv(argv[0], argv);
	fprintf(stderr, "could not execute %s\n", argv[0]);
	perror("execv");
	exit(2);
}

static void
run_command(int argc, char **argv)
{
	pid_t pid = fork();
	if (pid == 0) {
		/* I'm the child. */
		exec_command(argc, argv);
		/* NOTREACHED */
	} else if (pid > 0) {
		/* I'm the parent. */
		int status = 0;
		(void) wait(&status);
	} else {
		perror("fork");
		exit(2);
	}
}

static void
print_usage(void)
{
	printf(
"This command supports two mutually exclusive sets of command-line options.\n");
	printf(
"Here are the command-line options supported for compatibility with the\n"
"0.2.9 version of servicelog:\n");
	printf("\n");
	fflush(stdout);
	run_command(2, v29_usage);
	printf("\n");
	printf(
"Here are the command-line options for the current (%s) version of\n"
"servicelog:\n", VERSION);
	printf("\n");
	fflush(stdout);
	run_command(2, v1_usage);
}

int
main(int argc, char **argv)
{
	int v29_opts = 0, v1_opts = 0;
	int option_index, rc;
#ifndef SERVICELOG_TEST
	int platform = 0;

	platform = get_platform();
	switch (platform) {
	case PLATFORM_UNKNOWN:
	case PLATFORM_POWERNV:
		fprintf(stderr, "%s: is not supported on the %s platform\n",
				argv[0], __power_platform_name(platform));
		exit(1);
	}
#endif
	cmd = argv[0];

	set_up_commands();

	for (;;) {
		option_index = 0;

		rc = getopt_long(argc, argv, "dE:e:hi:q:R:r:S:s:t:Vv",
					long_options, &option_index);
		if (rc == -1)
			break;
		switch (rc) {
		case 'd':
		case 'q':
			v1_opts++;
			break;
		case 'E':
		case 'e':
		case 'i':
		case 'R':
		case 'r':
		case 'S':
		case 's':
		case 't':
			v29_opts++;
			break;
		case 'V':
			printf("%s: Version %s\n", cmd, VERSION);
			exit(0);
		case 'v':
			break;
		case 'h':
		case '?':
			print_usage();
			exit(0);
		default:
			print_usage();
			exit(1);
			/* NOTREACHED */
		}
	}
	if (v1_opts && v29_opts) {
		fprintf(stderr,
			"You cannot mix v0.2.9 options with v1+ options.\n\n");
		print_usage();
		exit(1);
	}
	if (v29_opts) {
		argv[0] = v29_cmd;
		exec_command(argc, argv);
	} else {
		argv[0] = v1_cmd;
		exec_command(argc, argv);
	}
	/* NOTREACHED */
	exit(2);
}
