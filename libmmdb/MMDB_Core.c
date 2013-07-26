#include "MMDB.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/mman.h>
#if HAVE_CONFIG_H
# include <config.h>
#endif

#define KEYS(...) __VA_ARGS__, NULL

#if MMDB_DEBUG
#define LOCAL
#else
#define LOCAL static
#endif

// prototypes
//
LOCAL void DPRINT_KEY(MMDB_s * mmdb, MMDB_return_s * data);

LOCAL uint32_t get_uint_value(MMDB_entry_s * start, ...);
LOCAL void skip_hash_array(MMDB_s * mmdb, MMDB_decode_s * decode);

LOCAL int get_tree(MMDB_s * mmdb, uint32_t offset, MMDB_decode_all_s * decode);
int MMDB_vget_value(MMDB_entry_s * start, MMDB_return_s * result,
                    va_list params);

LOCAL MMDB_decode_all_s *dump(MMDB_s * mmdb, MMDB_decode_all_s * decode_all,
                              int indent);

#if !defined HAVE_MEMMEM
LOCAL void *memmem(const void *big, size_t big_len, const void *little,
                   size_t little_len)
{
    if (little_len) {
        int first_char = ((uint8_t *) little)[0];
        const void *ptr = big;
        size_t len = big_len;
        while (len >= little_len
               && (ptr = memchr(ptr, first_char, len - little_len + 1))) {
            if (!memcmp(ptr, little, little_len))
                return (void *)ptr;
            len = big_len - (++ptr - big);
        }
    }
    return NULL;
}
#endif

int MMDB_lookupaddressX(const char *host, int ai_family, int ai_flags, void *ip)
{
    struct addrinfo hints = {.ai_family = ai_family,
        .ai_flags = ai_flags,
        .ai_socktype = SOCK_STREAM
    }, *aifirst;
    int gaierr = getaddrinfo(host, NULL, &hints, &aifirst);

    if (gaierr == 0) {
        if (ai_family == AF_INET) {
            memcpy(&((struct in_addr *)ip)->s_addr,
                   &((struct sockaddr_in *)aifirst->ai_addr)->sin_addr, 4);
        } else if (ai_family == AF_INET6) {
            memcpy(&((struct in6_addr *)ip)->s6_addr,
                   ((struct sockaddr_in6 *)aifirst->ai_addr)->sin6_addr.s6_addr,
                   16);

        } else {
            /* should never happen */
            assert(0);
        }
        freeaddrinfo(aifirst);
    }
    return gaierr;
}

LOCAL float get_ieee754_float(const uint8_t * restrict p)
{
    volatile float f;
    uint8_t *q = (void *)&f;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    q[3] = p[0];
    q[2] = p[1];
    q[1] = p[2];
    q[0] = p[3];
#else
    memcpy(q, p, 4);
#endif
    return f;
}

LOCAL double get_ieee754_double(const uint8_t * restrict p)
{
    volatile double d;
    uint8_t *q = (void *)&d;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    q[7] = p[0];
    q[6] = p[1];
    q[5] = p[2];
    q[4] = p[3];
    q[3] = p[4];
    q[2] = p[5];
    q[1] = p[6];
    q[0] = p[7];
#else
    memcpy(q, p, 8);
#endif

    return d;
}

LOCAL uint32_t get_uint32(const uint8_t * p)
{
    return (p[0] * 16777216U + p[1] * 65536 + p[2] * 256 + p[3]);
}

LOCAL uint32_t get_uint24(const uint8_t * p)
{
    return (p[0] * 65536U + p[1] * 256 + p[2]);
}

LOCAL uint32_t get_uint16(const uint8_t * p)
{
    return (p[0] * 256U + p[1]);
}

LOCAL uint32_t get_uintX(const uint8_t * p, int length)
{
    uint32_t r = 0;
    while (length-- > 0) {
        r <<= 8;
        r += *p++;
    }
    return r;
}

LOCAL int get_sintX(const uint8_t * p, int length)
{
    return (int)get_uintX(p, length);
}

