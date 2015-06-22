/**
 * @file slog_common_event.c
 * @brief Common program for logging certain informational events to servicelog
 *
 * Copyright (C) 2008  IBM
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
#include <time.h>
#include <getopt.h>
#include <inttypes.h>
#include <servicelog-1/servicelog.h>
#include "config.h"
#include "platform.h"

static struct option long_options[] = {
	{"event",       required_argument, NULL, 'e'},
	{"time",        required_argument, NULL, 't'},
	{"source",      required_argument, NULL, 's'},
	{"destination", required_argument, NULL, 'd'},
	{"location",    required_argument, NULL, 'l'},
	{"help",        no_argument,       NULL, 'h'},
	{"verbose",     no_argument,	   NULL, 'v'},
	{"version",     no_argument,	   NULL, 'V'},
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
	printf("Usage: %s --event=<event> {other_flags}\n", cmd);
	printf("    --event=<event>    <event> can be one of the following:\n");
	printf("                          migration\n");
	printf("                          fw_update\n");
	printf("                          dump_os\n");
	printf("  Other Flags:\n");
	printf("    --time=<time>      time that the event occured (in\n");
	printf("                       seconds since Epoch)\n");
	printf("    --source=<s>       source of migration, or version of\n");
	printf("                       firmware prior to update\n");
	printf("    --destination=<d>  destination of migration, or version\n");
	printf("                       of firmware after update\n");
	printf("    --location=<path>  location of dump data\n");
	printf("    --verbose | -v     verbose output\n");
	printf("    --version | -V     print version\n");
	printf("    --help | -h        print this help text and exit\n");

	return;
}

int
main(int argc, char **argv) {
	int option_index, rc, verbose=0, t=0;
	char *e=NULL, *s=NULL, *d=NULL, *l=NULL;
	char desc[1024];
	servicelog *slog;
	struct sl_event event;
	uint64_t event_id;
	int platform = 0;

	platform = get_platform();
	switch (platform) {
	case PLATFORM_UNKNOWN:
	case PLATFORM_POWERKVM:
		fprintf(stderr, "%s is not supported on the %s platform\n",
				argv[0], __power_platform_name(platform));
		exit(1);
	}

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, "e:t:s:d:l:hvV", long_options,
				 &option_index);

		if (rc == -1)
			break;

		switch (rc) {
		case 'e':
			e = optarg;
			break;
		case 't':
			t = atoi(optarg);
			break;
		case 's':
			s = optarg;
			break;
		case 'd':
			d = optarg;
			break;
		case 'l':
			l = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			printf("%s: Version %s\n", argv[0], VERSION);
			exit(0);
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

	if (e == NULL) {
		if (verbose) {
			fprintf(stderr, "The --event command-line argument is "
				"required.");
		}
		exit(1);
	}


	memset(&event, 0, sizeof(struct sl_event));
	if (t == 0)
		event.time_event = time(NULL);
	else
		event.time_event = t;
	event.type = SL_TYPE_BASIC;
	event.severity = SL_SEV_EVENT;
	event.description = desc;

	if (!strcmp(e, "migration")) {
		if (s == NULL) {
			if (verbose) {
				fprintf(stderr, "The --source command-line "
					"argument is required for migration "
					"events");
			}
			exit(1);
		}
		if (d == NULL) {
			if (verbose) {
				fprintf(stderr, "The --destination command-"
					"line argument is required for "
					"migration events");
			}
			exit(1);
		}
		event.refcode = "#MIGRATION";
		snprintf(desc, 1024, "Partition migration completed.  "
			 "Source: %s Destination: %s", s, d);
	}
	else if (!strcmp(e, "fw_update")) {
		if (s == NULL)
			s = "<unknown>";
		if (d == NULL) {
			if (verbose) {
				fprintf(stderr, "The --destination command-"
					"line argument is required for "
					"fw_update events");
			}
			exit(1);
		}
		event.refcode = "#FW_UPDATE";
		snprintf(desc, 1024, "System firmware update completed.  "
			 "Prior Level: %s New Level: %s", s, d);
	}
	else if (!strcmp(e, "dump_os")) {
		if (l == NULL) {
			if (verbose) {
				fprintf(stderr, "The --location command-line"
					"argument is required for dump_os"
					"events");
			}
			exit(1);
		}
		event.refcode = "#DUMP_OS";
		snprintf(desc, 1024, "An OS dump has been collected and is "
			 "available at %s", l);
	}

	rc = servicelog_open(&slog, 0);
	if (rc) {
		if (verbose) {
			fprintf(stderr, "Error opening servicelog: %s\n",
				servicelog_error(slog));
		}
		exit(2);
	}

	rc = servicelog_event_log(slog, &event, &event_id);
	if (rc) {
		if (verbose) {
			fprintf(stderr, "Error logging event: %s\n",
				servicelog_error(slog));
		}
		servicelog_close(slog);
		exit(3);
	}

	if (verbose) {
		printf("Logged event number ""%" PRIu64 "\n", event_id);
	}

	servicelog_close(slog);

	return 0;
}
