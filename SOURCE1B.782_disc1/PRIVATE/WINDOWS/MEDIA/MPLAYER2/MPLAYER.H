/*-----------------------------------------------------------------------------+
| MPLAYER.H                                                                    |
|                                                                              |
| (C) Copyright Microsoft Corporation 1991.  All rights reserved.              |
|                                                                              |
| Revision History                                                             |
|    Oct-1992 MikeTri Ported to WIN32 / WIN16 common code                      |
|                                                                              |
+-----------------------------------------------------------------------------*/
#ifdef WIN32
#if DBG
#ifndef DEBUG
#define DEBUG
#endif
#endif
#endif

#include <stdlib.h> // Make sure _MAX_PATH is defined if it's there
#include <malloc.h>

#ifndef WIN32
#include <winnt.h>
#endif

#include "dynalink.h"
#include "menuids.h"
#include "helpids.h"
#include "mci.h"
#include "riffdisp.h"
#include "unicode.h"
#include "alloc.h"


#ifdef CHICAGO_PRODUCT
#include <commctrl.h>
#else
#include "mmcntrls.h"
#include "track.h"
#endif /* CHICAGO_PRODUCT */

/* Dialog style on Win4+
 */
#ifndef DS_CONTEXTHELP
#define DS_CONTEXTHELP 0
#endif

/* PORT1632 compatibility stuff.
 * The motown port1632.h conflicts with windows.h, so we'll have to define stuff here:
 */
#if defined(WIN32)
#define LONG2POINT(l, pt)               ((pt).x = (SHORT)LOWORD(l), (pt).y = (SHORT)HIWORD(l))
#define GETWINDOWUINT(hwnd, index)      (UINT)GetWindowLong(hwnd, index)
#define GETWINDOWID(hwnd)               GETWINDOWUINT((hwnd), GWL_ID)
#define GETHWNDINSTANCE(hwnd)           (HINSTANCE)GETWINDOWUINT((hwnd), GWL_HINSTANCE)
#define SETWINDOWUINT(hwnd, index, ui)  (UINT)SetWindowLong(hwnd, index, (LONG)(ui))
#define SETHWNDPARENT(hwnd, h)          (HWND)SETWINDOWUINT((hwnd), GWL_HWNDPARENT, h)
#define MMoveTo(hdc, x, y)               MoveToEx(hdc, x, y, NULL)
#define MSetViewportExt(hdc, x, y)       SetViewportExtEx(hdc, x, y, NULL)
#define MSetViewportOrg(hdc, x, y)       SetViewportOrgEx(hdc, x, y, NULL)
#define MSetWindowExt(hdc, x, y)         SetWindowExtEx(hdc, x, y, NULL)
#define MSetWindowOrg(hdc, x, y)         SetWindowOrgEx(hdc, x, y, NULL)
#define MGetCurrentTask                     GetCurrentThreadId
#define _EXPORT
BOOL WINAPI   MGetTextExtent(HDC hdc, LPSTR lpstr, INT cnt, INT *pcx, INT *pcy);
#else
#define LONG2POINT(l, pt)               ((pt).x = (INT)LOWORD(l), (pt).y = (INT)HIWORD(l))
#define GETWINDOWUINT(hwnd, index)      (UINT)GetWindowWord(hwnd, index)
#define GETWINDOWID(hwnd)               GETWINDOWUINT((hwnd), GWW_ID)
#define GETHWNDINSTANCE(hwnd)           (HMODULE)GETWINDOWUINT((hwnd), GWW_HINSTANCE)
#define SETWINDOWUINT(hwnd, index, ui)  (UINT)SetWindowWord(hwnd, index, (WORD)(ui))
#define SETHWNDPARENT(hwnd, h)          (HWND)SETWINDOWUINT((hwnd), GWW_HWNDPARENT, h)
#define MMoveTo                  (VOID)MoveTo
#define MSetViewportExt          (VOID)SetViewportExt
#define MSetViewportOrg          (VOID)SetViewportOrg
#define MSetWindowExt            (VOID)SetWindowExt
#define MSetWindowOrg            (VOID)SetWindowOrg
#define MGetCurrentTask          GetCurrentTask
#define _EXPORT _export
BOOL  FAR PASCAL     MGetTextExtent(HDC hdc, LPSTR lpstr, INT cnt, INT FAR * pcx, INT FAR * pcy);
#endif


