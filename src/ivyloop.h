/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Main loop handling around select
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.dgac.fr>
 *		 St�phane Chatty <chatty@cena.dgac.fr>
 *
 *	$Id: ivyloop.h 3464 2011-01-25 17:34:23Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 *
 */

#ifndef IVYLOOP_H
#define IVYLOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ivychannel.h"

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#define IVY_HANDLE SOCKET
#else
#define IVY_HANDLE int
#endif

/*
Boucle principale d'IVY base� sur un select
les fonctions hook et unhook encradre le select 
de la maniere suivante:

	BeforeSelect est appeler avant l'appel system select
	AfterSelect est appeler avent l'appel system select
	ces function peuvent utilis�es pour depose un verrou dans le cas 
	d'utilisation de la mainloop Ivy dans une thread separe
	
	BeforeSelect ==> on libere l'acces avant la mise en attente sur le select
	AfterSelect == > on verrouille l'acces en sortie du select 

	!!!! Attention donc l'appel des callbacks ivy se fait avec l'acces verrouille !
*/

extern void IvyMainLoop(void);

typedef void ( *IvyHookPtr) ( void *data );

void IvyChannelAddWritableEvent(Channel channel);
void IvyChannelClearWritableEvent(Channel channel);
void IvySetBeforeSelectHook(IvyHookPtr before, void *data );
void IvySetAfterSelectHook(IvyHookPtr after, void *data );

#ifdef __cplusplus
}
#endif

#endif

