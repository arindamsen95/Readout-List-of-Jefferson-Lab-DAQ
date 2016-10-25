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
 *     VTP-I2C library
 *
 *----------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "vtpLib.h"
#include "vtp-i2c.h"

static int vtpI2CFD = -1;
const char vtpI2CDev[256] = "/dev/i2c-4me";


int
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

int
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
vtpI2CWrite(int dev, int page, unsigned int addr, unsigned short val)
{

  if(ioctl(vtpI2CFD, I2C_SLAVE, slaveAddr) < 0)
    exit_error(__func__, 1);

  if(page >= 0)
    if(i2c_smbus_write_byte_data(vtpI2CFD, LTM4676_CMD_PAGE, page) < 0)
      exit_error(__func__, 1);

}