LOCAL uint32_t get_ptr_from(uint8_t ctrl, uint8_t const *const ptr,
                            int ptr_size)
{
    uint32_t new_offset;
    switch (ptr_size) {
    case 0:
        new_offset = (ctrl & 7) * 256 + ptr[0];
        break;
    case 1:
#if BROKEN_PTR
        new_offset = (ctrl & 7) * 65536 + ptr[0] * 256 + ptr[1];
#else
        new_offset = 2048 + (ctrl & 7) * 65536 + ptr[0] * 256 + ptr[1];
#endif
        break;
    case 2:
#if BROKEN_PTR
        new_offset = (ctrl & 7) * 16777216 + get_uint24(ptr);
#else
        new_offset = 2048 + 524288 + (ctrl & 7) * 16777216 + get_uint24(ptr);
#endif
        break;
    case 3:
    default:
        new_offset = get_uint32(ptr);
        break;
    }
    return MMDB_DATASECTION_NOOP_SIZE + new_offset;
}

LOCAL char *bytesdup(MMDB_return_s const *const ret)
{
    char *mem = NULL;
    if (ret->offset) {
        mem = malloc(ret->data_size + 1);
        memcpy(mem, ret->ptr, ret->data_size);
        mem[ret->data_size] = '\0';
    }
    return mem;
}

// 0 match like strcmp
int MMDB_strcmp_result(MMDB_s * mmdb, MMDB_return_s const *const result,
                       char *str)
{
    if (result->offset > 0) {
        const char *p = result->ptr;
        for (int i = 0; i < result->data_size; i++) {
            if (p[i] != str[i])
                return 1;
        }
        return 0;
    }
    return 1;
}

LOCAL int get_ext_type(int raw_ext_type)
{
#if defined BROKEN_TYPE
    return 7 + raw_ext_type;
#else
    return 8 + raw_ext_type;
#endif
}

#define FD_RET_ON_ERR(fn) do{ \
  int err = (fn);             \
  if ( err != MMDB_SUCCESS )  \
    return err;               \
  }while(0)

#define MMDB_CHKBIT_128(bit,ptr) ((ptr)[((127U - (bit)) >> 3)] & (1U << (~(127U - (bit)) & 7)))

LOCAL void free_all(MMDB_s * mmdb)
{
    if (mmdb) {
        if (mmdb->fname)
            free(mmdb->fname);
        if (mmdb->fd >= 0)
            close(mmdb->fd);
        if (mmdb->file_in_mem_ptr)
            munmap(mmdb->file_in_mem_ptr, mmdb->size);
        if (mmdb->fake_metadata_db) {
            free(mmdb->fake_metadata_db);
        }
        free((void *)mmdb);
    }
}

#define RETURN_ON_END_OF_SEARCHX(offset,segments,depth,maxdepth, res) \
            if ((offset) >= (segments)) {                     \
                (res)->netmask = (maxdepth) - (depth);        \
                (res)->entry.offset = (offset) - (segments);  \
                return MMDB_SUCCESS;                          \
            }

#define RETURN_ON_END_OF_SEARCH32(offset,segments,depth, res) \
            MMDB_DBG_CARP( "RETURN_ON_END_OF_SEARCH32 depth:%d offset:%u segments:%d\n", depth, (unsigned int)offset, segments); \
	    RETURN_ON_END_OF_SEARCHX(offset,segments,depth, 32, res)

#define RETURN_ON_END_OF_SEARCH128(offset,segments,depth, res) \
	    RETURN_ON_END_OF_SEARCHX(offset,segments,depth,128, res)

int MMDB_lookup_by_ipnum_128(struct in6_addr ipnum, MMDB_root_entry_s * result)
{
    MMDB_s *mmdb = result->entry.mmdb;

    int segments = mmdb->node_count;
    uint32_t offset = 0;
    int rl = mmdb->full_record_size_bytes;
    const uint8_t *mem = mmdb->file_in_mem_ptr;
    const uint8_t *p;
    int depth;
    if (rl == 6) {

        for (depth = mmdb->depth - 1; depth >= 0; depth--) {
            p = &mem[offset * rl];
            if (MMDB_CHKBIT_128(depth, (uint8_t *) & ipnum))
                p += 3;
            offset = get_uint24(p);
            RETURN_ON_END_OF_SEARCH128(offset, segments, depth, result);
        }
    } else if (rl == 7) {
        for (depth = mmdb->depth - 1; depth >= 0; depth--) {
            p = &mem[offset * rl];
            if (MMDB_CHKBIT_128(depth, (uint8_t *) & ipnum)) {
                p += 3;
                offset = get_uint32(p);
                offset &= 0xfffffff;
            } else {

                offset =
                    p[0] * 65536 + p[1] * 256 + p[2] + ((p[3] & 0xf0) << 20);
            }
            RETURN_ON_END_OF_SEARCH128(offset, segments, depth, result);
        }
    } else if (rl == 8) {
        for (depth = mmdb->depth - 1; depth >= 0; depth--) {
            p = &mem[offset * rl];
            if (MMDB_CHKBIT_128(depth, (uint8_t *) & ipnum))
                p += 4;
            offset = get_uint32(p);
            RETURN_ON_END_OF_SEARCH128(offset, segments, depth, result);
        }
    }
    //uhhh should never happen !
    return MMDB_CORRUPTDATABASE;
}

