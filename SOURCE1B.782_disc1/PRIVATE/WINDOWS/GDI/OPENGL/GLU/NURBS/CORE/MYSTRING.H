/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * mystring.h - $Revision: 1.1 $
 */

#ifndef __MYSTRING_H__
#define __MYSTRING_H__

#ifdef STANDALONE
typedef unsigned int size_t;
extern "C" void *	GLOS_CCALL memcpy(void *, const void *, size_t);
extern "C" void *	GLOS_CCALL memset(void *, int, size_t);
#endif

#ifdef GLBUILD
#define memcpy(a,b,c)	bcopy(b,a,c)
#define memset(a,b,c)	bzero(a,c)
extern "C" void		bcopy(const void *, void *, int);
extern "C" void		bzero(void *, int);
#endif

#ifdef LIBRARYBUILD
#include <string.h>
#endif
#endif /* __MYSTRING_H__ */
