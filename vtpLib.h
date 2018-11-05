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
#include "vtp-i2c.h"
#include "vtp-spi.h"
#include "si5341_cfg.h"

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
  /** 0x0014 */ volatile uint32_t EbStatus;
  /** 0x0018 */ volatile uint32_t TiFifo;
  /** 0x001C */ volatile uint32_t VtpFifo;
  /** 0x0020 */ volatile uint32_t TestFifo;
  /** 0x0024 */ BLANK[(0x100-0x24)/4];
} EB_REGS;

#define VTP_EB_LINKCTRL_FIFO_RST    (1<<3)
#define VTP_EB_LINKCTRL_RX_FIFO_RST (1<<2)
#define VTP_EB_LINKCTRL_PLL_RST     (1<<1)
#define VTP_EB_LINKCTRL_RX_RESET    (1<<0)

#define VTP_EB_TICTRL_SYNCEVT_RST (1<<2)
#define VTP_EB_TICTRL_TI_ACK      (1<<1)
#define VTP_EB_TICTRL_TI_BL_REQ   (1<<0)

#define VTP_EB_LINKSTATUS_GCLK_PLL_LOCK     (1<<18)
#define VTP_EB_LINKSTATYS_RX_LOCKED         (1<<17)
#define VTP_EB_LINKSTATUS_RX_READY          (1<<16)
#define VTP_EB_LINKSTATUS_RX_ERROR_CNT_MASK 0xFFFF

#define VTP_EB_TISTATUS_SYNC_EVENT   (1<<16)
#define VTP_EB_TISTATUS_NEXT_BL_MASK 0xFF00
#define VTP_EB_TISTATUS_CUR_BL       0x00FF

#define VTP_EB_EBCTRL_BUILD_TEST (1<<2)
#define VTP_EB_EBCTRL_BUILD_VTP  (1<<1)
#define VTP_EB_EBCTRL_BUILD_TI   (1<<0)

typedef struct AxiDma_Struct
{
  /** 0x0000 */ volatile uint32_t MM2S_DMACR;
  /** 0x0004 */ volatile uint32_t MM2S_DMASR;
  /** 0x0008 */ BLANK[(0x18-0x8)/4];
  /** 0x0018 */ volatile uint32_t MM2S_SA;
  /** 0x001C */ volatile uint32_t MM2S_SA_MSB;
  /** 0x0020 */ BLANK[(0x28-0x20)/4];
  /** 0x0028 */ volatile uint32_t MM2S_LENGTH;
  /** 0x002C */ BLANK[(0x30-0x2C)/4];
  /** 0x0030 */ volatile uint32_t S2MM_DMACR;
  /** 0x0034 */ volatile uint32_t S2MM_DMASR;
  /** 0x0038 */ BLANK[(0x48-0x38)/4];
  /** 0x0048 */ volatile uint32_t S2MM_DA;
  /** 0x004C */ volatile uint32_t S2MM_DA_MSB;
  /** 0x0050 */ BLANK[(0x58-0x50)/4];
  /** 0x0058 */ volatile uint32_t S2MM_LENGTH;
  /** 0x005C */ BLANK[(0x1000-0x5C)/4];
} AXI_DMA_REGS;

typedef struct V7Clk_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t FW_Version;
  /** 0x000C */ BLANK[(0x10-0xC)/4];
  /** 0x0010 */ volatile uint32_t FW_Type;
  /** 0x0014 */ BLANK[(0x100-0x14)/4];
} V7CLK_REGS; /* must be 256 bytes */

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
  /** 0x0070 */ volatile uint32_t Scaler_BusClk;
  /** 0x0074 */ volatile uint32_t Scaler_Sync;
  /** 0x0078 */ volatile uint32_t Scaler_Trig1;
  /** 0x007C */ volatile uint32_t Scaler_Trig2;
  /** 0x0080 */ volatile uint32_t Scaler_Trigger[16];
  /** 0x00C0 */ BLANK[(0x100-0xC0)/4];
} SD_REGS;

#define VTP_SD_CTRL_FPB_OEN  (1<<1)
#define VTP_SD_CTRL_FPB_SEL  (1<<0)

