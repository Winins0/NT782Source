////////////////////////////////////////////////////////////////////////////////
//
//	MacPrint - Windows NT Print Server for Macintosh Clients
//		Copyright (c) Microsoft Corp., 1991, 1992, 1993
//
//	psp.c - Macintosh Print Service Postscript Parsing Routines
//
//	Author: Frank D. Byrum
//		adapted from MacPrint from LAN Manager Services for Macintosh
//
//	DESCRIPTION:
//		This module provides the routines to parse the Adobe DSC 2.0
//		comments in a PostScript stream.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <windows.h>
#include <macpsmsg.h>
#include <macps.h>
#include <pskey.h>
#include <debug.h>

// function prototypes
DWORD	HandleTitle(PJR pjr);
DWORD	HandleBeginExitServer(PJR pjr);
DWORD	HandleCreationDate(PJR pjr);
DWORD	HandleCreator(PJR pjr);
DWORD	HandleEndExitServer(PJR pjr);
DWORD	HandleEOF(PJR pjr);
DWORD	HandleFor(PJR pjr);
DWORD	HandleLogin(PJR pjr);
DWORD	HandleBeginProcSet(PJR pjr);
DWORD	HandleEndProcSet(PJR pjr);
DWORD	HandleIncludeProcSet(PJR pjr);
DWORD	HandleComment(PJR, PBYTE);
DWORD	HandleBeginBinary(PJR pjr);
DWORD	HandleEndBinary(PJR pjr);
DWORD	HandlePages(PJR pjr);
void	HandleJobComment (PJR, PBYTE);
PFR		ReAllocateFontList (PFR pfrOld, DWORD cOldFonts, DWORD cNewFonts);


char *	deffonts[DEFAULTFONTS] =
{
	FONT00,	FONT01,	FONT02,	FONT03,	FONT04,	FONT05,	FONT06,	FONT07,
	FONT08,	FONT09,	FONT10,	FONT11,	FONT12,	FONT13,	FONT14,	FONT15,
	FONT16,	FONT17,	FONT18,	FONT19,	FONT20,	FONT21,	FONT22,	FONT23,
	FONT24,	FONT25,	FONT26,	FONT27,	FONT28,	FONT29,	FONT30,	FONT31,
	FONT32,	FONT33,	FONT34
};

////////////////////////////////////////////////////////////////////////////////
//
//	SetDefaultPPDInfo() - Initialize to LaserWriter Plus configuration
//
//	DESCRIPTION:
//		This routine is used to set the default parameters of our
//		printer to LaserWriter Plus characteristics.  This is used
//		in the event there is no PPD file associated with the given
//		NT Printer Object (as in the case of non Postscript printers)
//
//		returns true if queue structure initialized OK.
//
////////////////////////////////////////////////////////////////////////////////
BOOLEAN
SetDefaultPPDInfo(
	PQR		pqr
)
{
	DWORD	i;

	//
	// initialize Postscript keywords
	//
	strcpy(pqr->LanguageVersion, ENGLISH);
	strcpy(pqr->Product, DEFAULTPRODUCTRESPONSE);
	strcpy(pqr->Version, DEFAULTPSVERSION);
	strcpy(pqr->Revision, DEFAULTPSREVISION);
	strcpy(pqr->DeviceNickName, UNKNOWNPRINTER);
	strcpy(pqr->pszColorDevice, COLORDEVICEDEFAULT);
	strcpy(pqr->pszResolution, RESOLUTIONDEFAULT);
	strcpy(pqr->pszLanguageLevel, DEFAULTLANGUAGELEVEL);
	pqr->FreeVM = VMDEFAULT;

	//
	// initialize font list
	//
	pqr->fonts = NULL;
	pqr->fonts = (PFR)LocalAlloc(LPTR, DEFAULTFONTS * sizeof (FONT_RECORD));
	if (pqr->fonts == NULL)
	{
		DBGPRINT(("ERROR: unable to allocate font data\n"));
		ReportEvent(
				hEventLog,
				EVENTLOG_ERROR_TYPE,
				EVENT_CATEGORY_INTERNAL,
				EVENT_SERVICE_OUT_OF_MEMORY,
				NULL, 0, 0, NULL, NULL);
		return (FALSE);
	}

	//
	// copy font names
	//

	for (i = 0; i < DEFAULTFONTS; i++)
	{
		strcpy(pqr->fonts[i].name, deffonts[i]);
	}
	pqr->MaxFontIndex = DEFAULTFONTS-1;

	return (TRUE);
}


////////////////////////////////////////////////////////////////////////////////
//
//	GetPPDInfo() - Initialize to LaserWriter Plus configuration
//
//	DESCRIPTION:
//		This routine is used to set the parameters of our
//		printer to the characteristics specified in the PPD
//		file for the printer.
//
//		returns true if queue structure initialized OK.
//
////////////////////////////////////////////////////////////////////////////////
BOOLEAN
GetPPDInfo(
	PQR		pqr
)
{
	FILE *  		ppdfile = NULL;
	char *			result = NULL;
	char *			token = NULL;
	char			line[PSLEN];
	PFR				fontPtr=NULL;
	USHORT  		MaxFonts = 100;
	USHORT  		fontindex = 0;
	LPDRIVER_INFO_2	pdiThis = NULL;
	DWORD			cbpdiThis = sizeof(DRIVER_INFO_2) + 256;
	LPSTR			pszPPDFile = NULL;
	BOOLEAN 		ReturnStatus = TRUE;
	HANDLE  		hPrinter = INVALID_HANDLE_VALUE;
	int				toklen;

	do
	{
		// get the path of the ppdfile
		if (!OpenPrinter(pqr->pPrinterName, &hPrinter, NULL))
		{
			hPrinter = INVALID_HANDLE_VALUE;
			DBGPRINT(("ERROR: unable to get printer handle, error=%d\n", GetLastError()));
			ReturnStatus = FALSE;
			break;
		}

		pdiThis = (LPDRIVER_INFO_2) LocalAlloc(LPTR, cbpdiThis);
		if (pdiThis == NULL)
		{
			DBGPRINT(("ERROR: unable to allocate new driverinfo buffer\n"));
			ReturnStatus = FALSE;
			break;
		}
		if (!GetPrinterDriver(hPrinter,
							  NULL,
							  2,
							  (LPBYTE) pdiThis,
							  cbpdiThis,
							  &cbpdiThis))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				DBGPRINT(("ERROR: unable to get printer driver info\n"));
				ReturnStatus = FALSE;
				break;
			}

			LocalFree(pdiThis);
			pdiThis = (LPDRIVER_INFO_2) LocalAlloc(LPTR, cbpdiThis);
			if (pdiThis == NULL)
			{
				DBGPRINT(("ERROR: unable to allocte new driverinfo buffer\n"));
				ReturnStatus = FALSE;
				break;
			}

			if (!GetPrinterDriver(hPrinter,
								  NULL,
								  2,
								  (LPBYTE) pdiThis,
								  cbpdiThis,
								  &cbpdiThis))
			{
				DBGPRINT(("ERROR: unable to get printer driver info\n"));
				ReturnStatus = FALSE;
				break;
			}
		}

		pszPPDFile = (LPSTR)LocalAlloc(LPTR, wcslen(pdiThis->pDataFile)+1);
		DBGPRINT(("pDataFile name length = %d\n", wcslen(pdiThis->pDataFile)));
		if (pszPPDFile == NULL)
		{
			DBGPRINT(("out of memory for pszPPDFile\n"));
			ReturnStatus = FALSE;
			break;
		}
		CharToOem(pdiThis->pDataFile, pszPPDFile);
		DBGPRINT(("pDataFile = %ws, pszPPDFile = %s\n", pdiThis->pDataFile, pszPPDFile));

		if ((ppdfile = fopen(pszPPDFile, "rt")) == NULL)
		{
			DBGPRINT(("File open error %s", pszPPDFile));
			ReturnStatus = FALSE;
			break;
		}

		/*
		 * Allocate a buffer for fonts. We don't know yet what size we need.
		 * We make a guess and increase the size as we go. The incremental
		 * size is 10 fonts. We start off with 100. We shrink the segment size
		 * to the final size.
		 */
		fontPtr = (PFR) LocalAlloc (LPTR, sizeof(FONT_RECORD)*MaxFonts);
		if (fontPtr == NULL)
		{
			DBGPRINT(("ERROR: cannot allocate font list buffer, error=%d\n", GetLastError()));
			ReturnStatus = FALSE;
			break;
		}

		while (result = fgets(line, PSLEN, ppdfile))
		{
			if (line[0] != ASTERISK || (token= strtok(line, " ")) == NULL)
				continue;

			// PPD Font Entry?
			if (!stricmp(line, ppdFONT))
			{
				/* This should be the fontname */
				if ((token= strtok(NULL, " :")) != NULL)
				{
					if (strlen(token) <= FONTNAMELEN)
					{
						strcpy(fontPtr[fontindex].name, token);
						DBGPRINT(("Font: %s\n", token));
						fontindex++;
						if (fontindex >= MaxFonts)
						{
							fontPtr = ReAllocateFontList (fontPtr, MaxFonts, MaxFonts + 10);
							if (fontPtr == NULL)
							{
								DBGPRINT(("ERROR: unable to grow font buffer, error=%d\n", GetLastError()));
								ReturnStatus = FALSE;
								break;
							}
							MaxFonts += 10;
						}
					}
					else DBGPRINT(("Fontname > PPDLEN ???\n"));
				}
			}
			else if (!stricmp(token, ppdPSVERSION))
			{
				// PPD Postscript Version Entry?
				/* Get the PostScript version */
				token= strtok(NULL, "()\""); /* This should be the version */
				toklen = strlen(token);

				/* Get the PostScript revision */
				if ((toklen <= PPDLEN) && (toklen > 0))
				{
					strcpy(pqr->Version, token);
					DBGPRINT(("Version: %s\n", pqr->Version));
				}
				else DBGPRINT(("Version > PPDLEN ???\n"));

				token= strtok(NULL, "()\""); /* This should be the revision */
				while (*token == ' ') token ++;
				toklen = strlen(token);
				if ((toklen <= PPDLEN) && (toklen > 0))
				{
					strcpy(pqr->Revision, token);
					DBGPRINT(("Revision: %s\n", pqr->Revision));
				}
				else DBGPRINT(("Revision > PPDLEN ???\n"));
			}
			else if (!stricmp(token, ppdNICKNAME))
			{
				// PPD NickName?
				/* Get the NICKNAME */
				token= strtok(NULL, "()\""); /* This should be the nickname */
				if (strlen(token) <= PPDLEN)
				{
					strcpy(pqr->DeviceNickName, token);
					DBGPRINT(("DeviceNickName: %s\n", pqr->DeviceNickName));
				}
				else DBGPRINT(("DeviceNickName > PPDLEN ???\n"));
			}
			else if (!stricmp(token, ppdLANGUAGEVERSION))
			{
				// PPD Postscript Language Version?
				/* Get the LANGUAGEVERSION */
				token= strtok(NULL, " :"); /* This should be the language */
				if (strlen(token) <= PPDLEN)
				{
					strcpy(pqr->LanguageVersion, token);
					DBGPRINT(("LanguageVersion: %s\n", pqr->LanguageVersion));
				}
				else DBGPRINT(("LanguageVersion > PPDLEN ???\n"));
			}
			else if (!stricmp(token, ppdPRODUCT))
			{
				// PPD Product ?
				/* Get the PRODUCT */
				token = strtok(NULL, "()\""); /* This should be the product */
				if (strlen(token) <= PPDLEN)
				{
					strcpy(pqr->Product, token);
					DBGPRINT(("Product: %s\n", pqr->Product));
				}
				else DBGPRINT(("Product > PPDLEN ???\n"));
			}
			else if (!stricmp(token, ppdFREEVM))
			{
				token= strtok(NULL, "()\""); /* This should be the product */
				sscanf(token, "%ld", &pqr->FreeVM);
				DBGPRINT(("Free VM: %ld\n", pqr->FreeVM));
			}
			else if (!stricmp(token, ppdCOLORDEVICE))
			{
				// this should be a string indicating color support or not
				// in the form of <True> or <False> (brackets not included)
				token = strtok(NULL, " :\x0d\x0a");
				if ((token != NULL) && (strlen(token) < COLORDEVICEBUFFLEN))
				{
					strcpy (pqr->pszColorDevice, token);
				}
				else
				{
					strcpy (pqr->pszColorDevice, COLORDEVICEDEFAULT);
				}
				DBGPRINT(("Color device: %s\n", pqr->pszColorDevice));
			}
			else if (!stricmp(token, ppdDEFAULTRESOLUTION))
			{
				// this should be a string indicating the default
				// resolution of the printer in the form <xxxxdpi>
				// where xxxx is a number
				token = strtok(NULL, " :\x0d\x0a");
				if ((token != NULL) && (strlen(token) < RESOLUTIONBUFFLEN))
				{
					strcpy (pqr->pszResolution, token);
				}
				else
				{
					strcpy (pqr->pszResolution, RESOLUTIONDEFAULT);
				}
				DBGPRINT(("Resolution: %s\n", pqr->pszResolution));
			}
			else if (!stricmp(token, ppdLANGUAGELEVEL))
			{
				// this should be the PostScript level ("1" or "2")
				// implemented in this printer
				token = strtok(NULL, " \"");
				if ((token != NULL) && (PPDLEN >= strlen(token)))
				{
					strcpy (pqr->pszLanguageLevel, token);
				}
				else
				{
					strcpy (pqr->pszLanguageLevel, DEFAULTLANGUAGELEVEL);
				}
				DBGPRINT(("Language Level: %s\n", pqr->pszLanguageLevel));
			}
		}

		if (!ReturnStatus)
		{
			pqr->fonts = NULL;
			pqr->MaxFontIndex = 0;
		}
		else
		{
			pqr->fonts = fontPtr;
			pqr->MaxFontIndex = fontindex-1;
		}
	} while (FALSE);

	if (pszPPDFile != NULL)
	{
		LocalFree(pszPPDFile);
	}

	if (ppdfile != NULL)
	{
		fclose(ppdfile);
	}

	if (hPrinter != INVALID_HANDLE_VALUE)
	{
		ClosePrinter(hPrinter);
	}

	if (pdiThis != NULL)
	{
		LocalFree(pdiThis);
	}

	if (!ReturnStatus)
	{
		if (fontPtr != NULL)
		{
			LocalFree(fontPtr);
		}
	}

	return (ReturnStatus);
}




PFR
ReAllocateFontList(
	PFR		pfrOld,
	DWORD	cOldFonts,
	DWORD	cNewFonts
)
{
	PFR pfrNew = NULL;

	DBGPRINT(("enter ReAllocateFontList()\n"));

	do
	{
		// allocate new font record
		pfrNew = LocalAlloc(LPTR, cNewFonts * sizeof(FONT_RECORD));
		if (pfrNew == NULL)
		{
			DBGPRINT(("LocalAlloc fails with %d\n", GetLastError()));
			break;
		}

		//
		// copy old font record
		//
		CopyMemory(pfrNew, pfrOld, cOldFonts * sizeof(FONT_RECORD));
	} while (FALSE);

	LocalFree(pfrOld);

	return pfrNew;
}


/*
**
** WriteToSpool()
**
**	Purpose: Determines if job stream is currently being written to
**		the spooler, then writes it to the file if it is being written.
**
**	Returns: fwrite return codes.
**
*/
DWORD
WriteToSpool(
	PJR		pjr,
	PBYTE	pchbuf,
	int		cchlen
)
{
	BOOL	SpoolIt=FALSE;
	DWORD	cbWritten;
	DWORD	dwError;

	if ((cchlen !=0) && (pchbuf != NULL) &&
		((pjr->psJobState==psExitServerJob) || (pjr->psJobState==psStandardJob)))
	{
		/* determine the data stream mode to know whether to write */
		switch (pjr->JSState)
		{
			case JSStripEOL:
			case JSStripKW:
			case JSStripTok:
				DBGPRINT(("POP - strip\n"));
				PopJSState(pjr);
				break;

			case JSWriteEOL:
			case JSWriteKW:
			case JSWriteTok:
				DBGPRINT(("POP - write\n"));
				PopJSState(pjr);
			case JSWrite:
				SpoolIt=TRUE;
				break;
		}

		// Do we write this Data to the Output Stream ?
		if (SpoolIt)
		{
			if (pjr->FirstWrite)
			{
				//
				// place comment in job to signal AppleTalk monitor not to filter control characters
				//
				if (!WritePrinter(pjr->hPrinter, FILTERCONTROL, SIZE_FC, &cbWritten))
				{
					dwError = GetLastError();
					DBGPRINT(("WritePrinter() failed with %d\n", dwError));
					return (dwError);
				}
				pjr->FirstWrite = FALSE;
			}
			if (!WritePrinter(pjr->hPrinter, pchbuf, cchlen, &cbWritten))
			{
				dwError = GetLastError();
				DBGPRINT(("ERROR: cannot write to printer, error = %x\n", dwError));
				return (dwError);
			}
		}
	}
	return NO_ERROR;
}


