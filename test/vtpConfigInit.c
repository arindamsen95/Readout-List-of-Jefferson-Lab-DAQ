/*
 * File:
 *    vtpConfigInit.c
 *
 * Description:
 *    Initialize VTP with provided or default config file
 *
 *   if a filename is provided, use it.  otherwise use the default
 *
 * Usage:
 *      vtpConfigInit <optional filename>
 *
 */
#define VTP

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "vtp.h"
#include "vtpLib.h"
#include "vtpConfig.h"


#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);

int
main(int argc, char *argv[])
{
  int stat;
  int useConfigFile = 1;
  char filename[250];
  char rol_usrConfig[250] = "none";
  char shortHostname[HOST_NAME_MAX];
  char buf[1000];
  const char *fwpath="/home/moffit/work/vtp/vtp/firmware";
  const char *z7file="fe_vtp_hallb_z7.bin";
  const char *v7file="fe_vtp_v7_nps.bin";


  if(argc > 1)
    {
      /* assume the only argument is the path to the config file */
      strncpy(filename, argv[1], sizeof(filename));
    }
  else
    {
      stat = getShortHostname(shortHostname);
      sprintf(filename, "/home/sbs-onl/cfg/%s_apv.cfg",shortHostname);
      sprintf(rol_usrConfig, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);
    }

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);

  /* Load firmware here */
  sprintf(buf, "%s/%s", fwpath, z7file);
  if(vtpZ7CfgLoad(buf) != OK)
    {
      printf("Z7 programming failed... (%s)\n", buf);
    }

  printf("loading V7 firmware...\n");
  sprintf(buf, "%s/%s", fwpath, v7file);
  if(vtpV7CfgLoad(buf) != OK)
    {
      printf("V7 programming failed... (%s)\n", buf);
    }

  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpCheckMutexHealth(1);

  vtpLock();
  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpUploadAllPrint();
  vtpUnlock();
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
  compile-command: "make -k -B vtpConfigInit"
  End:
 */
