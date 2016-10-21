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

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "vtpLib.h"


static int vtpFD;
const char vtpDev[256] = "/dev/uio0";

int
vtpOpen()
{
  if(vtpFD > 0)
    {
      printf("%s: ERROR: VTP already opened.\n",
	     __FUNCTION__);
      return ERROR;
    }
  
  vtpFD = open(vtpDev, O_RDWR);
  
  if(vtpFD < 0)
    {
      perror("vtpOpen: ERROR");
      return ERROR;
    }
  
  
  return OK;
}

int
vtpClose()
{
  if(vtpFD < 0)
    {
      printf("%s: ERROR: VTP not opened.\n",
	     __FUNCTION__);
      return ERROR;
    }

  close(vtpFD);
  return OK;
}
