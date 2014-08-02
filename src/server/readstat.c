
#include <stdio.h>
#include "readstat.h"
#include "measurement.h"

struct jiffyinfo_t
{
    int total;
    int idle;
};


int get_jiffy_info(struct jiffyinfo_t *info)
{
    int i;
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
    for (i = 0; i < 7; ++i)
        info->total += jiffies[i];
    info->idle = jiffies[3];
    return 0;
    
error:
    printf("Error reading process file");
    return 1;
}


int get_cpu_util(const struct jiffyinfo_t *lastInfo, const struct jiffyinfo_t *nowInfo)
{
    int diff_total = nowInfo->total - lastInfo->total;
    int diff_idle = nowInfo->idle - lastInfo->idle;
    
    if (diff_total <= 0 || diff_idle <= 0)
        return 0;

    int permil = 1000 - ((diff_idle * 1000) / diff_total);
    return permil;
}


//Callback handling
struct jiffyinfo_t lastInfo, nowInfo;
int cpuUtil;

int init_jiffies()
{
    get_jiffy_info(&lastInfo);
    return 0;
}

int measure_jiffies(struct measurement *measurements)
{
    if (get_jiffy_info(&nowInfo) != 0)
        return 0;
    
    cpuUtil = get_cpu_util(&lastInfo, &nowInfo);
    lastInfo = nowInfo;
    
    measurements[0].name = "cpu_util";
    measurements[0].type = MT_INT32;
    measurements[0].data = &cpuUtil;
    return 1;
}

struct measure_callback cpu_util_callback = {
    .num_measures = 1,
    .init_func = init_jiffies,
    .measure_func = measure_jiffies,
};
