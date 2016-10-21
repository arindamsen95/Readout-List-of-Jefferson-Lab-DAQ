/*
 * File:
 *    vtpFirmwareLoad.c
 *
 * Description:
 *    JLab VTP V7 Firmware Load
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vtpLib.h"

char *progName;

int  vtpV7Load(char *filename);
void Usage();

int 
main(int argc, char *argv[])
{
  
  int stat=0;
  char *bin_filename;
  char inputchar[16];

  printf("\nJLAB VTP firmware load\n");
  printf("----------------------------\n");
  
  progName = argv[0];
  
  if(argc<2)
    {
      printf(" ERROR: Must specify one argument\n");
      Usage();
      exit(-1);
    }
  else
    {
      bin_filename = argv[1];
    }

  /* Initialize library */
  stat = vtpOpen();
  if(stat < 0)
    {
      printf(" Unable to initialize VTP library.\n");
      goto CLOSE;
    }

  /* Get firmware version? */
  printf(" Will load firmware for VTP - V7 FPGA ");

  printf(" with file: \n   %s",bin_filename);

 REPEAT2:
  printf(" Press y and <ENTER> to continue... n or q and <ENTER> to quit without firmware load\n");

  scanf("%s",(char *)inputchar);

  if((strcmp(inputchar,"q")==0) || (strcmp(inputchar,"Q")==0) ||
     (strcmp(inputchar,"n")==0) || (strcmp(inputchar,"N")==0) )
    {
      printf(" Exiting without firmware load\n");
      goto CLOSE;
    }
  else if((strcmp(inputchar,"y")==0) || (strcmp(inputchar,"Y")==0))
    {}
  else
    goto REPEAT2;

  if(vtpV7Load(bin_filename) != OK)
    {
      printf(" ERROR: Firmware load failed!\n");
    }
  
 CLOSE:
  vtpClose();
    
  exit(0);
}

int
vtpV7Load(char *filename)
{
  vtpV7CtrlInit();
  
  vtpV7SetReset(1);
  vtpV7SetResetSoft(1);
  
  if(vtpV7CfgStart() != OK)
    return ERROR;
  if(vtpV7CfgLoad(filename) != OK)
    return ERROR;
  if(vtpV7CfgEnd() != OK)
    return ERROR;
  
  vtpV7SetReset(0);
  vtpV7SetResetSoft(0);
  
  return OK;
}


void
Usage()
{
  printf("\n");
  printf("%s <firmware .bin file>\n",progName);
  printf("\n");

}
