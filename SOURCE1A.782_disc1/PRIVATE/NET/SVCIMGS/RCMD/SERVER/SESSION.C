/****************************** Module Header ******************************\
* Module Name: session.c
*
* Copyright (c) 1991, Microsoft Corporation
*
* Remote shell session module
*
* History:
* 06-28-92 Davidc       Created.
* 05-05-94 DaveTh       Modifed for RCMD service
\***************************************************************************/


#include <nt.h>         // RcDbgPrint prototype
#include <ntrtl.h>      // RcDbgPrint prototype
#include <windef.h>
#include <nturtl.h>     // needed for winbase.h
#include <winbase.h>

#include "rcmdsrv.h"
#include <io.h>
#include <stdlib.h>

//
// Define shell command line
//

#define SHELL_COMMAND_LINE  TEXT("cmd /q")
#define SHELL_COMMAND_PREFIX TEXT("cmd /c ")
#define SHELL_CMD_PREFIX_LEN 7

//
// Define buffer size for reads/writes to/from shell
//

#define SHELL_BUFFER_SIZE   1000

//
//  Home directory constants
//

#define MAX_DIRECTORY_LENGTH 255
#define DEFAULT_DIRECTORY TEXT("\\USERS\\DEFAULT")

//
// Define multiple wait handle table size - extra handle for service stop
//

#define NUM_WAIT_HANDLES  5

//
// Define the structure used to describe each session
//

typedef struct {

    //
    // These fields are filled in at session creation time
    //

    HANDLE  ShellReadPipeHandle;        // Handle to shell stdout pipe
    HANDLE  ShellWritePipeHandle;        // Handle to shell stdin pipe
    HANDLE  ShellProcessHandle;     // Handle to shell process
    DWORD   ShellProcessGroupId;        // Process group ID of shell process
    PCHAR  DefaultDirectory;            // Default directory

    //
    // These fields maintain the state of asynchronouse reads/writes
    // to the shell process across client disconnections. They
    // are initialized at session creation.
    //

    BYTE    ShellReadBuffer[SHELL_BUFFER_SIZE]; // Data for shell reads goes here
    HANDLE  ShellReadAsyncHandle;   // Object used for async reads from shell

    BYTE    ShellWriteBuffer[SHELL_BUFFER_SIZE]; // Data for shell writes goes here
    HANDLE  ShellWriteAsyncHandle; // Object used for async writes to shell

    //
    // These fields are filled in at session connect time and are only
    // valid when the session is connected
    //

    HANDLE  ClientPipeHandle;       // Handle to client pipe
    HANDLE  SessionThreadHandle;    // Handle to session thread


} SESSION_DATA, *PSESSION_DATA;




//
// Private prototypes
//

HANDLE
StartShell(
    HANDLE StdinPipeHandle,
    HANDLE StdoutPipeHandle,
    PSESSION_DATA Session,
    HANDLE TokenToUse,
    PCOMMAND_HEADER LpCommandHeader
    );

DWORD
SessionThreadFn(
    LPVOID Parameter
    );


//
// Useful macros
//

#define SESSION_CONNECTED(Session) ((Session)->ClientPipeHandle != NULL)




/////////////////////////////////////////////////////////////////////////////
//
// CreateSession
//
// Creates a new session. Involves creating the shell process and establishing
// pipes for communication with it.
//
// Returns a handle to the session or NULL on failure.
//
/////////////////////////////////////////////////////////////////////////////

