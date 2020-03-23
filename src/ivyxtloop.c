/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop based on the X Toolkit
 *
 *	Authors: François-Régis Colin <fcolin@cena.dgac.fr>
 *		 Stéphane Chatty <chatty@cena.dgac.fr>
 *
 *	$Id: ivyxtloop.c 3460 2011-01-24 13:39:15Z bustico $
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
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif


#include <X11/Intrinsic.h>

#include "ivydebug.h"
#include "ivychannel.h"
#include "ivyxtloop.h"

struct _channel {
	XtInputId id_read;
	XtInputId id_write;
	XtInputId id_delete;

	IVY_HANDLE fd;
	void *data;
	ChannelHandleDelete handle_delete;
	ChannelHandleRead handle_read;
	ChannelHandleWrite handle_write;
	};


static int channel_initialized = 0;


static XtAppContext    app = NULL;


void IvyChannelInit(void)
{

	if ( channel_initialized ) return;

	/* pour eviter les plantages quand les autres applis font core-dump */
#ifndef WIN32
	signal( SIGPIPE, SIG_IGN);
#endif
	/* verifie si init correct */
	if ( !app )
	{
		fprintf( stderr, "You must call IvyXtChannelAppContext before XtMainLoop. Exiting.\n");
		exit(-1);
	}
	channel_initialized = 1;
}

void IvyChannelRemove( Channel channel )
{

	if ( channel->handle_delete )
		(*channel->handle_delete)( channel->data );
	XtRemoveInput( channel->id_read );
	XtRemoveInput( channel->id_delete );
}


static void IvyXtHandleChannelRead( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
	TRACE("Handle Channel read %d\n",*source );
	(*channel->handle_read)(channel,*source,channel->data);
}

static void IvyXtHandleChannelWrite( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
	TRACE("Handle Channel write %d\n",*source );
	(*channel->handle_write)(channel,*source,channel->data);
}

static void IvyXtHandleChannelDelete( XtPointer closure, int* source, XtInputId* id )
{
	Channel channel = (Channel)closure;
	TRACE("Handle Channel delete %d\n",*source );
	(*channel->handle_delete)(channel->data);
}


void IvyXtChannelAppContext( XtAppContext cntx )
{
	app = cntx;
}

Channel IvyChannelAdd(IVY_HANDLE fd, void *data,
				ChannelHandleDelete handle_delete,
				ChannelHandleRead handle_read,
		                ChannelHandleWrite handle_write
				)						
{
	Channel channel;

	channel = XtNew( struct _channel );
	if ( !channel )
		{
		fprintf(stderr,"NOK Memory Alloc Error\n");
		exit(0);
		}

	channel->handle_delete = handle_delete;
	channel->handle_read = handle_read;
	channel->handle_write = handle_write;
	channel->data = data;
	channel->fd = fd;

	channel->id_read = XtAppAddInput( app, fd, (XtPointer)XtInputReadMask, IvyXtHandleChannelRead, channel);
	channel->id_delete = XtAppAddInput( app, fd, (XtPointer)XtInputExceptMask, IvyXtHandleChannelDelete, channel);

	return channel;
}


void IvyChannelAddWritableEvent(Channel channel)
{
  channel->id_write = XtAppAddInput( app,  channel->fd, (XtPointer)XtInputWriteMask, IvyXtHandleChannelWrite, channel);
}

void IvyChannelClearWritableEvent(Channel channel)
{
  XtRemoveInput( channel->id_write );
}


void
IvyChannelStop ()
{
  /* To be implemented */
}

