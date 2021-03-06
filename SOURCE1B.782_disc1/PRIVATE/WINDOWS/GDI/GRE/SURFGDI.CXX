/******************************Module*Header*******************************\
* Module Name: surfgdi.cxx
*
* This file contains the bitmap creation functions
*
* Created: 14-Jun-1991 17:05:47
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"
#ifndef PRECOMPILED_GRE

#include "engine.hxx"
#include "sem.hxx"
#include "ldevobj.hxx"
#include "pdevobj.hxx"
#include "surfobj.hxx"
#include "xlateobj.hxx"
#include "dcobj.hxx"
extern "C" {
#include "server.h"
};

#endif

BOOL bDoGetSetBitmapBits(SURFOBJ *, SURFOBJ *, BOOL);
extern ULONG gaulConvert[7];


/******************************Public*Routine******************************\
* HBITMAP GreCreateBitmap
*
* API Entry point to create a bitmap.
*
* Returns: Handle to bitmap for success
*
* History:
*  Wed 23-Jan-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

HBITMAP APIENTRY GreCreateBitmap(
 int cx,
 int cy,
 UINT cPlanes,
 UINT cBits,
 LPBYTE pvBits)
{
    STACKPROBE;

// Validate the width and height

    if ((cx <= 0) || (cy <= 0))
    {
	WARNING("GreCreateBitmap failed cx or cy <= 0");
	SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
	return((HBITMAP) 0);
    }

// Try to guess what format the user wanted.  We will always guarentee
// enough space in the bitmap for the info.  Note that if either cPlanes
// or cBits is zero, a monochrome bitmap will be created.

    ULONG   iFormat = cPlanes * cBits;

// This if-else loop can be simplified and made smaller but until we get
// this right let's leave it like this so it's easy to change.

    DEVBITMAPINFO dbmi;
    dbmi.cxBitmap   = cx;
    dbmi.cyBitmap   = cy;
    dbmi.hpal       = (HPALETTE) 0;
    dbmi.fl         = BMF_TOPDOWN;

    if (iFormat <= 1)
    {
    // A monochrome bitmap has a fixed palette.  The 0 (black) is always
    // mapped to foreground, the 1 is always mapped to background (white).

        iFormat = BMF_1BPP;
        dbmi.hpal = hpalMono;
    }
    else if (iFormat <= 4)
    {
        iFormat = BMF_4BPP;
    }
    else if (iFormat <= 8)
    {
        iFormat = BMF_8BPP;
    }
    else if (iFormat <= 16)
    {
        iFormat = BMF_16BPP;
    }
    else if (iFormat <= 24)
    {
        iFormat = BMF_24BPP;
    }
    else if (iFormat <= 32)
    {
        iFormat = BMF_32BPP;
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HBITMAP) 0);
    }

    dbmi.iFormat    = iFormat;

    DIBMEMOBJ dimo(&dbmi, NULL);

    if (!dimo.bValid())
    {
        WARNING("Failed surface memory alloc in GreCreateBitmap\n");
        return((HBITMAP) 0);
    }

    if (pvBits != (LPBYTE) NULL)
    {
    // Initialize the bitmap.

        ULONG   cColors;
        cColors = 0;
        GreSetBitmapBits((HBITMAP)dimo.hsurf(), ((((cx * cPlanes * cBits) + 15) >> 4) << 1) * cy,
                         (PBYTE) pvBits, (LONG *) &cColors);
    }

    dimo.vKeepIt();
    bSetBitmapOwner((HBITMAP) dimo.hsurf(), OBJECTOWNER_CURRENT);
    return((HBITMAP) dimo.hsurf());
}

/******************************Public*Routine******************************\
* HBITMAP GreCreateCompatibleBitmap(hdc,cx,cy)
*
* GDI Interface routine to create a bitmap.
*
* History:
*  Mon 01-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Add IC support.
*
*  Fri 25-Jan-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

HBITMAP APIENTRY GreCreateCompatibleBitmap(HDC hdc, int cx, int cy)
{
    STACKPROBE;

// Validate the width and height

    if ((cx <= 0) || (cy <= 0))
    {
        WARNING("GreCreateCompatibleBitmap failed - invalid param - cx or cy was <= 0\n");
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HBITMAP) 0);
    }

// Without an hdc we create a monochrome bitmap.

    if (hdc == (HDC) NULL)
        return(GreCreateBitmap(cx, cy, 1, 1, (LPBYTE) NULL));

// Ok lock down the hdc and the surface.

    DCOBJ dco(hdc);

    if (!dco.bValid())
    {
        WARNING("CreateCompatibleBitmap failed - invalid hdc\n");
        return((HBITMAP) 0);
    }

// NOTE:  Even though we touch the SURFOBJ, we actually aren't going to read
// or write it.  We just need some of its information.

    PDEVOBJ po(dco.hdev());
    ASSERTGDI(po.bValid(), "CreateCompatibleBitmap po.bValid");
    ESURFOBJ *pso = dco.psoEff();

// Fill in the desired bitmap properties

    DEVBITMAPINFO   dbmi;
    dbmi.cxBitmap   = cx;
    dbmi.cyBitmap   = cy;
    dbmi.hpal       = (HPALETTE) 0;
    dbmi.fl         = BMF_TOPDOWN;

    if (dco.dctp() == DCTYPE_MEMORY)
    {
        dbmi.iFormat    = pso->iFormat();

        if (pso->ppal() != NULL)
            dbmi.hpal = pso->ppal()->hGet();
    }
    else
    {
        dbmi.iFormat = po.iDitherFormat();

        if (!po.bIsPalManaged())
            dbmi.hpal = po.ppalSurf()->hGet();
    }

// See if the device driver will manage the bitmap.

    if (pso->pldevOwner())
    {
        XLDEVOBJ lo(pso->pldevOwner());
        ASSERTGDI(lo.bValid(), "ERROR: invalid ldev in CreateCompatibleBM\n");

        if (PFNVALID(lo,CreateDeviceBitmap))
        {
        // Ok the device exports the entry point.  Let's call him up.

            HSURF   hsurf;

            SIZEL sizlTemp;
            sizlTemp.cx = cx;
            sizlTemp.cy = cy;

            ASSERTGDI(po.bValid(), "ERROR pdev bad");
            ASSERTGDI(po.hsemDisplay() != (HSEM) 0, "ERROR hsem bad");

            //
            // We synchronize access to the drivers.
            //

            SEMOBJ sem(po.hsemDisplay());

            hsurf = (*PFNDRV(lo,CreateDeviceBitmap))
                    (dco.dhpdev(), sizlTemp, dbmi.iFormat);

            if (hsurf)
            {
                ESURFOBJ so(hsurf);
                ASSERTGDI(so.bValid(), "ERROR device gave back invalid handle");

                if (dbmi.hpal != (HPALETTE) 0)
                {
                    EPALOBJ palSurf(dbmi.hpal);
                    ASSERTGDI(palSurf.bValid(), "ERROR invalid palette CreateCompatibleBitmap");

                // Set palette into surface.

                    so.ppal(palSurf.ppalGet());

                // Reference count it by making sure it is not unlocked.

                    palSurf.ppalSet((PPALETTE) NULL);  // It won't be unlocked
                }

                bSetBitmapOwner((HBITMAP) hsurf, OBJECTOWNER_CURRENT);

                //
                // Zero memory by sending a bitblt command to device
                //

                RECTL rclDst;

                rclDst.left   = 0;
                rclDst.top    = 0;
                rclDst.right  = cx;
                rclDst.bottom = cy;


                (*(so.pfnBitBlt()))((SURFOBJ *)&so,
                                (SURFOBJ *)NULL,
                                (SURFOBJ *)NULL,
                                (CLIPOBJ *)NULL,
                                NULL,
                                &rclDst,
                                (POINTL *)NULL,
                                (POINTL *)NULL,
                                (EBRUSHOBJ *)NULL,
                                (POINTL *)NULL,
                                0x0);

                return((HBITMAP) hsurf);
            }
        }
    }

    DIBMEMOBJ dimo(&dbmi, (PVOID) NULL);

    if (!dimo.bValid())
    {
        return((HBITMAP) 0);
    }

// Mark the bitmap a keeper.

    dimo.vKeepIt();
    dimo.hdev(dco.hdev());
    bSetBitmapOwner((HBITMAP) dimo.hsurf(), OBJECTOWNER_CURRENT);
    return((HBITMAP) dimo.hsurf());
}

/******************************Public*Routine******************************\
* GreSetBitmapBits
*
* Does the work for SetBitmapBits.
*
* History:
*  19-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

LONG GreSetBitmapBits(HBITMAP hbm, ULONG cjTotal, PBYTE pjBuffer, PLONG pOffset)
{
    if (cjTotal == 0)
        return(0);

    LONG lReturn = 0;
    BMOBJ bmo(hbm);
    SURFOBJ *pso;
    DIBMEMOBJ dimo;

    pso = bmo.pso();

    if (bmo.bValid())
    {
        LONG lInitOffset = *pOffset;
        SURFOBJ soTemp;
        ERECTL erclDst;
        POINTL ptlSrc;

        soTemp.dhsurf        = 0;
        soTemp.hsurf         = 0;
        soTemp.dhpdev        = bmo.dhpdev();
        soTemp.hdev          = bmo.hdev();
        soTemp.sizlBitmap.cx = bmo.sizl().cx;
        soTemp.sizlBitmap.cy = bmo.sizl().cy;
        soTemp.cjBits        = cjTotal;
        soTemp.pvBits        = pjBuffer;
        soTemp.pvScan0       = 0;
        soTemp.lDelta        = lInitOffset;
        soTemp.iUniq         = 0;
        soTemp.iType         = (USHORT) STYPE_BITMAP;
        soTemp.fjBitmap      = 0;

        ptlSrc.x = 0;
        ptlSrc.y = 0;
        erclDst.left = 0;
        erclDst.top  = 0;
        erclDst.right  = bmo.sizl().cx;
        erclDst.bottom = bmo.sizl().cy;

        HSEM hsemDisplay = (HSEM) 0;

        if (bmo.flags() & HOOK_SYNCHRONIZEACCESS)
        {
            PDEVOBJ po(bmo.hdev());
            ASSERTGDI(po.bValid(), "PDEV invalid");
            ASSERTGDI(po.hsemDisplay() != (HSEM) 0, "ERROR incorrect");
            hsemDisplay = po.hsemDisplay();
            VACQUIRESEM(hsemDisplay);
        }

        if (bmo.iType() == STYPE_DEVBITMAP)
        {
            DEVBITMAPINFO   dbmi;

            dbmi.iFormat    = bmo.iFormat();
            dbmi.cxBitmap   = bmo.sizl().cx;
            dbmi.cyBitmap   = bmo.sizl().cy;
            dbmi.hpal       = 0;
            dbmi.fl         = 0;

            //
            // Create a DIB (dimo) with the same height,width,
            // and BPP as the DEVBITMAP passed in
            //

            if (!dimo.bCreateDIB(&dbmi,NULL))
            {
                WARNING("GreSetBitmapBits failed bCreateDIB\n");
                lInitOffset = -1;  // This is to make us fail the call below.
            }

            pso = dimo.pso();

            //
            // We don't need to copy if the offset is 0 because either
            // it's a 1 shot set that inits the whole thing, or else
            // it's the first of a batch.  In either case anything
            // that's there before doesn't need to be saved because
            // it will all be over-written when we are done.
            //

            if (lInitOffset != 0)
            {
                #if DBG
                BOOL bCopyRet =
                #endif

                EngCopyBits
                (
                    pso,
                    bmo.pso(),
                    NULL,
                    NULL,
                    &erclDst,
                    &ptlSrc
                );

                ASSERTGDI(bCopyRet, "ERROR how can this fail ?");
            }
        }

        //
        // Check for invalid initial offset.
        //

        if (lInitOffset >= 0)
        {
            XLDEVOBJ lo(bmo.pldevOwner());

            //
            // Inc the target surface uniqueness
            //

            INC_SURF_UNIQ(&bmo);

            //
            // Copy the buffer to the DIB
            //

            #if DBG
            BOOL bReturn =
            #endif

            bDoGetSetBitmapBits(pso,&soTemp,FALSE);

            ASSERTGDI(bReturn, "GreSetBitmapBits failed bDoGetSetBitmapBits\n");

            lReturn = soTemp.cjBits;
            *pOffset = lInitOffset + lReturn;

            if (bmo.iType() == STYPE_DEVBITMAP)
            {
                //
                // Have the driver copy the temp DIB to the DEVBITMAP
                //

                if (!((*PFNGET(lo,CopyBits,bmo.flags()))
                    (
                        bmo.pso(),
                        pso,
                        NULL,
                        NULL,
                        &erclDst,
                        &ptlSrc
                    )))
                {
                    WARNING("GreSetBitmapBits failed copying temp DIB to DFB\n");
                    lReturn = 0;
                }
            }
        }

        if (hsemDisplay)
        {
            VRELEASESEM(hsemDisplay);
        }
    }

    return(lReturn);
}

/******************************Public*Routine******************************\
* GreGetBitmapBits
*
* Does the work for GetBitmapBits
*
* Returns: Number of bytes copied, or if pjBuffer is NULL the total size of
*          the bitmap.
*
* History:
*  Mon 01-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Return bitmap scanlines in BMF_TOPDOWN organization.
*
*  22-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

LONG GreGetBitmapBits(HBITMAP hbm, ULONG cjTotal, PBYTE pjBuffer, PLONG pOffset)
{
    LONG lReturn = 0;
    BMOBJ bmo(hbm);
    SURFOBJ *pso;
    XLDEVOBJ lo(bmo.pldevOwner());
    DIBMEMOBJ dimo;

    pso = bmo.pso();

    if (bmo.bValid())
    {
        lReturn = ((((gaulConvert[bmo.iFormat()] * bmo.sizl().cx) + 15) >> 4) << 1) * bmo.sizl().cy;

        if (pjBuffer != (PBYTE) NULL)
        {
            SURFOBJ soTemp;
            ERECTL erclDst;
            POINTL ptlSrc;

            soTemp.dhsurf        = 0;
            soTemp.hsurf         = 0;
            soTemp.dhpdev        = bmo.dhpdev();
            soTemp.hdev          = bmo.hdev();
            soTemp.sizlBitmap.cx = bmo.sizl().cx;
            soTemp.sizlBitmap.cy = bmo.sizl().cy;
            soTemp.cjBits        = 0;
            soTemp.pvBits        = 0;
            soTemp.pvScan0       = 0;
            soTemp.lDelta        = 0;
            soTemp.iUniq         = 0;
            soTemp.iType         = (USHORT) STYPE_BITMAP;
            soTemp.fjBitmap      = 0;

            ptlSrc.x = 0;
            ptlSrc.y = 0;
            erclDst.left = 0;
            erclDst.top  = 0;
            erclDst.right  = bmo.sizl().cx;
            erclDst.bottom = bmo.sizl().cy;

            HSEM hsemDisplay = (HSEM) 0;

            if (bmo.flags() & HOOK_SYNCHRONIZEACCESS)
            {
                PDEVOBJ po(bmo.hdev());
                ASSERTGDI(po.bValid(), "PDEV invalid");
                ASSERTGDI(po.hsemDisplay() != (HSEM) 0, "ERROR incorrect");
                hsemDisplay = po.hsemDisplay();
                VACQUIRESEM(hsemDisplay);
            }

            if (bmo.iType() == STYPE_DEVBITMAP)
            {
                //
                // Create a DIB (dimo) with the same height,width,
                // and BPP as the DEVBITMAP passed in
                //

                DEVBITMAPINFO   dbmi;

                dbmi.iFormat    = bmo.iFormat();
                dbmi.cxBitmap   = bmo.sizl().cx;
                dbmi.cyBitmap   = bmo.sizl().cy;
                dbmi.hpal     = 0;
                dbmi.fl       = 0;

                if (!dimo.bCreateDIB(&dbmi,NULL))
                {
                    WARNING("GreGetBitmapBits failed bCreateDIB\n");
                    lReturn = 0;
                }
                else
                {
                    pso = dimo.pso();

                    #if DBG
                    BOOL bCopyRet =
                    #endif

                    EngCopyBits
                    (
                        pso,
                        bmo.pso(),
                        NULL,
                        NULL,
                        &erclDst,
                        &ptlSrc
                    );

                    ASSERTGDI(bCopyRet, "ERROR how can this fail ?");
                }
            }

            if (lReturn)
            {
            // We know how big the buffer needs to be.  Set up the
            // soTemp so the driver knows how much to fill in.

                ULONG cjMaxLength = lReturn;
                LONG lInitOffset = *pOffset;

            // Check for invalid initial offset.

                if ((lInitOffset >= 0) && ((ULONG)lInitOffset < cjMaxLength))
                {
                // Make cjTotal valid range.

                    if ((lInitOffset + cjTotal) > cjMaxLength)
                    {
                        cjTotal = cjMaxLength - lInitOffset;
                    }

                    if (cjTotal > 0)
                    {
                    // Fill in our return values, we know them already.

                        soTemp.cjBits = cjTotal;
                        soTemp.lDelta = lInitOffset;
                        soTemp.pvBits = pjBuffer;

                        #if DBG
                        BOOL bReturn =
                        #endif

                        bDoGetSetBitmapBits(&soTemp,pso,TRUE);

                        ASSERTGDI(bReturn, "GreGetBitmapBits failed bDoGetSetBitmapBits\n");

                        lReturn = soTemp.cjBits;
                        *pOffset = lInitOffset + lReturn;
                    }
                    else
                    {
                        WARNING("GreGetBitmapBits failed cjTotal 0\n");
                        lReturn = 0;
                    }
                }
                else
                {
                    WARNING("GreGetBitmapBits failed lInitOffset invalid\n");
                    lReturn = 0;
                }
            }

            if (hsemDisplay)
            {
                VRELEASESEM(hsemDisplay);
            }
        }
    }
    else
    {
        WARNING("GreGetBitmapBits failed - invalid bitmap handle\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
    }

    return(lReturn);
}

/******************************Public*Routine******************************\
* bDoGetSetBitmapBits
*
* Does the get or set of bitmap bits for EngCopyBits.
*
* History:
*  16-Mar-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bDoGetSetBitmapBits(SURFOBJ *psoDst, SURFOBJ *psoSrc, BOOL bGetBits)
{
    ASSERTGDI(psoDst->iType == STYPE_BITMAP, "ERROR no DIB dst");
    ASSERTGDI(psoSrc->iType == STYPE_BITMAP, "ERROR no DIB src");

    //
    // Synchronize with the device driver before touching the device surface.
    //

    if (bGetBits)
    {
    // Doing a GetBitmapBits.

        if (((ESURFOBJ *) psoSrc)->flags() & HOOK_SYNCHRONIZE)
        {
            PDEVOBJ po(psoSrc->hdev);
            (po.pfnSync())(psoSrc->dhpdev,NULL);
        }

        if (psoDst->pvBits == NULL)
        {
            psoDst->cjBits = ((((gaulConvert[psoSrc->iBitmapFormat] * psoSrc->sizlBitmap.cx) + 15) >> 4) << 1) * psoSrc->sizlBitmap.cy;
        }
        else
        {
        // Initialize temporaries.

            PBYTE pjBuffer = (PBYTE) psoDst->pvBits;
            PBYTE pjBitmap = (PBYTE) psoSrc->pvScan0;
            LONG lDeltaBitmap = psoSrc->lDelta;
            ULONG cjScanBitmap = ((ESURFOBJ *) psoSrc)->cjScan();

            ASSERTGDI(pjBuffer != NULL, "ERROR pjBuffer is NULL");
            ASSERTGDI(pjBitmap != NULL, "ERROR pjBitmap is NULL");

        // Get the WORD aligned width of the input scanlines.

            ULONG cjScanInput = ((((gaulConvert[psoSrc->iBitmapFormat] * psoSrc->sizlBitmap.cx) + 15) >> 4) << 1);
            ULONG cjMaxLength = cjScanInput * psoSrc->sizlBitmap.cy;
            LONG lInitOffset = psoDst->lDelta;
            ULONG cjTotal = psoDst->cjBits;

        // Check for invalid initial offset.

            if ((lInitOffset < 0) || ((ULONG)lInitOffset >= cjMaxLength))
            {
                psoDst->cjBits = 0;
                return(FALSE);
            }

        // Make cjTotal valid range.

            if (lInitOffset + cjTotal > cjMaxLength)
            {
                cjTotal = cjMaxLength - lInitOffset;
            }

        // Fill in our return values.

            psoDst->cjBits = cjTotal;

        // Move pointer to current scanline in bitmap.

            pjBitmap += ((lInitOffset / cjScanInput) * lDeltaBitmap);

            ULONG ulTemp,ulCopy;

        // Move partial scan if necesary.

            ulTemp = (lInitOffset % cjScanInput);

            if (ulTemp)
            {
                ulCopy = MIN((cjScanInput - ulTemp), cjTotal);

                RtlCopyMemory((PVOID) pjBuffer, (PVOID) (pjBitmap + ulTemp), (unsigned int) ulCopy);

                pjBuffer += ulCopy;
                pjBitmap += lDeltaBitmap;
                cjTotal  -= ulCopy;
            }

        // Move as many scans that fit.

            ulTemp = cjTotal / cjScanInput;
            cjTotal -= (ulTemp * cjScanInput);

            while (ulTemp--)
            {
                RtlCopyMemory((PVOID) pjBuffer, (PVOID) pjBitmap, (unsigned int) cjScanInput);

                pjBuffer += cjScanInput;
                pjBitmap += lDeltaBitmap;
            }

        // Move as much of partial scan as possible.

            if (cjTotal)
            {
                RtlCopyMemory((PVOID) pjBuffer, (PVOID) pjBitmap, (unsigned int) cjTotal);
            }
        }
    }
    else
    {
    // Doing a SetBitmapBits call.

        if (((ESURFOBJ *) psoDst)->flags() & HOOK_SYNCHRONIZE)
        {
            PDEVOBJ po(psoDst->hdev);
            (po.pfnSync())(psoDst->dhpdev,NULL);
        }

    // Initialize temporaries.

        PBYTE pjBuffer = (PBYTE) psoSrc->pvBits;
        PBYTE pjBitmap = (PBYTE) psoDst->pvScan0;
        LONG lDeltaBitmap = psoDst->lDelta;
        ULONG cjScanBitmap = ((ESURFOBJ *) psoDst)->cjScan();

    // Get the WORD aligned width of the input scanlines.

        ULONG cjScanInput = ((((gaulConvert[psoDst->iBitmapFormat] * psoDst->sizlBitmap.cx) + 15) >> 4) << 1);
        ULONG cjMaxLength = cjScanInput * psoDst->sizlBitmap.cy;
        LONG lInitOffset = psoSrc->lDelta;
        ULONG cjTotal = psoSrc->cjBits;

    // Check for invalid initial offset.

        if ((lInitOffset < 0) || ((ULONG)lInitOffset >= cjMaxLength))
        {
            psoSrc->cjBits = 0;
            return(TRUE);
        }

    // Make cjTotal valid range.

        if (lInitOffset + cjTotal > cjMaxLength)
        {
            cjTotal = cjMaxLength - lInitOffset;
        }

    // Fill in our return values, we know them already.

        psoSrc->cjBits = cjTotal;

    // Move pointer to current scanline in bitmap.

        pjBitmap += ((lInitOffset / cjScanInput) * lDeltaBitmap);

        ULONG ulTemp,ulCopy;

    // Move partial scan if necesary.

        ulTemp = (lInitOffset % cjScanInput);

        if (ulTemp)
        {
            ulCopy = MIN((cjScanInput - ulTemp), cjTotal);

            RtlCopyMemory((PVOID) (pjBitmap + ulTemp), (PVOID) pjBuffer, (unsigned int) ulCopy);

            pjBuffer += ulCopy;
            pjBitmap += lDeltaBitmap;
            cjTotal  -= ulCopy;
        }

    // Move as many scans that fit.

        ulTemp = cjTotal / cjScanInput;
        cjTotal -= (ulTemp * cjScanInput);

        while (ulTemp--)
        {
            RtlCopyMemory((PVOID) pjBitmap, (PVOID) pjBuffer, (unsigned int) cjScanInput);

            pjBuffer += cjScanInput;
            pjBitmap += lDeltaBitmap;
        }

    // Move as much of partial scan as possible.

        if (cjTotal)
        {
            RtlCopyMemory((PVOID) pjBitmap, (PVOID) pjBuffer, (unsigned int) cjTotal);
        }
    }

    return(TRUE);
}
