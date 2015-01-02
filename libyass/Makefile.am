
AM_CFLAGS = \
	-fvisibility=hidden \
	-DSCHED_DIR=\"$(libdir)/yass/\" \
	-DDATA_DIR=\"$(datadir)/yass/data/\" \
	-DPROCESSORS_DIR=\"$(datadir)/yass/processors/\" \
	-DTESTS_DIR=\"$(datadir)/yass/tests/\" \
	$(JANSSON_CFLAGS)

AM_LDFLAGS = \
	-version-info 0:1:0 \
	-ldl -lm -lpthread \
	$(JANSSON_LIBS)

lib_LTLIBRARIES = libyass.la

libyass_la_SOURCES = \
	cpu.c \
	helpers.c \
	list.c \
	log.c \
	private.h \
	scheduler.c \
	task.c \
	yass.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libyass.pc

includedir = $(prefix)/include/libyass/libyass/
include_HEADERS = \
	common.h \
	cpu.h \
	helpers.h \
	list.h \
	log.h \
	scheduler.h \
	task.h \
	yass.h
