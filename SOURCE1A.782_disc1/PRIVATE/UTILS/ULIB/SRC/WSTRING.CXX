/*++

Copyright (c) 1992 Microsoft Corporation

Module Name:

    wstring.cxx

Author:

    Norbert P. Kusters (norbertk) 6-Aug-92

--*/

#include <pch.cxx>

#define _NTAPI_ULIB_
#define _ULIB_MEMBER_

#include "ulib.hxx"
#include "wstring.hxx"

extern "C" {
#include <stdio.h>
}

extern "C" {
#include <wchar.h>
}

BOOLEAN WSTRING::_UseAnsiConversions = FALSE;
BOOLEAN WSTRING::_UseConsoleConversions = FALSE;

// Helper functions for OEM/Unicode conversion.  Note that these
// are abstracted to private functions to make it easier to set
// them up for various environments.
//
#if defined( _AUTOCHECK_ )

INLINE
BOOLEAN
WSTRING::ConvertOemToUnicodeN(
    PWSTR UnicodeString,
    ULONG MaxBytesInUnicodeString,
    PULONG BytesInUnicodeString,
    PCHAR OemString,
    ULONG BytesInOemString
    )
{
    return _UseAnsiConversions ?
           NT_SUCCESS(RtlMultiByteToUnicodeN( UnicodeString,
                                   MaxBytesInUnicodeString,
                                   BytesInUnicodeString,
                                   OemString,
                                   BytesInOemString )) :
           NT_SUCCESS(RtlOemToUnicodeN( UnicodeString,
                             MaxBytesInUnicodeString,
                             BytesInUnicodeString,
                             OemString,
                             BytesInOemString ));
}

INLINE
BOOLEAN
WSTRING::ConvertUnicodeToOemN(
    PCHAR OemString,
    ULONG MaxBytesInOemString,
    PULONG BytesInOemString,
    PWSTR UnicodeString,
    ULONG BytesInUnicodeString
    )
{
    return _UseAnsiConversions ?
           NT_SUCCESS(RtlUnicodeToMultiByteN( OemString,
                                   MaxBytesInOemString,
                                   BytesInOemString,
                                   UnicodeString,
                                   BytesInUnicodeString )) :
           NT_SUCCESS(RtlUnicodeToOemN( OemString,
                             MaxBytesInOemString,
                             BytesInOemString,
                             UnicodeString,
                             BytesInUnicodeString ));
}

#else // _AUTOCHECK_ not defined

INLINE
BOOLEAN
ConvertToUnicodeWithConsoleCP(
    PWSTR UnicodeString,
    ULONG MaxBytesInUnicodeString,
    PULONG BytesInUnicodeString,
    PCHAR OemString,
    ULONG BytesInOemString
    )
{
    int result;

    *BytesInUnicodeString =
        MultiByteToWideChar( GetConsoleCP(),
                             MB_PRECOMPOSED,
                             OemString,
                             BytesInOemString,
                             UnicodeString,
                             MaxBytesInUnicodeString/sizeof(WCHAR) ) *
        sizeof(WCHAR);

    return( *BytesInUnicodeString != 0 );
}


INLINE
BOOLEAN
WSTRING::ConvertOemToUnicodeN(
    PWSTR UnicodeString,
    ULONG MaxBytesInUnicodeString,
    PULONG BytesInUnicodeString,
    PCHAR OemString,
    ULONG BytesInOemString
    )
{
    return
       _UseConsoleConversions ?
           ConvertToUnicodeWithConsoleCP( UnicodeString,
                                          MaxBytesInUnicodeString,
                                          BytesInUnicodeString,
                                          OemString,
                                          BytesInOemString ) :
       _UseAnsiConversions ?
           NT_SUCCESS(RtlMultiByteToUnicodeN( UnicodeString,
                                   MaxBytesInUnicodeString,
                                   BytesInUnicodeString,
                                   OemString,
                                   BytesInOemString )) :
           NT_SUCCESS(RtlOemToUnicodeN( UnicodeString,
                             MaxBytesInUnicodeString,
                             BytesInUnicodeString,
                             OemString,
                             BytesInOemString ));
}

