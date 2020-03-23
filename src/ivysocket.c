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
 *	$Id: ivysocket.c 3510 2011-11-02 16:53:03Z fcolin $
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */



#ifdef OPENMP
#include <omp.h>
#endif

#ifdef WIN32
#include <Ws2tcpip.h>
#include <windows.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#ifdef WIN32
#ifndef __MINGW32__
typedef long ssize_t;
#endif
#define close closesocket
/*#define perror (a ) printf(a" error=%d\n",WSAGetLastError());*/
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#endif

#include "param.h"
#include "list.h"
#include "ivychannel.h"
#include "ivysocket.h"
#include "ivyloop.h"
#include "ivybuffer.h"
#include "ivyfifo.h"
#include "ivydebug.h"


union sockaddr_46 {
  struct sockaddr_in  s4;
  struct sockaddr_in6 s6;
  struct sockaddr     sa;
  struct sockaddr_storage ss;
};

struct _server {
	Server next;
	IVY_HANDLE fd;
	Channel channel;
	unsigned short port;
	int ipv6;
	void *(*create)(Client client);
	void (*handle_delete)(Client client, const void *data);
	void (*handle_decongestion)(Client client, const void *data);
	SocketInterpretation interpretation;
};

struct _client {
	Client next;
	IVY_HANDLE fd;
	Channel channel;
	unsigned short port;
	char app_uuid[128];
	int ipv6;
	struct sockaddr_storage from; //  IPV6 ou IPV4
	socklen_t from_len;
	SocketInterpretation interpretation;
	void (*handle_delete)(Client client, const void *data);
	void (*handle_decongestion)(Client client, const void *data);
	char terminator;	/* character delimiter of the message */ 
	/* Buffer de reception */
	long buffer_size;
	char *buffer;		/* dynamicaly reallocated */
	char *ptr;
	/* Buffer d'emission */
        IvyFifoBuffer *ifb;             /* le buffer circulaire en cas de congestion */
  	/* user data */
	const void *data;
#ifdef OPENMP
	omp_lock_t fdLock;
#endif
};
 

static Server servers_list = NULL;
static Client clients_list = NULL;

static int debug_send = 0;

#ifdef WIN32
WSADATA	WsaData;
#endif


static SendState BufferizedSocketSendRaw (const Client client, const char *buffer, const int len );


void SocketInit()
{
	if ( getenv( "IVY_DEBUG_SEND" )) debug_send = 1;
	IvyChannelInit();
}

static void DeleteSocket(void *data)
{
	Client client = (Client )data;
	if (client->handle_delete )
		(*client->handle_delete) (client, client->data );
	shutdown (client->fd, 2 );
	close (client->fd );
#ifdef OPENMP
	omp_destroy_lock (&(client->fdLock));
#endif
	if (client->ifb != NULL) {
	  IvyFifoDelete (client->ifb);
	  client->ifb = NULL;
	}
	IVY_LIST_REMOVE (clients_list, client );
}


static void DeleteServerSocket(void *data)
{
        Server server = (Server )data;
#ifdef BUGGY_END
        if (server->handle_delete )
                (*server->handle_delete) (server, NULL );
#endif
        shutdown (server->fd, 2 );
        close (server->fd );
        IVY_LIST_REMOVE (servers_list, server);
}


