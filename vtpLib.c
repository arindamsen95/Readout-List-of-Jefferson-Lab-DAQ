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
 *     VTP library
 *
 *----------------------------------------------------------------------------*/
#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "ipc.h"
#include "vtpLib.h"

#define VTP_FT_SENDHODOSCALERS     1

/* Shared Robust Mutex for vme bus access */
char* shm_name_vtp = "/vtp";

typedef struct vtpShmData
{
  uint32_t Control;
  uint32_t Status;
  uint32_t FirmwareType;
} VTPSHMDATA;

/* Keep this as a structure, in case we want to add to it in the future */
struct shared_memory_struct
{
  pthread_mutex_t mutex;
  pthread_mutexattr_t m_attr;
  VTPSHMDATA vtp;
  uint32_t shmSize;
};
struct shared_memory_struct *p_sync=NULL;
/* mmap'd address of shared memory mutex */
void *addr_shm = NULL;

static int vtpDevOpenMASK = 0;
static int vtpFPGAFD = -1;
const char vtpFPGADev[256] = "/dev/uio0";

static int VTP_FW_Version = 0;
static int VTP_FW_Type = 0;

static volatile ZYNC_REGS *vtp = NULL;

static int vtpEbTiEventReadErrors;
static int vtpEbEventReadErrors;

/* Mutex to guard VTP read/writes */
pthread_mutex_t   vtpMutex = PTHREAD_MUTEX_INITIALIZER;
#define VLOCK     if(pthread_mutex_lock(&vtpMutex)<0) perror("pthread_mutex_lock");
#define VUNLOCK   if(pthread_mutex_unlock(&vtpMutex)<0) perror("pthread_mutex_unlock");

#define CHECKINIT {						\
    if(vtp == NULL) {						\
      printf("%s: ERROR: VTP not initialized\n",__func__);	\
      return ERROR;						\
    }								\
  }

#define CHECKTYPE(v) {        \
    if( (v != VTP_FW_TYPE_COMMON) && (v != VTP_FW_Type) ) { \
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);  \
      return ERROR;           \
    }               \
  }
      

/*******************************************************************************
 *
 * vtpInit - Initialize JLAB VTP Library. 
 *
 *
 *   iFlag: 18 bit integer
 *      bit 3-0:  Defines trig/sync/clock source
 *             1 Internal clock, software trig & sync
 *             2 VXS clock, trig, sync
 *             0,2-15 undefined
 * 
 * 
 *      bit 16:  Exit before board initialization
 *             0 Initialize FADC (default behavior)
 *             1 Skip initialization (just setup register map pointers)
 *
 *      bit 18:  Skip firmware check.  Useful for firmware updating.
 *             0 Perform firmware check
 *             1 Skip firmware check
 *      
 *
 * RETURNS: OK, or ERROR if the address is invalid or a board is not present.
 */

int
vtpInit(int iFlag)
{
  int rval = OK;
  int syncSrc, trig1Src, clkSrc, sdStatus;
  
  rval = vtpCheckAddresses();
  if(rval != OK)
    return rval;

  switch(iFlag & VTP_INIT_CLK_MASK)
  {
    case VTP_INIT_CLK_INT:
      syncSrc = VTP_SD_SYNCSEL_0;
      trig1Src = VTP_SD_TRIG1SEL_0;
      clkSrc = SI5341_IN_SEL_LOCAL;
      break;
      
    case VTP_INIT_CLK_VXS:
      syncSrc = VTP_SD_SYNCSEL_VXS;
      trig1Src = VTP_SD_TRIG1SEL_VXS;
      clkSrc = SI5341_IN_SEL_VXS;
      break;
      
    default:
      printf("%s: ERROR invalid trig/sync/clock source specification.\n", __func__);
      break;
  }

  if(iFlag & VTP_INIT_SKIP)
  {
    VTP_FW_Version = vtpV7GetFW_Version();
    VTP_FW_Type = vtpV7GetFW_Type();
    printf("%s: VTP_FW_Version=%d, VTP_FW_Type=%d\n", __func__, VTP_FW_Version, VTP_FW_Type);
    return rval;
  }
  
  vtpLock();
  
  vtpV7SetReset(1);
  vtpV7SetResetSoft(1);

  vtpV7SetReset(0);
  vtpV7SetResetSoft(0);

  VTP_FW_Type = vtpV7GetFW_Type();
  VTP_FW_Version = vtpV7GetFW_Version();
  printf("%s: VTP_FW_Version=%d, VTP_FW_Type=%d\n", __func__, VTP_FW_Version, VTP_FW_Type);
  
  if(clkSrc == SI5341_IN_SEL_LOCAL)
  {
    printf("%s: Setting up VTP PLL for local reference\n", __func__);
    si5341_Init(SI5341_IN_SEL_LOCAL);
  }
  else if(clkSrc == SI5341_IN_SEL_VXS)
  {
    switch(VTP_FW_Type)
    {
      case VTP_FW_TYPE_EC:
      case VTP_FW_TYPE_PC:
      case VTP_FW_TYPE_GT:
      case VTP_FW_TYPE_HCAL:
      case VTP_FW_TYPE_PCS:
      case VTP_FW_TYPE_HTCC:
      case VTP_FW_TYPE_FTOF:
      case VTP_FW_TYPE_CND:
      case VTP_FW_TYPE_ECS:
      case VTP_FW_TYPE_FTCAL:
      case VTP_FW_TYPE_FTHODO:
        printf("%s: Setting up VTP PLL for 250MHz VXS reference\n", __func__);
        si5341_Init(SI5341_IN_SEL_VXS_250);
        break;
        
      case VTP_FW_TYPE_DC:
        printf("%s: Setting up VTP PLL for 125MHz VXS reference\n", __func__);
        si5341_Init(SI5341_IN_SEL_VXS_125);
        break;
        
      default:
        printf("%s: ERROR - unknown firmware type %d. Unable to setup VTP PLL.\n", __func__, VTP_FW_Type);
        vtpUnlock();
        return ERROR;
    }
  }
  
  vtpV7PllReset(1);
  vtpV7PllReset(0);

  vtpUnlock();

  vtpSetTrig1Source(trig1Src);
  vtpSetSyncSource(syncSrc);

  vtpTiLinkInit();
  
  vtpEbResetFifo();

  VLOCK;
  vtp->v7.sd.FPAOSel = 0xFFFFFFFF;  /* Route trigger output to FPAO */
  vtp->v7.sd.FPBOSel = 0xFFFFFFFF;  /* Route trigger output to FPBO */
  sdStatus = vtp->v7.sd.Status;
  VUNLOCK;

  printf("VTP SD Daughtercard ID = 0x%08X\n", sdStatus);

  vtpEbTiEventReadErrors = 0;
  vtpEbEventReadErrors = 0;

  return rval;
}

int
vtpSetBlockLevel(int level)
  {
  CHECKINIT;

  VLOCK;
  vtp->v7.eb.BlockSize = level;
  VUNLOCK;
  
  return(OK);
  }

int
vtpGetBlockLevel()
{
  int rval;
  CHECKINIT;

  VLOCK;
  rval = vtp->v7.eb.BlockSize;
  VUNLOCK;
  
  return(rval);
}

int
vtpTiLinkGetBlockLevel(int print)
{
  int val;
  CHECKINIT;
  
  VLOCK;
  vtp->eb.TiCtrl = VTP_EB_TICTRL_TI_BL_REQ;
  VUNLOCK;
  
  usleep(1000);
  
  VLOCK;
  val = vtp->eb.TiStatus & 0xFF;
  VUNLOCK;
  
  if(print)
    printf("%s: returned %d\n", __func__, val);
  
  return val;
}

int
vtpSetWindow(int lookback, int width)
{
  CHECKINIT;

  VLOCK;
  vtp->v7.eb.Lookback = lookback/4;
  vtp->v7.eb.WindowWidth = width/4;
  VUNLOCK;

  return(OK);
}

int
vtpGetWindowLookback()
{
  int rval;
  CHECKINIT;

  VLOCK;
  rval = vtp->v7.eb.Lookback * 4;
  VUNLOCK;

  return(rval);
}

int
vtpGetWindowWidth()
{
  int rval;
  CHECKINIT;

  VLOCK;
  rval = vtp->v7.eb.WindowWidth * 4;;
  VUNLOCK;

  return(rval);
}
  
int
vtpCheckAddresses()
{
  int rval = OK;
  unsigned long offset=0, expected=0, base=0;
  ZYNC_REGS test;
  
  printf("%s:\n\t ---------- Checking VTP register map ---------- \n",
	 __func__);

  base = (unsigned long) &test.eb.LinkCtrl;

  offset = ((unsigned long) &test.v7) - base;
  expected = 0x10000;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7 not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.clk) - base;
  expected = 0x10100;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.clk not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.sd) - base;
  expected = 0x10200;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.sd not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.sd.FPAOVal) - base;
  expected = 0x10240;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.sd.FPAOVal not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.fadcDec) - base;
  expected = 0x10300;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.fadcDec not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.vxs[0]) - base;
  expected = 0x11000;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.vxs[0] not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.qsfp[0]) - base;
  expected = 0x12000;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.qsfp[0] not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.ecTrigger[0]) - base;
  expected = 0x14100;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.ecTrigger[0] not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.trigOut) - base;
  expected = 0x15000;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.trigOut not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.eb) - base;
  expected = 0x15100;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.eb not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &test.v7.Cfg) - base;
  expected = 0x1FFFC;
  if(offset != expected)
    {
      printf("%s: ERROR VTPp->v7.Cfg not at offset = 0x%lx (@ 0x%lx)\n",
	     __func__,expected,offset);
      rval = ERROR;
    }

  return rval;
}

/* Macro for Serdes routines */
#define CHECKTYPEDEV {							\
    if(type == VTP_SERDES_VXS) {					\
      if(dev > 16) {							\
	printf("%s: ERROR: Invalid dev (%d) for type (%d)\n",		\
	       __func__, dev, type);					\
	return ERROR;							\
      }	else {								\
	sdev = &vtp->v7.vxs[dev];					\
      }									\
    } else if(type == VTP_SERDES_QSFP) {				\
      if(dev > 3) {							\
	printf("%s: ERROR: Invalid dev (%d) for type (%d)\n",		\
	       __func__, dev, type);					\
	return ERROR;							\
      }	else {								\
	sdev = &vtp->v7.qsfp[dev];					\
      }									\
    } else {								\
      printf("%s: ERROR: Invalid type (%d)\n",				\
	     __func__, type);						\
      return ERROR;							\
    }									\
  }									\

#define VTP_SERDES_MAX_TRIES  10

int
vtpSerdesCheckLinks()
{
  uint32_t i, status, ctrl, pass, tries;
  CHECKINIT;
  
  for(tries=0; tries<VTP_SERDES_MAX_TRIES; tries++)
  {
    printf("Waiting on links:");
    pass = 1;
  
    for(i=0; i<20; i++)
    {
      VLOCK;
      if(i<16)
      {
        ctrl = vtp->v7.vxs[i].Ctrl;
        status = vtp->v7.vxs[i].Status;
      }
      else
      {
        ctrl = vtp->v7.qsfp[i-16].Ctrl;
        status = vtp->v7.qsfp[i-16].Status;
      }
      VUNLOCK;

      if(!(ctrl & VTP_SERDES_CTRL_GT_RESET))
      {
        if(!(status & VTP_SERDES_STATUS_CHUP))
        {
          if(i<16)
            printf(" PP%d", i+1);
          else
            printf(" FB%d", i-15);

          pass = 0;
        }
      }
    }

    if(pass)
    {
      printf(" none. All links up!\n");
      break;
    }
    else
    {
      printf("\n");
      sleep(1);
    }
  }

  if(tries>=VTP_SERDES_MAX_TRIES)
    printf("%s: ERROR - all serdes links not up!\n", __func__);

  return pass;
}

