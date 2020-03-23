/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Basic I/O handling
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.dgac.fr>
 *
 *	$Id: ivychannel.h 3460 2011-01-24 13:39:15Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 *
 */
#ifndef _IVYCHANNEL_H
#define _IVYCHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif
	
/* general Handle */

#ifdef WIN32
#include <windows.h>
#define IVY_HANDLE SOCKET
#else
#define IVY_HANDLE int
#endif

typedef struct _channel *Channel;
/* callback declenche par la gestion de boucle  sur evenement exception sur le canal */
typedef void (*ChannelHandleDelete)( void *data );
/* callback declenche par la gestion de boucle sur donnees pretes sur le canal */
typedef void (*ChannelHandleRead)( Channel channel, IVY_HANDLE fd, void *data);
typedef void (*ChannelHandleWrite)( Channel channel, IVY_HANDLE fd, void *data);

/* fonction appele par le bus pour initialisation */
extern void IvyChannelInit(void);

extern void IvyChannelStop (void);

/* fonction appele par le bus pour mise en place des callback sur le canal */
extern Channel IvyChannelAdd(
	IVY_HANDLE fd,
	void *data,
	ChannelHandleDelete handle_delete,
	ChannelHandleRead handle_read,
	ChannelHandleWrite handle_write
);

/* fonction appele par le bus pour suppression des callback sur le canal */
extern void IvyChannelRemove( Channel channel );


#ifdef __cplusplus
}
#endif

#endif

