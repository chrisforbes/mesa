################################################################################

# Meta-driver which combines whichever software rasterizers have been
# built into a single convenience library.

include Makefile.sources
include $(top_srcdir)/src/gallium/Automake.inc

AM_CFLAGS = \
	$(GALLIUM_DRIVER_CFLAGS)

noinst_LTLIBRARIES = libgalahad.la

libgalahad_la_SOURCES = $(C_SOURCES)

EXTRA_DIST = SConscript
