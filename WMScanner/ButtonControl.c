
#include "main.h"

//	电机控制接口
#include "MotorControl.h"
#include "WifiConnection.h"

#include "ProbeConf.h"

//	Flash是否处于电源安全的状态，可以操作SimpleLink
bool g_bSimpleLinkSave;

static unsigned long 	s_ulIdleTick = 0;

void ResetIdleTick()
{
	s_ulIdleTick = 0;	
}

#define BTN_NOACTION	0
#define BTN_PUSHDOWN	1
#define BTN_POPUP		2

#define BTN_UP			0
#define BTN_DOWN		1
#define BTN_UNKNOWN 	-1

static int ButtonCheck();
static void TurnOff();
static bool IsCharging();
static unsigned char s_nStatOut = BTN_UNKNOWN;
void ButtonGuardThread(void* pvParameters)
{
	unsigned long ulTick = 0;
	s_ulIdleTick = 0;
	unsigned long ulLongKeepOff = 0;
	
	unsigned long ulChrgTick = 0;
	bool bEnableRunning = true;
	while(1) {
		ulTick++;
		
		//	按钮状态检测
		int nBtnAction = ButtonCheck();
		if (nBtnAction != BTN_NOACTION) {
			s_ulIdleTick = 0;
			
			if ( (nBtnAction==BTN_PUSHDOWN) && g_bRunning ) {
				g_bRunning = false;
				MotorRun(g_bRunning);
				bEnableRunning = false;
			}
			else if ( (nBtnAction==BTN_POPUP) && !g_bRunning ) {
				if (!bEnableRunning) {
					bEnableRunning = true;
				}
				else {
					if (g_bRunable) {	//	如果不在SimpleLink操作阶段则不能运行探头
						g_bRunning = true;
						MotorRun(g_bRunning);
					}
				}
			}
			
			ulLongKeepOff = 0;	//	长按关机计数
		} else {	//	长按关机状态下关闭电源
			if ( s_nStatOut == BTN_UP) {
				ulLongKeepOff = 0;
			} else {
				++ulLongKeepOff;
				if (ulLongKeepOff/100 > 8) {	//	长按8秒关闭电源
					MotorRun(false);
					TurnOff();
				}
			}
			
			//	注：
			//		长按关机是为了避免运输工程中，如果探头的按钮受到挤压而开机；
			//	在这种状态下，按钮控制芯片并不会在一定时间后主动关闭电源，从而
			//	会导致电源耗尽或者其他潜在风险。
			//		故而解决的方法是检测按钮是否长时间（8S）处于未知或者按下
			//	状态，一旦检测到则关闭电源。
			//												吴文斌
			//											2015 - 3 - 30
		}
		
		//	充电状态检测
		if (IsCharging()) {
			ulChrgTick++;
			if (ulChrgTick > 20) {			
				TRACE("Begin Charging...\n\r");	
				g_bRunning = false;
				MotorRun(g_bRunning);
				TurnOff();
			}
		}
		else {
			ulChrgTick = 0;
		}
		
		
		//	待机时间检测
		++s_ulIdleTick;
		if (	(s_ulIdleTick/100/60 > 10)	//	连续运行10分钟则停止运行
			&&	g_bRunning )
		{
			TRACE("10min to Stop Running...\n\r");
			g_bRunning = !g_bRunning;
			MotorRun(g_bRunning);
		}
		if ( s_ulIdleTick/100/60 > 15) {		//	待机时间超过15分钟则关闭电源
			TRACE("15min to Power Off...\n\r");
			TurnOff();
		}
		
		
		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}

void LinkLedOn(bool bOn)
{
	if (bOn) {
		GPIOPinWrite(GPIOA2_BASE,0X40,0x40);
	}
	else {
		GPIOPinWrite(GPIOA2_BASE,0X40,0x00);
	}
}



//	=================================
//	静态函数
//	=================================
//
//	Button监控函数,在主循环中被轮询
//


//static unsigned char s_nStatOut = BTN_UNKNOWN;
static unsigned char s_nStatPre = BTN_UNKNOWN;
static unsigned long s_nStatCnt = 0;
static bool s_bBtnRdy = false;	//	探头是否已经启动，用于去除探头启动时探头抬起导致探头运行。
static int ButtonCheck() 
{
	//	读取按钮状态
	unsigned char nStatCur = BTN_UNKNOWN;
	
	long nBtnStatRd = GPIOPinRead(GPIOA0_BASE,0x20);
	if (nBtnStatRd & 0x0020) {
		nStatCur = BTN_DOWN;
		
		//	SimpleLink 操作安全性
		g_bSimpleLinkSave = false;
	}
	else {
		nStatCur = BTN_UP;
		
		g_bSimpleLinkSave = true;
	}
	
	//	对按钮状态作简单的滤波
	int nRet = BTN_NOACTION;
	if (nStatCur != s_nStatPre) {
		s_nStatPre = nStatCur;
		s_nStatCnt = 0;
	}
	else {
		if (s_nStatCnt < 5)
			s_nStatCnt++;
		if (s_nStatCnt == 3) {
			unsigned char nStatOut = s_nStatPre;
			if (nStatOut != s_nStatOut) {
				if (	(s_nStatOut == BTN_UP)
					&&	(nStatOut == BTN_DOWN)
					)
				{
					nRet = BTN_PUSHDOWN;
					s_bBtnRdy = true;	//	防止开机时首次抬起按钮导致探头运行
				}
				else if (	(s_nStatOut == BTN_DOWN)
						&&	(nStatOut == BTN_UP)
						)
				{
					if (s_bBtnRdy)		//	防止开机时首次抬起按钮导致探头运行
						nRet = BTN_POPUP;
				}
				s_nStatOut = nStatOut;
			}
		}
	}
	return nRet;
}

//	关闭电源
static void TurnOff()
{
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x00);	
	
	GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x10,0x00);
}



//	充电状态检测
static bool IsCharging()
{
	long nChrgStatRd = GPIOPinRead(GPIOA1_BASE,0x02);
	if (nChrgStatRd & 0x02) {
		return false;
	}
	return true;
}