INLINE
BOOLEAN
WSTRING::ConvertUnicodeToOemN(
    PCHAR OemString,
    ULONG MaxBytesInOemString,
    PULONG BytesInOemString,
    PWSTR UnicodeString,
    ULONG BytesInUnicodeString
    )
{
    return _UseAnsiConversions ?
           NT_SUCCESS(RtlUnicodeToMultiByteN( OemString,
                                   MaxBytesInOemString,
                                   BytesInOemString,
                                   UnicodeString,
                                   BytesInUnicodeString )) :
           NT_SUCCESS(RtlUnicodeToOemN( OemString,
                             MaxBytesInOemString,
                             BytesInOemString,
                             UnicodeString,
                             BytesInUnicodeString ));
}


#endif 

// Implement wcscoll and wcsicoll for AUTOCHECK,
// since they're not in LIBCNT.LIB

#if defined( _AUTOCHECK_ )

int _CRTAPI1 wcscoll(const wchar_t * left, const wchar_t * right)
{
    return wcscmp(left, right);
}

int _CRTAPI1 _wcsicoll(const wchar_t * left, const wchar_t * right)
{
    return wcsicmp(left, right);
}

#endif // _AUTOCHECK_

INLINE
VOID
WSTRING::Construct(
    )
{
    _s = NULL;
    _l = 0;
}


DEFINE_CONSTRUCTOR( WSTRING, OBJECT );


ULIB_EXPORT
BOOLEAN
WSTRING::Initialize(
    IN  PCWSTRING   InitialString,
    IN  CHNUM       Position,
    IN  CHNUM       Length
    )
/*++

Routine Description:

    This routine initializes the current string by copying the contents
    of the given string.

Arguments:

    InitialString   - Supplies the initial string.
    Position        - Supplies the position in the given string to start at.
    Length          - Supplies the length of the string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    DbgAssert(Position <= InitialString->_l);

    Length = min(Length, InitialString->_l - Position);

    if (!NewBuf(Length)) {
        return FALSE;
    }

    memcpy(_s, InitialString->_s + Position, (UINT) Length*sizeof(WCHAR));

    return TRUE;
}


ULIB_EXPORT
BOOLEAN
WSTRING::Initialize(
    IN  PCWSTR  InitialString,
    IN  CHNUM   StringLength
    )
/*++

Routine Description:

    This routine initializes the current string by copying the contents
    of the given string.

Arguments:

    InitialString   - Supplies the initial string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    if (StringLength == TO_END) {
        StringLength = wcslen(InitialString);
    }

    if (!NewBuf(StringLength)) {
        return FALSE;
    }

    memcpy(_s, InitialString, (UINT) StringLength*sizeof(WCHAR));

    return TRUE;
}


ULIB_EXPORT
BOOLEAN
WSTRING::Initialize(
    IN  PCSTR   InitialString,
    IN  CHNUM   StringLength
    )
/*++

Routine Description:

    This routine initializes the current string by copying the contents
    of the given string.

Arguments:

    InitialString   - Supplies the initial string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    CHNUM       length;
    BOOLEAN     status;

    if (StringLength == TO_END) {
        StringLength = strlen(InitialString);
    }

    if (!StringLength) {
        return Resize(0);
    }


    // We want to avoid making two calls to RtlOemToUnicodeN so
    // try to guess an adequate size for the buffer.

    if (!NewBuf(StringLength)) {
        return FALSE;
    }

    status = ConvertOemToUnicodeN(_s, _l*sizeof(WCHAR),
                                  &length, (PSTR) InitialString,
                                  StringLength);
    length /= sizeof(WCHAR);

    if (status) {
        return Resize(length);
    }


    // We didn't manage to make in one try so ask exactly how much
    // we need and then make the call.

    status = ConvertOemToUnicodeN(NULL, 0, &length, (PSTR) InitialString,
                                  StringLength);
    length /= sizeof(WCHAR);

    if (!status || !NewBuf(length)) {
        return FALSE;
    }

    status = ConvertOemToUnicodeN(_s, _l*sizeof(WCHAR),
                                  &length, (PSTR) InitialString, StringLength);

    if (!status) {
        return FALSE;
    }

    DbgAssert(length == _l*sizeof(WCHAR));

    return TRUE;
}


ULIB_EXPORT
BOOLEAN
WSTRING::Initialize(
    IN  LONG    Number
    )
/*++

Routine Description:

    This routine initializes the current string by copying the contents
    of the given string.

Arguments:

    Number  - Supplies the number to initialize the string to.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    CHAR    tmp[64];

    sprintf(tmp, "%d", Number);
    return Initialize(tmp);
}


NONVIRTUAL
ULIB_EXPORT
PWSTRING
WSTRING::QueryString(
    IN  CHNUM   Position,
    IN  CHNUM   Length
    ) CONST
/*++

Routine Description:

    This routine returns a copy of this string from the specified
    coordinates.

Arguments:

    Position    - Supplies the initialize position of the string.
    Length      - Supplies the length of the string.

Return Value:

    A pointer to a string or NULL.

--*/
{
    PWSTRING    p;

    if (!(p = NEW DSTRING) ||
        !p->Initialize(this, Position, Length)) {

        DELETE(p);
    }

    return p;
}


