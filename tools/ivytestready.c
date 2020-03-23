/*
 *	Ivy perf mesure le temp de round trip
 *
 *	Copyright (C) 1997-2004
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main and only file
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 * 	         Yannick Jestin <jestin@cena.fr>
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#ifdef __MINGW32__
#include <regex.h> 
#include <getopt.h>
#endif
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __INTERIX
extern char *optarg;
extern int optind;
#endif
#endif


#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#include "ivyloop.h"
#define MILLISEC 1000.0

const char * me = "A";
const char * other = "B";
char ready_message[1000] = "A ready";
char ready_bind[1000] = "^B ready";

void Ready (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	const char *name = IvyGetApplicationName( app );
	int count = IvySendMsg ("are you there %s",name);
	printf("Application %s received '%s' from %s sent question 'are you there %s'= %d\n", me, ready_bind, name, name, count);
}

void Question (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	const char *name = IvyGetApplicationName( app );
	int count = IvySendMsg ("yes i am %s",me);
	printf("Application %s Reply to %s are you there = %d\n", me, name, count);
	
}
void Reply (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	const char *name = IvyGetApplicationName( app );
	printf("Application %s Reply to our question! %s\n", name, argv[0]);
	
}

void binCB( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event ) 
{
	const char *app_name = IvyGetApplicationName( app );
	switch ( event )
	{
	case IvyAddBind:
		printf("%s receive Application:%s bind '%s' ADDED\n", me, app_name, regexp );
		//if ( *me == 'A' ) usleep( 200000 ); // slowdown sending of regexp
		break;
	case IvyRemoveBind:
		printf("%s receive Application:%s bind '%s' REMOVED\n", me, app_name, regexp );
		break;
	case IvyChangeBind:
		printf("%s receive Application:%s bind '%s' CHANGED\n", me, app_name, regexp );
		break;
	case IvyFilterBind:
		printf("%s receive Application:%s bind '%s' FILTRED\n", me, app_name, regexp );
		break;

	}
}




int main(int argc, char *argv[])
{
	
	/* Mainloop management */
	if ( argc > 1 )
	{
	 me = "B" ;
	 other = "A";
	 strcpy( ready_message, "B ready");
	 strcpy( ready_bind, "^A ready");
	}

	IvyInit (me, ready_message, NULL,NULL,NULL,NULL);
	IvySetBindCallback( binCB, 0 );

#if defined(__GNUC__) && __GNUC_PREREQ(4,7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	IvyBindMsg (Ready, NULL, ready_bind);
#if defined(__GNUC__) && __GNUC_PREREQ(4,7)
#pragma GCC diagnostic pop
#endif

	IvyBindMsg (Question, NULL, "^are you there %s",me);
	IvyBindMsg (Reply, NULL, "^(yes i am %s)",other);
	 
	IvyStart (0);

	
	IvyMainLoop ();
	return 0;
}