#define VTP_SD_STATUS_FPB_ID_MASK 0x0007

#define VTP_SD_SCALERLATCH_LATCH (1<<0)

#define VTP_SD_BUSYSEL_MASK  0x0003

#define VTP_SD_TRIG1SEL_MASK 0x0003
#define VTP_SD_TRIG1SEL_0    (0<<0)
#define VTP_SD_TRIG1SEL_1    (1<<0)
#define VTP_SD_TRIG1SEL_VXS  (2<<0)

#define VTP_SD_SYNCSEL_MASK  0x0003
#define VTP_SD_SYNCSEL_0     (0<<0)
#define VTP_SD_SYNCSEL_1     (1<<0)
#define VTP_SD_SYNCSEL_VXS   (2<<0)

#define VTP_SD_TRIG1_STATUS (1<<0)

#define VTP_SD_TRIG2_STATUS (1<<0)

#define VTP_SD_SYNC_STATUS  (1<<0)

typedef struct FadcDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x4)/4];
} FADCDECODER_REGS;

#define VTP_FADCDECODER_CTRL_PP_EN(x)  (1<<(x-1))

#define VTP_FADCDECODER_LATENCY_MASK   0xFFFF

typedef struct SspDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x4)/4];
} SSPDECODER_REGS;

typedef struct DcrbDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x4)/4];
} DCRBDECODER_REGS;

#define VTP_FTCALDECODER_CTRL_PP_EN(x)    (1<<(x-1))
#define VTP_FTCALDECODER_CTRL_FIBER_EN(x) (1<<(x+16))

typedef struct FtcalDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x4)/4];
} FTCALDECODER_REGS;

/* Same struct for VXS and QSFP */
typedef struct Serdes_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t DrpCtrl;
  /** 0x0008 */ volatile uint32_t TrxCtrl;
  /** 0x000C */ BLANK[(0x10-0xC)/4];
  /** 0x0010 */ volatile uint32_t Status;
  /** 0x0014 */ volatile uint32_t DrpStatus;
  /** 0x0018 */ BLANK[(0x34-0x18)/4];
  /** 0x0034 */ volatile uint32_t Latency;
  /** 0x0038 */ BLANK[(0x100-0x38)/4];
} SERDES_REGS;

#define VTP_SERDES_CTRL_ER_CNT_RST    (1<<9)
#define VTP_SERDES_CTRL_LOOPBACK_MASK 0x01C0
#define VTP_SERDES_CTRL_POWERDOWN     (1<<5)
#define VTP_SERDES_CTRL_RESETL        (1<<4)
#define VTP_SERDES_CTRL_MODESELL      (1<<3)
#define VTP_SERDES_CTRL_LINKSTATUS    (1<<2)
#define VTP_SERDES_CTRL_RESET         (1<<1)
#define VTP_SERDES_CTRL_GT_RESET      (1<<0)

#define VTP_SERDES_LOOPBACK_NEAREND_PCS  1
#define VTP_SERDES_LOOPBACK_NEAREND_PMA  2
#define VTP_SERDES_LOOPBACK_FAREEND_PMA  4
#define VTP_SERDES_LOOPBACK_FAREND_PCS   6

#define VTP_SERDES_STATUS_SOFT_ERR_CNT_MASK 0xFF000000
#define VTP_SERDES_STATUS_CHUP              (1<<0)
#define VTP_SERDES_STATUS_LANE_UP(x)        (1<<(x+1))

#define VTP_SERDES_DRP_CTRL_EN3       (1<<29)
#define VTP_SERDES_DRP_CTRL_EN2       (1<<28)
#define VTP_SERDES_DRP_CTRL_EN1       (1<<27)
#define VTP_SERDES_DRP_CTRL_EN0       (1<<26)
#define VTP_SERDES_DRP_CTRL_WE        (1<<25)
#define VTP_SERDES_DRP_CTRL_ADDR_MASK 0x01FF0000
#define VTP_SERDES_DRP_CTRL_DI_MASK   0x0000FFFF

#define VTP_SERDES_DRP_STATUS_RDY     (1<<16)
#define VTP_SERDES_DRP_STATUS_DO_MASK 0xFFFF

