
#include <stdio.h>

#include "libclient/include.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: rprm-consoleclient <ip>\n");
        return 1;
    }

    struct rprm_client client;
    enum rprm_error err = rprm_connect(&client, argv[1], 29100, IPV4);
    if (err != RPRM_ERROR_NONE)
    {
        printf("Error: %i\n", err);
        return 1;
    }

    while (1)
    {
        err = rprm_receive(&client);
        if (err != RPRM_ERROR_NONE)
        {
            printf("Error: %i\n", err);
            return 1;
        }

        size_t i;
        for(i = 0; i < client.num_measurements; i++)
        {
            printf("%s : ", client.measurements[i].name);
            switch(client.measurements[i].type)
            {
            case MT_INT8:
                printf("%i", (int)*(int8_t*)(client.measurements[i].data));
                break;
            case MT_INT16:
                printf("%i", (int)*(int16_t*)(client.measurements[i].data));
                break;
            case MT_INT32:
                printf("%i", (int)*(int32_t*)(client.measurements[i].data));
                break;

            case MT_UINT8:
                printf("%u", (int)*(uint8_t*)(client.measurements[i].data));
                break;
            case MT_UINT16:
                printf("%u", (int)*(uint16_t*)(client.measurements[i].data));
                break;
            case MT_UINT32:
                printf("%u", (int)*(uint32_t*)(client.measurements[i].data));
                break;

            case MT_FLOAT:
                printf("%f", *(float*)(client.measurements[i].data));
                break;
            case MT_STRING:
                printf("%s", client.measurements[i].data);
                break;
            }
            printf("\n");
        }
        printf("\n");
    }
    return 0;
}