int MMDB_lookup_by_ipnum(uint32_t ipnum, MMDB_root_entry_s * res)
{
    MMDB_s *mmdb = res->entry.mmdb;

    MMDB_DBG_CARP("MMDB_lookup_by_ipnum{mmdb} fd:%d depth:%d node_count:%d\n",
                  mmdb->fd, mmdb->depth, mmdb->node_count);
    MMDB_DBG_CARP("MMDB_lookup_by_ipnum ip:%u fd:%d\n", ipnum, mmdb->fd);

    int segments = mmdb->node_count;
    uint32_t offset = 0;
    int rl = mmdb->full_record_size_bytes;
    const uint8_t *mem = mmdb->file_in_mem_ptr;
    const uint8_t *p;
    uint32_t mask = 0x80000000U;
    int depth;
    if (rl == 6) {
        for (depth = 32 - 1; depth >= 0; depth--, mask >>= 1) {
            p = &mem[offset * rl];
            if (ipnum & mask)
                p += 3;
            offset = get_uint24(p);
            RETURN_ON_END_OF_SEARCH32(offset, segments, depth, res);
        }
    } else if (rl == 7) {
        for (depth = 32 - 1; depth >= 0; depth--, mask >>= 1) {
            p = &mem[offset * rl];
            if (ipnum & mask) {
                p += 3;
                offset = get_uint32(p);
                offset &= 0xfffffff;
            } else {
                offset =
                    p[0] * 65536 + p[1] * 256 + p[2] + ((p[3] & 0xf0) << 20);
            }
            RETURN_ON_END_OF_SEARCH32(offset, segments, depth, res);
        }
    } else if (rl == 8) {
        for (depth = 32 - 1; depth >= 0; depth--, mask >>= 1) {
            p = &mem[offset * rl];
            if (ipnum & mask)
                p += 4;
            offset = get_uint32(p);
            RETURN_ON_END_OF_SEARCH32(offset, segments, depth, res);
        }
    }
    //uhhh should never happen !
    return MMDB_CORRUPTDATABASE;
}

LOCAL int init(MMDB_s * mmdb, const char *fname, uint32_t flags)
{
    struct stat s;
    int fd;
    uint8_t *ptr;
    ssize_t size;
    off_t offset;
    mmdb->fname = strdup(fname);
    if (mmdb->fname == NULL)
        return MMDB_OUTOFMEMORY;
    mmdb->fd = fd = open(fname, O_RDONLY);

    if (fd < 0)
        return MMDB_OPENFILEERROR;
    fstat(fd, &s);
    mmdb->flags = flags;
    mmdb->size = size = s.st_size;
    offset = 0;
    ptr = mmdb->meta_data_content =
        mmap(NULL, size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);

    if (ptr == NULL)
        return MMDB_INVALIDDATABASE;

    int max_metasize = size > 4096 ? 4096 : size;
    const uint8_t *metadata =
        memmem(ptr + size - max_metasize, max_metasize, "\xab\xcd\xefMaxMind.com", 14);
    if (metadata == NULL) {
        mmdb->meta_data_content = NULL;
        return MMDB_INVALIDDATABASE;
    }

    mmdb->fake_metadata_db = calloc(1, sizeof(struct MMDB_s));
    mmdb->fake_metadata_db->fd = -1;
    mmdb->fake_metadata_db->dataptr = metadata + 14;
    mmdb->meta.mmdb = mmdb->fake_metadata_db;

    // we can't fail with ioerror's here. It is a memory operation
    mmdb->major_file_format =
        get_uint_value(&mmdb->meta, KEYS("binary_format_major_version"));

    mmdb->minor_file_format =
        get_uint_value(&mmdb->meta, KEYS("binary_format_minor_version"));

    // looks like the dataabase_type is the info string.
    // mmdb->database_type = get_uint_value(&meta, KEYS("database_type"));
    mmdb->full_record_size_bytes =
        get_uint_value(&mmdb->meta, KEYS("record_size")) * 2 / 8U;
    mmdb->node_count = get_uint_value(&mmdb->meta, KEYS("node_count"));

    // unfortunately we must guess the depth of the database
    mmdb->depth =
        get_uint_value(&mmdb->meta, KEYS("ip_version")) == 4 ? 32 : 128;

    mmdb->file_in_mem_ptr = ptr;
    mmdb->dataptr =
        mmdb->file_in_mem_ptr + mmdb->node_count * mmdb->full_record_size_bytes;

    // Success - but can we handle the data?
    if (mmdb->major_file_format != 2) {
        return MMDB_UNKNOWNDATABASEFMT;
    }

    return MMDB_SUCCESS;
}