static void HandleSocket (Channel channel, IVY_HANDLE fd, void *data)
{
	Client client = (Client)data;
	char *ptr;
	char *ptr_nl;
	long nb_to_read = 0;
	long nb;
	long nb_occuped;
	long len;
	
	/* limitation taille buffer */
	nb_occuped = client->ptr - client->buffer;
	nb_to_read = client->buffer_size - nb_occuped;
	if (nb_to_read == 0 ) {
		client->buffer_size *= 2; /* twice old size */
		client->buffer = (char *) realloc( client->buffer, client->buffer_size );
		if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		fprintf(stderr, "Buffer Limit reached realloc new size %ld\n", client->buffer_size );
		nb_to_read = client->buffer_size - nb_occuped;
		client->ptr = client->buffer + nb_occuped; 
	}
	client->from_len = sizeof (client->from );
	nb = recvfrom (fd, client->ptr, nb_to_read, 0, (struct sockaddr*)&(client->from), &(client->from_len));
	if (nb  < 0) {
		perror(" Read Socket ");
		IvyChannelRemove (client->channel );
		return;
	}
	if (nb == 0 ) {
		IvyChannelRemove (client->channel );
		return;
	}
	client->ptr += nb;
	ptr = client->buffer;
	while ((ptr_nl = (char *) memchr (ptr, client->terminator,  client->ptr - ptr )))
		{
		*ptr_nl ='\0';
		if (client->interpretation )
			(*client->interpretation) (client, client->data, ptr );
			else fprintf (stderr,"Socket No interpretation function ???\n");
		ptr = ++ptr_nl;
		}
	if (ptr < client->ptr )
		{ /* recopie ligne incomplete au debut du buffer */
		len = client->ptr - ptr;
		memmove (client->buffer, ptr, len  );
		client->ptr = client->buffer + len;
		}
		else
		{
		client->ptr = client->buffer;
		}
}



static void HandleCongestionWrite (Channel channel, IVY_HANDLE fd, void *data)
{
  Client client = (Client)data;
  
  if (IvyFifoSendSocket (client->ifb, fd) == 0) {
    // Not congestionned anymore
    IvyChannelClearWritableEvent (channel);
    //    printf ("DBG> Socket *DE*congestionnee\n");
    IvyFifoDelete (client->ifb);
    client->ifb = NULL;
    if (client->handle_decongestion )
      (*client->handle_decongestion) (client, client->data );

  }
}


static void HandleServer(Channel channel, IVY_HANDLE fd, void *data)
{
	Server server = (Server ) data;
	Client client;
	IVY_HANDLE ns;
	socklen_t addrlen;
	struct sockaddr_storage remote;
#ifdef WIN32
	u_long iMode = 1; /* non blocking Mode */
#else
	long   socketFlag;
#endif
	TRACE( "Accepting Connection...\n", );
	addrlen = sizeof (remote );
	if ((ns = accept (fd, (struct sockaddr*)&remote, &addrlen)) <0)
		{
		perror ("*** accept ***");
		return;
		};

	TRACE( "Accepting Connection ret\n", );

	IVY_LIST_ADD_START (clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = (char *) malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->ipv6 = server->ipv6;
	client->from = remote;
	client->from_len = addrlen;
	client->fd = ns;
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by HandleServer");

#ifdef WIN32
	if ( ioctlsocket(client->fd,FIONBIO, &iMode ) )
		fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
#else
	socketFlag = fcntl (client->fd, F_GETFL);
	if (fcntl (client->fd, F_SETFL, socketFlag|O_NONBLOCK)) {
	  fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
	}
#endif
	if (setsockopt(client->fd,            /* socket affected */
		       IPPROTO_TCP,     /* set option at TCP level */
		       TCP_NODELAY,     /* name of option */
		       (char *) &TCP_NO_DELAY_ACTIVATED,  /* the cast is historical */
 		       sizeof(TCP_NO_DELAY_ACTIVATED)) < 0)    /* length of option value */
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option  TCP_NODELAY***");
	    exit(0);
	  } 




	client->channel = IvyChannelAdd (ns, client,  DeleteSocket, HandleSocket,
					 HandleCongestionWrite);
	client->interpretation = server->interpretation;
	client->ptr = client->buffer;
	client->handle_delete = server->handle_delete;
	client->handle_decongestion = server->handle_decongestion;
	client->data = (*server->create) (client );
#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif


	IVY_LIST_ADD_END (clients_list, client );
	
}

Server SocketServer(int ipv6, unsigned short port, 
	void*(*create)(Client client),
	void(*handle_delete)(Client client, const void *data),
        void(*handle_decongestion)(Client client, const void *data),
	void(*interpretation) (Client client, const void *data, char *ligne))
{
	Server server;
	IVY_HANDLE fd;
	int one=1;
	union sockaddr_46 local;
	socklen_t addrlen;

	if ((fd = socket (ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("***open socket ***");
		exit(0);
		};

	
	if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one)) < 0)
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option SO_REUSEADDR ***");
	    exit(0);
	  } 

