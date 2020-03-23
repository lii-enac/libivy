/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main loop based on the Gtk Toolkit
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id: ivyglibloop.c 3460 2011-01-24 13:39:15Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifdef WIN32
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#else
#include <signal.h>
#endif

#include <glib.h>
#include "ivydebug.h"
#include "ivychannel.h"
#include "ivyglibloop.h"

struct _channel {
	guint id_read;
	guint id_write;
	guint id_delete;
	gpointer data;
	ChannelHandleDelete handle_delete;
	ChannelHandleRead handle_read;
	ChannelHandleWrite handle_write;
  GIOChannel*        io_channel;
	};

static int channel_initialized = 0;


static gboolean IvyGlibHandleChannelRead(GIOChannel *source,
					 GIOCondition condition,
					 gpointer data);

static gboolean IvyGlibHandleChannelDelete(GIOChannel *source,
					 GIOCondition condition,
					   gpointer data);

static gboolean IvyGlibHandleChannelWrite(GIOChannel *source,
					  GIOCondition condition,
					  gpointer data);

void IvyChannelInit(void) {
  if ( channel_initialized ) return;
  /* fixes bug when another app coredumps */
#ifndef WIN32
  signal( SIGPIPE, SIG_IGN);
#endif
  channel_initialized = 1;
}



Channel IvyChannelAdd(IVY_HANDLE fd, void *data,
		      ChannelHandleDelete handle_delete,
		      ChannelHandleRead handle_read,
		      ChannelHandleWrite handle_write
		      ) {
  Channel channel;
  channel = (Channel)g_new(struct _channel, 1);

  channel->handle_delete = handle_delete;
  channel->handle_read = handle_read;
  channel->handle_write = handle_write;
  channel->data = data;

  {
    GIOChannel* io_channel = g_io_channel_unix_new(fd);
    channel->io_channel = io_channel;
    channel->id_read = g_io_add_watch( io_channel, G_IO_IN, 
				       IvyGlibHandleChannelRead, channel);
    channel->id_delete = g_io_add_watch( io_channel, (GIOCondition) (G_IO_ERR | G_IO_HUP), 
					 IvyGlibHandleChannelDelete, channel);
  }
  return channel;
}

void IvyChannelAddWritableEvent(Channel channel)
{
  channel->id_write = g_io_add_watch( channel->io_channel, G_IO_OUT, 
				      IvyGlibHandleChannelWrite, channel);
}

void IvyChannelClearWritableEvent(Channel channel)
{
  g_source_remove( channel->id_write );
}



void IvyChannelRemove( Channel channel ) {
  if ( channel->handle_delete )
    (*channel->handle_delete)( channel->data );
  g_source_remove( channel->id_read );
  g_source_remove( channel->id_delete );
}
 

static gboolean IvyGlibHandleChannelRead(GIOChannel *source,
					 GIOCondition condition,
					 gpointer data) {
  Channel channel = (Channel)data;
  TRACE("Handle Channel read %p\n",source );
  (*channel->handle_read)(channel, g_io_channel_unix_get_fd(source), channel->data);
  return TRUE;
}

static gboolean IvyGlibHandleChannelWrite(GIOChannel *source,
					  GIOCondition condition,
					  gpointer data) {
  Channel channel = (Channel)data;
  TRACE("Handle Channel read %p\n",source );
  (*channel->handle_write)(channel, g_io_channel_unix_get_fd(source), channel->data);
  return TRUE;
}

static gboolean IvyGlibHandleChannelDelete(GIOChannel *source,
					 GIOCondition condition,
					 gpointer data) {
  Channel channel = (Channel)data;
  TRACE("Handle Channel delete %p\n",source );
  (*channel->handle_delete)(channel->data);
  return TRUE;
}


void
IvyChannelStop ()
{
  /* To be implemented */
}