int
vtpSerdesStatus(int type, uint16_t dev, int pflag, int data[NSERDES])
{
  volatile SERDES_REGS *sdev;
  uint32_t status = 0, ctrl = 0, ctrl2, latency = 0;
  int index;
  CHECKINIT;
  CHECKTYPEDEV;
  
  VLOCK;
  ctrl2 = sdev->Ctrl;
  status = sdev->Status;
  latency = sdev->Latency;
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_ECS:
    case VTP_FW_TYPE_PCS:
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_FTOF:
    case VTP_FW_TYPE_HTCC:
    case VTP_FW_TYPE_CND:
    case VTP_FW_TYPE_FTHODO:
      ctrl = vtp->v7.fadcDec.Ctrl;
      break;
    case VTP_FW_TYPE_GT:
      ctrl = vtp->v7.sspDec.Ctrl;
      break;
    case VTP_FW_TYPE_DC:
      ctrl = vtp->v7.dcrbDec.Ctrl;
      break;
    case VTP_FW_TYPE_HCAL:
      ctrl = vtp->v7.hcal.Ctrl;
      break;
    case VTP_FW_TYPE_FTCAL:
      ctrl = vtp->v7.ftcalDec.Ctrl;
      break;
  }
  VUNLOCK;


  if(pflag==1) /* print */
  {
    if(dev==0)
    {
      printf("\n");
      if(type == VTP_SERDES_VXS)
      {
        printf("    ---Lane---    Error  Link  Trg Latency(ns)\n");
        printf("PP  0 1 2 3    Ch Count  Reset En  RX     TX\n");
      }
      else
      {
        printf("    ---Lane---    Error  Link  Trg Latency(ns)\n");
        printf("FB  0 1 2 3    Ch Count  Reset En  RX     TX\n");
      }
      printf("------------------------------------------------------------------------------\n");
    }
  
    printf("%2d  ", dev);
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(0))?"U":"D");
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(1))?"U":"D");
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(2))?"U":"D");
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(3))?"U":"D");
    printf("%s  ", (status & VTP_SERDES_STATUS_CHUP)?"U":"D");
    printf("%3d    ", (status & VTP_SERDES_STATUS_SOFT_ERR_CNT_MASK)>>24);
    printf("%s     ", (ctrl2 & VTP_SERDES_CTRL_GT_RESET)?"1":"0");
    if(type == VTP_SERDES_VXS)
      printf("%s", (ctrl & (1<<dev)) ? "1   ":"0   ");
    else
      printf("%s", (ctrl & (1<<(dev+16))) ?  "1   ":"0   ");
    printf("%5d ", ((latency>>16)&0xFFFF)*4);
    printf("%5d ", ((latency>>0)&0xFFFF)*4);
    printf("\n");
  }
  else /* send */
  {
    index = 0;

    data[index++] = (status & VTP_SERDES_STATUS_LANE_UP(0)) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = (status & VTP_SERDES_STATUS_LANE_UP(1)) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = (status & VTP_SERDES_STATUS_LANE_UP(2)) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = (status & VTP_SERDES_STATUS_LANE_UP(3)) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = (status & VTP_SERDES_STATUS_CHUP) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = (status & VTP_SERDES_STATUS_SOFT_ERR_CNT_MASK)>>24;
    if(index>NSERDES) return(OK);

    data[index++] = (ctrl2 & VTP_SERDES_CTRL_GT_RESET) ? 1 : 0;
    if(index>NSERDES) return(OK);

    if(type == VTP_SERDES_VXS) data[index++] = (ctrl & (1<<dev)) ? 1 : 0;
    else                       data[index++] = (ctrl & (1<<(dev+16))) ? 1 : 0;
    if(index>NSERDES) return(OK);

    data[index++] = ((latency>>16)&0xFFFF)*4;
    if(index>NSERDES) return(OK);

    data[index++] = ((latency>>0)&0xFFFF)*4;
    if(index>NSERDES) return(OK);
  }

  return OK;
}

int
vtpSerdesStatusAll()
{
  int i;
  int data[NSERDES];

  for(i = 0; i < 16; i++)
    vtpSerdesStatus(VTP_SERDES_VXS, i, 1, data);

  for(i = 0; i < 4; i++)
    vtpSerdesStatus(VTP_SERDES_QSFP, i, 1, data);

  return OK;
}

int
vtpSerdesEnable(int type, uint16_t idx, int enable)
{
  CHECKINIT;
  volatile SERDES_REGS *pSerdes;

  if( (type == VTP_SERDES_VXS) && (idx >= 0) && (idx < 16) )
    pSerdes = &vtp->v7.vxs[idx];
  else if( (type == VTP_SERDES_QSFP) && (idx >= 0) && (idx < 4) )
    pSerdes = &vtp->v7.qsfp[idx];
  else
{
    printf("%s: Error - invalid serdes selection(type=%d, idx=%d)\n", __func__, type, idx);
    return ERROR;
}

  VLOCK;
  if(enable)
{
    pSerdes->Ctrl = VTP_SERDES_CTRL_GT_RESET;
    usleep(10);
    pSerdes->Ctrl = 0;
    usleep(10000);
}
  else
    pSerdes->Ctrl = VTP_SERDES_CTRL_GT_RESET;
  VUNLOCK;

  return OK;
}

int
vtpSerdesSettings(int type, uint16_t idx, int txpre, int txpost, int txswing, int lpmen)
{
  CHECKINIT;
  volatile SERDES_REGS *pSerdes;

  if( (type == VTP_SERDES_VXS) && (idx >= 0) && (idx < 16) )
    pSerdes = &vtp->v7.vxs[idx];
  else if( (type == VTP_SERDES_QSFP) && (idx >= 0) && (idx < 4) )
    pSerdes = &vtp->v7.qsfp[idx];
  else
{
    printf("%s: Error - invalid serdes selection(type=%d, idx=%d)\n", __func__, type, idx);
    return ERROR;
}

  VLOCK;
  pSerdes->TrxCtrl =
    ((txpre & 0x1F)<<0) |
    ((txpost & 0x1F)<<5) |
    ((txswing & 0xF)<<10) |
    ((lpmen & 0x1)<<31);
  pSerdes->Ctrl = VTP_SERDES_CTRL_GT_RESET;
  usleep(10);
  pSerdes->Ctrl = 0;
  usleep(10000);
  VUNLOCK;

  return OK;
}

int
vtpV7PllReset(int enable)
{
  int status;
  
  if(enable)
  {
    VLOCK;
    vtp->v7.clk.Ctrl = VTP_V7CLK_CTRL_GCLK_RESET;
    VUNLOCK;
  }
  else
  {
    VLOCK;
    vtp->v7.clk.Ctrl &= ~VTP_V7CLK_CTRL_GCLK_RESET;
    VUNLOCK;
  }
  usleep(10000);    
  
  VLOCK;
  status = vtp->v7.clk.Status;
  VUNLOCK;
  if(status & VTP_V7CLK_STATUS_GCLK_LOCKED)
  {
    printf("%s: PLL successfully locked\n",
      __func__);
  }
  else
  {
    printf("%s: PLL not locked\n",
      __func__);

    if(!enable)
      return ERROR;
  }
  
  return OK;
}

int
vtpV7GetFW_Version()
{
  int rval=0;
  CHECKINIT;
  
  VLOCK;
  rval = vtp->v7.clk.FW_Version;
  VUNLOCK;

  return rval;
}

int
vtpV7GetFW_Type()
{
  int rval=0;
  CHECKINIT;
  
  VLOCK;
  rval = vtp->v7.clk.FW_Type;
  VUNLOCK;

  return rval;
}

static unsigned int CfgCtrl_Shadow = 0x1F;

int
vtpV7CtrlInit()
{
  CHECKINIT;

  VLOCK;
  CfgCtrl_Shadow = 0x1F;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;

  return OK;
}

int
vtpV7SetReset(int val)
{
  CHECKINIT;

  VLOCK;
  if(val)
    CfgCtrl_Shadow |= VTP_V7BRIDGE_CTRL_RESET;
  else
    CfgCtrl_Shadow &= ~VTP_V7BRIDGE_CTRL_RESET;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;

  return OK;
}

int
vtpV7SetResetSoft(int val)
{
  CHECKINIT;

  VLOCK;
  if(val)
    CfgCtrl_Shadow |= VTP_V7BRIDGE_CTRL_RESET_SOFT;
  else
    CfgCtrl_Shadow &= ~VTP_V7BRIDGE_CTRL_RESET_SOFT;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;

  return OK;
}

int
vtpV7GetDone()
{
  int rval = 0;
  CHECKINIT;

  VLOCK;
  if(vtp->v7.Status & VTP_V7BRIDGE_STATUS_DONE)
    rval = 1;
  VUNLOCK;

  return rval;
}

int
vtpV7GetInit_B()
{
  int rval = 0;
  CHECKINIT;

  VLOCK;
  if(vtp->v7.Status & VTP_V7BRIDGE_STATUS_INIT_B)
    rval = 1;
  VUNLOCK;

  return rval;
}

int
vtpV7SetProgram_B(int val)
{
  CHECKINIT;

  VLOCK;
  if(val)
    CfgCtrl_Shadow |= VTP_V7BRIDGE_CTRL_PROGRAM_B;
  else
    CfgCtrl_Shadow &= ~VTP_V7BRIDGE_CTRL_PROGRAM_B;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;
  
  return OK;
}

int
vtpV7SetRDWR_B(int val)
{
  CHECKINIT;

  VLOCK;
  if(val)
    CfgCtrl_Shadow |= VTP_V7BRIDGE_CTRL_RDWR_B;
  else
    CfgCtrl_Shadow &= ~VTP_V7BRIDGE_CTRL_RDWR_B;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;

  return OK;
}

int
vtpV7SetCSI_B(int val)
{
  CHECKINIT;

  VLOCK;
  if(val)
    CfgCtrl_Shadow |= VTP_V7BRIDGE_CTRL_CSI_B;
  else
    CfgCtrl_Shadow &= ~VTP_V7BRIDGE_CTRL_CSI_B;

  vtp->v7.Ctrl = CfgCtrl_Shadow;
  VUNLOCK;

  return OK;
}

void
vtpV7WriteCfgData(uint16_t *buf, int N)
{
  while(N--)
    vtp->v7.Cfg = *buf++;
}

#define V7_CFG_INITB_CNT_MAX	100000
#define V7_CFG_DONE_CNT_MAX	10000000

int
vtpV7CfgStart()
{
  int i, result;
  vtpV7SetCSI_B(1);
  vtpV7SetProgram_B(1);
  vtpV7SetRDWR_B(0);	// Write Mode

  result = vtpV7GetInit_B();
  printf("%s: Init_B = %d, Expected to be 1...%s\r\n",
	 __func__, result, (result == 0) ? "Failed" : "Okay");
  fflush(stdout);

  vtpV7SetProgram_B(0);

  result = vtpV7GetInit_B();
  printf("%s: Init_B = %d, Expected to be 0...%s\r\n",
	 __func__, result, (result == 1) ? "Failed" : "Okay");
  fflush(stdout);

  vtpV7SetProgram_B(1);
  vtpV7SetCSI_B(0);

  for(i = 0; i <= V7_CFG_INITB_CNT_MAX; i++)
    {
      result = vtpV7GetInit_B();

      if(result)
	break;
      else if(i >= V7_CFG_INITB_CNT_MAX)
	{
	  printf("%s: ERROR Init_B assert timeout\r\n", __func__);
	  return ERROR;
	}
    }

  printf("%s: end reached.\r\n", __func__);

  return OK;
}

int
vtpV7CfgLoad(char *filename)
{
  uint16_t buf[256];
  unsigned int bytesRead, i = 0;
  FILE *f;

  vtpLock(); 
  vtpV7CfgStart();

  printf("%s: Opening file: %s...", __func__, filename);
  f = fopen(filename, "rb");
  if(!f)
    {
      printf("FAILED\r\n");
    vtpUnlock();
      return ERROR;
    }
  printf("Opened successfully\r\n");

  while(1)
    {
      bytesRead = fread(&buf[0], 1, sizeof(buf), f);

      if(bytesRead < 0)
	{
	  printf("ERROR: fread() returned %d\r\n", bytesRead);
      vtpUnlock();
	  return ERROR;
	}

      vtpV7WriteCfgData(buf, (bytesRead+1)>>1);
      i+= bytesRead;

      if(feof(f))
	break;
    }

  fclose(f);

  printf("%s: wrote %d bytes\r\n", __func__, i);
  printf("%s: end reached.\r\n", __func__);

  vtpV7CfgEnd();


  vtpUnlock();

  return OK;
}

int
vtpV7CfgEnd()
{
  uint16_t val = 0;
  int result, i;

  for(i = 0; i <= V7_CFG_DONE_CNT_MAX; i++)
    {
      vtpV7WriteCfgData(&val, 1);

      result = vtpV7GetDone();

      if(result)
	break;
      else if(i >= V7_CFG_DONE_CNT_MAX)
	{
	  printf("%s: ERROR Done assert timeout\r\n", __func__);
	  return ERROR;
	}
    }
  for(i = 0; i < 64; i++)
    vtpV7WriteCfgData(&val, 1);

  printf("%s: end reached.\r\n", __func__);

  return OK;
}

int
vtpZ7CfgLoad(char *filename)
{
  long len = 0;
  int fd = 0;
  unsigned char *pBits;
  FILE *f = NULL;

  printf("%s: Opening file: %s...", __func__, filename);
  f = fopen(filename, "rb");
  if(!f)
  {
    printf("failed to open file %s\n", filename);
    vtpUnlock();
    return ERROR;
  }
  printf("Opened successfully\r\n");

  fseek(f, 0, SEEK_END);
  len = ftell(f);
  fseek(f, 0, SEEK_SET);

  pBits = (unsigned char *)malloc(len);
  fread(pBits, 1, len, f);
  fclose(f);

  fd = open("/dev/xdevcfg", O_WRONLY);
  if(fd < 1)
  {
    free(pBits);
    printf("failed to open device\n");
    vtpUnlock();
    return ERROR;
  }
  write(fd, pBits, len);
  close(fd);
  free(pBits);
  
  printf("%s: wrote %ld bytes\r\n", __func__, len);
  printf("%s: end reached.\r\n", __func__);
  
  return OK;
}

/* send_daq_message_to_epics(expid,session,myname,chname,chtype,nelem,data_array) */
int send_daq_message_to_epics(const char *expid, const char *session, const char *myname, const char *caname, const char *catype, int nelem, void *data);

