/*
 * File:
 *    vtpConfigTest.c
 *
 * Description:
 *    Test program for the VTP Library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtpLib.h"

extern int nfadc;

int
main(int argc, char *argv[])
{
  printf(" vtpOpen: \n");
  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);

  printf(" vtpInit: \n");
  vtpInit(VTP_INIT_CLK_VXS_250);

  printf(" vtpSerdesStatusAll: \n");
  vtpSerdesStatusAll();

  exit(0);
}