#define VTP_SERDES_VXS   0
#define VTP_SERDES_QSFP  1

typedef struct ECTrigger_Struct
{
  /** 0x0000 */ volatile uint32_t Hit;
  /** 0x0004 */ volatile uint32_t Dalitz;
  /** 0x0008 */ BLANK[(0x010-0x8)/4];
  /** 0x0010 */ volatile uint32_t HistCtrl;
  /** 0x0014 */ volatile uint32_t HistTime;
  /** 0x0018 */ volatile uint32_t HistPeakPosition;
  /** 0x001C */ volatile uint32_t HistClusterPosition;
  /** 0x0020 */ volatile uint32_t HistClusterEnergy;
  /** 0x0024 */ volatile uint32_t HistClusterDalitz;
  /** 0x0028 */ volatile uint32_t HistClusterLatency;
  /** 0x002C */ BLANK[(0x100-0x2C)/4];
} ECTRIGGER_REGS;

#define VTP_ECTRIGGER_CTRL_EMIN_MASK       0x00001FFF
#define VTP_ECTRIGGER_CTRL_TCOIN_MASK      0x000F0000
#define VTP_ECTRIGGER_CTRL_DALITZ_MAX_MASK 0x03FF0000
#define VTP_ECTRIGGER_CTRL_DALITZ_MIN_MASK 0x000003FF

typedef struct FadcSum_Struct
{
  /** 0x0000 */ volatile uint32_t SumEn[8];
  /** 0x0020 */ BLANK[(0x100-0x020)/4];
} FADCSUM_REGS;

typedef struct ECCosmic_Struct
{
 /** 0x0000 */ volatile uint32_t Ctrl;
 /** 0x0004 */ volatile uint32_t Delay;
 /** 0x0008 */ BLANK[(0x100-0x008)/4];
} ECCOSMIC_REGS;

#define VTP_ECCOSMIC_CTRL_EMIN_MASK      0x00001FFF
#define VTP_ECCOSMIC_CTRL_MULTMAX_MASK   0x3F000000

#define VTP_ECCOSMIC_DELAY_WIDTH_MASK    0x000000FF
#define VTP_ECCOSMIC_DELAY_EVAL_MASK     0x00FF0000

typedef struct PCCosmic_Struct
{
 /** 0x0000 */ volatile uint32_t Ctrl;
 /** 0x0004 */ volatile uint32_t Delay;
 /** 0x0008 */ BLANK[(0x100-0x008)/4];
} PCCOSMIC_REGS;

#define VTP_PCCOSMIC_CTRL_EMIN_MASK      0x00001FFF
#define VTP_PCCOSMIC_CTRL_MULTMAX_MASK   0x3F000000
#define VTP_PCCOSMIC_CTRL_PIXEL_MASK     0x00010000
#define VTP_PCCOSMIC_DELAY_WIDTH_MASK    0x000000FF
#define VTP_PCCOSMIC_DELAY_EVAL_MASK     0x00FF0000


typedef struct PCSTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0010 */ volatile uint32_t Dipfactor;
 /** 0x0014 */ volatile uint32_t DalitzMin;
 /** 0x0018 */ volatile uint32_t DalitzMax;
 /** 0x001C */ volatile uint32_t NstripMax;
 /** 0x0020 */ BLANK[(0x080-0x020)/4];
 /** 0x0080 */ volatile uint32_t ScalerPeakU;
 /** 0x0084 */ volatile uint32_t ScalerPeakV;
 /** 0x0088 */ volatile uint32_t ScalerPeakW;
 /** 0x008c */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x090)/4];
} PCSTRIGGER_REGS;


typedef struct ECSTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0010 */ volatile uint32_t Dipfactor;
 /** 0x0014 */ volatile uint32_t DalitzMin;
 /** 0x0018 */ volatile uint32_t DalitzMax;
 /** 0x001C */ volatile uint32_t NstripMax;
 /** 0x0020 */ BLANK[(0x080-0x020)/4];
 /** 0x0080 */ volatile uint32_t ScalerPeakU;
 /** 0x0084 */ volatile uint32_t ScalerPeakV;
 /** 0x0088 */ volatile uint32_t ScalerPeakW;
 /** 0x008c */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x090)/4];
} ECSTRIGGER_REGS;

