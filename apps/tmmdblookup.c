#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tinymmdb.h"
#include "tinymmdb_helper.h"
#include "getopt.h"
#include <assert.h>
#include <netdb.h>

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
        fname = strdup(TMMDB_DEFAULT_DATABASE);
    }

    assert(fname != NULL);

    TMMDB_s *mmdb;
    int status = TMMDB_open(&mmdb, fname, TMMDB_MODE_STANDARD);
    if (status != TMMDB_SUCCESS)
        die("Can't open %s ( %d )\n", fname, status);

    free(fname);

    char *ipstr = argv[0];
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } ip;

    int ai_family = is_ipv4(mmdb) ? AF_INET : AF_INET6;
    int ai_flags = AI_V4MAPPED; // accept everything

    if (ipstr == NULL || 0 != TMMDB_resolve_address(ipstr, ai_family, ai_flags,
                                                    &ip)) {
        fprintf(stderr, "Invalid IP\n");
        exit(1);
    }

    if (verbose) {
        dump_meta(mmdb);
    }

    TMMDB_root_entry_s root = {.entry.mmdb = mmdb };
    status = is_ipv4(mmdb)
        ? TMMDB_lookup_by_ipnum(htonl(ip.v4.s_addr), &root)
        : TMMDB_lookup_by_ipnum_128(ip.v6, &root);

    if (status == TMMDB_SUCCESS) {
        dump_ipinfo(ipstr, &root);
    }

    return (0);
}
