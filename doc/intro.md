
## Public API ##

    TMMDB_open
    TMMDB_close
    TMMDB_lookupaddressX
    TMMDB_lookup_by_ipnum_128
    TMMDB_lookup_by_ipnum
    TMMDB_get_tree
    TMMDB_free_decode_all

## Getting started ##

`tinymmdb.h` contains all public API functions and definitions. Start with
`#include <tinymmdb.h>`.

It is recommended to browse the example CAPI for python in the pydemo directory.
I would start with `TMMDB_lookup_Py` in `py_libtinymmdb.c` and go from there,

Use `python setup.py install` to install the code and ` python test.py` to run it.

## Function reference ##

### `TMMDB_s *TMMDB_open(char *fname, uint32_t flags)` ###

Open takes two arguments, the database filename typically xyz,mmdb and the operation mode.
We support curently only two modes, the diskbased `TMMDB_MODE_STANDARD` and the in memory mode `TMMDB_MODE_MEMORY_CACHE`.
`TMMDB_MODE_STANDARD` is slower, but does not need much memory.

The structure `TMMDB_s` contains all information to search the database file. Please consider all fields readonly.

### `void TMMDB_close(TMMDB_s * mmdb)` ###

Free's all memory associated with the database and the filehandle.

### `int TMMDB_lookupaddressX(const char *addr, int ai_family, int ai_flags, void *ip)` ###

Resolves the IP address addr into ip. ip is a pointer to `in_addr` or `in6_addr` depend on your inputs.

For IPv4 it is probpaly 

    status = TMMDB_lookupaddressX( addr, AF_INET, 0, ip );

If your database is IPv6 you should use

    status = TMMDB_lookupaddressX( addr, AF_INET6, AI_V4MAPPED, ip6 );

So any numeric input works like 24.24.24.24, ::24.24.24.24, ::ffff:24.24.24.24 or 2001:4860:b002::68

The return value is gaierr ( man getaddrinfo ) or 0 on success.

### `int TMMDB_lookup_by_ipnum_128(struct in6_addr ipnum, TMMDB_root_entry_s * result)` ###

The `TMMDB_lookup_by_ipnum_128` checks if the ipnumber usually created with a call to `TMMDB_lookupaddressX` is part of the database.

    // initialize the root entry structure with the database
    TMMDB_root_entry_s root {.entry.mmdb = mmdb };
    status = TMMDB_lookup_by_ipnum_128(ip.v6, &root);
    if ( status == TMMDB_SUCCESS ) {
        if (root.entry.offset ){
           // found something in the database
        }
    }

The example initilsize our search structure with the database. Reuse the stucture if you want.
The next line search the database for the ipnum. The result is always `TMMDB_SUCCESS` unless somthing wired happened. Like out of memory or an io error.
Note, that even a failed search return `TMMDB_SUCCESS`.

`entry.offset > 0` indicates, that we found something.

### `int TMMDB_get_tree(entry_s * entry, TMMDB_decode_all_s ** dec)` ###

`TMMDB_get_tree` preparse the database content into smaller easy peaces.
Iterate over the `TMMDB_decode_all_s` structure after this call.
**Please notice**, you have to free the `TMMDB_decode_all_s` structure 
with `TMMDB_free_decode_all` for every successful call to `TMMDB_get_tree`.

The calling sequene looks like

    TMMDB_decode_all_s *decode_all;
    status = TMMDB_get_tree(&root.entry, &decode_all);
    if ( status == TMMDB_SUCCESS ){
       // do something
       TMMDB_free_decode_all(decode_all);       
    }

 The `TMMDB_decode_all_s` structure contains _one_ decoded entry, and an internal offset
 to the next entry to decode. This offset is only useful for the decode functions.

 The `TMMDB_return_s` structure inside `TMMDB_decode_s` is much more interesting, 
 it contains the almost decoded data for all data types returning something.

    TMMDB_DTYPE_UTF8_STRING
    TMMDB_DTYPE_IEEE754_DOUBLE
    TMMDB_DTYPE_BYTES
    TMMDB_DTYPE_UINT16
    TMMDB_DTYPE_UINT32
    TMMDB_DTYPE_MAP
    TMMDB_DTYPE_INT32
    TMMDB_DTYPE_UINT64
    TMMDB_DTYPE_UINT128
    TMMDB_DTYPE_ARRAY
    TMMDB_DTYPE_BOOLEAN
    TMMDB_DTYPE_IEEE754_FLOAT

`TMMDB_return_s` looks like:

    struct TMMDB_return_s {
        /* return values */
        union {
            float float_value;
            double double_value;
            int sinteger;
            uint32_t uinteger;
            uint8_t c8[8];
            uint8_t c16[16];
	    const void* ptr;
        };
        uint32_t offset;
        int data_size;
        int type;
    };

The sturcture is valid whenever `TMMDB_return_s.offset > 0`.
`TMMDB_return_s.type` contains the type from above for example `TMMDB_DTYPE_INT32`.
and the value of the field. Sometimes the size, if it makes sense ( for `TMMDB_DTYPE_BYTES`,`TMMDB_DTYPE_UTF8_STRING`, `TMMDB_DTYPE_ARRAY` and `TMMDB_DTYPE_MAP` ).


#### `TMMDB_DTYPE_UTF8_STRING`

    type: TMMDB_DTYPE_UTF8_STRING
    data_size: is the length of the string in bytes
    ptr: is a pointer to the string in memory, or for diskbased databases an offset into the datasection of the file.

#### `TMMDB_DTYPE_IEEE754_DOUBLE`

    type: TMMDB_DTYPE_IEEE754_DOUBLE
    double_value: contains the value

#### `TMMDB_DTYPE_BYTES`
    
    type: TMMDB_DTYPE_BYTES
    data_size:  Length in bytes
    ptr: is a pointer to the string in memory, or for diskbased databases an offset into the datasection of the file.

#### `TMMDB_DTYPE_UINT16`

    type: TMMDB_DTYPE_UINT16
    sinteger: contains the value

#### `TMMDB_DTYPE_UINT32`

    type: TMMDB_DTYPE_UINT32
    uinteger: contains the value

#### `TMMDB_DTYPE_INT32`

    type: TMMDB_DTYPE_INT32
    sinteger: contains the value

#### `TMMDB_DTYPE_INT64`

    type: TMMDB_DTYPE_UINT64
    c8: contains eight bytes in network order

#### `TMMDB_DTYPE_INT128`

    type: TMMDB_DTYPE_UINT128
    c16: contains 16 bytes in network order

#### `TMMDB_DTYPE_BOOLEAN`
    type: TMMDB_DTYPE_BOOLEAN
    sinteger: contains the value
    
#### `TMMDB_DTYPE_MAP`
    type: TMMDB_DTYPE_MAP
    data_size:  count key/value pairs in the dict

#### `TMMDB_DTYPE_ARRAY`
    type: TMMDB_DTYPE_ARRAY
    data_size:  Length of the array

#### `TMMDB_DTYPE_IEEE754_FLOAT`
    type: TMMDB_DTYPE_IEEE754_FLOAT
    double_value: contains the value


### `void TMMDB_free_decode_all(TMMDB_decode_all_s * dec)` ###

Free all temporary used memory by `TMMDB_decode_all_s` typical used after `TMMDB_get_tree`

### `int TMMDB_pread(int fd, uint8_t * buffer, ssize_t to_read, off_t offset)` ###

TMMDB_pread makes it easier for the user to read X bytes into a buffer,

The return value is `TMMDB_SUCCESS` for SUCCESS or `TMMDB_IOERROR` on failure.