int
vtpSendScalers()
{
  int i, r = OK;
  char host[100];
  CHECKINIT;

  gethostname(host,sizeof(host));
  for(i=0; i<strlen(host); i++)
  {
    if(host[i] == '.')
    {
      host[i] = '\0';
      break;
    }
  }

  vtpLock();
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_ECS:
      r = vtpEcsSendScalers(host);
      break;
    case VTP_FW_TYPE_PCS:
      r = vtpPcsSendScalers(host);
      break;
    case VTP_FW_TYPE_HTCC:
      r = vtpHtccSendScalers(host);
      break;
    case VTP_FW_TYPE_FTOF:
      break;
    case VTP_FW_TYPE_CND:
      r = vtpCndSendScalers(host);
      break;
    case VTP_FW_TYPE_EC:
      break;
    case VTP_FW_TYPE_PC:
      break;
    case VTP_FW_TYPE_FTHODO:
      r = vtpFTHodoSendScalers(host);
      break;
    case VTP_FW_TYPE_GT:
      r = vtpGtSendScalers(host);
      break;
    case VTP_FW_TYPE_DC:
      r = vtpDcSendScalers(host);
      break;
    case VTP_FW_TYPE_HCAL:
      break;
    case VTP_FW_TYPE_FTCAL:
      r = vtpFTSendScalers(host);
      break;
  }
  vtpUnlock();

  return r;
}

int
vtpSendSerdes()
{
  int i, r = OK;
  char host[100];
  char name[100];
  int data[NSERDES+1];
  int vxs_2_vmeslot[16] = {10,13,9,14,8,15,7,16,6,17,5,18,4,19,3,20};
  CHECKINIT;

  gethostname(host,sizeof(host));
  for(i=0; i<strlen(host); i++)
  {
    if(host[i] == '.')
    {
      host[i] = '\0';
      break;
    }
  }

  vtpLock();

  for(i = 0; i < 16; i++)
  {
    sprintf(name, "%s_VTP_SERDES_SLOT%d", host, vxs_2_vmeslot[i]);
    vtpSerdesStatus(VTP_SERDES_VXS, i, 0, data);
    epics_json_msg_send(name, "int", NSERDES, data);
  }

  for(i = 0; i < 4; i++)
  {
    sprintf(name, "%s_VTP_SERDES_QSFP%d", host, i);
    vtpSerdesStatus(VTP_SERDES_QSFP, i, 0, data);
    epics_json_msg_send(name, "int", NSERDES, data);
  }

  vtpUnlock();

  return r;
}

int
vtpWrite32(volatile unsigned int *addr, unsigned int val)
{
  uintptr_t pint = (uintptr_t)vtp + (uintptr_t)addr;
  volatile unsigned int *p = (volatile unsigned int *)pint;
  
  CHECKINIT;
  VLOCK;
    *p = val;
  VUNLOCK;
  
  return OK;
}

unsigned int
vtpRead32(volatile unsigned int *addr)
{
  uintptr_t pint = (uintptr_t)vtp + (uintptr_t)addr;
  volatile unsigned int *p = (volatile unsigned int *)pint;
  unsigned int val;
  
  CHECKINIT;
  VLOCK;
    val = *p;
  VUNLOCK;
  return val;
}

int
vtpEnableTriggerPayloadMask(int pp_mask)
{
  int i;
  CHECKINIT;
  
  VLOCK;
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_ECS:
    case VTP_FW_TYPE_PCS:
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_HTCC:
    case VTP_FW_TYPE_FTOF:
    case VTP_FW_TYPE_CND:
    case VTP_FW_TYPE_FTHODO:
      vtp->v7.fadcDec.Ctrl = pp_mask;
      break;
    case VTP_FW_TYPE_GT:
      vtp->v7.sspDec.Ctrl = pp_mask;
      break;
    case VTP_FW_TYPE_DC:
      vtp->v7.dcrbDec.Ctrl = pp_mask;
      break;
    case VTP_FW_TYPE_HCAL:
      vtp->v7.hcal.Ctrl = pp_mask;
      break;
    case VTP_FW_TYPE_FTCAL:
      vtp->v7.ftcalDec.Ctrl = pp_mask;
      break;
  }
  VUNLOCK;

  for(i = 0; i < 16; i++)
    vtpSerdesEnable(VTP_SERDES_VXS, i, pp_mask & (1<<i));
  
  return OK;
}

int
vtpGetTriggerPayloadMask()
  {
  int pp_mask = 0;
  CHECKINIT;
  
  VLOCK;
  switch(VTP_FW_Type)
    {
    case VTP_FW_TYPE_ECS:
    case VTP_FW_TYPE_PCS:
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_HTCC:
    case VTP_FW_TYPE_FTOF:
    case VTP_FW_TYPE_CND:
    case VTP_FW_TYPE_FTHODO:
      pp_mask = vtp->v7.fadcDec.Ctrl;
      break;
    case VTP_FW_TYPE_GT:
      pp_mask = vtp->v7.sspDec.Ctrl;
      break;
    case VTP_FW_TYPE_DC:
      pp_mask = vtp->v7.dcrbDec.Ctrl;
      break;
    case VTP_FW_TYPE_HCAL:
      pp_mask = vtp->v7.hcal.Ctrl;
      break;
    case VTP_FW_TYPE_FTCAL:
      pp_mask = vtp->v7.ftcalDec.Ctrl;
      break;
    }
  VUNLOCK;
  
  return pp_mask;
}

int
vtpEnableTriggerFiberMask(int fiber_mask)
    {
  int i, mask;
  CHECKINIT;

  VLOCK;
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_FTCAL:
      mask = (vtp->v7.ftcalDec.Ctrl & 0xFFFF) | (fiber_mask<<16);
      vtp->v7.ftcalDec.Ctrl = mask;
      break;
    case VTP_FW_TYPE_DC:
      vtp->v7.dcrbRoadFind.Ctrl = (fiber_mask>>1) & 0x3;
      break;
  }
  VUNLOCK;

  for(i = 0; i < 4; i++)
    vtpSerdesEnable(VTP_SERDES_QSFP, i, fiber_mask & (1<<i));
  
  return OK;
  }
  
int
vtpGetTriggerFiberMask()
{
  int val = 0;
  CHECKINIT;

  VLOCK;
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_FTCAL:
      val = (vtp->v7.ftcalDec.Ctrl>>16) & 0xF;
      break;
    case VTP_FW_TYPE_DC:
      val = (vtp->v7.dcrbRoadFind.Ctrl & 0x3)<<1;
      break;
  }
  VUNLOCK;

  return val;
}

int
vtpSetFPAO(unsigned int val)
{
  CHECKINIT;

  VLOCK;
  vtp->v7.sd.FPAOVal = val;
  VUNLOCK;
  return OK;
}

int
vtpSetFPASel(unsigned int sel)
{
  CHECKINIT;

  VLOCK;
  vtp->v7.sd.FPAOSel = sel;
  VUNLOCK;
  return OK;
}

int
vtpSetFPBO(unsigned int val)
{
  CHECKINIT;

  VLOCK;
  vtp->v7.sd.FPBOVal = val;
  VUNLOCK;
  return OK;
}

int
vtpSetFPBSel(unsigned int sel)
{
  CHECKINIT;

  VLOCK;
  vtp->v7.sd.FPBOSel = sel;
  VUNLOCK;
  return OK;
}

int
vtpSetTrig1Source(int src)
{
  CHECKINIT;
  
  src &= VTP_SD_TRIG1SEL_MASK;
  
  if(src == VTP_SD_TRIG1SEL_0)
    printf("%s: Setting trig1 source to constant 0.\n", __func__);
  else if(src == VTP_SD_TRIG1SEL_1)
    printf("%s: Setting trig1 source to constant 1.\n", __func__);
  else //if(src == VTP_SD_TRIG1SEL_VXS)
    printf("%s: Setting trig1 source to VXS.\n", __func__);
  
  VLOCK;
    vtp->v7.sd.Trig1Sel = src;
  VUNLOCK;

  return OK;
}

int
vtpSetSyncSource(int src)
{
  CHECKINIT;
  
  src &= VTP_SD_SYNCSEL_MASK;
  
  if(src == VTP_SD_SYNCSEL_0)
    printf("%s: Setting sync source to constant 0.\n", __func__);
  else if(src == VTP_SD_SYNCSEL_1)
    printf("%s: Setting sync source to constant 1.\n", __func__);
  else //if(src == VTP_SD_SYNCSEL_VXS)
    printf("%s: Setting sync source to VXS.\n", __func__);
  
  VLOCK;
    vtp->v7.sd.SyncSel = src;
  VUNLOCK;
  
  return OK;
}

int
vtpSetECtrig_dt(int inst, int dt)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  dt = dt / 4;
  if(dt<0)
  {
    printf("%s: ERROR dt too small. Setting to minimum (0).\n", __func__);
    dt = 0;
  }
  else if(dt>8)
  {
    printf("%s: ERROR dt too large. Setting to maximum (8).\n", __func__);
    dt = 8;
  }
  
  VLOCK;
  val = vtp->v7.ecTrigger[inst].Hit;
  val = (val & 0xFFF0FFFF) | (dt<<16);
  vtp->v7.ecTrigger[inst].Hit = val;
  VUNLOCK;
  
  return OK;
}


int
vtpGetECtrig_dt(int inst, int *dt)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
  *dt = ((vtp->v7.ecTrigger[inst].Hit>>16) & 0xF) * 4;
  VUNLOCK;
  
  return OK;
}

int
vtpSetECtrig_emin(int inst, int emin)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
    val = vtp->v7.ecTrigger[inst].Hit;
    val = (val & 0xFFFFE000) | (emin<<0);
    vtp->v7.ecTrigger[inst].Hit = val;
  VUNLOCK;
  
  return OK;
}

int
vtpGetECtrig_emin(int inst, int *emin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
  *emin = vtp->v7.ecTrigger[inst].Hit & 0x1FFF;
  VUNLOCK;
  
  return OK;
}

int
vtpSetECtrig_peak_multmax(int inst, int mult_max)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
    val = vtp->v7.ecTrigger[inst].Hit;
    val = (val & 0xE0FFFFFF) | (mult_max<<24);
    vtp->v7.ecTrigger[inst].Hit = val;
  VUNLOCK;
  
  return OK;
}

int
vtpGetECtrig_peak_multmax(int inst, int *mult_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
  *mult_max = (vtp->v7.ecTrigger[inst].Hit & 0x1F000000)>>24;
  VUNLOCK;
  
  return OK;
}

int
vtpSetECtrig_dalitz(int inst, int min, int max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
    vtp->v7.ecTrigger[inst].Dalitz = (max<<16) | (min<<0);
  VUNLOCK;
  
  return OK;
}

int
vtpGetECtrig_dalitz(int inst, int *min, int *max)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
  val = vtp->v7.ecTrigger[inst].Dalitz;
  *min = (val>>0) & 0x3FF;
  *max = (val>>16) & 0x3FF;
  VUNLOCK;
  
  return OK;
}

int
vtpSetFadcSum_MaskEn(unsigned int mask[16])
{
  int i;
  unsigned int val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_ECS:
    case VTP_FW_TYPE_FTCAL:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type %d\n",__func__, VTP_FW_Type);
      return ERROR;
  }
  
  VLOCK;
  for(i=0; i<8; i++)
  {
    val = mask[2*i+0] & 0xFFFF;
    val|= (mask[2*i+1] & 0xFFFF)<<16;
    vtp->v7.fadcSum.SumEn[i] = val;
  }
  VUNLOCK;
  
  return OK;
}

int
vtpGetFadcSum_MaskEn(unsigned int mask[16])
{
  int i;
  unsigned int val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_ECS:
    case VTP_FW_TYPE_FTCAL:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  for(i=0; i<8;i++)
  {
    val = vtp->v7.fadcSum.SumEn[i];
    mask[2*i+0] = val & 0xFFFF;
    mask[2*i+1] = (val>>16) & 0xFFFF;
  }
  VUNLOCK;
  
  return OK;
}

int
vtpSetECcosmic_emin(int inst, int emin)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.ecCosmic[inst].Ctrl;
  val = (val & ~VTP_ECCOSMIC_CTRL_EMIN_MASK) | emin;
  vtp->v7.ecCosmic[inst].Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetECcosmic_emin(int inst, int *emin)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *emin = vtp->v7.ecCosmic[inst].Ctrl & VTP_ECCOSMIC_CTRL_EMIN_MASK;
  VUNLOCK;

  return OK;
}

int
vtpSetECcosmic_multmax(int inst, int multmax)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.ecCosmic[inst].Ctrl;
  val = (val & ~VTP_ECCOSMIC_CTRL_MULTMAX_MASK) | (multmax<<24);
  vtp->v7.ecCosmic[inst].Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetECcosmic_multmax(int inst, int *multmax)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *multmax = (vtp->v7.ecCosmic[inst].Ctrl & VTP_ECCOSMIC_CTRL_MULTMAX_MASK)>>24;
  VUNLOCK;

  return OK;
}

