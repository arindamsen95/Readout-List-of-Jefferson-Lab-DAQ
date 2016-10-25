#ifndef VTP_I2C_H
#define VTP_I2C_H
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
 *     Header file for VTP-I2C 
 *
 *----------------------------------------------------------------------------*/


int vtpI2COpen();
int vtpI2CClose();

unsigned short vtpI2CRead(int dev, int page, unsigned int addr);
void vtpI2CWrite(int dev, int page, unsigned int addr, unsigned short val);



#endif /* VTP_I2C_H */
