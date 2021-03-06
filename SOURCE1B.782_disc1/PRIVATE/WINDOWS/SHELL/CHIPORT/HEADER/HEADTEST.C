/****************************** Module Header ******************************\
* Module Name: winbez.c
*
* Copyright (c) 1991, Microsoft Corporation
*
* Window Bezier Demo
*
* History:
* 05-20-91 PaulB        Created.
\***************************************************************************/

#include "tabtest.h"
#include "global.h"
#include "resource.h"
//JV doesn't exist!! #include "headapp.h"
#include "headdlg.h"
#include "headins.h"         
//#include "headget.h"         
#include "headdel.h"
#include "headlay.h"

const TCHAR g_szStubAppClass[] = TEXT("StubAppClass");
HINSTANCE g_hinst = NULL;
TCHAR const g_szTabControlClass[] = WC_TABCONTROL;
TCHAR const g_szHeadControlClass[] = WC_HEADER;
#define WID_TABS    1
HWND hwndTab = NULL;
BOOL MsgTrackOn = FALSE;
HBITMAP hBitMap1 = NULL;  
HBITMAP hBitMap2 = NULL;       
HINSTANCE hShellLib;
TCHAR szDbgMsg[MAX_PSZTEXT];

/*
 * Forward declarations.
 */
BOOL InitializeApp(void);
LONG CALLBACK App_WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LONG CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void InitGlobals(void);
void HandleTheCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


#ifdef  WIN32JV
#define StrNCmp strncmp
#endif

//--------------------------------------------------------------------------
UINT wDebugMask = 0x00ff;

void WINAPI MyDebugMsg(UINT mask, LPCTSTR pszMsg, ...)
{
    TCHAR ach[256];

//    if (wDebugMask & mask)
//    {
    wvsprintf(ach, pszMsg, ((LPCTSTR FAR*)&pszMsg + 1));
    OutputDebugString(ach);
    OutputDebugString(TEXT("\r\n"));
//    }
}


/***************************************************************************\
* winmain
*
*
* History:
* 07-07-93 SatoNa      Created.
\***************************************************************************/
#ifndef WIN32JV
extern int WINAPI StrNCmp(LPTSTR, LPTSTR, int);
#endif
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
           LPSTR lpszCmdParam, int nCmdShow)
{
    MSG msg;
    int ret=0;

    // GetModuleHandle(NULL);
    g_hinst = hInstance;
    MyDebugMsg(DM_TRACE, TEXT("WinMain: App started (%x)"), g_hinst);
        StrNCmp(TEXT("hello"), TEXT("bar"),3);
#ifndef WIN32
    if (!Shell_Initialize())
    return 1;
#endif
    if (InitializeApp())
    {
    while (GetMessage(&msg, NULL, 0, 0))
    {
         TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    }
    else
    {
    ret=1;
    }
#ifndef WIN32
    Shell_Terminate();
#endif
    return 0;
//      return (msg.wParam);
}

/***************************************************************************\
* InitializeApp
*
* History:
* 07-07-93 SatoNa       Created.
\***************************************************************************/

BOOL InitializeApp(void)
{
    WNDCLASS wc;
    HWND hwndMain;

    wc.style            = CS_OWNDC | CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc      = App_WndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = g_hinst;
    wc.hIcon            = LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_ICON1));;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)COLOR_APPWORKSPACE;
    wc.lpszMenuName     = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName    = g_szStubAppClass;
    
    InitGlobals();
    
    hBitMap1 = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
    hBitMap2 = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2));
    if (!RegisterClass(&wc))
    {
    MyDebugMsg(DM_TRACE, TEXT("%s: Can't register class (%x)"),
         wc.lpszClassName, g_hinst);
    return FALSE;
    }

    hwndMain = CreateWindowEx(0L, g_szStubAppClass, TEXT("Header Control") WC_SUFFIX32,
        WS_OVERLAPPED | WS_CAPTION | WS_BORDER | WS_THICKFRAME |
        WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN |
        WS_VISIBLE | WS_SYSMENU,
        80, 70, 400, 300,
        NULL, NULL, g_hinst, NULL);

    if (hwndMain == NULL)
    return FALSE;
    ShowWindow(hwndMain, SW_SHOWNORMAL) ;
    UpdateWindow(hwndMain);

    SetFocus(hwndMain);    /* set initial focus */
    
    
    return TRUE;
}