int
vtpSetECcosmic_width(int inst, int hitwidth)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.ecCosmic[inst].Delay;
  val = (val & ~VTP_ECCOSMIC_DELAY_WIDTH_MASK) | (hitwidth<<0);
  vtp->v7.ecCosmic[inst].Delay = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetECcosmic_width(int inst, int *hitwidth)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *hitwidth = (vtp->v7.ecCosmic[inst].Delay & VTP_ECCOSMIC_DELAY_WIDTH_MASK)>>0;
  VUNLOCK;

  return OK;
}

int
vtpSetECcosmic_delay(int inst, int evaldelay)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.ecCosmic[inst].Delay;
  val = (val & ~VTP_ECCOSMIC_DELAY_EVAL_MASK) | (evaldelay<<16);
  vtp->v7.ecCosmic[inst].Delay = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetECcosmic_delay(int inst, int *evaldelay)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_ECS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *evaldelay = (vtp->v7.ecCosmic[inst].Delay & VTP_ECCOSMIC_DELAY_EVAL_MASK)>>16;
  VUNLOCK;

  return OK;
}

int
vtpSetPCcosmic_emin(int emin)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }

  VLOCK;
  val = vtp->v7.pcCosmic.Ctrl;
  val = (val & ~VTP_PCCOSMIC_CTRL_EMIN_MASK) | emin;
  vtp->v7.pcCosmic.Ctrl = val;
  VUNLOCK;

  return OK;

}

int
vtpGetPCcosmic_emin(int *emin)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *emin = vtp->v7.pcCosmic.Ctrl & VTP_PCCOSMIC_CTRL_EMIN_MASK;
  VUNLOCK;

  return OK;
}

int
vtpSetPCcosmic_multmax(int multmax)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.pcCosmic.Ctrl;
  val = (val & ~VTP_PCCOSMIC_CTRL_MULTMAX_MASK) | (multmax<<24);
  vtp->v7.pcCosmic.Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetPCcosmic_multmax(int *multmax)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *multmax = (vtp->v7.pcCosmic.Ctrl & VTP_PCCOSMIC_CTRL_MULTMAX_MASK)>>24;
  VUNLOCK;

  return OK;
}

int
vtpSetPCcosmic_width(int hitwidth)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.pcCosmic.Delay;
  val = (val & ~VTP_PCCOSMIC_DELAY_WIDTH_MASK) | (hitwidth<<0);
  vtp->v7.pcCosmic.Delay = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetPCcosmic_width(int *hitwidth)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *hitwidth = (vtp->v7.pcCosmic.Delay & VTP_PCCOSMIC_DELAY_WIDTH_MASK)>>0;
  VUNLOCK;

  return OK;
}

int
vtpSetPCcosmic_delay(int evaldelay)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.pcCosmic.Delay;
  val = (val & ~VTP_PCCOSMIC_DELAY_EVAL_MASK) | (evaldelay<<16);
  vtp->v7.pcCosmic.Delay = val;
  VUNLOCK;

  return OK;

}

int
vtpGetPCcosmic_delay(int *evaldelay)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *evaldelay = (vtp->v7.pcCosmic.Delay & VTP_PCCOSMIC_DELAY_EVAL_MASK)>>16;
  VUNLOCK;

  return OK;
}



int
vtpSetPCcosmic_pixel(int enable)
{
  uint32_t val;
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  if(enable)
    enable = 1;
  
  VLOCK;
  val = vtp->v7.pcCosmic.Ctrl;
  val = (val & ~VTP_PCCOSMIC_CTRL_PIXEL_MASK) | (enable<<16);
  vtp->v7.pcCosmic.Ctrl = val;
  VUNLOCK;
  
printf("%s(%d): 0x%08X, 0x%08X\n", __func__, enable, val, vtp->v7.pcCosmic.Ctrl);
  return OK;

}

int
vtpGetPCcosmic_pixel(int *enable)
{
  CHECKINIT;
  /*CHECKTYPE();*/
  switch(VTP_FW_Type)
  {
    case VTP_FW_TYPE_PC:
    case VTP_FW_TYPE_PCS:
      break;
    default:
      printf("%s: ERROR: VTP wrong firmware type\n",__func__);
      return ERROR;
  }
  
  VLOCK;
  *enable = (vtp->v7.pcCosmic.Ctrl & VTP_PCCOSMIC_CTRL_PIXEL_MASK)>>16;
  VUNLOCK;

  return OK;
}

int
vtpSetFTCALseed_emin(int emin)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  val = vtp->v7.ftcalTrigger.Ctrl;
  val = (val & ~VTP_FTCAL_CTRL_SEEDTHR_MASK) | (emin<<0);
  vtp->v7.ftcalTrigger.Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTCALseed_emin(int *emin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  *emin = (vtp->v7.ftcalTrigger.Ctrl & VTP_FTCAL_CTRL_SEEDTHR_MASK)>>0;
  VUNLOCK;

  return OK;
}


int
vtpSetFTCALseed_dt(int dt)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  dt = dt/4;
  val = vtp->v7.ftcalTrigger.Ctrl;
  val = (val & ~VTP_FTCAL_CTRL_SEEDDT_MASK) | ((dt&0x7)<<16);
  vtp->v7.ftcalTrigger.Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTCALseed_dt(int *dt)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  *dt = ((vtp->v7.ftcalTrigger.Ctrl & VTP_FTCAL_CTRL_SEEDDT_MASK)>>16)*4;
  VUNLOCK;

  return OK;
}

int
vtpSetFTCALhodo_dt(int dt)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  dt = dt/4;
  val = vtp->v7.ftcalTrigger.Ctrl;
  val = (val & ~VTP_FTCAL_CTRL_HODODT_MASK) | ((dt&0x7)<<24);
  vtp->v7.ftcalTrigger.Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTCALhodo_dt(int *dt)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  *dt = ((vtp->v7.ftcalTrigger.Ctrl & VTP_FTCAL_CTRL_HODODT_MASK)>>24)*4;
  VUNLOCK;

  return OK;
}


int
vtpSetFTHODOemin(int emin)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTHODO);
  
  VLOCK;
  val = vtp->v7.fthodoTrigger.Ctrl;
  val = (emin & VTP_FTHODO_CTRL_EMIN_MASK)<<0;
  vtp->v7.fthodoTrigger.Ctrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTHODOemin(int *emin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTHODO);
  
  VLOCK;
  *emin = (vtp->v7.fthodoTrigger.Ctrl & VTP_FTHODO_CTRL_EMIN_MASK)>>0;
  VUNLOCK;

  return OK;
}

int
vtpSetFTCALcluster_deadtime(int deadtime)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  deadtime = deadtime/4;
  val = vtp->v7.ftcalTrigger.DeadtimeCtrl;
  val = (val & ~VTP_FTCAL_DEADTIMECTRL_DEADTIME_MASK) | ((deadtime&0x3f)<<16);
  vtp->v7.ftcalTrigger.DeadtimeCtrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTCALcluster_deadtime(int *deadtime)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  *deadtime = ((vtp->v7.ftcalTrigger.DeadtimeCtrl & VTP_FTCAL_DEADTIMECTRL_DEADTIME_MASK)>>16)*4;
  VUNLOCK;

  return OK;
}

int
vtpSetFTCALcluster_deadtime_emin(int emin)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  val = vtp->v7.ftcalTrigger.DeadtimeCtrl;
  val = (emin & VTP_FTCAL_DEADTIMECTRL_EMIN_MASK)<<0;
  vtp->v7.ftcalTrigger.DeadtimeCtrl = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetFTCALcluster_deadtime_emin(int *emin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTCAL);
  
  VLOCK;
  *emin = (vtp->v7.ftcalTrigger.DeadtimeCtrl & VTP_FTCAL_DEADTIMECTRL_EMIN_MASK)>>0;
  VUNLOCK;

  return OK;
}

int
vtpFTSendScalers(char *host)
{
  char name[100];
  float ref, data[1024];
  unsigned int val;
  int i;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
#if VTP_FT_SENDHODOSCALERS
  vtp->v7.sd.ScalerLatch = 1;
  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  for(i=0;i<256;i++)
  {
    data[i] = ref * (float)vtp->v7.fthodoScalers.Scalers[i];
    printf("vtp->v7.fthodoScalers.Scalers[%3d]=%9d\n", i, vtp->v7.fthodoScalers.Scalers[i]);
  }
  vtp->v7.sd.ScalerLatch = 0;
  sprintf(name, "%s_VTPFT_HODOSCALERS", host);
  epics_json_msg_send(name, "float", 256, data);
#endif

  vtp->v7.ftcalTrigger.HistCtrl = 0x60000000;
  printf("Start - Setting ftcalTrigger.HistCtrl, read back 0x%08X\n",
    vtp->v7.ftcalTrigger.HistCtrl);
  val = vtp->v7.ftcalTrigger.HistTime;
  if(!val) val = 1;

  ref = 1000000.0f / (float)val;

  // Cluster position histogram (all)
  for(i=0;i<1024;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistPos;
  sprintf(name, "%s_VTPFT_CLUSTERPOSITION", host);
  epics_json_msg_send(name, "float", 1024, data);

  // Cluster energy histogram (all)
  for(i=0;i<1024;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistEnergy;
  sprintf(name, "%s_VTPFT_CLUSTERENERGY", host);
  epics_json_msg_send(name, "float", 1024, data);

  // Cluster nhits histogram (all)
  for(i=0;i<9;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistNHits;
  sprintf(name, "%s_VTPFT_CLUSTERHITS", host);
  epics_json_msg_send(name, "float", 9, data);

  // Cluster position histogram (hodo tagged)
  for(i=0;i<1024;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistPosHodo;
  sprintf(name, "%s_VTPFT_CLUSTERPOSITION_HODO", host);
  epics_json_msg_send(name, "float", 1024, data);

  // Cluster energy histogram (hodo tagged)
  for(i=0;i<1024;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistEnergyHodo;
  sprintf(name, "%s_VTPFT_CLUSTERENERGY_HODO", host);
  epics_json_msg_send(name, "float", 1024, data);

  // Cluster nhits histogram (hodo tagged)
  for(i=0;i<9;i++)
    data[i] = ref * (float)vtp->v7.ftcalTrigger.HistNHitsHodo;
  sprintf(name, "%s_VTPFT_CLUSTERNHITS_HODO", host);
  epics_json_msg_send(name, "float", 9, data);

  vtp->v7.ftcalTrigger.HistCtrl = 0x60000000 | 0x7F;
  printf("Stop - Setting ftcalTrigger.HistCtrl, read back 0x%08X\n",
    vtp->v7.ftcalTrigger.HistCtrl);
  VUNLOCK;

  return OK;
}

int
vtpFTHodoSendScalers(char *host)
{
  char name[100];
  float ref, data[1024];
  unsigned int val;
  int i;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
#if VTP_FT_SENDHODOSCALERS
  vtp->v7.sd.ScalerLatch = 1;
  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  for(i=0;i<256;i++)
    data[i] = ref * (float)vtp->v7.fthodoScalers.Scalers[i];
  vtp->v7.sd.ScalerLatch = 0;
  sprintf(name, "%s_VTPFT_HODOSCALERS", host);
  epics_json_msg_send(name, "float", 256, data);
#endif
  VUNLOCK;

  return OK;
}
/*
 *
 sel = 0: clusters, no hodo tag
       1: clusters, l1 hodo tag
       2: clusters, l2 hodo tag
       3: clusters, l1*l2 hodo tag
       4: l1 hodo hits (tagged cal pixels)
       5: l2 hodo hits (tagged cal pixels)
 *
 */

int
vtpFTSelectHist(int sel)
{
  VLOCK;
  vtp->v7.ftcalTrigger.HistCtrl &= 0x1FFFFFFF | (sel<<29);
  VUNLOCK;

  return OK;
}

/* HTCC functions */

int
vtpSetHTCC_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  vtp->v7.htccTrigger.Thresholds[0] = thr0;
  vtp->v7.htccTrigger.Thresholds[1] = thr1;
  vtp->v7.htccTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetHTCC_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  *thr0 = vtp->v7.htccTrigger.Thresholds[0];
  *thr1 = vtp->v7.htccTrigger.Thresholds[1];
  *thr2 = vtp->v7.htccTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetHTCC_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  vtp->v7.htccTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetHTCC_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  *nframes = vtp->v7.htccTrigger.NFrames;
  VUNLOCK;

  return OK;
}

/* CTOF functions */
int
vtpSetCTOF_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  vtp->v7.ctofTrigger.Thresholds[0] = thr0;
  vtp->v7.ctofTrigger.Thresholds[1] = thr1;
  vtp->v7.ctofTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetCTOF_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  *thr0 = vtp->v7.ctofTrigger.Thresholds[0];
  *thr1 = vtp->v7.ctofTrigger.Thresholds[1];
  *thr2 = vtp->v7.ctofTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetCTOF_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  vtp->v7.ctofTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetCTOF_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
  
  VLOCK;
  *nframes = vtp->v7.ctofTrigger.NFrames;
  VUNLOCK;

  return OK;
}

int
vtpHtccPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[3];
  const char *scalers_name[3] = {
    "BusClk",
    "HTCCHit",
    "CTOFHit"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HTCC);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  scalers[1] = vtp->v7.htccTrigger.ScalerHit;
  scalers[2] = vtp->v7.ctofTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i=0; i<3; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpHtccSendScalers(char *host)
{
  char name[100];
  float ref, data[2];
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  data[0] = ref * (float)vtp->v7.htccTrigger.ScalerHit;
  data[1] = ref * (float)vtp->v7.ctofTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPHTCC_CLUSTERS", host);
  epics_json_msg_send(name, "float", 1, data);

  sprintf(name, "%s_VTPCTOF_CLUSTERS", host);
  epics_json_msg_send(name, "float", 1, data);

  return OK;
}








/* FTOF functions */

int
vtpSetFTOF_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTOF);
  
  VLOCK;
  vtp->v7.ftofTrigger.Thresholds[0] = thr0;
  vtp->v7.ftofTrigger.Thresholds[1] = thr1;
  vtp->v7.ftofTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetFTOF_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTOF);
  
  VLOCK;
  *thr0 = vtp->v7.ftofTrigger.Thresholds[0];
  *thr1 = vtp->v7.ftofTrigger.Thresholds[1];
  *thr2 = vtp->v7.ftofTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetFTOF_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTOF);
  
  VLOCK;
  vtp->v7.ftofTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetFTOF_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTOF);
  
  VLOCK;
  *nframes = vtp->v7.ftofTrigger.NFrames;
  VUNLOCK;

  return OK;
}

