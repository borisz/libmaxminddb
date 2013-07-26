#include "tinymmdb.h"
#include "tap.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include "test_helper.h"

int main(void)
{
    char *fname = get_test_db_fname();

    struct stat sstat;
    int err = stat(fname, &sstat);
    ok(err == 0, "%s exists", fname);

    TMMDB_s *mmdb = TMMDB_open(fname, TMMDB_MODE_MEMORY_CACHE);
    // TMMDB_s *mmdb = TMMDB_open(fname, TMMDB_MODE_STANDARD);
    ok(mmdb != NULL, "TMMDB_open successful");
    if (mmdb) {

        TMMDB_decode_all_s *decode_all;
        int err = TMMDB_get_tree(&mmdb->meta, &decode_all);
        if (err == TMMDB_SUCCESS) {
            if (decode_all != NULL)
                TMMDB_dump(mmdb, decode_all, 0);
            TMMDB_free_decode_all(decode_all);
        }
    }
    done_testing();
}