#ifdef SO_REUSEPORT

	if (setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (char *)&one, sizeof (one)) < 0)
	  {
	    perror ("*** set socket option REUSEPORT ***");
	    exit(0);
	  }
#endif
	
	memset( &local,0,sizeof(local) ); 
	if ( ipv6 ) 
	{ 
		struct sockaddr_in6*  local6= &local.s6;
		local6->sin6_family =  AF_INET6; 
		local6->sin6_addr = in6addr_any; 
		local6->sin6_port = htons (port); 
		addrlen = sizeof(struct sockaddr_in6); 
	} 
	else 
	{ 
		struct sockaddr_in*  local4= &local.s4;
		local4->sin_family =  AF_INET; 
		local4->sin_addr.s_addr = INADDR_ANY; 
		local4->sin_port = htons (port); 
		addrlen = sizeof(struct sockaddr_in);
	}

	if (bind(fd, &local.sa, addrlen) < 0)
		{
		perror ("*** bind ***");
		exit(0);
		}

	if (getsockname(fd, &local.sa, &addrlen) < 0)
		{
		perror ("***get socket name ***");
		exit(0);
		} 
	
	if (listen (fd, 128) < 0){
		perror ("*** listen ***");
		exit(0);
		};
	

	IVY_LIST_ADD_START (servers_list, server );
	server->fd = fd;
	server->channel = IvyChannelAdd (fd, server, DeleteServerSocket, 
					 HandleServer, NULL);
	server->ipv6 = ipv6;
	server->create = create;
	server->handle_delete = handle_delete;
	server->handle_decongestion = handle_decongestion;
	server->interpretation = interpretation;
	server->port = ntohs(ipv6 ? local.s6.sin6_port : local.s4.sin_port);
	IVY_LIST_ADD_END (servers_list, server );
	
	return server;
}

unsigned short SocketServerGetPort (Server server )
{
	return server ? server->port : 0;
}

void SocketServerClose (Server server )
{
	if (!server)
		return;
	IvyChannelRemove (server->channel );
}

const char *SocketGetPeerHost (Client client )
{
	int err;
	struct sockaddr_storage name;
	socklen_t len = sizeof(name);
	static char host[NI_MAXHOST];
	static char serv[NI_MAXSERV];
	
	if (!client)
		return "undefined";
	
	err = getpeername (client->fd, (struct sockaddr *)&name, &len );
	if (err < 0 ) return "can't get peer";

	err = getnameinfo((struct sockaddr*)&name, len, host, sizeof( host), serv, sizeof( serv ), NI_NOFQDN  );
		
	
	if (err != 0 ) 
	{
#ifdef WIN32
		DWORD dwError = WSAGetLastError();
        if (dwError != 0) {
            if (dwError == WSAHOST_NOT_FOUND) {
                fprintf(stderr, "SocketGetPeerHost :: getnameinfo Host not found\n");
            } else if (dwError == WSANO_DATA) {
                fprintf(stderr, "SocketGetPeerHost :: getnameinfo No data record found\n");
            } else {
                fprintf(stderr, "SocketGetPeerHost :: getnameinfo Function failed with error: %ld\n", dwError);
            }
		}
#else
		fprintf(stderr, "SocketGetPeerHost :: getnameinfo %s\n", gai_strerror( err ) );
#endif
		return "can't translate addr";
	}
	return host;
}

