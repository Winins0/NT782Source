/*****************************************************************************
*                                                                            *
*  ZECKDAT.H                                                                    *
*                                                                            *
*  Copyright (C) Microsoft Corporation 1990.                                 *
*  All Rights reserved.                                                      *
*                                                                            *
******************************************************************************
*                                                                            *
*  Module Intent                                                             *
*                                                                            *
*   Internal data decls for Zeck compression routines for bitmaps & topic
*   2K blocks.
*                                                                            *
******************************************************************************
*                                                                            *
*  Current Owner:  Tomsn                                                     *
*                                                                            *
*****************************************************************************/

/* ----- THIS STUFF USED INTERNALLY BY THE COMPRESSION ROUTINES ----- */

/* this bitfields structure encodes an offset and a length of the */
/* repeated pattern.  Since we deal with huge buffers, we must access */
/* this as two bytes to insert it into the buffer (structures cannot */
/*  cross huge segment boundaries). */
typedef union bytesoverlay {
#ifdef MAC
    struct bitfieldsstruct {
      UINT  cbPatternLen:4;
      UINT  uiBackwardsOffset:12;
    } bf;
#else
    struct bitfieldsstruct {
      UINT  uiBackwardsOffset:12;
      UINT  cbPatternLen:4;
    } bf;
#endif
    BYTE bytes[2];
} ZECKPACKBLOCK, FAR *QZECKPACKBLOCK;

/* The length encoding is offset by a min value since you would never */
/* encode a pattern of 1 or 2 bytes: */
#define MIN_PATTERN_LEN   3
#define MAX_PATTERN_LEN   18

#define PATTERNLEN_FROM_ENCODE( len ) ((len)+MIN_PATTERN_LEN)
#define ENCODE_FROM_PATTERNLEN( len ) ((len)-MIN_PATTERN_LEN)

/* do similar offset for backwards offset: */
#define BACKWARDS_FROM_ENCODE( offset ) ((offset)+1)
#define ENCODE_FROM_BACKWARDS( offset ) ((offset)-1)
