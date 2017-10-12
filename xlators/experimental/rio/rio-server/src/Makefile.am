xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/experimental
xlator_LTLIBRARIES = rios-mds.la rios-ds.la

RIOSERVER_SRC = $(top_srcdir)/xlators/experimental/rio/rio-server/src

rios_mds_sources = rio-server-mds-main.c
rios_mds_sources += rio-server-mds-fops.c
rios_mds_sources += rio-server-common.c

rios_ds_sources = rio-server-ds-main.c
rios_ds_sources += rio-server-common.c

nodist_rios_ds_la_SOURCES = rio-server-ds-autogen-fops.c
nodist_rios_ds_la_SOURCES += rio-server-ds-autogen-fops.h
BUILT_SOURCES = rio-server-ds-autogen-fops.h

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
        $(RIOCOMMON_BLD)/layout.h \
        rio-server-common.h \
        rio-server-mds-fops.h

noinst_HEADERS = $(top_builddir)/xlators/lib/src/libxlator.h \
        $(noinst_HEADERS_riocommon)

rios_mds_la_SOURCES = $(rios_mds_sources) $(riocommon_sources)
rios_mds_la_LDFLAGS = -module -avoid-version
rios_mds_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

rios_ds_la_SOURCES = $(rios_ds_sources) $(riocommon_sources)
rios_ds_la_LDFLAGS = -module -avoid-version
rios_ds_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

AM_CFLAGS = -DRIO_DEBUG -Wall $(GF_CFLAGS)

AM_CPPFLAGS = $(GF_CPPFLAGS)
AM_CPPFLAGS += -I$(RIOCOMMON_SRC)
AM_CPPFLAGS += -I$(top_srcdir)/libglusterfs/src
AM_CPPFLAGS += -I$(top_srcdir)/rpc/xdr/src
AM_CPPFLAGS += -I$(top_builddir)/rpc/xdr/src
AM_CPPFLAGS += -I$(top_srcdir)/xlators/lib/src

noinst_PYTHON = rio-server-ds-fops-c.py rio-server-ds-fops-h.py
EXTRA_DIST = rio-server-ds-autogen-fops-tmpl.c rio-server-ds-autogen-fops-tmpl.h

rio-server-ds-autogen-fops.c: rio-server-ds-fops-c.py rio-server-ds-autogen-fops-tmpl.c
	$(PYTHON) $(RIOSERVER_SRC)/rio-server-ds-fops-c.py \
        $(RIOSERVER_SRC)/rio-server-ds-autogen-fops-tmpl.c > $@

rio-server-ds-autogen-fops.h: rio-server-ds-fops-h.py rio-server-ds-autogen-fops-tmpl.h
	$(PYTHON) $(RIOSERVER_SRC)/rio-server-ds-fops-h.py \
        $(RIOSERVER_SRC)/rio-server-ds-autogen-fops-tmpl.h > $@

CLEANFILES = $(nodist_rios_ds_la_SOURCES)

uninstall-local:
	rm -f $(DESTDIR)$(xlatordir)/rios-mds.so
	rm -f $(DESTDIR)$(xlatordir)/rios-ds.so
