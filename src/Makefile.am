AM_CFLAGS = -Wall 
AM_LDFLAGS = -lservicelog -lrtas -lrtasevent -lsqlite3

bin_PROGRAMS = servicelog servicelog_notify log_repair_action \
	servicelog_manage v1_servicelog v29_servicelog
sbin_PROGRAMS = slog_common_event

servicelog_SOURCES = servicelog_switch.c
v1_servicelog_SOURCES = servicelog.c
v29_servicelog_SOURCES = v29_servicelog.c
servicelog_notify_SOURCES = servicelog_notify.c
log_repair_action_SOURCES = log_repair_action.c
slog_common_event_SOURCES = slog_common_event.c
servicelog_manage_SOURCES = v29_servicelog_manage.c
