#include "tinymmdb.h"
#include "tap.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include "test_helper.h"

char *ip4_string[][2] = {
    {"127.0.0.1", "24.24.24.24",},
    {NULL, NULL}
};

char *ip6_string[][2] = {
    {"127.0.0.1", "24.24.24.24",},
    {"::127.0.0.1", "::24.24.24.24",},
    {"::ffff:127.0.0.1", "::ffff:24.24.24.24"},
    {"2222::", "2001:4860:b002::68"},
    {NULL, NULL}
};

static void check_illegal_index(TMMDB_entry_s * entry, int idx)
{
    TMMDB_return_s result;
    char string[256];
    snprintf(string, sizeof(string), "%d", idx);
    TMMDB_get_value(entry, &result, string, NULL);
    ok(result.offset == 0, "Great nothing found");
}

void test_mmdb(TMMDB_s * mmdb)
{
    char *ipstr;
    char *(*ip_string)[2] = mmdb->depth == 32 ? ip4_string : ip6_string;
    for (int i = 0; (ipstr = ip_string[i][0]); i++) {
        in_addrX ipnum;
        TMMDB_root_entry_s root = {.entry.mmdb = mmdb };
        ip_to_num(mmdb, ipstr, &ipnum);
        int err =
            mmdb->depth == 32 ? TMMDB_lookup_by_ipnum(ipnum.v4.s_addr, &root)
            : TMMDB_lookup_by_ipnum_128(ipnum.v6, &root);
        ok(err == TMMDB_SUCCESS, "Search for %s SUCCESSFUL", ipstr);
        ok(root.entry.offset == 0, "No entries found for %s good", ipstr);
        ipstr = ip_string[i][1];
        ip_to_num(mmdb, ipstr, &ipnum);
        err = mmdb->depth == 32 ? TMMDB_lookup_by_ipnum(ipnum.v4.s_addr, &root)
            : TMMDB_lookup_by_ipnum_128(ipnum.v6, &root);
        ok(err == TMMDB_SUCCESS, "Search for %s SUCCESSFUL", ipstr);
        ok(root.entry.offset > 0, "Found something %s good", ipstr);
        TMMDB_return_s country;
        TMMDB_get_value(&root.entry, &country, "country", NULL);
        ok(country.offset > 0, "Found country hash for %s", ipstr);
        if (country.offset) {
            TMMDB_entry_s country_hash = {.mmdb = mmdb,.offset = country.offset
            };
            TMMDB_return_s result;
            TMMDB_get_value(&country_hash, &result, "iso_code", NULL);
            ok(result.offset > 0, "Found country code for %s", ipstr);
            if (result.offset > 0) {
                ok(TMMDB_strcmp_result(mmdb, &result, "US") == 0,
                   "Country code is US for %s", ipstr);
            }
            TMMDB_get_value(&country_hash, &result, "names", "ascii", NULL);
            if (result.offset > 0) {
                ok(TMMDB_strcmp_result(mmdb, &result, "United States") == 0,
                   "Country name ascii match United States for %s", ipstr);
            }
            TMMDB_get_value(&country_hash, &result, "names", "de", NULL);
            if (result.offset > 0) {
                ok(TMMDB_strcmp_result(mmdb, &result, "USA") == 0,
                   "Country name de is USA for %s", ipstr);
            }
            TMMDB_get_value(&country_hash, &result, "names", "es", NULL);
            if (result.offset > 0) {
                ok(TMMDB_strcmp_result(mmdb, &result, "Estados Unidos") == 0,
                   "Country name es is Estados Unidos for %s", ipstr);
            }
            TMMDB_get_value(&country_hash, &result, "names", "whatever", NULL);
            ok(result.offset == 0,
               "Country name whatever is not found for %s", ipstr);

            TMMDB_return_s military, cellular, flt, dbl;

            TMMDB_get_value(&root.entry, &military, "traits", "is_military",
                            NULL);
            ok(military.offset != 0, "traits/is_military _is_ found for %s",
               ipstr);

            ok(military.type == TMMDB_DTYPE_BOOLEAN,
               "is_military.type _is_ TMMDB_DTYPE_BOOLEAN for %s", ipstr);

            ok(military.uinteger == 0, "traits/is_military _is_ 0");

            TMMDB_get_value(&root.entry, &cellular, "traits", "cellular", NULL);
            ok(cellular.offset != 0, "traits/cellular _is_ found for %s",
               ipstr);

            ok(cellular.type == TMMDB_DTYPE_BOOLEAN,
               "cellular.type _is_ TMMDB_DTYPE_BOOLEAN for %s", ipstr);

            ok(cellular.uinteger == 1, "traits/is_cellular _is_ 1");

            TMMDB_get_value(&root.entry, &flt, "test_data", "max", "float_t",
                            NULL);
            ok(flt.offset != 0, "test_data/max/float_t _is_ found for %s",
               ipstr);

            ok(flt.type == TMMDB_DTYPE_IEEE754_FLOAT,
               "flt.type _is_ TMMDB_DTYPE_IEEE754_FLOAT for %s", ipstr);

            ok(!dbl_cmp(flt.float_value, 9999.99),
               "test_data/max/float_t _is_ nearly 9999.99");

            TMMDB_get_value(&root.entry, &dbl, "test_data", "max", "double_t",
                            NULL);
            ok(dbl.offset != 0, "test_data/max/double_t _is_ found for %s",
               ipstr);

            ok(dbl.type == TMMDB_DTYPE_IEEE754_DOUBLE,
               "dbl.type _is_ TMMDB_DTYPE_IEEE754_DOUBLE for %s", ipstr);

            ok(!dbl_cmp(dbl.double_value, 999999999.9999),
               "test_data/max/double_t _is_ nearly 999999999.9999");

            {
                double expect[] =
                    { 0.000000, 0.000000, 1.000000, 0.100000, 0.123000,
                    10.000000, 7.990000, 1000000000.000000, -1.000000,
                        -0.100000,
                    -0.123000, -10.000000, -7.990000, -1000000000.000000
                };
                int cnt = sizeof(expect) / sizeof(double);

                TMMDB_return_s got;
                TMMDB_get_value(&root.entry, &dbl, "test_data", "tst",
                                "array_ieee754_double_t", NULL);
                ok(dbl.offset > 0, "Found it");

                if (dbl.offset > 0) {
                    TMMDB_entry_s dbl_array = {.mmdb = mmdb,.offset = dbl.offset
                    };
                    for (int i = 0; i < cnt; i++) {
                        char idx[256];
                        snprintf(idx, sizeof(idx), "%d", i);
                        TMMDB_get_value(&dbl_array, &got, idx, NULL);
                        ok(got.offset != 0, "Found something");
                        ok(got.type == TMMDB_DTYPE_IEEE754_DOUBLE,
                           "type ok (is %d)", got.type);
                        ok(!dbl_cmp(got.double_value, expect[i]),
                           "test_data/max/double_t _is_ nearly %f", expect[i]);
                    }

                    check_illegal_index(&dbl_array, -1);
                    check_illegal_index(&dbl_array, cnt);
                }
            }
        }
    }
}

