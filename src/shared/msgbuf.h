
#ifndef MSGBUF_H__
#define MSGBUF_H__

#include <stddef.h>
#include <stdint.h>

//Call msgbuf_init before usage!!
struct msgbuf
{
    size_t size;
    size_t capacity;
    char *data;
};

void msgbuf_init(struct msgbuf *buf);

void msgbuf_app(struct msgbuf *buf, const void *data, size_t size);

void msgbuf_app_int(struct msgbuf *buf, uint32_t i);

void msgbuf_app_short(struct msgbuf *buf, uint16_t i);

void msgbuf_app_byte(struct msgbuf *buf, uint8_t i);

void msgbuf_app_string(struct msgbuf *buf, const char *str);

void msgbuf_app_float(struct msgbuf *buf, float f);

void msgbuf_clear(struct msgbuf *buf);

void msgbuf_dealloc(struct msgbuf *buf);


#endif