#ifndef RC_INVOKED
#pragma warning(disable: 4001)  // nonstandard extension 'single line comment' was used
#pragma warning(disable: 4201)  // nonstandard extension used : nameless struct/union
#pragma warning(disable: 4214)  // nonstandard extension used : bit field types other than int
#pragma warning(disable: 4103)  // used #pragma pack to change alignment (on Chicago)
#endif

#ifdef OLE1_HACK
/* Disgusting OLE1 hackery:
 */
void Ole1UpdateObject(void);
extern DWORD gDocVersion;
#define DOC_VERSION_NONE    0
#define DOC_VERSION_OLE1    1
#define DOC_VERSION_OLE2    2

VOID SetDocVersion( DWORD DocVersion );

#endif /* OLE1_HACK */

//
//  have the debug version show preview.
//
extern BOOL    gfShowPreview;
#ifndef WIN32
#define GetOpenFileName (gfShowPreview ? GetOpenFileNamePreview : GetOpenFileName)
#else                        // Laurie's DREADFUL HACK !!! ???
// I'm getting compliation errors saying the GetOpenFileName is not defined
// so I have removed this macro to reduce obfuscation.  This should be
// repaired before we ship!
#endif

#define DEF_WIDTH       400
#define DEF_HEIGHT      124

#define CAPTION_LEN     80        // max length of caption

/* BOGUS constants for Layout() */
#define FSARROW_WIDTH            20        // width of one arrow bitmap for SB
#define FSARROW_HEIGHT           17        // height
#define FSTRACK_HEIGHT           30        // height of the scrollbar
#define LARGE_CONTROL_WIDTH     180        // Width of full transport toolbar
#define SMALL_CONTROL_WIDTH      78        // width with only 3 buttons
#define SMALL_MENU_WIDTH         33        // When to switch to small menus
#define SB_XPOS                   4        // how far in to put scrollbar
#define SHOW_MARK_WIDTH         363        // when to drop mark buttons
#define MAP_HEIGHT               14        // height of trackmap window
#define FULL_TOOLBAR_WIDTH      307        // when to drop last 4 transport btns
#define MARK_WIDTH               56        // width of mark button toolbar
#define MAX_NORMAL_HEIGHT        73        // max size for non-windowed device
#define MIN_NORMAL_HEIGHT        27        // min size for anybody

#ifndef _MAX_PATH
#define _MAX_PATH    144        /* max. length of full pathname */
#endif

#define TOOLBAR_HEIGHT      25
#define BUTTONWIDTH         24
#define BUTTONHEIGHT        22

#ifndef WIN32
#define CODEBASE _based(_segname("_CODE"))
#else
#define CODEBASE
#endif //WIN32

#define SZCODE TCHAR CODEBASE

/* Strings that MUST be ANSI even if we're compiling Unicode
 * e.g. to pass to GetProcAddress, which is ANSI only:
 */
#define ANSI_SZCODE CHAR CODEBASE
#define ANSI_TEXT( quote )  quote

/* Macro for string replacement parameter in printf etc:
 * (Anyone know of a better way to do this?)
 */
#ifdef UNICODE
#define TS  L"ws"L
#ifdef DEBUG
#define DTS "ws"    /* For unicode args to ASCII API */
#endif
#else
#define TS  "s"
#ifdef DEBUG
#define DTS "s"
#endif
#endif /* UNICODE */


/* defines for set sel dlg box */
#define IDC_EDITALL     220        // these 3 are a group
#define IDC_EDITSOME    221
#define IDC_EDITNONE    222

#define IDC_EDITFROM    223
#define IDC_EDITTO      224
#define IDC_EDITNUM     225

#define IDC_SELECTG     226     /* Needed for Context sensitive help */
#define IDC_ETTEXT      227
#define IDC_ESTEXT      228

