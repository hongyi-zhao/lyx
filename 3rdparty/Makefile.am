include $(top_srcdir)/config/common.am

DIST_SUBDIRS = dtl hunspell mythes libiconv zlib nod

if USE_INCLUDED_NOD
NOD = nod
endif

if USE_INCLUDED_HUNSPELL
HUNSPELL = hunspell
endif

if USE_INCLUDED_MYTHES
MYTHES = mythes
endif

if USE_INCLUDED_ICONV
ICONV = libiconv
endif

if USE_INCLUDED_ZLIB
ZLIB = zlib
endif

if BUILD_INCLUDED_DTL
DTL=dtl
endif

SUBDIRS = $(DTL) $(HUNSPELL) $(MYTHES) $(ICONV) $(ZLIB) $(NOD)

EXTRA_DIST = \
	scripts/evince_sync/evince_backward_search \
	scripts/evince_sync/evince_forward_search \
	scripts/evince_sync/evince_sync_lyx \
	scripts/evince_sync/README

