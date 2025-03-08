#include "json5.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if defined(_MSC_VER)
#define INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define INLINE __attribute__((always_inline)) inline
#else
#define INLINE inline
#endif

#define RAISE(msg) do { PyErr_SetString(PyExc_ValueError, (msg)); return NULL; } while(0)
#define IS_JS_IDENT_START(c) ((c) == '_' || (c) == '$' || isalpha((unsigned char)(c)))
#define IS_JS_IDENT_PART(c)  ((c) == '_' || (c) == '$' || isalnum((unsigned char)(c)))

static INLINE void skip_whitespace(const char **p);
static INLINE PyObject* parse_value(const char **p);
static PyObject* parse_object(const char **p);
static PyObject* parse_array(const char **p);
static PyObject* parse_string(const char **p);
static INLINE PyObject* parse_number(const char **p);
static INLINE PyObject* parse_true(const char **p);
static INLINE PyObject* parse_false(const char **p);
static INLINE PyObject* parse_null(const char **p);

/*
 * skip_whitespace:
 * Advances the pointer over whitespace comments.
 */
static INLINE void skip_whitespace(const char **ref) {
    const char *p = *ref;
    while (1) {
        while ((unsigned char)*p <= ' ' && *p != '\0') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (*p && *p != '\n') {
                p++;
            }
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p) {
                if (p[0] == '*' && p[1] == '/') {
                    p += 2;
                    break;
                }
                p++;
            }
            continue;
        }
        break;
    }
    *ref = p;
}

/*
 * parse_value:
 * Detects the next token and branches accordingly.
 */
static INLINE PyObject* parse_value(const char **ref) {
    skip_whitespace(ref);
    const char *p = *ref;
    char c = *p;

    switch (c) {
        case '{':
            (*ref)++;
            return parse_object(ref);
        case '[':
            (*ref)++;
            return parse_array(ref);
        case '"':
        case '\'':
            return parse_string(ref);
        case 't':
            if (p[1] == 'r' && p[2] == 'u' && p[3] == 'e') {
                return parse_true(ref);
            }
            break;
        case 'f':
            if (p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e') {
                return parse_false(ref);
            }
            break;
        case 'n':
            if (p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
                return parse_null(ref);
            }
            break;
        default:
            if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) {
                return parse_number(ref);
            }
            break;
    }
    RAISE("Unexpected token");
}

/*
 * parse_object:
 * Creates a Python dict from JSON5 object syntax.
 */
static PyObject* parse_object(const char **ref) {
    PyObject *d = PyDict_New();
    if (!d) {
        return NULL;
    }
    skip_whitespace(ref);

    if (**ref == '}') {
        (*ref)++;
        return d;
    }

    while (**ref) {
        skip_whitespace(ref);
        PyObject *k = NULL;
        char c = **ref;

        if (c == '"' || c == '\'') {
            k = parse_string(ref);
            if (!k) {
                Py_DECREF(d);
                return NULL;
            }
        } else {
            const char *start = *ref;
            while (**ref && **ref != ':' && (unsigned char)**ref > ' '
                   && **ref != ',' && **ref != '}' && **ref != '/') {
                (*ref)++;
            }
            size_t length = (size_t)(*ref - start);
            if (length == 0) {
                Py_DECREF(d);
                RAISE("Invalid key");
            }
            if (!IS_JS_IDENT_START((unsigned char)start[0])) {
                Py_DECREF(d);
                RAISE("Invalid unquoted key start");
            }
            for (size_t i = 1; i < length; i++) {
                if (!IS_JS_IDENT_PART((unsigned char)start[i])) {
                    Py_DECREF(d);
                    RAISE("Invalid unquoted key char");
                }
            }
            k = PyUnicode_FromStringAndSize(start, (Py_ssize_t)length);
            if (!k) {
                Py_DECREF(d);
                return NULL;
            }
        }

        skip_whitespace(ref);
        if (**ref != ':') {
            Py_DECREF(k);
            Py_DECREF(d);
            RAISE("Missing colon");
        }
        (*ref)++;

        skip_whitespace(ref);
        PyObject *v = parse_value(ref);
        if (!v) {
            Py_DECREF(k);
            Py_DECREF(d);
            return NULL;
        }
        if (PyDict_SetItem(d, k, v) < 0) {
            Py_DECREF(k);
            Py_DECREF(v);
            Py_DECREF(d);
            return NULL;
        }
        Py_DECREF(k);
        Py_DECREF(v);

        skip_whitespace(ref);
        if (**ref == '}') {
            (*ref)++;
            return d;
        } else if (**ref == ',') {
            (*ref)++;
            skip_whitespace(ref);
            if (**ref == '}') {
                (*ref)++;
                return d;
            }
        } else {
            Py_DECREF(d);
            RAISE("Expected '}' or ','");
        }
    }
    Py_DECREF(d);
    RAISE("Unterminated object");
}