ULIB_EXPORT
BOOLEAN
WSTRING::QueryNumber(
    OUT PLONG   Number,
    IN  CHNUM   Position,
    IN  CHNUM   Length
    ) CONST
/*++

Routine Description:

    This routine queries a number from the string.

Arguments:

    Number      - Returns the number parsed out of the string.
    Position    - Supplies the position of the number.
    Length      - Supplies the length of the number.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    FSTRING String;
    PSTR    p;
    CHNUM   spn;

    if (Position >= _l) {
        return FALSE;
    }

    Length = min(Length, _l - Position);

	//
    //  Note that 123+123 will be a number!
	//
    String.Initialize((PWSTR) L"1234567890+-");

    spn = Strspn(&String, Position);

    if ((spn == INVALID_CHNUM || spn >= Position + Length) &&
        (p = QuerySTR(Position, Length))) {

        *Number = atol(p);

        DELETE(p);
		return TRUE;
	}

	return FALSE;
}


ULIB_EXPORT
VOID
WSTRING::DeleteChAt(
    IN  CHNUM   Position,
    IN  CHNUM   Length
    )
/*++

Routine Description:

    This routine removes the character at the given position.

Arguments:

    Position    - Supplies the position of the character to remove.
    Length      - Supplies the number of characters to remove.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    DbgAssert(Position <= _l);

    Length = min(Length, _l - Position);

    memmove(_s + Position, _s + Position + Length,
            (UINT) (_l - Position - Length)*sizeof(WCHAR));

    Resize(_l - Length);
}


NONVIRTUAL
BOOLEAN
WSTRING::InsertString(
    IN  CHNUM       AtPosition,
    IN  PCWSTRING   String,
    IN  CHNUM       FromPosition,
    IN  CHNUM       FromLength
    )
/*++

Routine Description:

    This routine inserts the given string at the given position in
    this string.

Arguments:

    AtPosition  - Supplies the position at which to insert the string.
    String      - Supplies the string to insert.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    CHNUM   old_length;

    DbgAssert(AtPosition <= _l);
    DbgAssert(FromPosition <= String->_l);

    FromLength = min(FromLength, String->_l - FromPosition);

    old_length = _l;
    if (!Resize(_l + FromLength)) {
        return FALSE;
    }

    memmove(_s + AtPosition + FromLength, _s + AtPosition,
            (UINT) (old_length - AtPosition)*sizeof(WCHAR));

    memcpy(_s + AtPosition, String->_s + FromPosition,
           (UINT) FromLength*sizeof(WCHAR));

    return TRUE;
}


NONVIRTUAL
ULIB_EXPORT
BOOLEAN
WSTRING::Replace(
    IN CHNUM        AtPosition,
    IN CHNUM        AtLength,
    IN PCWSTRING    String,
    IN CHNUM        FromPosition,
    IN CHNUM        FromLength
    )
/*++

Routine Description:

    This routine replaces the contents of this string from
    'Position' to 'Length' with the contents of 'String2'
    from 'Position2' to 'Length2'.

Arguments:

    AtPosition      - Supplies the position to replace at.
    AtLength        - Supplies the length to replace at.
    String          - Supplies the string to replace with.
    FromPosition    - Supplies the position to replace from in String2.
    FromLength      - Supplies the position to replace from in String2.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    CHNUM   old_length;

    DbgAssert(AtPosition <= _l);
    DbgAssert(FromPosition <= String->_l);

    AtLength = min(AtLength, _l - AtPosition);
    FromLength = min(FromLength, String->_l - FromPosition);

    // Make sure up front that we have the room but don't disturb
    // the string.

    if (FromLength > AtLength) {
        old_length = _l;
        if (!Resize(_l + FromLength - AtLength)) {
            return FALSE;
        }
        Resize(old_length);
    }

    DeleteChAt(AtPosition, AtLength);
    if (!InsertString(AtPosition, String, FromPosition, FromLength)) {
        DbgAbort("This absolutely can never happen\n");
        return FALSE;
    }

    return TRUE;
}


ULIB_EXPORT
PWSTR
WSTRING::QueryWSTR(
    IN  CHNUM   Position,
    IN  CHNUM   Length,
    OUT PWSTR   Buffer,
    IN  CHNUM   BufferLength,
    IN  BOOLEAN ForceNull
    ) CONST
/*++

Routine Description:

    This routine makes a copy of this string into the provided
    buffer.  If this string is not provided then a buffer is
    allocated on the heap.

Arguments:

    Position        - Supplies the position within this string.
    Length          - Supplies the length of this string to take.
    Buffer          - Supplies the buffer to copy to.
    BufferLength    - Supplies the number of characters in the buffer.
    ForceNull       - Specifies whether or not to force the final character
                        of the buffer to be NULL in the case when there
                        isn't enough room for the whole string including
                        the NULL.

Return Value:

    A pointer to a NULL terminated string.

--*/
{
    DbgAssert(Position <= _l);

    Length = min(Length, _l - Position);

    if (!Buffer) {
        BufferLength = Length + 1;
        if (!(Buffer = (PWCHAR) MALLOC(BufferLength*sizeof(WCHAR)))) {
            return NULL;
        }
    }

    if (BufferLength > Length) {
        memcpy(Buffer, _s + Position, (UINT) Length*sizeof(WCHAR));
        Buffer[Length] = 0;
    } else {
        memcpy(Buffer, _s + Position, (UINT) BufferLength*sizeof(WCHAR));
        if (ForceNull) {
            Buffer[BufferLength - 1] = 0;
        }
    }

    return Buffer;
}


