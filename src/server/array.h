
#ifndef ARRAY_H__
#define ARRAY_H__

#include <stddef.h>

struct vector
{
    size_t element_size;
    size_t size;
    size_t capacity;
    void *data;
};

void vector_init(struct vector *vec, size_t element_size, size_t capacity);
void vector_clear(struct vector *vec);
void vector_set(struct vector *vec, size_t pos, void *elem);
void *vector_get(struct vector *vec, size_t pos);
void vector_realloc(struct vector *vec, size_t new_capacity);
void vector_shrink(struct vector *vec);
void vector_push_back(struct vector *vec, void *elem);


#endif