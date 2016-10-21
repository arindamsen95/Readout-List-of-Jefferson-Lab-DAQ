/*
 * File:
 *    vtpLibTest.c
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

  if(vtpCheckAddresses() == ERROR)
    exit(-1);
  
  if(vtpOpen() != OK)
    goto CLOSE;
  
 CLOSE:
  vtpClose();

  exit(0);
}

