#ifndef __VTP_MPDRO_H
#define __VTP_MPDRO_H
/*----------------------------------------------------------------------------*
 *  Copyright (c) 2021        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     vtp-mpdro : header of VTP library for MPD readout
 *
 *----------------------------------------------------------------------------*/

/* MPD prototypes */
#ifdef MPD_MON_NOT_SUPPORTED
int  vtpMpdMonEnable(int fiber);
int  vtpMpdMonDump(int fiber);
#endif /* MPD_MON_NOT_SUPPORTED */

int  vtpMpdSetAvg(int fiber, int apv, int min, int max);
int  vtpMpdSetApvOffset(int fiber, int apv, int strip, int offset);
int  vtpMpdSetApvThreshold(int fiber, int apv, int strip, int threshold);
int  vtpMpdFiberReset();
int  vtpMpdFiberLinkReset(unsigned int mpdmask);

int  vtpMpdEbSetFlags(int build_all_samples, int build_debug_headers,
		      int enable_cm, int noprocessing_prescale,
		      int allow_peak_any_time, int min_avg_samples);
int  vtpMpdEbGetFlags(int *build_all_samples, int *build_debug_headers,
		      int *enable_cm, int *noprocessing_prescale,
		      int *allow_peak_any_time, int *min_avg_samples);

int  vtpMpdEnable(unsigned int mpdmask);
int  vtpMpdDisable(unsigned int mpdmask);

int  vtpMpdReadRegs(int impd);
unsigned int vtpMpdReadReg(int impd, unsigned int reg);
int  vtpMpdWriteReg(int impd, unsigned int reg, unsigned int value);
int  vtpMpdGetSoftErrorCount(int fiber);
int  vtpMpdPrintStatus(uint32_t pmask, int upOnly);
unsigned int vtpMpdGetChanUpMask();
int  vtpGetMpdMaxRxLen(int impd);

#ifdef MPD_EB_NOT_SUPPORTED
int  vtpGetEB_wordCount(int impd);
int  vtpGetEbStatus(unsigned int *blockcnt, unsigned int *wordcnt, unsigned int *eventcnt);
int  vtpPrintEbStatus(int id);
#endif /* MPD_EB_NOT_SUPPORTED */

int  vtpSetCommonModeFilename(char *filename);
int  vtpGetCommonModeFilename(char *filename);
int  vtpSetPedestalFilename(char *filename);
int  vtpGetPedestalFilename(char *filename);
#endif /* __VTP_MPDRO_H */
