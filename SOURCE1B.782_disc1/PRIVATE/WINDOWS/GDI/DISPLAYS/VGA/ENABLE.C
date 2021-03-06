/******************************Module*Header*******************************\
* Module Name: enable.c
*
* Functions to enable and disable the driver
*
* Copyright (c) 1992-1994 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

extern GDIINFO gaulCap;             // in gdiinfo.c
extern LOGPALETTE logPalVGA;        // in gdiinfo.c
extern DEVINFO devinfoVGA;          // in gdiinfo.c

extern LPBYTE pPtrSave;
extern LPBYTE pPtrWork;

BOOL SetUpBanking(PDEVSURF, PPDEV);
BOOL bInitPointer(PPDEV);

extern ajConvertBuffer[1];            // Arbitrary sized array!

static  DRVFN   gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV             },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV           },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV            },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface          },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface         },
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush           },
    {   INDEX_DrvCreateDeviceBitmap,    (PFN) DrvCreateDeviceBitmap     },
    {   INDEX_DrvDeleteDeviceBitmap,    (PFN) DrvDeleteDeviceBitmap     },
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt                 },
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut                },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape        },
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer            },
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath             },
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits               },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor            },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode             },
    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits         },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes               },
    {   INDEX_DrvFillPath,              (PFN) DrvFillPath               },
    {   INDEX_DrvPaint,                 (PFN) DrvPaint                  }
};

/******************************Public*Routine******************************\
* BOOL bEnableDriver(iEngineVersion, cb, pded)
*
* Enables the driver by filling the function table.  This call is made by
* the Engine to fill its driver function table in the LDEV (Logical DEVice).
* This call should only be made once per driver but we can handle being
* called multiple times.
*
\**************************************************************************/

