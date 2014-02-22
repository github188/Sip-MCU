/* Copyright (C) 2002  Andy Pennell
   22.3.2007 - minor modifications by Jaroslav Libak

   You can use code snippets and source code downloads in your applications as long as

   * You keep all copyright notices in the code intact.
   * You do not sell or republish the code or it's associated article without the author's written agreement
   * You agree the code is provided as-is without any implied or express warranty. 
*/

#ifndef mdump_h__
#define mdump_h__

#if defined WIN32

#ifndef __out_xcount
#define __out_xcount(x)
#endif

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#if _MSC_VER < 1300
#define DECLSPEC_DEPRECATED
// VC6: change this path to your Platform SDK headers
#include "M:\\dev7\\vs\\devtools\\common\\win32sdk\\include\\dbghelp.h"			// must be XP version of file
#else
// VC7: ships with updated headers
#include "dbghelp.h"
#endif

// DEFINES
// MACROS
#if defined UNICODE
#define TSTRDUP(x) _wcsdup(x)
#define TSTRCPY(a, b) wcscpy(a, b)
#define TSTRCAT(a, b) wcscat(a, b)
#define TSTRRCHR(a, b) wcsrchr(a, b)
#define TPRINTF swprintf_s
#else
#define TSTRDUP(x) _mbsdup(x)
#define TSTRCPY(a, b) _mbscpy(a, b)
#define TSTRCAT(a, b) _mbscat(a, b)
#define TSTRRCHR(a, b) _mbsrchr(a, b)
#define TPRINTF sprintf_s
#endif

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// FORWARD DECLARATIONS
// STRUCTS
// TYPEDEFS

// based on dbghelp.h
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
									CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
									CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
									CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
									);

/**
 * Class that can write minidumps during crash.
 */
class MiniDumper
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
public:
/* ============================ CREATORS ================================== */
///@name Creators
//@{
   /**
    * Constructor.
    */
   MiniDumper( LPCTSTR szAppName );

   /**
    * Destructor.
    */
   ~MiniDumper();
//@}

/* ============================ MANIPULATORS ============================== */
///@name Manipulators
//@{
//@}
/* ============================ ACCESSORS ================================= */
///@name Accessors
//@{
//@}
/* ============================ INQUIRY =================================== */
///@name Inquiry
//@{
//@}

/* //////////////////////////// PROTECTED ///////////////////////////////// */
protected:

/* //////////////////////////// PRIVATE /////////////////////////////////// */
private:
	static LPCTSTR m_szAppName;   ///< application name

   /**
    * Exception handler, writes the minidump.
    *
    * @param pExceptionInfo Information about exception.
    */
	static LONG WINAPI TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo);

};

#endif // WIN32

#endif // mdump_h__