ULIB_EXPORT
PSTR
WSTRING::QuerySTR(
    IN  CHNUM   Position,
    IN  CHNUM   Length,
    OUT PSTR    Buffer,
    IN  CHNUM   BufferLength,
    IN  BOOLEAN ForceNull
    ) CONST
/*++

Routine Description:

    This routine computes a multi-byte version of the current
    unicode string.  If the buffer is not supplied then it
    will be allocated by this routine.

Arguments:

    Position        - Supplies the position within this string.
    Length          - Supplies the length of this string to take.
    Buffer          - Supplies the buffer to convert into.
    BufferLength    - Supplies the number of characters in this buffer.
    ForceNull       - Specifies whether or not to force a NULL even
                        when the buffer is too small for the string.

Return Value:

    A pointer to a NULL terminated multi byte string.

--*/
{
    ULONG       ansi_length;

    DbgAssert(Position <= _l);

    Length = min(Length, _l - Position);


    // First special case the empty result.

    if (!Length) {

        if (!Buffer) {
            if (!(Buffer = (PSTR) MALLOC(1))) {
                return NULL;
            }
        } else if (!BufferLength) {
            return NULL;
        }

        Buffer[0] = 0;
        return Buffer;
    }


    // Next case is that the buffer is not provided and thus
    // we have to figure out what size it should be.

    if (!Buffer) {

        // We want to avoid too many calls to RtlUnicodeToOemN
        // so we'll estimate a correct size for the buffer and
        // hope that that works.

        BufferLength = 2*Length + 1;
        if (!(Buffer = (PSTR) MALLOC(BufferLength))) {
            return NULL;
        }

        if (ConvertUnicodeToOemN(Buffer, BufferLength - 1,
                                 &ansi_length, _s + Position,
                                 Length*sizeof(WCHAR))) {
            Buffer[ansi_length] = 0;
            return Buffer;
        }


        // We failed to estimate the necessary size of the buffer.
        // So ask the correct size and try again.

        DELETE(Buffer);

        if (!ConvertUnicodeToOemN(NULL, 0, &ansi_length,
                                  _s + Position, Length*sizeof(WCHAR))) {
            return NULL;
        }

        BufferLength = ansi_length + 1;
        if (!(Buffer = (PSTR) MALLOC(BufferLength))) {
            return NULL;
        }
    }

    if (!ConvertUnicodeToOemN(Buffer, BufferLength, &ansi_length,
                              _s + Position, Length*sizeof(WCHAR))) {
        return NULL;
    }

    if (BufferLength > ansi_length) {
        Buffer[ansi_length] = 0;
    } else {
        if (ForceNull) {
            Buffer[BufferLength - 1] = 0;
        }
    }

    return Buffer;
}