BOOL DrvEnableDriver
(
    ULONG           iEngineVersion,
    ULONG           cb,
    PDRVENABLEDATA  pded
)
{
    DISPDBG((2, "VGA: enabling Driver\n"));

    cb /= sizeof(ULONG);

    switch(cb)
    {
    case 3:
        pded->pdrvfn = gadrvfn;
    case 2:
        pded->c = sizeof(gadrvfn) / sizeof(DRVFN);
    case 1:
        pded->iDriverVersion = DDI_DRIVER_VERSION;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vDisableDriver
*
* Unload the driver and any data in may have created.
*
\**************************************************************************/

VOID DrvDisableDriver()
{
    DISPDBG((2, "VGA: disabling Driver\n"));
}

/******************************Public*Routine******************************\
* bInitDefaultPatterns
*
* Creates the default monochrome patterns to be used on it's surface.
*
\**************************************************************************/

BOOL bInitDefaultPatterns(PPDEV ppdev)
{
    SIZEL           sizl;
    INT             iLoop;
    PULONG          pulInit;

    sizl.cx = 8;
    sizl.cy = 8;

    DISPDBG((2, "VGA: initializing defailt patterns\n"));

    for (iLoop = 0; iLoop < HS_DDI_MAX; iLoop++)
    {
        pulInit = (PULONG) (&gaajPat[iLoop][0]);
        ppdev->ahbmPat[iLoop] = EngCreateBitmap(sizl, sizl.cx / 8, BMF_1BPP,
                                                BMF_TOPDOWN, pulInit);

        if (ppdev->ahbmPat[iLoop] == (HBITMAP)0)
        {
        // Release any bitmaps we have created.

            while(iLoop--)
                EngDeleteSurface((HSURF) ppdev->ahbmPat[iLoop]);

            return(FALSE);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vKillPatterns(ppdev)
*
* Clear the standard patterns
*
\**************************************************************************/

VOID vKillPatterns(PPDEV ppdev)
{
    INT     ii;

    for (ii = 0; ii < HS_DDI_MAX; ii++)
        EngDeleteSurface((HSURF) ppdev->ahbmPat[ii]);
}

/******************************Public*Routine******************************\
* DHPDEV DrvEnablePDEV
*
* Enable the Physical DEVice
*
* Warnings:
*   The PDEV isn't complete until bCompletePDEV is called.
*
\**************************************************************************/

DHPDEV DrvEnablePDEV
(
    DEVMODEW *pdrivw,
    PWSTR     pwszLogAddress,
    ULONG     cPatterns,
    PHSURF    ahsurfPatterns,
    ULONG     cjGdiInfo,
    ULONG    *pdevcaps,
    ULONG     cb,
    PDEVINFO  pdevinfo,
    PWSTR     pwszDataFile,
    PWSTR     pwszDeviceName,
    HANDLE    hDriver          // Handle to base driver
)
{
    PPDEV   ppdev;
    SYSTEM_INFO SystemInfo;

    DISPDBG((2, "VGA: enabling PDEV\n"));

    //
    // Define flag to keep track of allocation
    //

    ppdev = (PPDEV) LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(PDEV));

    if (ppdev == NULL)
    {
        goto errorAllocPDEV;
    }

    //
    // Identifier, for debugging purposes
    //

    ppdev->ident = PDEV_IDENT;

    //
    // Determine whether this is a 386 or a better processor
    //

    GetSystemInfo(&SystemInfo);

    if (SystemInfo.dwProcessorType == PROCESSOR_INTEL_386)
    {
        ppdev->ulIs386 = 1;
    }
    else
    {
        ppdev->ulIs386 = 0;
    }

    //
    // Cache the device driver handle away for later use.
    //

    ppdev->hDriver = hDriver;

    //
    // Initialize the cursor stuff.  We can violate the atomic rule here
    // since nobody can talk to the driver yet.
    //

    ppdev->xyCursor.x = 320;            // Non-atomic
    ppdev->xyCursor.y = 240;            // Non-atomic

    ppdev->ptlExtent.x = 0;
    ppdev->ptlExtent.y = 0;
    ppdev->cExtent = 0;

    ppdev->flCursor = CURSOR_DOWN;

    //
    // Get the current screen mode information.  Set up device caps and devinfo.
    //

    if (!bInitPDEV(ppdev, pdrivw, &gaulCap, &devinfoVGA))
    {
        DISPDBG((1, "DISP VGA DrvEnablePDEV failed bInitPDEV\n"));
        goto errorbInitPDEV;
    }

    if (!bInitDefaultPatterns(ppdev))
    {
        //
        // Don't call vKillPatterns
        //

        goto errorbInitDefaultPatterns;
    }

    cPatterns=min(cPatterns, HS_DDI_MAX);

    memcpy((PVOID)ahsurfPatterns, ppdev->ahbmPat, cPatterns*sizeof(HBITMAP));

    cjGdiInfo=min(cjGdiInfo, sizeof(GDIINFO));

    memcpy(pdevcaps, &gaulCap, cjGdiInfo);

     // Now let's pass back the devinfo

    devinfoVGA.hpalDefault = EngCreatePalette(PAL_INDEXED, 16,
                                              (PULONG) (logPalVGA.palPalEntry),
                                              0, 0, 0);

    if (devinfoVGA.hpalDefault == (HPALETTE) 0)
    {
        goto errorEngCreatePalette;
    }

    *pdevinfo = devinfoVGA;

    // Try to preallocate a saved screen bits buffer. If we fail, set the flag
    // to indicate the buffer is in use, so that we'll never attempt to use it.
    // If we succeed, mark the buffer as free.

    if ((ppdev->pjPreallocSSBBuffer = (PUCHAR)
              LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, PREALLOC_SSB_SIZE))
              != NULL)
    {
        ppdev->flPreallocSSBBufferInUse = FALSE;
        ppdev->ulPreallocSSBSize = PREALLOC_SSB_SIZE;
    }
    else
    {
        ppdev->flPreallocSSBBufferInUse = TRUE;
    }

    // Fill in the DIB4->VGA conversion tables. Allow 256 extra bytes so that
    // we can always safely align the tables to a 256-byte boundary, for
    // look-up reasons. There are four tables, each 256 bytes long

    ppdev->pucDIB4ToVGAConvBuffer =
            (UCHAR *) LocalAlloc((LMEM_FIXED | LMEM_ZEROINIT),
            ((256*4+256)*sizeof(UCHAR)));

    if (ppdev->pucDIB4ToVGAConvBuffer == NULL)
    {
        goto errorAllocpucDIB4ToVGAConvBuffer;
    }

    // Round the table start up to the nearest 256 byte boundary, because the
    // tables must start on 256-byte boundaries for look-up reasons

    ppdev->pucDIB4ToVGAConvTables =
            (UCHAR *) ((ULONG) (ppdev->pucDIB4ToVGAConvBuffer + 0xFF) & ~0xFF);

    vSetDIB4ToVGATables(ppdev->pucDIB4ToVGAConvTables);

    return((DHPDEV) ppdev);

errorAllocpucDIB4ToVGAConvBuffer:

    //
    // Free the preallocated saved screen bits buffer, if there is one.
    //

    if (ppdev->pjPreallocSSBBuffer != NULL)
    {
        LocalFree(ppdev->pjPreallocSSBBuffer);
    }

    EngDeletePalette(devinfoVGA.hpalDefault);

errorEngCreatePalette:

    vKillPatterns(ppdev);

errorbInitDefaultPatterns:
errorbInitPDEV:

    LocalFree(ppdev);

errorAllocPDEV:

    return((DHPDEV) 0);

    UNREFERENCED_PARAMETER(cb);
    UNREFERENCED_PARAMETER(pwszLogAddress);
    UNREFERENCED_PARAMETER(pwszDataFile);
    UNREFERENCED_PARAMETER(pwszDeviceName);
}

/******************************Public*Routine******************************\
* BOOL bCompletePDEV(dhpdev, hpdev)
*
* Complete the initialization of the PDEV
*
\**************************************************************************/

VOID DrvCompletePDEV(
    DHPDEV dhpdev,
    HDEV  hdev)
{
    PPDEV   ppdev;

    ppdev = (PPDEV) dhpdev;

    ppdev->hdevEng = hdev;
}

/******************************Public*Routine******************************\
* VOID DrvDisablePDEV(dhpdev)
*
* Shutdown this physical device.
*
* Warnings:
*   If a surface is still active for this PDEV it will be freed.
*

\**************************************************************************/

VOID DrvDisablePDEV(DHPDEV dhpdev)
{
    PPDEV   ppdev;

    ppdev = (PPDEV) dhpdev;

    DISPDBG((2, "VGA:disabling PDEV\n"));

    EngDeletePalette(devinfoVGA.hpalDefault);

// Free the resources and bitmaps associated with this PDEV

    vKillPatterns(ppdev);

// Free the preallocated saved screen bits buffer, if there is one.

    if (ppdev->pjPreallocSSBBuffer != NULL)
    {
        LocalFree(ppdev->pjPreallocSSBBuffer);
    }

// Free the conversion table buffer

    if (ppdev->pucDIB4ToVGAConvBuffer != NULL)
    {
        LocalFree(ppdev->pucDIB4ToVGAConvBuffer);
    }

// Delete the PDEV

    LocalFree(dhpdev);

    DISPDBG((2, "VGA: disabled PDEV\n"));

}

/******************************Public*Routine******************************\
* HSURF DrvEnableSurface(dhpdev)
*
* Enable the surface for the device.  This will actually intialize the
* screen on the VGA.
*
* Warnings:
*   This routine should only be called ONCE per PDEV.
*
\**************************************************************************/

HSURF DrvEnableSurface(DHPDEV dhpdev)
{
    PPDEV    ppdev;
    PDEVSURF pdsurf;
    DHSURF   dhsurf;
    HSURF    hsurf;

    DISPDBG((2, "VGA:enabling Surface\n"));

    ppdev = (PPDEV) dhpdev;

    //
    // Initialize the VGA device into the selected mode which will also map
    // the video frame buffer
    //

    if (!bInitVGA(ppdev, TRUE))
    {
        goto error_done;
    }

    dhsurf = (DHSURF) LocalAlloc(LMEM_FIXED, (SIZE_T) sizeof(DEVSURF));

    if (dhsurf == (DHSURF) 0)
        goto error_done;

    pdsurf = (PDEVSURF) dhsurf;

    pdsurf->ident           = DEVSURF_IDENT;
    pdsurf->flSurf          = 0;
    pdsurf->iFormat         = BMF_PHYSDEVICE;
    pdsurf->jReserved1      = 0;
    pdsurf->jReserved2      = 0;
    pdsurf->ppdev           = ppdev;
    pdsurf->sizlSurf.cx     = ppdev->sizlSurf.cx;
    pdsurf->sizlSurf.cy     = ppdev->sizlSurf.cy;
    pdsurf->lNextPlane      = 0;
    pdsurf->pvScan0         = ppdev->pjScreen;
    pdsurf->pvBitmapStart   = ppdev->pjScreen;
    pdsurf->pvStart         = ppdev->pjScreen;
    pdsurf->pvConv          = &ajConvertBuffer[0];

    // Initialize pointer information.

    //
    // bInitPointer must be called before bInitSavedBits.
    //

    if (!bInitPointer(ppdev)) {
        DISPDBG((0, "VGA DrvEnablePDEV failed bInitPointer\n"));
        goto error_clean;
    }

    if (!SetUpBanking(pdsurf, ppdev)) {
        DISPDBG((0, "VGA DrvEnablePDEV failed SetUpBanking\n"));
        goto error_clean;
    }

    if ((hsurf = EngCreateDeviceSurface(dhsurf, ppdev->sizlSurf, BMF_4BPP)) ==
        (HSURF) 0)
    {
        DISPDBG((0, "VGA DrvEnablePDEV failed EngCreateDeviceSurface\n"));
        goto error_clean;
    }

    //
    // vInitSavedBits must be called after bInitPointer.
    //

    vInitSavedBits(ppdev);

    if (EngAssociateSurface(hsurf, ppdev->hdevEng,
                        HOOK_BITBLT | HOOK_TEXTOUT | HOOK_STROKEPATH |
                        HOOK_COPYBITS | HOOK_PAINT | HOOK_FILLPATH
                        ))
    {
        ppdev->hsurfEng = hsurf;
        ppdev->pdsurf = pdsurf;

        // Set up an empty saved screen block list
        pdsurf->ssbList = NULL;

        DISPDBG((2, "VGA: enabled surface\n"));

        return(hsurf);
    }

    DISPDBG((0, "VGA DrvEnablePDEV failed EngDeleteSurface\n"));
    EngDeleteSurface(hsurf);

error_clean:
    // We created the surface, so delete it
    LocalFree(dhsurf);

error_done:
    return((HSURF) 0);
}


/******************************Public*Routine******************************\
* DrvDisableSurface
*
* Free resources associated with this surface.
*
\**************************************************************************/

VOID DrvDisableSurface(DHPDEV dhpdev)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    PDEVSURF pdsurf = ppdev->pdsurf;
    PSAVED_SCREEN_BITS pSSB, pSSBNext;

    DISPDBG((2, "VGA:disabling surface\n"));

    // Free up banking-related stuff.
    LocalFree(pdsurf->pBankSelectInfo);

    if (pdsurf->pbiBankInfo != NULL) {
        LocalFree(pdsurf->pbiBankInfo);
    }

    if (pdsurf->pbiBankInfo2RW != NULL) {
        LocalFree(pdsurf->pbiBankInfo2RW);
    }

    if (pdsurf->pvBankBufferPlane0 != NULL) {
        LocalFree(pdsurf->pvBankBufferPlane0);
    }

    if (ppdev->pPointerAttributes != NULL) {
        LocalFree(ppdev->pPointerAttributes);
    }

    // Free any pending saved screen bit blocks.
    pSSB = pdsurf->ssbList;
    while (pSSB != (PSAVED_SCREEN_BITS) NULL) {
        // Point to the next saved screen bits block
        pSSBNext = (PSAVED_SCREEN_BITS) pSSB->pvNextSSB;
        // Free the current block
        LocalFree(pSSB);
        pSSB = pSSBNext;
    }

    EngDeleteSurface((HSURF) ppdev->hsurfEng);

    LocalFree(pdsurf);  // free the surface

    DISPDBG((2, "VGA:disabled surface\n"));

}

/******************************Public*Routine******************************\
* DrvAssertMode
*
* Ping the device back into its last known mode
*
\**************************************************************************/

VOID DrvAssertMode(DHPDEV dhpdev, BOOL Enable)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    ULONG   returnedDataLength;

    DISPDBG((2, "VGA: DrvAssertMode\n"));

    if (Enable) {

        //
        // The screen must be reenabled since we had gone to full screen,
        // or another pdev.
        // Re-initialize the device.
        //

        pPtrSave = ppdev->pPtrSave;
        pPtrWork = ppdev->pPtrWork;

        if (!bInitPointer(ppdev))
        {
            RIP("VGA DrvAssertMode failed bInitPointer\n");
        }

        if (!bInitVGA(ppdev, FALSE))
        {
            RIP("VGA DrvAssertMode failed bInitVGA\n");
        }

        vForceBank0(ppdev);

        // Restore the off screen data.  This protects the Desktop
        // from a DOS application that might trash the off screen
        // memory.

        ppdev->bBitsSaved = FALSE;  // clear the DrvSaveScreenBits info flag
                                    // ie. blow away cached screen region

    } else {

        //
        // We must give up the display.
        // Call the kernel driver to reset the device to a known state.
        //

        if (!DeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_RESET_DEVICE,
                             NULL,
                             0,
                             NULL,
                             0,
                             &returnedDataLength,
                             NULL)) {

            RIP("VGA.DLL: Reset Device Failed");

        }
    }

    return;
}