unsigned short int SocketGetLocalPort ( Client client )
{
	int err;
	unsigned short port;
	union sockaddr_46 name;
	socklen_t len = sizeof(name);

	if (!client)
		return 0;

	err = getsockname (client->fd, &name.sa, &len );
	if (err < 0 ) return 0;
	if ( name.ss.ss_family == AF_INET6 )
	{
		struct sockaddr_in6*  local6= &name.s6;
		port = ntohs(local6->sin6_port);
	} 
	else 
	{ 
		struct sockaddr_in*  local4= &name.s4;
		port = ntohs(local4->sin_port);
	}
	
	return port;
}
unsigned short int SocketGetRemotePort ( Client client )
{
	if (!client)
		return 0;
	if ( client->from.ss_family == AF_INET6 )
		return ((struct sockaddr_in6*)(&client->from ))->sin6_port;
	return ((struct sockaddr_in*)(&client->from ))->sin_port;
}

struct sockaddr_storage * SocketGetRemoteAddr (Client client )
{
	return client ? &client->from : 0;
}

void SocketGetRemoteHost (Client client, const char **hostptr, unsigned short *port )
{
	int err;
	static char host[NI_MAXHOST];
	static char serv[NI_MAXSERV];
	
	if (!client)
		return;
	if( client->from_len == 0 )
	{
		fprintf(stderr, "SocketGetRemoteHost :: getnameinfo bad Addr Len\n" );
		*hostptr = "unknown";
		*port = 0;
		return;
	}
	err = getnameinfo((struct sockaddr*)&client->from, client->from_len, host, sizeof( host), serv, sizeof( serv ), NI_NOFQDN  );

	
	if (err != 0 )
	{
#ifdef WIN32
		DWORD dwError = WSAGetLastError();
        if (dwError != 0) {
            if (dwError == WSAHOST_NOT_FOUND) {
                fprintf(stderr, "SocketGetRemoteHost :: getnameinfo Host not found\n");
            } else if (dwError == WSANO_DATA) {
                fprintf(stderr, "SocketGetRemoteHost :: getnameinfo No data record found\n");
            } else {
                fprintf(stderr, "SocketGetRemoteHost :: getnameinfo Function failed with error: %ld\n", dwError);
            }
		}
#else
		fprintf(stderr, "SocketGetRemoteHost :: getnameinfo (%d) %s\n", err, gai_strerror( err ) );
#endif
		*hostptr = "unknown";
	}
	else 
	{
	/* extract hostname and port from last message received */

	if ( client->ipv6 )
	{
		struct sockaddr_in6* sock_addr = (struct sockaddr_in6*)(&client->from);
		*port = ntohs (sock_addr->sin6_port );
	}
	else
	{
		struct sockaddr_in* sock_addr = (struct sockaddr_in*)(&client->from);
		*port = ntohs (sock_addr->sin_port );
	}
	*hostptr = host;
	}
	
}

void SocketClose (Client client )
{
	if (client)
		IvyChannelRemove (client->channel );
}

SendState SocketSendRaw (const Client client, const char *buffer, const int len )
{
  SendState state;
  
  if (!client)
    return SendParamError;
  
#ifdef OPENMP
  omp_set_lock (&(client->fdLock));
#endif
  
  state = BufferizedSocketSendRaw (client, buffer, len);

#ifdef OPENMP
  omp_unset_lock (&(client->fdLock));
#endif
  
  return state;
}


