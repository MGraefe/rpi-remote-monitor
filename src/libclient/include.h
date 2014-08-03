
#ifndef LIBCLIENT_INCLUDE_H__
#define LIBCLIENT_INCLUDE_H__

#ifdef _WIN32
#define LIBCLIENT_EXPORT __declspec(dllexport)
#define LIBCLIENT_IMPORT __declspec(dllimport)
#define LIBCLIENT_PRIVATE
#else
#define LIBCLIENT_EXPORT __attribute__ ((visibility ("default")))
#define LIBCLIENT_IMPORT __attribute__ ((visibility ("default")))
#define LIBCLIENT_PRIVATE __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBCLIENT_EXPORT_SYMBOLS
#define LIBCLIENT_PUBLIC LIBCLIENT_EXPORT
#else
#define LIBCLIENT_PUBLIC LIBCLIENT_IMPORT
#endif

#include <stddef.h>
#include "shared/measurement.h"

struct rprm_connection
{
    void *internals;
};

enum rprm_connection_type
{
    UNSPEC,
    IPV4,
    IPV6,
};

enum rprm_error
{
    RPRM_ERROR_NONE = 0,
    RPRM_ERROR_INVALID_PORT,
    RPRM_ERROR_RESOLVE_ADRESS,
    RPRM_ERROR_COULD_NOT_CONNECT,
    RPRM_ERROR_DISCONNECTED,
    RPRM_ERROR_PROTOCOL_VERSION,
    RPRM_ERROR_INVALID_PACKET,
};

LIBCLIENT_PUBLIC
enum rprm_error rprm_connect(struct rprm_connection *conn,
        const char *address, int port, enum rprm_connection_type type);

LIBCLIENT_PUBLIC
enum rprm_error rprm_receive(const struct rprm_connection *conn, size_t *num_measurements);

LIBCLIENT_PUBLIC
enum rprm_error rprm_get_data(struct rprm_connection *conn,
        struct measurement *measurements);

#endif
