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

extern int vtpConfig(char *fname);
extern void vtpInitGlobals();

uint32_t ROCID = 10;


void
coda_download()
{
  char buf[1000];
  const char *z7file="fe_vtp_vxs_readout_z7_jul23.bin";
  const char *v7file="fe_vtp_vxs_readout_v7_jul22.bin";

  /* Open VTP library */
  int32_t stat = vtpOpen(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN);
  if(stat < 0)
    {
      printf(" Unable to Open VTP driver library.\n");
    }

  /* Load firmware here */
  sprintf(buf, "/daqfs/diskless/CentOS7-devel/armv7l/root/usr/local/src/vtp/firmware/%s", z7file);
  if(vtpZ7CfgLoad(buf) != OK)
    {
      printf("Z7 programming failed... (%s)\n", buf);
    }

  printf("loading V7 firmware...\n");
  sprintf(buf, "/daqfs/diskless/CentOS7-devel/armv7l/root/usr/local/src/vtp/firmware/%s", v7file);
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
  vtpRocSetID(ROCID);
  vtpRocStatus(0);

}

void
coda_prestart()
{
  /* Read Config file and Intialize VTP */
  vtpInitGlobals();
  vtpConfig("/daqfs/coda/3.10_devel/src/vtp/vtp_readout/config/vtpRocTriggered.cnf");


  /* Reset the ROC */
  vtpRocReset(0);

  /* Initialize the TI Interface */
  vtpTiLinkInit();


   /* Get Stream connection info from file. Then Setup the VTP connection registers manually and connect */
  {
    uint8_t ipaddr[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    uint8_t mac[6];
    uint32_t destip;
    uint8_t destip2[4];
    uint32_t destipport;
    uint16_t destipport2;

    // VTP IP Address
    ipaddr[0]=129; ipaddr[1]=57; ipaddr[2]=109; ipaddr[3]=124;
    /* ipaddr[0]=129; ipaddr[1]=57; ipaddr[2]=109; ipaddr[3]=128; */
    // Subnet mask
    subnet[0]=255; subnet[1]=255; subnet[2]=255; subnet[3]=0;
    // gateway
    gateway[0]=129; gateway[1]=57; gateway[2]=109; gateway[3]=1;
    // VTP MAC
    mac[0]=0xce; mac[1]=0xba; mac[2]=0xf0; mac[3]=0x03; mac[4]=0x00; mac[5]=0xa8;
    /* mac[0]=0xce; mac[1]=0xba; mac[2]=0xf0; mac[3]=0x03; mac[4]=0x00; mac[5]=0xfa; */
    // Destination IP
    destip = 0x81396DA2;
    // Desination Port
    destipport = 46101;

    printf(" ipaddr=%d.%d.%d.%d\n",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
    printf(" subnet=%d.%d.%d.%d\n",subnet[0],subnet[1],subnet[2],subnet[3]);
    printf(" gateway=%d.%d.%d.%d\n",gateway[0],gateway[1],gateway[2],gateway[3]);
    printf(" mac=%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    printf(" destip=0x%x\n",destip);
    printf(" destipport=%d\n",destipport);

      /* Set VTP connection registers */
      vtpRocSetTcpCfg(
          ipaddr,
          subnet,
          gateway,
          mac,
	  destip,
         destipport
      );

      /*Read it back to to make sure */
       vtpRocGetTcpCfg(
          ipaddr,
          subnet,
          gateway,
          mac,
          destip2,
          &destipport2
      );
       printf(" Readback of TCP CLient Registers:\n");
       printf("   ipaddr=%d.%d.%d.%d\n",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
       printf("   subnet=%d.%d.%d.%d\n",subnet[0],subnet[1],subnet[2],subnet[3]);
       printf("   gateway=%d.%d.%d.%d\n",gateway[0],gateway[1],gateway[2],gateway[3]);
       printf("   mac=%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
       printf("   destip=%d.%d.%d.%d\n",destip2[0],destip2[1],destip2[2],destip2[3]);
       printf("   destipport=%d\n",destipport2);


       uint32_t emuData[] = {0x634d7367,0x20697320,0x636f6f6c,6,0,4196352,1,1};
       emuData[4] = ROCID;

       /* Make the Connection . Pass Data needed to complete connection with the EMU */
       /*
	 for (ii=0;ii<8;ii++) {
	 emuData[ii] = htonl(emuData[ii]);
	 }
       */
       vtpRocTcpConnect(1,emuData,8);
       //vtpRocTcpConnect(1,0,0);
  }

  /* Reset and Configure the MIG and ROC Event Builder */
  vtpRocMigReset();

  vtpRocEbStop();
  vtpRocEbConfig(0x010005,0x010002,0x010003,0x1004);

  vtpRocEbioReset();


  /* Set TI readout to Hardware mode */
  vtpTiLinkSetMode(1);

  /* Enable Async&EB Events for ROC   bit2 - Async, bit1 - Sync, bit0 V7-EB */
  vtpRocEnable(0x5);

  vtpRocEbStart();

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

  printf("\n Enter to prestart\n");
  coda_prestart();

  printf("\n Enter to end\n");
  coda_end();

  printf("exit me\n");
  exit(0);
}

/*
  Local Variables:
  compile-command: "make vtpRocTcpConnect"
  End:
 */
