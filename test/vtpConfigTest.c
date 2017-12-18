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
  if(argc != 2)
  {
    printf("Error: must specify VTP config file to run.\n");
    exit(-1);
  }
  
  vtpInitGlobals();
  vtpConfig(argv[1]);
  
  exit(0);
}

