/**
 * @file v29_servicelog.c -- the v0.2.9 version of servicelog.c
 * @brief The servicelog utility.
 *
 * Copyright (C) 2005 IBM Corporation
 *
 * @author Michael Strosaker <strosake@us.ibm.com>
 * @author Nathan Fontenot <nfont@austin.ibm.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <servicelog-1/libservicelog.h>
#include "config.h"
#include "platform.h"

#define ARG_LIST	"i:t:s:e:E:S:R:r:l:hvV"

static char *cmd;

static uint32_t types[SL_MAX_EVENT_TYPE];
static int type_indx = 0;

static struct option long_options[] = {
	{"id",		    required_argument,  NULL, 'i'},
	{"type",	    required_argument,  NULL, 't'},
	{"start_time",	    required_argument,  NULL, 's'},
	{"end_time",	    required_argument,  NULL, 'e'},
	{"severity",	    required_argument,  NULL, 'E'},
	{"serviceable",	    required_argument,  NULL, 'S'},
	{"repair_action",   required_argument,  NULL, 'R'},
	{"event_repaired",  required_argument,  NULL, 'r'},
	{"location",        required_argument,  NULL, 'l'},
	{"help",	    no_argument,        NULL, 'h'},
	{"verbose",	    no_argument,	NULL, 'v'},
	{"Version",	    no_argument,	NULL, 'V'},
	{0,0,0,0}
};

/**
 * print_usage
 * @brief Print the usage message
 *
 * @param command the name of the command (argv[0] in main)
 */
static void
print_usage() 
{
	printf("Usage: %s {query_flags} {other_flags}\n", cmd);
	printf("  Query Flags:\n");
	printf("    --id=<id>          find servicelog event with key <id>\n");
	printf("    --type=<type>      event type(s) to query on\n");
	printf("                       types are os, app, ppc64_rtas, ppc64_encl\n");
	printf("                       (this option may be specified more than once)\n");
	printf("    --start_time=<time> beginning of time window\n");
	printf("    --end_time=<time>   end of time window\n");
	printf("    --repair_action={yes|no|all}\n");
	printf("                       search for repair actions?\n");
	printf("    --serviceable={yes|no|all}\n");
	printf("                       search for serviceable events?\n");
	printf("    --event_repaired={yes|no|all}\n");
	printf("                       search for repaired events?\n");
	printf("    --severity=<sev>   search for events of particular sev\n");
	printf("  Other Flags:\n");
//	printf("    --location=<path>  servicelog location (if not default)\n");
	printf("    --verbose | -v     verbose output\n");
	printf("    --Version | -V     print version\n");
	printf("    --help             print this menu and exit\n");

	return;
}

/**
 * valid_type_arg
 * @brief Validate an arg for a specified option
 *
 * @param type integer argument to the type option
 * @param max maximum value of the type parmaeter
 * @param arg name of the arg we are checking
 * @return 1 if is valid, 0 otherwise
 */
static int
valid_arg(int type, int max, const char *arg)
{
	if ((type > 0) && (type < max))
		return 1;

	fprintf(stderr, "The \"%d\" argument to the %s option is not valid\n",
		type, arg);
	print_usage();
	return 0;
}

/**
 * valid_yna_arg
 * @breif validate an arg as either "yes", "no" or "all"
 *
 * @param arg argument to validate
 * @return converted value if its is valid, -1 otherwise
 */
static int
valid_yna_arg(const char *arg, const char *optarg)
{
	if (strncmp(optarg, "yes", 3) == 0)
		return SL_QUERY_YES;

	if (strncmp(optarg, "no", 2) == 0)
		return SL_QUERY_NO;

	if (strncmp(optarg, "all", 3) == 0)
		return SL_QUERY_ALL;

	fprintf(stderr, "The \"%s\" argument to the %s option is not valid\n",
		optarg, arg);
	print_usage();
	return 0;
}

