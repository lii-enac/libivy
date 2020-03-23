/*
 *	Ivy, C interface
 *
 *	Copyright 1997-2000
 *	Centre d'Etudes de la Navigation Aerienne
 *
 *	Sockets
 *
 *	Authors: Francois-Regis Colin <fcolin@cena.fr>
 *
 *	$Id: ivybuffer.c 3460 2011-01-24 13:39:15Z bustico $
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifdef WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

#include "param.h"
#include "ivybuffer.h"

/* fonction de formtage a la printf d'un buffer avec reallocation dynamique   */
int make_message(IvyBuffer* buffer, const char *fmt, va_list ap)
{
    /* Guess we need no more than BUFFER_INIT_SIZE bytes. */
    int n;
	va_list ap_copy;
    if ( buffer->size == 0 || buffer->data == NULL )
		{
		buffer->size = IVY_BUFFER_SIZE;
		buffer->offset = 0;
		buffer->data = (char *) malloc (IVY_BUFFER_SIZE);
		if ( buffer->data == NULL )
			{
			perror(" Ivy make message MALLOC error: " );
			return -1;
			}
		}
    while (1) {
    /* Try to print in the allocated space. */
	
#ifdef WIN32
	ap_copy = ap;
	n = _vsnprintf (buffer->data + buffer->offset, buffer->size - buffer->offset, fmt, ap_copy);
#else
	va_copy( ap_copy, ap );
    n = vsnprintf (buffer->data + buffer->offset, buffer->size - buffer->offset, fmt, ap_copy);
#endif
	va_end(ap_copy);
    /* If that worked, return the string size. */
    if (n > -1 && n < (buffer->size - buffer->offset))
	{
		buffer->offset += n;
		return n;
	}
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
        buffer->size = buffer->offset + n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
        buffer->size *= 2;  /* twice the old size */
    if ((buffer->data = (char *) realloc (buffer->data, buffer->size)) == NULL)
		{
       		perror(" Ivy make message REALLOC error: " );
		return -1;
		}
    }
}
int make_message_var(IvyBuffer* buffer, const char *fmt, ... )
{
	va_list ap;
	int len;
	va_start (ap, fmt );
	len = make_message (buffer, fmt, ap );
	va_end (ap );
	return len;
}