/*
** MoveToPending()
**
**	Purpose: Moves the buffer pointed at into the pending buffer.
**
**	Returns: DosWrite error codes.
**
*/
DWORD
MoveToPending(
	PJR		pjr,
	PBYTE	pchbuf,
	int		cchlen
)
{
	DBGPRINT(("Enter MoveToPending\n"));
	if ((cchlen > PSLEN) || (*pchbuf != '%'))
	{
		/*
		 * input line is not a comment and is conforming PostScript line,
		 * so give it to WriteToSpool
		 */
		DBGPRINT(("not a DSC comment, so sending to spooler\n"));
		return (WriteToSpool (pjr, pchbuf, cchlen));
	}

	pjr->PendingLen= cchlen;
	memcpy(&pjr->bufPool[pjr->bufIndx].PendingBuffer[PENDLEN-cchlen], pchbuf, cchlen);
	return (NO_ERROR);
}


/*
** TellClient ()
**
**	Purpose: Sends a message back to the client
**
**	Returns: Any of the PAPWrite return codes.
**
*/
DWORD
TellClient(
	PJR		pjr,
	BOOL	fEof,
	PBYTE	BuffPtr,
	int		cchlen
)
{
	DWORD			rc = NO_ERROR;
	fd_set			writefds;
	struct timeval  timeout;
	int				sendflag;
	int			 	wsErr;

	DBGPRINT(("enter TellClient()\n"));

	do
	{
		FD_ZERO(&writefds);
		FD_SET(pjr->sJob, &writefds);

		//
		// wait up to 30 seconds to be able to write
		//

		if (fEof)
		{
			sendflag = 0;
		}
		else
		{
			sendflag = MSG_PARTIAL;
		}

		timeout.tv_sec = 30;
		timeout.tv_usec = 0;

		DBGPRINT(("waiting for writeability\n"));

		wsErr = select(0, NULL, &writefds, NULL, &timeout);

		if (wsErr == 0)
		{
			DBGPRINT(("response to client times out\n"));
			rc = ERROR_SEM_TIMEOUT;
			break;
		}

		if (wsErr != 1)
		{
			rc = GetLastError();
			DBGPRINT(("select(writefds) fails with %d\n"));
			break;
		}

		if (send(pjr->sJob, BuffPtr, cchlen, sendflag) == SOCKET_ERROR)
		{
			rc = GetLastError();
			DBGPRINT(("send() fails with %d\n", rc));
			break;
		}
	} while (FALSE);

	return rc;
}


