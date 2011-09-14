bin_PROGRAMS =a68g
a68g_SOURCES = a68g.c code.c environ.c genie.c gsl.c monitor.c mp.c plotutils.c postgresql.c syntax.c edit.c
a68g_CFLAGS = -DBINDIR='"$(bindir)"' -DINCLUDEDIR='"$(includedir)"'
TESTS_ENVIRONMENT=./a68g
TESTS=check-01.a68 check-02.a68 check-03.a68 check-04.a68 check-05.a68 check-06.a68 check-07.a68 check-08.a68 check-09.a68

if EXPORT_DYNAMIC
a68g_LDFLAGS = -Wl,--export-dynamic
else
a68g_LDFLAGS =
endif

pkginclude_HEADERS= a68g-config.h a68g.h
man_MANS = a68g.1
EXTRA_DIST = $(man_MANS) check-01.a68 check-02.a68 check-03.a68 check-04.a68 check-05.a68 check-06.a68 check-07.a68 check-08.a68 check-09.a68
