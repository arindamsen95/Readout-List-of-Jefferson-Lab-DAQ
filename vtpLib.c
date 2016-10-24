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
#include <linux/spi/spidev.h>
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

static int vtpI2CFD = -1;
const char vtpI2CDev[256] = "/dev/i2c-4me";

static int vtpSPIFD = -1;
const char vtpSPIDev[256] = "/dev/spi-on-me";

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
vtpV7WriteCfgData(unsigned short *buf, int N)
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
  unsigned short buf[256];
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
  unsigned short val = 0;
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

static int
vtpI2COpen()
{
  if(vtpI2CFD > 0)
    {
      printf("%s: ERROR: VTP I2C already opened.\n",
	     __func__);
      return ERROR;
    }
  
  vtpI2CFD = open(vtpI2CDev, O_RDWR);
  
  if(vtpI2CFD < 0)
    {
      printf("%s: ERROR from open: %s (%d)",
	     __func__, strerror(errno), errno);
      return ERROR;
    }

  return OK;
}

static int
vtpI2CClose()
{
  if(vtpI2CFD < 0)
    {
      printf("%s: ERROR: VTP I2C not opened.\n",
	     __func__);
      return ERROR;
    }

  close(vtpI2CFD);
  return OK;
}

unsigned short
vtpI2CRead(int dev, int page, unsigned int addr)
{
  unsigned int rval = 0;
  unsigned char buf[2];

  if((rval = i2c_smbus_read_word_data(vtpI2CFD, cmd)) < 0)
    exit_error(__func__, 1);	
  
  return (unsigned short)(rval & 0xFFFF);

}

void
vtpI2CWrite(int dev, int page, unsigned int addr, unsigned int val)
{

  if(ioctl(vtpI2CFD, I2C_SLAVE, slaveAddr) < 0)
    exit_error(__func__, 1);

  if(page >= 0)
    if(i2c_smbus_write_byte_data(vtpI2CFD, LTM4676_CMD_PAGE, page) < 0)
      exit_error(__func__, 1);

}

static  uint32_t mode;
static  uint8_t bits = 8;
static  uint32_t speed = 500000;
static  uint16_t delay;

static int
vtpSPIOpen()
{
  int ret;

 if(vtpSPIFD > 0)
    {
      printf("%s: ERROR: VTP SPI already opened.\n",
	     __func__);
      return ERROR;
    }
  
  vtpSPIFD = open(vtpSPIDev, O_RDWR | O_SYNC);
  
  if(vtpSPIFD < 0)
    {
      printf("%s: ERROR from open: %s (%d)",
	     __func__, strerror(errno), errno);
      return ERROR;
    }

  /*
   * spi mode
   */
  ret = ioctl(vtpSPIFD, SPI_IOC_WR_MODE32, &mode);
  if (ret == -1)
    perror("can't set spi mode");
  
  ret = ioctl(vtpSPIFD, SPI_IOC_RD_MODE32, &mode);
  if (ret == -1)
    perror("can't get spi mode");

  /*
   * bits per word
   */
  ret = ioctl(vtpSPIFD, SPI_IOC_WR_BITS_PER_WORD, &bits);
  if (ret == -1)
    perror("can't set bits per word");
  
  ret = ioctl(vtpSPIFD, SPI_IOC_RD_BITS_PER_WORD, &bits);
  if (ret == -1)
    perror("can't get bits per word");
  
  /*
   * max speed hz
   */
  ret = ioctl(vtpSPIFD, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  if (ret == -1)
    perror("can't set max speed hz");
  
  ret = ioctl(vtpSPIFD, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
  if (ret == -1)
    perror("can't get max speed hz");
  
  printf("spi mode: 0x%x\n", mode);
  printf("bits per word: %d\n", bits);
  printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
  
  return OK;
}

static int
vtpSPIClose()
{
  if(vtpSPIFD < 0)
    {
      printf("%s: ERROR: VTP SPI not opened.\n",
	     __func__);
      return ERROR;
    }

  close(vtpSPIFD);
  return OK;
}
/* Routine from Documentation/spi/spidev_test.c */

static void
transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
  int ret;
  
  struct spi_ioc_transfer tr = {
    .tx_buf = (unsigned long)tx,
    .rx_buf = (unsigned long)rx,
    .len = len,
    .delay_usecs = delay,
    .speed_hz = speed,
    .bits_per_word = bits,
  };

  if (mode & SPI_TX_QUAD)
    tr.tx_nbits = 4;
  else if (mode & SPI_TX_DUAL)
    tr.tx_nbits = 2;
  if (mode & SPI_RX_QUAD)
    tr.rx_nbits = 4;
  else if (mode & SPI_RX_DUAL)
    tr.rx_nbits = 2;
  if (!(mode & SPI_LOOP)) {
    if (mode & (SPI_TX_QUAD | SPI_TX_DUAL))
      tr.rx_buf = 0;
		else if (mode & (SPI_RX_QUAD | SPI_RX_DUAL))
		  tr.tx_buf = 0;
  }
  
  ret = ioctl(vtpSPIFD, SPI_IOC_MESSAGE(1), &tr);
  if (ret < 1)
    perror("can't send spi message");
  
  /* if (verbose) */
  /*   hex_dump(tx, len, 32, "TX"); */
  /* hex_dump(rx, len, 32, "RX"); */
}

unsigned int
vtpSPIRead(int dev, unsigned int addr)
{
  unsigned int rval = 0;

  return rval;
}

void
vtpSPIWrite(int dev, unsigned int addr, unsigned int val)
{
  
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
	  return ERROR;
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
	  return ERROR;
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
	  return ERROR;
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