MMDB_s *MMDB_open(const char *fname, uint32_t flags)
{
    MMDB_DBG_CARP("MMDB_open %s %d\n", fname, flags);
    MMDB_s *mmdb = calloc(1, sizeof(MMDB_s));
    if (MMDB_SUCCESS != init(mmdb, fname, flags)) {
        free_all(mmdb);
        return NULL;
    }
    return mmdb;
}

void MMDB_close(MMDB_s * mmdb)
{
    if (mmdb) {
        free_all(mmdb);
    }
}

/* return the result of any uint type with 32 bit's or less as uint32 */
uint32_t MMDB_get_uint(MMDB_return_s const *const result)
{
    return result->uinteger;
}

int MMDB_get_value(MMDB_entry_s * start, MMDB_return_s * result, ...)
{
    va_list keys;
    va_start(keys, result);
    int ioerror = MMDB_vget_value(start, result, keys);
    va_end(keys);
    return ioerror;
}

LOCAL uint32_t get_uint_value(MMDB_entry_s * start, ...)
{
    MMDB_return_s result;
    va_list params;
    va_start(params, start);
    MMDB_vget_value(start, &result, params);
    va_end(params);
    return MMDB_get_uint(&result);
}

LOCAL void decode_one(MMDB_s * mmdb, uint32_t offset, MMDB_decode_s * decode)
{

    const uint8_t *mem = mmdb->dataptr;
    uint8_t ctrl;
    int type;
    decode->data.offset = offset;
    ctrl = mem[offset++];
    type = (ctrl >> 5) & 7;
    if (type == MMDB_DTYPE_EXT)
        type = get_ext_type(mem[offset++]);

    // MMDB_DBG_CARP("decode_one type:%d\n", type);

    decode->data.type = type;

    if (type == MMDB_DTYPE_PTR) {
        int psize = (ctrl >> 3) & 3;
        decode->data.uinteger = get_ptr_from(ctrl, &mem[offset], psize);
        decode->data.data_size = psize + 1;
        decode->offset_to_next = offset + psize + 1;
        MMDB_DBG_CARP
            ("decode_one{ptr} ctrl:%d, offset:%d psize:%d point_to:%d\n", ctrl,
             offset, psize, decode->data.uinteger);
        return;
    }

    int size = ctrl & 31;
    switch (size) {
    case 29:
        size = 29 + mem[offset++];
        break;
    case 30:
        size = 285 + get_uint16(&mem[offset]);
        offset += 2;
        break;
    case 31:
        size = 65821 + get_uint24(&mem[offset]);
        offset += 3;
    default:
        break;
    }

    if (type == MMDB_DTYPE_MAP || type == MMDB_DTYPE_ARRAY) {
        decode->data.data_size = size;
        decode->offset_to_next = offset;
        MMDB_DBG_CARP("decode_one type:%d size:%d\n", type, size);
        return;
    }

    if (type == MMDB_DTYPE_BOOLEAN) {
        decode->data.uinteger = !!size;
        decode->data.data_size = 0;
        decode->offset_to_next = offset;
        MMDB_DBG_CARP("decode_one type:%d size:%d\n", type, 0);
        return;
    }

    if (size == 0 && type != MMDB_DTYPE_UINT16 && type != MMDB_DTYPE_UINT32
        && type != MMDB_DTYPE_INT32) {
        decode->data.ptr = NULL;
        decode->data.data_size = 0;
        decode->offset_to_next = offset;
        return;
    }

    if ((type == MMDB_DTYPE_UINT32) || (type == MMDB_DTYPE_UINT16)) {
        decode->data.uinteger = get_uintX(&mem[offset], size);
    } else if (type == MMDB_DTYPE_INT32) {
        decode->data.sinteger = get_sintX(&mem[offset], size);
    } else if (type == MMDB_DTYPE_UINT64) {
        assert(size >= 0 && size <= 8);
        memset(decode->data.c8, 0, 8);
        if (size > 0)
            memcpy(decode->data.c8 + 8 - size, &mem[offset], size);
    } else if (type == MMDB_DTYPE_UINT128) {
        assert(size >= 0 && size <= 16);
        memset(decode->data.c16, 0, 16);
        if (size > 0)
            memcpy(decode->data.c16 + 16 - size, &mem[offset], size);
    } else if (type == MMDB_DTYPE_IEEE754_FLOAT) {
        size = 4;
        decode->data.float_value = get_ieee754_float(&mem[offset]);
    } else if (type == MMDB_DTYPE_IEEE754_DOUBLE) {
        size = 8;
        decode->data.double_value = get_ieee754_double(&mem[offset]);
    } else {
        decode->data.ptr = &mem[offset];
        decode->data.data_size = size;
    }
    decode->offset_to_next = offset + size;
    MMDB_DBG_CARP("decode_one type:%d size:%d\n", type, size);

    return;
}

