xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/experimental
xlator_LTLIBRARIES = rioc.la

RIOCLIENT_SRC = $(top_srcdir)/xlators/experimental/rio/rio-client/src

rioc_sources = rio-client-main.c

RIOCOMMON_SRC = $(top_srcdir)/xlators/experimental/rio/rio-common/src
RIOCOMMON_BLD = $(top_builddir)/xlators/experimental/rio/rio-common/src

riocommon_sources = $(RIOCOMMON_SRC)/rio-common-helpers.c
riocommon_sources += $(RIOCOMMON_SRC)/rio-common-init.c
riocommon_sources += $(RIOCOMMON_SRC)/layout.c
riocommon_sources += $(RIOCOMMON_SRC)/layout-static-bucket.c
riocommon_sources += $(RIOCOMMON_SRC)/layout-inodehash-bucket.c

noinst_HEADERS_riocommon = $(RIOCOMMON_BLD)/rio-common.h \
        $(RIOCOMMON_BLD)/rio-common.h \
        $(RIOCOMMON_BLD)/rio-common-helpers.h \
        $(RIOCOMMON_BLD)/rio-mem-types.h \
        $(RIOCOMMON_BLD)/rio-messages.h \
        $(RIOCOMMON_BLD)/layout.h

noinst_HEADERS = $(top_builddir)/xlators/lib/src/libxlator.h \
        $(noinst_HEADERS_riocommon)

rioc_la_SOURCES = $(rioc_sources) $(riocommon_sources)

nodist_rioc_la_SOURCES = rio-client-autogen-fops.c rio-client-autogen-fops.h
BUILT_SOURCES = rio-client-autogen-fops.h

rioc_la_LDFLAGS = -module -avoid-version
rioc_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

AM_CFLAGS = -DRIO_DEBUG -Wall $(GF_CFLAGS)

AM_CPPFLAGS = $(GF_CPPFLAGS)
AM_CPPFLAGS += -I$(RIOCOMMON_SRC)/
AM_CPPFLAGS += -I$(top_srcdir)/libglusterfs/src
AM_CPPFLAGS += -I$(top_srcdir)/rpc/xdr/src
AM_CPPFLAGS += -I$(top_builddir)/rpc/xdr/src
AM_CPPFLAGS += -I$(top_srcdir)/xlators/lib/src

noinst_PYTHON = rio-client-fops-c.py rio-client-fops-h.py
EXTRA_DIST = rio-client-autogen-fops-tmpl.c rio-client-autogen-fops-tmpl.h

rio-client-autogen-fops.c: rio-client-fops-c.py rio-client-autogen-fops-tmpl.c
	$(PYTHON) $(RIOCLIENT_SRC)/rio-client-fops-c.py \
        $(RIOCLIENT_SRC)/rio-client-autogen-fops-tmpl.c > $@

rio-client-autogen-fops.h: rio-client-fops-h.py rio-client-autogen-fops-tmpl.h
	$(PYTHON) $(RIOCLIENT_SRC)/rio-client-fops-h.py \
        $(RIOCLIENT_SRC)/rio-client-autogen-fops-tmpl.h > $@

CLEANFILES = $(nodist_rioc_la_SOURCES)

uninstall-local:
	rm -f $(DESTDIR)$(xlatordir)/rioc.so