void App_OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc=BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
}

void App_OnCreate(HWND hwnd, LPCREATESTRUCT lpc)
{
    RECT rcClient;
    /****** TC_ITEM ti = {
    TCIF_ALL,   // mask
    0,          // state
    0,          // stateMask
    NULL,       // pszText,
    0,          // cchTextMax
    0,          // iImage
    0           // lParam
    }; *******/
    
    HD_ITEM hi = {
    HDI_WIDTH|HDI_TEXT| HDI_FORMAT|HDI_LPARAM|HDI_BITMAP,
    100,
    NULL,
    NULL,
    128,
    HDF_CENTER|HDF_BITMAP,
    0
    };

    HBITMAP hbmp;                  
    int ret;
    int aZigzag[] = { 0xFF, 0xF7, 0xEB, 0xDD, 0xBE, 0x7F, 0xFF, 0xFF };

    hbmp = CreateBitmap(8, 8, 1, 1, aZigzag);


//      DeleteObject(hbmp);

#ifndef WIN32JV     //JV how does hShellLib ever get assigned??
#ifdef WIN32    
//    hShellLib = LoadLibrary("COMCTRL32.DLL");               
    if ((UINT)hShellLib > 32)
        MyDebugMsg(DM_TRACE, "Load Library is successful");
    else
        MyDebugMsg(DM_TRACE, "Could not load lib ");
#endif
#endif

    GetClientRect(hwnd, &rcClient);
    hwndTab=CreateWindowEx(0L, g_szHeadControlClass, NULL, //"Header Control Test " WC_SUFFIX32,
    WS_CHILD | HDS_HORZ | HDS_BUTTONS | HDS_DIVIDERTRACK |WS_VISIBLE,
        0, 0, 100, 100,
            hwnd, (HMENU) 0, g_hinst, NULL);                                         

    if (!hwndTab) { 
        sprintf(szTemp, TEXT("Couldnt create HeaderWindow %s"), g_szHeadControlClass);
        MessageBox(hwnd, szTemp, TEXT("CreateWindowEx"), MB_OK | MB_ICONSTOP);
//JV shouldn't return anything here guys...  return (FALSE); 
    }
#ifdef  WIN32JV
    else
    {
#endif

//    ShowWindow(hwnd, SW_SHOWNORMAL) ;
//    UpdateWindow(hWnd);
    hi.hbm = hbmp;
    hi.pszText=TEXT("One");
    Header_InsertItem(hwndTab, 1, &hi);
    hi.hbm = hBitMap1;
    hi.mask &= ~(HDI_TEXT);
    hi.pszText=TEXT("Two");
    Header_InsertItem(hwndTab, 2, &hi);
    hi.hbm = hBitMap2; 
    hi.pszText = TEXT("Three");
    hi.mask |= HDI_TEXT;
    Header_InsertItem(hwndTab, 3, &hi);
    ret = Header_GetItemCount(hwndTab);
    sii.hwnd = hwndTab;
    wsprintf(szTemp, TEXT("Item count: %d"), ret);        
//  MessageBox(hwnd, szTemp, "Header_GetItemCount", MB_OK);
#ifdef  WIN32JV
    }
#endif
}