#define VTP_FTCAL_CTRL_SEEDTHR_MASK           0x00001FFF
#define VTP_FTCAL_CTRL_SEEDDT_MASK            0x00070000
#define VTP_FTCAL_CTRL_HODODT_MASK            0x07000000
#define VTP_FTCAL_DEADTIMECTRL_DEADTIME_MASK  0x003F0000
#define VTP_FTCAL_DEADTIMECTRL_EMIN_MASK      0x00003FFF

typedef struct FTCALTrigger_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t HistCtrl;
  /** 0x0008 */ volatile uint32_t DeadtimeCtrl;
  /** 0x000C */ BLANK[(0x010-0x00C)/4];
  /** 0x0010 */ volatile uint32_t HistTime;
  /** 0x0014 */ volatile uint32_t HistPosHodo;
  /** 0x0018 */ volatile uint32_t HistEnergyHodo;
  /** 0x001C */ volatile uint32_t HistNHitsHodo;
  /** 0x0020 */ volatile uint32_t HistPos;
  /** 0x0024 */ volatile uint32_t HistEnergy;
  /** 0x0028 */ volatile uint32_t HistNHits;
  /** 0x002C */ BLANK[(0x100-0x02C)/4];
} FTCALTRIGGER_REGS;

#define VTP_FTHODO_CTRL_EMIN_MASK     0x00001FFF

typedef struct FTHODOTrigger_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x004)/4];
} FTHODOTRIGGER_REGS;

typedef struct FTHODOScalers_Struct
{
  /** 0x0000 */ volatile uint32_t Scalers[256];
} FTHODOSCALERS_REGS;

typedef struct DcrbSegment_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x100-0x004)/4];
} DCRBSEGFIND_REGS;

typedef struct DcrbRoad_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ BLANK[(0x020-0x004)/4];
  /** 0x0020 */ volatile uint32_t Scalers[6];
  /** 0x0038 */ BLANK[(0x100-0x038)/4];
} DCRBROADFIND_REGS;

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

typedef struct HtccTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0020 */ BLANK[(0x080-0x010)/4];
 /** 0x0080 */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x084)/4];
} HTCCTRIGGER_REGS;

typedef struct PcuTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0020 */ BLANK[(0x080-0x010)/4];
 /** 0x0080 */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x084)/4];
} PCUTRIGGER_REGS;

typedef struct CtofTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0020 */ BLANK[(0x080-0x010)/4];
 /** 0x0080 */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x084)/4];
} CTOFTRIGGER_REGS;

typedef struct FtofTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0020 */ BLANK[(0x080-0x010)/4];
 /** 0x0080 */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x084)/4];
} FTOFTRIGGER_REGS;

typedef struct CndTrigger_Struct
{
 /** 0x0000 */ volatile uint32_t Thresholds[3];
 /** 0x000C */ volatile uint32_t NFrames;
 /** 0x0020 */ BLANK[(0x080-0x010)/4];
 /** 0x0080 */ volatile uint32_t ScalerHit;
 /** 0x0090 */ BLANK[(0x100-0x084)/4];
} CNDTRIGGER_REGS;

typedef struct GtBit_Struct
{
  /** 0x0000 */ volatile uint32_t STrigger;
  /** 0x0004 */ volatile uint32_t CTrigger;
  /** 0x0008 */ volatile uint32_t Pulser;
  /** 0x000C */ volatile uint32_t STriggerMask;
  /** 0x0010 */ BLANK[(0x40-0x10)/4];
  /** 0x0040 */ volatile uint32_t TriggerScaler;
  /** 0x0044 */ BLANK[(0x80-0x44)/4];
} GTBIT_REGS;

#define VTP_V7_HCAL_CTRL_MASK         0xFFFF
#define VTP_V7_HCAL_CLPULSECOIN_MASK  0x0007
#define VTP_V7_HCAL_CLPULSETHR_MASK   0x1FFF

typedef struct Hcal_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t ClusterPulseCoincidence;
  /** 0x0008 */ volatile uint32_t ClusterPulseThreshold;
  /** 0x000C */ BLANK[(0x100-0x0C)/4];
} HCAL_REGS;