#define ARROWEDITDELTA  10      /* Add to arrow ID to get edit ID */

#define IDC_XARROWS     180
#define IDC_YARROWS     181
#define IDC_WARROWS     182
#define IDC_HARROWS     183

#define IDC_FARROWS     183
#define IDC_TARROWS     184
#define IDC_NARROWS     210

/* Controls for Selection range dialog */
#define IDC_MARKIN      150
#define IDC_MARKOUT     151

#define DLG_MCICOMMAND  942
#define IDC_MCICOMMAND  10
#define IDC_RESULT      11

/* Bit fields for the gwOptions */
// bottom two bits = Scale Mode (01 = FRAMES) (10 = TIME) (11 = TRACK)
#define OPT_SCALE       0x0003
#define OPT_TITLE       0x0004
#define OPT_BORDER      0x0008
#define OPT_PLAY        0x0010
#define OPT_BAR         0x0020
#define OPT_DITHER      0x0040
#define OPT_AUTORWD     0x0080
#define OPT_AUTOREP     0x0100
#define OPT_USEPALETTE  0x0200
#define OPT_DEFAULT     (ID_TIME|OPT_TITLE|OPT_BORDER|OPT_PLAY|OPT_BAR)

#define OPT_FIRST       OPT_TITLE
#if 1
#define OPT_LAST        OPT_AUTOREP
#else
#define OPT_LAST        OPT_USEPALETTE
#endif

#define IDC_CAPTIONTEXT             202
#define IDC_TITLETEXT   160
#define TITLE_HEIGHT    TOOLBAR_HEIGHT  // height of title bar part of object picture

/* Options used when initial Open dialog is displayed:
 */
#define OPEN_MIDI       1
#define OPEN_VFW        2
#define OPEN_WAVE       3

/* Default extended window style to pass to CreateWindowEx */
#ifdef BIDI
#define WS_EX_BIDI  WS_EX_BIDI_SCROLL | WS_EX_BIDI_MENU | WS_EX_BIDI_NOICON
#else
#define WS_EX_BIDI  0
#endif

#define MCIWND_STYLE WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | \
                     MCIWNDF_NOPLAYBAR | MCIWNDF_NOMENU | MCIWNDF_SHOWNAME | \
                     MCIWNDF_NOTIFYALL

/* global array of all mci devices */

typedef struct {
    UINT    wDeviceType;        // flags, DTMCI_*
    PTSTR   szDevice;           // name used to open device e.g. "WaveAudio"
    PTSTR   szDeviceName;       // name to display to user  e.g. "Wave Audio"
    PTSTR   szFileExt;          // file extensions used by device.
}   MCIDEVICE, *PMCIDEVICE;

#define MAX_MCI_DEVICES     50

extern UINT         gwCurDevice;       /* current device */
extern UINT         gwNumDevices;      /* number of available devices   */
extern MCIDEVICE    garMciDevices[];   /* array with info about a device */

/* global variables */
extern BOOL    gfEmbeddedObject;       // TRUE if editing embedded OLE object
extern BOOL    gfPlayingInPlace;       // TRUE if playing in place
extern BOOL    gfParentWasEnabled;     // TRUE if parent was enabled
extern BOOL    gfShowWhilePlaying;     //
extern BOOL    gfDirty;                //
extern int     gfErrorBox;             // TRUE if we have a message box active
extern BOOL    gfErrorDeath;           // Die when errorbox is up

extern BOOL gfOle2IPEditing;
extern BOOL gfOle2IPPlaying;
extern RECT gInPlacePosRect;
extern HWND ghwndCntr;
extern HWND ghwndIPToolWindow;
extern HWND ghwndIPScrollWindow;

extern LPWSTR sz1Ole10Native;