ULIB_EXPORT
BOOLEAN
WSTRING::Strcat(
    IN  PCWSTRING   String
    )
/*++

Routine Description:

    This routine concatenates the given string onto this one.

Arguments:

    String  - Supplies the string to concatenate to this one.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    CHNUM   old_length;

    old_length = _l;
    if (!Resize(_l + String->_l)) {
        return FALSE;
    }

    memcpy(_s + old_length, String->_s, (UINT) String->_l*sizeof(WCHAR));

    return TRUE;
}


NONVIRTUAL
PWSTRING
WSTRING::Strupr(
    IN  CHNUM   StartPosition,
    IN  CHNUM   Length
    )
/*++

Routine Description:

    This routine upcases a portion of this string.

Arguments:

    StartPosition   - Supplies the start position of the substring to upcase.
    Length          - Supplies the length of the substring to upscase.

Return Value:

    A pointer to this string.

--*/
{
    WCHAR   c;

    DbgAssert(StartPosition <= _l);

    Length = min(Length, _l - StartPosition);

    c = _s[StartPosition + Length];
    _s[StartPosition + Length] = 0;

    wcsupr(_s + StartPosition);

    _s[StartPosition + Length] = c;

    return this;
}


NONVIRTUAL
PWSTRING
WSTRING::Strlwr(
    IN  CHNUM   StartPosition,
    IN  CHNUM   Length
    )
/*++

Routine Description:

    This routine lowercases a portion of this string.

Arguments:

    StartPosition   - Supplies the start position of the substring to lowercase.
    Length          - Supplies the length of the substring to lowercase.

Return Value:

    A pointer to this string.

--*/
{
    WCHAR   c;

    DbgAssert(StartPosition <= _l);

    Length = min(Length, _l - StartPosition);

    c = _s[StartPosition + Length];
    _s[StartPosition + Length] = 0;

    wcslwr(_s + StartPosition);

    _s[StartPosition + Length] = c;

    return this;
}