/*
**
** HandleBeginBinary()
**
**	Purpose: Handles BeginBinary Comment Events.
**
*/
DWORD
HandleBeginBinary(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleBeginBinary\n"));

	/* Process the BeginBinary Comment */
	pjr->InBinaryOp = TRUE;
	return NO_ERROR;
}


/*
**
** HandleEndBinary()
**
**	Purpose: Handles BeginBinary Comment Events.
**
*/
DWORD
HandleEndBinary(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleEndBinary\n"));

	// Process the EndBinary Comment
	pjr->InBinaryOp = FALSE;
	return NO_ERROR;
}


/*
**
** HandleBeginExitServer()
**
**	Purpose: Handles BeginExitServer Comment Events.
**
*/
DWORD
HandleBeginExitServer(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleBeginExitServer\n"));
	switch (pjr->psJobState)
	{
		case psQueryJob:
		case psExitServerJob:
			PushJSState(pjr, JSStrip);
			break;

		case psStandardJob:
			PushJSState(pjr, JSStripEOL);
			break;
	}
	return NO_ERROR;
}


/*
**
** HandleCreationDate()
**
**	Purpose: Handles CreationDate Comment Events.
**
**	Returns: Number of lines that should be skipped before scanning
**		for another event starts again.
**
*/
DWORD
HandleCreationDate(
	PJR		pjr
)
{
	return NO_ERROR;
}


/*
**
** HandleCreator() -
**
**	Purpose: Handles Creator Comment Events.
**
*/
DWORD
HandleCreator(
	PJR		pjr
)
{
	return NO_ERROR;
}



/*
**
** HandleEndExitServer()-
**
**	Purpose: Handles EndExitServer Comment Events.
**
*/
DWORD
HandleEndExitServer(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleEndExitServer\n"));

	if (pjr->psJobState == psStandardJob)
		PushJSState (pjr, JSStripEOL);

	return NO_ERROR;
}

/*
** HandleEOF()
**
** Purpose: Handles EOF Comment Events.
**
*/
DWORD
HandleEOF(
	PJR		pjr
)
{

	DBGPRINT(("Enter HandleEOF\n"));

	if (pjr->psJobState == psQueryJob || pjr->psJobState == psExitServerJob)
	{
		pjr->psJobState = psStandardJob;
	}
	// pjr->JSState = JSStripKW;

	return NO_ERROR;
}


