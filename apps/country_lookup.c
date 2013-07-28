#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tinymmdb.h"
#include "tinymmdb_helper.h"
#include "getopt.h"

// country lookup is not useful, it is just to see how fast it is compared to
// 106

int main(int argc, char *const argv[])
{
    int verbose = 0;
    int character;
    char *fname = NULL;

    while ((character = getopt(argc, argv, "vf:")) != -1) {
        switch (character) {
        case 'v':
            verbose = 1;
            break;
        case 'f':
            fname = strdup(optarg);
            break;
        default:
        case '?':
            usage(argv[0]);
        }
    }
    argc -= optind;
    argv += optind;

    TMMDB_s *mmdb;
    int status = TMMDB_open(&mmdb, fname ? fname : TMMDB_DEFAULT_DATABASE,
                            TMMDB_MODE_MEMORY_CACHE);

    if (status == TMMDB_SUCCESS) {
        fprintf(stderr, "Can't open %s ( %d )\n", fname, status);
        exit(1);
    }

    for (int i = 1; i <= 10000000; i++) {

        TMMDB_root_entry_s root = {.entry.mmdb = mmdb };
//    int err = TMMDB_lookup_by_ipnum(404232216, &root);
        uint32_t ipnum = htonl(rand());
        int err = TMMDB_lookup_by_ipnum(ipnum, &root);
        if (err == TMMDB_SUCCESS) {
            char *name, *code;
            if (root.entry.offset > 0) {
                TMMDB_return_s country;
                TMMDB_get_value(&root.entry, &country, "country", NULL);
                TMMDB_entry_s start = {.mmdb = mmdb,.offset = country.offset };
                if (country.offset) {
                    TMMDB_return_s res;
                    TMMDB_get_value(&start, &res, "code", NULL);
                    code = bytesdup(mmdb, &res);
                    TMMDB_get_value(&start, &res, "name", "en", NULL);
                    name = bytesdup(mmdb, &res);

//                    printf( "%u %s %s\n", ipnum , code, name);

                    if (name)
                        free(name);
                    if (code)
                        free(code);
                }
            } else {
                ;               // not found
            }
        }
    }

    if (fname)
        free(fname);

    return (0);
}
