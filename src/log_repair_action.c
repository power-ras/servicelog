/**
 * @file log_repair_action.c
 * @brief Log a repair action for a serviceable event
 *
 * Copyright (C) 2005, 2008, 2013 IBM
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <servicelog-1/servicelog.h>
#include "config.h"
#include "platform.h"

#define BUF_SIZE	512
#define ARG_LIST	"l:p:d:n:t:qvVh"

static struct option long_options[] = {
	{"location",	required_argument, NULL, 'l'},
	{"procedure",	required_argument, NULL, 'p'},
	{"date",	required_argument, NULL, 'd'},
	{"note",	required_argument, NULL, 'n'},
	{"quiet",	no_argument,       NULL, 'q'},
	{"type",	required_argument, NULL, 't'},
//	{"api_version",	required_argument, NULL, 'v'},
	{"help",	no_argument,       NULL, 'h'},
	{"version",	no_argument,       NULL, 'V'},
	{0,0,0,0}
};

/**
 * print_usage
 * @brief print the usage message
 *
 * @param command the name of the command (argv[0] in main)
 */
void
print_usage(char *command) {
	printf("Usage: %s -l <location> -p <procedure> {optional_flags}\n",
	       command);
	printf("    -l: location code of the device that was repaired\n");
	printf("    -p: repair procedure that was followed\n");
	printf("  Optional Flags:\n");
	printf("    -d: date/time that the procedure was performed\n");
	printf("        (defaults to current date/time if not specified)\n");
	printf("    -n: include a note with the repair action\n");
	printf("        (useful for indicating who performed the repair)\n");
	printf("    -q: quiet mode (log the repair action without prompting\n");
	printf("        for confirmation)\n");
	printf("    -t: type of event this repair action is for (v0.2.9)\n");
	printf("	Valid types: (os, ppc64_rtas, or ppc64_encl)\n");
	printf("    -V: print the version of the command and exit\n");
//	printf("    -v: Version of API to use. \n");
//	printf("	Valid arguments: '-v 1', otherwise defaults to v0.2.9)\n");
	printf("    -h: print this help text and exit\n");

	return;
}

/**
 * main
 * @brief parse cmd line options and log repair action
 *
 * @param argc the number of command-line arguments
 * @param argv array of command-line arguments
 * @return exit status: 0 for normal exit, 1 for usage error, >1 for other error
 */
