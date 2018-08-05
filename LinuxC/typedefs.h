//
// typedefs.h
//

#if defined(TYPEDEFS_H)
#else
#define TYPEDEFS_H

//
//

#define UNUSED_VARIABLE(x) (void)(x)
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define IN     /* input parameter */
#define OUT    /* output parameter */
#define INOUT  /* input/output parameter */
#define IN_OPT     /* input parameter, optional */
#define OUT_OPT    /* output parameter, optional */
#define INOUT_OPT  /* input/output parameter, optional */

#define TRUE (1)
#define FALSE (0)
#ifndef true
#define true (1)
#endif
#ifndef false
#define FALSE (0)
#endif
#define ON (1)
#define OFF (0)
#define YES (1)
#define NO (0)
#define ENABLE (1)
#define DISABLE (0)

//typedef void VOID;
//typedef int bool;
typedef int BOOL;
typedef char CHAR;
//typedef unsigned char byte;
//typedef unsigned char BYTE;
//typedef unsigned short ushort;
//typedef unsigned short USHORT;
//typedef unsigned short WORD;
typedef int INT;
//typedef unsigned int uint;
typedef unsigned int UINT;
//typedef unsigned int DWORD;
//typedef long LONG;
//typedef unsigned long ulong;
//typedef unsigned long ULONG;
//typedef float FLOAT;
//typedef double DOUBLE;

#endif