HANDLE
CreateSession(
    HANDLE TokenToUse,
    PCOMMAND_HEADER LpCommandHeader
    )
{
    PSESSION_DATA Session = NULL;
    BOOL Result;
    SECURITY_ATTRIBUTES SecurityAttributes;
    SECURITY_DESCRIPTOR SecurityDescriptor;
    HANDLE ShellStdinPipe = NULL;
    HANDLE ShellStdoutPipe = NULL;
    UCHAR HomeDirectory[MAX_DIRECTORY_LENGTH];
    WIN32_FIND_DATA FileFindData;

    //
    // Allocate space for the session data
    //

    Session = (PSESSION_DATA)Alloc(sizeof(SESSION_DATA));
    if (Session == NULL) {
	return(NULL);
    }

    //
    // Reset fields in preparation for failure
    //

    Session->ShellReadPipeHandle  = NULL;
    Session->ShellWritePipeHandle = NULL;
    Session->ShellReadAsyncHandle = NULL;
    Session->ShellWriteAsyncHandle = NULL;


    //
    // Create the I/O pipes for the shell - give world access so that spawned
    // command process can access them in client's contex
    //

    Result = InitializeSecurityDescriptor (
		    &SecurityDescriptor,
		    SECURITY_DESCRIPTOR_REVISION);

    if (!Result) {
	RcDbgPrint("Failed to initialize shell pipe security descriptor, error = %d\n", GetLastError());
	goto Failure;
    }

    Result = SetSecurityDescriptorDacl (
		    &SecurityDescriptor,
		    TRUE,
		    NULL,
		    FALSE);

    if (!Result) {
	RcDbgPrint("Failed to set shell pipe DACL, error = %d\n", GetLastError());
	goto Failure;
    }

    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.lpSecurityDescriptor = &SecurityDescriptor; // Use world-ACL
    SecurityAttributes.bInheritHandle = TRUE; // Shell will inherit handles

    Result = MyCreatePipe(&Session->ShellReadPipeHandle,
			  &ShellStdoutPipe,
			  &SecurityAttributes,
			  0,            // Default pipe size
			  0,            // Default timeout
			  FILE_FLAG_OVERLAPPED, // shell read flags
			  0              // shell stdout flags
			  );
    if (!Result) {
	RcDbgPrint("Failed to create shell stdout pipe, error = %d\n", GetLastError());
	goto Failure;
    }

    Result = MyCreatePipe(&ShellStdinPipe,
			  &Session->ShellWritePipeHandle,
			  &SecurityAttributes,
			  0,            // Default pipe size
			  0,            // Default timeout
			  0,            // shell stdin flags
			  FILE_FLAG_OVERLAPPED // shell write flags
			  );
    if (!Result) {
	RcDbgPrint("Failed to create shell stdin pipe, error = %d\n", GetLastError());
	goto Failure;
    }


    //
    // Initialize async objects
    //

    Session->ShellReadAsyncHandle = CreateAsync(FALSE);
    if (Session->ShellReadAsyncHandle == NULL) {
	RcDbgPrint("Failed to create shell read async object, error = %d\n", GetLastError());
	goto Failure;
    }

    Session->ShellWriteAsyncHandle = CreateAsync(FALSE);
    if (Session->ShellWriteAsyncHandle == NULL) {
	RcDbgPrint("Failed to create shell write async object, error = %d\n", GetLastError());
	goto Failure;
    }

    //
    //  Set default directory to HOMEDRIVE:HOMEPATH if it exists
    //

    if ((getenv("SystemRoot") != NULL) &&
	    (strlen(getenv("SystemRoot")) < MAX_DIRECTORY_LENGTH))  {
	strcpy(HomeDirectory, getenv("SystemRoot"));
	HomeDirectory[2] = '\0';    // terminate drive string
	strcat(HomeDirectory, DEFAULT_DIRECTORY);

	if (FindFirstFile(HomeDirectory, &FileFindData) == INVALID_HANDLE_VALUE)  {
	    Session->DefaultDirectory = NULL;
	}  else  {
	    Session->DefaultDirectory = HomeDirectory;
	}

    }  else  {

	Session->DefaultDirectory = NULL;

    }


    //
    //  Start command shell with pipes connection to stdin/out/err
    //

    Session->ShellProcessHandle = StartShell(ShellStdinPipe, ShellStdoutPipe, Session, TokenToUse, LpCommandHeader);

    //
    //  Close local handles
    //

    if (!(CloseHandle(ShellStdinPipe) &&
	  CloseHandle(ShellStdoutPipe)))  {
	ShellStdinPipe = NULL;
	ShellStdoutPipe = NULL;
	RcDbgPrint("Failed to close local pipe handles, error = %d\n", GetLastError());
	goto Failure;
    }

    //
    // Check result of shell start
    //

    if (Session->ShellProcessHandle == NULL) {
	RcDbgPrint("Failed to execute shell\n");
	goto Failure;
    }

    //
    // The session is not yet connected, initialize variables to indicate that
    //

    Session->ClientPipeHandle = NULL;

    //
    // Success, return the session pointer as a handle
    //

    return((HANDLE)Session);



Failure:

    //
    // We get here for any failure case.
    // Free up any resources and exit
    //


    //
    // Cleanup shell pipe handles
    //

    if (ShellStdinPipe != NULL) {
	MyCloseHandle(ShellStdinPipe, "shell stdin pipe (shell side)");
    }

    if (ShellStdoutPipe != NULL) {
	MyCloseHandle(ShellStdoutPipe, "shell stdout pipe (shell side)");
    }

    if (Session->ShellReadPipeHandle != NULL) {
	MyCloseHandle(Session->ShellReadPipeHandle, "shell read pipe (session side)");
    }

    if (Session->ShellWritePipeHandle != NULL) {
	MyCloseHandle(Session->ShellWritePipeHandle, "shell write pipe (session side)");
    }


    //
    // Cleanup async data
    //

    if (Session->ShellReadAsyncHandle != NULL) {
	DeleteAsync(Session->ShellReadAsyncHandle);
    }

    if (Session->ShellWriteAsyncHandle != NULL) {
	DeleteAsync(Session->ShellWriteAsyncHandle);
    }


    //
    // Free up our session data
    //

    Free(Session);

    return(NULL);
}