int
vtpFtofPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[2];
  const char *scalers_name[2] = {
    "BusClk",
    "Hit"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_FTOF);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  scalers[1] = vtp->v7.ftofTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i=0; i<2; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpFtofSendScalers(char *host)
{
  char name[100];
  float ref, data[1];
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  data[0] = ref * (float)vtp->v7.ftofTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPFTOF_CLUSTERS", host);
  epics_json_msg_send(name, "float", 1, data);

  return OK;
}






/* CND functions */

int
vtpSetCND_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_CND);
  
  VLOCK;
  vtp->v7.cndTrigger.Thresholds[0] = thr0;
  vtp->v7.cndTrigger.Thresholds[1] = thr1;
  vtp->v7.cndTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetCND_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_CND);
  
  VLOCK;
  *thr0 = vtp->v7.cndTrigger.Thresholds[0];
  *thr1 = vtp->v7.cndTrigger.Thresholds[1];
  *thr2 = vtp->v7.cndTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetCND_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_CND);
  
  VLOCK;
  vtp->v7.cndTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetCND_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_CND);
  
  VLOCK;
  *nframes = vtp->v7.cndTrigger.NFrames;
  VUNLOCK;

  return OK;
}

int
vtpCndPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[2];
  const char *scalers_name[2] = {
    "BusClk",
    "Hit"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_CND);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  scalers[1] = vtp->v7.cndTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i=0; i<2; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpCndSendScalers(char *host)
{
  char name[100];
  float ref, data[1];
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  data[0] = ref * (float)vtp->v7.cndTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPCND_CLUSTERS", host);
  epics_json_msg_send(name, "float", 1, data);

  return OK;
}














/* PCS functions */

int
vtpSetPCS_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  vtp->v7.pcsTrigger.Thresholds[0] = thr0;
  vtp->v7.pcsTrigger.Thresholds[1] = thr1;
  vtp->v7.pcsTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetPCS_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  *thr0 = vtp->v7.pcsTrigger.Thresholds[0];
  *thr1 = vtp->v7.pcsTrigger.Thresholds[1];
  *thr2 = vtp->v7.pcsTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetPCS_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  vtp->v7.pcsTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetPCS_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  *nframes = vtp->v7.pcsTrigger.NFrames;
  VUNLOCK;

  return OK;
}

int
vtpSetPCS_dipfactor(int dipfactor)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  vtp->v7.pcsTrigger.Dipfactor = dipfactor;
  VUNLOCK;

  return OK;
}

int
vtpGetPCS_dipfactor(int *dipfactor)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  *dipfactor = vtp->v7.pcsTrigger.Dipfactor;
  VUNLOCK;

  return OK;
}

int
vtpSetPCS_nstrip(int nstripmin, int nstripmax)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  /* nstripmin is not implemented */
  VLOCK;
  vtp->v7.pcsTrigger.NstripMax = nstripmax;
  VUNLOCK;

  return OK;
}

int
vtpGetPCS_nstrip(int *nstripmin, int *nstripmax)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  /* nstripmin is not implemented */
  *nstripmax = 0;

  VLOCK;
  *nstripmax = vtp->v7.pcsTrigger.NstripMax;
  VUNLOCK;

  return OK;
}

int
vtpSetPCS_dalitz(int dalitz_min, int dalitz_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  vtp->v7.pcsTrigger.DalitzMin = dalitz_min;
  vtp->v7.pcsTrigger.DalitzMax = dalitz_max;
  VUNLOCK;

  return OK;
}

int
vtpGetPCS_dalitz(int *dalitz_min, int *dalitz_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  *dalitz_min = vtp->v7.pcsTrigger.DalitzMin;
  *dalitz_max = vtp->v7.pcsTrigger.DalitzMax;
  VUNLOCK;

  return OK;
}

int
vtpPcsSendScalers(char *host)
{
  char name[100];
  float ref, data[4], pcudata[1];
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  data[0] = ref * (float)vtp->v7.pcsTrigger.ScalerPeakU;
  data[1] = ref * (float)vtp->v7.pcsTrigger.ScalerPeakV;
  data[2] = ref * (float)vtp->v7.pcsTrigger.ScalerPeakW;
  data[3] = ref * (float)vtp->v7.pcsTrigger.ScalerHit;

  pcudata[0] = ref * (float)vtp->v7.pcuTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPPCS_CLUSTERS", host);
  epics_json_msg_send(name, "float", 4, data);

  sprintf(name, "%s_VTPPCU_SCALER", host);
  epics_json_msg_send(name, "float", 1, pcudata);

  return OK;
}

int
vtpSetPCU_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  vtp->v7.pcuTrigger.Thresholds[0] = thr0;
  vtp->v7.pcuTrigger.Thresholds[1] = thr1;
  vtp->v7.pcuTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetPCU_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
  
  VLOCK;
  *thr0 = vtp->v7.pcuTrigger.Thresholds[0];
  *thr1 = vtp->v7.pcuTrigger.Thresholds[1];
  *thr2 = vtp->v7.pcuTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

/* ECS functions */


int
vtpSetECS_thresholds(int thr0, int thr1, int thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  vtp->v7.ecsTrigger.Thresholds[0] = thr0;
  vtp->v7.ecsTrigger.Thresholds[1] = thr1;
  vtp->v7.ecsTrigger.Thresholds[2] = thr2;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_thresholds(int *thr0, int *thr1, int *thr2)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  *thr0 = vtp->v7.ecsTrigger.Thresholds[0];
  *thr1 = vtp->v7.ecsTrigger.Thresholds[1];
  *thr2 = vtp->v7.ecsTrigger.Thresholds[2];
  VUNLOCK;

  return OK;
}

int
vtpSetECS_nframes(int nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  vtp->v7.ecsTrigger.NFrames = nframes;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_nframes(int *nframes)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  *nframes = vtp->v7.ecsTrigger.NFrames;
  VUNLOCK;

  return OK;
}

int
vtpSetECS_dipfactor(int dipfactor)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  vtp->v7.ecsTrigger.Dipfactor = dipfactor;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_dipfactor(int *dipfactor)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  *dipfactor = vtp->v7.ecsTrigger.Dipfactor;
  VUNLOCK;

  return OK;
}

int
vtpSetECS_nstrip(int nstripmin, int nstripmax)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  /* nstripmin is not implemented */
  VLOCK;
  vtp->v7.ecsTrigger.NstripMax = nstripmax;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_nstrip(int *nstripmin, int *nstripmax)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  /* nstripmin is not implemented */
  *nstripmax = 0;

  VLOCK;
  *nstripmax = vtp->v7.ecsTrigger.NstripMax;
  VUNLOCK;

  return OK;
}

int
vtpSetECS_dalitz(int dalitz_min, int dalitz_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  vtp->v7.ecsTrigger.DalitzMin = dalitz_min<<3;
  vtp->v7.ecsTrigger.DalitzMax = dalitz_max<<3;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_dalitz(int *dalitz_min, int *dalitz_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  *dalitz_min = vtp->v7.ecsTrigger.DalitzMin>>3;
  *dalitz_max = vtp->v7.ecsTrigger.DalitzMax>>3;
  VUNLOCK;

  return OK;
}


int
vtpEcsPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[5];
  const char *scalers_name[5] = {
    "BusClk",
    "PeakU",
    "PeakV",
    "PeakW",
    "Hit"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  scalers[1] = vtp->v7.ecsTrigger.ScalerPeakU;
  scalers[2] = vtp->v7.ecsTrigger.ScalerPeakV;
  scalers[3] = vtp->v7.ecsTrigger.ScalerPeakW;
  scalers[4] = vtp->v7.ecsTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i = 0; i < 5; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpEcsSendScalers(char *host)
{
  char name[100];
  float ref, data[4];
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  data[0] = ref * (float)vtp->v7.ecsTrigger.ScalerPeakU;
  data[1] = ref * (float)vtp->v7.ecsTrigger.ScalerPeakV;
  data[2] = ref * (float)vtp->v7.ecsTrigger.ScalerPeakW;
  data[3] = ref * (float)vtp->v7.ecsTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPECS_CLUSTERS", host);
  epics_json_msg_send(name, "float", 4, data);

  return OK;
}

/**********************/


int
vtpSetPCScosmic_pixel(int enable)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PC);
  
  if(enable)
    enable = 1;
  
  VLOCK;
  val = vtp->v7.pcCosmic.Ctrl;
  val = (val & ~VTP_PCCOSMIC_CTRL_PIXEL_MASK) | (enable<<16);
  vtp->v7.pcCosmic.Delay = val;
  VUNLOCK;
  
  return OK;

}

int
vtpGetPCScosmic_pixel(int *enable)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PC);
  
  VLOCK;
  *enable = (vtp->v7.pcCosmic.Ctrl & VTP_PCCOSMIC_CTRL_PIXEL_MASK)>>16;
  VUNLOCK;

  return OK;
}




int
vtpSetDc_SegmentThresholdMin(int inst, int threshold)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_DC);

  if(inst < 0 || inst > 1)
  {
    printf("%s: ERROR - invalid instance %d\n", __func__, inst);
    return ERROR;
  }
  
  VLOCK;
  vtp->v7.dcrbSegFind[inst].Ctrl = threshold;
  VUNLOCK;
  
  return OK;
}

int
vtpGetDc_SegmentThresholdMin(int inst, int *threshold)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_DC);
  
  if(inst < 0 || inst > 1)
  {
    printf("%s: ERROR - invalid instance %d\n", __func__, inst);
    return ERROR;
  }
  
  VLOCK;
  *threshold = vtp->v7.dcrbSegFind[inst].Ctrl;
  VUNLOCK;
  
  return OK;
}

int
vtpSetGt_latency(int latency)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  VLOCK;
  vtp->v7.trigOut.Latency = latency/4;
  VUNLOCK;
  
  return OK;
}

int
vtpGetGt_latency()
{
  int latency;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  VLOCK;
  latency = vtp->v7.trigOut.Latency;
  VUNLOCK;
  
  return latency*4;
}

int
vtpSetGt_width(int width)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  VLOCK;
  vtp->v7.trigOut.Width = width;
  VUNLOCK;
  
  return OK;
}

int
vtpGetGt_width()
{
  int width;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  VLOCK;
  width = vtp->v7.trigOut.Width;
  VUNLOCK;
  
  return width;
}

int
vtpPrintGtTriggerBitRegs()
{
  int strig, strigmask, ctrig, pulser, i;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);

  for(i=0; i<32; i++)
  {
    VLOCK;
    strig = vtp->v7.gtBit[i].STrigger;
    strigmask= vtp->v7.gtBit[i].STriggerMask;
    ctrig = vtp->v7.gtBit[i].CTrigger;
    pulser = vtp->v7.gtBit[i].Pulser;
    VUNLOCK;
    printf("Bit %d: STrigger = 0x%08X, STriggerMask = 0x%08X, CTrigger = 0x%08X, Pulser = 0x%08X\n", i, strig, strigmask, ctrig, pulser);
  }
  
  return OK;  
}

int
vtpSetGtTriggerBit(int inst, int strigger_mask, int sector_mask, int mult_min, int coin_width, int ctrigger_mask, int delay, float pulser_freq)
{
  float f;
  int strig, strigmask, ctrig, pulser;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  if(inst < 0 || inst > 32)
  {
    printf("%s: ERROR - invalid trigger bit %d\n", __func__, inst);
    return ERROR;
  }

  ctrig = ((ctrigger_mask & 0xF) <<0);  

  strig = ((mult_min      & 0x7) <<4) |
          ((sector_mask   & 0x3F)<<8) |
          ((coin_width    & 0xFF)<<16) |
          ((delay         & 0xFF)<<24);

  strigmask = ((strigger_mask & 0xFFFF) <<0);

  // convert freq to 250MHz period ticks
  if(pulser_freq > 0.0f)
  {
    f = 250000000.0f / pulser_freq;
    pulser = 0x80000000 | (int)f;
  }
  else
    pulser = 0x00000000;

  VLOCK;
  vtp->v7.gtBit[inst].STrigger = strig;
  vtp->v7.gtBit[inst].CTrigger = ctrig;
  vtp->v7.gtBit[inst].Pulser = pulser;
  vtp->v7.gtBit[inst].STriggerMask = strigmask;
  VUNLOCK;
  
  return OK;  
}

