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

// Forward declarations
static INLINE void skip_whitespace(const char **p);
static INLINE PyObject* parse_value(const char **p);
static PyObject* parse_object(const char **p);
static PyObject* parse_array(const char **p);
static PyObject* parse_string(const char **p);
static INLINE PyObject* parse_number(const char **p);
static INLINE PyObject* parse_true(const char **p);
static INLINE PyObject* parse_false(const char **p);
static INLINE PyObject* parse_null(const char **p);

/* Forward declaration for fastNum */
static PyObject* fastNum(const char *start, const char **end);

/*
 * skip_whitespace:
 * Advances the pointer over whitespace and comments.
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
 * parse_array:
 * Creates a PyList from JSON5 array syntax.
 * Simplified to use dynamic appending.
 */
static PyObject* parse_array(const char **ref) {
    PyObject *lst = PyList_New(0);
    if (!lst) {
        return NULL;
    }

    while (1) {
        skip_whitespace(ref);
        if (**ref == ']') {
            (*ref)++;
            break;
        }

        PyObject *val = parse_value(ref);
        if (!val) {
            Py_DECREF(lst);
            return NULL;
        }
        if (PyList_Append(lst, val) < 0) {
            Py_DECREF(val);
            Py_DECREF(lst);
            return NULL;
        }
        Py_DECREF(val);

        skip_whitespace(ref);
        if (**ref == ',') {
            (*ref)++;
            // Allow trailing comma: if next is ']', finish parsing.
            skip_whitespace(ref);
            if (**ref == ']') {
                (*ref)++;
                break;
            }
        } else if (**ref == ']') {
            (*ref)++;
            break;
        } else {
            Py_DECREF(lst);
            RAISE("Expected ']' or ','");
        }
    }
    return lst;
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
            // Skip line continuation if backslash is immediately followed by a newline.
            if (c == '\n' || c == '\r') {
                if (c == '\r' && (*ref)[1] == '\n') {
                    (*ref)++;
                }
                (*ref)++;
                continue;
            }
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
    return fastNum(*ref, ref);
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
static PyObject* fastNum(const char *start, const char **end) {
    const char *p = start;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    // Check for hexadecimal literal: e.g. 0xdecaf
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        unsigned long long hexVal = 0;
        int hasHex = 0;
        while (isxdigit((unsigned char)*p)) {
            hasHex = 1;
            int digit = 0;
            if (*p >= '0' && *p <= '9')
                digit = *p - '0';
            else if (*p >= 'a' && *p <= 'f')
                digit = *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F')
                digit = *p - 'A' + 10;
            hexVal = hexVal * 16 + digit;
            p++;
        }
        if (!hasHex) {
            PyErr_SetString(PyExc_ValueError, "Invalid hexadecimal number");
            return NULL;
        }
        *end = p;
        if (sign < 0) {
            if (hexVal <= 0x8000000000000000ULL)
                return PyLong_FromLongLong(-(long long)hexVal);
            else
                return PyFloat_FromDouble(-(double)hexVal);
        } else {
            if (hexVal <= 0x7FFFFFFFFFFFFFFFULL)
                return PyLong_FromLongLong((long long)hexVal);
            else
                return PyFloat_FromDouble((double)hexVal);
        }
    }

    int isFloat = 0;
    double frac = 0.0;
    double factor = 0.1;
    int hasDig = 0;
    long long iPart = 0;

    // Support numbers starting with a dot (e.g. .8675309)
    if (*p == '.') {
        isFloat = 1;
        p++;
        while (*p >= '0' && *p <= '9') {
            hasDig = 1;
            frac += (*p - '0') * factor;
            factor *= 0.1;
            p++;
        }
    } else {
        // Parse integer part
        while (*p >= '0' && *p <= '9') {
            hasDig = 1;
            iPart = (iPart * 10) + (*p - '0');
            p++;
        }
        // Check for decimal point (allow trailing dot)
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
        PyErr_SetString(PyExc_ValueError, "Invalid number literal");
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
            double factorE = pow(10.0, eVal);
            if (eSign < 0) {
                dbl /= factorE;
            } else {
                dbl *= factorE;
            }
        }
        double integralPart;
        if (modf(dbl, &integralPart) == 0.0 &&
            dbl >= -9007199254740992.0 &&
            dbl <= 9007199254740992.0) {
            long long i64 = (long long)dbl;
            return PyLong_FromLongLong(i64);
        } else {
            return PyFloat_FromDouble(dbl);
        }
    }
}

/*
 * Dumping functions: dump_value, dump_dict, dump_list
 * Convert Python objects to JSON5 text.
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
            PyErr_NoMemory();
            return;
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
        const char *s = PyUnicode_AsUTF8(obj);
        if (!s) {
            return;
        }
        append_char(buffer, len, cap, '\"');
        for (const char *p = s; *p; p++) {
            unsigned char ch = (unsigned char)*p;
            switch (ch) {
                case '\"': append_str(buffer, len, cap, "\\\""); break;
                case '\\': append_str(buffer, len, cap, "\\\\"); break;
                case '\b': append_str(buffer, len, cap, "\\b");  break;
                case '\f': append_str(buffer, len, cap, "\\f");  break;
                case '\n': append_str(buffer, len, cap, "\\n");  break;
                case '\r': append_str(buffer, len, cap, "\\r");  break;
                case '\t': append_str(buffer, len, cap, "\\t");  break;
                default:
                    if (ch < 0x20) {
                        char esc[7];
                        snprintf(esc, sizeof(esc), "\\u%04x", ch);
                        append_str(buffer, len, cap, esc);
                    } else {
                        append_char(buffer, len, cap, (char)ch);
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
            if (indent == 0) {
                append_char(buffer, len, cap, ' ');
            }
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
    Py_ssize_t sz = 0;
    int isList = PyList_Check(obj);
    if (isList) {
        sz = PyList_GET_SIZE(obj);
    } else {
        sz = PySequence_Size(obj);
    }
    if (sz == 0) {
        append_char(buffer, len, cap, ']');
        return;
    }
    if (indent > 0) {
        append_char(buffer, len, cap, '\n');
    }
    for (Py_ssize_t i = 0; i < sz; i++) {
        PyObject *item = NULL;
        if (isList) {
            item = PyList_GET_ITEM(obj, i);
            Py_INCREF(item);
        } else {
            item = PySequence_GetItem(obj, i);
        }
        append_indent(indent, level + 1, buffer, len, cap);
        dump_value(item, indent, level + 1, buffer, len, cap);
        Py_DECREF(item);
        if (i < sz - 1) {
            append_char(buffer, len, cap, ',');
            if (indent == 0) {
                append_char(buffer, len, cap, ' ');
            }
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