typedef struct v7_bridge_struct
{
  /** 0x43C10000 */ BLANK[0x100/4];

  /** 0x43C10100 */ V7CLK_REGS clk;

  /** 0x43C10200 */ SD_REGS sd;

  /** 0x43C10300 */ FADCDECODER_REGS fadcDec;
  
  /** 0x43C10400 */ SSPDECODER_REGS sspDec;

  /** 0x43C10500 */ DCRBDECODER_REGS dcrbDec;
  
  /** 0x43C10600 */ FTCALDECODER_REGS ftcalDec;

  /** 0x43C10700 */ BLANK[(0x1000 - 0x700)/4];

  /** 0x43C11000 */ SERDES_REGS vxs[16];

  /** 0x43C12000 */ SERDES_REGS qsfp[4];

  /** 0x43C12400 */ BLANK[(0x3E00 - 0x2400)/4];

  /** 0x43C13E00 */ PCUTRIGGER_REGS pcuTrigger;

  /** 0x43C13F00 */ CTOFTRIGGER_REGS ctofTrigger;

  /** 0x43C14000 */ FTOFTRIGGER_REGS ftofTrigger;

  /** 0x43C14100 */ ECTRIGGER_REGS ecTrigger[2];

  /** 0x43C14300 */ FADCSUM_REGS fadcSum;
  
  /** 0x43C14400 */ DCRBSEGFIND_REGS dcrbSegFind[2];
  
  /** 0x43C14600 */ ECCOSMIC_REGS ecCosmic[2];

  /** 0x43C14800 */ HCAL_REGS hcal;
  
  /** 0x43C14900 */ BLANK[(0x4A00 - 0x4900)/4];
  
  /** 0x43C14A00 */ PCCOSMIC_REGS pcCosmic;
  
  /** 0x43C14B00 */ FTCALTRIGGER_REGS ftcalTrigger;
  
  /** 0x43C14C00 */ PCSTRIGGER_REGS pcsTrigger;

  /** 0x43C14D00 */ ECSTRIGGER_REGS ecsTrigger;

  /** 0x43C14E00 */ HTCCTRIGGER_REGS htccTrigger;

  /** 0x43C14F00 */ FTHODOTRIGGER_REGS fthodoTrigger;

  /** 0x43C15000 */ TRIGGER_OUTPUT_REGS trigOut;

  /** 0x43C15100 */ V7_EB_REGS eb;

  /** 0x43C15200 */ DCRBROADFIND_REGS dcrbRoadFind;

  /** 0x43C15300 */ CNDTRIGGER_REGS cndTrigger;

  /** 0x43C15400 */ FTHODOSCALERS_REGS fthodoScalers;

  /** 0x43C15800 */ BLANK[(0x6000 - 0x5800)/4];

  /** 0x43C16000 */ GTBIT_REGS gtBit[16]; /* 128 bytes */
  
  /** 0x43C16800 */ BLANK[(0xFFF4 - 0x6800)/4];
  
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
  /** 0x43C00100 */ BLANK[(0x1000-0x100)/4];
  /** 0x43C01000 */ AXI_DMA_REGS dma_ti;
  /** 0x43C02000 */ AXI_DMA_REGS dma_vtp;
  /** 0x43C03000 */ BLANK[(0x10000-0x3000)/4];
  /** 0x43C10000 */ V7_REGS v7;
} ZYNC_REGS;

/* Open/Close argument (device mask bits) */
#define VTP_FPGA_OPEN  (1<<0)					     
#define VTP_I2C_OPEN   (1<<1)					     
#define VTP_SPI_OPEN   (1<<2)					     

/* Initialization Flags */
#define VTP_INIT_CLK_MASK            0x0000000F
#define VTP_INIT_CLK_INT             (1<<0)
#define VTP_INIT_CLK_VXS             (2<<0)
#define VTP_INIT_SKIP                (1<<16)
#define VTP_INIT_SKIP_FIRMWARE_CHECK (1<<18)