/******************************Public*Routine******************************\
* DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes(
HANDLE hDriver,
ULONG cjSize,
DEVMODEW *pdm)

{

    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation, pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;

    DISPDBG((2, "Vga.dll:DrvGetModes\n"));

    cModes = getAvailableModes(hDriver,
                               (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                               &cbModeSize);

    if (cModes == 0)
    {
        DISPDBG((0, "VGA DISP DrvGetModes failed to get mode information"));
        return 0;
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        //
        // Now copy the information for the supported modes back into the output
        // buffer
        //

        cbOutputSize = 0;

        pVideoTemp = pVideoModeInformation;

        do
        {
            if (pVideoTemp->Length != 0)
            {
                if (cOutputModes == 0)
                {
                    break;
                }

                //
                // Zero the entire structure to start off with.
                //

                memset(pdm, 0, sizeof(DEVMODEW));

                //
                // Set the name of the device to the name of the DLL.
                //

                memcpy(&(pdm->dmDeviceName), L"vga", sizeof(L"vga"));

                pdm->dmSpecVersion = DM_SPECVERSION;
                pdm->dmDriverVersion = DM_SPECVERSION;

                //
                // We currently do not support Extra information in the driver
                //

                pdm->dmDriverExtra = DRIVER_EXTRA_SIZE;

                pdm->dmSize = sizeof(DEVMODEW);
                pdm->dmBitsPerPel = pVideoTemp->NumberOfPlanes *
                                    pVideoTemp->BitsPerPlane;
                pdm->dmPelsWidth = pVideoTemp->VisScreenWidth;
                pdm->dmPelsHeight = pVideoTemp->VisScreenHeight;
                pdm->dmDisplayFrequency = pVideoTemp->Frequency;

                if (pVideoTemp->AttributeFlags & VIDEO_MODE_INTERLACED)
                {
                    pdm->dmDisplayFlags |= DM_INTERLACED;
                }

                //
                // Go to the next DEVMODE entry in the buffer.
                //

                cOutputModes--;

                pdm = (LPDEVMODEW) ( ((ULONG)pdm) + sizeof(DEVMODEW) +
                                                   DRIVER_EXTRA_SIZE);

                cbOutputSize += (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);

            }

            pVideoTemp = (PVIDEO_MODE_INFORMATION)
                (((PUCHAR)pVideoTemp) + cbModeSize);

        } while (--cModes);
    }

    LocalFree(pVideoModeInformation);

    return cbOutputSize;

}

/******************************Public*Routine******************************\
* VOID vInitSavedBits(ppdev)
*
* Initializes saved bits structures.  Must be done after bank
* initialization and vInitBrushCache.
*
\**************************************************************************/

