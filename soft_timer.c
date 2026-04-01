#include "soft_timer.h"
#include "bsp.h"
#include "stdbool.h"



#if WITH_FREE_RTOS
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
TickType_t xTaskGetTickCountFromISR(void);
#define GetSysTick()          (__get_IPSR()?xTaskGetTickCountFromISR():xTaskGetTickCount())
#else
static uint64_t msTickCnt;
void msTickIncrease()
{
    msTickCnt++;
}
#define GetSysTick()          (msTickCnt)
#endif

#define NOP()             {asm("nop");}


/*
********************************************************************************
*                                osTaskGetTimeCost()
*                                   计时函数
* Description : 系统计时函数 ms
* Argument(s) : ms:单位ms、us(精度<5us)
* Return(s)   : 无
* Caller(s)   : Internal
* Note(s)     : 第一次调用初始化计时，之后每次调用得到两次时间差值.
              : 返回的index为0则为返回失败
              : 最多创建31个独立的计时
********************************************************************************
*/
void osTaskGetTimeCost(osTimeCost_T* pTimeCost)
{
    if (pTimeCost == NULL) return;
    //用静态数组不用堆，节省不必要的时间开销
    static uint32_t allTimeCostCache[TIMER_NUM_MAX] = { 0 };//allTimeCostCache[0]保留，如不为0则异常。
    static uint32_t currentTimeCostIndex = 0;

    uint32_t sysTickLoad = SYSTICK_LOAD_REG;
    uint32_t sysTickVal = SYSTICK_CURRENT_VALUE_REG;

    uint32_t u32TickDiff = (sysTickLoad - sysTickVal);
    uint32_t u32NowUs = u32TickDiff / (SystemCoreClock / 1000000);
    
    uint32_t u32NowMs = 1000 * GetSysTick() / configTICK_RATE_HZ;

    uint64_t u64Now = u32NowMs * 1000 + u32NowUs;
    uint64_t u64TempTime = 0;

    //判断是否不允许使用
    if (allTimeCostCache[0] != 0)
    {
        return;
    }

    //判断有无初始化
    if (currentTimeCostIndex == 0
        || allTimeCostCache[pTimeCost->timerIndex] != (uint32_t)(pTimeCost)
        || pTimeCost->timerIndex == 0)
    {
        currentTimeCostIndex++;
        //判断是否要生成新的计时器
        if (currentTimeCostIndex >= TIMER_NUM_MAX)
        {
            currentTimeCostIndex--;
            pTimeCost->timerIndex = 0;
            return;
        }
        pTimeCost->timerIndex = currentTimeCostIndex;
        allTimeCostCache[pTimeCost->timerIndex] = (uint32_t)(pTimeCost);
        pTimeCost->timeCostUs = u32NowUs;
        pTimeCost->timeCostMs = u32NowMs;
    }
    else
    {
        //计时和缓存
        if (pTimeCost->timeCache <= u64Now)
        {
            u64TempTime = u64Now - pTimeCost->timeCache;
        }
        else
        {
            //不可能出现
            u64TempTime = 0xFFFFFFFFFFFFFFFF - pTimeCost->timeCache + u64Now;
        }
        pTimeCost->timeCostUs = (uint32_t)(u64TempTime % 1000);
        pTimeCost->timeCostMs = (uint32_t)(u64TempTime / 1000);
    }
    pTimeCost->timeCache = u64Now;
}

/*
********************************************************************************
*                                osTaskIsTimeOutClear()
*                                 清除计时-重新计时
* Description : 清除计时-重新计时
* Argument(s) : 无
* Return(s)   : 无
* Caller(s)   : Internal
* Note(s)     : 清pTimer
********************************************************************************
*/
void osTaskIsTimeOutClear(volatile uint32_t* pTimer)
{
    *pTimer = 0;
}

/*
********************************************************************************
*                                osTaskIsTimeOutMs()
*                                   计时函数
* Description : 系统超时函数 ms
* Argument(s) : ms:单位ms
* Return(s)   : 无
* Caller(s)   : Internal
* Note(s)     : 第一次调用时确保pTimer == 0
********************************************************************************
*/
uint8_t osTaskIsTimeOutMs(volatile uint32_t* pTimer, uint32_t u32TimeOutMs)
{
    volatile uint32_t u32Temp = 0;
    uint32_t u32TimeNowMs = 1000 * GetSysTick() / configTICK_RATE_HZ;

    //第一次进入
    if (*pTimer == 0)
    {
        *pTimer = u32TimeNowMs;
        return false;
    }

    if (*pTimer <= u32TimeNowMs)
    {
        u32Temp = u32TimeNowMs - *pTimer;
    }
    else
    {
        u32Temp = 0xFFFFFFFF - *pTimer + u32TimeNowMs;
    }

    if (u32Temp >= u32TimeOutMs)
    {
        *pTimer = u32TimeNowMs;
        return true;
    }
    else
    {
        return false;
    }
}

