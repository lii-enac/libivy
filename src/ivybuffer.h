/*
 *	Ivy, C interface
 *
 *	Copyright 1997-2000
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Sockets
 *
 *	Authors: Francois-Regis Colin <fcolin@cena.dgac.fr>
 *
 *	$Id: ivybuffer.h 3243 2008-03-21 09:03:34Z bustico $
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYBUFFER_H
#define IVYBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
typedef struct {
	char * data; 
	int size;
	int offset;
	} IvyBuffer;


/* utility fonction do make vsprintf without buffer limit */
extern int make_message(IvyBuffer * buffer, const char *fmt, va_list ap);
extern int make_message_var(IvyBuffer* buffer, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif

