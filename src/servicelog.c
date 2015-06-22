/**
 * @file servicelog.c
 * @brief Program for querying the servicelog database
 *
 * Copyright (C) 2005, 2008, 2009, 2012  IBM
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <inttypes.h>
#include <servicelog-1/servicelog.h>
#include "config.h"
#include "platform.h"

static char *cmd;

static struct option long_options[] = {
	{"query",	    required_argument, NULL, 'q'},
	{"dump",	    no_argument,       NULL, 'd'},
	{"help",	    no_argument,       NULL, 'h'},
	{"verbose",	    no_argument,       NULL, 'v'},
	{"version",	    no_argument,       NULL, 'V'},
	{0,0,0,0}
};

/**
 * print_usage
 * @brief Print the usage message
 *
 * @param cmd the name of the command (argv[0] in main)
 */
static void
print_usage(char *cmd) 
{
	printf("Usage: %s {[--dump] | [--query='<query>']} [-vVh]\n", cmd);
	printf("  Without any command-line arguments, prints the statistics\n");
	printf("  of the current servicelog database contents.\n\n");

	printf("  --dump             Dumps all of the events in the\n");
	printf("                     servicelog database.\n");
	printf("  --query='<query>'  Prints all of the events that match the\n");
	printf("                     query string. <query> is formatted like\n");
	printf("                     the WHERE clause of an SQL statement\n");
// Don't advertise -v.  It doesn't do anything, but it might be used by
// Director or some such.
//	printf("  --verbose | -v     Verbose output\n");
	printf("  --version | -V     Print the version of the command and exit\n");
	printf("  --help | -h        Print this help text and exit\n\n");

	printf("  Sample Queries:\n");
	printf("    servicelog --query='id=12'\n");
	printf("        prints the event with an ID of 12\n");
	printf("    servicelog --query='serviceable=1 AND closed=0'\n");
	printf("        prints all open (unfixed) serviceable events\n");
	printf("    servicelog --query='severity>=$WARNING AND closed=0'\n");
	printf("        prints all open events with a sev of WARNING or greater\n");
	printf("    servicelog --query=\"time_event>'2008-02-08'\"\n");
	printf("        prints all events that occurred after Feb 08, 2008\n");

	return;
}

void
check_event(struct sl_event *e, int *open, int *close, int *info)
{
	if (e->serviceable) {
		if (e->closed)
			(*close)++;
		else
			(*open)++;
	}
	else
		(*info)++;
}

/**
 * main
 * @brief Parse command line args and execute diagnostics
 *
 * @param argc the number of command-line arguments
 * @param argv array of command-line arguments
 * @return exit status: 0 for normal exit, 1 for usage error, >1 for other error
 */
