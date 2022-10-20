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
#include "vtpConfig.h"

extern int nfadc;

int
main(int argc, char *argv[])
{
  char rol_usrConfig[256] = "none";

  if(argc==2)
    strncpy(rol_usrConfig, argv[1], 256);

  vtpInitGlobals();
  vtpConfig("");
  if(strncasecmp(rol_usrConfig,"none",4))
    vtpConfig(rol_usrConfig);

  exit(0);
}