void App_OnSize(HWND hwnd, UINT cx, UINT cy)
{
/*    HWND hwndTab=GetDlgItem(hwnd, WID_TABS);                             */
    TCHAR buf[100];  
    HGLOBAL hglb;
    
                       
    HD_LAYOUT FAR *playout;                                       
    HD_ITEM hi = {
    HDI_WIDTH|HDI_TEXT| HDI_FORMAT|HDI_LPARAM|HDI_BITMAP,
    10,
    NULL,
    NULL,
    128,
    HDF_CENTER,
    0
    };
    
    
    if (hwndTab)
    {
    SetWindowPos(hwndTab, NULL, 0, 0, cx, cy, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    /***
    hi.mask = HDI_WIDTH;
    hi.cxy = cx/3;
        hi.hbm = NULL;
    Header_SetItem(hwndTab, 1, &hi);
    Header_SetItem(hwndTab, 2, &hi);
        Header_SetItem(hwndTab, 3, &hi);  
    sprintf(buf, "There are %d items", Header_GetItemCount(hwndTab)); 
    MessageBox(hwndTab, buf,
            "ITEM COUNT", MB_OK);              
        hglb = GlobalAlloc(GPTR, sizeof(HD_LAYOUT));  
        playout = GlobalLock(hglb);
    //      Header_Layout(hwndTab, playout);
    //      sprintf(buf, "x = %d\t y = %d\n cx = %d\t cy = %d",
    //                      playout->pwpos->x, playout->pwpos->y, playout->pwpos->cx, playout->pwpos->cy);
        MessageBox(hwndTab, buf, "ITEM INFO", MB_OK);
        GlobalUnlock(hglb);     
        GlobalFree(hglb);               
    **/
    }
}                                             

static void
App_OnPress(HWND hwnd, HD_NOTIFY FAR *NotifyStruct, int HDNMsg)
{
  TCHAR  MsgBuf[100];
  
  sprintf(MsgBuf, TEXT("Button %d involved in notification: "), NotifyStruct->iItem);
  switch (NotifyStruct->hdr.code) {
    case HDN_ITEMCHANGING:
    case HDN_ITEMCHANGED:
        lstrcat(MsgBuf, TEXT("HDN_ITEMCHANGING"));
        break;
    
    case HDN_ITEMCLICK:
        lstrcat(MsgBuf, TEXT("HDN_ITEMCLICK"));
        sprintf(szTemp, TEXT(" MButton = %d "), NotifyStruct->iButton);
        lstrcat(MsgBuf, szTemp);
        break;
    
    case HDN_ITEMDBLCLICK:
        lstrcat(MsgBuf, TEXT("HDN_ITEMDBLCLICK"));
        sprintf(szTemp, TEXT(" MButton = %d "), NotifyStruct->iButton);
        lstrcat(MsgBuf, szTemp);
        break;      
        
    case HDN_DIVIDERCLICK:
        lstrcat(MsgBuf, TEXT("HDN_ITEMDIVIDERCLICK"));
        sprintf(szTemp, TEXT(" MButton = %d "), NotifyStruct->iButton);
        lstrcat(MsgBuf, szTemp);
        break;
        
    case HDN_DIVIDERDBLCLICK:
        lstrcat(MsgBuf, TEXT("HDN_DIVIDERDBLCLICK"));
        sprintf(szTemp, TEXT(" MButton = %d "), NotifyStruct->iButton);
        lstrcat(MsgBuf, szTemp);
        break;
        
    case HDN_BEGINTRACK:
        lstrcat(MsgBuf, TEXT("HDN_BEGINTRACK"));
        break;
    
    case HDN_ENDTRACK:
        lstrcat(MsgBuf, TEXT("HDN_ENDTRACK"));
        break;

  }
 
  MyDebugMsg(DM_TRACE, TEXT("%s"), (LPCTSTR) MsgBuf);
  // MessageBox(hwnd, MsgBuf, "Info", MB_OK);
  return;
}

//--------------------------------------------------------------------------
// App_WndProc
//
// History:
//  07-07-93 Satona      Created
//--------------------------------------------------------------------------

LONG CALLBACK App_WndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)                                 
{
    switch (message)
    {
    case WM_CREATE:
    App_OnCreate(hwnd, (LPCREATESTRUCT)lParam);
    break;  // should return 0.
               
    case WM_COMMAND:
    HandleTheCommand(hwnd, message, wParam, lParam);
    break;
                   
    case WM_DESTROY:
    PostQuitMessage(0);
    break;

    case WM_PAINT:
    App_OnPaint(hwnd);
    break;                                
    
    case WM_NOTIFY:
    if (MsgTrackOn)
        App_OnPress(hwnd,  (HD_NOTIFY FAR *)lParam, wParam);
    break;
    
    case WM_SIZE:
    App_OnSize(hwnd, LOWORD(lParam), HIWORD(lParam));
    break;

    default:
    return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0L;
}






void InitGlobals(void)
{

  /* not really too much to do here.  Create a hex wsprintf() filter since
     the app starts off in Hex mode. */

  lstrcpy(szShortFilter, TEXT("%x")) ;
  lstrcpy(szLongFilter, TEXT("%lx")) ;
  hInst = g_hinst;
}



void HandleTheCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

  switch (LOWORD(wParam))
  {
    case IDM_GETITEMCOUNT:                //For any of the dialog creation
      DoGetItemCountDialog(hWnd) ;       //commands, call the appropriate
      break ;                      //function.  The function will
                   //create the dialog...

    case IDM_INSERTITEM:
      DoInsertItemDialog(hWnd, wParam);
      break;
                     
    case IDM_GETITEM:
      DoInsertItemDialog(hWnd, wParam);
      break;                                                  
      
    case IDM_SETITEM:
      DoInsertItemDialog(hWnd, wParam);
      break;                             
    
    case IDM_DELETEITEM:
      DoDeleteItemDialog(hWnd);
      break;
      
    case IDM_LAYOUT:
      DoLayoutDialog(hWnd);
      break;
      
    case IDM_EXIT:
      PostQuitMessage(0) ;
      break ;


    case IDM_ABOUT:
//     DialogBox(hInst, TEXT("about"), hWnd, AboutProc) ;
       break ;          
       
    case IDM_TRACKON:
    MsgTrackOn = TRUE;
    break;
    
    case IDM_TRACKOFF:
    MsgTrackOn = FALSE;
    break;


    default: 
#ifdef  WIN32JV
        DefWindowProc(hWnd, message, wParam, lParam);
#else
        return (DefWindowProc(hWnd, message, wParam, lParam));    
#endif
    break ;
  }

  return ;
}