/*
 * arrCount:
 * Quick pass to count array items for pre-allocating the PyList.
 */
static INLINE int arrCount(const char **ref) {
    const char *p = *ref;
    int depth = 1;
    int cnt = 0;
    int inItem = 0;

    while (1) {
        while ((unsigned char)*p <= ' ' && *p != '\0') { p++; }
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (*p && *p != '\n') { p++; }
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p) {
                if (p[0] == '*' && p[1] == '/') { p += 2; break; }
                p++;
            }
            continue;
        }

        char c = *p;
        if (!c) break;

        if (c == '[') {
            depth++;
            p++;
            continue;
        } else if (c == ']') {
            depth--;
            if (depth <= 0) {
                if (inItem) { cnt++; }
                break;
            }
            p++;
            continue;
        } else if (c == '{') {
            int obj_depth = 1;
            p++;
            while (*p && obj_depth > 0) {
                if (*p == '"' || *p == '\'') {
                    char quote = *p;
                    p++;
                    while (*p && *p != quote) {
                        if (*p == '\\' && p[1]) { p += 2; continue; }
                        p++;
                    }
                    if (*p) p++;
                    continue;
                } else if (*p == '{') {
                    obj_depth++;
                } else if (*p == '}') {
                    obj_depth--;
                }
                p++;
            }
            inItem = 1;
            continue;
        } else if (c == '"' || c == '\'') {
            char quote = c;
            p++;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1]) { p += 2; continue; }
                p++;
            }
            if (*p) p++;
            inItem = 1;
            continue;
        } else if (c == ',') {
            if (depth == 1 && inItem) {
                cnt++;
                inItem = 0;
            }
            p++;
            continue;
        } else {
            inItem = 1;
            p++;
            continue;
        }
    }
    return cnt;
}

/*
 * parse_array:
 * Creates a PyList from JSON5 array syntax.
 */
static PyObject* parse_array(const char **ref) {
    while ((unsigned char)**ref <= ' ' && **ref != '\0') {
        (*ref)++;
    }
    if (**ref == ']') {
        (*ref)++;
        return PyList_New(0);
    }
    const char *saved = *ref;
    int guess = arrCount(&saved);
    if (guess < 1) {
        guess = 1;
    }
    PyObject *lst = PyList_New(guess);
    if (!lst) {
        return NULL;
    }

    int idx = 0;
    while (**ref) {
        skip_whitespace(ref);
        if (**ref == ']') {
            (*ref)++;
            while (idx < guess) {
                PyList_SetItem(lst, idx, Py_NewRef(Py_None));
                idx++;
            }
            return lst;
        }
        PyObject *val = parse_value(ref);
        if (!val) {
            Py_DECREF(lst);
            return NULL;
        }
        if (idx < guess) {
            PyList_SET_ITEM(lst, idx, val);
        } else {
            int r = PyList_Append(lst, val);
            Py_DECREF(val);
            if (r < 0) {
                Py_DECREF(lst);
                return NULL;
            }
        }
        idx++;

        skip_whitespace(ref);
        if (**ref == ']') {
            (*ref)++;
            while (idx < guess) {
                PyList_SetItem(lst, idx, Py_NewRef(Py_None));
                idx++;
            }
            return lst;
        } else if (**ref == ',') {
            (*ref)++;
            skip_whitespace(ref);
            if (**ref == ']') {
                (*ref)++;
                while (idx < guess) {
                    PyList_SetItem(lst, idx, Py_NewRef(Py_None));
                    idx++;
                }
                return lst;
            }
        } else {
            Py_DECREF(lst);
            RAISE("Expected ']' or ','");
        }
    }
    Py_DECREF(lst);
    RAISE("Unterminated array");
}

/*
 * parse_string:
 * Consumes a quoted string (single or double).
 */
