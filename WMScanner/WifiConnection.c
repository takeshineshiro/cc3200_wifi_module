#include "main.h"
#include "simplelink.h"
#include "common.h"
#include "ButtonControl.h"

#include "ProbeConf.h"
#include "SocketConnection.h"

//	--------------------------------------------------------
//	局部的连接相关参数
//	连接状态参数等
unsigned char  g_ulStatus = 0;
unsigned long  g_ulStaIp = 0;
unsigned long  g_ulPingPacketsRecv = 0;
unsigned long  g_uiGatewayIP = 0;

//	探头是否可以运行
bool g_bRunable;	

//
// Values for below macros shall be modified for setting the 'Ping' properties
//
#define PING_INTERVAL       1000    /* In msecs */
#define PING_TIMEOUT        3000    /* In msecs */
#define PING_PKT_SIZE       4      /* In bytes */
#define NO_OF_ATTEMPTS      3
#define PING_FLAG           0

// Application specific status/error codes
typedef enum{
    // Choosing this number to avoid overlap w/ host-driver's error codes 
    LAN_CONNECTION_FAILED = -0x7D0,        
    CLIENT_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = CLIENT_CONNECTION_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;


static void WaitUntilSave() {
	while (!g_bSimpleLinkSave) {
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}


//****************************************************************************
//
//!    \brief This function initializes the application variables
//!
//!    \param[in]  None
//!
//!    \return     None
//
//****************************************************************************
static void InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulStaIp = 0;
    g_ulPingPacketsRecv = 0;
    g_uiGatewayIP = 0;
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

	WaitUntilSave();
    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    // If the device is not in station-mode, try configuring it in station-mode 
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event 
            // before doing anything 
            while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
            }
        }

		WaitUntilSave();
        // Switch to STA role and restart 
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

		WaitUntilSave();
        lRetVal = sl_Stop(0xFF);
        ASSERT_ON_ERROR(lRetVal);

		WaitUntilSave();
        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Check if the device is in station again 
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in STA-mode 
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }
    
	WaitUntilSave();
    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, 
                                &ucConfigLen, (unsigned char *)(&ver));
    ASSERT_ON_ERROR(lRetVal);
    
    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

	WaitUntilSave();
    // Set connection policy to Auto + SmartConfig 
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, 
                                SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);
    

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
	WaitUntilSave();
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
        }
    }

	WaitUntilSave();
    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, 
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);
	
	WaitUntilSave();
	//	设置国家码
	char szCountryCode[] = "EU";
	sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE,
				  2,szCountryCode);
	
	WaitUntilSave();
	//	Set Channel to g_ucChannel
	unsigned char val = g_ucChannel;
	sl_WlanSet(SL_WLAN_CFG_AP_ID,WLAN_AP_OPT_CHANNEL,1,(unsigned char*)&val);
	
	WaitUntilSave();
    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();
    
    return lRetVal; // Success
}



