#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "json5.h"

/*
 * Python methods:
 *   loads(str) -> Python object
 *   dumps(obj, indent=0) -> str
 */
static PyObject* py_loads(PyObject* self, PyObject* args, PyObject* kwargs) {
    static char *kwlist[] = {"data", NULL};
    const char* input = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &input)) {
        return NULL;
    }
    PyObject* result = parse_json5(input);
    return result;  // parse_json5 sets exceptions on failure
}

static PyObject* py_dumps(PyObject* self, PyObject* args, PyObject* kwargs) {
    static char *kwlist[] = {"obj", "indent", NULL};
    PyObject* obj = NULL;
    PyObject* indent_obj = Py_None;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist,
                                     &obj, &indent_obj)) {
        return NULL;
    }
    int indent_val = 0;
    if (indent_obj != Py_None && PyLong_Check(indent_obj)) {
        long tmp = PyLong_AsLong(indent_obj);
        if (tmp < 0) tmp = 0;
        indent_val = (int)tmp;
    }
    PyObject* text = dump_json5(obj, indent_val);
    return text;
}

static PyMethodDef py_json5_methods[] = {
    {"loads",  (PyCFunction)(void*)py_loads,  METH_VARARGS|METH_KEYWORDS,
     "Parse JSON5 string into Python object."},
    {"dumps",  (PyCFunction)(void*)py_dumps,  METH_VARARGS|METH_KEYWORDS,
     "Serialize Python object into JSON5 string."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef py_json5_module = {
    PyModuleDef_HEAD_INIT,
    "py_json5",
    "Python <-> JSON5 Bridge",
    -1,
    py_json5_methods
};

PyMODINIT_FUNC PyInit_py_json5(void) {
    return PyModule_Create(&py_json5_module);
}
