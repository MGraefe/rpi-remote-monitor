
#include <stdlib.h>
#include <string.h>

#include "msgbuf.h"

void msgbuf_ensure_cap(struct msgbuf *buf, size_t size)
{
    if (buf->capacity >= size)
        return;
    if (buf->capacity == 0)
    {
        buf->data = malloc(size);
        buf->capacity = size;
    }
    else
    {
        size_t new_cap = buf->capacity;
        while (new_cap < size)
            new_cap *= 2;
        buf->data = realloc(buf->data, new_cap);
        buf->capacity = new_cap;
    }
}


void msgbuf_init(struct msgbuf *buf)
{
    buf->size = 0;
    buf->capacity = 0;
    buf->data = NULL;
}


void msgbuf_app(struct msgbuf *buf, const void *data, size_t size)
{
    msgbuf_ensure_cap(buf, buf->size + size);
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
}


void msgbuf_app_int(struct msgbuf *buf, uint32_t i)
{
    uint32_t hi = htonl(i);
    msgbuf_app(buf, &hi, sizeof(hi));
}

void msgbuf_app_short(struct msgbuf *buf, uint16_t i)
{
    uint16_t hi = htons(i);
    msgbuf_app(buf, &hi, sizeof(hi));
}

void msgbuf_app_byte(struct msgbuf *buf, uint8_t i)
{
    msgbuf_app(buf, &i, sizeof(i));
}

void msgbuf_app_string(struct msgbuf *buf, const char *str)
{
    size_t len = strlen(str);
    msgbuf_app(buf, str, len+1);
}

void msgbuf_app_float(struct msgbuf *buf, float f)
{
    msgbuf_app(buf, &f, sizeof(float));
}

void msgbuf_clear(struct msgbuf *buf)
{
    buf->size = 0;
}

void msgbuf_dealloc(struct msgbuf *buf)
{
    if (buf->data != NULL)
        free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}