//****************************************************************************
//
//! Confgiures the mode in which the device will work
//!
//! \param iMode is the current mode of the device
//!
//! This function
//!    1. prompt user for desired configuration and accordingly configure the
//!          networking mode(STA or AP).
//!       2. also give the user the option to configure the ssid name in case of
//!       AP mode.
//!
//! \return sl_start return value(int).
//
//****************************************************************************
static int ConfigureMode(int iMode)
{
    //char    pcSsidName[33] = "WLArray";
	//	判断探头序列号是否有效
	bool bIsSNValid = false;
	if (	(strlen(g_szSN) == 10) 
		&&	(	( (g_szSN[0] == 'W') && (g_szSN[1] == 'S') && (g_szSN[2] == 'P') )	//	无线凸阵 人用 SS-1
			)
		&& 	(g_szSN[3]>='A' && g_szSN[3]<='Z')
		&& 	(g_szSN[4]>='A' && g_szSN[4]<='Z')
		&& 	(g_szSN[5]>='A' && g_szSN[5]<='Z')
		&& 	(g_szSN[6]>='A' && g_szSN[6]<='Z')
		&&	(g_szSN[7]>='0' && g_szSN[7]<='9')
		&&	(g_szSN[8]>='0' && g_szSN[8]<='9')
		&&	(g_szSN[9]>='0' && g_szSN[9]<='9')
		)
	{
		bIsSNValid = true;
	}

	//	根据探头序列号确定SSID
	char    pcSsidName[33] = {0};
	if (!bIsSNValid) {
		pcSsidName[0] = 'W';
		pcSsidName[1] = 'S';
		pcSsidName[2] = 'S';
		pcSsidName[3] = 'c';
		pcSsidName[4] = 'a';
		pcSsidName[5] = 'n';
		pcSsidName[6] = 'n';
		pcSsidName[7] = 'e';
		pcSsidName[8] = 'r';
	}
	else {
		pcSsidName[0] = 'S';
		pcSsidName[1] = 'S';
		pcSsidName[2] = '-';
		pcSsidName[3] = '1';
		pcSsidName[4] = ' ';
		if (g_ucSalesCode == SALES_CODE_CHINA) {
			pcSsidName[5] = 'C';
		} else {
			pcSsidName[5] = 'G';
		}
		pcSsidName[6] = 'A' + g_ucChannel - 1;
		//	SN的编号部分
		for (int i=0;i<7;i++) {
			pcSsidName[7+i] = g_szSN[3+i];
		}
	}
	
	//	 根据探头序列号确定PWD
	char 	Pswd[16]={0};
	if (!bIsSNValid) {
		Pswd[0] = 's';
		Pswd[1] = 'o';
		Pswd[2] = 'n';
		Pswd[3] = 'o';
		Pswd[4] = 'p';
		Pswd[5] = 't';
		Pswd[6] = 'e';
		Pswd[7] = 'k';
	}
	else {
		for (int i=0;i<10;i++) {
			Pswd[i] = g_szSN[i];
			if (Pswd[i]>='A' && Pswd[i]<='Z') {	//	使用小写字母
				Pswd[i] -= 'A' - 'a';
			}
		}
	}
	
	WaitUntilSave();
    long   lRetVal = -1;
    lRetVal = sl_WlanSetMode(ROLE_AP);
    ASSERT_ON_ERROR(lRetVal);

	WaitUntilSave();
    lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, strlen(pcSsidName),
                            (unsigned char*)pcSsidName);
    ASSERT(lRetVal>=0);
	
	WaitUntilSave();
	unsigned char val = SL_SEC_TYPE_WPA;
	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID,WLAN_AP_OPT_SECURITY_TYPE,
			 		  1,(unsigned char*)&val);
	ASSERT(lRetVal>=0);
	
	WaitUntilSave();
	//char Pswd[]="bjsonoptek";
	int len = strlen(Pswd);
	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD, len, (unsigned char*)Pswd);
	ASSERT(lRetVal>=0);
	
	

    TRACE("Device is configured in AP mode\n\r");

	WaitUntilSave();
    /* Restart Network processor */
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);

    // reset status bits
    CLR_STATUS_BIT_ALL(g_ulStatus);

	WaitUntilSave();
    return sl_Start(NULL,NULL,NULL);
}