LOCAL void skip_hash_array(MMDB_s * mmdb, MMDB_decode_s * decode)
{
    if (decode->data.type == MMDB_DTYPE_MAP) {
        int size = decode->data.data_size;
        while (size-- > 0) {
            decode_one(mmdb, decode->offset_to_next, decode);   // key
            decode_one(mmdb, decode->offset_to_next, decode);   // value
            skip_hash_array(mmdb, decode);
        }

    } else if (decode->data.type == MMDB_DTYPE_ARRAY) {
        int size = decode->data.data_size;
        while (size-- > 0) {
            decode_one(mmdb, decode->offset_to_next, decode);   // value
            skip_hash_array(mmdb, decode);
        }
    }
}

LOCAL void DPRINT_KEY(MMDB_s * mmdb, MMDB_return_s * data)
{
    uint8_t str[256];
    int len = data->data_size > 255 ? 255 : data->data_size;

    memcpy(str, data->ptr, len);
    str[len] = '\0';
    fprintf(stderr, "%s\n", str);
}

const char *MMDB_lib_version(void)
{
    return PACKAGE_VERSION;
}

int MMDB_get_tree(MMDB_entry_s * start, MMDB_decode_all_s ** decode_all)
{
    MMDB_decode_all_s *decode = *decode_all = MMDB_alloc_decode_all();
    return get_tree(start->mmdb, start->offset, decode);
}

