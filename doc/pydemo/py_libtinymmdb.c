/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

#include <Python.h>
#include <tinymmdb.h>
#include <netdb.h>

staticforward PyTypeObject TMMDB_MMDBType;
static PyObject *mkobj_r(TMMDB_s * mmdb, TMMDB_decode_all_s ** current);

/* Exception object for python */
static PyObject *PyMMDBError;

typedef struct {
    PyObject_HEAD               /* no semicolon */
    TMMDB_s * mmdb;
} TMMDB_MMDBObject;

// Create a new Python MMDB object
static PyObject *TMMDB_new_Py(PyObject * self, PyObject * args)
{
    TMMDB_MMDBObject *obj;
    char *filename;
    int flags;

    if (!PyArg_ParseTuple(args, "si", &filename, &flags)) {
        return NULL;
    }

    obj = PyObject_New(TMMDB_MMDBObject, &TMMDB_MMDBType);
    if (!obj)
        return NULL;

    int status = TMMDB_open(&obj->mmdb, filename, flags);
    if (status == TMMDB_SUCCESS && !obj->mmdb) {
        PyErr_SetString(PyMMDBError, "Can't create obj->mmdb object");
        Py_DECREF(obj);
        return NULL;
    }
    return (PyObject *) obj;
}

// Destroy the MMDB object
static void TMMDB_MMDB_dealloc(PyObject * self)
{
    TMMDB_MMDBObject *obj = (TMMDB_MMDBObject *) self;
    TMMDB_close(obj->mmdb);
    PyObject_Del(self);
}

// This function creates the Py object for us 
static PyObject *mkobj(TMMDB_s * mmdb, TMMDB_decode_all_s ** current)
{
    TMMDB_decode_all_s *tmp = *current;
    PyObject *py = mkobj_r(mmdb, current);
    *current = tmp;
    return py;
}

// Return the version of the CAPI
static PyObject *TMMDB_lib_version_Py(PyObject * self, PyObject * args)
{
    return Py_BuildValue("s", TMMDB_lib_version());
}

// This function is used to do the lookup
static PyObject *TMMDB_lookup_Py(PyObject * self, PyObject * args)
{
    char *name;
    struct in6_addr ip;
    int status;

    TMMDB_MMDBObject *obj = (TMMDB_MMDBObject *) self;
    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    status = TMMDB_resolve_address(name, AF_INET6, AI_V4MAPPED, &ip);
    if (status == 0) {
        TMMDB_root_entry_s root = {.entry.mmdb = obj->mmdb };
        status = TMMDB_lookup_by_ipnum_128(ip, &root);
        if (status == TMMDB_SUCCESS && root.entry.offset > 0) {
            TMMDB_decode_all_s *decode_all;
            if (TMMDB_get_tree(&root.entry, &decode_all) == TMMDB_SUCCESS) {
                PyObject *retval = mkobj(obj->mmdb, &decode_all);
                TMMDB_free_decode_all(decode_all);
                return retval;
            }
        }
    }
    Py_RETURN_NONE;
}

// minor helper fuction to create a python string from the database
static PyObject *build_PyString_FromStringAndSize(TMMDB_s * mmdb, void *ptr,
                                                  int size)
{
    return PyString_FromStringAndSize(ptr, size);
}

// minor helper fuction to create a python string from the database
static PyObject *build_PyUnicode_DecodeUTF8(TMMDB_s * mmdb, void *ptr, int size)
{
    return PyUnicode_DecodeUTF8(ptr, size, NULL);
}

