#ifndef __SOFT_TIMER_H__
#define __SOFT_TIMER_H__

#include "stdint.h"

//-------------------config----------↓↓↓↓↓↓↓
#define WITH_FREE_RTOS  1   
#define TIMER_NUM_MAX   64  

//毫秒源---RTOS tick或其他timer中断
#if !WITH_FREE_RTOS
//使用裸机需要实现一个1ms产生中断的Timer，并将msTickIncrease放入中断响应函数
extern void msTickIncrease();
#endif

//微秒源---系统计时器
#if defined(GD32F30X_HD) || defined(GD32F30X_XD) || defined(GD32F30X_CL)
#define SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#elif defined(OTHER_CHIP)//自行修改
#define SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#else
#define SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#endif
//-------------------config----------↑↑↑↑↑↑↑


typedef struct
{
    uint64_t  timeCache;
    uint32_t  timeCostMs;
    uint32_t  timeCostUs;
    uint32_t  timerIndex;
} osTimeCost_T;

void osTaskDelay(uint16_t ms);
void osTaskGetTimeCost(osTimeCost_T* pTimeCost);

uint8_t osTaskIsTimeOutMs(volatile uint32_t* pTimer, uint32_t u32TimeOutMs);
uint8_t osTaskIsTimeOutUs(volatile uint32_t* pTimer, uint32_t u32TimeOutUs);

//死循环延时us
void osDelayByLoop_us(uint32_t u32nus);
void osDelayByLoop_ms(uint32_t u32nms);

#endif