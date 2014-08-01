
#ifndef MEASUREMENT_H__
#define MEASUREMENT_H__

enum measurement_datatype
{
    MT_INT8,
    MT_INT16,
    MT_INT32,

    MT_UINT8,
    MT_UINT16,
    MT_UINT32,

    MT_FLOAT,
    MT_STRING,
};

struct measurement
{
    const char *name;
    enum measurement_datatype type;
    void *data;
};

struct measure_callback
{
    size_t num_measures;
    int (*init_func)(void); //returns 0 on success
    int (*measure_func)(struct measurement *measurements); //returns number of successfull measurements
};


#endif
