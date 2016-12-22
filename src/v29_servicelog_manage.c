/**
 * @file servicelog_manage.c
 * @brief The servicelog_manage utility.
 *
 * Copyright (C) 2006, 2009, 2011, 2012 IBM Corporation
 *
 * @author Michael Strosaker <strosake@us.ibm.com>
 * @author Nathan Fontenot <nfont@austin.ibm.com>
 *
 * Port to v1.1:
 * @author Brad Peters <formerly@us.ibm.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <servicelog-1/servicelog.h>
#include "platform.h"

#define ACTION_TOOMANY		-1
#define ACTION_UNSPECIFIED	0
#define ACTION_STATUS		1
#define ACTION_EVENTS		2
#define ACTION_NOTIFY		3
#define ACTION_TRUNCATE_EVENTS	4
#define ACTION_TRUNCATE_NOTIFY	5
#define ACTION_CLEAN		6

#define ARG_LIST	"a:cst:fh"

#define SECONDS_IN_DAY		24 * 60 * 60
#define SECONDS_IN_YEAR		365 * SECONDS_IN_DAY

static char *cmd;

static struct option long_options[] = {
	{"status",	no_argument,		NULL, 's'},
	{"truncate",	required_argument,	NULL, 't'},
	{"clean",	no_argument,		NULL, 'c'},
	{"force",	no_argument,		NULL, 'f'},
	{"age",		required_argument,	NULL, 'a'},
	{"help",	no_argument,		NULL, 'h'},
	{0, 0, 0, 0}
};

/**
 * print_usage
 * @brief Print the usage message
 */
static void
print_usage()
{
	printf("Usage:\n");
	printf("  %s --status            return status\n", cmd);
	printf("  %s --truncate events   delete all events and repair actions\n", cmd);
	printf("  %s --truncate notify   delete all notification tools\n", cmd);
	printf("  %s --clean [--age=<# days>]\n", cmd);
	printf("                            clean out old/repaired events\n\n");

	printf("  Other Flags:\n");
	printf("    --help             print this help text and exit\n");
	printf("    --force            do not prompt the user to verify\n");
}

/**
 * main
 * @brief Parse command line args process database
 *
 * @param argc the number of command-line arguments
 * @param argv array of command-line arguments
 * @return exit status: 0 for normal exit, 1 for usage error, >1 for other error
 */
