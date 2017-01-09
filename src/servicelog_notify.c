/**
 * @file servicelog_notify.c
 * @brief Program for registering/manipulating servicelog notification tools
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <servicelog-1/servicelog.h>
#include "config.h"
#include "platform.h"

#define ACTION_TOOMANY		-1
#define ACTION_UNSPECIFIED	0
#define ACTION_ADD		1
#define ACTION_LIST		2
#define ACTION_REMOVE	3
#define ACTION_QUERY	4

#define TYPE_EVENTS 0x1
#define TYPE_REPAIRS 0x2

#define ARG_LIST	"alrqi:t:E:R:S:c:M:m:h"

static char *cmd;

static struct option long_options[] = {
	{"add",		    no_argument,        NULL, 'a'},
	{"remove",	    no_argument,        NULL, 'r'},
	{"list",	    no_argument,        NULL, 'l'},
	{"match",	    required_argument,  NULL, 'm'},
	{"type",	    required_argument,  NULL, 't'},
	{"command",	    required_argument,	NULL, 'c'},
	{"method",	    required_argument,	NULL, 'M'},
	{"id",	            required_argument,	NULL, 'i'},
	{"help",	    no_argument,        NULL, 'h'},
	/*  v29-only command line options */
	{"severity",	    required_argument,  NULL, 'E'},
	{"repair_action",   required_argument,  NULL, 'R'},
	{"serviceable",	    required_argument,  NULL, 'S'},
	{"query",	    no_argument,        NULL, 'q'},
	{0,0,0,0}
};

/**
 * print_usage
 * @brief Print the usage message
 */
static void
print_usage()
{
	printf("Usage: %s {--add | --remove | --list} [flags]\n",
	       cmd);
	printf("  Add Flags:\n");
	printf("    --command=\"<cmd>\"  command to be run when notified\n");
	printf("    --type=EVENT|REPAIR  notify on events or repair actions?\n");
	printf("    --match=<query_string>  notify on events matching query\n");
	printf("    --method={num_stdin|num_arg|text_stdin|pairs_stdin}\n");
	printf("  Remove Flags:  One of --id or --command must be specified.\n");
	printf("  List Flags:    At most one of --id or --command may be specified.\n");
	printf("    --id=<id>    ID of registered tool to list or remove\n");
	printf("  Flags supported for backward compatibility:\n");
	printf("    --type=\"<type>\"  notify on specified event type(s).\n");
	printf("        Can be: [os|ppc64_encl|ppc64_rtas|ppc64_bmc],\n");
	printf("        or multiple with '|' between\n");
	printf("    --severity=<sev>   notify only of events with at least\n");
	printf("        severity<sev>. (Range 1 (lowest) to 7 (fatal))\n");
	printf("    --repair_action={yes|no|all}\n");
	printf("    --serviceable={yes|no|all}\n");
	printf("    --query    Like --list, but requires --id or --command.\n");
	printf("  Other Flags:\n");
	printf("    --help       Print this help text and exit\n");

	return;
}


/**
 * valid_yna_arg
 * @brief validate an arg as either "yes", "no" or "all"
 *
 * @param arg command-line option that is being validated
 * @param optarg argument to validate
 * @return converted value if its is valid, -1 otherwise
 */
static int
valid_yna_arg(const char *optarg)
{
	if (strncmp(optarg, "yes", 3) == 0)
		return SL_QUERY_YES;

	if (strncmp(optarg, "no", 2) == 0)
		return SL_QUERY_NO;

	if (strncmp(optarg, "all", 3) == 0)
		return SL_QUERY_ALL;

	fprintf(stderr, "The \"%s\" argument is not valid\n", optarg);
	print_usage();
	return -1;
}

/**
 * valid_method_arg
 * @brief validate an arg as either "num_stdin", "num_arg" or "text_stdin"
 *
 * @param arg argument to validate
 * @return converted value if it is valid, -1 otherwise
 */