/////////////////////////////////////////////////////////////////////////////
//
// DeleteSession
//
// Deletes the session specified by SessionHandle.
//
// Returns nothing
//
/////////////////////////////////////////////////////////////////////////////

VOID
DeleteSession(
    HANDLE  SessionHandle
    )
{
    PSESSION_DATA   Session = (PSESSION_DATA)SessionHandle;
    BOOL Result;


    //
    // Kill off the shell process
    //

    Result = TerminateProcess(Session->ShellProcessHandle, 1);
    if (!Result) {
	RcDbgPrint("May have failed to terminate shell, error = %d\n", GetLastError());
    }

    MyCloseHandle(Session->ShellProcessHandle, "shell process");


    //
    // Close the shell pipe handles
    //

    MyCloseHandle(Session->ShellReadPipeHandle, "shell read pipe (session side)");
    MyCloseHandle(Session->ShellWritePipeHandle, "shell write pipe (session side)");


    //
    // Cleanup async data
    //

    DeleteAsync(Session->ShellReadAsyncHandle);
    DeleteAsync(Session->ShellWriteAsyncHandle);


    //
    // Free up the session structure
    //

    Free(Session);

    //
    // We're done
    //

    return;
}




/////////////////////////////////////////////////////////////////////////////
//
// ConnectSession
//
// Connects the session specified by SessionHandle to a client
// on the other end of the pipe specified by PipeHandle
//
// Returns a session disconnect notification handle or NULL on failure.
// The returned handle will be signalled if the client disconnects or the
// shell terminates.
//
/////////////////////////////////////////////////////////////////////////////

HANDLE
ConnectSession(
    HANDLE  SessionHandle,
    HANDLE  ClientPipeHandle
    )
{
    PSESSION_DATA   Session = (PSESSION_DATA)SessionHandle;
    DWORD ThreadId;

    assert(ClientPipeHandle != NULL);

    //
    // Fail if the session is already connected
    //

    if (SESSION_CONNECTED(Session)) {
	RcDbgPrint("Attempted to connect session already connected\n");
	return(NULL);
    }


    //
    // Store the client pipe handle in the session structure so the thread
    // can get at it. This also signals that the session is connected.
    //

    Session->ClientPipeHandle = ClientPipeHandle;


    //
    // Create the session thread
    //

    Session->SessionThreadHandle = CreateThread(
				     NULL,
				     0,                 // Default stack size
	     (LPTHREAD_START_ROUTINE)SessionThreadFn,   // Start address
			     (LPVOID)Session,           // Parameter
				     0,                 // Creation flags
				     &ThreadId          // Thread id
				     );
    if (Session->SessionThreadHandle == NULL) {

	RcDbgPrint("Failed to create session thread, error = %d\n", GetLastError());

	//
	// Reset the client pipe handle to indicate this session is disconnected
	//

	Session->ClientPipeHandle = NULL;
    }

    return(Session->SessionThreadHandle);
}





/////////////////////////////////////////////////////////////////////////////
//
// StartShell
//
// Execs the shell with the specified handle as stdin, stdout/err
//
// Returns process handle or NULL on failure
//
/////////////////////////////////////////////////////////////////////////////

