AM_CFLAGS = -Wall 
AM_LDFLAGS = -lservicelog -lrtas -lrtasevent -lsqlite3

bin_PROGRAMS = servicelog servicelog_notify log_repair_action \
	servicelog_manage v1_servicelog v29_servicelog
sbin_PROGRAMS = slog_common_event

platform_SOURCES = platform.c platform.h
servicelog_SOURCES = servicelog_switch.c $(platform_SOURCES)
v1_servicelog_SOURCES = servicelog.c $(platform_SOURCES)
v29_servicelog_SOURCES = v29_servicelog.c $(platform_SOURCES)
servicelog_notify_SOURCES = servicelog_notify.c $(platform_SOURCES)
log_repair_action_SOURCES = log_repair_action.c $(platform_SOURCES)
slog_common_event_SOURCES = slog_common_event.c $(platform_SOURCES)
servicelog_manage_SOURCES = v29_servicelog_manage.c $(platform_SOURCES)
