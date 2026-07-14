#ifndef __FAULT_DETECT_H
#define __FAULT_DETECT_H

#include "measure.h"

typedef enum
{
    FAULT_NONE = 0,
    FAULT_R1_OPEN,
    FAULT_R1_SHORT,
    FAULT_R2_OPEN,
    FAULT_R2_SHORT,
    FAULT_R3_OPEN,
    FAULT_R3_SHORT,
    FAULT_R4_OPEN,
    FAULT_R4_SHORT,
    FAULT_C1_OPEN,
    FAULT_C1_DOUBLE,
    FAULT_C2_OPEN,
    FAULT_C2_DOUBLE,
    FAULT_C3_OPEN,
    FAULT_C3_DOUBLE,
    FAULT_UNKNOWN
} FaultType;

/* 在电路已知正常的状态下调用一次，保存基准数据（增益、截止频率、相位等） */
void FaultDetect_SetBaseline(const MeasureResult *baseline);

/* 用当前一次测量结果与基准比较，返回故障判断结果 */
FaultType FaultDetect_Run(const MeasureResult *current);

const char *FaultDetect_TypeName(FaultType type);

#endif