static int
valid_method_arg(const char *arg) {
	if (strncmp(arg, "num_stdin", 9) == 0)
		return SL_METHOD_NUM_VIA_STDIN;
	else if (strncmp(arg, "num_arg", 7) == 0)
		return SL_METHOD_NUM_VIA_CMD_LINE;
	else if (strncmp(arg, "text_stdin", 10) == 0)
		return SL_METHOD_PRETTY_VIA_STDIN;
	else if (strncmp(arg, "pairs_stdin", 11) == 0)
		return SL_METHOD_SIMPLE_VIA_STDIN;

	return -1;
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
	int option_index, rc, action=ACTION_UNSPECIFIED;
	int flag_id=0;
	int add_flags=0, method=0, servicable = 0;

	/*
	 * Once again, it was necessary to make some tradeoffs in adding
	 * v29 support to v1.  In the case of a --type argument,
	 * v1 permits EVENT or REPAIR, which is then passed
	 * in during the add in notify_struct->notify.  v29 permitted
	 * --type=os|app|ppc_rtas etc, which is transformed into a
	 * MATCH string in v1.  (Unlike v29, we don't support multiple
	 * --type options.)  So, type_match is the match
	 * string we generate to encode any v29-style filters on
	 * event type, severity, or serviceability.  (In v1, such
	 * filters are specified with the -m option.)
	 *
	 * If the user specifies both a v1 -m option and one or more
	 * v29 type, severity, or serviceability options, we use the
	 * -m match string and ignore type_match (!).
	 *
	 * All this needs to be revisited.
	 */
	char type_match[1024], *type_tmp = NULL;
	char *next = type_match, *end = next + sizeof(type_match) - 1;
	int notify_flag = 0;
	uint64_t id=0;
	char *command=NULL, *match=NULL, query[256], cmdbuf[256];
	char *next_char;
	struct servicelog *servlog;
	struct sl_notify *notify, *current;
	struct stat sbuf;
	char *tSev = NULL;
	int tRepAct = 0;
	int platform = 0;
	char *connector = "";

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

	memset(&servlog, 0, sizeof(servlog));

	for (;;) {
		option_index = 0;
		rc = getopt_long(argc, argv, ARG_LIST, long_options,
				 &option_index);

		if (rc == -1)
			break;

		switch (rc) {
		case 'a':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_ADD;
			break;
		case 'r':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_REMOVE;
			break;
		case 'l':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_LIST;
			break;
		case 'q':
			if (action != ACTION_UNSPECIFIED)
				action = ACTION_TOOMANY;
			if (action != ACTION_TOOMANY)
				action = ACTION_QUERY;
			break;
		case 'i':	/* event ID */
			id = strtoul(optarg, &next_char, 10);
			if (optarg[0] == '\0' || *next_char != '\0' ||
			    (long)id <= 0) {
				fprintf(stderr,"--id argument invalid.\n\n");
				print_usage();
				exit(1);
			}
			flag_id = 1;
			break;
		case 't':
			type_tmp = optarg;
			add_flags++;
			break;
		case 'c':
			command = optarg;

			/* verify command argument */
			snprintf(cmdbuf, 256, "%s", command);
			next_char = strchr(cmdbuf, ' ');
			if (next_char != NULL)
				*next_char = '\0';

			if (stat(cmdbuf, &sbuf) < 0) {
				fprintf(stderr, "Command '%s' does not exist.\n",
					cmdbuf);
				exit(1);
			}
			if (!S_ISREG(sbuf.st_mode)) {
				fprintf(stderr, "'%s' is not a valid command.\n",
					cmdbuf);
				exit(1);
			}
			if (!(sbuf.st_mode & S_IXUSR)) {
				fprintf(stderr, "'%s' does not have execute "
					"permission.\n", cmdbuf);
				exit(1);
			}
			break;
		case 'm':
			match = optarg;
			add_flags++;
			break;
		case 'M':
			method = valid_method_arg(optarg);
			if (method == -1) {
				fprintf(stderr, "--method or -M argument invalid\n");
				print_usage();
				exit(1);
			}
			add_flags++;
			break;
		case 'h':	/* help */
			print_usage();
			exit(0);
			break;
		case'E':
			tSev = optarg;
			add_flags++;
			break;
		case'R':
			tRepAct = valid_yna_arg(optarg);
			switch (tRepAct) {
			case SL_QUERY_YES:
				notify_flag = TYPE_REPAIRS;
				break;
			case SL_QUERY_NO:
				notify_flag = TYPE_EVENTS;
				break;
			case SL_QUERY_ALL:
				notify_flag = TYPE_EVENTS | TYPE_REPAIRS;
				break;
			default:
				fprintf(stderr, "--repair_action or -R argument invalid\n");
				print_usage();
				exit(1);
			}
			add_flags++;
			break;
		case'S':
			servicable = valid_yna_arg(optarg);
			if (servicable < 0) {
				fprintf(stderr, "--serviceable or -S argument invalid\n");
				print_usage();
				exit(1);
			}
			// This seems odd.
			if (servicable < 2) //  Yes or All
				notify_flag |= TYPE_EVENTS;
			add_flags++;
			break;
		case '?':
			fprintf(stderr, "Unknown flag: -%c\n", rc);
			print_usage();
			exit(1);
			break;
		default:
			fprintf(stderr, "Invalid argument\n");
		}
	}

	/* Command-line validation */
	if (action == ACTION_UNSPECIFIED) {
		fprintf(stderr, "One of --add, --remove, --query or --list "
			"is required.\n\n");
		print_usage();
		exit(1);
	}

	*next = '\0';
	if (type_tmp) {
		/* convert type arg to a valid v1 match string */
		/*
		 * This code is pretty bad.  It permits only one --type
		 * option, and that could contain all sorts of garbage we
		 * we wouldn't notice.
		 */
		uint64_t type_bitmap = 0;
		extern char *v29_types_to_v1_match(char*, uint64_t);
		extern uint32_t convert_type_to_v29(uint32_t v1_type);

		if (strstr(type_tmp, "EVENT") != NULL) {
			notify_flag = notify_flag | TYPE_EVENTS;
		}
		if (strstr(type_tmp, "REPAIR") != NULL) {
			notify_flag = notify_flag | TYPE_REPAIRS;
		}
		if (strstr(type_tmp, "ppc64_rtas") != NULL) {
			type_bitmap |= (1 << convert_type_to_v29(SL_TYPE_RTAS));
		}
		if (strstr(type_tmp, "os") != NULL) {
			type_bitmap |= (1 << convert_type_to_v29(SL_TYPE_OS));
		}
		if (strstr(type_tmp, "ppc64_encl") != NULL) {
			type_bitmap |= (1 << convert_type_to_v29(SL_TYPE_ENCLOSURE));
		}
		if (type_bitmap) {
			next = v29_types_to_v1_match(next, type_bitmap);
			connector = " and ";
		}
	}

	if (action == ACTION_TOOMANY) {
		fprintf(stderr, "Only one of the --add, --remove, or "
			"--list options may be specified.\n\n");
		print_usage();
		exit(1);
	}

	if ((action == ACTION_QUERY) && ((!command) && (!flag_id))) {
		fprintf(stderr, "--query must be accompanies by --command='command path' or --id=.\n\n");
		print_usage();
		exit(1);
	}

	rc = servicelog_open(&servlog, 0);
	if (rc != 0) {
		fprintf(stderr, "%s\n", strerror(rc));
		return 2;
	}

	/* Added for v0.2.9 backwards compatibility */
	// Build a match string out of the provided params
	if (tSev) {
		next += snprintf(next, (end - next),
			"%s severity>=%s", connector, tSev);
		connector = " and ";
	}

	if (servicable) {
		if (servicable == SL_QUERY_NO) {
			next += snprintf(next, (end - next),
			"%s serviceable=0", connector);
			connector = " and ";
		} else if (servicable == SL_QUERY_YES) {
			next += snprintf(next, (end - next),
			"%s serviceable=1", connector);
			connector = " and ";
		}
	}

	switch (action) {

	case ACTION_ADD:
		/* bpeters:
		 * Note: In v0.2.9, repair and system event notifications could both be added in
		 * the same command line call. In v1, this is not supported.  To circumvent this,
		 * I do two separate action add calls, one for repair and one for events.
		 * Default is to add an event notification.
		 */
			/* Default to adding an event notification type */
			if (!notify_flag)
				notify_flag = TYPE_EVENTS;

			/* additional command line validation */
			if (flag_id) {
				fprintf(stderr, "The --id flag may not be used with "
					"the --add option.\n\n");
				print_usage();
				rc = 1;
				goto err_out;
			}
			if (command == NULL) {
				fprintf(stderr, "The --command flag must be specified "
					"with the --add option.\n\n");
				print_usage();
				rc = 1;
				goto err_out;
			}

			/* Must register two events, since in v1 EVENT and REPAIR cannot be done with 1 DB entry */
			/* set up an sl_notify struct and call servicelog_notify_log */
			if (notify_flag & TYPE_EVENTS) {
				notify = calloc(1, sizeof(struct sl_notify));
				if (!notify) {
					fprintf(stderr, "malloc failed, while"
					" allocating notify.\n\n");
					rc = 1;
					goto err_out;
				} /* calloc */
				notify->notify = SL_NOTIFY_EVENTS;
				notify->method = method;

				/* A v1 command line arg will take precedence */
				if (match)
					notify->match = strdup(match);
				else
					/*
					 * notify->match can't be NULL, but
					 * it can be an empty string.
					 */
					notify->match = strdup(type_match);

				notify->command = strdup(command);

				rc = servicelog_notify_log(servlog, notify, &id);
				servicelog_notify_free(notify);
				if (rc == 0) {
					printf("Event Notification Registration successful (id: ""%" PRIu64 ")\n", id);
				}
				else {
					fprintf(stderr, "%s\n", servicelog_error(servlog));
					rc = 2;
					goto err_out;
				}
			}

			if (notify_flag & TYPE_REPAIRS) {
				/* set up an sl_notify struct and call servicelog_notify_log */
				notify = calloc(1, sizeof(struct sl_notify));
				if (!notify) {
					fprintf(stderr, "malloc failed, while"
					" allocating notify.\n\n");
					rc = 1;
					goto err_out;
				} /* calloc */
				notify->notify = SL_NOTIFY_REPAIRS;
				notify->method = method;

				/* A v1 command line arg will take precedence */
				if (match)
					notify->match = strdup(match);
				else
					/*
					 * None of the filters in type_match
					 * (type, severity, serviceable)
					 * apply to repair_actions, but
					 * servicelog_notify_log() demands
					 * a non-NULL (but possibly empty)
					 * match string.
					 */
					notify->match = strdup("");

				notify->command = strdup(command);

				rc = servicelog_notify_log(servlog, notify, &id);
				servicelog_notify_free(notify);
				if (rc == 0) {
					printf("Repair Notification Registration successful (id: ""%" PRIu64 ")\n", id);
				}
				else {
					fprintf(stderr, "%s\n", servicelog_error(servlog));
					rc = 2;
					goto err_out;
				}
			}

		break;

	case ACTION_LIST:
	case ACTION_QUERY:
			/* additional command line validation */
			if ((command && flag_id) || add_flags) {
				fprintf(stderr, "Only one of the --command or --id "
					"flags may be specified with the --list "
					"or --query option.\n\n");
				print_usage();
				rc = 1;
				goto err_out;
			}

			/*
			 * Query the database.
			 *
			 * Note: ppc64-diag's ppc64_diag_setup script expects
			 * us to exit(1) if we print no notification tools...
			 * bugzilla #76334 notwithstanding.
			 */
			if (flag_id) {
				rc = servicelog_notify_get(servlog, id,
								&notify);
				if (rc) {
					fprintf(stderr, "%s\n",
						servicelog_error(servlog));
					goto err_out;
				} else if (notify == NULL) {
					fprintf(stderr, "Could not find a registered "
						"notification tool with the specified "
						"id (""%" PRIu64 ").\n", id);
					rc = 1;
					goto err_out;
				}
			}
			else if (command) {
				snprintf(query, 256, "command = '%s'", command);
				rc = servicelog_notify_query(servlog, query,
								&notify);
				if (rc) {
					fprintf(stderr, "%s\n",
						servicelog_error(servlog));
					goto err_out;
				} else if (notify == NULL) {
					fprintf(stderr, "Could not find a registered "
						"notification tool with the specified "
						"command ('%s').\n", command);
					rc = 1;
					goto err_out;
				}
			}
			else {
				rc = servicelog_notify_query(servlog, "id>0",
								&notify);
				if (rc) {
					fprintf(stderr, "%s\n",
						servicelog_error(servlog));
					goto err_out;
				} else if (notify == NULL) {
					fprintf(stderr, "There are no registered "
						"notification tools.\n");
					rc = 1;
					goto err_out;
				}
			}

			/* display the notification tools */
			servicelog_notify_print(stdout, notify, 2);
			servicelog_notify_free(notify);
		break;

	case ACTION_REMOVE:
		/* additional command line validation */
		if ((command == NULL) && (!flag_id)) {
			fprintf(stderr, "At least one of the --command or --id "
				"flags must be specified with the --remove "
				"option.\n\n");
			print_usage();
			rc = 1;
			goto err_out;
		}

		/* find the registered notification tool to be removed */
		if (flag_id) {
			rc = servicelog_notify_get(servlog, id, &notify);
			if (rc) {
				fprintf(stderr, "%s\n",
						servicelog_error(servlog));
				goto err_out;
			} else if (notify == NULL) {
				fprintf(stderr, "Could not find a registered "
					"notification tool with the specified "
					"id (""%" PRIu64 ").\n", id);
				rc = 1;
				goto err_out;
			}
		}
		else {		/* search for the notify tool by command name */
			snprintf(query, 256, "command = '%s'", command);
			rc = servicelog_notify_query(servlog, query, &notify);
			if (rc) {
				fprintf(stderr, "%s\n",
						servicelog_error(servlog));
				goto err_out;
			} else if (notify == NULL) {
				fprintf(stderr, "Could not find a registered "
					"notification tool with the specified "
					"command ('%s').\n", command);
				rc = 1;
				goto err_out;
			}
		}

		for (current = notify; current; current = current->next)
			servicelog_notify_delete(servlog, current->id);
		servicelog_notify_free(notify);
		break;

	default:
		fprintf(stderr, "Internal error; unknown action %d\n", action);
		rc = 1;
		goto err_out;
	}

err_out:
	servicelog_close(servlog);

	return rc;
}
