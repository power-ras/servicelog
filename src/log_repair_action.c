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
#include <servicelog-1/servicelog.h>
#include "config.h"

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
	FILE *fp;
	int option_index, rc, quiet=0;
	char *date = NULL, *dummy;
	char cmd[BUF_SIZE], buf[BUF_SIZE];
	uint64_t id;
	struct servicelog *servlog;
	struct sl_repair_action repair_action, *ra = &repair_action;
	struct sl_event *events = NULL;
	time_t epoch;
	int api_version = 0;  // Short for version 0.2.9

	memset(ra, 0, sizeof(*ra));

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
		/* convert the date argument to a time_t */
		snprintf(cmd, BUF_SIZE, "/bin/date --date=\"%s\" +%%s", date);

		if ((fp = popen(cmd, "r")) == NULL) {
			fprintf(stderr, "%s: Could not run /bin/date\n",
				argv[0]);
			return 1;
		}

		rc = fread(buf, 1, BUF_SIZE, fp);
		buf[rc-1] = '\0';
		rc = pclose(fp);

		if ((epoch = strtol(buf, NULL, 0)) == 0) {
			if (!quiet)
				fprintf(stderr, "%s: %s\n", argv[0], buf);
			return 1;
		}
	}
	else {
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
		if (strlen(buf) != 2 || buf[0] != 'y') {
			printf("\nCancelled.\n");
			return 0;
		}
	}

	rc = servicelog_open(&servlog, 0);
	if (rc != 0) {
		if (!quiet)
			fprintf(stderr, "%s: Could not open servicelog "
				"databse to log the repair action.\n%s\n",
				argv[0], servicelog_error(servlog));
		return 2;
	}

	rc = servicelog_repair_log(servlog, ra, &id, &events);
	if (rc == 0) {
		if (!quiet) {
			fprintf(stdout, "%s: servicelog record ID = %llu.\n",
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
