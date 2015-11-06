#include "main.h"

#include "board.h"

#include "ButtonControl.h"

#include "ProbeConf.h"

#include "WifiConnection.h"

#include "QueueManager.h"



//----------------------------------------
//
//	ȫ�ֱ�������
//
QueueHandle_t	g_hImageQueue;


//----------------------------------------

#define SPAWN_TASK_PRIORITY		9
static void InitGlobalParameters();
int main()
{
	//	Ӳ����ʼ��
	HardwareInit();
	
	//	��ʼ��ȫ��״̬
	InitGlobalParameters();
	
	//	��ʼ��̽ͷ����
	InitProbeConf();
	
	InitQueue();
	
	//	����Button�����߳�
	long xReturn;
	xReturn = xTaskCreate(	ButtonGuardThread,
							"Button Guard Thread",
							512,
							NULL,
							2,
							NULL);
	if (!xReturn) 
		ASSERT(false);

	//
	//	Start the SimpleLink Host
	//
	VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
	xReturn = xTaskCreate(	WifiConnectionThread,
						  	"Wifi Connection Thread",
							512,
							NULL,
							2,
							NULL);
	//MotorRun(1);//g_bRunning);
	
	vTaskStartScheduler();
	
	for(;;);
	return 0;
}

static void InitGlobalParameters()
{
	g_bRunning = false;
}


//	����
#ifndef NOTERM
void assert_failed(bool b,char* pfile,int nline)
{
	taskENTER_CRITICAL();
	if (!b) {
		Error("File: %s, Line: %d\r\n",pfile,nline);
		for(;;);
	}
	taskEXIT_CRITICAL();
}
#endif