extern UINT         gwOptions;         /* play options */
extern UINT         gwOpenOption;      /* Type of file to open */
extern BOOL         gfOpenDialog;      // If TRUE, put up open dialog
extern BOOL         gfCloseAfterPlaying;  // TRUE if we are to hide after play
extern BOOL         gfRunWithEmbeddingFlag; // TRUE if run with "-Embedding"
extern HMENU        ghMenu;            /* normal menu */
extern HMENU        ghMenuSmall;       /* short menu */
extern HICON        hiconApp;          /* the applicaiion's icon */
extern HANDLE       ghInst;            /* the application's instance handle   */
extern HANDLE       ghInstPrev;        /* the application's instance handle   */
extern HFONT        ghfontMap;         /* font used for drawing the track map */
extern HWND         ghwndApp;          /* handle to the main dialog window    */
extern HWND         ghwndMap;          /* handle to the track map window      */
extern HWND         ghwndTrackbar;     /* handle to the horizontal track bar  */
extern HWND         ghwndToolbar;      /* handle to the toolbar               */
extern HWND         ghwndMark;         /* handle to the Mark buttons toolbar  */
extern HWND         ghwndFSArrows;     /* handle to the scrollbar arrows      */
extern HWND         ghwndStatic;       /* handle ot the static text window    */
extern HWND         ghwndMCI;          /* MCI window returned from MCIWndCreate */
extern HBRUSH       ghbrFillPat;       /* The selection fill pattern.         */
extern UINT         gwHeightAdjust;    /* Difference between client & non-c height */
extern LPTSTR       gszCmdLine;        /* null-terminated command line str.   */
extern int          giCmdShow;         /* show command                        */
extern UINT         gwDeviceID;        /* current MCI device ID (or NULL)     */
extern UINT         gwStatus;          /* status of current MCI device        */
extern UINT         gwNumTracks;       /* current # of tracks in the medium   */
extern UINT         gwFirstTrack;      /* number of first track                      */
extern BOOL         gfValidMediaInfo;  /* are we displaying valid media info? */
extern BOOL         gfValidCaption;    /* are we displaying a valid caption?  */
extern BOOL         gfPlayOnly;        /* play only window?  */
extern BOOL         gfJustPlayed;      /* Just sent a PlayMCI() command       */
extern DWORD        gdwLastSeekToPosition; /* Last requested seek position    */
extern DWORD        gdwMediaLength;    /* length of entire medium in msec.    */
extern DWORD        gdwMediaStart;     /* start of medium in msec.              */
extern DWORD NEAR * gadwTrackStart;    /* array of track start positions      */
extern UINT         gwOptions;               /* the options from the dlg box        */
extern SZCODE       aszIntl[];         /* string for international section */
extern TCHAR        gachAppName[];     /* string for holding the app. name  */
extern TCHAR        gachClassRoot[];   /* string for holding the class root OLE "Media Clip"*/
extern TCHAR        chDecimal;         /* Stores current decimal separator */
extern TCHAR        chTime;            /* Stores the current Time separator */
extern TCHAR        chLzero;           /* Stores if Leading Zero required for decimal numbers less than 1 */
extern TCHAR        aszNotReadyFormat[];
extern TCHAR        aszReadyFormat[];
extern TCHAR        gachFileDevice[_MAX_PATH];  /* string holding the current file or device name */
extern TCHAR        gachWindowTitle[_MAX_PATH]; /* string holding name */
extern TCHAR        gachCaption[_MAX_PATH];     /* string holding name */
extern TCHAR        gachDocTitle[_MAX_PATH];        /* string holding doc name */

extern TCHAR        gszHelpFileName[];/* name of the help file        */

extern DWORD        gdwSeekPosition;  /* Position to seek to after skipping track. */
extern BOOL         gfScrollTrack;    /* is user dragging the scrollbar thumb?  */
extern UINT         gwCurScale;       /* scale is TIME/TRACKS/FRAMES */
extern INT          gwCurZoom;        /* current zoom factor */

extern HWND         ghwndMCI;         /* current window for window objects */
extern RECT         grcSize;          /* size of MCI object */
extern BOOL         gfAppActive;      /* Our app active? (incl. playback win) */
extern LONG         glSelStart;       /* See if selection changes (dirty object)*/
extern LONG         glSelEnd;         /* See if selection changes (dirty object)*/

extern HPALETTE     ghpalApp;
extern BOOL     gfInClose;
extern BOOL     gfWinIniChange;

