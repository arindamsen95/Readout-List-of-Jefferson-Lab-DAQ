
/*
 * File:
 *    vtpMpdHistoMode.c
 *
 * Description:
 *    Sample mode data taking for fish / goalpost plots
 *
 *   if a filename is provided, only initialize those MPD defined
 *
 *   otherwise, only attempt to initialize MPDs found via the fiber+serial
 *    connection (channel must be up).
 *
 *
 * Usage:
 *      vtpMpdSampleMode
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/signal.h>
#include "vtp.h"
#include "vtpLib.h"
#include "vtpConfig.h"
#include "vtpMpdConfig.h"
#define VTP
#include "mpdLib.h"
#include "vtp_mpd_setup.c"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);


char cfgFilename[255];
char progName[255];

void mpdHisto(char *outfile, int h_gain);

int sig_ctrl = 0; // 0 = force quit, 1 = soft quit ... when CTRL-C is pressed
int verbose_level = 0; // 0 = quit, 1 = some output, 2 = verbose output
void sig_handler(int signo);

void
usage()
{
  int h_gain=5;
  int mask_mpd = 0x0; // if bit set, corresponding MPD is masked; first bit is MPD with lower slot ID

  printf("SYNTAX: %s config_file out_data_file par0 par1\n", progName);
  printf("         HISTO mode\n");
  printf("               par0 = gain (%d)\n",h_gain);
  printf("               par1 = MPD mask, if bit set corresponding MPD configuration disabled (0x%x)\n",mask_mpd);
}


int
main(int argc, char *argv[])
{
  int stat;
  char outfile[255];
  char rol_usrConfig[250];
  char shortHostname[HOST_NAME_MAX];
  int useConfigFile = 0;
  int h_gain=5;
  int mask_mpd = 0x0; // if bit set, corresponding MPD is masked; first bit is MPD with lower slot ID
  int mpd_slot0=999; // lowest MPD address (slot number), will be determined below

  if (argc != 5)
    {
      usage();
      exit(-1);
    }
  else
    {
      strncpy(cfgFilename, argv[1], 255); /* config_file */
      strncpy(outfile, argv[2], 255);  /* out_data_file */
      h_gain = atoi(argv[3]);  /* par0 = gain */
      mask_mpd = strtoll(argv[4],NULL,16);  /* par1 = MPD mask, if bit set corresponding MPD configuration disabled  */
    }

  signal(SIGINT, sig_handler);
  signal(SIGTSTP, sig_handler);


  printf("VTP - MPD SAMPLE check\n");
  printf("----------------------------\n");
  printf(" outfile = %s\n",outfile);

  stat = getShortHostname(shortHostname);
  sprintf(rol_usrConfig, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpCheckMutexHealth(1);
  vtpLock();

  /* just make a call to vtp_mpd_setup() */
  vtp_mpd_setup(cfgFilename);

  //mpdGStatus(0);

  mpdHisto(outfile, h_gain);

  vtpUnlock();

  return 0;
}

void
mpdHisto(char *outfile, int h_gain)
{
  /**********************************
   * SAMPLES TEST
   * sample APV output at 40 MHz
   * only for testing not for normal daq
   **********************************/
  int acq_mode = 0x4;
  FILE *fout;
  int k,h,i,j, error_count;
  uint32_t hcount;
  int sch0, sch1;
  int fnMPD = mpdGetNumberMPD();
#define MAX_HDATA 4096
  uint32_t hdata[MAX_HDATA]; // histogram data buffer

  /********************************************
   * HISTO Mode speed optimized; sampled data are histogrammed
   * only for testing, non for normal daq
   ********************************************/

  if (acq_mode & 0x4) {

    fout = fopen(outfile,"w");
    if (fout == NULL) { fout = stdout; }

    fprintf(fout,"# MODE=%d\n# OUTPUT=mpd adc gain ch0 ... ch4095\n",acq_mode);

    for (k=0;k<fnMPD;k++) { // only active mpd set
      i = mpdSlot(k);

      mpdSetAcqMode(i, "histo");

      // load pedestal and thr default values
      mpdPEDTHR_Write(i);

      // set ADC gain
      mpdADS5281_SetGain(i,0,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain);
      mpdADS5281_SetGain(i,1,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain,h_gain);
    }

    for (j=0;j<16;j++) {  // loop on adc channels

      for (k=0;k<fnMPD;k++) { // only active mpd set
	i = mpdSlot(k);
	//
	mpdHISTO_Clear(i,j,0);

	mpdHISTO_Start(i,j);

      }

      sleep(1); // wait to get some data

      for (k=0;k<fnMPD;k++) { // only active mpd set
	i = mpdSlot(k);

	mpdHISTO_Stop(i,j);

	hcount=0;
	mpdHISTO_GetIntegral(i,j,&hcount);
	error_count = mpdHISTO_Read(i,j, hdata);

	if (error_count != OK) {
	  printf("ERROR: reading histogram data from MPD %d ADCch %d\n",i,j);
	  continue;
	}

	printf(" ### histo peaks integral MPD/ADCch = %d / %d (total Integral= %d)\n",i,j,hcount);
	sch0=-1;
	sch1=0;

	fprintf(fout,"# MPD=%d\n# ADC=%d\n# GAIN=%d\n",i,j,h_gain);
	fprintf(fout,"%d %d %d",i,j,h_gain);

	for (h=0;h<4096;h++) {

	  fprintf(fout," %d",hdata[h]);

	  if (hdata[h]>0) {
	    if (sch0<0) { printf(" first bin= %d: ", h); } else {
	      if ((sch0+1) < h) { printf( "%d last bin= %d\n first bin= %d: ",sch1,sch0,h); sch1=0; }
	    }
	    sch1 += hdata[h];
	    sch0=h;
	  }

	  //	if ((h%64) == 63) { printf("\n"); }
	}

	fprintf(fout, "\n");

	if (sch1>0) {
	  printf( "%d last bin= %d\n",sch1,sch0);
	} else { printf("\n");}

      }

    } // end loop adc channels in histo mode

    if (fout != stdout)  fclose(fout);


    /*
      mpdGStatus(0);
      for (k=0;k<fnMPD;k++) {
      i = mpdSlot(k);
      mpdApvStatus(i, 0xFFFF);
      }
    */




  } // histo mode ends

}


void sig_handler(int signo)
{
  //  int i, status;
  int k, fnMPD;

  switch (signo) {
  case SIGINT:
    printf("\nCTRL-C pressed, (level = %d)\n\n",sig_ctrl);

    if (sig_ctrl == 0) { // force quit
      fnMPD = mpdGetNumberMPD();
      for (k=0;k<fnMPD;k++)
	{
	  mpdDAQ_Disable(mpdSlot(k));
	}

      vtpUnlock();
      exit(1);  /* exit if CRTL/C is issued */
    }

    if (sig_ctrl == 1) { // soft quit
      sig_ctrl = 2;
    }
    break;
  case SIGTSTP:
    verbose_level += 1;
    verbose_level = verbose_level % 3;
    printf("Verbose level = %d\n",verbose_level);
    break;

  }
  return;
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
  compile-command: "make -k -B vtpMpdHistoMode"
  End:
 */