NONVIRTUAL
LONG
WSTRING::Strcmp(
    IN  PCWSTRING   String,
    IN  CHNUM       LeftPosition,
    IN  CHNUM       LeftLength,
    IN  CHNUM       RightPosition,
    IN  CHNUM       RightLength
    ) CONST
/*++

Routine Description:

    This routine compares two substrings.

Arguments:

    String          - Supplies the string to compare this one to.
    LeftPosition    - Supplies the postion for the left substring.
    LeftLength      - Supplies the length of the left substring.
    LeftPosition    - Supplies the postion for the left substring.
    LeftLength      - Supplies the length of the left substring.

Return Value:

    <0  - Left substring is less than right substring.
    0   - Left and Right substrings are equal
    >0  - Left substring is greater than right substring.

--*/
{
    WCHAR   c, d;
    LONG    r;

    DbgAssert(LeftPosition <= _l);
    DbgAssert(RightPosition <= String->_l);

    LeftLength = min(LeftLength, _l - LeftPosition);
    RightLength = min(RightLength, String->_l - RightPosition);

    c = _s[LeftPosition + LeftLength];
    d = String->_s[RightPosition + RightLength];
    _s[LeftPosition + LeftLength] = 0;
    String->_s[RightPosition + RightLength] = 0;

    r = wcscoll(_s + LeftPosition, String->_s + RightPosition);

    _s[LeftPosition + LeftLength] = c;
    String->_s[RightPosition + RightLength] = d;

    return r;
}


NONVIRTUAL
ULIB_EXPORT
LONG
WSTRING::Stricmp(
    IN  PCWSTRING   String,
    IN  CHNUM       LeftPosition,
    IN  CHNUM       LeftLength,
    IN  CHNUM       RightPosition,
    IN  CHNUM       RightLength
    ) CONST
/*++

Routine Description:

    This routine compares two substrings insensitive of case.

Arguments:

    String          - Supplies the string to compare this one to.
    LeftPosition    - Supplies the postion for the left substring.
    LeftLength      - Supplies the length of the left substring.
    LeftPosition    - Supplies the postion for the left substring.
    LeftLength      - Supplies the length of the left substring.

Return Value:

    <0  - Left substring is less than right substring.
    0   - Left and Right substrings are equal
    >0  - Left substring is greater than right substring.

--*/
{
    WCHAR   c, d;
    LONG    r;

    DbgAssert(LeftPosition <= _l);
    DbgAssert(RightPosition <= String->_l);

    LeftLength = min(LeftLength, _l - LeftPosition);
    RightLength = min(RightLength, String->_l - RightPosition);

    c = _s[LeftPosition + LeftLength];
    d = String->_s[RightPosition + RightLength];
    _s[LeftPosition + LeftLength] = 0;
    String->_s[RightPosition + RightLength] = 0;

    r = wcsicoll(_s + LeftPosition, String->_s + RightPosition);

    _s[LeftPosition + LeftLength] = c;
    String->_s[RightPosition + RightLength] = d;

    return r;
}


PWSTR
WSTRING::SkipWhite(
    IN  PWSTR    p
    )
{

#ifdef DBCS

  while (*p) {

    if (iswspace(*p))
      p++;
    else if (*p == DB_SP_HI && *(p+1) == DB_SP_LO)
    {
      *p++ = ' ';
      *p++ = ' ';
    }
    else
      break;
  }

#else
    while (iswspace(*p)) {
        p++;
    }
#endif

  return p;

}


