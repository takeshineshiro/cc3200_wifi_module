#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Driverlib includes
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_gpio.h"
#include "hw_timer.h"
#include "hw_mcspi.h"
#include "pin.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "uart.h"
#include "prcm.h"
#include "gpio.h"
#include "timer.h"
#include "spi.h"
#include "utils.h"
#include "udma.h"

// Free-RTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "portmacro.h"
#include "osi.h"

//--------------------------------------
//
//	全局变量
//

//
//	探头是否处于运行状态中
//
extern bool g_bRunning;

//--------------------------------------
//	
//	调试输出的定义
//
#ifndef NOTERM
	#include <stdbool.h>
	#include "uart_if.h"
	extern void assert_failed(bool b,char* pfile,int nline);
	#define ASSERT(b)	assert_failed((b),__FILE__,__LINE__)
	#define TRACE		Error
#else
	#define ASSERT(b)
	#define TRACE
#endif