extern DWORD        gdwPosition;      /* The Seek position we emedded */
extern BOOL         gfSeenPBCloseMsg;
extern int      giHelpContext;    /* Contains the context id for help */

/* constants */

#define DEVNAME_MAXLEN            40               /* the maximum length of a device name */

/* IDs of icons and dialog boxes */

#define APPICON                 10     /* ID of the application's icon        */
#define	IDI_DMP			11
#define	IDI_DSOUND		12
#define	IDI_DVIDEO		13
#define	IDI_DANI		14
#define	IDI_DMIDI		15
#define	IDI_DCDA		16
#define IDC_DRAG                17


#define MPLAYERBOX              11     /* ID of the main "MPlayer" dialog box */
#define MPLAYERACCEL             1

/* IDs of the MPLAYERBOX (main dialog box) controls */

#define ID_MAP                  20     /* ID of thetrack map                  */
#define ID_SB                   21     /* ID of the horizontal scrollbar      */
#define ID_STATIC               22     /* ID of the static text control       */
#define ID_PLAY                 23     /* Command   'Play'                    */
#define ID_PAUSE                24     /* Command   'Pause'                   */
#define ID_STOP                 25     /* Command   'Stop'                    */
#define ID_PLAYSEL              26     /* Command   'Play Selection'          */
#define ID_PLAYTOGGLE           27     /* For accelerator to toggle play/pause*/
#define ID_EJECT                28     /* Command   'Eject'                   */

/* IDs of the string resources */

#define IDS_APPNAME             100    /* ID of the application name string   */
#define IDS_OPENTITLE           101    /* ID of the "Open Media File" str.    */
#define IDS_OUTOFMEMORY         102    /* ID of the "Out of memory" string    */
#define IDS_CANTOPENFILEDEV     103    /* ID of the "Cannot open file/dev" str*/
#define IDS_DEVICEINUSE         104    /* ID of the "Device is in use" string */
#define IDS_CANTOPENFILE        105    /* ID of the "Cannot open file" string */
#define IDS_CANTACCESSFILEDEV   106    /* ID of the "Cannot access..." str.   */
#define IDS_DEVICECANNOTPLAY    107    /* ID of the "Cannot play..." str.     */
#define IDS_SCALE               108    /* ID of the "Scale" string            */
#define IDS_CANTPLAY            109
#define IDS_CANTEDIT            110
#define IDS_CANTCOPY            111
#define IDS_FINDFILE            112
#define IDS_DEVICENOTINSTALLED  113
#define IDS_DEVICEERROR         114    /* ID of the "Device error" str.              */
#define IDS_CANTPLAYSOUND       115
#define IDS_CANTPLAYVIDEO       116
#define IDS_CANTPLAYMIDI        117
#define IDS_NOTSOUNDFILE        118
#define IDS_NOTVIDEOFILE        119
#define IDS_NOTMIDIFILE         120

#define IDS_TIMEMODE            121    /* Moved "Set Selection" strings from  */
#define IDS_FRAMEMODE           122    /* being embedded in DLGS.C !          */
#define IDS_TRACKMODE           123

#define IDS_SEC         124 /* "hrs", "min", "sec" and "msec" string ids. */
#define IDS_MIN         125
#define IDS_HRS         126
#define IDS_MSEC        127
#define IDS_FRAME       128 /* "frame" string id.               */

#define IDS_UPDATEOBJECT        129

#define IDS_NOTREADYFORMAT      132
#define IDS_READYFORMAT         133
#define IDS_ALLFILES            134
#define IDS_ANYDEVICE           135
#define IDS_CLASSROOT           136
#define IDS_NOGOODTIMEFORMATS   137
#define IDS_NOTIFYFAILURE       138
#define IDS_FRAMERANGE          139
#define IDS_INIFILE             140    /* ID of the private INI file name     */
#define IDS_HELPFILE            141    /* ID of help file name     */

#define IDS_NOMCIDEVICES        143    /* no MCI devices are installed.  */
#define IDS_PLAYVERB            144
#define IDS_EDITVERB            145
#define IDS_CANTSTARTOLE        146
#define IDS_NOHANDLER           147
#define IDS_UNTITLED            149
#define IDS_BADOBJECT		150

