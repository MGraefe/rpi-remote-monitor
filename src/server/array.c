#include "array.h"

#include <string.h>
#include <stdlib.h>

void vector_init(struct vector *vec, size_t element_size, size_t capacity)
{
    vec->element_size = element_size;
    vec->size = 0;
    vec->capacity = capacity;
    if (capacity > 0)
        vec->data = malloc(capacity * element_size);
    else
        vec->data = 0;
}

void vector_clear(struct vector *vec)
{
    vec->size = 0;
}

void vector_set(struct vector *vec, size_t pos, void *elem)
{
    memcpy(vec->data + pos * vec->element_size, elem, vec->element_size);
}

void *vector_get(struct vector *vec, size_t pos)
{
    return vec->data + pos * vec->element_size;
}

void vector_realloc(struct vector *vec, size_t new_capacity)
{
    if (new_capacity == 0)
    {
        if (vec->data != 0)
        {
            free(vec->data);
            vec->data = 0;
        }
    }
    else
        vec->data = realloc(vec->data, vec->element_size * new_capacity);
    vec->capacity = new_capacity;
    if (new_capacity < vec->size)
        vec->size = new_capacity;
}

void vector_shrink(struct vector *vec)
{
    vector_realloc(vec, vec->size);
}

void vector_push_back(struct vector *vec, void *elem)
{
    if (vec->size >= vec->capacity)
        vector_realloc(vec, vec->capacity * 2);
    vector_set(vec, vec->size, elem);
    vec->size++;
}