LOCAL int get_tree(MMDB_s * mmdb, uint32_t offset, MMDB_decode_all_s * decode)
{
    decode_one(mmdb, offset, &decode->decode);

    if (decode->decode.data.type == MMDB_DTYPE_PTR) {
        // skip pointer silently
        MMDB_DBG_CARP("Skip ptr\n");
        uint32_t tmp = decode->decode.offset_to_next;
        uint32_t last_offset;
        while (decode->decode.data.type == MMDB_DTYPE_PTR) {
            decode_one(mmdb, last_offset =
                       decode->decode.data.uinteger, &decode->decode);
        }

        if (decode->decode.data.type == MMDB_DTYPE_ARRAY
            || decode->decode.data.type == MMDB_DTYPE_MAP) {
            get_tree(mmdb, last_offset, decode);
        }
        decode->decode.offset_to_next = tmp;
        return MMDB_SUCCESS;

    }

    switch (decode->decode.data.type) {

    case MMDB_DTYPE_ARRAY:
        {
            int array_size = decode->decode.data.data_size;
            MMDB_DBG_CARP("Decode array with %d entries\n", array_size);
            uint32_t array_offset = decode->decode.offset_to_next;
            MMDB_decode_all_s *previous = decode;
            // decode->indent = 1;
            while (array_size-- > 0) {
                MMDB_decode_all_s *decode_to = previous->next =
                    MMDB_alloc_decode_all();
                get_tree(mmdb, array_offset, decode_to);
                array_offset = decode_to->decode.offset_to_next;
                while (previous->next)
                    previous = previous->next;

            }
            decode->decode.offset_to_next = array_offset;

        }
        break;
    case MMDB_DTYPE_MAP:
        {
            int size = decode->decode.data.data_size;

#if MMDB_DEBUG
            int rnd = rand();
            MMDB_DBG_CARP("%u decode hash with %d keys\n", rnd, size);
#endif
            offset = decode->decode.offset_to_next;
            MMDB_decode_all_s *previous = decode;
            while (size-- > 0) {
                MMDB_decode_all_s *decode_to = previous->next =
                    MMDB_alloc_decode_all();
                get_tree(mmdb, offset, decode_to);
                while (previous->next)
                    previous = previous->next;

#if MMDB_DEBUG
                MMDB_DBG_CARP("key num: %d (%u)", size, rnd);
                DPRINT_KEY(mmdb, &decode_to->decode.data);
#endif

                offset = decode_to->decode.offset_to_next;
                decode_to = previous->next = MMDB_alloc_decode_all();
                get_tree(mmdb, offset, decode_to);
                while (previous->next)
                    previous = previous->next;
                offset = decode_to->decode.offset_to_next;
            }
            decode->decode.offset_to_next = offset;
        }
        break;
    default:
        break;
    }
    return MMDB_SUCCESS;
}

int MMDB_dump(MMDB_s * mmdb, MMDB_decode_all_s * decode_all, int indent)
{
    while (decode_all) {
        decode_all = dump(mmdb, decode_all, indent);
    }
    // not sure about the return type right now
    return MMDB_SUCCESS;
}

LOCAL void silly_pindent(int i)
{
    char buffer[1024];
    int size = i >= 1024 ? 1023 : i;
    memset(buffer, 32, size);
    buffer[size] = '\0';
    fputs(buffer, stderr);
}

LOCAL MMDB_decode_all_s *dump(MMDB_s * mmdb, MMDB_decode_all_s * decode_all,
                              int indent)
{
    switch (decode_all->decode.data.type) {
    case MMDB_DTYPE_MAP:
        {
            int size = decode_all->decode.data.data_size;
            for (decode_all = decode_all->next; size && decode_all; size--) {
                decode_all = dump(mmdb, decode_all, indent + 2);
                decode_all = dump(mmdb, decode_all, indent + 2);
            }
        }
        break;
    case MMDB_DTYPE_ARRAY:
        {
            int size = decode_all->decode.data.data_size;
            for (decode_all = decode_all->next; size && decode_all; size--) {
                decode_all = dump(mmdb, decode_all, indent + 2);
            }
        }
        break;
    case MMDB_DTYPE_UTF8_STRING:
    case MMDB_DTYPE_BYTES:
        silly_pindent(indent);
        DPRINT_KEY(mmdb, &decode_all->decode.data);
        decode_all = decode_all->next;
        break;
    case MMDB_DTYPE_IEEE754_DOUBLE:
        silly_pindent(indent);
        fprintf(stdout, "%f\n", decode_all->decode.data.double_value);
        decode_all = decode_all->next;
        break;
    case MMDB_DTYPE_IEEE754_FLOAT:
        silly_pindent(indent);
        fprintf(stdout, "%f\n", decode_all->decode.data.float_value);
        decode_all = decode_all->next;
        break;
    case MMDB_DTYPE_UINT16:
    case MMDB_DTYPE_UINT32:
    case MMDB_DTYPE_BOOLEAN:
        silly_pindent(indent);
        fprintf(stdout, "%u\n", decode_all->decode.data.uinteger);
        decode_all = decode_all->next;
        break;
    case MMDB_DTYPE_UINT64:
    case MMDB_DTYPE_UINT128:
        silly_pindent(indent);
        fprintf(stdout, "Some UINT64 or UINT128 data\n");
        //fprintf(stderr, "%u\n", decode_all->decode.data.uinteger);
        decode_all = decode_all->next;
        break;
    case MMDB_DTYPE_INT32:
        silly_pindent(indent);
        fprintf(stdout, "%d\n", decode_all->decode.data.sinteger);
        decode_all = decode_all->next;
        break;
    default:
        MMDB_DBG_CARP("decode_one UNIPLEMENTED type:%d\n",
                      decode_all->decode.data.type);
        assert(0);
    }
    return decode_all;
}