static PyObject* parse_string(const char **ref) {
    char quote_char = **ref;
    (*ref)++;

    size_t capacity = 64;
    size_t length = 0;
    char *buf = (char*)malloc(capacity);
    if (!buf) {
        RAISE("Out of memory");
    }

    while (**ref) {
        char c = **ref;
        if (c == quote_char) {
            (*ref)++;
            buf[length] = '\0';
            PyObject *res = PyUnicode_FromStringAndSize(buf, (Py_ssize_t)length);
            free(buf);
            return res;
        }
        if (c == '\\') {
            (*ref)++;
            c = **ref;
            switch (c) {
                case 'n':  buf[length++] = '\n'; break;
                case 't':  buf[length++] = '\t'; break;
                case 'r':  buf[length++] = '\r'; break;
                case 'b':  buf[length++] = '\b'; break;
                case 'f':  buf[length++] = '\f'; break;
                case '\\': buf[length++] = '\\'; break;
                case '"':  buf[length++] = '"';  break;
                case '\'': buf[length++] = '\''; break;
                default:   buf[length++] = c;    break;
            }
            (*ref)++;
        } else {
            buf[length++] = c;
            (*ref)++;
        }
        if (length + 1 >= capacity) {
            capacity <<= 1;
            char *tmp = (char*)realloc(buf, capacity);
            if (!tmp) {
                free(buf);
                RAISE("Out of memory");
            }
            buf = tmp;
        }
    }
    free(buf);
    RAISE("Unterminated string");
}

/*
 * parse_number:
 * Tries to parse an integer or floating value from the input.
 */
static INLINE PyObject* parse_number(const char **ref) {
    const char *start = *ref;
    const char *endp;
    extern PyObject* fastNum(const char*, const char**);
    PyObject *nm = fastNum(start, &endp);
    if (!nm) {
        RAISE("Invalid number");
    }
    *ref = endp;
    return nm;
}

/*
 * parse_true, parse_false, parse_null
 */
static INLINE PyObject* parse_true(const char **ref) {
    (*ref) += 4;
    Py_RETURN_TRUE;
}

static INLINE PyObject* parse_false(const char **ref) {
    (*ref) += 5;
    Py_RETURN_FALSE;
}

static INLINE PyObject* parse_null(const char **ref) {
    (*ref) += 4;
    Py_RETURN_NONE;
}

/*
 * fastNum:
 * Helper for parse_number; returns PyLong or PyFloat.
 */
PyObject* fastNum(const char *start, const char **end) {
    const char *p = start;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while ((unsigned char)*p <= ' ' && *p != '\0') {
        p++;
    }

    long long iPart = 0;
    int hasDig = 0;
    while (*p >= '0' && *p <= '9') {
        hasDig = 1;
        iPart = (iPart * 10) + (*p - '0');
        p++;
    }

    int isFloat = 0;
    double frac = 0.0;
    double factor = 0.1;

    if (*p == '.') {
        isFloat = 1;
        p++;
        while (*p >= '0' && *p <= '9') {
            hasDig = 1;
            frac += (*p - '0') * factor;
            factor *= 0.1;
            p++;
        }
    }

    int eSign = 1, eMode = 0;
    long eVal = 0;
    if (*p == 'e' || *p == 'E') {
        isFloat = 1;
        p++;
        if (*p == '-') {
            eSign = -1;
            p++;
        } else if (*p == '+') {
            p++;
        }
        eMode = 1;
        while (*p >= '0' && *p <= '9') {
            eVal = eVal * 10 + (*p - '0');
            p++;
        }
    }
    *end = p;

    if (!hasDig) {
        return NULL;
    }
    if (!isFloat) {
        if (sign < 0) {
            iPart = -iPart;
        }
        if (iPart >= -9007199254740992LL && iPart <= 9007199254740992LL) {
            return PyLong_FromLongLong(iPart);
        } else {
            double dbl = (double)iPart;
            return PyFloat_FromDouble(dbl);
        }
    } else {
        double dbl = (double)iPart + frac;
        if (sign < 0) {
            dbl = -dbl;
        }
        if (eMode && eVal != 0) {
            double factorE = 1.0;
            long tmpE = eVal;
            while (tmpE-- > 0) {
                factorE *= 10.0;
            }
            if (eSign < 0) {
                dbl /= factorE;
            } else {
                dbl *= factorE;
            }
        }
        double integralPart;
        if (modf(dbl, &integralPart) == 0.0
            && dbl >= -9007199254740992.0
            && dbl <= 9007199254740992.0) {
            long long i64 = (long long)dbl;
            return PyLong_FromLongLong(i64);
        } else {
            return PyFloat_FromDouble(dbl);
        }
    }
}

