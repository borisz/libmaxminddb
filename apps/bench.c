#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "MMDB.h"
#include "MMDB_Helper.h"
#include "getopt.h"
#include <assert.h>
#include <netdb.h>
#include <sys/time.h>

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

    if (!fname) {
        fname = strdup(MMDB_DEFAULT_DATABASE);
    }

    assert(fname != NULL);

    MMDB_s *mmdb = MMDB_open(fname, MMDB_MODE_MEMORY_CACHE);

    if (!mmdb)
        die("Can't open %s\n", fname);

    free(fname);

    char *ipstr = argv[0];
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } ip;

    struct timeval tv_start, tv_stop;
    gettimeofday(&tv_start, NULL);

    int ai_family = is_ipv4(mmdb) ? AF_INET : AF_INET6;
    int ai_flags = AI_V4MAPPED | AI_NUMERICHOST;        // accept everything

    int count = 1000000;

    for (int i = 0; i < count; i++) {
        if (ipstr == NULL
            || 0 != MMDB_lookupaddressX(ipstr, ai_family, ai_flags, &ip)) {
            fprintf(stderr, "Invalid IP\n");
            exit(1);
        }

        MMDB_root_entry_s root = {.entry.mmdb = mmdb };
        int status = is_ipv4(mmdb)
            ? MMDB_lookup_by_ipnum(htonl(ip.v4.s_addr), &root)
            : MMDB_lookup_by_ipnum_128(ip.v6, &root);

        if (status == MMDB_SUCCESS && root.entry.offset > 0) {
            MMDB_decode_all_s *decode_all;
            status = MMDB_get_tree(&root.entry, &decode_all);
            assert(status == MMDB_SUCCESS);

            MMDB_free_decode_all(decode_all);
        }
    }

    gettimeofday(&tv_stop, NULL);
    double reqs_sec =
        count / (((double)tv_stop.tv_sec + tv_stop.tv_usec / 1000000.0)
                 - ((double)tv_start.tv_sec + tv_start.tv_usec / 1000000.0));

    printf("reqs/sec %.2f\n", reqs_sec);

#if 0
    if (status == MMDB_SUCCESS) {
        dump_ipinfo(ipstr, &root);
    }
#endif

    return (0);
}
