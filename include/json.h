#ifndef CBLOG_JSON_H
#define CBLOG_JSON_H

#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonPair  JsonPair;

struct JsonValue {
    JsonType type;
    union {
        bool    boolean;
        double  number;
        char   *string;
        struct {
            JsonValue **items;
            int         count;
        } array;
        struct {
            JsonPair *pairs;
            int       count;
        } object;
    } u;
};

struct JsonPair {
    char      *key;
    JsonValue *value;
};

/* Parse a JSON string. Returns NULL on error. */
JsonValue *json_parse(const char *input);

/* Free a parsed JSON value tree. */
void json_free(JsonValue *v);

/* Lookup helpers */
JsonValue  *json_object_get(const JsonValue *obj, const char *key);
const char *json_string_value(const JsonValue *v);
double      json_number_value(const JsonValue *v);
bool        json_bool_value(const JsonValue *v);

/* Serialize a JSON value to a newly allocated string. Caller frees. */
char *json_serialize(const JsonValue *v);

/* Builder helpers for writing config */
JsonValue *json_new_string(const char *s);
JsonValue *json_new_bool(bool b);
JsonValue *json_new_number(double n);
JsonValue *json_new_object(void);
void       json_object_set(JsonValue *obj, const char *key, JsonValue *val);

#endif /* CBLOG_JSON_H */
