#include "main.h"

#include "SampleControl.h"
#include "ButtonControl.h"

//---------------------------------------
//
//	全局变量
//

//	运行状态
bool 	g_bRunning;

//	虚拟霍尔信号计数器
int 	g_nVirHallCnt;


//	---------------------------------------
//	全局函数
//	---------------------------------------
void MotorInit()
{	
	MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2,PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_TIMERA2);
	MAP_TimerConfigure(TIMERA2_BASE,TIMER_CFG_PERIODIC);
	MAP_TimerPrescaleSet(TIMERA2_BASE,TIMER_A,0);
	
	MAP_TimerIntEnable(TIMERA2_BASE,TIMER_TIMA_TIMEOUT);
	//	Virtual Hall Int
	osi_InterruptRegister(INT_TIMERA2A, HallSyncHandler, 32);
}

void MotorRun(bool bGoRunning)
{
	if (bGoRunning) {
		g_nVirHallCnt = 0;
		MAP_TimerLoadSet(TIMERA2_BASE,TIMER_A,80*1000000/(8*10));		//	最大12 -- 45000
		MAP_TimerEnable(TIMERA2_BASE,TIMER_TIMA_TIMEOUT);
	}
	else {
		MAP_TimerDisable(TIMERA2_BASE,TIMER_TIMA_TIMEOUT);
	}
	
	//	复位待机定时器
	ResetIdleTick();
}