MMDB_decode_all_s *MMDB_alloc_decode_all(void)
{
    return calloc(1, sizeof(MMDB_decode_all_s));
}

void MMDB_free_decode_all(MMDB_decode_all_s * freeme)
{
    if (freeme == NULL)
        return;
    if (freeme->next)
        MMDB_free_decode_all(freeme->next);
    free(freeme);
}

LOCAL void decode_one_follow(MMDB_s * mmdb, uint32_t offset,
                             MMDB_decode_s * decode)
{
    decode_one(mmdb, offset, decode);
    if (decode->data.type == MMDB_DTYPE_PTR)
        decode_one(mmdb, decode->data.uinteger, decode);
}

int MMDB_vget_value(MMDB_entry_s * start, MMDB_return_s * result,
                    va_list params)
{
    MMDB_decode_s decode, key, value;
    MMDB_s *mmdb = start->mmdb;
    uint32_t offset = start->offset;
    char *src_key;              // = va_arg(params, char *);
    int src_keylen;
    while ((src_key = va_arg(params, char *))) {
        MMDB_DBG_CARP("decode_one src_key:%s\n", src_key);
        decode_one(mmdb, offset, &decode);
 donotdecode:
        src_keylen = strlen(src_key);
        switch (decode.data.type) {
        case MMDB_DTYPE_PTR:
            // we follow the pointer
            decode_one(mmdb, decode.data.uinteger, &decode);
            break;

            // learn to skip this
        case MMDB_DTYPE_ARRAY:
            {
                int size = decode.data.data_size;
                int offset = strtol(src_key, NULL, 10);
                if (offset >= size || offset < 0) {
                    result->offset = 0; // not found.
                    goto end;
                }
                for (int i = 0; i < offset; i++) {
                    decode_one(mmdb, decode.offset_to_next, &decode);
                    skip_hash_array(mmdb, &decode);
                }
                if ((src_key = va_arg(params, char *))) {
                    decode_one_follow(mmdb, decode.offset_to_next, &decode);
                    offset = decode.offset_to_next;
                    goto donotdecode;
                }
                decode_one_follow(mmdb, decode.offset_to_next, &value);
                memcpy(result, &value.data, sizeof(MMDB_return_s));
                goto end;
            }
            break;
        case MMDB_DTYPE_MAP:
            {
                int size = decode.data.data_size;
                // printf("decode hash with %d keys\n", size);
                offset = decode.offset_to_next;
                while (size-- > 0) {
                    decode_one(mmdb, offset, &key);

                    uint32_t offset_to_value = key.offset_to_next;

                    if (key.data.type == MMDB_DTYPE_PTR) {
                        // while (key.data.type == MMDB_DTYPE_PTR) {
                        decode_one(mmdb, key.data.uinteger, &key);
                        // }
                    }

                    assert(key.data.type == MMDB_DTYPE_BYTES ||
                           key.data.type == MMDB_DTYPE_UTF8_STRING);

                    if (key.data.data_size == src_keylen &&
                        !memcmp(src_key, key.data.ptr, src_keylen)) {
                        if ((src_key = va_arg(params, char *))) {
                            // DPRINT_KEY(&key.data);
                            decode_one_follow(mmdb, offset_to_value, &decode);
                            offset = decode.offset_to_next;

                            goto donotdecode;
                        }
                        // found it!
                        decode_one_follow(mmdb, offset_to_value, &value);
                        memcpy(result, &value.data, sizeof(MMDB_return_s));
                        goto end;
                    } else {
                        // we search for another key skip  this
                        decode_one(mmdb, offset_to_value, &value);
                        skip_hash_array(mmdb, &value);
                        offset = value.offset_to_next;
                    }
                }
                // not found!! do something
                //DPRINT_KEY(&key.data);
                //
                result->offset = 0;     // not found.
                goto end;
            }
        default:
            break;
        }
    }
 end:
    va_end(params);
    return MMDB_SUCCESS;
}