/*
********************************************************************************
*                                osTaskIsTimeOutUs()
*                                   计时函数
* Description : 系统超时函数 us
* Argument(s) : us:单位us
* Return(s)   : 无
* Caller(s)   : Internal
* Note(s)     : 第一次调用时确保pTimer == 0
********************************************************************************
*/
uint8_t osTaskIsTimeOutUs(volatile uint32_t* pTimer, uint32_t u32TimeOutUs)
{
    volatile uint32_t u32Temp = 0;
    uint32_t u32TimeNowUs = 1000 * GetSysTick() / configTICK_RATE_HZ * 1000
        + (SYSTICK_LOAD_REG - SYSTICK_CURRENT_VALUE_REG) / (SystemCoreClock / 1000000);

    //第一次进入
    if (*pTimer == 0)
    {
        *pTimer = u32TimeNowUs;
        return false;
    }

    if (*pTimer <= u32TimeNowUs)
    {
        u32Temp = u32TimeNowUs - *pTimer;
    }
    else
    {
        if ((*pTimer - u32TimeNowUs) < 1000)
        {
            //sysTick 还没来得及更新
            u32TimeNowUs += 1000;
            u32Temp = u32TimeNowUs - *pTimer;
        }
        else
        {
            u32Temp = 0xFFFFFFFF - *pTimer + u32TimeNowUs;
            return true;
        }
    }

    if (u32Temp >= u32TimeOutUs)
    {
        *pTimer = u32TimeNowUs;
        return true;
    }
    else
    {
        return false;
    }
}

//死循环延时us
void osDelayByLoop_us(uint32_t u32nus)
{
    uint32_t sysTickLoad = SYSTICK_LOAD_REG;
    uint32_t sysTickVal = SYSTICK_CURRENT_VALUE_REG;

    uint32_t u32TimeCacheUs = sysTickLoad - sysTickVal;
    uint32_t u32TimeNowUs = u32TimeCacheUs;
    uint32_t u32TimeNowMs = 0;
    uint32_t u32MsIncreaseFlag = false;

    volatile uint32_t pTimer = 0;

    if (__get_IPSR() || u32nus <= 3)
    {
        u32nus *= (SystemCoreClock / 1000000);
        do
        {
            sysTickVal = SYSTICK_CURRENT_VALUE_REG;
            u32TimeNowUs = sysTickLoad - sysTickVal;
            if (u32TimeNowUs < u32TimeCacheUs)
            {
                if (u32MsIncreaseFlag)
                {
                    u32MsIncreaseFlag = false;
                    u32TimeNowMs += (SystemCoreClock / 1000000) * 1000;
                }
            }
            else
            {
                u32MsIncreaseFlag = true;
            }
        } while (u32TimeNowUs + u32TimeNowMs - u32TimeCacheUs <= u32nus);
    }
    else
    {
        while (!osTaskIsTimeOutUs(&pTimer, u32nus));
    }
}

//死循环延时ms
void osDelayByLoop_ms(uint32_t u32nms)
{
    uint32_t sysTickLoad = SYSTICK_LOAD_REG;
    uint32_t sysTickVal = SYSTICK_CURRENT_VALUE_REG;

    uint32_t u32TimeCacheUs = sysTickLoad - sysTickVal;
    uint32_t u32TimeNowUs = u32TimeCacheUs;
    uint32_t u32TimeNowMs = 0;
    uint32_t u32MsIncreaseFlag = false;

    volatile uint32_t pTimer = 0;

    if (__get_IPSR())
    {
        u32nms *= (SystemCoreClock / 1000000) * 1000;
        do
        {
            sysTickVal = SYSTICK_CURRENT_VALUE_REG;
            u32TimeNowUs = sysTickLoad - sysTickVal;
            if (u32TimeNowUs < u32TimeCacheUs)
            {
                if (u32MsIncreaseFlag)
                {
                    u32MsIncreaseFlag = false;
                    u32TimeNowMs += (SystemCoreClock / 1000000) * 1000;
                }
            }
            else
            {
                u32MsIncreaseFlag = true;
            }
        } while (u32TimeNowUs + u32TimeNowMs - u32TimeCacheUs <= u32nms);

    }
    else
    {
        while (!osTaskIsTimeOutMs(&pTimer, u32nms));
    }
}