/*
 * dump_value, dump_dict, dump_list:
 * These handle writing Python data -> JSON5 text into a buffer.
 */
static void dump_value(PyObject *obj, int indent, int level,
                       char **buffer, size_t *len, size_t *cap);
static void dump_dict(PyObject *obj, int indent, int level,
                      char **buffer, size_t *len, size_t *cap);
static void dump_list(PyObject *obj, int indent, int level,
                      char **buffer, size_t *len, size_t *cap);

static INLINE void grow(char **buf, size_t *used, size_t *capacity, size_t need) {
    if (!*buf) {
        return;
    }
    if ((*used) + need >= (*capacity)) {
        while ((*used) + need >= (*capacity)) {
            (*capacity) <<= 1;
        }
        char *tmp = (char*)realloc(*buf, (*capacity));
        if (!tmp) {
            free(*buf);
            *buf = NULL;
            *capacity = 0;
            *used = 0;
        } else {
            *buf = tmp;
        }
    }
}

static INLINE void append_char(char **buf, size_t *used, size_t *cap, char c) {
    if (!*buf) {
        return;
    }
    grow(buf, used, cap, 1);
    if (*buf) {
        (*buf)[(*used)++] = c;
    }
}

static INLINE void append_str(char **buf, size_t *used, size_t *cap, const char *s) {
    if (!*buf) {
        return;
    }
    size_t n = strlen(s);
    grow(buf, used, cap, n);
    if (*buf) {
        memcpy((*buf) + (*used), s, n);
        (*used) += n;
    }
}

static INLINE void append_indent(int indent, int level,
                                 char **buf, size_t *used, size_t *cap) {
    if (!*buf || indent <= 0) {
        return;
    }
    int total = indent * level;
    grow(buf, used, cap, (size_t)total);
    if (*buf) {
        memset((*buf) + (*used), ' ', (size_t)total);
        (*used) += total;
    }
}

static void dump_value(PyObject *obj, int indent, int level,
                       char **buffer, size_t *len, size_t *cap) {
    if (!*buffer) {
        return;
    }
    if (obj == Py_None) {
        append_str(buffer, len, cap, "null");
    } else if (PyBool_Check(obj)) {
        if (obj == Py_True) {
            append_str(buffer, len, cap, "true");
        } else {
            append_str(buffer, len, cap, "false");
        }
    } else if (PyLong_Check(obj)) {
        PyObject *tmp = PyObject_Str(obj);
        if (!tmp) {
            return;
        }
        const char *s = PyUnicode_AsUTF8(tmp);
        if (s) {
            append_str(buffer, len, cap, s);
        }
        Py_DECREF(tmp);
    } else if (PyFloat_Check(obj)) {
        PyObject *tmp = PyObject_Repr(obj);
        if (!tmp) {
            return;
        }
        const char *s = PyUnicode_AsUTF8(tmp);
        if (s) {
            append_str(buffer, len, cap, s);
        }
        Py_DECREF(tmp);
    } else if (PyUnicode_Check(obj)) {
        append_char(buffer, len, cap, '\"');
        Py_ssize_t sz = PyUnicode_GET_LENGTH(obj);
        for (Py_ssize_t i = 0; i < sz; i++) {
            Py_UCS4 ch = PyUnicode_ReadChar(obj, i);
            switch (ch) {
                case '\"': append_str(buffer, len, cap, "\\\""); break;
                case '\\': append_str(buffer, len, cap, "\\\\"); break;
                case '\b': append_str(buffer, len, cap, "\\b");  break;
                case '\f': append_str(buffer, len, cap, "\\f");  break;
                case '\n': append_str(buffer, len, cap, "\\n");  break;
                case '\r': append_str(buffer, len, cap, "\\r");  break;
                case '\t': append_str(buffer, len, cap, "\\t");  break;
                default:
                    if (ch < 0x80) {
                        append_char(buffer, len, cap, (char)ch);
                    } else {
                        PyObject *one = PyUnicode_New(1, ch);
                        if (one) {
                            PyUnicode_WriteChar(one, 0, ch);
                            const char *u = PyUnicode_AsUTF8(one);
                            if (u) {
                                append_str(buffer, len, cap, u);
                            }
                            Py_DECREF(one);
                        }
                    }
                    break;
            }
            if (!*buffer) {
                return;
            }
        }
        append_char(buffer, len, cap, '\"');
    } else if (PyDict_Check(obj)) {
        dump_dict(obj, indent, level, buffer, len, cap);
    } else if (PyList_Check(obj) || PyTuple_Check(obj)) {
        dump_list(obj, indent, level, buffer, len, cap);
    } else {
        PyObject *tmp = PyObject_Str(obj);
        if (!tmp) {
            return;
        }
        const char *s = PyUnicode_AsUTF8(tmp);
        if (s) {
            append_str(buffer, len, cap, s);
        }
        Py_DECREF(tmp);
    }
}