HANDLE
StartShell(
    HANDLE StdinPipeHandle,
    HANDLE StdoutPipeHandle,
    PSESSION_DATA Session,
    HANDLE TokenToUse,
    PCOMMAND_HEADER LpCommandHeader
    )
{

    PROCESS_INFORMATION ProcessInformation;
    SECURITY_DESCRIPTOR SecurityDescriptor;
    STARTUPINFO si;
    HANDLE ProcessHandle = NULL;
    PROCESS_ACCESS_TOKEN ProcessTokenInfo;
    DWORD Result;
    UCHAR ShellCommandLine[MAX_CMD_LENGTH+SHELL_CMD_PREFIX_LEN+1];

    //
    // Detached process has no console - create new process with stdin,
    // stdout set to the passed-in handles
    //

    //
    // Initialize process startup info
    //

    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = NULL;
    si.lpTitle = NULL;
    si.lpDesktop = NULL;
    si.dwX = si.dwY = si.dwXSize = si.dwYSize = 0L;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput =  StdinPipeHandle;
    si.hStdOutput = StdoutPipeHandle;
    si.hStdError =  StdoutPipeHandle;
    si.wShowWindow = SW_SHOW;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;

    //
    //  Set command string for single command or interactive mode
    //

    if (LpCommandHeader->CommandLength == 0) {
	strcpy (ShellCommandLine, SHELL_COMMAND_LINE);
    }  else  {

	//
	// Construct command string for execute and exit
	//

	strcpy (ShellCommandLine, SHELL_COMMAND_PREFIX);
	strncat (ShellCommandLine,
		 LpCommandHeader->Command,
		 LpCommandHeader->CommandLength);
    }

    //
    // Create process with stdin/out/err connected to session pipes.
    // Create suspended then set the process token to the token of the
    // connected client (primary version of impersonate token).
    //

    if (CreateProcess(NULL,
		      ShellCommandLine,
		      NULL,
		      NULL,
		      TRUE, // Inherit handles
		      CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
		      NULL,
		      Session->DefaultDirectory,
		      &si,
		      &ProcessInformation)) {

	ProcessHandle = ProcessInformation.hProcess;
	Session->ShellProcessGroupId = ProcessInformation.dwProcessId;

    } else {
	RcDbgPrint("Failed to execute shell, error = %d\n", GetLastError());
	return(NULL);
    }

    //
    //  Set process token for client and local system context access
    //  BUGBUG - actually, world, for now
    //

    Result = InitializeSecurityDescriptor (
		    &SecurityDescriptor,
		    SECURITY_DESCRIPTOR_REVISION);

    if (!Result)  {
	RcDbgPrint("Failed to initialize shell process security descriptor, error = %d\n", GetLastError());
	MyCloseHandle(ProcessInformation.hThread, "process thread");
	return(NULL);
    }

    Result = SetSecurityDescriptorDacl (
		    &SecurityDescriptor,
		    TRUE,
		    NULL,
		    FALSE);

    if (!Result)  {
	RcDbgPrint("Failed to initialize shell process DACL, error = %d\n", GetLastError());
	MyCloseHandle(ProcessInformation.hThread, "process thread");
	return(NULL);
    }

    if (!SetKernelObjectSecurity(
	    ProcessHandle,
	    DACL_SECURITY_INFORMATION,
	    &SecurityDescriptor ))  {

	RcDbgPrint("Failed to set DACL on client token, error = %d\n", GetLastError());
	MyCloseHandle(ProcessInformation.hThread, "process thread");
	return(NULL);
    }

    //
    //  Now set the process token and resume execution
    //

    ProcessTokenInfo.Token = TokenToUse;
    ProcessTokenInfo.Thread = ProcessInformation.hThread;

    if (!NT_SUCCESS(NtSetInformationProcess(
	    ProcessInformation.hProcess,
	    ProcessAccessToken,
	    &ProcessTokenInfo,
	    sizeof(ProcessTokenInfo)))) {

	RcDbgPrint("Failed to set token");
	MyCloseHandle(ProcessInformation.hThread, "process thread");
	return (NULL);
    }

    Result = ResumeThread (ProcessInformation.hThread);

    if (Result != 1)  {
	RcDbgPrint("Failed to resume shell, suspend = %d, error = %d\n", Result, GetLastError());
	MyCloseHandle(ProcessInformation.hThread, "process thread");
	return(NULL);
    }

    MyCloseHandle(ProcessInformation.hThread, "process thread");

    return(ProcessHandle);
}








