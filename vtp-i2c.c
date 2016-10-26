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
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include "vtpLib.h"
#include "vtp-i2c.h"

static int vtpI2CFD = -1;
const char vtpI2CDev[256] = "/dev/i2c-0";

#define CHECKI2CFD \
  if(vtpI2CFD > 0) \
    {							\
      printf("%s: ERROR: VTP I2C already opened.\n",	\
	     __func__);					\
      return ERROR;					\
    }							\

int
vtpI2COpen()
{
  CHECKI2CFD;

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
  CHECKI2CFD;
  close(vtpI2CFD);
  return OK;
}

int
vtpI2CSelectSlave(uint8_t slaveAddr)
{
  int lerrno;

  if(ioctl(vtpI2CFD, I2C_SLAVE, slaveAddr) < 0)
    {
      lerrno = errno;
      printf("%s: ioctl ERROR %d: %s\n", __func__,
	     lerrno, strerror(lerrno));
      return ERROR;
    }
      
  return OK;
}


uint8_t
vtpI2CRead8(uint8_t slaveAddr, uint8_t cmd)
{
  uint32_t rval = 0;
  int lerrno = 0;

  CHECKI2CFD;
  if(vtpI2CSelectSlave(slaveAddr) != OK)
    return ERROR;

  if((rval = i2c_smbus_read_byte_data(vtpI2CFD, cmd)) < 0)
	{
	  lerrno = errno;
	  printf("%s: i2c read word ERROR %d: %s\n", __func__,
		 lerrno, strerror(lerrno));
	  return ERROR;
	}
  
  return (uint8_t)(rval & 0xFF);
}

uint16_t
vtpI2CRead16(uint8_t slaveAddr, uint8_t cmd)
{
  uint32_t rval = 0;
  int lerrno = 0;

  CHECKI2CFD;
  if(vtpI2CSelectSlave(slaveAddr) != OK)
    return ERROR;

  if((rval = i2c_smbus_read_word_data(vtpI2CFD, cmd)) < 0)
	{
	  lerrno = errno;
	  printf("%s: i2c read word ERROR %d: %s\n", __func__,
		 lerrno, strerror(lerrno));
	  return ERROR;
	}
  
  return (uint8_t)(rval & 0xFFFF);
}

int
vtpI2CWrite8(uint8_t slaveAddr, uint8_t cmd, uint8_t val)
{
  int lerrno = 0;

  CHECKI2CFD;

  if(vtpI2CSelectSlave(slaveAddr) != OK)
    return ERROR;
  
  if(i2c_smbus_write_byte_data(vtpI2CFD, cmd, val) < 0)
    {
      lerrno = errno;
      printf("%s: i2c ERROR %d: %s\n", __func__,
	     lerrno, strerror(lerrno));
      return ERROR;
    }

  return OK;
}

int
vtpI2CWrite16(uint8_t slaveAddr, uint8_t cmd, uint16_t val)
{
  int lerrno = 0;

  CHECKI2CFD;

  if(vtpI2CSelectSlave(slaveAddr) != OK)
    return ERROR;
  
  if(i2c_smbus_write_word_data(vtpI2CFD, cmd, val) < 0)
    {
      lerrno = errno;
      printf("%s: i2c read word ERROR %d: %s\n", __func__,
	     lerrno, strerror(lerrno));
      return ERROR;
    }

  return OK;
}

