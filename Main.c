/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"

#include "Main.h"
#include "config.h"

// DEBUG options

#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"
#include "SMBus.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
//#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
//#define ToCoNet_USE_MOD_NBSCAN_SLAVE

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"
#include "Adafruit_GPS_I2C.h"
/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef struct
{
    // MAC
    uint8 u8channel;
    uint16 u16addr;

    // LED Counter
    uint32 u32LedCt;

    // シーケンス番号
    uint32 u32Seq;

    // スリープカウンタ
    uint8 u8SleepCt;
} tsAppData;


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);

void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt);
static void vHandleSerialInput(void);

int16 i16TransmitPingMessage(uint8 *pMsg);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
/* Version/build information. This is not used in the application unless we
   are in serial debug mode. However the 'used' attribute ensures it is
   present in all binary files, allowing easy identifaction... */

/* Local data used by the tag during operation */
static tsAppData sAppData;

PUBLIC tsFILE sSerStream;
tsSerialPortSetup sSerPort;

// Wakeup port
const uint32 u32DioPortWakeUp = 1UL << 7; // UART Rx Port

// センサー状況
#define KICKED_SENSOR_SHT21_TEMP 1
#define KICKED_SENSOR_SHT21_HUMD 2
#define KICKED_SENSOR_BH1715 3
#define KICKED_SENSOR_MPL115 4
#define KICKED_SENSOR_ADT7410 5
#define KICKED_SENSOR_LIS3DH 6
#define KICKED_SENSOR_ADXL345 7
#define KICKED_SENSOR_GPS 8
    
static uint8 u8KickedSensor; //!< 開始されたセンサーの種類
static uint32 u32KickedTimeStamp; //! 開始されたタイムスタンプ

#define GPS_MAX_I2C_TRANSFER  32
extern uint8 data[GPS_MAX_I2C_TRANSFER];
    
/****************************************************************************
 *
 * NAME: AppColdStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbAppColdStart(bool_t bAfterAhiInit)
{
	//static uint8 u8WkState;
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.

		// Register modules
		ToCoNet_REG_MOD_ALL();

	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// clear application context
		memset (&sAppData, 0x00, sizeof(sAppData));
		sAppData.u8channel = CHANNEL;

		// ToCoNet configuration
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;

		sToCoNet_AppContext.bRxOnIdle = TRUE;

		// others
		SPRINTF_vInit128();

		// Register
		ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// Others
		vInitHardware(FALSE);

		// MAC start
		ToCoNet_vMacStart();
	}
}

/****************************************************************************
 *
 * NAME: AppWarmStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static bool_t bWakeupByButton;

void cbAppWarmStart(bool_t bAfterAhiInit)
{
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.
		bWakeupByButton = FALSE;

		if(u8AHI_WakeTimerFiredStatus()) {
			// wake up timer
		} else
		if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
			bWakeupByButton = TRUE;
		} else {
			bWakeupByButton = FALSE;
		}
	} else {
		// Initialize hardware
		vInitHardware(TRUE);

		// MAC start
		ToCoNet_vMacStart();
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vMain
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbToCoNet_vMain(void)
{
	/* handle uart input */
	vHandleSerialInput();
}

