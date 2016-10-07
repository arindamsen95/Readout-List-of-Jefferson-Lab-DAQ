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

struct EventBuilder_Struct
{
  /** 0x0000 */ volatile uint32_t LinkCtrl;
  /** 0x0004 */ volatile uint32_t TiCtrl;
  /** 0x0008 */ volatile uint32_t LinkStatus;
  /** 0x000C */ volatile uint32_t TiStatus;
  /** 0x0010 */ volatile uint32_t EbCtrl;
};

struct V7Bridge_FPGAConfig_Struct
{
  /** 0x0000 */ volatile uint32_t Bridge_space[(0xFFF4-0x0000)/4];
  /** 0xFFF4 */ volatile uint32_t V7Status;
  /** 0xFFF8 */ volatile uint32_t V7Ctrl;
  /** 0xFFFC */ volatile uint32_t VTCfg;
};

struct V7_Clk_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
};

struct SD_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t ScalerLatch;
  /** 0x000C          */ uint32_t blank0;
  /** 0x0010 */ volatile uint32_t FPAOSel;
  /** 0x0014 */ volatile uint32_t FPBOSel;
  /** 0x0018 */ volatile uint32_t BusySel;
  /** 0x001C */ volatile uint32_t Trig1Sel;
  /** 0x0020 */ volatile uint32_t SyncSel;
  /** 0x0024          */ uint32_t blank1[(0x40-0x24)/4];
  /** 0x0040 */ volatile uint32_t FPAOVal;
  /** 0x0044 */ volatile uint32_t FPAOVal;
  /** 0x0048          */ uint32_t blank2[(0x60-0x48)/4];
  /** 0x0060 */ volatile uint32_t FPBIStatus;
  /** 0x0064 */ volatile uint32_t Trig1Status;
  /** 0x0068 */ volatile uint32_t Trig2Status;
  /** 0x006C */ volatile uint32_t SyncStatus;
};

struct FadcDecoder_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004          */ uint32_t blank0[(0x20-0x4)/4];
  /** 0x0020 */ volatile uint32_t Latency[16];
};

struct VXS_Serdes_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t DrpCtrl;
  /** 0x000C */ volatile uint32_t DrpStatus;
};

struct QSFP_Serdes_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
  /** 0x0004 */ volatile uint32_t Status;
  /** 0x0008 */ volatile uint32_t DrpCtrl;
  /** 0x000C */ volatile uint32_t DrpStatus;
};

struct ECTrigger_Struct
{
  /** 0x0000 */ volatile uint32_t Ctrl;
};

struct Trigger_Output_Struct
{
  /** 0x0000 */ volatile uint32_t Latency;
  /** 0x0004 */ volatile uint32_t Width;
  /** 0x0008          */ uint32_t blank0[(0x10-0x8)/4;
  /** 0x0010 */ volatile uint32_t Ctrl;
};

struct V7_Event_Builder_Struct
{
  /** 0x0000 */ volatile uint32_t BlockSize;
  /** 0x0004 */ volatile uint32_t TriggerFifoBusyThreshold;
  /** 0x0008 */ volatile uint32_t Lookback;
  /** 0x000C */ volatile uint32_t WindowWidth;
};

  

#endif /* VTPLIB_H */
