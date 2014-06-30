# Copyright (C) 2008, IBM
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# Licence along with this program; if not, see <http://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I m4

man_MANS = man/servicelog.8 man/servicelog_notify.8 \
	   man/log_repair_action.8 man/servicelog_manage.8

bin_PROGRAMS = src/servicelog src/v1_servicelog src/v29_servicelog \
	       src/servicelog_notify src/log_repair_action \
	       src/servicelog_manage

sbin_PROGRAMS = src/slog_common_event

platform_SOURCES = src/platform.c src/platform.h

src_servicelog_SOURCES = src/servicelog_switch.c $(platform_SOURCES)
src_servicelog_LDADD = -lservicelog -lsqlite3

src_v1_servicelog_SOURCES = src/servicelog.c $(platform_SOURCES)
src_v1_servicelog_LDADD = -lservicelog -lsqlite3

src_v29_servicelog_SOURCES = src/v29_servicelog.c $(platform_SOURCES)
src_v29_servicelog_LDADD = -lservicelog -lsqlite3

src_servicelog_notify_SOURCES = src/servicelog_notify.c $(platform_SOURCES)
src_servicelog_notify_LDADD = -lservicelog -lsqlite3

src_log_repair_action_SOURCES = src/log_repair_action.c $(platform_SOURCES)
src_log_repair_action_LDADD = -lservicelog -lsqlite3

src_servicelog_manage_SOURCES = src/v29_servicelog_manage.c $(platform_SOURCES)
src_servicelog_manage_LDADD = -lservicelog -lsqlite3

src_slog_common_event_SOURCES = src/slog_common_event.c $(platform_SOURCES)
src_slog_common_event_LDADD = -lservicelog -lsqlite3

EXTRA_DIST = $(man_MANS) bootstrap.sh
