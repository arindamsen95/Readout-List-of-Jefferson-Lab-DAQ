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
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "vtpLib.h"


static int vtpDevOpenMASK = 0;
static int vtpFPGAFD = -1;
const char vtpFPGADev[256] = "/dev/uio0";

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

int
vtpSerdesSetLoopback(int type, uint16_t dev, uint8_t lb_select)
{
  volatile SERDES_REGS *sdev;
  CHECKINIT;
  CHECKTYPEDEV;
  
  if(lb_select > 7)
    {
      printf("%s: ERROR: Invalid loopback selection (%d)\n",
	     __func__, lb_select);
      return ERROR;
    }
  
  VLOCK;
  sdev->Ctrl =
    (sdev->Ctrl &~ VTP_SERDES_CTRL_LOOPBACK_MASK) | (lb_select<<6);
  VUNLOCK;
  
  return OK;
}

int
vtpSerdesSoftErrorReset(int type, uint16_t dev, int enable)
{
  volatile SERDES_REGS *sdev;
  CHECKINIT;
  CHECKTYPEDEV;

  VLOCK;
  if(enable)
    sdev->Ctrl |= VTP_SERDES_CTRL_ER_CNT_RST;
  else
    sdev->Ctrl &= ~VTP_SERDES_CTRL_ER_CNT_RST;
  VUNLOCK;

  return OK;
}

int
vtpSerdesPower(int type, uint16_t dev, int enable)
{
  volatile SERDES_REGS *sdev;
  CHECKINIT;
  CHECKTYPEDEV;

  VLOCK;
  if(enable)
    sdev->Ctrl &= ~VTP_SERDES_CTRL_POWERDOWN;
  else
    sdev->Ctrl |= VTP_SERDES_CTRL_POWERDOWN;
  VUNLOCK;

  return OK;
}

int
vtpSerdesGTReset(int type, uint16_t dev, int enable)
{
  volatile SERDES_REGS *sdev;
  CHECKINIT;
  CHECKTYPEDEV;

  VLOCK;
  if(enable)
    sdev->Ctrl |= VTP_SERDES_CTRL_GT_RESET;  /* Toggle On  */
  else
    sdev->Ctrl &= ~VTP_SERDES_CTRL_GT_RESET; /* Toggle Off */
  VUNLOCK;

  return OK;
}

int
vtpSerdesReset(int type, uint16_t dev, int enable)
{
  volatile SERDES_REGS *sdev;
  CHECKINIT;
  CHECKTYPEDEV;

  VLOCK;
  if(enable)
    sdev->Ctrl |= VTP_SERDES_CTRL_RESET;  /* Toggle On  */
  else
    sdev->Ctrl &= ~VTP_SERDES_CTRL_RESET; /* Toggle Off */
  VUNLOCK;

  return OK;
}

int
vtpSerdesStatus(int type, uint16_t dev, int pflag)
{
  volatile SERDES_REGS *sdev;
  uint32_t status = 0;
  CHECKINIT;
  CHECKTYPEDEV;
  
  VLOCK;
  status = sdev->Status;
  VUNLOCK;

  if(pflag)
    {
      printf("\n");
      printf("    Hard   Soft   ---Lane---          Soft Error  TX      Reset     Link\n");
      printf("PP  Error  Error  0     1      Ch     Count       PLL    TX   RX    Reset\n");
      printf("--------------------------------------------------------------------------------\n");
    }
  
  printf("%2d  ", dev);
  printf("%s    ", (status & VTP_SERDES_STATUS_HARD_ERR)?"ERR":"---");
  printf("%s    ", (status & VTP_SERDES_STATUS_SOFT_ERR)?"ERR":"---");
  printf("%s  ", (status & VTP_SERDES_STATUS_LANE_UP(0))?" UP ":"DOWN");
  printf("%s   ", (status & VTP_SERDES_STATUS_LANE_UP(1))?" UP ":"DOWN");
  printf("%s   ", (status & VTP_SERDES_STATUS_CHUP)?" UP ":"DOWN");
  printf("%3d         ", (status & VTP_SERDES_STATUS_SOFT_ERR_CNT_MASK)>>8);
  printf("%s   ", (status & VTP_SERDES_STATUS_TX_LOCK)?"LOCK":"----");
  printf("%s ", (status & VTP_SERDES_STATUS_TX_RST_DONE)?"DONE":"----");
  printf("%s  ", (status & VTP_SERDES_STATUS_RX_RST_DONE)?"DONE":"----");
  printf("%s", (status & VTP_SERDES_STATUS_LINK_RST)?"IN PROGRESS":"----");
  printf("\n");
  
  return OK;
}

int
vtpVXSSerdesSetLoopback(uint16_t pp, uint8_t lb_select)
{
  return vtpSerdesSetLoopback(VTP_SERDES_VXS, pp, lb_select);
}

int
vtpVXSSerdesSoftErrorReset(uint16_t pp, int enable)
{
  return vtpSerdesSoftErrorReset(VTP_SERDES_VXS, pp, enable);
}

int
vtpVXSSerdesPower(uint16_t pp, int enable)
{
  return vtpSerdesPower(VTP_SERDES_VXS, pp, enable);
}

int
vtpVXSSerdesGTReset(uint16_t pp, int enable)
{
  return vtpSerdesGTReset(VTP_SERDES_VXS, pp, enable);
}

int
vtpVXSSerdesReset(uint16_t pp, int enable)
{
  return vtpSerdesReset(VTP_SERDES_VXS, pp, enable);
}

int
vtpVXSSerdesStatus(uint16_t pp, int pflag)
{
  return vtpSerdesStatus(VTP_SERDES_VXS, pp, pflag);
}

int
vtpQSFPSerdesSetLoopback(uint16_t qsfp, uint8_t lb_select)
{
  return vtpSerdesSetLoopback(VTP_SERDES_QSFP, qsfp, lb_select);
}

int
vtpQSFPSerdesSoftErrorReset(uint16_t qsfp, int enable)
{
  return vtpSerdesSoftErrorReset(VTP_SERDES_QSFP, qsfp, enable);
}

int
vtpQSFPSerdesPower(uint16_t qsfp, int enable)
{
  return vtpSerdesPower(VTP_SERDES_QSFP, qsfp, enable);
}

int
vtpQSFPSerdesGTReset(uint16_t qsfp, int enable)
{
  return vtpSerdesGTReset(VTP_SERDES_QSFP, qsfp, enable);
}

int
vtpQSFPSerdesReset(uint16_t qsfp, int enable)
{
  return vtpSerdesReset(VTP_SERDES_QSFP, qsfp, enable);
}

int
vtpQSFPSerdesStatus(uint16_t qsfp, int pflag)
{
  return vtpSerdesStatus(VTP_SERDES_QSFP, qsfp, pflag);
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

  VLOCK;
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

  printf(" size = %d\n", sizeof(ZYNC_REGS));
  
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