/****************************************************************************
 *
 * NAME: cbToCoNet_vNwkEvent
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: cbvMcRxHandler
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	int i;
	static uint16 u16seqPrev = 0xFFFF;
	//uint8 *p = pRx->auData;
	SendData receive;
	// GpsData receive;


	// 受け取りがうまく行ってなかった
	// memcpy(receive.gps, pRx->auData, sizeof(receive));
	memcpy(&receive, pRx->auData, sizeof(SendData));
//vfPrintf(&sSerStream, "%d, %d\n", pRx->u8Len, sizeof(SendData));
	 showGPS(&receive);



	// // print coming payload
	// vfPrintf(&sSerStream, LB"[PKT Ad:%04x,Ln:%03d,Seq:%03d,Lq:%03d,Tms:%05d \"",
	// 		pRx->u32SrcAddr,
	// 		pRx->u8Len+4, // Actual payload byte: the network layer uses additional 4 bytes.
	// 		pRx->u8Seq,
	// 		pRx->u8Lqi,
	// 		pRx->u32Tick & 0xFFFF);
	// for (i = 0; i < pRx->u8Len; i++) {
	// 	if (i < 32) {
	// 		sSerStream.bPutChar(sSerStream.u8Device,
	// 				(pRx->auData[i] >= 0x20 && pRx->auData[i] <= 0x7f) ? pRx->auData[i] : '.');
	// 	} else {
	// 		vfPrintf(&sSerStream, "..");
	// 		break;
	// 	}
	// }
	// vfPrintf(&sSerStream, "C\"]");

	// // 打ち返す
	// if (    pRx->u8Seq != u16seqPrev // シーケンス番号による重複チェック
	// 	&& !memcmp(pRx->auData, "PING:", 5) // パケットの先頭は PING: の場合
	// ) {
	// 	u16seqPrev = pRx->u8Seq;
	// 	// transmit Ack back
	// 	tsTxDataApp tsTx;
	// 	memset(&tsTx, 0, sizeof(tsTxDataApp));

	// 	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); //
	// 	tsTx.u32DstAddr = pRx->u32SrcAddr; // 送り返す

	// 	tsTx.bAckReq = TRUE;
	// 	tsTx.u8Retry = 0;
	// 	tsTx.u8CbId = pRx->u8Seq;
	// 	tsTx.u8Seq = pRx->u8Seq;
	// 	tsTx.u8Len = pRx->u8Len;
	// 	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	// 	tsTx.u16DelayMin = 64; // 最小遅延
	// 	tsTx.u16DelayMax = 256; // 最大遅延

	// 	if (tsTx.u8Len > 0) {
	// 		memcpy(tsTx.auData, pRx->auData, tsTx.u8Len);
	// 	}
	// 	tsTx.auData[1] = 'O'; // メッセージを PONG に書き換える

	// 	ToCoNet_bMacTxReq(&tsTx);

	// 	// turn on Led a while
	// 	sAppData.u32LedCt = u32TickCount_ms;

	// 	// ＵＡＲＴに出力
	// 	vfPrintf(&sSerStream, LB "Fire PONG Message to %08x" LB, pRx->u32SrcAddr);
	// } else if (!memcmp(pRx->auData, "PONG:", 5)) {
	// 	// ＵＡＲＴに出力
	// 	vfPrintf(&sSerStream, LB "PONG Message from %08x" LB, pRx->u32SrcAddr);
	// }
}

/****************************************************************************
 *
 * NAME: cbvMcEvTxHandler
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	return;
}

int16 BroadCastGpsData(GpsData* gps) {
	// transmit Ack back
	tsTxDataApp tsTx;
	memset(&tsTx, 0, sizeof(tsTxDataApp));

	sAppData.u32Seq++;

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	tsTx.bAckReq = FALSE;
	tsTx.u8Retry = 0x00; //0x82; // ブロードキャストで都合３回送る
	tsTx.u8CbId = sAppData.u32Seq & 0xFF;
	tsTx.u8Seq = sAppData.u32Seq & 0xFF;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	SendData send;

	// 送信用データのコピー
	send.gps = *gps;
	send.ttl = 3;

	memcpy(tsTx.auData, &send, sizeof(SendData));
	tsTx.u8Len = sizeof(SendData);
	// memcpy(tsTx.auData, gps, sizeof(GpsData));
	// tsTx.u8Len = sizeof(GpsData);

	// 送信
	if (ToCoNet_bMacTxReq(&tsTx)) {
		// LEDの制御
		sAppData.u32LedCt = u32TickCount_ms;
		return tsTx.u8CbId;
	} else {
		return -1;
	}
}
/****************************************************************************
 *
 * NAME: cbToCoNet_vHwEvent
 *
 * DESCRIPTION:
 * Process any hardware events.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap)
{
    int i = 0;
    int j = 0;
    static uint32 timer = 0;


	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		// LED BLINK
		vPortSet_TrueAsLo(PORT_KIT_LED2, u32TickCount_ms & 0x400);

		// LED ON when receive
		if (u32TickCount_ms - sAppData.u32LedCt < 300) {
			vPortSetLo(PORT_KIT_LED1);
		} else {
			vPortSetHi(PORT_KIT_LED1);
		}

	    if(u32TickCount_ms - timer > 100){
	        GpsData gps;
	        if(gps_read(&gps)>0){
	            sprintf(gps.name, "Nihon");
	            showGPS(&gps);
	            BroadCastGpsData(&gps);
	        }
            timer = u32TickCount_ms;
	    }
		break;
	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: cbToCoNet_u8HwInt
 *
 * DESCRIPTION:
 *   called during an interrupt
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 *                  FALSE -  interrupt is not handled, escalated to further
 *                           event call (cbToCoNet_vHwEvent).
 *                  TRUE  -  interrupt is handled, no further call.
 *
 * NOTES:
 *   Do not put a big job here.
 ****************************************************************************/
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	return FALSE;
}