/*
**
** HandleFor()
**
**	Purpose: Handles For Comment Events.
**
*/
DWORD
HandleFor(
	PJR		pjr
)
{

	LPSTR		token;
	BYTE		pbBuffer[GENERIC_BUFFER_SIZE];
	PJOB_INFO_1	pji1Job;
	DWORD		cbNeeded;
	DWORD		Status = NO_ERROR;

	DBGPRINT(("Enter HandleFor\n"));

	//
	// only look for name in main part of print job
	//
	if (pjr->psJobState != psStandardJob)
	{
		DBGPRINT(("not in standard job, skipping username\n"));
		return NO_ERROR;
	}

	//
	// make sure we haven't already set the title
	//

	if (pjr->dwFlags & JOB_FLAG_OWNERSET)
	{
		DBGPRINT(("owner already set, skipping username\n"));
		return NO_ERROR;
	}

	//
	// mark the job as having an owner
	//
	pjr->dwFlags |= JOB_FLAG_OWNERSET;

	//
	//	look for the client name in the comment and
	//	default if not found
	//
	if (((token = strtok(NULL, NULL_STR)) == NULL) ||
		(strchr(token, '*') != NULL))
	{
		token = CLIENTNAME;
	}

	//
	// get the current job info
	//
	pji1Job = (PJOB_INFO_1)pbBuffer;
	if (!GetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				pbBuffer,
				GENERIC_BUFFER_SIZE,
				&cbNeeded))
	{
		//
		// need more buffer?  If so, try again with a larger one
		//

		if (cbNeeded > GENERIC_BUFFER_SIZE)
		{
			DBGPRINT(("GetJob needs larger buffer.  Retrying\n"));
			pji1Job = (PJOB_INFO_1)LocalAlloc(LPTR, cbNeeded);
			if (pji1Job == NULL)
			{
				Status = GetLastError();
				DBGPRINT(("ERROR: out of memory in HandleFor\n"));
				return Status;
			}

			if (!GetJob(pjr->hPrinter,
						pjr->dwJobId,
						1,
						(LPBYTE)pji1Job,
						cbNeeded,
						&cbNeeded))
			{
				Status = GetLastError();
				DBGPRINT(("ERROR: second GetJob fails in HandleFor with %d\n",
					Status));
				return Status;
			}
		}
		else
		{
			Status = GetLastError();
			DBGPRINT(("GetJob fails with %d\n", Status));
			return Status	;
		}
	}

	//
	// change the username
	//
	OemToChar(token, pjr->pszUser);
	pji1Job->pUserName = pjr->pszUser;
	DBGPRINT(("Setting user name to %ws\n", pjr->pszUser));

	//
	// set new job information (do not change job position)
	//
	pji1Job->Position = 0;

	if (!SetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				(LPBYTE)pji1Job,
				0))
	{
		Status = GetLastError();
		DBGPRINT(("WARNING: tried to change user name and failed setjob with %d\n", Status));
	}

	return Status;
}


/*
**
** HandleLogin()
**
**	Purpose: Handles Login Comment Events.
**
**	Returns: PAPWrite errors.
**
*/
DWORD
HandleLogin(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleLogin\n"));
	PushJSState(pjr,JSStripEOL);
	return (TellClient(pjr, TRUE, LOGINRESPONSE, sizeof(LOGINRESPONSE)-1));
}


/*
**
** HandleTitle()
**
**	Purpose: Handles Title Comment Events.
**
*/
DWORD
HandleTitle(
	PJR		pjr
)
{
	LPSTR		token;
	LPWSTR		pszTitle;
	BYTE		pbBuffer[GENERIC_BUFFER_SIZE];
	PJOB_INFO_1	pji1Job;
	DWORD		cbNeeded;
	DWORD		Status = NO_ERROR;

	DBGPRINT(("Enter HandleTitle\n"));

	//
	// only get title if we are in main part of job
	//
	if (pjr->psJobState != psStandardJob)
	{
		DBGPRINT(("skipping this title, not main job\n"));
		return NO_ERROR	;
	}
#if 0
	//
	// make sure title not already set
	//
	// JH - Commented this out - what if we have already seen a title ? This fixes
	//		Chooser 8.0 'Cover Page' problem.
	//
	if (JOB_FLAG_TITLESET & pjr->dwFlags)
	{
		DBGPRINT(("title already set.  Skipping this title\n"));
		return NO_ERROR;
	}
#endif
	//
	// marke the title as set
	//

	pjr->dwFlags |= JOB_FLAG_TITLESET;

	//
	// get the current job data
	//

	pji1Job = (PJOB_INFO_1)pbBuffer;
	if (!GetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				pbBuffer,
				GENERIC_BUFFER_SIZE,
				&cbNeeded))
	{
		//
		// need more buffer?  If so, try again with a larger one
		//

		if (cbNeeded > GENERIC_BUFFER_SIZE)
		{
			DBGPRINT(("GetJob needs larger buffer.  Retrying\n"));
			pji1Job = (PJOB_INFO_1)LocalAlloc(LPTR, cbNeeded);
			if (pji1Job == NULL)
			{
				Status = GetLastError();
				DBGPRINT(("ERROR: out of memory\n"));
				return Status;
			}
			if (!GetJob(pjr->hPrinter,
						pjr->dwJobId,
						1,
						(LPBYTE)pji1Job,
						cbNeeded,
						&cbNeeded))
			{
				Status = GetLastError();
				DBGPRINT(("ERROR: second GetJob fails with %d\n", Status));
				return Status;
			}
		}
		else
		{
			Status = GetLastError();
			DBGPRINT(("GetJob fails with %d\n", Status));
			return Status;
		}
	}

	//
	// get the title
	//
	token = strtok(NULL, NULL_STR);
	pszTitle = (LPWSTR)LocalAlloc(LPTR, sizeof(WCHAR) * (strlen(token)+1));
	if (pszTitle == NULL)
	{
		Status = GetLastError();
		DBGPRINT(("out of memory for pszTitle\n"));
		return Status;
	}

	OemToChar(token, pszTitle);

	//
	// change the title
	//
	pji1Job->Position = 0;
	pji1Job->pDocument = pszTitle;
	DBGPRINT(("changing title to %ws\n", pszTitle));

	if (!SetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				(LPBYTE)pji1Job,
				0))
	{

		Status = GetLastError();
		DBGPRINT(("WARNING: tried to change title and failed setjob with %d\n", Status));
	}

	LocalFree(pszTitle);
	return Status;
}