//****************************************************************************
//
//!    \brief device will try to ping the machine that has just connected to the
//!           device.
//!
//!    \param  ulIpAddr is the ip address of the station which has connected to
//!            device
//!
//!    \return 0 if ping is successful, -1 for error
//
//****************************************************************************
static int PingTest(unsigned long ulIpAddr)
{  
    signed long           lRetVal = -1;
    SlPingStartCommand_t PingParams;
    SlPingReport_t PingReport;
    PingParams.PingIntervalTime = PING_INTERVAL;
    PingParams.PingSize = PING_PKT_SIZE;
    PingParams.PingRequestTimeout = PING_TIMEOUT;
    PingParams.TotalNumberOfAttempts = NO_OF_ATTEMPTS;
    PingParams.Flags = PING_FLAG;
    PingParams.Ip = ulIpAddr; /* Cleint's ip address */
    
    UART_PRINT("Running Ping Test...\n\r");
    /* Check for LAN connection */
    lRetVal = sl_NetAppPingStart((SlPingStartCommand_t*)&PingParams, SL_AF_INET,
                            (SlPingReport_t*)&PingReport, NULL);
    ASSERT_ON_ERROR(lRetVal);

    g_ulPingPacketsRecv = PingReport.PacketsReceived;

    if (g_ulPingPacketsRecv > 0 && g_ulPingPacketsRecv <= NO_OF_ATTEMPTS)
    {
      // LAN connection is successful
      UART_PRINT("Ping Test successful\n\r");
    }
    else
    {
        // Problem with LAN connection
        ASSERT_ON_ERROR(LAN_CONNECTION_FAILED);
    }

    return SUCCESS;
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************

//*****************************************************************************
//
//! On Successful completion of Wlan Connect, This function triggers Connection
//! status to be set. 
//!
//! \param  pSlWlanEvent pointer indicating Event type
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pSlWlanEvent)
{
    switch(pSlWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
			TRACE("SL_WLAN_CONNECT_EVENT \n\r");
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'-Applications
            // can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //
            //
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
			TRACE("SL_WLAN_DISCONNECT_EVENT \n\r");
			
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pSlWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_USER_INITIATED_DISCONNECTION
            if(SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                TRACE("Device disconnected from the AP on application's "
                            "request \n\r");
            }
            else
            {
                UART_PRINT("Device disconnected from the AP on an ERROR..!! \n\r");
            }

        }
        break;

        case SL_WLAN_STA_CONNECTED_EVENT:
        {
			TRACE("SL_WLAN_STA_CONNECTED_EVENT \n\r");
            // when device is in AP mode and any client connects to device cc3xxx
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected client (like SSID, MAC etc) will be
            // available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModeStaConnected;
            //

        }
        break;

        case SL_WLAN_STA_DISCONNECTED_EVENT:
        {
			TRACE("SL_WLAN_STA_DISCONNECTED_EVENT \n\r");
			
            // when client disconnects from device (AP)
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

            //
            // Information about the connected client (like SSID, MAC etc) will
            // be available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModestaDisconnected;
            //            
        }
        break;

        default:
        {
            TRACE("[WLAN EVENT] Unexpected event \n\r");
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
        {
			TRACE("SL_NETAPP_IPV4_IPACQUIRED_EVENT \n\r");
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
        }
        break;
        
        case SL_NETAPP_IP_LEASED_EVENT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
        
            g_ulStaIp = (pNetAppEvent)->EventData.ipLeased.ip_address;
            
            TRACE("[NETAPP EVENT] IP Leased to Client: IP=%d.%d.%d.%d , ",
                        SL_IPV4_BYTE(g_ulStaIp,3), SL_IPV4_BYTE(g_ulStaIp,2),
                        SL_IPV4_BYTE(g_ulStaIp,1), SL_IPV4_BYTE(g_ulStaIp,0));
        }
        break;
        
        case SL_NETAPP_IP_RELEASED_EVENT:
        {
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

            TRACE("[NETAPP EVENT] IP Leased to Client: IP=%d.%d.%d.%d , ",
                        SL_IPV4_BYTE(g_ulStaIp,3), SL_IPV4_BYTE(g_ulStaIp,2),
                        SL_IPV4_BYTE(g_ulStaIp,1), SL_IPV4_BYTE(g_ulStaIp,0));

        }
        break;

        default:
        {
            TRACE("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Event);
        }
        break;
    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    TRACE("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->EventData.deviceEvent.status, 
               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    //
    // This application doesn't work w/ socket - Events are not expected
    //
	TRACE("SimpleLinkSockEventHandler\n\r");
	switch( pSock->Event )
    {
        case SL_SOCKET_TX_FAILED_EVENT:
			
            switch( pSock->EventData.status )
            {
                case SL_ECLOSE: 
                    TRACE("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n", 
                                pSock->EventData.sd);
                    break;
                default: 
                    TRACE("[SOCK ERROR] - TX FAILED : socket %d , reason"
                                "(%d) \n\n",
                                pSock->EventData.sd, pSock->EventData.status);
            }
            break;

        default:
          TRACE("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
    }
	
	CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
	CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
			
}

//*****************************************************************************
//
//! \brief This function handles ping report events
//!
//! \param[in]     pPingReport - Ping report statistics
//!
//! \return None
//
//****************************************************************************
void SimpleLinkPingReport(SlPingReport_t *pPingReport)
{
	TRACE("SimpleLinkPingReport \n\r");
    SET_STATUS_BIT(g_ulStatus, STATUS_BIT_PING_DONE);
    g_ulPingPacketsRecv = pPingReport->PacketsReceived;
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************



//
//	AP 连接线程
//
/*
void WlanAPModeThread(void* pvParameters)
{
	long xReturn;
	int nDelayCnt;
	long retVal;
	portBASE_TYPE pdRet;
	
	//	创建用于线程同步的信号量
	g_mutexCtrlSrvExit = xSemaphoreCreateMutex();
	ASSERT(g_mutexCtrlSrvExit != NULL);

	g_mutexStatSrvExit = xSemaphoreCreateMutex();
	ASSERT(g_mutexStatSrvExit != NULL);
	
	g_mutexTranSrvExit = xSemaphoreCreateMutex();
	ASSERT(g_mutexTranSrvExit != NULL);
	
_lable_restart:
	
	TRACE("Start Take Semaphore CTRL,STAT,TRAN\r\n");

	pdRet = xSemaphoreTake(g_mutexCtrlSrvExit,100/portTICK_RATE_MS);
	ASSERT(pdRet == pdPASS);
	
	pdRet = xSemaphoreTake(g_mutexStatSrvExit,100/portTICK_RATE_MS);
	ASSERT(pdRet == pdPASS);
	
	pdRet = xSemaphoreTake(g_mutexTranSrvExit,100/portTICK_RATE_MS);
	ASSERT(pdRet == pdPASS);
	
	TRACE("End Take Semaphore CTRL,STAT,TRAN\r\n");
	
	retVal = -1;
	InitializeAppVariables();
	
	retVal = ConfigureSimpleLinkToDefaultState();
	if (retVal < 0) {
		ASSERT(false);
	}
	//SampleInit();
	//for(;;) vTaskDelay(100/portTICK_RATE_MS);
	
	vTaskDelay(100/portTICK_RATE_MS);
	
	retVal = sl_Start(NULL,NULL,NULL);
	if (retVal < 0) {
		ASSERT(false);
	}
	if (retVal != ROLE_AP) {
		if (ConfigureMode(retVal) != ROLE_AP)
		{
			sl_Stop(SL_STOP_TIMEOUT);
			ASSERT(false);
		}
	}
	
	//for(;;) vTaskDelay(100/portTICK_RATE_MS);
	
	
_lable_reconnect:
	//	looping till ip is acquired
	nDelayCnt = 0;
	while(!IS_IP_ACQUIRED(g_ulStatus)) {
		vTaskDelay(100/portTICK_RATE_MS);
		nDelayCnt++;
		if( nDelayCnt > 20)
			goto _lable_connect_failed;
	}
	TRACE("APThread: IP_ACQUIRED.\r\n");
	
	//	等待连接建立
	nDelayCnt = 0;
	while(!IS_IP_LEASED(g_ulStatus)) {
		vTaskDelay(100/portTICK_RATE_MS);
		if (IS_CONNECTED(g_ulStatus))
		{	nDelayCnt++;
			if (nDelayCnt > 200)
				goto _lable_connect_failed;
		}
		else {
			nDelayCnt = 0;
		}
	}
	TRACE("APThread: IP_LEASED.\r\n");
	
	//	创建控制服务线程
	xReturn	= xTaskCreate(	DataTransServerThread,
							"Data Trans Server Thread",
							512,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);

	xReturn	= xTaskCreate(	ControlServerThread,
							"Control Server Thread",
							128,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);
	
	xReturn	= xTaskCreate(	StatusServerThread,
							"Status Server Thread",
							256,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);
	
	//	等待连接断开
	while(IS_IP_LEASED(g_ulStatus)) {
		vTaskDelay(200/portTICK_RATE_MS);
	}
	
	//TRACE("Start Give Semaphore CTRL,STAT,TRAN\r\n");
_lable_connect_failed:
	
	xSemaphoreGive(g_mutexCtrlSrvExit);
	vTaskDelay(100/portTICK_RATE_MS);
	xSemaphoreGive(g_mutexStatSrvExit);
	vTaskDelay(100/portTICK_RATE_MS);
	xSemaphoreGive(g_mutexTranSrvExit);
	vTaskDelay(100/portTICK_RATE_MS);
	
	//TRACE("End Give Semaphore CTRL,STAT,TRAN\r\n");
	
	TRACE("Stop Wlan\r\n");
	sl_WlanSetMode(ROLE_STA);
	sl_Stop(SL_STOP_TIMEOUT);
	TRACE("Stop Wlan\r\n");
	
	
	InitializeAppVariables();
	vTaskDelay(200/portTICK_RATE_MS);
	
	TRACE("goto _lable_restart\r\n");
	goto _lable_restart;
	
	vTaskDelete(NULL);
	for(;;);
}
*/

void WifiConnectionThread( void *pvParameters )
{   
	int iTestResult;// = 0;
    unsigned char ucDHCP;
    long lRetVal;// = -1;
_label_wifi_reconnect:
	g_bRunable = false;
	
	iTestResult = 0;
	lRetVal = -1;
	
	WaitUntilSave();
    InitializeAppVariables();
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
            TRACE("Failed to configure the device in its default state \n\r");

        ASSERT(false);
    }

    TRACE("Device is configured in default state \n\r");

    //
    // Asumption is that the device is configured in station mode already
    // and it is in its default state
    //
	WaitUntilSave();
    lRetVal = sl_Start(NULL,NULL,NULL);
    if (lRetVal < 0)
    {
        TRACE("Failed to start the device \n\r");
        ASSERT(false);
    }

    TRACE("Device started as STATION \n\r");
    
    //
    // Configure the networking mode and ssid name(for AP mode)
    //
    if(lRetVal != ROLE_AP)
    {
		WaitUntilSave();
        if(ConfigureMode(lRetVal) != ROLE_AP)
        {
            TRACE("Unable to set AP mode, exiting Application...\n\r");
            sl_Stop(SL_STOP_TIMEOUT);
            ASSERT(false);
        }
    }

	g_bRunable = true;
    while(!IS_IP_ACQUIRED(g_ulStatus))
    {
		//looping till ip is acquired
		vTaskDelay(100/portTICK_PERIOD_MS);
		
    }
	g_bRunable = false;

    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    SlNetCfgIpV4Args_t ipV4 = {0};

    // get network configuration
	WaitUntilSave();
    lRetVal = sl_NetCfgGet(SL_IPV4_AP_P2P_GO_GET_INFO,&ucDHCP,&len,
                            (unsigned char *)&ipV4);
    if (lRetVal < 0)
    {
        TRACE("Failed to get network configuration \n\r");
        ASSERT(false);
    }
    
    TRACE("Connect a client to Device\n\r");
	
	g_bRunable = true;
	int nLedCnt=0;
    while(!IS_IP_LEASED(g_ulStatus))
    {
      	//wating for the client to connect
		vTaskDelay(100/portTICK_PERIOD_MS);
		nLedCnt = (nLedCnt + 1) % 10;
		switch (nLedCnt) {
		case 0:
			LinkLedOn(false);
			break;
		case 5:
			LinkLedOn(true);
			break;
		default:
			break;
		}
    }
	g_bRunable = false;
    TRACE("Client is connected to Device\n\r");

	/*
    iTestResult = PingTest(g_ulStaIp);
    if(iTestResult < 0)
    {
        TRACE("Ping to client failed \n\r");
    }
	*/
	
	//
	//	...	启动服务器线程 ...
	//
	long xReturn;
	WaitUntilSave();
	xReturn	= xTaskCreate(	DataTransServerThread,
							"Data Trans Server Thread",
							1024,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);

	xReturn	= xTaskCreate(	ControlServerThread,
							"Control Server Thread",
							512,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);
	
	xReturn	= xTaskCreate(	StatusServerThread,
							"Status Server Thread",
							512,
							NULL,
							2,
							NULL);
	ASSERT(xReturn >= 0);
	

	//
	//	检测连接是否还在，如果连接还在则继续等待
	//
	g_bRunable = true;
	while(IS_IP_LEASED(g_ulStatus)) {
		vTaskDelay(250/portTICK_PERIOD_MS);
	}
	g_bRunable = false;
	LinkLedOn(false);
	//
	//	...	退出服务器线程 ...
	//

	g_bRunable = false;
	WaitUntilSave();
    // revert to STA mode
    lRetVal = sl_WlanSetMode(ROLE_STA);
    ASSERT(lRetVal >=0);
	
	WaitUntilSave();
    // Switch off Network processor
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
	g_bRunable = true;
	
	//	等待1s后，重新启动Wifi连接线程
	vTaskDelay(1000/portTICK_PERIOD_MS);
	goto _label_wifi_reconnect;
	
	for(;;)
		vTaskDelay(10000/portTICK_PERIOD_MS);
}


