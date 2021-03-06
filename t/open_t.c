#include "tinymmdb.h"
#include "tap.h"
#include <sys/stat.h>
#include "test_helper.h"

int main(void)
{
    TMMDB_s *mmdb_m, *mmdb_s;
    char *fname = get_test_db_fname();
    struct stat sstat;
    int err = stat(fname, &sstat);
    ok(err == 0, "%s exists", fname);

    int status = TMMDB_open(&mmdb_m, fname, TMMDB_MODE_MEMORY_CACHE);
    ok(status == TMMDB_SUCCESS,
       "TMMDB_open successful ( TMMDB_MODE_MEMORY_CACHE ERRCODE: SUCCESS)");
    ok(mmdb_m != NULL, "TMMDB_open successful ( TMMDB_MODE_MEMORY_CACHE )");

    status = TMMDB_open(&mmdb_s, fname, TMMDB_MODE_STANDARD);
    ok(status == TMMDB_SUCCESS,
       "TMMDB_open successful ( TMMDB_MODE_STANDARD ERRCODE: SUCCESS)");
    ok(mmdb_s != NULL, "TMMDB_open successful ( TMMDB_MODE_STANDARD )");

    if (mmdb_m && mmdb_s) {
        int rec_size_m = mmdb_m->full_record_size_bytes;
        int rec_size_s = mmdb_s->full_record_size_bytes;

        ok((rec_size_s == 6 || rec_size_s == 7
            || rec_size_s == 8),
           "full_record_size_bytes = %d TMMDB_MODE_STANDARD", rec_size_s);
        ok((rec_size_m == 6 || rec_size_m == 7
            || rec_size_m == 8),
           "full_record_size_bytes = %d TMMDB_MODE_MEMORY_CACHE", rec_size_m);

        ok(rec_size_m == rec_size_s, "full_record_size_bytes are the same");

        ok(mmdb_s->node_count == mmdb_m->node_count,
           "node_count are the same ( %d == %d )", mmdb_m->node_count,
           mmdb_s->node_count);

    }
    done_testing();
}