static int
add_type(char *type)
{
	if (strncmp(type, "app", 3) == 0) 
		types[type_indx++] = SL_TYPE_APP;
	else if (strncmp(type, "os", 2) == 0)
		types[type_indx++] = SL_TYPE_OS;
	else if (strncmp(type, "ppc64_rtas", 10) == 0)
		types[type_indx++] = SL_TYPE_PPC64_RTAS;
	else if (strncmp(type, "ppc64_encl", 10) == 0)
		types[type_indx++] = SL_TYPE_PPC64_ENCL;
	else if (strncmp(type, "all", 3) == 0)
		type_indx = 0;
	else
		return 0;

	return 1;
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
	int verbose = 0;
	uint32_t id = 0;
	int other_flag = 0;
	size_t sz;
	void *data; 
	char *location = NULL;
	struct servicelog slog;
	struct sl_query query;
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

	if (argc <= 1) {
		print_usage();
		exit(0);
	}
		
	memset(&slog, 0, sizeof(slog));
	memset(&query, 0, sizeof(query));

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, ARG_LIST, long_options,
				 &option_index);

		if (rc == -1)
			break;

		switch (rc) {
		case 'i':	/* event ID */
			id = atoi(optarg);
			break;
		case 't':
			if (! add_type(optarg))
				exit (-1);

			other_flag++;
			query.num_types = type_indx;
			query.event_types = types;
			break;
		case 's':
			other_flag++;
			query.start_time = atoi(optarg);
			break;
		case 'e':
			other_flag++;
			query.end_time = atoi(optarg);
			break;
		case 'S':
			rc = valid_yna_arg("serviceable", optarg);
			if (rc == -1)
				exit(-1);

			other_flag++;
			query.is_serviceable = rc;
			break;
		case 'R':
			rc = valid_yna_arg("repair_action", optarg);
			if (rc == -1)
				exit(-1);

			other_flag++;
			query.is_repair_action = rc;
			break;
		case 'r':
			rc = valid_yna_arg("event_repaired", optarg);
			if (rc == -1)
				exit(-1);

			other_flag++;
			query.is_repaired = rc;
			break;
		case 'E':
			if (! valid_arg(atoi(optarg), 8, "severity"))
				exit(-1);

			other_flag++;
			query.severity = atoi(optarg);
			break;
		case 'l':
			location = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			printf("%s: Version %s\n", argv[0], VERSION);
			exit(0);
			break;
		case '?':
			print_usage();
			exit(1);
			break;
		default:
			printf("huh?\n");
		case 'h':	/* help */
			print_usage();
			exit(0);
			break;
		}
	}

	/* Command-line validation */
	if (id && other_flag) {
		fprintf(stderr, "The --id flag is mutually exclusive with all "
			"other query flags.\n");
		print_usage();
		exit(-1);
	}
	
	if ((! id) && (! other_flag)) {
		fprintf(stderr, "One of the query flags must be specified to "
			"query the servicelog.\n");
		print_usage();
		exit(-1);
	}

	rc = servicelog_open(&slog, location, 0);
	if (rc != 0) {
		fprintf(stderr, "%s\n", servicelog_error(&slog));
		return 2;
	}

	if (id) {
		/* print the specified event and/or repair action */
		struct sl_header *hdr;

		rc = servicelog_get_event(&slog, id, &data, &sz);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(&slog));
			servicelog_close(&slog);
			return 2;
		}

		for (hdr = (struct sl_header*) data; hdr; hdr = hdr->next) {
			if (verbose) 
				servicelog_print_event(stdout, hdr, verbose);
			else
				servicelog_print_header(stdout, hdr, 0);
			printf("\n");
		}
	} 
	else {
		struct sl_header *hdr;

		rc = servicelog_query(&slog, &query);
		if (rc != 0) {
			fprintf(stderr, "%s\n", servicelog_error(&slog));
			servicelog_close(&slog);
			return 2;
		}

		for (hdr = query.result; hdr != NULL; hdr = hdr->next) {
			servicelog_print_event(stdout, hdr, verbose);
			printf("\n");
		}
	}
	
	servicelog_query_close(&slog, &query);
	servicelog_close(&slog);

	return 0;
}