// iterated over our datastructure and create python from it
static PyObject *mkobj_r(TMMDB_s * mmdb, TMMDB_decode_all_s ** current)
{
    PyObject *sv = NULL;
    switch ((*current)->decode.data.type) {
    case TMMDB_DTYPE_MAP:
        {
            PyObject *hv = PyDict_New();
            int size = (*current)->decode.data.data_size;
            for (*current = (*current)->next; size; size--) {
                PyObject *key, *val;
                int key_size = (*current)->decode.data.data_size;
                void *key_ptr = size ? (void *)(*current)->decode.data.ptr : "";
                *current = (*current)->next;
                val = mkobj_r(mmdb, current);
                key = build_PyString_FromStringAndSize(mmdb, key_ptr, key_size);
                PyDict_SetItem(hv, key, val);
                Py_DECREF(val);
                Py_DECREF(key);
            }
            return hv;
        }
        break;
    case TMMDB_DTYPE_ARRAY:
        {
            int size = (*current)->decode.data.data_size;
            PyObject *av = PyList_New(0);
            for (*current = (*current)->next; size; size--) {
                PyObject *val = mkobj_r(mmdb, current);
                PyList_Append(av, val);
                Py_DECREF(val);
            }
            return av;
        }
        break;
    case TMMDB_DTYPE_UTF8_STRING:
        {
            int size = (*current)->decode.data.data_size;
            void *ptr = size ? (void *)(*current)->decode.data.ptr : "";
            sv = build_PyUnicode_DecodeUTF8(mmdb, ptr, size);
        }
        break;
    case TMMDB_DTYPE_BYTES:
        {
            int size = (*current)->decode.data.data_size;
            sv = build_PyString_FromStringAndSize(mmdb,
                                                  (void *)(*current)->decode.
                                                  data.ptr, size);
        }
        break;
    case TMMDB_DTYPE_IEEE754_FLOAT:
        sv = Py_BuildValue("d", (*current)->decode.data.float_value);
        break;
    case TMMDB_DTYPE_IEEE754_DOUBLE:
        sv = Py_BuildValue("d", (*current)->decode.data.double_value);
        break;
    case TMMDB_DTYPE_UINT32:
        sv = Py_BuildValue("I", (*current)->decode.data.uinteger);
        break;
    case TMMDB_DTYPE_UINT64:
        sv = build_PyString_FromStringAndSize(mmdb,
                                              (void *)(*current)->decode.data.
                                              c8, 8);
        break;
    case TMMDB_DTYPE_UINT128:
        sv = build_PyString_FromStringAndSize(mmdb,
                                              (void *)(*current)->decode.data.
                                              c16, 16);
        break;
    case TMMDB_DTYPE_BOOLEAN:
    case TMMDB_DTYPE_UINT16:
    case TMMDB_DTYPE_INT32:
        sv = PyInt_FromLong((*current)->decode.data.sinteger);
        break;
    default:
        assert(0);
    }

    if (*current)
        *current = (*current)->next;

    return sv;
}

static PyMethodDef TMMDB_Object_methods[] = {
    {"lookup", TMMDB_lookup_Py, 1, "Lookup entry by ipaddr"},
    {NULL, NULL, 0, NULL}
};

static PyObject *TMMDB_GetAttr(PyObject * self, char *attrname)
{
    return Py_FindMethod(TMMDB_Object_methods, self, attrname);
}

static PyTypeObject TMMDB_MMDBType = {
    PyObject_HEAD_INIT(NULL)
        0,
    "MMDB",
    sizeof(TMMDB_MMDBObject),
    0,
    TMMDB_MMDB_dealloc,          /*tp_dealloc */
    0,                          /*tp_print */
    (getattrfunc) TMMDB_GetAttr, /*tp_getattr */
    0,                          /*tp_setattr */
    0,                          /*tp_compare */
    0,                          /*tp_repr */
    0,                          /*tp_as_number */
    0,                          /*tp_as_sequence */
    0,                          /*tp_as_mapping */
    0,                          /*tp_hash */
};

static PyMethodDef TMMDB_Class_methods[] = {
    {"new", TMMDB_new_Py, 1,
     "TMMDB Constructor with database filename argument"},
    {"lib_version", TMMDB_lib_version_Py, 1, "Returns the CAPI version"},
    {NULL, NULL, 0, NULL}
};

DL_EXPORT(void) initTMMDB(void)
{
    PyObject *m, *d, *tmp;
    TMMDB_MMDBType.ob_type = &PyType_Type;

    m = Py_InitModule("TMMDB", TMMDB_Class_methods);
    d = PyModule_GetDict(m);

    PyMMDBError = PyErr_NewException("py_tmmdb.error", NULL, NULL);
    PyDict_SetItemString(d, "error", PyMMDBError);

    tmp = PyInt_FromLong(TMMDB_MODE_STANDARD);
    PyDict_SetItemString(d, "TMMDB_MODE_STANDARD", tmp);
    Py_DECREF(tmp);

    tmp = PyInt_FromLong(TMMDB_MODE_MEMORY_CACHE);
    PyDict_SetItemString(d, "TMMDB_MODE_MEMORY_CACHE", tmp);
    Py_DECREF(tmp);
}
