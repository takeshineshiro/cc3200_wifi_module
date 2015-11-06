#include "main.h"
#include "QueueManager.h"
#include "FreeRTOS.h"

static QueueHandle_t	s_QueueBuff;	//	缓冲区队列
static QueueHandle_t	s_QueueLine;	//	图像的扫描线队列
static unsigned char*	s_pBuff;
void InitQueue()
{
	s_pBuff = (unsigned char*)pvPortMalloc(QUEUE_LENGTH*ITEM_LENGTH);
	s_QueueBuff = xQueueCreate(QUEUE_LENGTH, sizeof(unsigned char*));
	s_QueueLine = xQueueCreate(QUEUE_LENGTH, sizeof(unsigned char*));
	
	portBASE_TYPE pdRet;
	for (int i=0; i<QUEUE_LENGTH; i++) {
		unsigned char* pLineBuf = s_pBuff + ITEM_LENGTH * (unsigned int)i;
		pdRet = xQueueSendToBack(s_QueueBuff,&pLineBuf,0);
		ASSERT(pdRet == pdPASS);
	}
}

bool LineSend(unsigned char ucFrame,unsigned char ucLine,unsigned char* pLine, unsigned long ulLength)
{
	portBASE_TYPE ptRet;
	unsigned char* pBuf;
	ptRet = xQueueReceive(s_QueueBuff,&pBuf,0);
	if (ptRet != pdPASS)
		return false;
	
	ASSERT(ulLength==512);
	pBuf[0] = ucFrame;
	pBuf[1] = ucLine;
	memcpy(&pBuf[2],pLine,512);
	
	ptRet = xQueueSendToBack(s_QueueLine,&pBuf,0);
	ASSERT(ptRet == pdPASS);
	
	return true;
}

bool ISR_LineSend(unsigned char ucFrame,unsigned char ucLine,unsigned char* pLine, unsigned long ulLength)
{
	portBASE_TYPE ptRet;
	unsigned char* pBuf;
	ptRet = xQueueReceiveFromISR(s_QueueBuff,&pBuf,0);
	if (ptRet != pdPASS)
		return false;
	
	ASSERT(ulLength==512);
	pBuf[0] = ucFrame;
	pBuf[1] = ucLine;
	memcpy(&pBuf[2],pLine,512);
	
	ptRet = xQueueSendToBackFromISR(s_QueueLine,&pBuf,0);
	ASSERT(ptRet == pdPASS);
	
	return true;
}

bool ISR_LineSend_16(unsigned char ucFrame,unsigned char ucLine,unsigned short* pLine, unsigned long ulLength)
{
	portBASE_TYPE ptRet;
	unsigned char* pBuf;
	ptRet = xQueueReceiveFromISR(s_QueueBuff,&pBuf,0);
	if (ptRet != pdPASS)
		return false;
	
	ASSERT(ulLength==512);
	pBuf[0] = ucFrame;
	pBuf[1] = ucLine;
	unsigned char* pDst = &pBuf[2];
	unsigned short* pSrc = pLine;
	for (int i=0;i<ulLength;i++) {
		*pDst = ((*pSrc) & (0x00FF<<7)) >> 7;
		++pDst;
		++pSrc;
	}
	//memcpy(&pBuf[2],pLine,512);
	
	ptRet = xQueueSendToBackFromISR(s_QueueLine,&pBuf,0);
	ASSERT(ptRet == pdPASS);
	
	return true;
}

bool LineRecv(unsigned char* pRecvBuf,unsigned long ulLength)
{
	//ASSERT(ulLength>=ITEM_LENGTH);
	//ASSERT(pRecvBuf != NULL);
	taskDISABLE_INTERRUPTS();
	unsigned char* pLine;
	portBASE_TYPE ptRet = xQueueReceive(s_QueueLine,&pLine,0);
	if (ptRet != pdPASS) {
		taskENABLE_INTERRUPTS();
		return false;
	}
	memcpy(pRecvBuf, pLine, ITEM_LENGTH);
	ptRet = xQueueSendToBack(s_QueueBuff,&pLine,0);
	ASSERT(ptRet == pdPASS);
	taskENABLE_INTERRUPTS();
	
	return true;
}

bool ISR_LineRecv(unsigned char* pRecvBuf,unsigned long ulLength)
{
	ASSERT(ulLength>=ITEM_LENGTH);
	ASSERT(pRecvBuf != NULL);
	
	unsigned char* pLine;
	portBASE_TYPE ptRet = xQueueReceiveFromISR(s_QueueLine,&pLine,0);
	if (ptRet != pdPASS)
		return false;
	memcpy(pRecvBuf, pLine, ITEM_LENGTH);
	ptRet = xQueueSendToBackFromISR(s_QueueBuff,&pLine,0);
	ASSERT(ptRet == pdPASS);
	
	return true;
}


bool BufferCapacity()
{
	unsigned portBASE_TYPE uxBufLen = uxQueueMessagesWaiting(s_QueueBuff);
	if (uxBufLen < 128)
		return false;
	return true;
}

bool ISR_BufferCapacity()
{
	unsigned portBASE_TYPE uxBufLen = uxQueueMessagesWaitingFromISR(s_QueueBuff);
	if (uxBufLen < 128)
		return false;
	return true;
}