/*
**
** HandleBeginProcSet()
**
**	Purpose: Handles Begining of a ProcSet Upload
**
*/
DWORD
HandleBeginProcSet(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleBeginProcSet\n"));
	return NO_ERROR;
}



/*
** HandleEndProcSet()
**
**	Purpose: Handles End of a procset inclusion.
**
*/
DWORD
HandleEndProcSet(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleEndProcSet\n"));
	return NO_ERROR;
}


/*
** HandleIncludeProcSet()
**
** Purpose: Handles end of a procset inclusion.
**
** Entry:
**	Pointer to Job Structure
**
** Exit:
**
**	0 if no error, otherwise error code.
*/
DWORD
HandleIncludeProcSet(
	PJR		pjr
)
{
	DBGPRINT(("Enter HandleIncludeProcSet\n"));

	return NO_ERROR;
}





///////////////////////////////////////////////////////////////////////////////
//
// HandlePages()
//
//  This comment includes the total number of pages in the job and is
//  used to set the jobinfo structure for the job with the total number
//  of pages
//
///////////////////////////////////////////////////////////////////////////////
DWORD
HandlePages(
	PJR		pjr
)
{
	LPSTR		token;
	DWORD		cPages = 0;
	BYTE		pbBuffer[GENERIC_BUFFER_SIZE];
	PJOB_INFO_1	pji1Job;
	DWORD		cbNeeded;
	DWORD		Status = NO_ERROR;

	DBGPRINT(("Enter HandlePages\n"));

	//
	// only get pages if we are in main part of job
	//
	if (pjr->psJobState != psStandardJob)
	{
		DBGPRINT(("skipping this comment, not main job\n"));
		return NO_ERROR	;
	}

	//
	// get the current job data
	//

	pji1Job = (PJOB_INFO_1)pbBuffer;
	if (!GetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				pbBuffer,
				GENERIC_BUFFER_SIZE,
				&cbNeeded))
	{
		//
		// GetJob failed, and buffer passed in is larger than the largest
		// possible buffer for a job_info_1, so abort this ADSC comment
		//

		Status = GetLastError();
		DBGPRINT(("GetJob() fails with %d\n", Status));
		return Status;
	}

	//
	// get the number of pages.  The comment is of the form %%Pages xx nn
	// where xx is the number of pages to display
	//

	token = strtok(NULL, " ");
	cPages = atoi(token);

	//
	// change the number of pages
	//

	pji1Job->Position = 0;
	pji1Job->TotalPages = cPages;
	DBGPRINT(("changing page count to %d\n", cPages));

	if (!SetJob(pjr->hPrinter,
				pjr->dwJobId,
				1,
				(LPBYTE)pji1Job,
				0))
	{
		Status = GetLastError();
		DBGPRINT(("SetJob fails with %d\n",Status));
	}

	return Status;
}


struct commtable
{
	PSZ	commentstr;
	DWORD	(near *pfnHandle)(PJR);
} commtable [] =
{
	{ FORCOMMENT,		HandleFor				},
	{ TITLECOMMENT,		HandleTitle				},
	{ BEXITSERVER,		HandleBeginExitServer	},
	{ EEXITSERVER,		HandleEndExitServer		},
	{ BPROCSET,			HandleBeginProcSet		},
	{ EPROCSET,			HandleEndProcSet		},
	{ INCLUDEPROCSET,	HandleIncludeProcSet	},
	{ CREATIONDATE,		HandleCreationDate		},
	{ CREATOR,			HandleCreator			},
	{ EOFCOMMENT,		HandleEOF				},
	{ LOGIN,			HandleLogin				},
	{ LOGINCONT,		HandleLogin				},
	{ BEGINBINARY,		HandleBeginBinary		},
	{ ENDBINARY,		HandleEndBinary			},
	{ PAGESCOMMENT,		HandlePages				},
	{ NULL,				NULL					}
};

/*
** HandleComment()
**
**	Purpose: Handles Comment Events.
**
*/
DWORD
HandleComment(
	PJR		pjr,
	PBYTE	ps
)
{
	PSZ	token;
	struct commtable *pct;
	DWORD  status = NO_ERROR;

	DBGPRINT(("Enter HandleComment\n"));

	if ((token = strtok(ps," :")) != NULL)
	{
		DBGPRINT(("Comment: %s\n", token));
		for (pct = commtable; pct->pfnHandle; pct++)
		{
			if (!stricmp(token, pct->commentstr))
			{
				status = pct->pfnHandle(pjr);
				break;
			}
		}
	}

	// No action on this keyword !!!
	return status;
}


