#ifndef QJSON5_JSON5_H
#define QJSON5_JSON5_H

#include <Python.h>  /* We rely on Python objects here. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * parse_json5:
 *   Takes a const char* JSON5 text.
 *   Returns a PyObject* (the parsed Python object),
 *   or NULL on error (with a Python exception set).
 */
PyObject* parse_json5(const char *input);

/*
 * dump_json5:
 *   Takes a PyObject*, plus an integer indent,
 *   returns a new PyUnicode* with JSON5 text or NULL on error.
 */
PyObject* dump_json5(PyObject *obj, int indent);

#ifdef __cplusplus
}
#endif

#endif
