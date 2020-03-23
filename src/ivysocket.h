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
 *	$Id: ivysocket.h 3460 2011-01-24 13:39:15Z bustico $
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYSOCKET_H
#define IVYSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/* general Handle */

#define ANYPORT	0

#ifdef WIN32
#include <windows.h>
#ifdef __MINGW32__
#include <ws2tcpip.h>
#endif
#define IVY_HANDLE SOCKET
#define socklen_t int
#ifndef IN_MULTICAST
#define IN_MULTICAST(i)            (((long)(i) & 0xf0000000) == 0xe0000000)
#endif
#else
#define IVY_HANDLE int
#include <netinet/in.h>
#endif
#ifdef __INTERIX
#define socklen_t int
#endif

typedef enum {SendOk, SendStillCongestion, SendStateChangeToCongestion, 
	      SendStateChangeToDecongestion, SendStateFifoFull, SendError, 
	      SendParamError} SendState;

/* General Init */
extern void SocketInit();

/* Forward def */
typedef struct _client *Client;
typedef void (*SocketInterpretation) (Client client, const void *data, char *ligne);

/* Server Part */
typedef struct _server *Server;
extern Server SocketServer(int ipv6, unsigned short port, 
	void*(*create)(Client client),
	void(*handle_delete)(Client client, const void *data),
	void(*handle_decongestion)(Client client, const void *data),
        SocketInterpretation interpretation);
extern unsigned short SocketServerGetPort( Server server );
extern void SocketServerClose( Server server );

/* Client Part */

extern void SocketClose( Client client );
extern SendState SocketSend( Client client, const char *fmt, ... );
extern SendState SocketSendRaw( const Client client, const char *buffer, const int len );
extern SendState SocketSendRawWithId( const Client client, const char *id, const char *buffer, const int len );
extern const char *SocketGetPeerHost( Client client );
extern void SocketSetData( Client client, const void *data );
extern const void *SocketGetData( Client client );
extern void SocketBroadcast( char *fmt, ... );
extern Client SocketConnect( int ipv6, char * host, unsigned short port,
			void *data, 
			SocketInterpretation interpretation,
 	                void (*handle_delete)(Client client, const void *data),
			void(*handle_decongestion)(Client client, const void *data)
 );

extern Client SocketConnectAddr( int ipv6, struct sockaddr_storage* addr, unsigned short port, 
			void *data, 
			SocketInterpretation interpretation,
  		        void (*handle_delete)(Client client, const void *data),
			void(*handle_decongestion)(Client client, const void *data)
			);

extern int SocketWaitForReply( Client client, char *buffer, int size, int delai);

/* Socket UDP */
/* Creation d'une socket en mode non connecte */
/* et ecoute des messages */
extern Client SocketBroadcastCreate(
			int ipv6, 
			unsigned short port, 
			void *data, 
			SocketInterpretation interpretation
			);
/* Socket Multicast */
extern int SocketAddMember( Client client, unsigned long host );
extern int SocketAddMember6( Client client, struct in6_addr* host );

/* recuperation de l'emetteur du message */
extern struct sockaddr_storage* SocketGetRemoteAddr( Client client );
extern void SocketSetUuid (Client client, const char *uuid);
extern  const char* SocketGetUuid (const Client client);
extern int  SocketCmpUuid (const Client c1, const Client c2);
extern void SocketGetRemoteHost (Client client, const char **host, unsigned short *port );

extern unsigned short int SocketGetLocalPort ( Client client );
extern unsigned short int SocketGetRemotePort ( Client client );
/* emmission d'un broadcast UDP */
extern void SocketSendBroadcast( Client client, unsigned long host, unsigned short port, const char *fmt, ... );
extern void SocketSendBroadcast6( Client client, struct in6_addr* host, unsigned short port, const char *fmt, ... );

#ifdef __cplusplus
}
#endif

#endif

