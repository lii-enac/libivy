/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Main loop based on select
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.dgac.fr>
 *		 St�phane Chatty <chatty@cena.dgac.fr>
 *
 *	$Id: ivyloop.c 3460 2011-01-24 13:39:15Z bustico $
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

#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif

#include "list.h"
#include "ivychannel.h"
#include "ivyloop.h"
#include "timer.h"

struct _channel {
  Channel next;
  IVY_HANDLE fd;
  void *data;
  int tobedeleted;
  ChannelHandleDelete handle_delete;
  ChannelHandleRead handle_read;
  ChannelHandleWrite handle_write;
};

static Channel channels_list = NULL;

static int channel_initialized = 0;

static fd_set open_fds;
static fd_set wrdy_fds;
static IVY_HANDLE highestFd=0;

static int MainLoop = 1;

/* Hook callback & data */
static IvyHookPtr BeforeSelect = NULL;
static IvyHookPtr AfterSelect = NULL;

static void *BeforeSelectData = NULL;
static void *AfterSelectData = NULL;

#ifdef WIN32
WSADATA WsaData;
#endif

void
IvyChannelRemove (Channel channel)
{
  channel->tobedeleted = 1;
}

static void
IvyChannelDelete (Channel channel)
{
  if (channel->handle_delete)
    (*channel->handle_delete) (channel->data);

  FD_CLR (channel->fd, &open_fds);
  FD_CLR (channel->fd, &wrdy_fds);
  IVY_LIST_REMOVE (channels_list, channel);
}

static void
ChannelDefferedDelete ()
{
  Channel channel, next;
  IVY_LIST_EACH_SAFE (channels_list, channel,next)	{
    if (channel->tobedeleted ) {
      IvyChannelDelete (channel);
    }
  }
}

Channel IvyChannelAdd (IVY_HANDLE fd, void *data, 
		       ChannelHandleDelete handle_delete,
		       ChannelHandleRead handle_read,
		       ChannelHandleWrite handle_write
		       )						
{
  Channel channel;


  IVY_LIST_ADD_START (channels_list, channel)
    channel->fd = fd;
  channel->tobedeleted = 0;
  channel->handle_delete = handle_delete;
  channel->handle_read = handle_read;
  channel->handle_write = handle_write;
  channel->data = data;
  if (channel->fd >= highestFd)  
    highestFd = channel->fd+1 ;

  IVY_LIST_ADD_END (channels_list, channel)
    
    FD_SET (channel->fd, &open_fds);

  return channel;
}

void IvyChannelAddWritableEvent(Channel channel)
{
  if (channel->fd >= highestFd)  
    highestFd = channel->fd+1 ;

  FD_SET (channel->fd, &wrdy_fds);
}

void IvyChannelClearWritableEvent(Channel channel)
{
  FD_CLR (channel->fd, &wrdy_fds);
}

static void
IvyChannelHandleWrite (fd_set *current)
{
  Channel channel, next;
	
  IVY_LIST_EACH_SAFE (channels_list, channel, next) {
    if (FD_ISSET (channel->fd, current)) {
      (*channel->handle_write)(channel,channel->fd,channel->data);
    }
  }
}

static void
IvyChannelHandleRead (fd_set *current)
{
  Channel channel, next;
	
  IVY_LIST_EACH_SAFE (channels_list, channel, next) {
    if (FD_ISSET (channel->fd, current)) {
      (*channel->handle_read)(channel,channel->fd,channel->data);
    }
  }
}

static void
IvyChannelHandleExcpt (fd_set *current)
{
  Channel channel,next;
  IVY_LIST_EACH_SAFE (channels_list, channel, next) {
    if (FD_ISSET (channel->fd, current)) {
      if (channel->handle_delete)
	(*channel->handle_delete)(channel->data);
      /*			IvyChannelClose (channel); */
    }
  }
}

void IvyChannelInit (void)
{
#ifdef WIN32
  int error;
#else 
  /* pour eviter les plantages quand les autres applis font core-dump */
  signal (SIGPIPE, SIG_IGN);
#endif
  MainLoop = 1;
  if (channel_initialized) return;

  FD_ZERO (&open_fds);
  FD_ZERO (&wrdy_fds);

#ifdef WIN32
  error = WSAStartup (0x0101, &WsaData);
  if (error == SOCKET_ERROR) {
    printf ("WSAStartup failed.\n");
  }
#endif
  channel_initialized = 1;
}

void IvyChannelStop (void)
{
  MainLoop = 0;
}

void IvyMainLoop(void)
{

  fd_set rdset, exset, wrset;
  int ready;

  while (MainLoop) {
		
    ChannelDefferedDelete();
	   	
    if (BeforeSelect)
      (*BeforeSelect)(BeforeSelectData);
    rdset = open_fds;
    wrset = wrdy_fds;
    exset = open_fds;
    ready = select(highestFd, &rdset, &wrset,  &exset, 
		   TimerGetSmallestTimeout());
		
    if (AfterSelect) 
      (*AfterSelect)(AfterSelectData);
		
    if (ready < 0 && (errno != EINTR)) {
      fprintf (stderr, "select error %d\n",errno);
      perror("select");
      return;
    }
    TimerScan(); /* should be spliited in two part ( next timeout & callbacks */
    if (ready > 0) {
      IvyChannelHandleExcpt(&exset);
      IvyChannelHandleRead(&rdset);
      IvyChannelHandleWrite(&wrset);
    }
  }
}

void IvyIdle()
{
  fd_set rdset, exset, wrset;
  int ready;
  struct timeval timeout = {0,0}; 

	
  ChannelDefferedDelete();
  rdset = open_fds;
  wrset = wrdy_fds;
  exset = open_fds;
  ready = select(highestFd, &rdset, &wrset,  &exset, &timeout);
  if (ready < 0 && (errno != EINTR)) {
    fprintf (stderr, "select error %d\n",errno);
    perror("select");
    return;
  }
  if (ready > 0) {
    IvyChannelHandleExcpt(&exset);
    IvyChannelHandleRead(&rdset);
    IvyChannelHandleWrite(&wrset);
  }
}



void IvySetBeforeSelectHook(IvyHookPtr before, void *data )
{
  BeforeSelect = before;
  BeforeSelectData = data;
}
void IvySetAfterSelectHook(IvyHookPtr after, void *data )
{
  AfterSelect = after;
  AfterSelectData = data;
}
