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
#include "vtpLib.h"


static int vtpDevOpenMASK = 0;
static int vtpFPGAFD = -1;
const char vtpFPGADev[256] = "/dev/uio0";

static int VTP_FW_Version = 0;
static int VTP_FW_Type = 0;

static volatile ZYNC_REGS *vtp = NULL;

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
  int syncSrc, trig1Src, clkSrc;
  
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
  
  vtpV7SetReset(1);
  vtpV7SetResetSoft(1);

  vtpV7SetReset(0);
  vtpV7SetResetSoft(0);
  
  VTP_FW_Version = vtpV7GetFW_Version();
  VTP_FW_Type = vtpV7GetFW_Type();
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
        return ERROR;
    }
  }

  vtpV7PllReset(1);
  vtpV7PllReset(0);

  vtpSetTrig1Source(trig1Src);
  vtpSetSyncSource(syncSrc);
  
  vtpTiLinkInit();
  
  vtpEbResetFifo();

  VLOCK;
  vtp->v7.sd.FPAOSel = 0xFFFFFFFF;  /* Route trigger output to FPAO */
  VUNLOCK;

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
vtpSerdesStatus(int type, uint16_t dev, int pflag)
{
  volatile SERDES_REGS *sdev;
  uint32_t status = 0, ctrl = 0, ctrl2, latency = 0;
  CHECKINIT;
  CHECKTYPEDEV;
  
  VLOCK;
  ctrl2 = sdev->Ctrl;
  status = sdev->Status;
  latency = sdev->Latency;
  if(VTP_FW_Type == VTP_FW_TYPE_EC)
    ctrl = vtp->v7.fadcDec.Ctrl;
  else if(VTP_FW_Type == VTP_FW_TYPE_GT)
    ctrl = vtp->v7.sspDec.Ctrl;
  else if(VTP_FW_Type == VTP_FW_TYPE_DC)
    ctrl = vtp->v7.dcrbDec.Ctrl;
  else if(VTP_FW_Type == VTP_FW_TYPE_HCAL)
    ctrl = vtp->v7.hcal.Ctrl;
  else if(VTP_FW_Type == VTP_FW_TYPE_FTCAL)
    ctrl = vtp->v7.ftcalDec.Ctrl;
  VUNLOCK;

  if(pflag)
  {
      printf("\n");
      if(type == VTP_SERDES_VXS)
      {
        printf("    ---Lane---    Error  Link  Trg Latency(ns)\n");
        printf("PP  0 1        Ch Count  Reset En  RX     TX\n");
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
  if(type == VTP_SERDES_QSFP)
  {
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(2))?"U":"D");
    printf("%s ", (status & VTP_SERDES_STATUS_LANE_UP(3))?"U":"D");
  }
  else
    printf("       ");
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
  
  return OK;
}

int
vtpSerdesStatusAll()
{
  int i;
  for(i = 0; i < 16; i++)
    vtpSerdesStatus(VTP_SERDES_VXS, i, (i==0));
  
  for(i = 0; i < 4; i++)
    vtpSerdesStatus(VTP_SERDES_QSFP, i, (i==0));

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

  vtpV7SetProgram_B(0);

  result = vtpV7GetInit_B();
  printf("%s: Init_B = %d, Expected to be 0...%s\r\n",
	 __func__, result, (result == 1) ? "Failed" : "Okay");

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

  printf("%s: Opening file: %s...", __func__, filename);
  f = fopen(filename, "rb");
  if(!f)
  {
    printf("FAILED\r\n");
    return ERROR;
  }
  printf("Opened successfully\r\n");

  while(1)
  {
    bytesRead = fread(&buf[0], 1, sizeof(buf), f);

    if(bytesRead < 0)
	  {
	    printf("ERROR: fread() returned %d\r\n", bytesRead);
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
    return ERROR;
  }
  write(fd, pBits, len);
  close(fd);
  free(pBits);
  
  printf("%s: wrote %ld bytes\r\n", __func__, len);
  printf("%s: end reached.\r\n", __func__);
  
  return OK;
}

/**************************************************************************************
 *
 *  vtpReadScalers - Scaler Data readout routine
 *
 *    data        - local memory address to place data
 *    max_scalers - Maximum number of scalers that can be written to data
 * 
 *   RETURNS the number of 32bit words read, or ERROR if unsuccessful.
 */
int
vtpReadScalers(volatile unsigned int *data, int max_scalers)
{
  return 0;
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
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
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
    case VTP_FW_TYPE_EC:
    case VTP_FW_TYPE_PC:
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
  }
  VUNLOCK;

  return val;
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
vtpSetFadcSum_MaskEn(unsigned int mask[8])
{
  int i;
  CHECKINIT;
  /*CHECKTYPE(VTP_FW_TYPE_EC);*/
  if( (VTP_FW_TYPE_PC != VTP_FW_Type) && (VTP_FW_TYPE_EC != VTP_FW_Type) )
  {
    printf("%s: ERROR: VTP wrong firmware type\n",__func__);
    return ERROR;
  }
  
  VLOCK;
  for(i=0; i<8; i++)
    vtp->v7.fadcSum.SumEn[i] = mask[i];
  VUNLOCK;
  
  return OK;
}

int
vtpGetFadcSum_MaskEn(unsigned int mask[8])
{
  int i;
  CHECKINIT;
  /*CHECKTYPE(VTP_FW_TYPE_EC);*/
  if( (VTP_FW_TYPE_PC != VTP_FW_Type) && (VTP_FW_TYPE_EC != VTP_FW_Type) )
  {
    printf("%s: ERROR: VTP wrong firmware type\n",__func__);
    return ERROR;
  }
  
  VLOCK;
  for(i=0; i<8; i++)
    mask[i] = vtp->v7.fadcSum.SumEn[i];
  VUNLOCK;
  
  return OK;
}

int
vtpSetECcosmic_emin(int inst, int emin)
{
  uint32_t val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_EC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
vtpGetPCcosmic_pixel(int *enable)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_PC);
  
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
  vtp->v7.ecsTrigger.DalitzMin = dalitz_min;
  vtp->v7.ecsTrigger.DalitzMax = dalitz_max;
  VUNLOCK;

  return OK;
}