int
main(int argc, char *argv[])
{
	int option_index, quiet=0;
	char *date = NULL, *dummy;
	char buf[BUF_SIZE], tmp_system_arg[(BUF_SIZE/2)];
	uint64_t id;
	struct servicelog *servlog;
	struct sl_repair_action repair_action, *ra = &repair_action;
	struct sl_event *events = NULL;
	time_t epoch;
	int api_version = 0;  // Short for version 0.2.9
	int platform = 0;
	pid_t cpid;	/* Pid of child		*/
	int rc;		/* Holds return value	*/
	int status;	/* exit value of child	*/
	int pipefd[2];	/* pipe file descriptor	*/

	platform = get_platform();
	switch (platform) {
	case PLATFORM_UNKNOWN:
	case PLATFORM_POWERNV:
		fprintf(stderr, "%s is not supported on the %s platform\n",
					argv[0], __power_platform_name(platform));
		exit(1);
	}

	memset(ra, 0, sizeof(*ra));
	memset(buf, 0, BUF_SIZE);
	memset(tmp_system_arg, 0, (BUF_SIZE/2));

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, ARG_LIST, long_options,
				&option_index);

		if (rc == -1)
			break;
		switch (rc) {
		case 'l':	/* location */
			ra->location = optarg;
			break;
		case 'p':	/* procedure */
			ra->procedure = optarg;
			break;
		case 'd':	/* date */
			date = optarg;
			break;
		case 'n':	/* note */
			ra->notes = optarg;
			break;
		case 'q':	/* quiet */
			quiet = 1;
			break;
		case 'V':
			printf("%s: Version %s\n", argv[0], VERSION);
			exit(0);
			break;
		case 'v':
			if ((strncmp(optarg, "1", 1) == 0))
				api_version = 1;
			break;
		case 't':	/* type - Added for backwards v29 compatibility */
			/* Not actually used.  Loc code is sufficient for device lookup with v1 API */
//			if ((rc = valid_type(optarg)) == -1) {
//				fprintf(stderr, "%s: Valid types are: os, "
//					"ppc64_rtas, or ppc64_encl\n", argv[0]);
//				print_usage(argv[0]);
//				return 1;
//			}
//			type = strdup(optarg);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
			break;
		case '?':
			print_usage(argv[0]);
			exit(1);
			break;
		default:
			fprintf(stderr, "Encountered a problem while parsing "
				"options; report a bug to the maintainers "
				"(%s).\n", PACKAGE_BUGREPORT);
			exit(1);
		}
	}

	if (ra->location == NULL) {
		fprintf(stderr, "%s: A location code was not specified\n",
			argv[0]);
		return 1;
	}
	if (ra->procedure == NULL) {
		fprintf(stderr, "%s: A procedure was not specified. Defaulting to ''\n",
			argv[0]);
		ra->procedure = "";
	}

	if (date) {
		/* Create a pipe */
		if (pipe(pipefd) == -1) {
			if (!quiet) {
				fprintf(stderr, "%s: Pipe creation failed at %s,%d\n",
					argv[0], __func__, __LINE__);
			}
			exit(1);
		}/* pipe */

		/* fork/exec */
		cpid = fork();
		if (cpid == -1) {
			if (!quiet) {
				fprintf(stderr, "%s: Forking Failed at: %s,%d\n",
					argv[0], __func__, __LINE__);
			}
			close(pipefd[0]);
			close(pipefd[1]);
			exit(1);
		} /* fork */

		if (cpid == 0) {
			char *system_arg[5] = {NULL,};	/* execv argument list */
			int re_fd;			/* redirects stderr to /dev/null */

			/* Redirect stdout to pipe */
			if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
				if (!quiet) {
					fprintf(stderr, "%s: closing stdout failed at %s,%d\n",
						argv[0], __func__, __LINE__);
				}
				close(pipefd[0]);
				close(pipefd[1]);
				exit(1);
			} /* dup of stdout */

                        re_fd = open("/dev/null", O_WRONLY);
                        if (re_fd == -1) {
                                if (!quiet) {
                                        fprintf(stderr, "%s: Failed to open /dev/null at"
                                        " %s,%d\n", argv[0], __func__, __LINE__);
                                }
                                close(pipefd[0]);
                                close(pipefd[1]);
                                exit(1);
                        }

                        if (dup2(re_fd, STDERR_FILENO) == -1) {
                                if (!quiet) {
                                        fprintf(stderr, "%s: Failed to redirect "
                                                "stderr to /dev/null at %s,%d\n",
                                                argv[0], __func__,__LINE__);
                                }
				close(re_fd);
                                close(pipefd[0]);
                                close(pipefd[1]);
                                exit(1);
                        }

			/* Close read end of pipe */
			close(pipefd[0]);

			system_arg[0] = "/bin/date";
			system_arg[1] = "+\%s";
			system_arg[2] = "--date";
			snprintf(tmp_system_arg, (BUF_SIZE/2) -1 ,"%s",date);
			system_arg[3] = tmp_system_arg;

			/* execv */
			execv(system_arg[0], system_arg);
			exit(1);
		} else {
			/* Parent */
			/* Close write end of pipe */
			close(pipefd[1]);

			rc = read(pipefd[0], buf, BUF_SIZE);
			if (rc == -1) {
				/* read failed. Either broken pipe or child terminated */
				if (!quiet) {
					fprintf(stderr, "%s reading from pipe failed %s,%d\n",
						argv[0], __func__, __LINE__);
				}
			} else {
				if (strlen(buf) == 0) {
					if (!quiet) {
						fprintf(stderr, "%s: Invalid date %s\n",
							argv[0], date);
					}
					rc = -1;
				} else if ((epoch = strtol(buf, NULL, 0)) == 0) {
					if (!quiet) {
						fprintf(stderr, "%s: %s\n", argv[0], buf);
					}
					rc = -1;
				} /* if epoch */
			} /* if read */

			close(pipefd[0]);

			/* Wait for lsvpd command to complete */
			if (waitpid(cpid, &status, 0) == -1) {
				if (!quiet) {
					fprintf(stderr, "%s: wait on pid failed at "
						"%s,%d\n", argv[0], __func__, __LINE__);
				}
			} /* if waitpid */

			if (rc == -1)
				exit(1);
		}/* if fork */
	} else {
			/* use the current date */
			epoch = time(NULL);
	}

	ra->time_repair = epoch;

	if (!quiet) {
		/* prompt the user for confirmation */
		printf("Are you certain you wish to log the following repair "
			"action?\nDate: %sLocation: %s\nProcedure: %s\n"
			"(y to continue, any other key to cancel): ",
			ctime(&epoch), ra->location, ra->procedure);
		dummy = fgets(buf, BUF_SIZE, stdin);
		if (!dummy)
			return 4;

		if (strlen(buf) != 2 || buf[0] != 'y') {
			printf("\nCancelled.\n");
			return 0;
		}
	}

	rc = servicelog_open(&servlog, 0);
	if (rc != 0) {
		if (!quiet)
			fprintf(stderr, "%s: Could not open servicelog "
				"database to log the repair action.\n%s\n",
				argv[0], servicelog_error(servlog));
		return 2;
	}

	rc = servicelog_repair_log(servlog, ra, &id, &events);
	if (rc == 0) {
		if (!quiet) {
			fprintf(stdout, "%s: servicelog record ID =""%" PRIu64 ".\n",
				argv[0], id);
			fprintf(stdout, "\nThe following events were "
				"repaired:\n\n");
			servicelog_event_print(stdout, events, 0);
			servicelog_event_free(events);
		}
	} else {
		if (!quiet)
			fprintf(stderr, "%s: Could not log the repair action."
				"\n%s\n", argv[0], servicelog_error(servlog));
		servicelog_close(servlog);
		return 3;
	}

	servicelog_close(servlog);

	return 0;
}