/**************************************************************************/
/* Compare two strings, ignoring white space, case is significant, return */
/* 0 if identical, <>0 otherwise.  Leading and trailing white space is    */
/* ignored, internal white space is treated as single characters.         */
/**************************************************************************/
ULIB_EXPORT
INT
WSTRING::Strcmps (
    IN  PWSTR    p1,
    IN  PWSTR    p2
    )
{
  WCHAR *q;

  p1 = WSTRING::SkipWhite(p1);                /* skip any leading white space */
  p2 = WSTRING::SkipWhite(p2);

  while (TRUE)
  {
    if (*p1 == *p2)
    {
      if (*p1++ == 0)             /* quit if at the end */
        return (0);
      else
        p2++;

#ifdef DBCS
      if (CheckSpace(p1))
#else
      if (iswspace(*p1))           /* compress multiple spaces */
#endif
      {
        q = WSTRING::SkipWhite(p1);
        p1 = (*q == 0) ? q : q - 1;
      }

#ifdef DBCS
      if (CheckSpace(p2))
#else
      if (iswspace(*p2))
#endif
      {
        q = WSTRING::SkipWhite(p2);
        p2 = (*q == 0) ? q : q - 1;
      }
    }
    else
      return *p1-*p2;
  }
}





/**************************************************************************/
/* Compare two strings, ignoring white space, case is not significant,    */
/* return 0 if identical, <>0 otherwise.  Leading and trailing white      */
/* space is ignored, internal white space is treated as single characters.*/
/**************************************************************************/
ULIB_EXPORT
INT
WSTRING::Strcmpis (
    IN  PWSTR    p1,
    IN  PWSTR    p2
    )
{
  WCHAR *q;
#ifdef DBCS
  WCHAR c1,c2;
#endif

  p1 = WSTRING::SkipWhite(p1);                  /* skip any leading white space */
  p2 = WSTRING::SkipWhite(p2);

  while (TRUE)
  {
#ifdef DBCS
    c1 = *p1;
    c2 = *p2;

    if (IsDBCSLeadBYTE(c1) && IsDBCSLeadBYTE(c2) && c1 == c2)
    {
      c1 = *++p1;
      c2 = *++p2;
    }
    else if (IsDBCSLeadBYTE(c1) || IsDBCSLeadBYTE(c2))
      return (c1 - c2);
    else
    {
      c1 = towupper(c1);
      c2 = towupper(c2);
    }
    if (c1 == c2)
#else
      if (towupper(*p1) == towupper(*p2))
#endif
      {
        if (*p1++ == 0)                /* quit if at the end */
          return (0);
        else
          p2++;
#ifdef DBCS
        if (CheckSpace(p1))
#else
        if (iswspace(*p1))              /* compress multiple spaces */
#endif
        {
          q = SkipWhite(p1);
          p1 = (*q == 0) ? q : q - 1;
        }
#ifdef DBCS
        if (CheckSpace(p2))
#else
        if (iswspace(*p2))
#endif
        {
          q = WSTRING::SkipWhite(p2);
          p2 = (*q == 0) ? q : q - 1;
        }
      }
      else
        return *p1-*p2;
  }
}

#ifdef DBCS

/**************************************************************************/
/* Routine:  CheckSpace                                                   */
/* Arguments: an arbitrary string                                         */
/* Function: Determine whether there is a space in the string.            */
/* Side effects: none                                                     */
/**************************************************************************/
INT
WSTRING::CheckSpace(
    IN  PWSTR    s
    )
{
  if (iswspace(*s) || (*s == DB_SP_HI && *(s+1) == DB_SP_LO))
    return (TRUE);
  else
    return (FALSE);
}

#endif


DEFINE_EXPORTED_CONSTRUCTOR( FSTRING, WSTRING, ULIB_EXPORT );


ULIB_EXPORT
BOOLEAN
FSTRING::Resize(
    IN  CHNUM   NewStringLength
    )