#define IDS_CANTLOADLIB         151
#define IDS_CANTFINDPROC        152


/* These macros and typedefs can be used to clarify whether we're talking about
 * numbers of characters or numbers of bytes in a given buffer.
 * Unfortunately C doesn't give us type checking to do this properly.
 */
#define BYTE_COUNT( buf )   sizeof(buf)
#define CHAR_COUNT( buf )   (sizeof(buf) / sizeof(TCHAR))

/* Find how many bytes are in a given string:
 */
#define STRING_BYTE_COUNT( str )    (STRLEN(str) * sizeof(TCHAR) + sizeof(TCHAR))
#define ANSI_STRING_BYTE_COUNT( str )   (strlen(str) * sizeof(CHAR) + sizeof(CHAR))

/* Check the length of a string or number of bytes where NULL is a valid
 * pointer which should give a length of 0:
 */
#define STRLEN_NULLOK( str )    ((str) ? STRLEN(str) : 0)
#define STRING_BYTE_COUNT_NULLOK( str ) ((str) ? STRING_BYTE_COUNT(str) : 0)

/* Use this macro for loading strings.  It make the parameter list tidier
 * and ensures we pass the buffer size in CHARACTERS, not bytes.
 * This assumes that the buffer is static, not dynamically allocated.
 */
#define LOADSTRING( id, buf )   LoadString(ghInst, id, buf, CHAR_COUNT(buf))
#define LOADSTRINGFROM( inst, id, buf )   LoadString(inst, id, buf, CHAR_COUNT(buf))

/* track map scale values */

#define SCALE_HOURS             153    /* draw the scale in hours          */
#define SCALE_MINUTES           154    /* draw the scale in minutes        */
#define SCALE_SECONDS           155    /* draw the scale in seconds        */
#define SCALE_MSEC              156    /* draw it in milli vanilliseconds  */
#define SCALE_FRAMES            157    /* draw scale in 'frames'           */
#define SCALE_TRACKS            158
#define SCALE_NOTRACKS          159

#define IDS_CLOSE               160
#define IDS_UPDATE              161
#define IDS_NOPICTURE           162
#define IDS_EXIT                163
#define IDS_EXITRETURN          164

#define IDS_SSNOTREADY      170
#define IDS_SSPAUSED        171
#define IDS_SSPLAYING       172
#define IDS_SSSTOPPED       173
#define IDS_SSOPEN      174
#define IDS_SSPARKED        175
#define IDS_SSRECORDING     176
#define IDS_SSSEEKING       177
#define IDS_SSUNKNOWN       178

#define IDS_OLEVER      180
#define IDS_OLEINIT     181
#define IDS_OLENOMEM        182

//InPlace menu names.
#define IDS_EDITMENU        185
#define IDS_INSERTMENU      186
#define IDS_SCALEMENU       187
#define IDS_COMMANDMENU     188
#define IDS_HELPMENU        189
#define IDS_NONE                190

#define IDS_MSGFORMAT		191

#define IDS_FORMATEMBEDDEDTITLE 192

// String for registry fix-up message
#define IDS_BADREG          193
#define IDS_FIXREGERROR     194

/* macros for displaying error messages */

#define MB_ERROR    (MB_ICONEXCLAMATION | MB_OK)
#define Error(hwnd, idsFmt)                                               \
    ( ErrorResBox(hwnd, ghInst, MB_ERROR, IDS_APPNAME, (idsFmt)), FALSE )
#define Error1(hwnd, idsFmt, arg1)                                        \
    ( ErrorResBox(hwnd, ghInst, MB_ERROR, IDS_APPNAME, (idsFmt), (arg1)), \
        FALSE )
#define Error2(hwnd, idsFmt, arg1, arg2)                                  \
    ( ErrorResBox(hwnd, ghInst, MB_ERROR, IDS_APPNAME,  (idsFmt), (arg1), \
        (arg2)), FALSE )


/* app tabbing code */
BOOL IsDannyDialogMessage(HWND hwndApp, MSG msg);

