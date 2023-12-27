#ifndef NOB_H_
#define NOB_H_
#include <ctype.h>

#ifndef NOB_TEMP_CAPACITY
#define NOB_TEMP_CAPACITY (8 * 1024 * 1024)
#endif // NOB_TEMP_CAPACITY

typedef struct
{
    char *items;
    size_t count;
    size_t capacity;
} String_Builder;

typedef struct
{
    size_t count;
    const char *data;
} String_View;

// Free the memory allocated by a string builder
#define sb_free(sb) free((sb).items)

String_View sv_from_parts(const char *data, size_t count);
String_View sv_chop_by_delim(String_View *sv, char delim);
String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
String_View sv_trim(String_View sv);
String_View sv_from_cstr(const char *cstr);
bool nob_sv_eq(String_View a, String_View b);
bool read_entire_file(const char *path, String_Builder *sb);
#endif // NOB_H_

#ifdef NOB_IMPLEMENTATION
static size_t temp_size = 0;
static char nob_temp[NOB_TEMP_CAPACITY] = {0};

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define ARRAY_GET(array, index) \
    (NOB_ASSERT(index >= 0), NOB_ASSERT(index < NOB_ARRAY_LEN(array)), array[index])

#define return_defer(value) \
    do                      \
    {                       \
        result = (value);   \
        goto defer;         \
    } while (0)

// Initial capacity of a dynamic array
#define DA_INIT_CAP 256

// Append several items to a dynamic array
#define da_append_many(da, new_items, new_items_count)                                        \
    do                                                                                        \
    {                                                                                         \
        if ((da)->count + new_items_count > (da)->capacity)                                   \
        {                                                                                     \
            if ((da)->capacity == 0)                                                          \
            {                                                                                 \
                (da)->capacity = DA_INIT_CAP;                                                 \
            }                                                                                 \
            while ((da)->count + new_items_count > (da)->capacity)                            \
            {                                                                                 \
                (da)->capacity *= 2;                                                          \
            }                                                                                 \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items));        \
            assert((da)->items != NULL && "Buy more RAM lol");                                \
        }                                                                                     \
        memcpy((da)->items + (da)->count, new_items, new_items_count * sizeof(*(da)->items)); \
        (da)->count += new_items_count;                                                       \
    } while (0)

void *temp_alloc(size_t size)
{
    if (temp_size + size > NOB_TEMP_CAPACITY)
        return NULL;
    void *result = &nob_temp[temp_size];
    temp_size += size;
    return result;
}

const char *temp_sv_to_cstr(String_View sv)
{
    char *result = temp_alloc(sv.count + 1);
    assert(result != NULL && "Extend the size of the temporary allocator");
    memcpy(result, sv.data, sv.count);
    result[sv.count] = '\0';
    return result;
}

int temp_sv_to_int(String_View sv)
{
    char *result = temp_alloc(sv.count + 1);
    assert(result != NULL && "Extend the size of the temporary allocator");
    memcpy(result, sv.data, sv.count);
    result[sv.count] = '\0';
    return atoi(result);
}

String_View sv_from_parts(const char *data, size_t count)
{
    String_View sv;
    sv.count = count;
    sv.data = data;
    return sv;
}

String_View sv_chop_by_delim(String_View *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim)
    {
        i += 1;
    }

    String_View result = sv_from_parts(sv->data, i);

    if (i < sv->count)
    {
        sv->count -= i + 1;
        sv->data += i + 1;
    }
    else
    {
        sv->count -= i;
        sv->data += i;
    }

    return result;
}

String_View sv_trim_left(String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i]))
    {
        i += 1;
    }

    return sv_from_parts(sv.data + i, sv.count - i);
}

String_View sv_trim_right(String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i]))
    {
        i += 1;
    }

    return sv_from_parts(sv.data, sv.count - i);
}

String_View sv_trim(String_View sv)
{
    return sv_trim_right(sv_trim_left(sv));
}

String_View sv_from_cstr(const char *cstr)
{
    return sv_from_parts(cstr, strlen(cstr));
}

bool nob_sv_eq(String_View a, String_View b)
{
    if (a.count != b.count)
    {
        return false;
    }
    else
    {
        return memcmp(a.data, b.data, a.count) == 0;
    }
}

#define sb_append_buf(sb, buf, size) da_append_many(sb, buf, size)

bool read_entire_file(const char *path, String_Builder *sb)
{
    bool result = true;

    size_t buf_size = 32 * 1024;
    char *buf = realloc(NULL, buf_size);
    assert(buf != NULL && "Buy more RAM lool!!\n");
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        printf("[ ERR ]: Could not open %s for reading: %s\n", path, strerror(errno));
        return_defer(false);
    }

    size_t n = fread(buf, 1, buf_size, f);
    while (n > 0)
    {
        sb_append_buf(sb, buf, n);
        n = fread(buf, 1, buf_size, f);
    }
    if (ferror(f))
    {
        printf("[ ERR ]: Could not read %s: %s\n", path, strerror(errno));
        return_defer(false);
    }

defer:
    free(buf);
    if (f)
        fclose(f);
    return result;
}

#endif // NOB_IMPLEMENTATION