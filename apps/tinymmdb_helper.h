#ifndef TINYMMDB_HELPER
#define TINYMMDB_HELPER (1)
#include "tinymmdb.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/* dummy content */

#define die(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(1); \
} while(0)

#define free_list(...) do{ \
    { \
        void *ptr[] = { __VA_ARGS__ }; \
        for (int i = 0; i < sizeof(ptr)/sizeof(void *); i++){ \
            if (ptr[i]) \
                free(ptr[i]); \
        } \
    } \
}while(0)

int addr_to_num(char *addr, struct in_addr *result);
int addr6_to_num(char *addr, struct in6_addr *result);
char *bytesdup(TMMDB_s * mmdb, TMMDB_return_s const *const ret);
void dump_ipinfo(const char *ipstr, TMMDB_root_entry_s * ipinfo);
void dump_meta(TMMDB_s * mmdb);
void usage(char *prg);
int is_ipv4(TMMDB_s * mmdb);
#endif
