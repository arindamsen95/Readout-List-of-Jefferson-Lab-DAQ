
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
char *apvbuffer;
char *errorbuffer;
char *bufp;

char cfgFilename[255];
char progName[255];

extern int I2C_SendStop(int id);
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

  if(vtpMpdConfigInit(cfgFilename) == ERROR)
    {
      printf("ERROR: Error in configuration file\n\t%s", cfgFilename);
      return -1;
    }

  signal(SIGINT, sig_handler);
  signal(SIGTSTP, sig_handler);

  apvbuffer = (char *)malloc(1024*50*sizeof(char));
  errorbuffer = (char *)malloc(1024*50*sizeof(char));

  vtpMpdConfigLoad();

  char *rol_usrConfig = "/home/sbs-onl/vtp/cfg/sbsvtp3.config";

  printf("VTP - MPD SAMPLE check\n");
  printf("----------------------------\n");
  printf(" outfile = %s\n",outfile);

  vtpOpen(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
  vtpInit(VTP_INIT_CLK_VXS_250);

  vtpInitGlobals();
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  vtpCheckMutexHealth(1);
  vtpLock();

  uint32_t vtpFiberMaskToInit;

  vtpMpdFiberReset();
  vtpMpdFiberLinkReset(0xffffffff);

  vtpMpdDisable(0xffffffff);
  vtpMpdEnable(0xffffffff);

  vtpStatus(0);
  vtpMpdPrintStatus(0,0);
  vtpMpdPrintStatus(0,1);

  /*****************
   *   MPD SETUP
   *****************/
  int rval = OK;
  unsigned int errSlotMask = 0;

  mpdSetPrintDebug(0);

  // discover MPDs and initialize memory mapping

  // In VTP mode, par1(fiber mask) and par3(number of mpds) are not used in mpdInit(par1, par2, par3, par4)
  // Instead, they come from the configuration file
  int initFlag = MPD_INIT_FIBER_MODE;
  unsigned int chanmask;

  chanmask = mpdGetVTPFiberMask();

  mpdInit(chanmask, 0, 32, initFlag);

  int fnMPD = mpdGetNumberMPD();


  //fnMPD = 1;
  if (fnMPD<=0) { // test all possible vme slot ?
    printf("ERR: no MPD discovered, cannot continue\n");
    return -1;
  }

  int k,i;
  for (k=0;k<fnMPD;k++) { // get lowest slot
    i = mpdSlot(k);
    mpd_slot0 = (i<mpd_slot0) ? i : mpd_slot0;
  }

  printf(" MPD discovered = %d starting from slot %d (MPD mask 0x%x)\n",fnMPD, mpd_slot0, mask_mpd);

  // APV configuration on all active MPDs
  int error_status = OK;

  for (k=0;k<fnMPD;k++) { // only active mpd set
    i = mpdSlot(k);

    if (mask_mpd & (1<<(i-mpd_slot0))) {
      printf(" MPD in slot %d disabled \n", i);
      mpdSetApvEnableMask(i,0);
      continue;
    }

    int try_cnt = 0;

    mpdHISTO_MemTest(i);

  retry:

    printf(" Try initialize I2C mpd in slot %d\n",i);
    if (mpdI2C_Init(i) != OK) {
      printf("WRN: I2C fails on MPD %d\n",i);
    }

    printf("Try APV discovery and init on MPD slot %d\n",i);
    if (mpdAPV_Scan(i)<=0 && try_cnt < 3 ) { // no apd found, skip next
      try_cnt++;
      printf("failing retrying\n");


      goto retry;
    }



    if( try_cnt == 3 )
      {
	printf("APV blind scan failed for %d TIMES !!!!\n\n", try_cnt);
	errSlotMask |= (1 << i);
      }

    printf(" - APV Reset\n");
    fflush(stdout);
    if (mpdI2C_ApvReset(i) != OK)
      {
	printf(" * * FAILED\n");
	error_status = ERROR;
	errSlotMask |= (1 << i);
      }

    usleep(10);
    I2C_SendStop(i);


    // board configuration (APV-ADC clocks phase)
    printf("Do DELAY setting on MPD slot %d\n",i);
    mpdDELAY25_Set(i, mpdGetAdcClockPhase(i,0), mpdGetAdcClockPhase(i,1));



    // apv configuration
    printf("Configure %d APVs on MPD slot %d\n",mpdGetNumberAPV(i),i);


    // apv configuration
    mpdSetPrintDebug(0);
    printf(" - Configure Individual APVs\n");
    printf(" - - ");
    fflush(stdout);
    int itry, badTry = 0, iapv, saveError = error_status;
    error_status = OK;
    for (itry = 0; itry < 3; itry++)
      {
	if(badTry)
	  {
	    printf(" ******** RETRY ********\n");
	    printf(" - - ");
	    fflush(stdout);
	    error_status = OK;
	  }
	badTry = 0;
	for (iapv = 0; iapv < mpdGetNumberAPV(i); iapv++)
	  {
	    printf("%2d ", iapv);
	    fflush(stdout);

	    if (mpdAPV_Config(i, iapv) != OK)
	      {
		printf(" * * FAILED for APV %2d\n", iapv);
		if(iapv < (mpdGetNumberAPV(i) - 1))
		  printf(" - - ");
		fflush(stdout);
		error_status = ERROR;
		badTry = 1;
	      }
	  }
	printf("\n");
	fflush(stdout);
	if(badTry)
	  {
	    printf(" ***** APV RESET *****\n");
	    fflush(stdout);
	    mpdI2C_ApvReset(i);
	  }
	else
	  {
	    if(itry > 0)
	      {
		printf(" ****** SUCCESS!!!! ******\n");
		fflush(stdout);
	      }
	    break;
	  }

      }

    error_status |= saveError;

    if(error_status == ERROR)
      errSlotMask |= (1 << i);

    // configure adc on MPD
    printf("Configure ADC on MPD slot %d\n",i);
    mpdADS5281_Config(i);

    // configure fir
    // not implemented yet

    // 101 reset on the APV
    printf("Do 101 Reset on MPD slot %d\n",i);
    mpdAPV_Reset101(i);

    // <- MPD+APV initialization ends here

  } // end loop on mpds
  //END of MPD configure

  // summary report
  bufp = (char *) &(apvbuffer[0]);

  rval = sprintf(bufp, "\n");
  if(rval > 0)
    bufp += rval;
  rval = sprintf(bufp, "Configured APVs (ADC 15 ... 0)\n");
  if(rval > 0)
    bufp += rval;

  int ibit;
  int impd, id, iapv;
  for (impd = 0; impd < fnMPD; impd++)
    {
      id = mpdSlot(impd);

      if (mpdGetApvEnableMask(id) != 0)
	{
	  rval = sprintf(bufp, "  MPD %2d : ", id);
	  if(rval > 0)
	    bufp += rval;
	  iapv = 0;
	  for (ibit = 15; ibit >= 0; ibit--)
	    {
	      if (((ibit + 1) % 4) == 0)
		{
		  rval = sprintf(bufp, " ");
		  if(rval > 0)
		    bufp += rval;
		}
	      if (mpdGetApvEnableMask(id) & (1 << ibit))
		{
		  rval = sprintf(bufp, "1");
		  if(rval > 0)
		    bufp += rval;
		  iapv++;
		}
	      else
		{
		  rval = sprintf(bufp, ".");
		  if(rval > 0)
		    bufp += rval;
		}
	    }
	  rval = sprintf(bufp, " (#APV %d)", iapv);
	  if(rval > 0)
	    bufp += rval;
	  if(errSlotMask & (1 << id))
	    {
	      rval = sprintf(bufp, " INIT ERRORS\n");
	      if(rval > 0)
		bufp += rval;
	    }
	  else
	    {
	      rval = sprintf(bufp, "\n");
	      if(rval > 0)
		bufp += rval;
	    }
	}
      else
	{
	  rval = sprintf(bufp,
			 "  MPD %2d :                                INIT ERRORS\n", id);
	  if(rval > 0)
	    bufp += rval;
	}
    }
  rval = sprintf(bufp, "\n");
  if(rval > 0)
    bufp += rval;

  printf("%s",apvbuffer);

  mpdGStatus(0);

  mpdHisto(outfile, h_gain);

  vtpUnlock();

  if(apvbuffer)
    free(apvbuffer);

  if(errorbuffer)
    free(errorbuffer);

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

/*
  Local Variables:
  compile-command: "make -k -B vtpMpdHistoMode"
  End:
 */
