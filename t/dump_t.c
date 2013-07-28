#include "tinymmdb.h"
#include "tap.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include "test_helper.h"

int main(void)
{
    in_addrX ipnum;
    struct stat sstat;
    char *fname = get_test_db_fname();
    int err = stat(fname, &sstat);
    ok(err == 0, "%s exists", fname);

    TMMDB_s *mmdb;
    int status = TMMDB_open(&mmdb, fname, TMMDB_MODE_MEMORY_CACHE);
    ok(status == TMMDB_SUCCESS, "TMMDB_open successful (ERRCODE: SUCCESS)");
    ok(mmdb != NULL, "TMMDB_open successful");
    if (mmdb) {

        TMMDB_root_entry_s root = {.entry.mmdb = mmdb };
        char *ipstr = "24.24.24.24";
        ip_to_num(mmdb, ipstr, &ipnum);
        err = TMMDB_lookup_by_ipnum_128(ipnum.v6, &root);
        ok(err == TMMDB_SUCCESS, "Search for %s SUCCESSFUL", ipstr);
        ok(root.entry.offset > 0, "Found something %s good", ipstr);
        TMMDB_decode_all_s *decode_all;
        int err = TMMDB_get_tree(&root.entry, &decode_all);
        if (err == TMMDB_SUCCESS) {
            if (decode_all != NULL)
                TMMDB_dump(mmdb, decode_all, 0);
            TMMDB_free_decode_all(decode_all);
        }
    }
    done_testing();
}
