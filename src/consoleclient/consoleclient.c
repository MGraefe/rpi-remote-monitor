
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "libclient/include.h"



void print_measurement_data(struct measurement *m)
{
    printf("%s : ", m->name);
    switch(m->type)
    {
    case MT_INT8:
        printf("%i", (int)*(int8_t*)(m->data));
        break;
    case MT_INT16:
        printf("%i", (int)*(int16_t*)(m->data));
        break;
    case MT_INT32:
        printf("%i", (int)*(int32_t*)(m->data));
        break;

    case MT_UINT8:
        printf("%u", (int)*(uint8_t*)(m->data));
        break;
    case MT_UINT16:
        printf("%u", (int)*(uint16_t*)(m->data));
        break;
    case MT_UINT32:
        printf("%u", (int)*(uint32_t*)(m->data));
        break;

    case MT_FLOAT:
        printf("%f", *(float*)(m->data));
        break;
    case MT_STRING:
        printf("%s", m->data);
        break;
    }
    printf("\n");
}


int main(int argc, char **argv)
{
    size_t num_measurements = 0;
    struct measurement *measurements = NULL;
    enum rprm_error err;
    size_t i;
    struct rprm_connection conn;

    if (argc != 2)
    {
        printf("Usage: rprm-consoleclient <ip>\n");
        return 1;
    }

    err = rprm_connect(&conn, argv[1], 29100, IPV4);
    if (err != RPRM_ERROR_NONE)
        goto error;

    while (1)
    {
        num_measurements = 0;
        err = rprm_receive(&conn, &num_measurements);
        if (err != RPRM_ERROR_NONE)
            goto error;

        measurements = malloc(num_measurements * sizeof(struct measurement));

        err = rprm_get_data(&conn, measurements);
        if (err != RPRM_ERROR_NONE)
            goto error;

        for(i = 0; i < num_measurements; i++)
            print_measurement_data(&measurements[i]);
        printf("\n");

        free(measurements);
    }

    return 0;

error:
    if (measurements)
        free(measurements);
    printf("Error: %i", err);
    return 1;
}
