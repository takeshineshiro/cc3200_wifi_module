
#include "main.h"

//	������ƽӿ�
#include "MotorControl.h"
#include "WifiConnection.h"

#include "ProbeConf.h"

//	Flash�Ƿ��ڵ�Դ��ȫ��״̬�����Բ���SimpleLink
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
		
		//	��ť״̬���
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
					if (g_bRunable) {	//	�������SimpleLink�����׶���������̽ͷ
						g_bRunning = true;
						MotorRun(g_bRunning);
					}
				}
			}
			
			ulLongKeepOff = 0;	//	�����ػ�����
		} else {	//	�����ػ�״̬�¹رյ�Դ
			if ( s_nStatOut == BTN_UP) {
				ulLongKeepOff = 0;
			} else {
				++ulLongKeepOff;
				if (ulLongKeepOff/100 > 8) {	//	����8��رյ�Դ
					MotorRun(false);
					TurnOff();
				}
			}
			
			//	ע��
			//		�����ػ���Ϊ�˱������乤���У����̽ͷ�İ�ť�ܵ���ѹ��������
			//	������״̬�£���ť����оƬ��������һ��ʱ��������رյ�Դ���Ӷ�
			//	�ᵼ�µ�Դ�ľ���������Ǳ�ڷ��ա�
			//		�ʶ�����ķ����Ǽ�ⰴť�Ƿ�ʱ�䣨8S������δ֪���߰���
			//	״̬��һ����⵽��رյ�Դ��
			//												���ı�
			//											2015 - 3 - 30
		}
		
		//	���״̬���
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
		
		
		//	����ʱ����
		++s_ulIdleTick;
		if (	(s_ulIdleTick/100/60 > 10)	//	��������10������ֹͣ����
			&&	g_bRunning )
		{
			TRACE("10min to Stop Running...\n\r");
			g_bRunning = !g_bRunning;
			MotorRun(g_bRunning);
		}
		if ( s_ulIdleTick/100/60 > 15) {		//	����ʱ�䳬��15������رյ�Դ
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
//	��̬����
//	=================================
//
//	Button��غ���,����ѭ���б���ѯ
//


//static unsigned char s_nStatOut = BTN_UNKNOWN;
static unsigned char s_nStatPre = BTN_UNKNOWN;
static unsigned long s_nStatCnt = 0;
static bool s_bBtnRdy = false;	//	̽ͷ�Ƿ��Ѿ�����������ȥ��̽ͷ����ʱ̽ͷ̧����̽ͷ���С�
static int ButtonCheck() 
{
	//	��ȡ��ť״̬
	unsigned char nStatCur = BTN_UNKNOWN;
	
	long nBtnStatRd = GPIOPinRead(GPIOA0_BASE,0x20);
	if (nBtnStatRd & 0x0020) {
		nStatCur = BTN_DOWN;
		
		//	SimpleLink ������ȫ��
		g_bSimpleLinkSave = false;
	}
	else {
		nStatCur = BTN_UP;
		
		g_bSimpleLinkSave = true;
	}
	
	//	�԰�ť״̬���򵥵��˲�
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
					s_bBtnRdy = true;	//	��ֹ����ʱ�״�̧��ť����̽ͷ����
				}
				else if (	(s_nStatOut == BTN_DOWN)
						&&	(nStatOut == BTN_UP)
						)
				{
					if (s_bBtnRdy)		//	��ֹ����ʱ�״�̧��ť����̽ͷ����
						nRet = BTN_POPUP;
				}
				s_nStatOut = nStatOut;
			}
		}
	}
	return nRet;
}

//	�رյ�Դ
static void TurnOff()
{
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x00);	
	
	GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x10,0x00);
}



//	���״̬���
static bool IsCharging()
{
	long nChrgStatRd = GPIOPinRead(GPIOA1_BASE,0x02);
	if (nChrgStatRd & 0x02) {
		return false;
	}
	return true;
}