/*
** HandleJobComment()
**
**	Purpose: This parses PostScript Job Comments
*/
void
HandleJobComment(
	PJR		pjr,
	PBYTE	ps
)
{
	char *token;

	DBGPRINT(("Enter HandleJobComment\n"));

	token= strtok(ps, " ");

	//
	// it's a job statement
	//

	if ((token = strtok(NULL, " ")) != NULL)
	{
		/* standard job identification */
		if (!strcmp(token, QUERYJOBID))
		{
			pjr->psJobState = psQueryJob;
			pjr->JSState = JSStrip;
			DBGPRINT(("This is a standard job\n"));
			return;
		}

		if (!strcmp(token, EXITJOBID))
		{
			pjr->psJobState = psExitServerJob;
			pjr->JSState = JSStrip;
			DBGPRINT(("This is an exitjob\n"));
			return;
		}
	}

	//
	// Job identification not recognized, but some PostScript hackers
	// put the program name in this comment, so we treat this as a standard
	// job
	//

	DBGPRINT(("This is an unknown jobtype - processing as standard job\n"));
	pjr->psJobState = psStandardJob;
	pjr->JSState = JSWrite;
}



/*  LineLength -
 *	Returns the number of bytes, including CR/LF to the next
 *	CR/LF in the buffer.  If no CR/LF found, returns -1
 */
int
LineLength(PBYTE pBuf, int cbBuf)
{

	int	 intLength = 0;

	while (intLength < cbBuf)
	{
		//
		// we are looking for a CR
		//
		if (pBuf[intLength] != '\x0d')
		{
			intLength++;
			continue;
		}

		//
		// we've found a CR.  If it's followed by a LF, return that
		// length too, otherwise, just return what we've found
		//
		if ((intLength + 1) < cbBuf)
		{
			if (pBuf[intLength + 1] == '\x0a')
			{
				return intLength + 2;
			}
		}

		return intLength + 1;
	}

	return (-1);
}



/*
**
** PSParse()
**
**	Purpose: This does the actual parsing of the PostScript Data Stream.
**		This routine is always called pointing to the data stream at
**		the beginning of a the Data Stream, or the beginning of a line.
**
**	Returns: PAPWrite error codes.
**
*/
DWORD
PSParse(
	PJR		pjr,
	PBYTE	pchbuf,
	int		cchlen
)
{
	int	cbskip;
	char	ps[PENDLEN];
	DWORD	err = NO_ERROR;

	DBGPRINT(("ENTER: PSParse()\n"));

	while (cchlen > 0)
	{
		if ((cbskip = LineLength(pchbuf, cchlen)) == -1)
			return (MoveToPending(pjr, pchbuf, cchlen));

		/* Determine what the event is */
		if ((cbskip < PSLEN) && (pchbuf[0] == '%'))
		{
			/* copy a comment into the ps string */
			memcpy(ps, pchbuf, cbskip);
			ps[cbskip-1] = 0;		// OverWrite the CR/LF

			if (ps[1] == '%')
			{
				 /* Its a Query Comment */
				if (ps[2] == '?'&& !pjr->InBinaryOp)
				{
					if (ps[3] == 'B')
					{
						/* Process the Begin Query Comment */
						if ((err = HandleBQComment(pjr, ps)) != NO_ERROR)
						{
							DBGPRINT(("PSParse: HandleBQComment %ld\n", err));
							return(err);
						}
					}
					else if (ps[3] == 'E')
					{
						if (pjr->InProgress ==  QUERYDEFAULT)
						{
							if ((err = FinishDefaultQuery(pjr, ps)) != NO_ERROR)
							{
								DBGPRINT(("PSParse: FinishDefaultQuery %ld\n", err));
								return(err);
							}
						}
					}
				}
				else
				{
					/* Process the Comment */
					if ((err = HandleComment(pjr, ps)) != NO_ERROR)
					{
						DBGPRINT(("PSParse: HandleComment %ld\n", err));
						return(err);
					}
				}
			}
			else if (ps[1] == '!'&& !pjr->InBinaryOp)
			{
				/* Process Job ID Comment */
				HandleJobComment(pjr, ps);
			}
		}

		/* Write the lines to the spoolfile? */
		if ((err = WriteToSpool (pjr, pchbuf, cbskip)) != NO_ERROR)
			return (err);

		pchbuf += cbskip;
		cchlen  -= cbskip;
	}
	return NO_ERROR;
}