int
vtpGetGtTriggerBit(int inst, int *strigger_mask, int *sector_mask, int *mult_min, int *coin_width, int *ctrigger_mask, int *delay, float *pulser_freq)
{
  int strig, strigmask, ctrig, pulser;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  if(inst < 0 || inst > 32)
  {
    printf("%s: ERROR - invalid trigger bit %d\n", __func__, inst);
    return ERROR;
  }
  
  VLOCK;
  strig = vtp->v7.gtBit[inst].STrigger;
  ctrig = vtp->v7.gtBit[inst].CTrigger;
  pulser = vtp->v7.gtBit[inst].Pulser;
  strigmask = vtp->v7.gtBit[inst].STriggerMask;
  VUNLOCK;

  *mult_min      = (strig>>4)&0x7;
  *sector_mask   = (strig>>8)&0x3F;
  *coin_width    = (strig>>16)&0xFF;
  *delay         = (strig>>24)&0xFF;

  *strigger_mask = (strigmask>>0)&0xFFFF;

  *ctrigger_mask = (ctrig>>0)&0xF;

  // convert 250MHz period ticks to freq
  if(pulser & 0x80000000)
  {
    pulser &= 0x7FFFFFFF;
    *pulser_freq = ((float)pulser) / 250000000.0f;
  }
  else
    *pulser_freq = 0.0f;

  return OK;  
}

int
vtpGtSendScalers(char *host)
{
  char name[100];
  float ref, data[32];
  unsigned int val;
  int i;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  //Trigger bit scalers
  for(i=0; i<32; i++)
    data[i] = ref * (float)vtp->v7.gtBit[i].TriggerScaler;
  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPGT_TRIGGERBITS", host);
  epics_json_msg_send(name, "float", 32, data);

  return OK;
}

int
vtpGtPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int gtscalers[36];
  const char *scalers_name[36] = {
    "BusClk",
    "Sync",
    "Trig1",
    "Trig2",
    "Trigger0",
    "Trigger1",
    "Trigger2",
    "Trigger3",
    "Trigger4",
    "Trigger5",
    "Trigger6",
    "Trigger7",
    "Trigger8",
    "Trigger9",
    "Trigger10",
    "Trigger11",
    "Trigger12",
    "Trigger13",
    "Trigger14",
    "Trigger15",
    "Trigger16",
    "Trigger17",
    "Trigger18",
    "Trigger19",
    "Trigger20",
    "Trigger21",
    "Trigger22",
    "Trigger23",
    "Trigger24",
    "Trigger25",
    "Trigger26",
    "Trigger27",
    "Trigger28",
    "Trigger29",
    "Trigger30",
    "Trigger31"
    };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
 
  
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;
  gtscalers[0] = vtp->v7.sd.Scaler_BusClk;
  gtscalers[1] = vtp->v7.sd.Scaler_Sync;
  gtscalers[2] = vtp->v7.sd.Scaler_Trig1;
  gtscalers[3] = vtp->v7.sd.Scaler_Trig2;
  for(i=0; i<32; i++)
    gtscalers[4+i] = vtp->v7.gtBit[i].TriggerScaler;
//    gtscalers[4+i] = vtp->v7.sd.Scaler_Trigger[i];
  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!gtscalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)gtscalers[0] / (double)33330000;
  } 

  for(i = 0; i < 36; i++) 
  { 
    rate = (double)gtscalers[i]; 
    rate = rate / ref; 
    if(gtscalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], gtscalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], gtscalers[i], rate); 
  }
  return OK;
}

int
vtpPcsPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[6];
  const char *scalers_name[6] = {
    "BusClk",
    "PeakU",
    "PeakV",
    "PeakW",
    "Hit",
    "Pcu"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PCS);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  scalers[1] = vtp->v7.pcsTrigger.ScalerPeakU;
  scalers[2] = vtp->v7.pcsTrigger.ScalerPeakV;
  scalers[3] = vtp->v7.pcsTrigger.ScalerPeakW;
  scalers[4] = vtp->v7.pcsTrigger.ScalerHit;
  scalers[5] = vtp->v7.pcuTrigger.ScalerHit;

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i = 0; i < 6; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpPrintHist_PeakPosition(int inst)
{
  uint32_t val;
  float hist_u[36], hist_v[36], hist_w[36];
  float tot_u = 0.0f, tot_v = 0.0f, tot_w = 0.0f;
  float scale, fval;
  int i;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
    vtp->v7.ecTrigger[inst].HistCtrl &= ~0x00000003;
    val = vtp->v7.ecTrigger[inst].HistTime;
    scale = (float)val;
    if(scale != 0)
    {
      scale = scale * 256.0f / 250.0E6;
      scale = 1.0f / scale;
    }
    else
    {
      scale = 1.0f;
      printf("%s: Error - normalization invalid. Raw counts will be displayed.\n", __func__);
    }
    
    for(i=0;i<256;i++)
    {
      val = vtp->v7.ecTrigger[inst].HistPeakPosition;
      fval = scale * (float)val;
      if(i>=0 && i<=35)
      {
        hist_u[i-0] = fval;
        tot_u+= fval;
      }
      else if(i>=64 && i<=99)
      {
        hist_v[i-64] = fval;
        tot_v+= fval;
      }
      else if(i>=128 && i<=163)
      {
        hist_w[i-128] = fval;
        tot_w+= fval;
      }
    }
    vtp->v7.ecTrigger[inst].HistCtrl |= 0x00000003;
  VUNLOCK;

  printf("ecTrig Peak Position Histogram(u,v,w):\n");
  for(i=0;i<36;i++)
    printf("strip %2d: %9.2f, %9.2f, %9.2f\n", i, hist_u[i], hist_v[i], hist_w[i]);
  printf("    total: %9.2f, %9.2f, %9.2f\n", tot_u, tot_v, tot_w);
  
  return OK;
}

int
vtpPrintHist_ClusterPosition(int inst)
{
  float hist_uv[36][36];
  float tot=0;
  float scale, fval;
  uint32_t val;
  int i,u,v;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
  VLOCK;
    vtp->v7.ecTrigger[inst].HistCtrl &= ~0x00000005;
    val = vtp->v7.ecTrigger[inst].HistTime;
    scale = (float)val;
    if(scale != 0)
    {
      scale = scale * 256.0f / 250.0E6;
      scale = 1.0f / scale;
    }
    else
    {
      scale = 1.0f;
      printf("%s: Error - normalization invalid. Raw counts will be displayed.\n", __func__);
    }

    for(i=0;i<4096;i++)
    {
      val = vtp->v7.ecTrigger[inst].HistClusterPosition;
      fval = scale * (float)val;
      
      tot+= fval;
      u = i%64;
      v = i/64;
      if(u<36 && v<36)
        hist_uv[u][v] = fval;
    }
    vtp->v7.ecTrigger[inst].HistCtrl |= 0x00000005;
  VUNLOCK;

  printf("ecTrig Cluster Position Histogram:\n");
  for(u=0;u<36;u++)
  {
    for(v=0;v<36;v++)
      printf("%4.0f ", hist_uv[u][v]);
    printf("\n");
  }
  printf("total = %f\n", tot);
  
  return OK;
}

int
vtpSetHcal_ClusterCoincidence(int coin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HCAL);
  
  if(coin > 7)
  {
    printf("%s: Error - invalid coincidence specified %dns\n", __func__, coin);
    coin = 7;
  }
  
  VLOCK;
  vtp->v7.hcal.ClusterPulseCoincidence = coin/4;
  VUNLOCK;
  
  return OK;
}

int
vtpGetHcal_ClusterCoincidence(int *coin)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HCAL);
  
  VLOCK;
  *coin = vtp->v7.hcal.ClusterPulseCoincidence * 4;
  VUNLOCK;
  
  return OK;
}

int
vtpSetHcal_ClusterThreshold(int thr)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HCAL);
  
  if(thr > 8191)
  {
    printf("%s: Error - invalid threshold specified %dns\n", __func__, thr);
    thr = 8191;
  }
  
  VLOCK;
  vtp->v7.hcal.ClusterPulseThreshold = thr;
  VUNLOCK;
  
  return OK;
}

int
vtpGetHcal_ClusterThreshold(int *thr)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_HCAL);
  
  VLOCK;
  *thr = vtp->v7.hcal.ClusterPulseThreshold;
  VUNLOCK;
  
  return OK;
}


int
vtpDcPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int scalers[7];
  const char *scalers_name[7] = {
    "BusClk",
    "SL1",
    "SL2",
    "SL3",
    "SL4",
    "SL5",
    "SL6"
   };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_DC);
 
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  scalers[0] = vtp->v7.sd.Scaler_BusClk;
  for(i=0;i<6;i++)
    scalers[i+1] = vtp->v7.dcrbRoadFind.Scalers[i];

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

 
  printf("%s - \n", __FUNCTION__); 
  if(!scalers[0]) 
  {
    printf("Error: %s reference time is 0. Reported rates will not be normalized.\n", __func__); 
    ref = 1.0; 
  } 
  else 
  { 
    ref = (double)scalers[0] / (double)33330000;
  } 

  for(i=0; i<7; i++) 
  { 
    rate = (double)scalers[i]; 
    rate = rate / ref; 
    if(scalers[i] == 0xFFFFFFFF) 
     printf("   %-25s %10u,%.3fHz [OVERFLOW]\n", scalers_name[i], scalers[i], rate); 
    else 
     printf("   %-25s %10u,%.3fHz\n", scalers_name[i], scalers[i], rate); 
  }
  return OK;
}

int
vtpDcSendScalers(char *host)
{
  char name[100];
  float ref, data[6];
  int i;
  unsigned int val;
  CHECKINIT;

  printf("%s...", __func__);

  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;

  //Read/normalize reference
  val = vtp->v7.sd.Scaler_BusClk;
  if(!val) val = 1;
  ref = 33330000.0f / (float)val;

  for(i=0;i<6;i++)
    data[i] = ref * (float)vtp->v7.dcrbRoadFind.Scalers[i];

  vtp->v7.sd.ScalerLatch = 0;
  VUNLOCK;

  sprintf(name, "%s_VTPDC_SUPERLAYER", host);
  epics_json_msg_send(name, "float", 6, data);

  return OK;
}


int
vtpTiAck(int clearsync)
{
  int val = VTP_EB_TICTRL_TI_ACK;
  CHECKINIT;

  if(clearsync)
    val |= VTP_EB_TICTRL_SYNCEVT_RST;
  
  VLOCK;
  vtp->eb.TiCtrl = val;
  VUNLOCK;
  
  return OK;
}

#define TI_LINK_INIT_TRIES    3
int
vtpTiLinkInit()
{
  int i, val;
  CHECKINIT;

  for(i = 0; i < TI_LINK_INIT_TRIES; i++)
  {
    VLOCK;
    vtp->eb.LinkCtrl = VTP_EB_LINKCTRL_RX_RESET | VTP_EB_LINKCTRL_PLL_RST | VTP_EB_LINKCTRL_RX_FIFO_RST;
    vtp->eb.LinkCtrl = VTP_EB_LINKCTRL_RX_RESET | VTP_EB_LINKCTRL_RX_FIFO_RST;
    vtp->eb.LinkCtrl = VTP_EB_LINKCTRL_RX_FIFO_RST;
    vtp->eb.LinkCtrl = 0;
    VUNLOCK;
  
    usleep(10000);
    
    VLOCK;
    val = vtp->eb.LinkStatus;
    VUNLOCK;
    
    if(val & VTP_EB_LINKSTATUS_RX_READY)
    {
      printf("%s: VTP <-> TI Link RX Ready (status=0x%08X)\n", __func__, val);
      break;
    }
    else
      printf("%s: *** Warning *** VTP <-> TI Link NOT Ready (status=0x%08X)...", __func__, val);
    
    if(i != TI_LINK_INIT_TRIES-1)
      printf("trying again.\n");
    else
    {
      printf("failed.\n");
      printf("%s: *** ERROR *** VTP <-> TI Link problem.\n", __func__);
    }
  }
  
  VLOCK;
  vtp->eb.TiCtrl = VTP_EB_TICTRL_SYNCEVT_RST;
  VUNLOCK;
  
  return OK;
}

int
vtpTiLinkStatus()
{
  int val;
  CHECKINIT;

  VLOCK;
  val = vtp->eb.LinkStatus;
  VUNLOCK;
  
  printf("%s: LinkStatus = 0x%08X RxReady=%u, RxLocked=%u,PllLocked=%u,RxErrorCnt = %u\n",
         __func__, val,
         (val & VTP_EB_LINKSTATUS_RX_READY) ? 1:0,
         (val & VTP_EB_LINKSTATYS_RX_LOCKED) ? 1:0,
         (val & VTP_EB_LINKSTATUS_GCLK_PLL_LOCK) ? 1:0,
         (val & VTP_EB_LINKSTATUS_RX_ERROR_CNT_MASK)
        );
  
  return OK;
}

