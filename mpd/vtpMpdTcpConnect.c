/*
 * File:
 *    vtpRocTcpConnect.c
 *
 * Description:
 *    Configure the VTP 10Gb link
 *
 */


#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "vtpLib.h"
#include "vtpConfig.h"
#include "vtpMpdConfig.h"

char APV_CONFIG_FILENAME[250];
char VTP_CONFIG_FILENAME[250];

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);

uint32_t emuData[] = {0x634d7367,0x20697320,0x636f6f6c,6,0,4196352,1,1};
uint32_t ROCID = 10;

void
vtp_mpd_setup()
{
  int stat = 0;
  char rol_usrConfig[250];
  char rol_usrString[250];
  char shortHostname[HOST_NAME_MAX];

  stat = getShortHostname(shortHostname);
  sprintf(rol_usrConfig, "/home/sbs-onl/cfg/%s_apv.cfg",shortHostname);
  sprintf(rol_usrString, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);

  if(vtpMpdConfigInit(rol_usrConfig) == ERROR)
    {
      printf("%s: ERROR: APV Configuration load error for %s (CHECK THIS FIRST)\n",
	     __func__, rol_usrConfig);

      return;
    }
  else
    {
      strncpy(APV_CONFIG_FILENAME, rol_usrConfig, 250);
    }

  vtpMpdConfigLoad();

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffffffffffff);

  /* ... the VTP holds all the MPD event build stuff in reset... */
  vtpMpdDisable(0xffffffffffffffff);

  // skip MPD setup

  /* and clear out it's buffers */
  vtpRocMigReset(1);
  vtpRocMigReset(0);

  vtpV7SetResetSoft(1);
  vtpV7SetResetSoft(0);

  /* Read Config file and Intialize VTP */
  /* extern int vtpReadConfigFile(char *filename); */
  /* extern int vtpDownloadAll(); */

  vtpInitGlobals();

  if(vtpReadConfigFile(rol_usrString) == ERROR)
    {
      printf("%s: Error using rol->usrString %s.\n",
	     __func__, rol_usrString);
#define DEFAULT_VTP_CONFIG "/home/sbs-onl/vtp/cfg/sbsvtp3.config"

      printf("  trying %s\n",
	     DEFAULT_VTP_CONFIG);

      if(vtpConfig(DEFAULT_VTP_CONFIG) == ERROR)
	{
	  printf("%s: ERROR: Error loading VTP configuration file", __func__);
	  return;
	}
      else
	{
	  strncpy(VTP_CONFIG_FILENAME, DEFAULT_VTP_CONFIG, 250);
	}
    }
  else
    {
      strncpy(VTP_CONFIG_FILENAME, rol_usrString, 250);
    }
  vtpDownloadAll();


}


void
coda_download()
{
  char buf[1000];
  const char *fwpath="/home/sbs-onl/vtp/vtp/firmware";
  const char *z7file="fe_vtp_vxs_readout_z7.bin";
  const char *v7file="fe_vtp_v7_mpd.bin";

  /* Open VTP library */
  int32_t stat = vtpOpen(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN);
  if(stat < 0)
    {
      printf(" Unable to Open VTP driver library.\n");
    }

  /* Load firmware here */
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

  if(vtpInit(VTP_INIT_CLK_VXS_250))
  {
    printf("vtpInit() **FAILED**. User should not continue.\n");
    return;
  }


  /* Configure the ROC*/
  vtpRocReset(0);
  vtpRocConfig(ROCID, 0, 8, 0);  /* Use defaults for other parameters MaxRecSize, Max#Blocks, timeout*/
  emuData[4] = ROCID;  /* define ROCID in the EB Connection data as well*/

  vtpRocStatus(0);

}

void
coda_prestart()
{
  uint32_t emuip = 0, emuport = 0;

  vtp_mpd_setup();

  /* Reset the ROC */
  vtpRocReset(0);

  /* Initialize the TI Interface */
  vtpTiLinkInit();

  emuip = 0x8139C067; /* adaq2.jlab.org 129.57.192.103 8139C067 */
  emuport = 46111;
  printf("%s: INFO: EMU IP = 0x%08x  Port= %d\n",
	 __func__, emuip, emuport);

   /* Readback the VTP 10Gig network registers and connect */
  unsigned char ipaddr[4];
  unsigned char subnet[4];
  unsigned char gateway[4];
  unsigned char mac[6];
  unsigned char destipaddr[4];
  unsigned short destipport;

  /*Read it back to to make sure */
  vtpRocGetTcpCfg(ipaddr, subnet, gateway, mac, destipaddr, &destipport);

  /* If the ipaddr is all zero's, then the config file was not loaded */
  if(ipaddr[0] == 0)
    {
      printf("%s: ERROR: Invalid Network Settings.  Check VTP Configuration File",
	     __func__);
      return;
    }

  /* Set the emu ip and port */
  vtpRocSetTcpCfg(ipaddr, subnet, gateway, mac, emuip, emuport);

  printf(" Readback of TCP CLient Registers:\n");
  printf("   ipaddr=%d.%d.%d.%d\n",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
  printf("   subnet=%d.%d.%d.%d\n",subnet[0],subnet[1],subnet[2],subnet[3]);
  printf("   gateway=%d.%d.%d.%d\n",gateway[0],gateway[1],gateway[2],gateway[3]);
  printf("   mac=%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  printf("   emuip=0x%08x\n",emuip);
  printf("   emuport=%d\n",emuport);


  /* Make the Connection . Pass Data needed to complete connection with the EMU */
  vtpRocTcpConnect(1,emuData,8);

  /* Reset and Configure the MIG and ROC Event Builder */
  vtpRocMigReset();

  PP_CONF ppInfo[16];
  memset(ppInfo, 0, sizeof(ppInfo));
#define VTPMPD_BANK 3561

  int32_t ppmask = 0;  //(payload ports(vme slot) 3(9), 6(15), 12(18), 13(4))
  vtpRocEbStop();
  vtpRocEbInit(VTPMPD_BANK,6,7);   // define bank1 tag = 3562, bank2 tag = 6, bank3 tag = 7
  vtpRocEbConfig(ppInfo,0);  // blocklevel=0 will skip setting the block level

  vtpRocEbioReset();


  /* Set TI readout to Hardware mode */
  vtpTiLinkSetMode(1);

  /* Enable Async&EB Events for ROC   bit2 - Async, bit1 - Sync, bit0 V7-EB */
  vtpRocEnable(0x5);

  /*Send Prestart Event*/
  vtpRocEvioWriteControl(0xffd1,1234,5);

  vtpRocStatus(0);
}

void
coda_end()
{
  /* Disconnect the socket */
  vtpRocTcpConnect(0,0,0);

  vtpClose(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);

}
int
main(int argc, char *argv[])
{
  printf("\n Enter to download\n");
  coda_download();

  sleep(1);
  printf("\n Enter to prestart\n");
  coda_prestart();

  sleep(1);
  printf("\n Enter to end\n");
  coda_end();

  printf("exit me\n");
  exit(0);
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
  compile-command: "make vtpMpdTcpConnect"
  End:
 */