/****************************************************************************
 *
 * NAME: vInitHardware
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vInitHardware(int f_warm_start)
{
	// Serial Initialize
#if 0
	// UART の細かい設定テスト
	tsUartOpt sUartOpt;
	memset(&sUartOpt, 0, sizeof(tsUartOpt));
	sUartOpt.bHwFlowEnabled = FALSE;
	sUartOpt.bParityEnabled = E_AHI_UART_PARITY_ENABLE;
	sUartOpt.u8ParityType = E_AHI_UART_EVEN_PARITY;
	sUartOpt.u8StopBit = E_AHI_UART_2_STOP_BITS;
	sUartOpt.u8WordLen = 7;

	vSerialInit(UART_BAUD, &sUartOpt);
#else
	vSerialInit(UART_BAUD, NULL);
#endif


	ToCoNet_vDebugInit(&sSerStream);
	ToCoNet_vDebugLevel(0);

	/// IOs
	vPortSetLo(PORT_KIT_LED1);
	vPortSetHi(PORT_KIT_LED2);
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);


	// SMBUS
	vSMBusInit();
}

/****************************************************************************
 *
 * NAME: vInitHardware
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[864];
	static uint8 au8SerialRxBuffer[128];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT_SLAVE;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_SLAVE;
}

/****************************************************************************
 *
 * NAME: vHandleSerialInput
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
static void vHandleSerialInput(void)
{
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		//vfPrintf(&sSerStream, "\n\r# [%c] --> ", i16Char);
	    SERIAL_vFlush(sSerStream.u8Device);

		switch(i16Char) {
		case '>': case '.':
			/* channel up */
			sAppData.u8channel++;
			if (sAppData.u8channel > 26) sAppData.u8channel = 11;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case '<': case ',':
			/* channel down */
			sAppData.u8channel--;
			if (sAppData.u8channel < 11) sAppData.u8channel = 26;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case 'd': case 'D':
			_C {
				static uint8 u8DgbLvl;

				u8DgbLvl++;
				if(u8DgbLvl > 5) u8DgbLvl = 0;
				ToCoNet_vDebugLevel(u8DgbLvl);

				vfPrintf(&sSerStream, "set NwkCode debug level to %d.", u8DgbLvl);
			}
			break;

		case 't': // パケット送信してみる
			_C {
			}
			break;

		default:
			break;
		}

		vfPrintf(&sSerStream, LB);
	    SERIAL_vFlush(sSerStream.u8Device);
	}
}

int16 i16TransmitPingMessage(uint8 *pMsg) {
	// transmit Ack back
	tsTxDataApp tsTx;
	memset(&tsTx, 0, sizeof(tsTxDataApp));
	uint8 *q = tsTx.auData;

	sAppData.u32Seq++;

	tsTx.u32SrcAddr = ToCoNet_u32GetSerial(); // 自身のアドレス
	tsTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	tsTx.bAckReq = FALSE;
	tsTx.u8Retry = 0x82; // ブロードキャストで都合３回送る
	tsTx.u8CbId = sAppData.u32Seq & 0xFF;
	tsTx.u8Seq = sAppData.u32Seq & 0xFF;
	tsTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA;

	// SPRINTF でメッセージを作成
	S_OCTET('P');
	S_OCTET('I');
	S_OCTET('N');
	S_OCTET('G');
	S_OCTET(':');
	S_OCTET(' ');

	uint8 u8len = strlen((const char *)pMsg);
	memcpy(q, pMsg, u8len);
	q += u8len;
	tsTx.u8Len = q - tsTx.auData;

	// 送信
	if (ToCoNet_bMacTxReq(&tsTx)) {
		// LEDの制御
		sAppData.u32LedCt = u32TickCount_ms;

		// ＵＡＲＴに出力
		//vfPrintf(&sSerStream, LB "Fire PING Broadcast Message.");

		return tsTx.u8CbId;
	} else {
		return -1;
	}
}

/****************************************************************************
 *
 * NAME: vProcessEvent
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		// ここで UART のメッセージを出力すれば安全である。
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			vfPrintf(&sSerStream, LB "RAMHOLD");
		}
	    if (u32evarg & EVARG_START_UP_WAKEUP_MASK) {
			vfPrintf(&sSerStream, LB "Wake up by %s. SleepCt=%d",
					bWakeupByButton ? "UART PORT" : "WAKE TIMER",
					sAppData.u8SleepCt);
	    } else {
	    	vfPrintf(&sSerStream, "\r\n*** TWELITE NET I2C SAMPLE %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	    	vfPrintf(&sSerStream, "\r\n*** %08x ***", ToCoNet_u32GetSerial());
	    }
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