int
vtpEbResetFifo()
{
  CHECKINIT;

  VLOCK;
  vtp->eb.LinkCtrl |= VTP_EB_LINKCTRL_FIFO_RST;
  vtp->eb.LinkCtrl &= ~VTP_EB_LINKCTRL_FIFO_RST;
  VUNLOCK;
  
  return OK;
}

int
vtpDmaStatus()
{
  uint32_t cr, sr, len, da;
  CHECKINIT;
  
  VLOCK;
  cr = vtp->dma.S2MM_DMACR;
  sr = vtp->dma.S2MM_DMASR;
  len = vtp->dma.S2MM_LENGTH;
  da = vtp->dma.S2MM_DA;
  VUNLOCK;
  
  printf("%s: cr=0x%08X, sr=0x%08X, len=%d, da=0x%08X\n", __func__,  cr, sr, len, da);
  return OK;
}


int
vtpDmaInit()
{
  CHECKINIT;
  
  printf("%s: start", __func__);
  vtpDmaStatus();
  
  VLOCK;
  vtp->dma.S2MM_DMACR =
    (0<<0)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (1<<2);     // 1-reset DMA engine
    
  vtp->dma.S2MM_DMACR =
    (0<<0)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (0<<2);     // 1-reset DMA engine
  VUNLOCK;
  
  printf("%s: end  ", __func__);
  vtpDmaStatus();
  
  return OK;
}

int
vtpDmaStart(unsigned int destAddr, int maxLength)
{
  int i;
  CHECKINIT;
  
  printf("%s: start", __func__);
  vtpDmaStatus();
  
  VLOCK;
  vtp->dma.S2MM_DMACR =
    (1<<0)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (0<<2);     // 1-reset DMA engine

  vtp->dma.S2MM_DA_MSB = 0;
  vtp->dma.S2MM_DA = destAddr;
  vtp->dma.S2MM_LENGTH = maxLength;

  VUNLOCK;
  
  printf("%s: end  ", __func__);
  for(i=0;i<10;i++)
  {
  vtpDmaStatus();
  }
  
  return OK;
}

int
vtpDmaWaitDone()
{
  int rval = 0;
  unsigned int cnt = 0;
  CHECKINIT;

  printf("%s: start", __func__);
  vtpDmaStatus();
  
  VLOCK;
  while(1)
  {
    if((vtp->dma.S2MM_DMASR & 0x3) == 0x2)
    {
      rval = vtp->dma.S2MM_LENGTH;
      break;
    }
    else if(++cnt > 1000000)
      break;
  }
  VUNLOCK;
 
  
  printf("%s: end  ", __func__);
  vtpDmaStatus();

  return rval;
}

int
vtpEbBuildTestEvent(int len)
{
  CHECKINIT;
  
  VLOCK;
  vtp->eb.EbCtrl = 0x8 | 0x4 | (len<<8);
  VUNLOCK;
  
  return OK;
}

int
vtpEbReset()
{
  CHECKINIT;
  
  VLOCK;
  vtp->eb.LinkCtrl = 0x8;
  vtp->eb.LinkCtrl = 0x0;
  VUNLOCK;
  
  return OK;
}

int
vtpEbTiReadEvent(uint32_t *pBuf, uint32_t maxsize)
{
  int status, cnt = 0;
  CHECKINIT;

  if(vtpEbTiEventReadErrors)
    printf("{vtpEbTiEventReadErrors=%d}\n", vtpEbTiEventReadErrors);

  int retry=100;
  while(cnt < maxsize)
  {
    VLOCK;
    status = vtp->eb.EbStatus;
    VUNLOCK;
    
    if(status & 0x1)
    {
      if(retry-- > 0)
      {
        continue;
      }
      else
      {
        vtpEbTiEventReadErrors++;
        printf("vtpEbTiReadEvent: TIMEOUT ERROR (cnt=%d)\n", vtpEbTiEventReadErrors);
        break;
      }
    }    

    VLOCK;
    *pBuf++ = vtp->eb.TiFifo;
    VUNLOCK;

    if(status & 0x10000)
      break;
    
    if(++cnt > maxsize)
    {
      printf("too many event words...exiting\n");
      break;
    }
  }
  
  return cnt;
}

#define VTP_EB_NRETRIES   10000

int
vtpEbReadEvent(uint32_t *pBuf, uint32_t maxsize)
{
  int status, cnt = 0;
  CHECKINIT;

  if(vtpEbEventReadErrors)
    printf("{vtpEbEventReadErrors=%d}\n", vtpEbEventReadErrors);
 
  int retry=VTP_EB_NRETRIES;
  while(cnt < maxsize)
  {
    VLOCK;
    status = vtp->eb.EbStatus;
    VUNLOCK;
    
    if(status & 0x2)
    {
      if(retry-- > 0)
      {
        continue;
      }
      else
      {
        vtpEbEventReadErrors++;
        printf("vtpEbReadEvent: TIMEOUT ERROR (cnt=%d)\n", vtpEbEventReadErrors);
        break;
      }
    }    
    
    VLOCK;
    *pBuf++ = vtp->eb.VtpFifo;
    VUNLOCK;

    if(status & 0x20000)
      break;
    
    if(++cnt > maxsize)
    {
      printf("too many event words...exitting\n");
      break;
    }
  }
  
  return cnt;
}

int
vtpEbReadEvent_test(uint32_t *pBuf, uint32_t maxsize)
{
  int status, cnt = 0, tries = 0;
  CHECKINIT;
  
  while(cnt < maxsize)
  {
    VLOCK;
    status = vtp->eb.EbStatus;
    VUNLOCK;
   
    // if buffer is empty, try again until data is ready 
    if(status & 0x2)
    {
      tries++;
      printf("{cnt=%d}", cnt);
      if(tries > 20)
        return cnt;
      continue;
    }
    tries = 0;
    
    VLOCK;
    *pBuf++ = vtp->eb.VtpFifo;
    VUNLOCK;

    if(status & 0x20000)
      break;
    
    if(++cnt > maxsize)
    {
      printf("too many event words...exitting\n");
      break;
    }
  }
  
  return cnt;
}

int
vtpEbReadTestEvent(uint32_t *pBuf, uint32_t maxsize)
{
  int status, cnt = 0;
  CHECKINIT;
  
  while(cnt < maxsize)
  {
    VLOCK;
    status = vtp->eb.EbStatus;
    VUNLOCK;
    
    if(status & 0x4)
      break;
    
    VLOCK;
    *pBuf++ = vtp->eb.TestFifo;
    VUNLOCK;

    if(status & 0x40000)
      break;
    
    if(++cnt > maxsize)
    {
      printf("too many event words...exitting\n");
      break;
    }
  }
  
  return cnt;
}

int
vtpBReady()
{
  int status;
  CHECKINIT;

  VLOCK;
  status = vtp->eb.EbStatus;
  VUNLOCK;

  // bit 0 = ti event buffer empty flag
  // bit 1 = vtp event buffer empty flag
  //if(status & 0x3) return(0); /* both TI and VTP: not ready */
  if(status & 0x1) return(0); /* TI only: not ready */
  return(1);
}

int
vtpEbDecodeEvent(uint32_t *pBuf, uint32_t size)
{
  uint32_t tag = 0, index = 0, val, last_val, v;
  char view_array[4] = {'U', 'V', 'W', '?'};
  
  printf("%s: Decoding VTP event buffer:\n", __func__);
  while(size--)
  {
    last_val = val;
    val = *pBuf++;
    
    if(val & 0x80000000)
    {
      index = 0;
      tag = (val>>27) & 0xF;
    }
    else
      index++;
    
    
    printf("%08X: ", val);
    
    switch(tag)
    {
      case 0:  // Block Header Tag
        printf("Block Header:");
        printf(" Block Count = %d,", (val>>8) & 0x3FF);
        printf(" Block Size = %d\n", (val>>0) & 0xFF);
        break;
        
      case 1:  // Block Trailer Tag
        printf("Block Trailer:");
        printf(" Word Count = %d\n", (val>>0) & 0x3FFFFF);
        break;
        
      case 2:  // Event Header Tag
        printf("Event Header:");
        printf(" Event Number: %d\n", (val>>0) & 0x3FFFFF);
        break;
        
      case 3:  // Trigger Time Tag
        if(index == 1)
        {
          printf("Trigger Time:");
          printf(" 23:0: %d,", (last_val>>0) & 0xFFFFFF);
          printf(" 47:24: %d\n", (val>>0) & 0xFFFFFF);
        }
        break;

      case 4:  // ECtrig Peak Tag
        if(index == 1)
        {
          printf("ECtrig Peak:");
          printf(" Inst = %1d", (last_val>>26) & 0x1);
          printf(" View = %c", view_array[(last_val>>24) & 0x3]);
          printf(" Coord = %2.2f", ((float)((last_val>>15) & 0x1ff)) / 8.0f);
          printf(" Energy = %4d", (last_val>>2) & 0x1fff);
          printf(" Time = %4d\n", (val>>0) & 0x7ff);
        }
        break;
        
      case 5:  // ECtrig Cluster Tag
        if(index == 1)
        {
          printf("ECtrig Cluster:");
          printf(" Inst = %1d", (last_val>>26) & 0x1);
          printf(" Coord W = %2.2f", ((float)((last_val>>17) & 0x1ff)) / 8.0f);
          printf(" Coord V = %2.2f", ((float)((last_val>>8) & 0x1ff)) / 8.0f);

          v = ((last_val<<1)&0x1fe) | ((val>>30)&0x1);
          printf(" Coord U = %2.2f", ((float)v) / 8.0f);
          printf(" Energy = %4d", (val>>17) & 0x1fff);
          printf(" Time = %4d\n", (val>>0) & 0x7ff);
        }
        break;
        
      case 6:  // Trigger bit Tag
        printf("Trigger bit:");
        printf(" Inst = %1d", (val>>16)&0x1);
        printf(" Lane = %2d", (val>>11)&0x1f);
        printf(" Time = %4d\n", (val>>0)&0x7ff);
        break;
        
      default:
        printf("*** UNKNOWN TAG/WORD *** 0x%08X\n", val);
        break;
    }
  }
  
  return OK;
}

int
vtpEbReadAndDecodeEvent()
{
  uint32_t buf[1000], size;

  size = vtpEbReadEvent(buf, sizeof(buf)/sizeof(buf[0]));
  return vtpEbDecodeEvent(buf, size);
  
  return OK;
}

static int
vtpFPGAOpen()
{
  if(vtpFPGAFD > 0)
    {
      printf("%s: ERROR: VTP FPGA already opened.\n",
	     __func__);
      return ERROR;
    }
  
  vtpFPGAFD = open(vtpFPGADev, O_RDWR | O_SYNC);
  
  if(vtpFPGAFD < 0)
    {
      printf("%s: ERROR from open: %s (%d)",
	     __func__, strerror(errno), errno);
      return ERROR;
    }

  printf(" size = %d\n", sizeof(ZYNC_REGS)); /* should be 131072 */
  
  vtp = (volatile ZYNC_REGS *) mmap((void *)VTP_ZYNC_PHYSMEM_BASE, sizeof(ZYNC_REGS),
				    PROT_READ|PROT_WRITE, MAP_SHARED,
				    vtpFPGAFD, 0);

  if(vtp == MAP_FAILED)
    {
      printf("%s: ERROR from mmap: %s (%d)\n",
	     __func__, strerror(errno), errno);
      return ERROR;
    }
  
  return OK;
}

static int
vtpFPGAClose()
{
  if(vtpFPGAFD < 0)
    {
      printf("%s: ERROR: VTP FPGA not opened.\n",
	     __func__);
      return ERROR;
    }

  if(munmap((void *)vtp, sizeof(ZYNC_REGS)) < 0)
      printf("%s: ERROR from munmap: %s (%d)\n",
	     __func__, strerror(errno), errno);

  vtp = NULL;
  
  close(vtpFPGAFD);
  return OK;
}


int
vtpOpen(int dev_mask)
{
  if(dev_mask & VTP_FPGA_OPEN)
    {
      
      if(vtpFPGAOpen() == OK)
	{
	  vtpDevOpenMASK |= VTP_FPGA_OPEN; 
	}
      else
	{
	  printf("%s: ERROR opening V7 map\n",
		 __func__);
	}
    }

  if(dev_mask & VTP_I2C_OPEN)
    {
      
      if(vtpI2COpen() == OK)
	{
	  vtpDevOpenMASK |= VTP_I2C_OPEN; 
	}
      else
	{
	  printf("%s: ERROR opening I2C device\n",
		 __func__);
	}
    }
  
  if(dev_mask & VTP_SPI_OPEN)
    {
      
      if(vtpSPIOpen() == OK)
	{
	  vtpDevOpenMASK |= VTP_SPI_OPEN; 
	}
      else
	{
	  printf("%s: ERROR opening SPI device\n",
		 __func__);
	}
    }

  vtpCreateLockShm();
  
  return vtpDevOpenMASK;
}

