#include "struct_typedef.h"

void Exchange_task(void const * argument);

typedef struct INS_DATA
{
    fp32 accel_offset[3];
    fp32 gyro_offset[3];
    fp32 accel[3];
    fp32 temp;
    fp32 gyro[3];
    fp32 angle[3];
    fp32 INS_quat[4];  
} ins_data_t;