/* calc where the tics belong on the scrollbar */
void FAR PASCAL CalcTicsOfDoom(void);

/* Layout the main window and the children */
void FAR PASCAL Layout(void);

/* prototypes from "errorbox.c" */
short FAR cdecl ErrorResBox(HWND hwnd, HANDLE hInst, UINT flags,
        UINT idAppName, UINT idErrorStr, ...);

/* function prototypes from "framebox.c" */
BOOL FAR PASCAL frameboxInit(HANDLE hInst, HANDLE hPrev);
/* function prototypes from "dlgs.c" */
BOOL FAR PASCAL mciDialog(HWND hwnd);
BOOL FAR PASCAL setselDialog(HWND hwnd);
BOOL FAR PASCAL optionsDialog(HWND hwnd);
/* function prototypes from "arrow.c" */
BOOL FAR PASCAL ArrowInit(HANDLE hInst);

/* function prototypes from "mplayer.c" */
void FAR PASCAL FormatTime(DWORD dwPosition, LPTSTR szNum, LPTSTR szBuf, BOOL fRound);
void FAR PASCAL UpdateDisplay(void);
void FAR PASCAL EnableTimer(BOOL fEnable);
LONG FAR PASCAL MPlayerWndProc(HWND hwnd, UINT wMsg, WPARAM wParam, LPARAM lParam);

/* function prototypes from "init.c" */
BOOL FAR PASCAL AppInit(HANDLE hInst, HANDLE hPrev, LPTSTR szCmdLine);
void FAR PASCAL InitMPlayerDialog(HWND hwnd);
BOOL FAR PASCAL GetIntlSpecs(void);

void FAR PASCAL SmartWindowPosition (HWND hWndDlg, HWND hWndShow, BOOL fForce);

void FAR PASCAL SizeMPlayer(void);
void FAR PASCAL SetMPlayerSize(LPRECT prc);

void FAR PASCAL InitDeviceMenu(void);

VOID FAR PASCAL CompleteOpenDialog(BOOL FileSelected);

/* function prototypes from "open.c" */

BOOL FAR PASCAL DoOpen(UINT wCurDevice, LPTSTR SzFileName);
BOOL FAR PASCAL DoChooseDevice(UINT wID);
void FAR PASCAL CheckDeviceMenu(void);
BOOL FAR PASCAL OpenMciDevice(LPCTSTR szFile, LPCTSTR szDevice);
UINT FAR PASCAL IsMCIDevice(LPCTSTR szDevice);

/* function prototype from "trackmap.c" */

LONG FAR PASCAL fnMPlayerTrackMap(HWND hwnd, UINT wMsg, UINT wParam,
    LONG lParam);

/* function prototype from init.c */
void FAR PASCAL WriteOutPosition(void);
void FAR PASCAL WriteOutOptions(void);
void FAR PASCAL ReadOptions(void);

void MapStatusString(LPTSTR lpstatusstr);

/* function prototype from "math.asm", if needed */

#ifdef WIN32
#define MULDIV32( number, numerator, denominator )  \
    MulDiv( (int)number, (int)numerator, (int)denominator )
#else
DWORD FAR PASCAL muldiv32(long, long, long);
#define MULDIV32( number, numerator, denominator )  \
    muldiv32( (long)number, (long)numerator, (long)denominator )
#endif


/* function prototype from "doverb.c" */
LPTSTR FAR FileName(LPCTSTR);

/* constants */

#define WM_USER_DESTROY     (WM_USER+120)
#define WM_SEND_OLE_CHANGE  (WM_USER+122)
#define WM_STARTTRACK       (WM_USER+123)
#define WM_ENDTRACK         (WM_USER+124)
#define WM_BADREG           (WM_USER+125)


/* constants */

#define SCROLL_GRANULARITY  ((gdwMediaLength+127)/128) /* granularity of the scrollbar  */
#define SCROLL_BIGGRAN      ((gdwMediaLength+15)/16)   /* gran. for page up/down in time mode */
#define UPDATE_TIMER        1                /* # of the timer being used     */
#define UPDATE_MSEC         500              /* msec between display updates  */
#define UPDATE_INACTIVE_MSEC 2000             /* msec betw. upds. when inactive*/
#define SKIPTRACKDELAY_MSEC 3000             /* max msec for double-page-left */

