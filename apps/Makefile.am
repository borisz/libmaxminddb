AM_CPPFLAGS =      \
        -I$(top_srcdir)/libtinymmdb

bin_PROGRAMS = tmmdblookup tmmdbdump country_lookup

tmmdblookup_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
tmmdblookup_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
tmmdblookup_SOURCES = tmmdblookup.c tinymmdb_helper.c

tmmdblookup.lo tmmdblookup.o: tmmdblookup.c

tmmdbdump_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
tmmdbdump_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
tmmdbdump_SOURCES = tmmdbdump.c tinymmdb_helper.c
tmmdbdump.lo tmmdbdump.o: tmmdbdump.c

country_lookup_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
country_lookup_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
country_lookup_SOURCES = country_lookup.c tinymmdb_helper.c
coutry_lookup.lo country_lookup.o: country_lookup.c

tinymmdb_helper.lo tinymmdb_helper.o: tinymmdb_helper.c