/*++

Routine Description:

    This routine implements the WSTRING Resize routine by using
    the buffer supplied at initialization time.

Arguments:

    NewStringLength - Supplies the new length of the string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    return NewBuf(NewStringLength);
}


BOOLEAN
FSTRING::NewBuf(
    IN  CHNUM   NewStringLength
    )
/*++

Routine Description:

    This routine implements the WSTRING NewBuf routine by using
    the buffer supplied at initialization time.

Arguments:

    NewStringLength - Supplies the new length of the string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    if (NewStringLength >= _buffer_length) {
        return FALSE;
    }

    PutString((PWSTR) GetWSTR(), NewStringLength);

    return TRUE;
}


INLINE
VOID
DSTRING::Construct(
    )
/*++

Routine Description:

    This routine initializes the string to a valid initial state.

Arguments:

    None.

Return Value:

    None.

--*/
{
    _buf = NULL;
    _length = 0;
}


DEFINE_EXPORTED_CONSTRUCTOR( DSTRING, WSTRING, ULIB_EXPORT );


ULIB_EXPORT
DSTRING::~DSTRING(
    )
/*++

Routine Description:

    Destructor for DSTRING.

Arguments:

    None.

Return Value:

    None.

--*/
{
    DELETE(_buf);
}


BOOLEAN
DSTRING::Resize(
    IN  CHNUM   NewStringLength
    )
/*++

Routine Description:

    This routine resizes this string to the specified new size.

Arguments:

    NewStringLength - Supplies the new length of the string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    PWSTR   new_buf;

    if (NewStringLength >= _length) {

        if (_buf) {
            if (!(new_buf = (PWSTR)
                  REALLOC(_buf, (NewStringLength + 1)*sizeof(WCHAR)))) {

                return FALSE;
            }
        } else {
            if (!(new_buf = (PWSTR)
                  MALLOC((NewStringLength + 1)*sizeof(WCHAR)))) {

                return FALSE;
            }
        }

        _buf = new_buf;
        _length = NewStringLength + 1;
    }

    PutString(_buf, NewStringLength);

    return TRUE;
}


BOOLEAN
DSTRING::NewBuf(
    IN  CHNUM   NewStringLength
    )
/*++

Routine Description:

    This routine resizes this string to the specified new size.

Arguments:

    NewStringLength - Supplies the new length of the string.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    PWSTR   new_buf;

    if (NewStringLength >= _length) {

        if (!(new_buf = (PWSTR)
              MALLOC((NewStringLength + 1)*sizeof(WCHAR)))) {

            return FALSE;
        }

        _buf = new_buf;
        _length = NewStringLength + 1;
    }

    PutString(_buf, NewStringLength);

    return TRUE;
}


ULIB_EXPORT
VOID
WSTRING::SetAnsiConversions(
    )
/*++

Routine Description:

    This routine declares that all conversions from multi byte
    to unicode will take place using the ANSI code page.  Note
    that this is a STATIC method.  Therefore this switch affects
    *all* WSTRINGs.

Arguments:

    None.

Return Value:

    None.

--*/
{
    _UseAnsiConversions = TRUE;
    _UseConsoleConversions = FALSE;
}


ULIB_EXPORT
VOID
WSTRING::SetOemConversions(
    )
/*++

Routine Description:

    This routine declares that all conversions from multi byte
    to unicode will take place using the OEM code page.  Note
    that this is a STATIC method.  Therefore this switch affects
    *all* WSTRINGs.

    This is the default if neither this nor the above function is
    called.

Arguments:

    None.

Return Value:

    None.

--*/
{
    _UseAnsiConversions = FALSE;
    _UseConsoleConversions = FALSE;
}

ULIB_EXPORT
VOID
WSTRING::SetConsoleConversions(
    )
/*++

Routine Description:

    This routine declares that all conversions from multi byte
    to unicode will take place using the current console code page.
    Note that this is a STATIC method.  Therefore this switch
    affects *all* WSTRINGs.

Arguments:

    None.

Return Value:

    None.

--*/
{
    _UseAnsiConversions = FALSE;
    _UseConsoleConversions = TRUE;
}