VOID vInitSavedBits(PPDEV ppdev)
{
    if (!(ppdev->fl & DRIVER_OFFSCREEN_REFRESHED))
    {
        return;
    }

    //
    // set up rect to right of visible screen
    //
    ppdev->rclSavedBitsRight.left   = ppdev->sizlSurf.cx;
    ppdev->rclSavedBitsRight.top    = 0;
    ppdev->rclSavedBitsRight.right  = ppdev->sizlMem.cx-PLANAR_PELS_PER_CPU_ADDRESS;
    ppdev->rclSavedBitsRight.bottom = ppdev->sizlSurf.cy;

    if ((ppdev->rclSavedBitsRight.right <= ppdev->rclSavedBitsRight.left) ||
        (ppdev->rclSavedBitsRight.bottom <= ppdev->rclSavedBitsRight.top))
    {
        ppdev->rclSavedBitsRight.left   = 0;
        ppdev->rclSavedBitsRight.top    = 0;
        ppdev->rclSavedBitsRight.right  = 0;
        ppdev->rclSavedBitsRight.bottom = 0;
    }

    //
    // set up rect below visible screen
    //
    ppdev->rclSavedBitsBottom.left   = 0;
    ppdev->rclSavedBitsBottom.top    = ppdev->sizlSurf.cy;
    ppdev->rclSavedBitsBottom.right  = ppdev->sizlMem.cx-PLANAR_PELS_PER_CPU_ADDRESS;
    ppdev->rclSavedBitsBottom.bottom = ppdev->sizlMem.cy - ppdev->cNumScansUsedByPointer;

    if ((ppdev->rclSavedBitsBottom.right <= ppdev->rclSavedBitsBottom.left) ||
        (ppdev->rclSavedBitsBottom.bottom <= ppdev->rclSavedBitsBottom.top))
    {
        ppdev->rclSavedBitsBottom.left   = 0;
        ppdev->rclSavedBitsBottom.top    = 0;
        ppdev->rclSavedBitsBottom.right  = 0;
        ppdev->rclSavedBitsBottom.bottom = 0;
    }

    DISPDBG((1,"rclSavedBitsRight: (%4x,%4x,%4x,%4x)\n",
               ppdev->rclSavedBitsRight.left,
               ppdev->rclSavedBitsRight.top,
               ppdev->rclSavedBitsRight.right,
               ppdev->rclSavedBitsRight.bottom
            ));

    DISPDBG((1,"rclSavedBitsBottom: (%4x,%4x,%4x,%4x)\n",
               ppdev->rclSavedBitsBottom.left,
               ppdev->rclSavedBitsBottom.top,
               ppdev->rclSavedBitsBottom.right,
               ppdev->rclSavedBitsBottom.bottom
            ));

    //
    // NOTE: we have subtracted one DWORD from the right edge.  This is because
    //       later it is assumed that we can align by right shifting by up to
    //       one DWORD (unless of course, the width of the buffer is 0).
    //

    ppdev->bBitsSaved = FALSE;

    return;
}