int
main(int argc, char *argv[])
{
	struct servicelog *slog;
	int rc;
	struct sl_event *event, *events;
	struct sl_repair_action *repair, *repairs;
	struct sl_notify *notify, *notifications;
	int option_index, action=ACTION_UNSPECIFIED;
	int flag_force=0;
	int age = 60;	/* default age for --clean */
	int platform = 0;
	char buf[124];
	char *tmp;
	char *next_char;
	uint32_t num=0, num_repaired=0, num_unrepaired=0, num_info=0, num_ra=0;
	uint32_t span;
	time_t now;

	cmd = argv[0];

	platform = get_platform();
	switch (platform) {
	case PLATFORM_UNKNOWN:
	case PLATFORM_POWERNV:
		fprintf(stderr, "%s: is not supported on the %s platform\n",
					cmd, __power_platform_name(platform));
		exit(1);
	}

	if (argc <= 1) {
		print_usage();
		exit(0);
	}

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, ARG_LIST, long_options,
				&option_index);

		if (rc == -1)
			break;

		switch (rc) {
		case 's':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_STATUS;
			break;
		case 't':
			if (!optarg) {
				fprintf(stderr, "The --truncate option "
					"requires either \"events\" or "
					"\"notify\" as an argument.\n");
				print_usage();
				exit(1);
			}

			if (!strcmp(optarg, "events")) {
				if (action != ACTION_UNSPECIFIED)
					action = ACTION_TOOMANY;
				if (action != ACTION_TOOMANY)
					action = ACTION_TRUNCATE_EVENTS;
			}
			else if (!strcmp(optarg, "notify")) {
				if (action != ACTION_UNSPECIFIED)
					action = ACTION_TOOMANY;
				if (action != ACTION_TOOMANY)
					action = ACTION_TRUNCATE_NOTIFY;
			}
			else {
				fprintf(stderr, "The --truncate option "
					"requires either \"events\" or "
					"\"notify\" as an argument.\n");
				print_usage();
				exit(1);
			}
			break;
		case 'c':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_CLEAN;
			break;
		case 'a':
			age = (int)strtoul(optarg, &next_char, 10);
			if (optarg[0] == '\0' || *next_char != '\0' ||
								age < 0) {
				print_usage();
				exit(1);
			}
			break;
		case 'f':
			flag_force = 1;
			break;
		case 'h':	/* help */
			print_usage();
			exit(0);
		case '?':
			print_usage();
			exit(1);
		default:
			printf("Invalid argument: %s\n", optarg);
			print_usage();
			exit(1);
		}
	}

	if (optind < argc) {
		print_usage();
		exit(1);
	}

	/* Command-line validation */
	if (action == ACTION_UNSPECIFIED) {
		fprintf(stderr, "One of the action options is required.\n");
		print_usage();
		exit(1);
	}

	if (action == ACTION_TOOMANY) {
		fprintf(stderr, "Only one of the action options may be "
			"specified.\n");
		print_usage();
		exit(1);
	}


	switch (action) {

	case ACTION_STATUS:
		rc = servicelog_open(&slog, 0);
		if (rc != 0) {
			fprintf(stderr, "%s: Could not open servicelog "
					"database.\n%s\n",
					argv[0], servicelog_error(slog));
			exit(2);
		}

		rc = servicelog_event_query(slog, "", &events);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (event = events; event; event = event->next) {
			num++; // total event count
			if (event->serviceable && (event->repair > 0))
				num_repaired++;
			else if (event->serviceable)
				num_unrepaired++;
			else
				num_info++; // informational events
		}

		servicelog_event_free(events);

		// Now need to query repair actions:
		rc = servicelog_repair_query(slog, "", &repairs);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (repair = repairs; repair; repair = repair->next)
			num_ra++;
		servicelog_repair_free(repairs);

		servicelog_close(slog);

		printf("%-39s%10u\n", "Logged events:", num);
		printf("    %-35s%10u\n", "unrepaired serviceable events:",
		       num_unrepaired);
		printf("    %-35s%10u\n", "repaired serviceable events:",
		       num_repaired);
		printf("    %-35s%10u\n", "informational events:", num_info);
		printf("    %-35s%10u\n", "repair actions:", num_ra);
		break;


	case ACTION_TRUNCATE_EVENTS:
		if (geteuid() != 0) // Check to see if user is root
		{
			printf("Must be root to truncate the database!\n");
			exit(2);
		}

		num = 0;

		if (!flag_force) {
			printf("Are you certain you wish to delete ALL events "
			       "from the servicelog?\n");
			printf("Enter 'yes' to continue > ");
			tmp = fgets(buf, 80, stdin);
			if (!tmp)
				exit(2);

			if (strcasecmp(buf, "yes\n")) {
				printf("Operation cancelled.\n");
				exit(4);
			}
		}

		rc = servicelog_open(&slog, SL_FLAG_ADMIN);
		if (rc != 0) {
			fprintf(stderr, "%s: Could not open servicelog "
					"database.\n%s\n",
					argv[0], servicelog_error(slog));
			exit(2);
		}
		rc = servicelog_event_query(slog, "", &events);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (event = events; event; event = event->next) {
			num++;
			servicelog_event_delete(slog, event->id);
		}

		servicelog_event_free(events);

		// Delete repair actions as well.
		rc = servicelog_repair_query(slog, "", &repairs);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (repair = repairs; repair; repair = repair->next) {
			num_ra++;
			servicelog_repair_delete(slog, repair->id);
		}
		servicelog_repair_free(repairs);

		printf("Deleted %u records.\n", num + num_ra);
		servicelog_close(slog);
		break;

	case ACTION_TRUNCATE_NOTIFY:
		if (geteuid() != 0) // Check to see if user is root
		{
			printf("Must be root to truncate the database!\n");
			exit(2);
		}

		num = 0;

		if (!flag_force) {
			printf("Are you certain you wish to delete ALL "
					"notification tools from the servicelog?\n");
			printf("Enter 'yes' to continue > ");
			tmp = fgets(buf, 80, stdin);
			if (!tmp)
				exit(2);

			if (strcasecmp(buf, "yes\n")) {
				printf("Operation cancelled.\n");
				exit(4);
			}
		}

		rc = servicelog_open(&slog, SL_FLAG_ADMIN);
		if (rc != 0) {
			fprintf(stderr, "%s: Could not open servicelog "
					"database.\n%s\n",
					argv[0], servicelog_error(slog));
			exit(2);
		}
		rc = servicelog_notify_query(slog, "", &notifications);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (notify = notifications; notify; notify = notify->next) {
			num++;
			servicelog_notify_delete(slog, notify->id);
		}
		servicelog_notify_free(notifications);
		servicelog_close(slog);

		printf("Deleted %u records.\n", num);
		break;

	case ACTION_CLEAN:
		if (geteuid() != 0) { // Check to see if user is root
			printf("Must be root to purge older events "
			       "in the database!\n");
			exit(2);
		}

		if (!flag_force) {
			printf("Are you certain you wish to perform the "
			       "following tasks?\n"
			       " - Delete all repaired serviceable events\n"
			       " - Delete all informational events older than "
			       "%d days\n"
			       " - Delete all repair actions older than "
			       "%d days\n"
			       " - Delete anything older than 1 year\n",
			       age, age);
			printf("Enter 'yes' to continue > ");
			tmp = fgets(buf, 80, stdin);
			if (!tmp)
				exit(2);

			if (strcasecmp(buf, "yes\n")) {
				printf("Operation cancelled.\n");
				break;
			}
		}

		rc = servicelog_open(&slog, 0);
		if (rc != 0) {
			fprintf(stderr, "%s: Could not open servicelog "
					"database.\n%s\n",
					argv[0], servicelog_error(slog));
			exit(2);
		}

		now = time(NULL);
		span = age * SECONDS_IN_DAY;

		rc = servicelog_event_query(slog, "", &events);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}

		for (event = events; event; event = event->next) {
			if (event->serviceable && event->closed) {
				num_repaired++;
				servicelog_event_delete(slog, event->id);
			}
			else if (!event->serviceable &&
				 (event->time_logged + span) < now) {
				num_info++;
				servicelog_event_delete(slog, event->id);
			}
			else if ((event->time_logged + SECONDS_IN_YEAR) < now) {
				num++;
				servicelog_event_delete(slog, event->id);
			}
		}
		servicelog_event_free(events);

		/* Delete repair actions which are older than age */
		rc = servicelog_repair_query(slog, "", &repairs);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			servicelog_close(slog);
			exit(2);
		}
		for (repair = repairs; repair; repair = repair->next) {
			if ((repair->time_logged + span) < now ) {
				num_ra++;
				servicelog_repair_delete(slog, repair->id);
			}
		}
		servicelog_repair_free(repairs);
		servicelog_close(slog);

		printf("Removed %u repaired serviceable events.\n",
		       num_repaired);
		printf("Removed %u informational events older than %d days.\n",
		       num_info, age);
		printf("Removed %u repair actions older than %d days.\n",
		       num_ra, age);
		printf("Removed %u other events older than one year.\n", num);
		break;

	default:
		fprintf(stderr, "Internal error; unknown action %d\n", action);
		exit(3);
	}

	exit(0);
}
