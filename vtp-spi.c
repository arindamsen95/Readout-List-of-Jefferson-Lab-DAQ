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
 *     VTP-SPI library
 *
 *----------------------------------------------------------------------------*/

#include <linux/spi/spidev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "vtpLib.h"
#include "vtp-spi.h"

static int vtpSPIFD = -1;
const char vtpSPIDev[256] = "/dev/spi-on-me";


static  uint32_t mode;
static  uint8_t bits = 8;
static  uint32_t speed = 500000;
static  uint16_t delay;

int
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

int
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

