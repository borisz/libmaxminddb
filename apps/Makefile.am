AM_CPPFLAGS =      \
        -I$(top_srcdir)/libtinymmdb

bin_PROGRAMS = mmdblookup mmdbdump country_lookup

mmdblookup_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
mmdblookup_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
mmdblookup_SOURCES = mmdblookup.c tinymmdb_helper.c

mmdblookup.lo mmdblookup.o: mmdblookup.c

mmdbdump_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
mmdbdump_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
mmdbdump_SOURCES = mmdbdump.c tinymmdb_helper.c
mmdbdump.lo mmdbdump.o: mmdbdump.c

country_lookup_DEPENDENCIES = $(top_builddir)/libtinymmdb/libtinymmdb.la
country_lookup_LDADD = $(top_builddir)/libtinymmdb/libtinymmdb.la -lc
country_lookup_SOURCES = country_lookup.c tinymmdb_helper.c
coutry_lookup.lo country_lookup.o: country_lookup.c

tinymmdb_helper.lo tinymmdb_helper.o: tinymmdb_helper.c