#define SetDlgFocus(hwnd) SendMessage(ghwndApp, WM_NEXTDLGCTL, (WPARAM)(hwnd), 1L)

#ifdef DEBUG
        #define STATICDT
        #define STATICFN
        int __iDebugLevel;

        extern void cdecl dprintf(LPSTR, ...);

        #define DPF0                         dprintf
        #define DPF  if (__iDebugLevel >  0) dprintf
        #define DPFI if (__iDebugLevel >= 1) dprintf
        #define DPF1 if (__iDebugLevel >= 1) dprintf
        #define DPF2 if (__iDebugLevel >= 2) dprintf
        #define DPF3 if (__iDebugLevel >= 3) dprintf
        #define DPF4 if (__iDebugLevel >= 4) dprintf
        #define CPF
#else
        #define STATICDT    static
        #define STATICFN    static
        #define DPF0       ; / ## /
        #define DPFI       ; / ## /
        #define DPF        ; / ## /
        #define DPF1        ; / ## /
        #define DPF2        ; / ## /
        #define DPF3        ; / ## /
        #define DPF4        ; / ## /

        #define CPF          / ## /
#endif

/*----Constants--------------------------------------------------------------*/
    /* Push buttons */
#define psh15       0x040e
#define pshHelp     psh15
    /* Checkboxes */
#define chx1        0x0410
    /* Static text */
#define stc1        0x0440
#define stc2        0x0441
#define stc3        0x0442
#define stc4        0x0443
    /* Listboxes */
#define lst1        0x0460
#define lst2        0x0461
    /* Combo boxes */
#define cmb1        0x0470
#define cmb2        0x0471
    /* Edit controls */
#define edt1        0x0480


/**************************************************************************
***************************************************************************/

#define GetWS(hwnd)     GetWindowLong(hwnd, GWL_STYLE)
#define PutWS(hwnd, f)  SetWindowLong(hwnd, GWL_STYLE, f)
#define TestWS(hwnd,f)  (GetWS(hwnd) & f)
#define SetWS(hwnd, f)  ((PutWS(hwnd, GetWS(hwnd) | f) & (f)) != (f))
#define ClrWS(hwnd, f)  ((PutWS(hwnd, GetWS(hwnd) & ~(f)) & (f)) != 0)

#define GetWSEx(hwnd)    GetWindowLong(hwnd, GWL_EXSTYLE)
#define PutWSEx(hwnd, f) SetWindowLong(hwnd, GWL_EXSTYLE, f)
#define SetWSEx(hwnd, f) ((PutWSEx(hwnd, GetWSEx(hwnd) | f) & (f)) != (f))
#define ClrWSEx(hwnd, f) ((PutWSEx(hwnd, GetWSEx(hwnd) & ~(f)) & (f)) != 0)


// server related stuff.

void FAR PASCAL ServerUnblock(void);
void FAR PASCAL BlockServer(void);
void FAR PASCAL UnblockServer(void);
void FAR PASCAL PlayInPlace(HWND hwndApp, HWND hwndClient, LPRECT prc);
void FAR PASCAL EditInPlace(HWND hwndApp, HWND hwndClient, LPRECT prc);
void FAR PASCAL EndPlayInPlace(HWND hwndApp);
void FAR PASCAL EndEditInPlace(HWND hwndApp);
void FAR PASCAL DelayedFixLink(UINT verb, BOOL fShow, BOOL fActivate);
void DirtyObject(BOOL fDocStgChangeOnly);
void CleanObject(void);
void UpdateObject(void);
BOOL FAR PASCAL IsObjectDirty(void);
void FAR PASCAL TerminateServer(void);
void FAR PASCAL SetEmbeddedObjectFlag(BOOL flag);
HMENU GetInPlaceMenu(void);
void PostCloseMessage();
void FAR PASCAL InitDoc(BOOL fUntitled);
