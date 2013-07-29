#ifndef TMMDB_H
#define TMMDB_H

#ifdef __cplusplus
extern "C" {
#endif
#define _GNU_SOURCE
#include <sys/types.h>
//#include <sys/socket.h>
#include <netinet/in.h>
//#include <arpa/inet.h>

#define TMMDB_DEFAULT_DATABASE "/usr/local/share/GeoIP2/GeoIP2-City.mmdb"

// *** the EXT_TYPE is wrong it should be type - 8 not type
#define BROKEN_TYPE (1)
// every pointer start at the MAX + 1 of the previous type. To extend the range
// only th 32bit ptr contains all of them.
#define BROKEN_PTR (0)

#define TMMDB_DTYPE_EXT (0)
#define TMMDB_DTYPE_PTR (1)
#define TMMDB_DTYPE_UTF8_STRING (2)
#define TMMDB_DTYPE_IEEE754_DOUBLE (3)
#define TMMDB_DTYPE_BYTES (4)
#define TMMDB_DTYPE_UINT16 (5)
#define TMMDB_DTYPE_UINT32 (6)
#define TMMDB_DTYPE_MAP (7)      /* HASH */
#define TMMDB_DTYPE_INT32 (8)
#define TMMDB_DTYPE_UINT64 (9)
#define TMMDB_DTYPE_UINT128 (10)
#define TMMDB_DTYPE_ARRAY (11)
#define TMMDB_DTYPE_CONTAINER (12)
#define TMMDB_DTYPE_END_MARKER (13)
#define TMMDB_DTYPE_BOOLEAN (14)
#define TMMDB_DTYPE_IEEE754_FLOAT (15)

#define TMMDB_DTYPE_MAX (TMMDB_DTYPE_IEEE754_FLOAT)

#define TMMDB_DATASECTION_NOOP_SIZE (16)

/* flags */
#define TMMDB_MODE_NOOP (0)
#define TMMDB_MODE_STANDARD     TMMDB_MODE_NOOP
#define TMMDB_MODE_MEMORY_CACHE TMMDB_MODE_NOOP
#define TMMDB_MODE_MEMORY_MAP (3)
#define TMMDB_MODE_MASK (7)

/* err codes */
#define TMMDB_SUCCESS (0)
#define TMMDB_OPENFILEERROR (-1)
#define TMMDB_CORRUPTDATABASE (-2)
#define TMMDB_INVALIDDATABASE (-3)
#define TMMDB_IOERROR (-4)
#define TMMDB_OUTOFMEMORY (-5)
#define TMMDB_UNKNOWNDATABASEFMT (-6)

/* Looks better */
#define TMMDB_TRUE (1)
#define TMMDB_FALSE (0)

/* */
#define TMMDB_DEBUG (0)

#if TMMDB_DEBUG
#define TMMDB_DBG_CARP(...) fprintf(stderr, __VA_ARGS__ );
#define TMMDB_DBG_ASSERT(ex) assert(#ex)
#else
#define TMMDB_DBG_CARP(...)
#define TMMDB_DBG_ASSERT(ex)
#endif

// This is the starting point for every search.
// It is like the hash to start the search. It may or may not the root hash
    typedef struct TMMDB_entry_s {
        struct TMMDB_s *mmdb;
        unsigned int offset;    /* usually pointer to the struct */
        //uint8_t const *ptr;             /* usually pointer to the struct */
    } TMMDB_entry_s;

// the root entry is the entry structure from a lookup
// think of it as the root of all informations about the IP.

    typedef struct {
        TMMDB_entry_s entry;
        int netmask;
    } TMMDB_root_entry_s;

// information about the database file.
    typedef struct TMMDB_s {
        uint32_t flags;
        int fd;
        int size;               // file size
        char *fname;
        const uint8_t *file_in_mem_ptr;
        int major_file_format;
        int minor_file_format;
        int database_type;
        uint32_t full_record_size_bytes;        /* recbits * 2 / 8 */
        int depth;
        int node_count;
        const uint8_t *dataptr;
        uint8_t *meta_data_content;
        struct TMMDB_s *fake_metadata_db;
        TMMDB_entry_s meta;     // should change to entry_s
    } TMMDB_s;

// this is the result for every field
    typedef struct TMMDB_return_s {
        /* return values */
        union {
            float float_value;
            double double_value;
            int sinteger;
            uint32_t uinteger;
            uint8_t c8[8];
            uint8_t c16[16];
            const void *ptr;
        };
        uint32_t offset;        /* start of our field or zero for not found */
        int data_size;          /* only valid for strings, utf8_strings or binary data */
        int type;               /* type like string utf8_string, int32, ... */
    } TMMDB_return_s;

    // The decode structure is like the result ( return_s ) but with the start
    // of the next entry. For example if we search for a key but this is the
    // wrong key.
    typedef struct TMMDB_decode_s {
        TMMDB_return_s data;
        uint32_t offset_to_next;
    } TMMDB_decode_s;

    typedef struct TMMDB_decode_all_s {
        TMMDB_decode_s decode;
        struct TMMDB_decode_all_s *next;
    } TMMDB_decode_all_s;

    extern int TMMDB_open(TMMDB_s ** mmdbp, const char *fname, uint32_t flags);
    extern void TMMDB_close(TMMDB_s * mmdb);
    extern int TMMDB_lookup_by_ipnum(uint32_t ipnum, TMMDB_root_entry_s * res);
    extern int TMMDB_lookup_by_ipnum_128(struct in6_addr ipnum,
                                         TMMDB_root_entry_s * result);

    extern int TMMDB_get_value(TMMDB_entry_s * start, TMMDB_return_s * result,
                               ...);
    extern int TMMDB_strcmp_result(TMMDB_s * mmdb,
                                   TMMDB_return_s const *const result,
                                   char *str);

    extern const char *TMMDB_lib_version(void);

    extern int TMMDB_dump(TMMDB_s * mmdb, TMMDB_decode_all_s * decode_all,
                          int indent);
    extern int TMMDB_get_tree(TMMDB_entry_s * start,
                              TMMDB_decode_all_s ** decode_all);
    extern TMMDB_decode_all_s *TMMDB_alloc_decode_all(void);
    extern void TMMDB_free_decode_all(TMMDB_decode_all_s * freeme);

    extern int TMMDB_resolve_address(const char *host, int ai_family,
                                     int ai_flags, void *ip);

#ifdef __cplusplus
}
#endif
#endif                          /* TMMDB_H */
