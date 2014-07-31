
#include <stdio.h>
#include "readstat.h"

int get_jiffy_info(struct jiffyinfo_t *info)
{    
    FILE *f = fopen("/proc/stat", "r");
    if (!f)
        goto error;
    
    int jiffies[7];
    int numVals = fscanf(f, "cpu %i %i %i %i %i %i %i", 
        &jiffies[0], &jiffies[1], &jiffies[2], &jiffies[3],
        &jiffies[4], &jiffies[5], &jiffies[6]);
    fclose(f);
    if (numVals != 7)
        goto error;
    
    info->total = 0;
    for (int i = 0; i < 7; ++i)
        info->total += jiffies[i];
    info->idle = jiffies[3];
    return 1;
    
error:
    printf("Error reading process file");
    return 0;
}


int get_cpu_util(const struct jiffyinfo_t *lastInfo, const struct jiffyinfo_t *nowInfo)
{
    int diff_total = nowInfo->total - lastInfo->total;
    int diff_idle = nowInfo->idle - lastInfo->idle;
    
    int permil = 1000 - ((diff_idle * 1000) / diff_total);
    return permil;
}

