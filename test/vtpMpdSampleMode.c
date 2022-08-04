
/*
 * File:
 *    vtpMpdSampleMode.c
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

char cfgFilename[255];
char progName[255];

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 250
#endif
int getShortHostname(char *shortHostname);

void mpdSampleTest(char *outfile, int32_t clp_clock0, int32_t clp_clock1, int32_t clp_clockd);

int sig_ctrl = 0; // 0 = force quit, 1 = soft quit ... when CTRL-C is pressed
int verbose_level = 0; // 0 = quit, 1 = some output, 2 = verbose output
void sig_handler(int signo);

void
usage()
{
  int clp_clock0 = 0;
  int clp_clock1 = 50;
  int clp_clockd = 2;

  printf("SYNTAX: %s config_file out_data_file par0 par1 par2\n", progName);
  printf("         SAMPLE check\n");
  printf("               par0 = min_clock_phase  (%d)\n",clp_clock0);
  printf("               par1 = max_clock_phase  (%d)\n", clp_clock1);
  printf("               par2 = clock_phase_step (%d)\n", clp_clockd);
}


int
main(int argc, char *argv[])
{
  int stat;
  char outfile[255];
  int useConfigFile = 0;
  int clp_clock0 = 0;
  int clp_clock1 = 50;
  int clp_clockd = 2;

  if (argc != 6)
    {
      usage();
      exit(-1);
    }
  else
    {
      strncpy(cfgFilename, argv[1], 255); /* config_file */
      strncpy(outfile, argv[2], 255);  /* out_data_file */
      clp_clock0 = atoi(argv[3]);  /* par0 = min_clock_phase */
      clp_clock1 = atoi(argv[4]);  /* par1 = max_clock_phase */
      clp_clockd = atoi(argv[5]);  /* par2 = clock__phase_step */
    }

  signal(SIGINT, sig_handler);
  signal(SIGTSTP, sig_handler);

  char rol_usrConfig[250];
  char shortHostname[HOST_NAME_MAX];

  stat = getShortHostname(shortHostname);
  sprintf(rol_usrConfig, "/home/sbs-onl/vtp/cfg/%s.config",shortHostname);

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

  /* just make a call to vtp_mpd_setup() */
  vtp_mpd_setup(cfgFilename);

  mpdSampleTest(outfile, clp_clock0, clp_clock1, clp_clockd);

  vtpUnlock();

  return 0;
}

void
mpdSampleTest(char *outfile, int32_t clp_clock0, int32_t clp_clock1, int32_t clp_clockd)
{
  /**********************************
   * SAMPLES TEST
   * sample APV output at 40 MHz
   * only for testing not for normal daq
   **********************************/
  int acq_mode = 0x2;
  FILE *fout;
  int k,h,i,j,kk, error_count;
  int scount, sfreq;
  int sch0, sch1;
  int rtout;
  int fnMPD = mpdGetNumberMPD();
  uint16_t mfull, mempty;
#define MAX_SDATA 1024
  uint32_t sdata[MAX_SDATA]; // samples data buffer

  if (acq_mode & 0x2) {

    fout = fopen(outfile,"w");
    if (fout == NULL) { fout = stdout; }

    fprintf(fout,"# SAMPLE MODE OUTPUT\n");
    fprintf(fout,"# CLOCK_RANGE: %d %d %d\n",clp_clock0, clp_clock1, clp_clockd);

    for (k=0;k<fnMPD;k++) { // only active mpd set
      i = mpdSlot(k);

      mpdSetAcqMode(i, "sample");
      mpdSetEventBuilding(i, 0);

      // load pedestal and thr default values
      mpdPEDTHR_Write(i);

      mpdSetApvEnableMask(i, 0x7fff);
      for (kk=clp_clock0;kk<clp_clock1;kk+=clp_clockd) { // loop on clock phases
	// set clock phase
	mpdDELAY25_Set(i, kk, kk);

	// enable acq
	mpdDAQ_Enable(i);

	// wait for FIFOs to get full
	mfull=0;
	mempty=1;
	printf("MPD / Clock : %d %d\n",i, kk);

	rtout = 2;
	do {
	  mpdFIFO_GetAllFlags(i,&mfull,&mempty);
	  printf("SAMPLE test wait fifo full: %x (%x) %x\n",
		 mfull,mpdGetApvEnableMask(i),mempty);
	  sleep(1);
	  rtout--;
	} while ((mfull!=mpdGetApvEnableMask(i)) && (rtout>0));

	// read data from FIFOs and estimate synch pulse period
	for (j=0;j<16;j++) { // 16 = number of ADC channel in one MPD
	  if (mpdGetApvEnableMask(i) & (1<<j)) { // apv is enabled
	    mpdFIFO_Samples(i,j, sdata, &scount, MAX_SDATA, &error_count);

	    printf("i: %d",i);
	    if (error_count != 0) {
	      printf("ERROR returned from FIFO_Samples %d\n",error_count);
	      continue;
	    }

	    fprintf(fout,"# MPD_ADC_COUNT_CLOCK_TOUT: %d %d %d %d %d\n",i, j, scount, kk, rtout);
	    printf("MPD/APV : %d / %d, peaks around: ",i,j);
	    sch0=-1;
	    sch1=-1;
	    sfreq=0;
	    for (h=0;h<scount;h++) { // detects synch peaks
	      //    printf("%04d ", sdata[h]); // output data
	      fprintf(fout,"%d ",sdata[h]);
	      if (sdata[h]>2500) { // threshold could be lower
	     	if (sch0<0) { sch0=h; sch1=sch0;} else {
		  if (h==(sch1+1)) { sch1=h; } else {
		    printf("%d-%d (v %d) ",sch0,sch1, sdata[h]);
		    sch0=h;
		    sch1=sch0;
		    sfreq++;
		  }
		}
	      }
	    }
	    fprintf(fout,"\n");
	    printf("\n Estimated synch period = %f (us) ,expected (20MHz:1.8, 40MHz:0.9)\n",((float) scount)/sfreq/40.0);
	  }
	}

      } // end loop on clock phases

    } // end loop on mpds

    if (fout != stdout)  fclose(fout);

  } // sample check mode
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
  compile-command: "make -k -B vtpMpdSampleMode"
  End:
 */
