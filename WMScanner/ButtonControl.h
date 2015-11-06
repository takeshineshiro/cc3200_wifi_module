//	
//	按钮操作控制
//

//	Flash是否处于电源安全的状态，可以操作SimpleLink
extern bool g_bSimpleLinkSave;


extern void ResetIdleTick();

//	按钮状态监视线程
extern void ButtonGuardThread(void* pvParameters);

//	连接指示灯
extern void LinkLedOn(bool bOn);