/////////////////////////////////////////////////////////////////////////////
//
// SessionThreadFn
//
// This is the code executed by the session thread
//
// Waits for read or write from/to shell or client pipe and termination
// event. Handles reads or writes by passing data to either client or
// shell as appropriate. Any error or termination event being signalled
// causes the thread to exit with an appropriate exit code.
//
/////////////////////////////////////////////////////////////////////////////

DWORD
SessionThreadFn(
    LPVOID Parameter
    )
{
    PSESSION_DATA   Session = (PSESSION_DATA)Parameter;
    HANDLE  ClientReadAsyncHandle;
    HANDLE  ClientWriteAsyncHandle;
    DWORD   BytesTransferred;
    DWORD   CompletionCode;
    BOOL    Result;
    DWORD   WaitResult;
    DWORD   ExitCode;
    HANDLE  WaitHandles[NUM_WAIT_HANDLES];
    BOOL    Done;
    DWORD   i;


    //
    // Initialize the client async structures
    //

    ClientReadAsyncHandle = CreateAsync(TRUE);
    if (ClientReadAsyncHandle == NULL) {
	RcDbgPrint("Failed to create client read async object, error = %d\n", GetLastError());
	return((DWORD)ConnectError);
    }

    ClientWriteAsyncHandle = CreateAsync(TRUE);
    if (ClientWriteAsyncHandle == NULL) {
	RcDbgPrint("Failed to create client write async object, error = %d\n", GetLastError());
	DeleteAsync(ClientReadAsyncHandle);
	return((DWORD)ConnectError);
    }



    //
    // Initialize the handle array we'll wait on
    //

    WaitHandles[0] = RcmdStopEvent;
    WaitHandles[1] = GetAsyncCompletionHandle(Session->ShellReadAsyncHandle);
    WaitHandles[2] = GetAsyncCompletionHandle(Session->ShellWriteAsyncHandle);
    WaitHandles[3] = GetAsyncCompletionHandle(ClientReadAsyncHandle);
    WaitHandles[4] = GetAsyncCompletionHandle(ClientWriteAsyncHandle);

    //
    // Wait on our handle array in a loop until an error occurs or
    // we're signalled to exit.
    //

    Done = FALSE;

    while (!Done) {

	//
	// Wait for one of our objects to be signalled.
	//

	WaitResult = WaitForMultipleObjects(NUM_WAIT_HANDLES, WaitHandles, FALSE, INFINITE);

	if (WaitResult == WAIT_FAILED) {
	    RcDbgPrint("Session thread wait failed, error = %d\n", GetLastError());
	    ExitCode = (DWORD)ConnectError;
	    break; // out of while
	}


	switch (WaitResult-WAIT_OBJECT_0) {
	case 0:

	    //
	    //  Service being stopped
	    //

	    RcDbgPrint("Service Shutdown\n");
	    ExitCode = (DWORD)ServiceStopped;
	    Done = TRUE;
	    break;  // out of switch

	case 1:

	    //
	    // Shell read completed
	    //

	    CompletionCode = GetAsyncResult(Session->ShellReadAsyncHandle,
					    &BytesTransferred);

	    if (CompletionCode != ERROR_SUCCESS) {
		RcDbgPrint("Async read from shell returned error, completion code = %d\n", CompletionCode);
		ExitCode = (DWORD)ShellEnded;
		Done = TRUE;
		break; // out of switch
	    }

	    //
	    // Start an async write to client pipe
	    //

	    Result = WriteFileAsync(Session->ClientPipeHandle,
				    Session->ShellReadBuffer,
				    BytesTransferred,
				    ClientWriteAsyncHandle);
	    if (!Result) {
		RcDbgPrint("Async write to client pipe failed, error = %d\n", GetLastError());
		ExitCode = (DWORD)ClientDisconnected;
		Done = TRUE;
	    }

	    break; // out of switch


	case 4:

	    //
	    // Client write completed
	    //

	    CompletionCode = GetAsyncResult(ClientWriteAsyncHandle,
					    &BytesTransferred);

	    if (CompletionCode != ERROR_SUCCESS) {
		RcDbgPrint("Async write to client returned error, completion code = %d\n", CompletionCode);
		ExitCode = (DWORD)ClientDisconnected;
		Done = TRUE;
		break; // out of switch
	    }

	    //
	    // Start an async read from shell
	    //

	    Result = ReadFileAsync(Session->ShellReadPipeHandle,
				   Session->ShellReadBuffer,
				   sizeof(Session->ShellReadBuffer),
				   Session->ShellReadAsyncHandle);
	    if (!Result) {
		RcDbgPrint("Async read from shell failed, error = %d\n", GetLastError());
		ExitCode = (DWORD)ShellEnded;
		Done = TRUE;
	    }

	    break; // out of switch


	case 3:

	    //
	    // Client read completed
	    //

	    CompletionCode = GetAsyncResult(ClientReadAsyncHandle,
					    &BytesTransferred);

	    if (CompletionCode != ERROR_SUCCESS) {
		RcDbgPrint("Async read from client returned error, completion code = %d\n", CompletionCode);
		ExitCode = (DWORD)ClientDisconnected;
		Done = TRUE;
		break; // out of switch
	    }

	    //
	    // Check for Ctrl-C from the client
	    //

	    for (i=0; i < BytesTransferred; i++) {
		if (Session->ShellWriteBuffer[i] == '\003') {

		    //
		    // Generate control-C:  use Ctrl-Break because ctrl-c is
		    // disabled for process group
		    //

		    if (!(GenerateConsoleCtrlEvent (CTRL_BREAK_EVENT,
						   Session->ShellProcessGroupId)))  {
			RcDbgPrint("Ctrl-break event failure, error = %d\n", GetLastError());
		    }

		    //
		    // Remove the Ctrl-C from the buffer
		    //

		    BytesTransferred --;

		    for (; i < BytesTransferred; i++) {
			Session->ShellWriteBuffer[i] = Session->ShellWriteBuffer[i+1];
		    }
		}
	    }

	    //
	    // Start an async write to shell
	    //

	    Result = WriteFileAsync(Session->ShellWritePipeHandle,
				    Session->ShellWriteBuffer,
				    BytesTransferred,
				    Session->ShellWriteAsyncHandle);
	    if (!Result) {
		RcDbgPrint("Async write to shell failed, error = %d\n", GetLastError());
		ExitCode = (DWORD)ShellEnded;
		Done = TRUE;
	    }

	    break; // out of switch



	case 2:

	    //
	    // Shell write completed
	    //

	    CompletionCode = GetAsyncResult(Session->ShellWriteAsyncHandle,
					    &BytesTransferred);

	    if (CompletionCode != ERROR_SUCCESS) {
		RcDbgPrint("Async write to shell returned error, completion code = %d\n", CompletionCode);
		ExitCode = (DWORD)ShellEnded;
		Done = TRUE;
		break; // out of switch
	    }

	    //
	    // Start an async read from client
	    //

	    Result = ReadFileAsync(Session->ClientPipeHandle,
				   Session->ShellWriteBuffer,
				   sizeof(Session->ShellWriteBuffer),
				   ClientReadAsyncHandle);
	    if (!Result) {
		RcDbgPrint("Async read from client failed, error = %d\n", GetLastError());
		ExitCode = (DWORD)ClientDisconnected;
		Done = TRUE;
	    }

	    break; // out of switch


	default:

	    RcDbgPrint("Session thread, unexpected result from wait, result = %d\n", WaitResult);
	    ExitCode = (DWORD)ConnectError;
	    Done = TRUE;
	    break;

	}
    }



    //
    // Cleanup and exit
    //

    //
    // Closing the client pipe should interrupt any pending I/O so
    // we should then be safe to close the event handles in the client
    // overlapped structs
    //

    Result = DisconnectNamedPipe(Session->ClientPipeHandle);
    if (!Result) {
	RcDbgPrint("Session thread: disconnect client named pipe failed, error = %d\n", GetLastError());
    }

    MyCloseHandle(Session->ClientPipeHandle, "client pipe");
    Session->ClientPipeHandle = NULL;

    //
    //  Delete client async objects
    //

    DeleteAsync(ClientReadAsyncHandle);
    DeleteAsync(ClientWriteAsyncHandle);

    //
    //  Terminate shell process, close shell pipe handles, and free session
    //  structure
    //

    DeleteSession (Session);


    //
    // Return the appropriate exit code
    //

    ExitThread(ExitCode);

    assert(FALSE);
    return(ExitCode); // keep compiler happy
}
