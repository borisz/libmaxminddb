#include <stdlib.h>
#include "tinymmdb.h"
#include <netdb.h>
#include <stdio.h>
#include "test_helper.h"
#include <math.h>

// 0 == equal
int dbl_cmp(double a, double b)
{
    if (b)
        return (fabs((a - b) / b) >= 0.00025);
    return (fabs(a - b) >= 0.00025);
}

char *get_test_db_fname(void)
{
    char *fname = getenv("TMMDB_TEST_DATABASE");
    if (!fname)
        fname = TMMDB_DEFAULT_DATABASE;
    return fname;
}

void ip_to_num(TMMDB_s * mmdb, char *ipstr, in_addrX * dest_ipnum)
{
    int ai_family = mmdb->depth == 32 ? AF_INET : AF_INET6;
    int ai_flags = AI_V4MAPPED; // accept every crap

    if (ipstr == NULL || 0 != TMMDB_lookupaddressX(ipstr, ai_family, ai_flags,
                                                  dest_ipnum)) {
        fprintf(stderr, "Invalid IP\n");
        exit(1);
    }
}