static void dump_dict(PyObject *obj, int indent, int level,
                      char **buffer, size_t *len, size_t *cap) {
    append_char(buffer, len, cap, '{');
    Py_ssize_t dsize = PyDict_Size(obj);
    if (dsize == 0) {
        append_char(buffer, len, cap, '}');
        return;
    }
    if (indent > 0) {
        append_char(buffer, len, cap, '\n');
    }
    Py_ssize_t pos = 0;
    Py_ssize_t i = 0;
    PyObject *key, *val;

    while (PyDict_Next(obj, &pos, &key, &val)) {
        i++;
        append_indent(indent, level + 1, buffer, len, cap);
        dump_value(key, indent, level + 1, buffer, len, cap);
        append_str(buffer, len, cap, ": ");
        dump_value(val, indent, level + 1, buffer, len, cap);
        if (i < dsize) {
            append_char(buffer, len, cap, ',');
        }
        if (indent > 0) {
            append_char(buffer, len, cap, '\n');
        }
        if (!*buffer) {
            return;
        }
    }
    append_indent(indent, level, buffer, len, cap);
    append_char(buffer, len, cap, '}');
}

static void dump_list(PyObject *obj, int indent, int level,
                      char **buffer, size_t *len, size_t *cap) {
    append_char(buffer, len, cap, '[');
    Py_ssize_t sz = PySequence_Size(obj);
    if (sz == 0) {
        append_char(buffer, len, cap, ']');
        return;
    }
    if (indent > 0) {
        append_char(buffer, len, cap, '\n');
    }
    for (Py_ssize_t i = 0; i < sz; i++) {
        PyObject *item = PySequence_GetItem(obj, i);
        append_indent(indent, level + 1, buffer, len, cap);
        dump_value(item, indent, level + 1, buffer, len, cap);
        Py_DECREF(item);
        if (i < sz - 1) {
            append_char(buffer, len, cap, ',');
        }
        if (indent > 0) {
            append_char(buffer, len, cap, '\n');
        }
        if (!*buffer) {
            return;
        }
    }
    append_indent(indent, level, buffer, len, cap);
    append_char(buffer, len, cap, ']');
}

PyObject* parse_json5(const char *input) {
    if (!input) {
        PyErr_SetString(PyExc_ValueError, "No input data");
        return NULL;
    }
    const char *ptr = input;
    skip_whitespace(&ptr);
    PyObject *val = parse_value(&ptr);
    if (!val) {
        return NULL;
    }
    skip_whitespace(&ptr);
    if (*ptr != '\0') {
        Py_XDECREF(val);
        PyErr_SetString(PyExc_ValueError, "Extra data after top-level value");
        return NULL;
    }
    return val;
}

PyObject* dump_json5(PyObject *obj, int indent) {
    if (!obj) {
        PyErr_SetString(PyExc_ValueError, "dump_json5() called with null object");
        return NULL;
    }
    size_t capacity = 256;
    size_t length = 0;
    char *buf = (char*)malloc(capacity);
    if (!buf) {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }
    dump_value(obj, indent, 0, &buf, &length, &capacity);
    if (!buf) {
        return NULL;
    }
    grow(&buf, &length, &capacity, 1);
    if (!buf) {
        return NULL;
    }
    buf[length] = '\0';
    PyObject *res = PyUnicode_FromStringAndSize(buf, (Py_ssize_t)length);
    free(buf);
    return res;
}
