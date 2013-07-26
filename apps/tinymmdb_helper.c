#include "tinymmdb_helper.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int is_ipv4(TMMDB_s * mmdb)
{
    return mmdb->depth == 32;
}

char *bytesdup(TMMDB_s * mmdb, TMMDB_return_s const *const ret)
{
    char *mem = NULL;

    if (ret->offset) {
        mem = malloc(ret->data_size + 1);

        memcpy(mem, ret->ptr, ret->data_size);
        mem[ret->data_size] = '\0';
    }
    return mem;
}

int addr_to_num(char *addr, struct in_addr *result)
{
    return inet_pton(AF_INET, addr, result);
}

int addr6_to_num(char *addr, struct in6_addr *result)
{
    return inet_pton(AF_INET6, addr, result);
}

void usage(char *prg)
{
    die("Usage: %s -f database addr\n", prg);
}

void dump_meta(TMMDB_s * mmdb)
{
    TMMDB_decode_all_s *decode_all = calloc(1, sizeof(TMMDB_decode_all_s));
    int err = TMMDB_get_tree(&mmdb->meta, &decode_all);
    assert(err == TMMDB_SUCCESS);

    if (decode_all != NULL)
        TMMDB_dump(NULL, decode_all, 0);
    free(decode_all);
}

static const char *na(char const *string)
{
    return string ? string : "N/A";
}

void dump_ipinfo(const char *ipstr, TMMDB_root_entry_s * ipinfo)
{

    char *city, *country, *region;
    double dlat, dlon;
    dlat = dlon = 0;
    if (ipinfo->entry.offset > 0) {
        TMMDB_return_s res_location;
        TMMDB_get_value(&ipinfo->entry, &res_location, "location", NULL);
        // TODO handle failed search somehow.
        TMMDB_return_s lat, lon;
        TMMDB_entry_s location = {.mmdb = ipinfo->entry.mmdb,.offset =
                res_location.offset
        };
        if (res_location.offset) {
            TMMDB_get_value(&location, &lat, "latitude", NULL);
            TMMDB_get_value(&location, &lon, "longitude", NULL);
            if (lat.offset)
                dlat = lat.double_value;
            if (lon.offset)
                dlon = lon.double_value;
        }

        TMMDB_return_s res;
        TMMDB_get_value(&ipinfo->entry, &res, "city", "names", "en", NULL);
        city = bytesdup(ipinfo->entry.mmdb, &res);
        TMMDB_get_value(&ipinfo->entry, &res, "country", "names", "en", NULL);
        country = bytesdup(ipinfo->entry.mmdb, &res);

        TMMDB_get_value(&ipinfo->entry, &res, "subdivisions", "0", "names", "en",
                       NULL);
        region = bytesdup(ipinfo->entry.mmdb, &res);

        printf("%s %f %f %s %s %s\n", ipstr, dlat, dlon,
               na(region), na(city), na(country));
        free_list(city, country, region);
    } else {
        puts("Sorry, nothing found");   // not found
    }
}
