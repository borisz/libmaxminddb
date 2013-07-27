#!/usr/bin/python

import TMMDB
print TMMDB.lib_version();
mmdb =TMMDB.new("/usr/local/share/GeoIP2/GeoIP2-City.mmdb",TMMDB.TMMDB_MODE_MEMORY_CACHE )

print mmdb.lookup("24.24.24.24")

