/*
 * File:
 *    vtpLibTest.c
 *
 * Description:
 *    Test program for the VTP Library
 *
 *
 */

#if defined(Linux_armv7l)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtpLib.h"

#define BUILD_TEST_LEN  100

int 
main(int argc, char *argv[]) 
{
  int result, i;
  unsigned int buf[BUILD_TEST_LEN];
  
  if(vtpCheckAddresses() == ERROR)
    exit(-1);
  vtpOpen(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN);
//  if(vtpOpen(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN) != OK)
//    goto CLOSE;
  
//  vtpZ7CfgLoad("/usr/clas12/release/1.3.0/parms/firmwares/fe_vtp_hallb_z7.bin");
  
//  vtpV7CfgStart();
//  vtpV7CfgLoad("/usr/clas12/release/1.3.0/parms/firmwares/fe_vtp_hallb_v7_ec.bin");
//  vtpV7CfgEnd();
  
//  vtpInit(1);
  vtpInit(VTP_INIT_SKIP);
  vtpSetBlockLevel(1);
  vtpDmaInit();

  vtpEbReset();  
  vtpEbBuildTestEvent(BUILD_TEST_LEN);      // BUILD_TEST_LEN 32bit word test event size
  
  vtpDmaStart(0x20000000, 1000);  // write @ 50% point in system memory, maximum 10000 bytes
  result = vtpDmaWaitDone();
  printf("result = %d\n", result);
/*
  result = vtpEbReadTestEvent(buf, BUILD_TEST_LEN);
  printf("result = %d\n", result);
  for(i=0; i<result; i++)
  {
    if(!(i%4))
      printf("\n%08X:", i*4);
    printf(" %08X", buf[i]);
  }
  printf("\n");
  */
 CLOSE:
  vtpClose(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN);

  exit(0);
}

#else

main()
{
  return;
}

#endif