#define VTP_FW_TYPE_COMMON            0
#define VTP_FW_TYPE_EC                1
#define VTP_FW_TYPE_GT                2
#define VTP_FW_TYPE_DC                3
#define VTP_FW_TYPE_HCAL              4
#define VTP_FW_TYPE_PC                5
#define VTP_FW_TYPE_PCS               6
#define VTP_FW_TYPE_HTCC              7
#define VTP_FW_TYPE_ECS               8
#define VTP_FW_TYPE_FTCAL             9
#define VTP_FW_TYPE_FTHODO            10
#define VTP_FW_TYPE_FTOF              11
#define VTP_FW_TYPE_CND               12

/* Routine prototypes */
int  vtpInit(int iFlag);
int  vtpBReady();
int  vtpV7GetFW_Version();
int  vtpV7GetFW_Type();
#ifdef IPC
int  vtpSendScalers();
int  vtpSendSerdes();
#endif
					     
int  vtpCheckAddresses();

#define NSERDES 10
int  vtpSerdesStatus(int type, uint16_t pp, int pflag, int data[NSERDES]);
int  vtpSerdesEnable(int type, uint16_t idx, int enable);
int  vtpSerdesStatusAll();
int  vtpSerdesCheckLinks();

int  vtpV7PllReset(int enable);

int  vtpV7CtrlInit();
int  vtpV7SetReset(int val);
int  vtpV7SetResetSoft(int val);
int  vtpV7GetDone();
int  vtpV7GetInit_B();
int  vtpV7SetProgram_B(int val);
int  vtpV7SetRDWR_B(int val);
int  vtpV7SetCSI_B(int val);
void vtpV7WriteCfgData(uint16_t *buf, int N);
int  vtpV7CfgStart();
int  vtpV7CfgLoad(char *filename);
int  vtpV7CfgEnd();
int  vtpZ7CfgLoad(char *filename);

int  vtpOpen(int dev_mask);
int  vtpClose(int dev_mask);

unsigned int vtpRead32(volatile unsigned int *addr);
int  vtpWrite32(volatile unsigned int *addr, unsigned int val);

// VTP_FW_TYPE_EC functions
int  vtpSetFadcSum_MaskEn(unsigned int mask[16]);
int  vtpGetFadcSum_MaskEn(unsigned int mask[16]);
int  vtpPrintHist_ClusterPosition(int inst);
int  vtpPrintHist_PeakPosition(int inst);
int  vtpSetECtrig_dalitz(int inst, int min, int max);
int  vtpSetECtrig_emin(int inst, int emin);
int  vtpSetECtrig_dt(int inst, int dt);
int  vtpGetECtrig_dt(int inst, int *dt);
int  vtpGetECtrig_emin(int inst, int *emin);
int  vtpGetECtrig_dalitz(int inst, int *min, int *max);
int  vtpGetECtrig_peak_multmax(int inst, int *mult_max);
int  vtpSetECcosmic_emin(int inst, int emin);
int  vtpGetECcosmic_emin(int inst, int *emin);
int  vtpSetECcosmic_multmax(int inst, int mult_max);
int  vtpGetECcosmic_multmax(int inst, int *mult_max);
int  vtpSetECcosmic_width(int inst, int hitwidth);
int  vtpGetECcosmic_width(int inst, int *hitwidth);
int  vtpSetECcosmic_delay(int inst, int evaldelay);
int  vtpGetECcosmic_delay(int inst, int *evaldelay);

// VTP_FW_TYPE_PC functions
//int  vtpSetFadcSum_MaskEn(unsigned int mask[16]);
//int  vtpGetFadcSum_MaskEn(unsigned int mask[16]);
int  vtpSetPCcosmic_emin(int emin);
int  vtpGetPCcosmic_emin(int *emin);
int  vtpSetPCcosmic_multmax(int mult_max);
int  vtpGetPCcosmic_multmax(int *mult_max);
int  vtpSetPCcosmic_width(int hitwidth);
int  vtpGetPCcosmic_width(int *hitwidth);
int  vtpSetPCcosmic_delay(int evaldelay);
int  vtpGetPCcosmic_delay(int *evaldelay);
int  vtpSetPCcosmic_pixel(int enable);
int  vtpGetPCcosmic_pixel(int *enable);

