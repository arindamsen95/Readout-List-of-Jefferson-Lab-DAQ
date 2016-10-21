#ifndef VTPLIB_H
#define VTPLIB_H
/*----------------------------------------------------------------------------*
 *  Copyright (c) 2016        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Header file for VTP library
 *
 *----------------------------------------------------------------------------*/

#include <stdint.h>

#ifndef ERROR
#define ERROR -1
#endif
#ifndef OK 
#define OK 0
#endif

/* Macros to help with register spacers */
#define MERGE_(a,b)  a##b
#define LABEL_(a) MERGE_(uint32_t vtpblank, a)
#define BLANK LABEL_(__LINE__)

#define VTP_ZYNC_PHYSMEM_BASE 0x43C00000


typedef struct EventBuilder_Struct
{
  /** 0x0000 */ volatile uint32_t LinkCtrl;
  /** 0x0004 */ volatile uint32_t TiCtrl;
  /** 0x0008 */ volatile uint32_t LinkStatus;
  /** 0x000C */ volatile uint32_t TiStatus;
  /** 0x0010 */ volatile uint32_t EbCtrl;
  /** 0x0014 */ BLANK[(0x100-0x14)/4];
} EB_REGS;

#define VTP_EB_LINKCTRL_FIFO_RST    (1<<3)
#define VTP_EB_LINKCTRL_RX_FIFO_RST (1<<2)
#define VTP_EB_LINKCTRL_PLL_RST     (1<<1)
#define VTP_EB_LINKCTRL_RX_RESET    (1<<0)

#define VTP_EB_TICTRL_SYNCEVT_RST (1<<2)
#define VTP_EB_TICTRL_TI_ACK      (1<<1)
#define VTP_EB_TICTRL_TI_BL_REQ   (1<<0)

#define VTP_EB_LINKSTATUS_GCLK_PLL_LOCK     (1<<18)
#define VTP_EB_LINKSTATUS_RX_READY          (1<<16)
#define VTP_EB_LINKSTATUS_RX_ERROR_CNT_MASK 0xFFFF

#define VTP_EB_TISTATUS_SYNC_EVENT   (1<<16)
#define VTP_EB_TISTATUS_NEXT_BL_MASK 0xFF00
#define VTP_EB_TISTATUS_CUR_BL       0x00FF

#define VTP_EB_EBCTRL_BUILD_VTP  (1<<1)
#define VTP_EB_EBCTRL_BUILD_TI   (1<<0)

typedef struct V7Clk_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ BLANK[(0x100-0x8)/4];
} V7CLK_REGS;

#define VTP_V7CLK_CTRL_GCLK_RESET    (1<<0)
#define VTP_V7CLK_STATUS_GCLK_LOCKED (1<<0)

typedef struct SD_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t ScalerLatch;
  /** 0x000C */ BLANK;
  /** 0x0010 */ volatile uint32_t FPAOSel;
  /** 0x0014 */ volatile uint32_t FPBOSel;
  /** 0x0018 */ volatile uint32_t BusySel;
  /** 0x001C */ volatile uint32_t Trig1Sel;
  /** 0x0020 */ volatile uint32_t SyncSel;
  /** 0x0024 */ BLANK[(0x40-0x24)/4];
  /** 0x0040 */ volatile uint32_t FPAOVal;
  /** 0x0044 */ volatile uint32_t FPBOVal;
  /** 0x0048 */ BLANK[(0x60-0x48)/4];
  /** 0x0060 */ volatile uint32_t FPBIStatus;
  /** 0x0064 */ volatile uint32_t Trig1Status;
  /** 0x0068 */ volatile uint32_t Trig2Status;
  /** 0x006C */ volatile uint32_t SyncStatus;
  /** 0x0070 */ BLANK[(0x100-0x70)/4];
} SD_REGS;

#define VTP_SD_CTRL_FPB_OEN  (1<<1)
#define VTP_SD_CTRL_FPB_SEL  (1<<0)

#define VTP_SD_STATUS_FPB_ID_MASK 0x0007

#define VTP_SD_SCALERLATCH_LATCH (1<<0)

#define VTP_SD_BUSYSEL_MASK  0x0003

#define VTP_SD_TRIG1SEL_MASK 0x0003

#define VTP_SD_SYNCSEL_MASK  0x0003

#define VTP_SD_TRIG1_STATUS (1<<0)

#define VTP_SD_TRIG2_STATUS (1<<0)

#define VTP_SD_SYNC_STATUS  (1<<0)

typedef struct FadcDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x20-0x4)/4];
  /** 0x0020 */ volatile uint32_t Latency[16];
  /** 0x0060 */ BLANK[(0x100-0x60)/4];
} FADCDECODER_REGS;

#define VTP_FADCDECODER_CTRL_PP_EN(x)  (1<<(x-1))

#define VTP_FADCDECODER_LATENCY_MASK   0xFFFF

/* Same struct for VXS and QSFP */
typedef struct Serdes_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t DrpCtrl;
  /** 0x000C */ volatile uint32_t DrpStatus;
  /** 0x0010 */ BLANK[(0x100-0x10)/4];
} SERDES_REGS;

#define VTP_SERDES_CTRL_ER_CNT_RST    (1<<9)
#define VTP_SERDES_CTRL_LOOPBACK_MASK 0x01C0
#define VTP_SERDES_CTRL_POWERDOWN     (1<<5)
#define VTP_SERDES_CTRL_RESETL        (1<<4)
#define VTP_SERDES_CTRL_MODESELL      (1<<3)
#define VTP_SERDES_CTRL_LINKSTATUS    (1<<2)
#define VTP_SERDES_CTRL_RESET         (1<<1)
#define VTP_SERDES_CTRL_GT_RESET      (1<<0)

#define VTP_SERDES_STATUS_LINK_RST          (1<<21)
#define VTP_SERDES_STATUS_RX_RST_DONE       (1<<20)
#define VTP_SERDES_STATUS_TX_RST_DONE       (1<<19)
#define VTP_SERDES_STATUS_TX_LOCK           (1<<18)
#define VTP_SERDES_STATUS_SOFT_ERR_CNT_MASK 0xFF00
#define VTP_SERDES_STATUS_CHUP              (1<<6)
#define VTP_SERDES_STATUS_LANE_UP(x)        (1<<(x+2))
#define VTP_SERDES_STATUS_SOFT_ERR          (1<<1)
#define VTP_SERDES_STATUS_HARD_ERR          (1<<0

#define VTP_SERDES_DRP_CTRL_EN3       (1<<29)
#define VTP_SERDES_DRP_CTRL_EN2       (1<<28)
#define VTP_SERDES_DRP_CTRL_EN1       (1<<27)
#define VTP_SERDES_DRP_CTRL_EN0       (1<<26)
#define VTP_SERDES_DRP_CTRL_WE        (1<<25)
#define VTP_SERDES_DRP_CTRL_ADDR_MASK 0x01FF0000
#define VTP_SERDES_DRP_CTRL_DI_MASK   0x0000FFFF

#define VTP_SERDES_DRP_STATUS_RDY     (1<<16)
#define VTP_SERDES_DRP_STATUS_DO_MASK 0xFFFF

typedef struct ECTrigger_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x4)/4];
} ECTRIGGER_REGS;

#define VTP_ECTRIGGER_CTRL_TCOIL_MASK      0x00F00000
#define VTP_ECTRIGGER_CTRL_DALITZ_MAX_MASK 0x000FFC00
#define VTP_ECTRIGGER_CTRL_DALITZ_MIN_MASK 0x000003FF

typedef struct Trigger_Output_Struct
{
  /** 0x0000 */ volatile uint32_t Latency;
  /** 0x0004 */ volatile uint32_t Width;
  /** 0x0008 */ BLANK[(0x10-0x8)/4];
  /** 0x0010 */ volatile uint32_t BusyScaler;
  /** 0x0014 */ BLANK[(0x100-0x14)/4];
} TRIGGER_OUTPUT_REGS;