int
main(int argc, char *argv[]) 
{
	int option_index, rc;
	int dump = 0;
	int platform = 0;
	char *query = NULL;
	servicelog *slog;
	struct sl_event *event;

	cmd = argv[0];

	platform = get_platform();
	switch (platform) {
	case PLATFORM_UNKNOWN:
	case PLATFORM_POWERKVM:
		fprintf(stderr, "%s: is not supported on the %s platform\n",
					cmd, __power_platform_name(platform));
		exit(1);
	}

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, "dq:vVh", long_options,
				 &option_index);

		if (rc == -1)
			break;

		switch (rc) {
		case 'd':
			dump = 1;
			break;
		case 'q':
			query = optarg;
			break;
		case 'v':
			/* obsolete */
			break;
		case 'V':
			printf("%s: Version %s\n", argv[0], VERSION);
			exit(0);
			break;
		case 'h':	/* help */
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

	if (dump && query) {
		fprintf(stderr, "The dump and query flags cannot be specified "
			"on the same command line.\n\n");
		print_usage(argv[0]);
		exit(1);
	}

	rc = servicelog_open(&slog, 0);
	if (rc) {
		fprintf(stderr, "Error opening servicelog: %s\n", strerror(rc));
		exit(2);
	}

	if (dump) {
		rc = servicelog_event_query(slog, "", &event);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			exit(2);
		}
		rc = servicelog_event_print(stdout, event, 1);
		if (rc < 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			exit(2);
		}
		servicelog_event_free(event);
	}
	else if (query) {
		rc = servicelog_event_query(slog, query, &event);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			exit(2);
		}
		rc = servicelog_event_print(stdout, event, 1);
		if (rc < 0) {
			fprintf(stderr, "%s\n", servicelog_error(slog));
			exit(2);
		}
		servicelog_event_free(event);
	}
	else {
		struct sl_event *e;

		struct sl_repair_action *repair, *r;
		struct sl_notify *notify, *n;

		int n_repair = 0, n_notify = 0;

		int n_events = 0, n_open = 0;
		/* _o = open events, _c = closed events, _i = info events */
		int n_basic = 0, n_basic_o = 0, n_basic_c = 0, n_basic_i=0;
		int n_os = 0, n_os_o = 0, n_os_c = 0, n_os_i=0;
		int n_rtas = 0, n_rtas_o = 0, n_rtas_c = 0, n_rtas_i=0;
		int n_encl = 0, n_encl_o = 0, n_encl_c = 0, n_encl_i=0;
		int n_bmc = 0, n_bmc_o = 0, n_bmc_c = 0, n_bmc_i=0;

		/* Print a summary of the database contents */
		printf("Servicelog Statistics:\n\n");

		rc = servicelog_event_query(slog, "", &event);
		e = event;
		while (e) {
			n_events++;

			if (e->serviceable && !(e->closed))
				n_open++;

			switch (e->type) {
			case SL_TYPE_BASIC:
				n_basic++;
				check_event(e, &n_basic_o, &n_basic_c,
					    &n_basic_i);
				break;
			case SL_TYPE_OS:
				n_os++;
				check_event(e, &n_os_o, &n_os_c, &n_os_i);
				break;
			case SL_TYPE_RTAS:
				n_rtas++;
				check_event(e, &n_rtas_o, &n_rtas_c, &n_rtas_i);
				break;
			case SL_TYPE_ENCLOSURE:
				n_encl++;
				check_event(e, &n_encl_o, &n_encl_c, &n_encl_i);
				break;
			case SL_TYPE_BMC:
				n_bmc++;
				check_event(e, &n_bmc_o, &n_bmc_c, &n_bmc_i);
				break;
			default:
				fprintf(stderr, "Event ""%" PRIu64 "has unknown type "
					"%d\n", e->id, e->type);
			}

			e = e->next;
		}
		servicelog_event_free(event);

		if (n_open == 0)
			printf("There are no open events that require action."
			       "\n\n");
		else if (n_open == 1)
			printf("There is 1 open event requiring action.\n\n");
		else
			printf("There are %d open events requiring action.\n\n",
			       n_open);

		printf("Summary of Logged Events:\n\n");

		printf("  %10s %7s %7s %7s %7s\n\n", "Type", "Total", "Open",
		       "Closed", "Info");

		if (n_basic)
			printf("  %10s %7d %7d %7d %7d\n", "Basic", n_basic,
			       n_basic_o, n_basic_c, n_basic_i);
		if (n_os)
			printf("  %10s %7d %7d %7d %7d\n", "OS", n_os,
			       n_os_o, n_os_c, n_os_i);
		if (n_rtas)
			printf("  %10s %7d %7d %7d %7d\n", "RTAS", n_rtas,
			       n_rtas_o, n_rtas_c, n_rtas_i);
		if (n_encl)
			printf("  %10s %7d %7d %7d %7d\n", "Enclosure", n_encl,
			       n_encl_o, n_encl_c, n_encl_i);
		if (n_bmc)
			printf("  %10s %7d %7d %7d %7d\n", "BMC", n_bmc,
			       n_bmc_o, n_bmc_c, n_bmc_i);

		printf("  %10s -------------------------------\n", "");
		printf("  %10s %7d %7d %7d %7d\n\n", "",
		       n_basic + n_os + n_rtas + n_encl + n_bmc,
		       n_basic_o + n_os_o + n_rtas_o + n_encl_o + n_bmc_o,
		       n_basic_c + n_os_c + n_rtas_c + n_encl_c + n_bmc_c,
		       n_basic_i + n_os_i + n_rtas_i + n_encl_i + n_bmc_i);
       

		rc = servicelog_repair_query(slog, "", &repair);
		r = repair;
		while (r) {
			n_repair++;
			r = r->next;
		}
		servicelog_repair_free(repair);
		printf("Logged Repair Actions:         %d\n", n_repair);

		rc = servicelog_notify_query(slog, "", &notify);
		n = notify;
		while (n) {
			n_notify++;
			n = n->next;
		}
		servicelog_notify_free(notify);
		printf("Registered Notification Tools: %d\n", n_notify);
	}

	servicelog_close(slog);

	return 0;
}