// VTP_FW_TYPE_PCS functions
int  vtpSetPCS_thresholds(int thr0, int thr1, int thr2);
int  vtpGetPCS_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetPCS_nframes(int nframes);
int  vtpGetPCS_nframes(int *nframes);
int  vtpSetPCS_dipfactor(int dipfactor);
int  vtpGetPCS_dipfactor(int *dipfactor);
int  vtpSetPCS_nstrip(int nstripmin, int nstripmax);
int  vtpGetPCS_nstrip(int *nstripmin, int *nstripmax);
int  vtpSetPCS_dalitz(int dalitz_min, int dalitz_max);
int  vtpGetPCS_dalitz(int *dalitz_min, int *dalitz_max);
int  vtpPcsPrintScalers();
#ifdef IPC
int  vtpPcsSendErrors(char *host);
int  vtpPcsSendScalers(char *host);
#endif
int  vtpSetPCU_thresholds(int thr0, int thr1, int thr2);
int  vtpGetPCU_thresholds(int *thr0, int *thr1, int *thr2);

// VTP_FW_TYPE_ECS functions
//int  vtpSetFadcSum_MaskEn(unsigned int mask[16]);
//int  vtpGetFadcSum_MaskEn(unsigned int mask[16]);
int  vtpSetECS_thresholds(int thr0, int thr1, int thr2);
int  vtpGetECS_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetECS_nframes(int nframes);
int  vtpGetECS_nframes(int *nframes);
int  vtpSetECS_dipfactor(int dipfactor);
int  vtpGetECS_dipfactor(int *dipfactor);
int  vtpSetECS_nstrip(int nstripmin, int nstripmax);
int  vtpGetECS_nstrip(int *nstripmin, int *nstripmax);
int  vtpSetECS_dalitz(int dalitz_min, int dalitz_max);
int  vtpGetECS_dalitz(int *dalitz_min, int *dalitz_max);
int  vtpEcsPrintScalers();
#ifdef IPC
int  vtpEcsSendErrors(char *host);
int  vtpEcsSendScalers(char *host);
#endif

// VTP_FW_TYPE_GT functions
int  vtpSetGt_latency(int latency);
int  vtpGetGt_latency();
int  vtpSetGt_width(int width);
int  vtpGetGt_width();
int  vtpSetGtTriggerBit(int inst, int strigger_mask, int sector_mask, int mult_min, int coin_width, int ctrigger_mask, int delay, float pulser_freq);
int  vtpGetGtTriggerBit(int inst, int *strigger_mask, int *sector_mask, int *mult_min, int *coin_width, int *ctrigger_mask, int *delay, float *pulser_freq);
#ifdef IPC
int  vtpGtSendScalers(char *host);
#endif

// VTP_FW_TYPE_DC functions
int  vtpSetDc_SegmentThresholdMin(int inst, int threshold);
int  vtpGetDc_SegmentThresholdMin(int inst, int *threshold);
#ifdef IPC
int  vtpDcSendErrors(char *host);
int  vtpDcSendScalers(char *host);
#endif

// VTP_FW_TYPE_HCAL functions
int  vtpSetHcal_ClusterCoincidence(int coin);
int  vtpGetHcal_ClusterCoincidence(int *coin);
int  vtpSetHcal_ClusterThreshold(int thr);
int  vtpGetHcal_ClusterThreshold(int *thr);

// VTP_FW_TYPE_FTCAL functions
//int  vtpSetFadcSum_MaskEn(unsigned int mask[16]);
//int  vtpGetFadcSum_MaskEn(unsigned int mask[16]);
int  vtpSetFTCALseed_emin(int emin);
int  vtpGetFTCALseed_emin(int *emin);
int  vtpSetFTCALseed_dt(int dt);
int  vtpGetFTCALseed_dt(int *dt);
int  vtpSetFTCALhodo_dt(int dt);
int  vtpGetFTCALhodo_dt(int *dt);
int  vtpSetFTCALcluster_deadtime(int deadtime);
int  vtpGetFTCALcluster_deadtime(int *deadtime);
int  vtpSetFTCALcluster_deadtime_emin(int emin);
int  vtpGetFTCALcluster_deadtime_emin(int *emin);
#ifdef IPC
int  vtpFTSendErrors(char *host);
int  vtpFTSendScalers(char *host);
#endif