#define VTP_TRIGGER_OUTPUT_LATENCY_MASK 0x07FF

#define VTP_TRIGGER_OUTPUT_WIDTH_MASK   0x000F

typedef struct V7_Event_Builder_Struct
{
  /** 0x0000 */ volatile uint32_t BlockSize;
  /** 0x0004 */ volatile uint32_t TriggerFifoBusyThreshold;
  /** 0x0008 */ volatile uint32_t Lookback;
  /** 0x000C */ volatile uint32_t WindowWidth;
  /** 0x0010 */ BLANK[(0x100-0x10)/4];
} V7_EB_REGS;

#define VTP_V7_EB_BLOCKSIZE_MASK  0x00FF

#define VTP_V7_EB_BUSYLEVEL_MASK  0x00FF

#define VTP_V7_EB_LOOKBACK_MASK   0x07FF

#define VTP_V7_EB_WINDOW_MASK     0x07FF

typedef struct v7_bridge_struct
{
  /** 0x43C10000 */ BLANK[0x100/4];

  /** 0x43C10100 */ V7CLK_REGS clk;

  /** 0x43C10200 */ SD_REGS sd;

  /** 0x43C10300 */ FADCDECODER_REGS fadcDec;

  /** 0x43C10400 */ BLANK[(0x1000 - 0x400)/4];

  /** 0x43C11000 */ SERDES_REGS vxs[16];

  /** 0x43C12000 */ SERDES_REGS qsfp[4];

  /** 0x43C12400 */ BLANK[(0x4100 - 0x2400)/4];

  /** 0x43C14100 */ ECTRIGGER_REGS ecTrigger[2];

  /** 0x43C14300 */ BLANK[(0x5000 - 0x4300)/4];

  /** 0x43C15000 */ TRIGGER_OUTPUT_REGS trigOut;

  /** 0x43C15100 */ V7_EB_REGS eb;

  /** 0x43C15200 */ BLANK[(0xFFF4 - 0x5200)/4];
  
  /** 0x43C1FFF4 */ volatile uint32_t Status;
  /** 0x43C1FFF8 */ volatile uint32_t Ctrl;
  /** 0x43C1FFFC */ volatile uint32_t Cfg;

} V7_REGS;

#define VTP_V7BRIDGE_STATUS_INIT_B (1<<1)
#define VTP_V7BRIDGE_STATUS_DONE   (1<<0)

#define VTP_V7BRIDGE_CTRL_PROGRAM_B  (1<<4)
#define VTP_V7BRIDGE_CTRL_RDWR_B     (1<<3)
#define VTP_V7BRIDGE_CTRL_CSI_B      (1<<2)
#define VTP_V7BRIDGE_CTRL_RESET_SOFT (1<<1)
#define VTP_V7BRIDGE_CTRL_RESET      (1<<0)

#define VTP_V7BRIDGE_CFG_DATA_MASK   0xFFFF

typedef struct zync_reg_struct
{
  /** 0x43C00000 */ EB_REGS eb;
  /** 0x43C00100 */ BLANK[(0x10000-0x100)/4];
  /** 0x43C10000 */ V7_REGS v7;
} ZYNC_REGS;

/* Routine prototypes */
int  vtpCheckAddresses();

int  vtpV7SetReset(int val);
int  vtpV7SetResetSoft(int val);
int  vtpV7GetDone();
int  vtpV7GetInit_B();
int  vtpV7SetProgram_B(int val);
int  vtpV7SetRDWR_B(int val);
int  vtpV7SetCSI_B(int val);
void vtpv7WriteCfgData(unsigned short *buf, int N);
int  vtpv7CfgStart();
int  vtpv7CfgLoad(char *filename);
int  vtpv7CfgEnd();

int  vtpOpen();
int  vtpClose();

#endif /* VTPLIB_H */
