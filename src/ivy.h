/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 * 	Main functions
 *
 *	Authors: François-Régis Colin <fcolin@cena.dgac.fr>
 *		 Stéphane Chatty <chatty@cena.dgac.fr>
 *
 *	$Id: ivy.h 3588 2013-06-19 12:39:15Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */


#ifndef IVY_H
#define IVY_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

/* numero par default du bus */

typedef  struct _clnt_lst_dict *RWIvyClientPtr;
typedef  const struct _clnt_lst_dict *IvyClientPtr;

typedef enum { IvyApplicationConnected, IvyApplicationDisconnected, 
	       IvyApplicationCongestion , IvyApplicationDecongestion,
	       IvyApplicationFifoFull } IvyApplicationEvent;
typedef enum { IvyAddBind, IvyRemoveBind, IvyFilterBind, IvyChangeBind } IvyBindEvent;

extern void IvyDefaultApplicationCallback( IvyClientPtr app, void *user_data, IvyApplicationEvent event ) ;
extern void IvyDefaultBindCallback( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event ) ;


/* callback callback appele sur connexion deconnexion d'une appli */
typedef void (*IvyApplicationCallback)( IvyClientPtr app, void *user_data, IvyApplicationEvent event ) ;

/* callback callback appele sur ajout ou suppression d'un bind */
typedef void (*IvyBindCallback)( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event ) ;

/* callback appele sur reception de die */
typedef void (*IvyDieCallback)( IvyClientPtr app, void *user_data, int id ) ;

/* callback appele sur reception de pong */
typedef void (*IvyPongCallback)( IvyClientPtr app, int round_trip_delay) ;

/* callback appele sur reception de messages normaux */
typedef void (*MsgCallback)( IvyClientPtr app, void *user_data, int argc, char **argv ) ;

/* callback appele sur reception de messages directs */
typedef void (*MsgDirectCallback)( IvyClientPtr app, void *user_data, int id, char *msg ) ;

/* identifiant d'une expression reguliere ( Bind/Unbind ) */
typedef struct _msg_rcv *MsgRcvPtr;

/* filtrage des regexps */
void IvySetFilter( int argc, const char **argv);
void IvyAddFilter( const char *arg);
void IvyRemoveFilter( const char *arg);

void IvyInit(
	 const char *AppName,		/* nom de l'application */
	 const char *ready,		/* ready Message peut etre NULL */
	 IvyApplicationCallback callback, /* callback appele sur connection deconnection d'une appli */
	 void *data,			/* user data passe au callback */
	 IvyDieCallback die_callback,	/* last change callback before die */
	 void *die_data 		/* user data */
	 );
void IvyTerminate(void);
  
void IvySetBindCallback(	 
			  IvyBindCallback bind_callback,
			  void *bind_data );
void IvySetPongCallback(	 
			  IvyPongCallback pong_callback );

void IvyStart (const char*);
void IvyStop (void);

/* query sur les applications connectees */
const char *IvyGetApplicationName( IvyClientPtr app );
const char *IvyGetApplicationHost( IvyClientPtr app );
IvyClientPtr IvyGetApplication( char *name );
char *IvyGetApplicationList(const char *sep);
char **IvyGetApplicationMessages( IvyClientPtr app); /* demande de reception d'un message */

MsgRcvPtr IvyBindMsg( MsgCallback callback, void *user_data, const char *fmt_regexp, ... )
__attribute__((format(printf,3,4))) ; /* avec sprintf prealable */

MsgRcvPtr IvyChangeMsg (MsgRcvPtr msg, const char *fmt_regex, ... )
__attribute__((format(printf,2,3))); /* avec sprintf prealable */

void IvyUnbindMsg( MsgRcvPtr id );

/* emission d'un message d'erreur */
void IvySendError(IvyClientPtr app, int id, const char *fmt, ... )
__attribute__((format(printf,3,4))) ; /* avec sprintf prealable */

/* emmission d'un message die pour terminer l'application */
void IvySendDieMsg(IvyClientPtr app );

/* emission d'un message retourne le nb effectivement emis */

int IvySendMsg( const char *fmt_message, ... )
__attribute__((format(printf,1,2))); /* avec sprintf prealable */

/* Message Direct Inter-application */

void IvyBindDirectMsg( MsgDirectCallback callback, void *user_data);
void IvySendDirectMsg( IvyClientPtr app, int id, char *msg );

  /* to use ping protocol, you need to bind a callback to pong with IvySetPongCallback 
   prior to sending ping 
   When pong is received, your supplied callback will be called with an int argument which is :
   ° if positive value : the round trip time in micro seconds
   ° if negative value : the timout of the previous ping (for the same application) 
                         which has not yet been answered
   see ivyprobe.c to see a simple example of usage
*/
void IvySendPing( IvyClientPtr app);

#ifdef __cplusplus
}
#endif

#endif
