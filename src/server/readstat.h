
#ifndef READSTAT_H__
#define READSTAT_H__

struct jiffyinfo_t
{
    int total;
    int idle;
};

int get_jiffy_info(struct jiffyinfo_t *info);
int get_cpu_util(const struct jiffyinfo_t *lastInfo, const struct jiffyinfo_t *nowInfo);

#endif