int
vtpGetECS_dalitz(int *dalitz_min, int *dalitz_max)
{
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_ECS);
  
  VLOCK;
  *dalitz_min = vtp->v7.ecsTrigger.DalitzMin;
  *dalitz_max = vtp->v7.ecsTrigger.DalitzMax;
  VUNLOCK;

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
vtpSetGtTriggerBit(int inst, int strigger_mask, int sector_mask, int mult_min, int coin_width, int central_en)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  if(inst < 0 || inst > 16)
  {
    printf("%s: ERROR - invalid trigger bit %d\n", __func__, inst);
    return ERROR;
  }
  
  val = ((strigger_mask & 0xF) <<0) |
        ((mult_min      & 0x7) <<4) |
        ((central_en    & 0x1) <<7) |
        ((sector_mask   & 0x3F)<<8) |
        ((coin_width    & 0xFF)<<16);
  
  VLOCK;
  vtp->v7.gtBit[inst].STrigger = val;
  VUNLOCK;
  
  return OK;  
}

int
vtpGetGtTriggerBit(int inst, int *strigger_mask, int *sector_mask, int *mult_min, int *coin_width, int *central_en)
{
  int val;
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
  
  if(inst < 0 || inst > 16)
  {
    printf("%s: ERROR - invalid trigger bit %d\n", __func__, inst);
    return ERROR;
  }
  
  VLOCK;
  val = vtp->v7.gtBit[inst].STrigger;
  VUNLOCK;

  *strigger_mask = (val>>0)&0xF;
  *mult_min      = (val>>4)&0x7;
  *central_en    = (val>>7)&0x1;
  *sector_mask   = (val>>8)&0x3F;
  *coin_width    = (val>>16)&0xFF;
    
  return OK;  
}

int
vtpGtPrintScalers()
{
  double ref, rate; 
  int i; 
  unsigned int gtscalers[20];
  const char *scalers_name[20] = {
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
    "Trigger15"
    };
 
  CHECKINIT;
  CHECKTYPE(VTP_FW_TYPE_GT);
 
  
  VLOCK;
  vtp->v7.sd.ScalerLatch = 1;
  gtscalers[0] = vtp->v7.sd.Scaler_BusClk;
  gtscalers[1] = vtp->v7.sd.Scaler_Sync;
  gtscalers[2] = vtp->v7.sd.Scaler_Trig1;
  gtscalers[3] = vtp->v7.sd.Scaler_Trig2;
  for(i=0; i<16; i++)
    gtscalers[4+i] = vtp->v7.sd.Scaler_Trigger[i];
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

  for(i = 0; i < 20; i++) 
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
  unsigned int scalers[5];
  const char *scalers_name[5] = {
    "BusClk",
    "PeakU",
    "PeakV",
    "PeakW",
    "Hit"
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
  uint32_t cr, sr, len;
  CHECKINIT;
  
  VLOCK;
  cr = vtp->dma.S2MM_DMACR;
  sr = vtp->dma.S2MM_DMASR;
  len = vtp->dma.S2MM_LENGTH;
  VUNLOCK;
  
  printf("%s: cr=0x%08X, sr=0x%08X, len=%d\n", __func__,  cr, sr, len);
  return OK;
}


int
vtpDmaInit()
{
  CHECKINIT;
  
  printf("%s: start", __func__);
  vtpDmaStatus();
  
  VLOCK;
  vtp->dma.MM2S_DMACR =
    (0<<0)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (1<<2);     // 1-reset DMA engine
    
  vtp->dma.MM2S_DMACR =
    (0<<0)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (0<<2);     // 1-reset DMA engine

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
  CHECKINIT;
  
  printf("%s: start", __func__);
  vtpDmaStatus();
  
  VLOCK;
  vtp->dma.S2MM_DMACR =
    (1<<1)  |   // 0-stops, 1-starts DMA engine
    (1<<1)  |   // reserved, defaults to 1
    (0<<2);     // 1-reset DMA engine
  VUNLOCK;
  
  printf("%s: 1    ", __func__);
  vtpDmaStatus();
  printf("%s: 2    ", __func__);
  vtpDmaStatus();
  printf("%s: 3    ", __func__);
  vtpDmaStatus();
  
  VLOCK;
  vtp->dma.S2MM_DA_MSB = 0;
  vtp->dma.S2MM_DA = destAddr;
  vtp->dma.S2MM_LENGTH = maxLength;

  VUNLOCK;
  
  printf("%s: end  ", __func__);
  vtpDmaStatus();
  
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
//  unsigned int val;
  
  CHECKINIT;
  
  VLOCK;
  vtp->eb.EbCtrl = 0x4 | (len<<8);
//  val = vtp->eb.EbCtrl;
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
        printf("vtpEbTiReadEvent: TIMEOUT ERROR\n");
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

int
vtpEbReadEvent(uint32_t *pBuf, uint32_t maxsize)
{
  int status, cnt = 0;
  CHECKINIT;
  
  while(cnt < maxsize)
  {
    VLOCK;
    status = vtp->eb.EbStatus;
    VUNLOCK;
    
    if(status & 0x2)
      break;
    
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
  
  return vtpDevOpenMASK;
}