int main(void)
{
    char *fnames[] =
        { "./data/v4-24.mmdb", "./data/v4-28.mmdb", "./data/v4-32.mmdb",
        "./data/v6-24.mmdb", "./data/v6-28.mmdb", "./data/v6-32.mmdb", NULL
    };

    char *fname;
    for (char **ptr = fnames; (fname = *ptr++);) {
        struct stat sstat;
        int err = stat(fname, &sstat);
        ok(err == 0, "%s exists", fname);

        TMMDB_s *mmdb_m;
        int status = TMMDB_open(&mmdb_m, fname, TMMDB_MODE_MEMORY_CACHE);
        ok(status == TMMDB_SUCCESS,
           "TMMDB_open successful ( TMMDB_MODE_MEMORY_CACHE ERRCODE: SUCCESS)");
        ok(mmdb_m != NULL, "TMMDB_open successful ( TMMDB_MODE_MEMORY_CACHE )");
        if (mmdb_m) {
            test_mmdb(mmdb_m);
            TMMDB_close(mmdb_m);
        }

        TMMDB_s *mmdb_s;
        status = TMMDB_open(&mmdb_s, fname, TMMDB_MODE_STANDARD);
        ok(status == TMMDB_SUCCESS,
           "TMMDB_open successful ( TMMDB_MODE_STANDARD ERRCODE: SUCCESS)");
        ok(mmdb_s != NULL, "TMMDB_open successful ( TMMDB_MODE_STANDARD )");
        if (mmdb_s) {
            test_mmdb(mmdb_s);
            TMMDB_close(mmdb_s);
        }
    }
    done_testing();
}