static SendState BufferizedSocketSendRaw (const Client client, const char *buffer, const int len )
{
  ssize_t reallySent;
  SendState state;

  if (client->ifb != NULL) {
    // Socket en congestion : on rajoute juste le flux dans le buffer, 
    // quand la socket sera dispo en ecriture, le select appellera la callback
    // pour vider ce buffer
    IvyFifoWrite (client->ifb, buffer, len);
    state = IvyFifoIsFull (client->ifb) ? SendStateFifoFull : SendStillCongestion;
  } else {
    // on tente d'ecrire direct dans la socket
    reallySent =  send (client->fd, buffer, len, 0);
    if (reallySent == len) 
	{
      state = SendOk; // PAS CONGESTIONNEE
    } else if (reallySent == -1) 
	{
#ifdef WIN32
	if ( WSAGetLastError() == WSAEWOULDBLOCK) {
#else
      if (errno == EWOULDBLOCK) {
#endif
	// Aucun octet n'a été envoyé, mais le send ne rend pas 0
	// car 0 peut être une longueur passée au send, donc dans ce cas
	// send renvoie -1 et met errno a EWOULDBLOCK
	client->ifb = IvyFifoNew ();
	IvyFifoWrite (client->ifb, buffer, len);
	// on ajoute un fdset pour que le select appelle une callback pour vider
	// le buffer quand la socket sera ?? nouveau libre
	IvyChannelAddWritableEvent (client->channel);
	state = SendStateChangeToCongestion;
      } else {
	state = SendError; // ERREUR
      }
    } else {
      // socket congestionnée
      // on initialise une fifo pour accumuler les données
      client->ifb = IvyFifoNew ();
      IvyFifoWrite (client->ifb, &(buffer[reallySent]), len-reallySent);
      // on ajoute un fdset pour que le select appelle une callback pour vider
      // le buffer quand la socket sera à nouveau libre
      IvyChannelAddWritableEvent (client->channel);
      state = SendStateChangeToCongestion;
    }
  }

#ifdef DEBUG
  // DBG BEGIN DEBUG
  /* SendOk, SendStillCongestion, SendStateChangeToCongestion,
          SendStateChangeToDecongestion, SendStateFifoFull, SendError,
	  SendParamError
  */
  {
    static SendState DBG_state = SendOk;
    char *litState="";
    if (state != DBG_state) {
      switch (state) {
      case SendOk : litState = "SendOk";
	break;
      case  SendStillCongestion: litState = "SendStillCongestion";
	break;
      case SendStateChangeToCongestion : litState = "SendStateChangeToCongestion";
	break;
      case  SendStateChangeToDecongestion: litState = "SendStateChangeToDecongestion";
	break;
      case  SendStateFifoFull: litState = "SendStateFifoFull";
	break;
      case  SendError: litState = "SendError";
	break;
      case  SendParamError: litState = "SendParamError";
	break;
      }
      printf ("DBG>> BufferizedSocketSendRaw, state changed to '%s'\n", litState);
      DBG_state = state;
    }
  }
  // DBG END DEBUG
#endif

  return (state);
}



SendState SocketSendRawWithId( const Client client, const char *id, const char *buffer, const int len )
{
  SendState s1, s2;
  
#ifdef OPENMP
  omp_set_lock (&(client->fdLock));
#endif
  
  s1 = BufferizedSocketSendRaw (client, id, strlen (id));

  s2 = BufferizedSocketSendRaw (client, buffer, len);
  
#ifdef OPENMP
  omp_unset_lock (&(client->fdLock));
#endif
  
  if (s1 == SendStateChangeToCongestion) {
    // si le passage en congestion s'est fait sur l'envoi de l'id
    s2 = s1;
  }

  return (s2);
}


void SocketSetData (Client client, const void *data )
{
  if (client) {
    client->data = data;
  }
}

SendState SocketSend (Client client, const char *fmt, ... )
{
  SendState state;
  static IvyBuffer buffer = {NULL, 0, 0 }; /* Use static mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif

  va_list ap;
  int len;
  va_start (ap, fmt );
  buffer.offset = 0;
  len = make_message (&buffer, fmt, ap );
  state = SocketSendRaw (client, buffer.data, len );
  va_end (ap );
  return state;
}

const void *SocketGetData (Client client )
{
	return client ? client->data : 0;
}

void SocketBroadcast ( char *fmt, ... )
{
	Client client;
	static IvyBuffer buffer = {NULL, 0, 0 }; /* Use static mem to eliminate 
						    multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int len;
	
	va_start (ap, fmt );
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	IVY_LIST_EACH (clients_list, client )
		{
		SocketSendRaw (client, buffer.data, len );
		}
}

/*
Ouverture d'un canal TCP/IP en mode client
*/
//Client SocketConnect (int ipv6, char * host, unsigned short port, 
//			void *data, 
//			SocketInterpretation interpretation,
//		        void (*handle_delete)(Client client, const void *data),
//		        void(*handle_decongestion)(Client client, const void *data)
//			)
//{
//	struct hostent *rhost;
//
//	if ((rhost = gethostbyname (host )) == NULL) {
//		fprintf(stderr, "Erreur %s Calculateur inconnu !\n",host);
//		 return NULL;
//	}
//	return SocketConnectAddr (ipv6, rhost->h_addr, port, data, 
//				  interpretation, handle_delete, handle_decongestion);
//}

Client SocketConnectAddr (int ipv6, struct sockaddr_storage * addr, unsigned short port, 
			  void *data, 
			  SocketInterpretation interpretation,
			  void (*handle_delete)(Client client, const void *data),
		          void(*handle_decongestion)(Client client, const void *data)
			  )
{
	IVY_HANDLE handle;
	Client client;
	union sockaddr_46 remote;
	socklen_t addrlen;
#ifdef WIN32
	u_long iMode = 1; /* non blocking Mode */
#else
	long   socketFlag;
#endif

	if ((handle = socket ( ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) < 0){
		perror ("*** client socket ***");
		return NULL;
	};
	memset( &remote,0,sizeof(remote) ); 
	
	if ( ipv6 )
	{
		struct sockaddr_in6* remote6= &remote.s6;
		remote6->sin6_family = AF_INET6;
		remote6->sin6_addr =  ((struct sockaddr_in6*)addr)->sin6_addr;
		remote6->sin6_scope_id =  ((struct sockaddr_in6*)addr)->sin6_scope_id;
		remote6->sin6_flowinfo =  ((struct sockaddr_in6*)addr)->sin6_flowinfo;
		remote6->sin6_port = htons (port);
		addrlen = sizeof(struct sockaddr_in6);
	}
	else
	{
		struct sockaddr_in* remote4= &remote.s4;
		remote4->sin_family = AF_INET;
		remote4->sin_addr = ((struct sockaddr_in*)addr)->sin_addr;
		remote4->sin_port = htons (port);
		addrlen = sizeof(struct sockaddr_in);
	}

	if (connect (handle, &remote.sa, addrlen ) < 0){
		perror ("*** client connect ***");
		return NULL;
	};
#ifdef WIN32
	if ( ioctlsocket(handle,FIONBIO, &iMode ) )
		fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
#else
	socketFlag = fcntl (handle, F_GETFL);
	if (fcntl (handle, F_SETFL, socketFlag|O_NONBLOCK)) {
	  fprintf(stderr,"Warning : Setting socket in nonblock mode FAILED\n");
	}
#endif
	if (setsockopt(handle,            /* socket affected */
		       IPPROTO_TCP,     /* set option at TCP level */
		       TCP_NODELAY,     /* name of option */
		       (char *) &TCP_NO_DELAY_ACTIVATED,  /* the cast is historical */
 		       sizeof(TCP_NO_DELAY_ACTIVATED)) < 0)    /* length of option value */
	  {
#ifdef WIN32
	    fprintf(stderr," setsockopt %d\n",WSAGetLastError());
#endif
	    perror ("*** set socket option  TCP_NODELAY***");
	    exit(0);
	  } 


	IVY_LIST_ADD_START(clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = (char *) malloc( client->buffer_size );
	if (!client->buffer )
		{
		fprintf(stderr,"HandleSocket Buffer Memory Alloc Error\n");
		exit(0);
		}
		client->terminator = '\n';
	client->fd = handle;
	client->ipv6 = ipv6;
	client->channel = IvyChannelAdd (handle, client,  DeleteSocket, 
					 HandleSocket, HandleCongestionWrite );
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->handle_delete = handle_delete;
	client->handle_decongestion = handle_decongestion;
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by SocketConnectAddr");


#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif
	IVY_LIST_ADD_END(clients_list, client );
	

	return client;
}
/* TODO factoriser avec HandleRead !!!! */
int SocketWaitForReply (Client client, char *buffer, int size, int delai)
{
	fd_set rdset;
	struct timeval timeout;
	struct timeval *timeoutptr = &timeout;
	int ready;
	char *ptr;
	char *ptr_nl;
	long nb_to_read = 0;
	long nb;
	IVY_HANDLE fd;

	fd = client->fd;
	ptr = buffer;
	timeout.tv_sec = delai;
	timeout.tv_usec = 0;
   	do {
		/* limitation taille buffer */
		nb_to_read = size - (ptr - buffer );
		if (nb_to_read == 0 )
			{
			fprintf(stderr, "Erreur message trop long sans LF\n");
			ptr  = buffer;
			return -1;
			}
		FD_ZERO (&rdset );
		FD_SET (fd, &rdset );
		ready = select(fd+1, &rdset, 0,  0, timeoutptr);
		if (ready < 0 )
			{
			perror("select");
			return -1;
			}
		if (ready == 0 )
			{
			return -2;
			}
		if ((nb = recv (fd , ptr, nb_to_read, 0 )) < 0)
			{
			perror(" Read Socket ");
			return -1;
			}
		if (nb == 0 )
			return 0;

		ptr += nb;
		*ptr = '\0';
		ptr_nl = strchr (buffer, client->terminator );
	} while (!ptr_nl );
	*ptr_nl = '\0';
	return (ptr_nl - buffer);
}

/* Socket UDP */

Client SocketBroadcastCreate (int ipv6, unsigned short port, 
				void *data, 
				SocketInterpretation interpretation
			)
{
	IVY_HANDLE handle;
	Client client;
	int on = 1;
	union sockaddr_46 local;
	socklen_t addrlen;

	memset( &local,0,sizeof(local) ); 
	if ( ipv6 ) 
	{ 
		struct sockaddr_in6*  local6= &local.s6;
		local6->sin6_family =  AF_INET6; 
		local6->sin6_addr = in6addr_any; 
		local6->sin6_port = htons (port); 
		addrlen = sizeof( struct sockaddr_in6 );
	} 
	else 
	{ 
		struct sockaddr_in*  local4= &local.s4;
		local4->sin_family =  AF_INET; 
		local4->sin_addr.s_addr = INADDR_ANY; 
		local4->sin_port = htons (port); 
		addrlen = sizeof( struct sockaddr_in );
	}
	
	if ((handle = socket ( ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror ("*** dgram socket ***");
		return NULL;
	};

	/* wee need to used multiple client on the same host */
	if (setsockopt (handle, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** set socket option REUSEADDR ***");
			return NULL;
		};
#ifdef SO_REUSEPORT

	if (setsockopt (handle, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** set socket option REUSEPORT ***");
			return NULL;
		}
#endif
	/* wee need to broadcast */
	if (setsockopt (handle, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof (on)) < 0)
		{
			perror ("*** BROADCAST ***");
			return NULL;
		};

	if (bind(handle, &local.sa,  addrlen ) < 0)
		{
			perror ("*** BIND ***");
			return NULL;
		};

	IVY_LIST_ADD_START(clients_list, client );
	
	client->buffer_size = IVY_BUFFER_SIZE;
	client->buffer = (char *) malloc( client->buffer_size );
	if (!client->buffer )
		{
		perror("HandleSocket Buffer Memory Alloc Error: ");
		exit(0);
		}
	client->terminator = '\n';
	client->fd = handle;
	client->ipv6 = ipv6;
	client->channel = IvyChannelAdd (handle, client,  DeleteSocket, 
					 HandleSocket, HandleCongestionWrite);
	client->interpretation = interpretation;
	client->ptr = client->buffer;
	client->data = data;
	client->ifb = NULL;
	strcpy (client->app_uuid, "init by SocketBroadcastCreate");

#ifdef OPENMP
	omp_init_lock (&(client->fdLock));
#endif
	IVY_LIST_ADD_END(clients_list, client );
	
	return client;
}
/* TODO unifier les deux fonctions */
void SocketSendBroadcast (Client client, unsigned long host, unsigned short port, const char *fmt, ... )
{
	struct sockaddr_in remote;
	static IvyBuffer buffer = { NULL, 0, 0 }; /* Use satic mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int err,len;

	if (!client)
		return;

	va_start (ap, fmt );
	buffer.offset = 0;
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	/* Send UDP packet to the dest */
	memset( &remote,0,sizeof(remote) );
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = htonl (host );
	remote.sin_port = htons(port);
	err = sendto (client->fd, 
			buffer.data, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}	
	
}

void SocketSendBroadcast6 (Client client, struct in6_addr* host, unsigned short port, const char *fmt, ... )
{
	struct sockaddr_in6 remote;
	static IvyBuffer buffer = { NULL, 0, 0 }; /* Use satic mem to eliminate multiple call to malloc /free */
#ifdef OPENMP
#pragma omp threadprivate (buffer)
#endif
	va_list ap;
	int err,len;

	if (!client)
		return;

	va_start (ap, fmt );
	buffer.offset = 0;
	len = make_message (&buffer, fmt, ap );
	va_end (ap );
	/* Send UDP packet to the dest */
	memset( &remote,0,sizeof(remote) );
	remote.sin6_family = AF_INET6;
	remote.sin6_addr = *host;
	remote.sin6_port = htons(port);
	err = sendto (client->fd, 
			buffer.data, len,0,
			(struct sockaddr *)&remote,sizeof(remote));
	if (err != len) {
		perror ("*** send ***");
	}	
	
}


/* Socket Multicast */

int SocketAddMember(Client client, unsigned long host )
{
	struct ip_mreq imr;
/*
Multicast datagrams with initial TTL 0 are restricted to the same host. 
Multicast datagrams with initial TTL 1 are restricted to the same subnet. 
Multicast datagrams with initial TTL 32 are restricted to the same site. 
Multicast datagrams with initial TTL 64 are restricted to the same region. 
Multicast datagrams with initial TTL 128 are restricted to the same continent. 
Multicast datagrams with initial TTL 255 are unrestricted in scope. 
*/
	unsigned char ttl = 64 ; /* Arbitrary TTL value. */
	/* wee need to broadcast */

	imr.imr_multiaddr.s_addr = htonl( host );
	imr.imr_interface.s_addr = INADDR_ANY; 
	if(setsockopt(client->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&imr,sizeof(imr)) == -1 )
		{
      	perror("setsockopt() Cannot join group");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
				
  	if(setsockopt(client->fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0 )
		{
      	perror("setsockopt() Cannot set TTL");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}

	return 1;
}

int SocketAddMember6(Client client, struct in6_addr* host )
{
	struct ipv6_mreq imr;
/*
Multicast datagrams with initial TTL 0 are restricted to the same host. 
Multicast datagrams with initial TTL 1 are restricted to the same subnet. 
Multicast datagrams with initial TTL 32 are restricted to the same site. 
Multicast datagrams with initial TTL 64 are restricted to the same region. 
Multicast datagrams with initial TTL 128 are restricted to the same continent. 
Multicast datagrams with initial TTL 255 are unrestricted in scope. 
*/
	//unsigned char ttl = 64 ; /* Arbitrary TTL value. */
	/* wee need to broadcast */

	imr.ipv6mr_multiaddr = *host;
	imr.ipv6mr_interface = 0;  /// TODO: Attention ca fait quoi ca ???
	if(setsockopt(client->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&imr,sizeof(imr)) == -1 )
		{
      	perror("setsockopt() Cannot join group");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
				
  	/*
	if(setsockopt(client->fd, IPPROTO_IPV6, IPV6_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0 )
		{
      	perror("setsockopt() Cannot set TTL");
      	fprintf(stderr, "Does your kernel support IP multicast extensions ?\n");
      	return 0;
    		}
	*/

	return 1;
}

extern void SocketSetUuid (Client client, const char *uuid)
{
  strncpy (client->app_uuid, uuid, sizeof (client->app_uuid));
}

const char* SocketGetUuid (const Client client)
{
  return client->app_uuid;
}

extern int  SocketCmpUuid (const Client c1, const Client c2)
{
  return strncmp (c1->app_uuid, c2->app_uuid, sizeof (c1->app_uuid));
}