LONG MyAtol(LPTSTR szString, BOOL bHex,/*was LPBOOL*/ BOOL bSuccess)
{
  LPTSTR p ;
  LONG l ;
  LONG lMultiplier ;
  BOOL bDigit ;

  if (bHex)
    lMultiplier = 16 ;
  else
    lMultiplier = 10 ;

  p = szString ;
  l = 0 ;

  while (*p)      //while chars
  {
     bDigit = FALSE ;  //set to false for each char that we look at

     if (*p >= (TCHAR) '0' && *p <= (TCHAR) '9')  //is it an ascii char ?
     {
       bDigit = TRUE ;
       l+=(*p - (TCHAR) '0') ;
     }

     if (bHex)
     {
       if (*p >= (TCHAR) 'A' && *p <= (TCHAR) 'F')  //or hex?
       {
     l+=(*p - (TCHAR) 'A' + 10) ;
     bDigit = TRUE ;
       }

       if (*p >= (TCHAR) 'a' && *p <= (TCHAR) 'f') 
       {
     l+=(*p - (TCHAR) 'a' + 10) ;
     bDigit = TRUE ;
       }

     }

     if (bDigit == FALSE)
     {
       bSuccess = FALSE ;
       return 0 ;
     }

     p++ ;               //get next char

     if (*p)             //if there is going to be at least one more char
       l*=lMultiplier ;  //then multiply what we have by the multiplier...
  }

  bSuccess = TRUE ;

  return l ;             //success! return the value.
}







