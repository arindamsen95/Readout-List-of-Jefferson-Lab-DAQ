/*
 * File:
 *    vtpFiberStatus.c
 *
 * Description:
 *    Show fiber status of VTP
 *
 * Usage:
 *      vtpFiberStatus
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtp.h"
#include "vtpLib.h"
#include "vtpConfig.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);

int main(int argc, char *argv[])
{
  int stat;
  char rol_usrConfig[250];

  char shortHostname[HOST_NAME_MAX];

  stat = getShortHostname(shortHostname);
  sprintf(rol_usrConfig, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);

  printf("rol_usrConfig = %s", rol_usrConfig);

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpStatus(1);

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffff);

  vtpMpdDisable(0xffffffff);
  vtpMpdEnable(0xffffffff);

  usleep(10);
  vtpStatus(0);
  vtpMpdPrintStatus(0,0);


 CLOSE:
  vtpClose(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);

  return 0;

}

int
getShortHostname(char *shortHostname)
{
  char longHostname[HOST_NAME_MAX];
  char *tempShort;
  int rval;

  rval = gethostname(longHostname, HOST_NAME_MAX);
  if(rval < 0)
    {
      perror("gethostname");
      return rval;
    }

  printf("long Hostname : %s\n", longHostname);

  tempShort = strtok((char *)&longHostname,".");
  if(tempShort != NULL)
    {
      printf("short Hostname : >%s<\n", tempShort);
      strcpy(shortHostname,tempShort);
    }
  else
    printf("null\n");

  return rval;
}

/*
  Local Variables:
  compile-command: "make -k vtpMpdFiberStatus"
  End:
 */