// VTP_FW_TYPE_FTHODO functions
int  vtpSetFTHODOemin(int emin);
int  vtpGetFTHODOemin(int *emin);
#ifdef IPC
int  vtpFTHodoSendErrors(char *host);
int  vtpFTHodoSendScalers(char *host);
#endif

// VTP_FT_TYPE_HTCC functions
#ifdef IPC
int  vtpHtccSendErrors(char *host);
int  vtpHtccSendScalers(char *host);
#endif
int  vtpHtccPrintScalers();
int  vtpSetHTCC_thresholds(int thr0, int thr1, int thr2);
int  vtpGetHTCC_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetHTCC_nframes(int nframes);
int  vtpGetHTCC_nframes(int *nframes);
int  vtpSetCTOF_thresholds(int thr0, int thr1, int thr2);
int  vtpGetCTOF_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetCTOF_nframes(int nframes);
int  vtpGetCTOF_nframes(int *nframes);

// VTP_FT_TYPE_FTOF functions
#ifdef IPC
int  vtpFtofSendErrors(char *host);
int  vtpFtofSendScalers(char *host);
#endif
int  vtpFtofPrintScalers();
int  vtpSetFTOF_thresholds(int thr0, int thr1, int thr2);
int  vtpGetFTOF_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetFTOF_nframes(int nframes);
int  vtpGetFTOF_nframes(int *nframes);

// VTP_FT_TYPE_CND functions
#ifdef IPC
int  vtpCndSendErrors(char *host);
int  vtpCndSendScalers(char *host);
#endif
int  vtpCndPrintScalers();
int  vtpSetCND_thresholds(int thr0, int thr1, int thr2);
int  vtpGetCND_thresholds(int *thr0, int *thr1, int *thr2);
int  vtpSetCND_nframes(int nframes);
int  vtpGetCND_nframes(int *nframes);

// VTP_FW_TYPE_COMMON functions
int  vtpSetFPBO(unsigned int val);
int  vtpSetFPBSel(unsigned int sel);
int  vtpReadScalers(unsigned int *data, int max_scalers);
int  vtpSetTrig1Source(int src);
int  vtpSetSyncSource(int src);
int  vtpEnableTriggerPayloadMask(int pp_mask);
int  vtpEnableTriggerFiberMask(int fiber_mask);
int  vtpGetWindowWidth();
int  vtpGetWindowLookback();
int  vtpGetTriggerPayloadMask();
int  vtpGetTriggerFiberMask();
int  vtpEbReadEvent(uint32_t *pBuf, uint32_t maxsize);
int  vtpEbTiReadEvent(uint32_t *pBuf, uint32_t maxsize);
int  vtpEbDecodeEvent(uint32_t *pBuf, uint32_t size);
int  vtpEbReadAndDecodeEvent();
int  vtpSetWindow(int, int);
int  vtpTiLinkInit();
int  vtpEbResetFifo();
int  vtpEbBuildTestEvent(int len);
int  vtpEbReset();
int  vtpSetBlockLevel(int level);
int  vtpTiLinkGetBlockLevel(int print);
int  vtpTiAck(int clearsync);

#define VTP_DMA_TI  0
#define VTP_DMA_VTP 1

int  vtpDmaStatus(int id);
int  vtpDmaInit(int id);
int  vtpDmaStart(int id, unsigned int destAddr, int maxLength);
int  vtpDmaWaitDone(int id);

int  vtpCreateLockShm();
int  vtpKillLockShm(int kflag);
int  vtpLock();
int  vtpTryLock();
int  vtpTimedLock(int time_seconds);
int  vtpUnlock();
int  vtpCheckMutexHealth(int time_seconds);

int  vtpDmaMemOpen(int nbuffers, int size);
int  vtpDmaMemClose();
unsigned long vtpDmaMemGetPhysAddress(int buffer_id);
unsigned long vtpDmaMemGetLocalAddress(int buffer_id);

#endif /* VTPLIB_H */