int
vtpClose(int dev_mask)
{
  if(dev_mask & VTP_SPI_OPEN)
    {
      
      if(vtpSPIClose() == OK)
	{
	  vtpDevOpenMASK &= ~VTP_SPI_OPEN;
	}

    }
      
  if(dev_mask & VTP_I2C_OPEN)
    {

      if(vtpI2CClose() == OK)
	{
	  vtpDevOpenMASK &= ~VTP_I2C_OPEN;
	}

    }

  if(dev_mask & VTP_FPGA_OPEN)
    {

      if(vtpFPGAClose() == OK)
	{
	  vtpDevOpenMASK &= ~VTP_FPGA_OPEN;
	}

    }
  
  vtpKillLockShm(0);
  
  return vtpDevOpenMASK;
}


static int
vtpMutexInit()
{
  printf("%s: Initializing vtp mutex\n",__FUNCTION__);
  if(pthread_mutexattr_init(&p_sync->m_attr)<0) 
    {
      perror("pthread_mutexattr_init");
      printf("%s: ERROR:  Unable to initialized mutex attribute\n",__FUNCTION__);
      return ERROR;
    }
  if(pthread_mutexattr_setpshared(&p_sync->m_attr, PTHREAD_PROCESS_SHARED)<0)
    {
      perror("pthread_mutexattr_setpshared");
      printf("%s: ERROR:  Unable to set shared attribute\n",__FUNCTION__);
      return ERROR;
    }
  if(pthread_mutexattr_setrobust_np(&p_sync->m_attr, PTHREAD_MUTEX_ROBUST_NP)<0)
    {
      perror("pthread_mutexattr_setrobust_np");
      printf("%s: ERROR:  Unable to set robust attribute\n",__FUNCTION__);
      return ERROR;
    }
  if(pthread_mutex_init(&(p_sync->mutex), &p_sync->m_attr)<0)
    {
      perror("pthread_mutex_init");
      printf("%s: ERROR:  Unable to initialize shared mutex\n",__FUNCTION__);
      return ERROR;
    }

  memset(&p_sync->vtp, 0, sizeof(VTPSHMDATA));

  return OK;
}

/*!
  Routine to create (if needed) a shared mutex for VME Bus locking
 
  @return 0, if successful. -1, otherwise.
*/
int
vtpCreateLockShm()
{
  int fd_shm;
  int needMutexInit=0, stat=0;
  mode_t prev_mode;

  /* First check to see if the file already exists */
  fd_shm = shm_open(shm_name_vtp, O_RDWR, 
		    S_IRUSR | S_IWUSR |
		    S_IRGRP | S_IWGRP |
		    S_IROTH | S_IWOTH );
  if(fd_shm<0)
    {
      /* Bad file handler.. */
      if(errno == ENOENT)
	{
	  needMutexInit=1;
	}
      else
	{
	  perror("shm_open");
	  printf(" %s: ERROR: Unable to open shared memory\n",__FUNCTION__);
	  return ERROR;
	}
    }

  if(needMutexInit)
    {
      printf("%s: Creating VTP shared memory file\n",__FUNCTION__);
      prev_mode = umask(0); /* need to override the current umask, if necessary */
      /* Create and map 'mutex' shared memory */
      fd_shm = shm_open(shm_name_vtp, O_CREAT|O_RDWR,
			S_IRUSR | S_IWUSR |
			S_IRGRP | S_IWGRP |
			S_IROTH | S_IWOTH );
      umask(prev_mode);
      if(fd_shm<0)
	{
	  perror("shm_open");
	  printf(" %s: ERROR: Unable to open shared memory\n",__FUNCTION__);
	  return ERROR;
	}
      ftruncate(fd_shm, sizeof(struct shared_memory_struct));
    }

  addr_shm = mmap(0, sizeof(struct shared_memory_struct), PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(addr_shm<0) 
    {
      perror("mmap");
      printf("%s: ERROR: Unable to mmap shared memory\n",__FUNCTION__);
      return ERROR;
    }
  p_sync = addr_shm;

  if(needMutexInit)
    {
      p_sync->shmSize = sizeof(struct shared_memory_struct);

      stat = vtpMutexInit();
      if(stat==ERROR)
	{
	  printf("%s: ERROR Initializing VTP Mutex\n",
		 __FUNCTION__);
	  return ERROR;
	}
    }
  else
    {
      if(p_sync->shmSize != sizeof(struct shared_memory_struct))
	{
	  printf("%s: ERROR: Inconsistency in size of shared memory structure!\n",
		 __func__);
	  printf("\t File = %d  Library = %d\n",
		 p_sync->shmSize, sizeof(struct shared_memory_struct));
	  printf("\t Possible version mismatch!\n");
	  return ERROR;
	}
    }

  return OK;
}

/*!
  Routine to destroy the shared mutex created by vtpCreateLockShm()
 
  @return 0, if successful. -1, otherwise.
*/
int
vtpKillLockShm(int kflag)
{

  if(munmap(addr_shm, sizeof(struct shared_memory_struct))<0)
    perror("munmap");
  if(kflag==1)
    {
      if(pthread_mutexattr_destroy(&p_sync->m_attr)<0)
	perror("pthread_mutexattr_destroy");
      
      if(pthread_mutex_destroy(&p_sync->mutex)<0)
	perror("pthread_mutex_destroy");

      if(shm_unlink(shm_name_vtp)<0) 
	perror("shm_unlink");

      printf("%s: VTP shared memory mutex destroyed\n",__FUNCTION__);
    }
  return OK;
}

/*!
  Routine to lock the shared mutex created by vtpCreateLockShm()
 
  @return 0, if successful. -1 or other error code otherwise.
*/
int
vtpLock()
{
  int rval;

printf("%s - start\n", __func__);

  if(p_sync!=NULL)
    {
      rval = pthread_mutex_lock(&(p_sync->mutex));
      if(rval<0) 
	{
	  perror("pthread_mutex_lock");
	  printf("%s: ERROR locking VTP\n",__FUNCTION__);
	}
      else if (rval>0)
	{
	  printf("%s: ERROR: %s\n",__FUNCTION__,
		 (rval==EINVAL)?"EINVAL":
		 (rval==EBUSY)?"EBUSY":
		 (rval==EAGAIN)?"EAGAIN":
		 (rval==EPERM)?"EPERM":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"ENOTRECOVERABLE":
		 "Undefined");
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of VTP (mutex) died unexpectedly\n",
		     __FUNCTION__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	  if(rval==ENOTRECOVERABLE)
	    {
	      printf("%s: ERROR: VTP mutex in an unrecoverable state!\n",
		     __FUNCTION__);
	    }
	}
    }
  else
    {
      printf("%s: ERROR: vtpLock not initialized.\n",__FUNCTION__);
      return ERROR;
    }

printf("%s - end\n", __func__);
  return rval;
}

/*!
  Routine to try to lock the shared mutex created by vtpCreateLockShm()
 
  @return 0, if successful. -1 or other error code otherwise.
*/
int
vtpTryLock()
{
  int rval=ERROR;
  
  if(p_sync!=NULL)
    {
      rval = pthread_mutex_trylock(&(p_sync->mutex));
      if(rval<0) 
	{
	  perror("pthread_mutex_trylock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s\n",__FUNCTION__,
		 (rval==EINVAL)?"EINVAL":
		 (rval==EBUSY)?"EBUSY":
		 (rval==EAGAIN)?"EAGAIN":
		 (rval==EPERM)?"EPERM":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"ENOTRECOVERABLE":
		 "Undefined");
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of VTP (mutex) died unexpectedly\n",
		     __FUNCTION__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	  if(rval==ENOTRECOVERABLE)
	    {
	      printf("%s: ERROR: VTP mutex in an unrecoverable state!\n",
		     __FUNCTION__);
	    }
	}
    }
  else
    {
      printf("%s: ERROR: VTP mutex not initialized\n",__FUNCTION__);
      return ERROR;
    }

  return rval;

}

/*!
  Routine to lock the shared mutex created by vtpCreateLockShm()
 
  @return 0, if successful. -1 or other error code otherwise.
*/

int
vtpTimedLock(int time_seconds)
{
  int rval=ERROR;
  struct timespec timeout;

  if(p_sync!=NULL)
    {
      clock_gettime(CLOCK_REALTIME, &timeout);
      timeout.tv_nsec = 0;
      timeout.tv_sec += time_seconds;

      rval = pthread_mutex_timedlock(&p_sync->mutex,&timeout);
      if(rval<0) 
	{
	  perror("pthread_mutex_timedlock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s\n",__FUNCTION__,
		 (rval==EINVAL)?"Invalid Argument":
		 (rval==EBUSY)?"Device or Resource Busy":
		 (rval==EAGAIN)?"Maximum number of recursive locks exceeded":
		 (rval==ETIMEDOUT)?"Not locked before specified timeout":
		 (rval==EPERM)?"Operation Not Permitted":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"Mutex Not Recoverable":
		 "Undefined");
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of VTP (mutex) died unexpectedly\n",
		     __FUNCTION__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	}
    }
  else
    {
      printf("%s: ERROR: VTP mutex not initialized\n",__FUNCTION__);
      return ERROR;
    }

  return rval;

}

/*!
  Routine to unlock the shared mutex created by vtpCreateLockShm()
 
  @return 0, if successful. -1 or other error code otherwise.
*/
int
vtpUnlock()
{
  int rval=0;
printf("%s - start\n", __func__);
  if(p_sync!=NULL)
    {
      rval = pthread_mutex_unlock(&p_sync->mutex);
      if(rval<0) 
	{
	  perror("pthread_mutex_unlock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s \n",__FUNCTION__,
		   (rval==EINVAL)?"EINVAL":
		   (rval==EBUSY)?"EBUSY":
		   (rval==EAGAIN)?"EAGAIN":
		   (rval==EPERM)?"EPERM":
		   "Undefined");
	}
    }
  else
    {
      printf("%s: ERROR: VTP mutex not initialized.\n",__FUNCTION__);
      return ERROR;
    }
printf("%s - end\n", __func__);
  return rval;
}

/*!
  Routine to check the "health" of the mutex created with vtpCreateLockShm()

  If the mutex is found to be stale (Owner of the lock has died), it will
  be recovered.
 
  @param time_seconds     How many seconds to wait for mutex to unlock when testing

  @return 0, if successful. -1, otherwise.
*/
int
vtpCheckMutexHealth(int time_seconds)
{
  int rval=0, busy_rval=0;

  if(p_sync!=NULL)
    {
      printf("%s: Checking health of VTP shared mutex...\n",
	     __FUNCTION__);
      /* Try the Mutex to see if it's state (locked/unlocked) */
      printf(" * ");
      rval = vtpTryLock();
      switch (rval)
	{
	case -1: /* Error */
	  printf("%s: rval = %d: Not sure what to do here\n",
		 __FUNCTION__,rval);
	  break;
	case 0:  /* Success - Got the lock */
	  printf(" * ");
	  rval = vtpUnlock();
	  break;

	case EAGAIN: /* Bad mutex attribute initialization */
	case EINVAL: /* Bad mutex attribute initialization */
	  /* Re-Init here */
	  printf(" * ");
	  rval = vtpMutexInit();
	  break;

	case EBUSY: /* It's Locked */
	  {
	    /* Check to see if we can unlock it */
	    printf(" * ");
	    busy_rval = vtpUnlock();
	    switch(busy_rval)
	      {
	      case OK:     /* Got the unlock */
		rval=busy_rval;
		break;

	      case EAGAIN: /* Bad mutex attribute initialization */
	      case EINVAL: /* Bad mutex attribute initialization */
		/* Re-Init here */
		printf(" * ");
		rval = vtpMutexInit();
		break;

	      case EPERM: /* Mutex owned by another thread */
		{
		  /* Check to see if we can get the lock within 5 seconds */
		  printf(" * ");
		  busy_rval = vtpTimedLock(time_seconds);
		  switch(busy_rval)
		    {
		    case -1: /* Error */
		      printf("%s: rval = %d: Not sure what to do here\n",
			     __FUNCTION__,busy_rval);
		      break;

		    case 0:  /* Success - Got the lock */
		      printf(" * ");
		      rval = vtpUnlock();
		      break;

		    case EAGAIN: /* Bad mutex attribute initialization */
		    case EINVAL: /* Bad mutex attribute initialization */
		      /* Re-Init here */
		      printf(" * ");
		      rval = vtpMutexInit();
		      break;

		    case ETIMEDOUT: /* Timeout getting the lock */
		      /* Re-Init here */
		      printf(" * ");
		      rval = vtpMutexInit();
		      break;

		    default:
		      printf("%s: Undefined return from pthread_mutex_timedlock (%d)\n",
			     __FUNCTION__,busy_rval);
		      rval=busy_rval;

		    }

		}
		break;

	      default:
		printf("%s: Undefined return from vtpUnlock (%d)\n",
		       __FUNCTION__,busy_rval);
		      rval=busy_rval;

	      }

	  }
	  break;
	  
	default:
	  printf("%s: Undefined return from vtpTryLock (%d)\n",
		 __FUNCTION__,rval);
	  
	}

      if(rval==OK)
	{
	  printf("%s: Mutex Clean and Unlocked\n",__FUNCTION__);
	}
      else
	{
	  printf("%s: Mutex is NOT usable\n",__FUNCTION__);
	}

    }
  else
    {
      printf("%s: INFO: VTP Mutex not initialized\n",
	     __FUNCTION__);
      return ERROR;
    }

  return rval;
}
