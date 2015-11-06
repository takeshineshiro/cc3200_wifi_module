#include "main.h"
#include "SocketConnection.h"
#include "simplelink.h"

#include "common.h"
#include "WifiConnection.h"
#include "ProbeConf.h"

#include "QueueManager.h"

#include "ButtonControl.h"

//
static bool  s_bStatusRestarted = false;		//	状态线程失败
static bool  s_bControlRestarted = false;	//	控制显示是否已经重启
static bool  s_bDataRestarted = false;		//	数据线程是否已经重启

//	配置状态标志
static bool s_bConfing = false;

//=========================================================================================//
//
//	数据传输服务器线程
//
//----------vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv--------//
#define usDataPort		5002
static unsigned char* s_pPack;
static unsigned long  s_nPackLen;
void DataTransServerThread(void* pvParameters)
{
	SlSockAddrIn_t	sAddr;
	SlSockAddrIn_t	sLocalAddr;
	int 			iSockID;
	int             iAddrSize;
	int             iStatus;
	int             iNewSockID;
	long            lNonBlocking;
_label_start:
	lNonBlocking = 1;
	iSockID = -1;
	iNewSockID = -1;
	
	//	如果没有连接则退出线程
	if (!IS_CONNECTED(g_ulStatus)) {
		goto _label_exit;
	}
	
	
	//	filling the TCP server socket address
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = sl_Htons((unsigned short)usDataPort);
	sLocalAddr.sin_addr.s_addr = 0;
	
	//	creating a TCP socket
	iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM,0);
	if (iSockID < 0) {
		TRACE("Creating TCP socket FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	binding the TCP socket to the TCP server address
	iAddrSize = sizeof(SlSockAddrIn_t);
	iStatus = sl_Bind(iSockID,(SlSockAddr_t*)&sLocalAddr,iAddrSize);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Binding the TCP socket to the TCP server address FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}

	//	putting the socket for listening
	iStatus = sl_Listen(iSockID,0);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Putting the socket for listening FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	setting socket option to make the socket as non blocking
	iStatus = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
							SL_SO_NONBLOCKING,
							&lNonBlocking,
							sizeof(lNonBlocking));
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("setting socket option to make the socket as non blocking FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	portENTER_CRITICAL();
	s_bDataRestarted = true;
	portEXIT_CRITICAL();
	
	TRACE("DataTransServerThread is Listenning...\n\r");
	
	//	waiting for an incoming TCP connection
	iNewSockID = SL_EAGAIN;
	while (iNewSockID < 0)
	{
		//	accepts a connection
		iNewSockID = sl_Accept(iSockID,
							   (struct SlSockAddr_t *)&sAddr,
							   (SlSocklen_t*)&iAddrSize);
		if (iNewSockID == SL_EAGAIN) {
			;//vTaskDelay(40/portTICK_RATE_MS);
		}
		else if (iNewSockID < 0){
			TRACE("Accept Socket FAILED in DataTransThread.\n\r");
			vTaskDelay(500/portTICK_PERIOD_MS);
			goto _label_start;;
		}
		
		vTaskDelay(250/portTICK_PERIOD_MS);
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
	}
	
	unsigned char pucStatus[4]= {0};
	pucStatus[0] = 0xFF;	pucStatus[1] = 0x5A;
	unsigned char ucTick = 0;
	unsigned char pucCmd[4] = {0};
	s_pPack = (unsigned char*)pvPortMalloc(4200);
	s_nPackLen = 0;
	unsigned char ucSec = 0;
	unsigned char ucaSec[525];
	portBASE_TYPE retRcv;
	int nSendLen = 0;
	int nSendImgIndex=0;
	int nQueueIndex = 0;
	unsigned char ucFrame=0;
	unsigned char ucaLine[525];
	ucaLine[0] = 0x5A;		ucaLine[1] = 0xA5;
	ucaLine[2] = 0xFF;		ucaLine[3] = 0x00;
	ucaLine[4] = 0x5A;		ucaLine[5] = 0xA5;
	ucaLine[6] = 0xFF;		ucaLine[7] = 0x00;
	ucaLine[522] = 0x00;	ucaLine[523] = 0x00;	ucaLine[524] = 0x00;
	
	
	bool bExit = false;
	portBASE_TYPE pdRet;
	int nsendcount = 0;
	TRACE("Starting Sending data\r\n");
	while(true) {
		nsendcount++;
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
		bool bRestart = false;
		portENTER_CRITICAL();
		bRestart = !s_bDataRestarted;
		portEXIT_CRITICAL();
		if (bRestart) {
			goto _label_restart;
		}
		
		//
		//	发送图像数据
		//
		bool bQueueRcv = false;
		bQueueRcv = LineRecv(&ucaLine[8],514);
	
		if (!bQueueRcv || !g_bRunning) {
			vTaskDelay(10/portTICK_PERIOD_MS);
			TRACE(".");
		}
		else
		{
			//TRACE("> ");
			//	发送
			memcpy(s_pPack+s_nPackLen,ucaLine,525);
			s_nPackLen += 525;
			while (s_nPackLen >= 1400) {
				nsendcount++;
				iStatus = sl_Send(iNewSockID,s_pPack,1400,0);
				nsendcount++;
				if (nsendcount > 100) {
					nsendcount = 0;
					if (!IS_CONNECTED(g_ulStatus)) {
						goto _label_exit;
					}
				}
				if (iStatus<=0) {
					vTaskDelay(1/portTICK_PERIOD_MS);
				}
				else {
					s_nPackLen -= iStatus;
					if (s_nPackLen > 0)
						memmove(s_pPack,s_pPack+iStatus,s_nPackLen);
				}
			}
		}
	}
	TRACE("x");
_label_restart:
	if (iNewSockID >=0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	goto _label_start;
_label_exit:
	if (iNewSockID >= 0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	TRACE("Tran Thread Exit.\r\n");
	vTaskDelete(NULL);
	for(;;);
}
//	数据传输服务器线程
//----------^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^--------//

//=========================================================================================//
//
//	控制服务器线程
//
//----------vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv--------//
#define usControlPort		5001
extern void CommandHandler(unsigned short usCmd);
void ControlServerThread(void* pvParameters)
{
	SlSockAddrIn_t	sAddr;
	SlSockAddrIn_t	sLocalAddr;
	int 			iSockID;
	int             iAddrSize;
	int             iStatus;
	int             iNewSockID;
	long            lNonBlocking = 1;
_label_start:
	lNonBlocking = 1;
	iSockID = -1;
	iNewSockID = -1;
	
	//	如果没有连接则退出线程
	if (!IS_CONNECTED(g_ulStatus)) {
		goto _label_exit;
	}
	
	
	//	filling the TCP server socket address
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = sl_Htons((unsigned short)usControlPort);
	sLocalAddr.sin_addr.s_addr = 0;
	
	//	creating a TCP socket
	iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM,0);
	if (iSockID < 0) {
		TRACE("Creating TCP socket FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	binding the TCP socket to the TCP server address
	iAddrSize = sizeof(SlSockAddrIn_t);
	iStatus = sl_Bind(iSockID,(SlSockAddr_t*)&sLocalAddr,iAddrSize);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Binding the TCP socket to the TCP server address FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}

	//	putting the socket for listening
	iStatus = sl_Listen(iSockID,0);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Putting the socket for listening FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	setting socket option to make the socket as non blocking
	iStatus = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
							SL_SO_NONBLOCKING,
							&lNonBlocking,
							sizeof(lNonBlocking));
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("setting socket option to make the socket as non blocking FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	portENTER_CRITICAL();
	s_bControlRestarted = true;
	portEXIT_CRITICAL();
				
	TRACE("ControlServerThread is Listenning...\n\r");
	
	//	waiting for an incoming TCP connection
	iNewSockID = SL_EAGAIN;
	while (iNewSockID < 0)
	{
		//	accepts a connection
		iNewSockID = sl_Accept(iSockID,
							   (struct SlSockAddr_t *)&sAddr,
							   (SlSocklen_t*)&iAddrSize);
		if (iNewSockID == SL_EAGAIN) {
			;//vTaskDelay(40/portTICK_RATE_MS);
		}
		else if (iNewSockID < 0){
			TRACE("Accept Socket FAILED in DataTransThread.\n\r");
			vTaskDelay(500/portTICK_PERIOD_MS);
			goto _label_start;;
		}
		
		vTaskDelay(250/portTICK_PERIOD_MS);
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
	}
	
	unsigned char pucStatus[4]= {0};
	pucStatus[0] = 0xFF;	pucStatus[1] = 0x5A;
	unsigned char ucTick = 0;
	unsigned char pucCmd[4] = {0};
	unsigned char pucCmdBuf[532+64]={0};
	unsigned long nSize = 0;
	while(true) {
		//
		//	接收控制命令
		//
		iStatus = sl_Recv(iNewSockID,pucCmd,4,0);
		if (iStatus>0) {
			memcpy(pucCmdBuf+nSize,pucCmd,iStatus);
			nSize += iStatus;
			TRACE("==%d\r\n",nSize);
		}
		while (nSize >= 4) {
			
			bool bGetConf = false;
			bool bWithE0 = false;
			//	处理命令
			if (	(pucCmdBuf[0] == 0x5A)	&&	(pucCmdBuf[1] == 0xA5) ) 
			{
				bool bDstRunning = false;
				if (pucCmdBuf[2] & 0x80) {
					bDstRunning = true;
				}
				else {
					bDstRunning = false;
				}
				
				g_ucZoom = pucCmdBuf[2] & 0x03;
				g_ucGain = pucCmdBuf[3] & 0x7F;
				
				nSize -= 4;
				if (nSize > 0)
					memmove(pucCmdBuf,pucCmdBuf+4,nSize);
				
				//TRACE("%d,%x\n\r",g_ucZoom,g_ucGain);
				
				if (bDstRunning != g_bRunning) {
					if (g_bRunable || !bDstRunning) {
						g_bRunning = bDstRunning;
						MotorRun(g_bRunning);
					}
				}
			}
			else if ( (pucCmdBuf[0] == 0x5C)	&&	(pucCmdBuf[1] == 0xC5) )
			{
				if (pucCmdBuf[2]>=1 && pucCmdBuf[2]<=13) {
					g_ucChannel = pucCmdBuf[2];
					StoreToFRam(false,false,false,true,false);
					
					//
					//	用户重新启动设备 ... ...
					//
				}
			}
			else if ( (pucCmdBuf[0] == 0x5B)	&&	(pucCmdBuf[1] == 0xB5) )
			{
				//	启动读取E0数据
				if (pucCmdBuf[2]) {
					s_bConfing = true;
				}
				else {
					s_bConfing = false;
				}
				
				//	写入数据
				if (pucCmdBuf[3]) {
					bGetConf = true;
					if (pucCmdBuf[3] & 0xF0) {
						bWithE0 = true;
					}
				}
				nSize -= 4;
				memmove(pucCmdBuf,pucCmdBuf+4,nSize);
			}
			//	获取配置参数
			if (bGetConf) {
				unsigned char ucReadBuf[4];
				int nTotalLen = 20;
				if (bWithE0)
					nTotalLen = 532;
				while (nSize < nTotalLen) {
					iStatus = sl_Recv(iNewSockID,ucReadBuf,4,0);
					if (iStatus>0) {
						memcpy(pucCmdBuf+nSize,ucReadBuf,iStatus);
						nSize += iStatus;
					}
					else {
						vTaskDelay(10/portTICK_PERIOD_MS);
						if (!IS_CONNECTED(g_ulStatus)) {
							goto _label_exit;
						}
					}
				}
				
				TRACE("---CONF--- CH:%d SC: %x SN:%s\r\n",
					  pucCmdBuf[0],
					  pucCmdBuf[2],
					  (char*)&pucCmdBuf[4]);
				
				g_ucChannel = pucCmdBuf[0];
				g_ucSalesCode = pucCmdBuf[2];
				//	nChannel pucCmdBuf[0-3];
				memcpy(g_szSN,pucCmdBuf+4,16);
				/*
				if (bWithE0)
					memcpy((void*)g_ulE0Delay,pucCmdBuf+20,128*4);
				*/
				if (bWithE0) 
					nSize -= 532;
				else
					nSize -= 20;
				memmove(pucCmdBuf,pucCmdBuf+nTotalLen,nSize);
				
				//	保存数据到铁电中
				StoreToFRam(true,true,true,true,true);
			}
		}
		
		//	控制数据 收发频率为20Hz
		vTaskDelay(50/portTICK_PERIOD_MS);
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
		
		bool bRestart = false;
		portENTER_CRITICAL();
		bRestart = !s_bControlRestarted;
		portEXIT_CRITICAL();
		if (bRestart) {
			goto _label_restart;
		}
	}
	
_label_restart:
	if (iNewSockID >=0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	vTaskDelay(500/portTICK_PERIOD_MS);
	TRACE("ControlServerThread Restarting...\n\r");
	goto _label_start;
_label_exit:
	if (iNewSockID >= 0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	TRACE("Control Thread Exit.\r\n");
	vTaskDelete(NULL);
	for(;;);
}
//	控制服务器线程
//----------^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^--------//






//=========================================================================================//
//
//	状态服务器线程
//
//----------vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv--------//
#define usStatusPort		5003
void StatusServerThread(void* pvParameters)
{	
	SlSockAddrIn_t	sAddr;
	SlSockAddrIn_t	sLocalAddr;
	int 			iSockID;
	int             iAddrSize;
	int             iStatus;
	int             iNewSockID;
	long            lNonBlocking = 1;
	
_label_start:
	lNonBlocking = 1;
	iSockID = -1;
	iNewSockID = -1;
	
	//	如果没有连接则退出线程
	if (!IS_CONNECTED(g_ulStatus)) {
		goto _label_exit;
	}
	
	//	filling the TCP server socket address
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = sl_Htons((unsigned short)usStatusPort);
	sLocalAddr.sin_addr.s_addr = 0;
	
	//	creating a TCP socket
	iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM,0);
	if (iSockID < 0) {
		TRACE("Creating TCP socket FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	binding the TCP socket to the TCP server address
	iAddrSize = sizeof(SlSockAddrIn_t);
	iStatus = sl_Bind(iSockID,(SlSockAddr_t*)&sLocalAddr,iAddrSize);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Binding the TCP socket to the TCP server address FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}

	//	putting the socket for listening
	iStatus = sl_Listen(iSockID,0);
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("Putting the socket for listening FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	//	setting socket option to make the socket as non blocking
	iStatus = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
							SL_SO_NONBLOCKING,
							&lNonBlocking,
							sizeof(lNonBlocking));
	if (iStatus < 0) {
		sl_Close(iSockID);
		TRACE("setting socket option to make the socket as non blocking FAILED in DataTransThread.\n\r");
		vTaskDelay(500/portTICK_PERIOD_MS);
		goto _label_start;
	}
	
	portENTER_CRITICAL();
	s_bStatusRestarted = true;
	portEXIT_CRITICAL();
	
	TRACE("StatusServerThread is Listenning...\n\r");
	
	//	waiting for an incoming TCP connection
	iNewSockID = SL_EAGAIN;
	int nLedCnt=0;
	while (iNewSockID < 0)
	{
		//	accepts a connection
		iNewSockID = sl_Accept(iSockID,
							   (struct SlSockAddr_t *)&sAddr,
							   (SlSocklen_t*)&iAddrSize);
		if (iNewSockID == SL_EAGAIN) {
			;//vTaskDelay(40/portTICK_RATE_MS);
		}
		else if (iNewSockID < 0){
			TRACE("Accept Socket FAILED in DataTransThread.\n\r");
			vTaskDelay(500/portTICK_PERIOD_MS);
			goto _label_start;;
		}
		
		vTaskDelay(100/portTICK_PERIOD_MS);
		LinkLedOn(false);
		vTaskDelay(100/portTICK_PERIOD_MS);
		LinkLedOn(true);
		
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
	}
	
	//----------------------------------------------------------
	//	实际数据传输部分
	//
	unsigned char pucStatus[4]= {0};
	pucStatus[0] = 0x5A;	pucStatus[1] = 0xA5;
	unsigned char ucTick = 0;
	unsigned char pucCmd[4] = {0};
	
	int nFailedIndex = 0;
	int nCnt = 1;
	while(true) {
		//
		//	发送状态信息
		//
		pucStatus[2] = 0x00;//ucTick;
		pucStatus[3] = 0x00;
		if (g_bRunning)
			pucStatus[2] |= 0x80;
		pucStatus[2] |= (g_ucZoom & 0x03);
		pucStatus[3] |= (g_ucGain & 0x7F);
		iStatus = sl_Send(iNewSockID,pucStatus,sizeof(pucStatus),0);
		nCnt = (nCnt+1) % 20;
		if (iStatus < 0) {
			nFailedIndex++;
			TRACE("%d\n\r",nFailedIndex);
			if (nFailedIndex >= 50) {
				portENTER_CRITICAL();
				s_bStatusRestarted = false;
				s_bControlRestarted = false;
				s_bDataRestarted = false;
				portEXIT_CRITICAL();
				goto _label_restart;
			}
		}
		else {
			nFailedIndex = 0;
		}
		
		//	控制数据 收发频率为10Hz
		if (g_bRunning) {
			vTaskDelay(50/portTICK_PERIOD_MS);
			LinkLedOn(false);
			vTaskDelay(50/portTICK_PERIOD_MS);
			LinkLedOn(true);
		}
		else {
			vTaskDelay(100/portTICK_PERIOD_MS);
			LinkLedOn(true);
			
			//	每秒发送一次探头信息数据
			if (nCnt == 0) {
				unsigned char ucPrbInfo[20];
				ucPrbInfo[0] = 0x5B;
				ucPrbInfo[1] = 0xB5;
				ucPrbInfo[2] = g_ucSalesCode;	//	是否有E0信息
				ucPrbInfo[3] = g_ucChannel;	//	nChannel
				if (s_bConfing)
					ucPrbInfo[2] |= 0x80;
				memcpy(&ucPrbInfo[4],g_szSN,16);	//	SN
				//	发送探头配置信息
				sl_Send(iNewSockID,ucPrbInfo,20,0);
				if (s_bConfing)
					sl_Send(iNewSockID,(void*)g_ulE0Delay,sizeof(unsigned long)*128,0);
			}
		}
		
		if (!IS_CONNECTED(g_ulStatus)) {
			goto _label_exit;
		}
	}
	//==========================================================================
	
_label_restart:
	if (iNewSockID >=0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	vTaskDelay(500/portTICK_PERIOD_MS);
	TRACE("StatusServerThread Restarting...\n\r");
	goto _label_start;
_label_exit:
	if (iNewSockID >= 0)
		sl_Close(iNewSockID);
	if (iSockID >= 0)
		sl_Close(iSockID);
	TRACE("Status Thread Exit.\r\n");
	vTaskDelete(NULL);
	for(;;);
}
//	状态服务器线程
//----------^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^--------//
