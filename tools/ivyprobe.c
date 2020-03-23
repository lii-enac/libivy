/*
 *	Ivy probe
 *
 *	Copyright (C) 1997-2004
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Main and only file
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.fr>
 * 	         Yannick Jestin <jestin@cena.fr>
 *
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#define DEFAULT_IVYPROBE_NAME "IVYPROBE"
#define DEFAULT_READY " Ready"
#include "version.h"

#define IVYMAINLOOP

#ifdef XTMAINLOOP
#undef IVYMAINLOOP
#endif
#ifdef GLIBMAINLOOP
#undef IVYMAINLOOP
#endif

#ifdef GLUTMAINLOOP
#undef IVYMAINLOOP
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#ifdef __MINGW32__
#include <regex.h> 
#include <getopt.h>
#else
#include "getopt.h"
#endif
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __INTERIX
extern char *optarg;
extern int optind;
#endif

#endif
#ifdef XTMAINLOOP
#include "ivyxtloop.h"
#endif
#ifdef GLIBMAINLOOP
#include <glib.h>
#include "ivyglibloop.h"
#endif
#ifdef GLUTMAINLOOP
#include "ivyglutloop.h"
#endif
#ifdef IVYMAINLOOP
#include "ivyloop.h"
#endif
#include "ivysocket.h"
#include "ivychannel.h"
#include "ivybind.h" /* to test regexp before passing to BinMsg */
#include "ivy.h"
#include "timer.h"
#ifdef XTMAINLOOP
#include <X11/Intrinsic.h>
XtAppContext cntx;
#endif

int app_count = 0;
int wait_count = 0;
int fbindcallback = 0;
int filter_count = 0;
const char *filter[4096];
char *classes;

void DirectCallback(IvyClientPtr app, void *user_data, int id, char *msg ) 
{
	printf("%s sent a direct message, id=%d, message=%s\n",
	    IvyGetApplicationName(app),id,msg);
}


void PongCallback (IvyClientPtr app, int roundTripOrTimout)
{
	if (roundTripOrTimout >= 0) {
	  printf ("%s respond to ping in %.3f ms\n", IvyGetApplicationName(app),
		  roundTripOrTimout/1000.0);
	} else {
	  printf ("%s ping timout after %.3f ms\n", IvyGetApplicationName(app),
		  -roundTripOrTimout/1000.0);
	}
}

void Callback (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
	int i;
	printf ("%s sent ",IvyGetApplicationName(app));
	for  (i = 0; i < argc; i++)
			printf(" '%s'",argv[i]);
	printf("\n");
}

char * Chop(char *arg)
{
  	size_t len;
	if (arg==NULL) return arg;
	len=strlen(arg)-1;
	if ((*(arg+len))=='\n') *(arg+len)=0;
	return arg;
}

void HandleStdin (Channel channel, IVY_HANDLE fd, void *data)
{
	static const char *separator = "#";
	char buf[4096];
	char *line;
	char *cmd;
	char *arg;
	int id;
	IvyClientPtr app;
	int err;
	line = fgets(buf, 4096, stdin);
	if  (!line)	{

		IvyChannelRemove (channel);
		IvyStop();
		return;
	}
	if  (*line == '.') {
		cmd = strtok (line, ".: \n");

		if  (strcmp (cmd, "die") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
					IvySendDieMsg (app);
					else printf ("No Application %s!!!\n",arg);
			}

		} else if (strcmp(cmd, "dieall-yes-i-am-sure") == 0) {
			arg = IvyGetApplicationList(separator);
			arg = strtok (arg, separator);
			while  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
				{
					printf ("Killing '%s'...\n",arg);
					IvySendDieMsg (app);
				}
				else
					printf ("No Application %s!!!\n",arg);
				arg = strtok (NULL, separator);
			}
			
		} else if (strcmp(cmd,  "bind") == 0) {
		  arg = strtok (NULL, "'");
		  Chop(arg);
		  if  (arg) {
		    IvyBinding binding;
		    const char *errbuf;
		    int erroffset;
		    binding = IvyBindingCompile(arg, & erroffset, & errbuf);
		    if (binding==NULL) {
			  printf("Error compiling '%s', %s, not bound\n", arg, errbuf);
		    } else {
			  IvyBindingFree( binding );
			  IvyBindMsg (Callback, NULL, "%s", arg);
		    }
		  }

		} else if  (strcmp(cmd,  "where") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app)
					printf ("Application %s on %s\n",arg, IvyGetApplicationHost (app));
					else printf ("No Application %s!!!\n",arg);
			}
		} else if  (strcmp(cmd, "direct") == 0) {
			arg = strtok (NULL, " \n");
			if  (arg) {
				app = IvyGetApplication (arg);
				if  (app) {
					arg = strtok (NULL, " ");
					id = atoi (arg) ;
					arg = strtok (NULL, "'");
					IvySendDirectMsg (app, id, Chop(arg));
				} else
					printf ("No Application %s!!!\n",arg);
			}
			
		} else if  (strcmp(cmd, "who") == 0) {
			printf("Apps: %s\n", IvyGetApplicationList(","));

		} else if  (strcmp(cmd, "ping") == 0) {
		  arg = strtok (NULL, " \n");
		  if  (arg) {
		    app = IvyGetApplication (arg);
		    if  (app) {
		      IvySendPing (app);
		    }
		    else 
		      printf ("No Application %s!!!\n",arg);
		  }
		} else if  (strcmp(cmd, "help") == 0) {
			fprintf(stderr,"Commands list:\n");
			printf("	.help						- this help\n");
			printf("	.quit						- terminate this application\n");
			printf("	.die appname				- send die msg to appname\n");
			printf("	.dieall-yes-i-am-sure		- send die msg to all applis\n");
			printf("	.direct appname	id 'arg'	- send direct msg to appname\n");
			printf("	.ping appname	                - send ping to appname\n");
			printf("	.where appname				- on which host is appname\n");
			printf("	.bind 'regexp'				- add a msg to receive\n");
			printf("	.showbind					- show bindings \n");
			
			printf("	.who				- who is on the bus\n");
		} else if  (strcmp(cmd, "showbind") == 0) {
		  if (!fbindcallback) {
		    IvySetBindCallback(IvyDefaultBindCallback, NULL);
		    fbindcallback=1;
		  } else {
		    IvySetBindCallback(NULL, NULL);
		    fbindcallback=0;
		  }
		} else if  (strcmp(cmd, "quit") == 0) {
			exit(0);
		}
	} else {
		cmd = strtok (buf, "\n");
		err = IvySendMsg ("%s", cmd);
		printf("-> Sent to %d peer%s\n", err, err == 1 ? "" : "s");
	}
}

void ApplicationCallback (IvyClientPtr app, void *user_data, IvyApplicationEvent event)
{
	const char *appname;
	const char *host;
/*	char **msgList;*/
	appname = IvyGetApplicationName (app);
	host = IvyGetApplicationHost (app);
	switch  (event)  {

	case IvyApplicationConnected:
		app_count++;
		printf("%s connected from %s\n", appname,  host);
/*		printf("Application(%s): Begin Messages\n", appname);*/
/* double usage with -s flag remove it 
		msgList = IvyGetApplicationMessages (app);
		while (*msgList )
			printf("%s subscribes to '%s'\n",appname,*msgList++);
*/
/*		printf("Application(%s): End Messages\n",appname);*/
#ifndef WIN32
/* Stdin not compatible with select , select only accept socket */
		if  (app_count == wait_count)
		  IvyChannelAdd (0, NULL, NULL, HandleStdin, NULL);
#endif
		break;

	case IvyApplicationDisconnected:
		app_count--;
		printf("%s disconnected from %s\n", appname,  host);
		break;

	default:
		printf("%s: unkown event %d\n", appname, event);
		break;
	}
}
void IvyPrintBindCallback( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event)
{
        switch ( event )  {
        case IvyAddBind:
                if ( fbindcallback )
					printf("Application: %s on %s add regexp %d : %s\n", 
						IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
                break;
        case IvyRemoveBind:
                if ( fbindcallback )
					printf("Application: %s on %s remove regexp %d :%s\n",
						IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
                break;
        case IvyFilterBind:
                printf("Application: %s on %s as been filtred regexp %d :%s\n",
					IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
                break;
        case IvyChangeBind:
                if ( fbindcallback )
					printf("Application: %s on %s change regexp %d : %s\n", 
						IvyGetApplicationName( app ), IvyGetApplicationHost(app), id, regexp);
                break;
        default:
                printf("Application: %s unkown event %d\n",IvyGetApplicationName( app ), event);
                break;
        }
}


#ifdef IVYMAINLOOP
void TimerCall(TimerId id, void *user_data, unsigned long delta)
{
	printf("Timer callback: %ld delta %lu ms\n", (long)user_data, delta);
	IvySendMsg ("TEST TIMER %ld", (long) user_data);
	/*if  ((int)user_data == 5) TimerModify (id, 2000);*/
}
#endif
#ifdef GLUTMAINLLOP
void
display(void)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glFlush();
}
#endif

void BindMsgOfFile( const char * regex_file )
{
	char line[4096];
	size_t size;
	FILE* file;
	file = fopen( regex_file, "r" );
	if ( !file ) {
		perror( "Regexp file open ");
		return;
	}
	while( !feof( file ) )
	{
	if ( fgets( line, sizeof(line), file ) )
		{
		size = strlen(line);
		if ( size > 1 )
			{
			line[size-1] = '\0'; /* supress \n */
			IvyBindMsg (Callback, NULL, "%s", line);
			}
		}
	}
}
void BuildFilterRegexp()
{
	char *word=strtok( classes, "," );
	while ( word != NULL && (filter_count < 4096 ))
	{
	filter[filter_count++] = word;
	word = strtok( NULL, ",");
	}
	if ( filter_count )
	IvySetFilter( filter_count, filter );
}
int main(int argc, char *argv[])
{
	int c;
	int timer_test = 0;
	char busbuf [1024] = "";
	const char* bus = 0;
	const char* regex_file = 0;
	char agentnamebuf [1024] = "";
	const char* agentname = DEFAULT_IVYPROBE_NAME;
	char agentready [1024] = "";
	const char* helpmsg =
	  "[options] [regexps]\n\t-b bus\tdefines the Ivy bus to which to connect to, defaults to 127:2010\n"
	  "\t-t\ttriggers the timer test\n"
	  "\t-n name\tchanges the name of the agent, defaults to IVYPROBE\n"
	  "\t-v\tprints the ivy relase number\n\n"
	  "regexp is a Perl5 compatible regular expression (see ivyprobe(1) and pcrepattern(3) for more info\n"
	  "use .help within ivyprobe\n"
	  "\t-s bindcall\tactive the interception of regexp's subscribing or unscribing\n"
	  "\t-f regexfile\tread list of regexp's from file one by line\n"
	  "\t-c msg1,msg2,msg3,...\tfilter the regexp's not beginning with words\n"
	  ;
	while ((c = getopt(argc, argv, "vn:d:b:w:t:sf:c:")) != EOF)
			switch (c) {
			case 'b':
				strcpy (busbuf, optarg);
				bus = busbuf;
				break;
			case 'w':
				wait_count = atoi(optarg) ;
				break;
			case 'f':
				regex_file = optarg ;
				break;
			case 'n':
				strcpy(agentnamebuf, optarg);
				agentname=agentnamebuf;
			case 'v':
				printf("ivy c library version %d.%d\n",IVYMAJOR_VERSION,IVYMINOR_VERSION);
				break;
			case 't':
			        timer_test = 1;
			        break;
			case 's':
			        fbindcallback=1;
				break;
			case 'c':
			        classes= strdup(optarg);
				break;
			default:
				printf("usage: %s %s",argv[0],helpmsg);
				exit(1);
			}
	sprintf(agentready,"%s Ready",agentname);

	/* Mainloop management */
#ifdef XTMAINLOOP
	/*XtToolkitInitialize();*/
	cntx = XtCreateApplicationContext();
	IvyXtChannelAppContext (cntx);
#endif
#ifdef GLUTMAINLLOOP
	glutInit(&argc, argv);
	glutCreateWindow("IvyProbe Test");
	glClearColor(0.49, 0.62, 0.75, 0.0);
	glutDisplayFunc(display);
#endif
	IvyInit (agentname, agentready, ApplicationCallback,NULL,NULL,NULL);
	IvySetBindCallback(IvyPrintBindCallback, NULL);
	IvySetPongCallback(PongCallback);		        
	IvyBindDirectMsg( DirectCallback,NULL);
	if ( classes )
		BuildFilterRegexp();
	if ( regex_file )
		BindMsgOfFile( regex_file );
	for  (; optind < argc; optind++)
	{
		printf("Binding to '%s'\n", argv[optind] );
		IvyBindMsg (Callback, NULL, "%s", argv[optind]);
	}

	
#ifdef WIN32
	printf("Stdin not compatible with select , select only accept socket on Windows\n");	
#else
/* Stdin not compatible with select , select only accept socket */
	if  (wait_count == 0)
	  IvyChannelAdd (0, NULL, NULL, HandleStdin, NULL);
#endif
	
	IvyStart (bus);

	if  (timer_test) {
#ifdef IVYMAINLOOP
		TimerRepeatAfter (TIMER_LOOP, 1000, TimerCall, (void*)1);
		TimerRepeatAfter (5, 5000, TimerCall, (void*)5);
#endif
	}

#ifdef XTMAINLOOP
	XtAppMainLoop (cntx);
#endif
#ifdef GLIBMAINLOOP
	{
	  GMainLoop *ml =  g_main_loop_new(NULL, FALSE);
	  g_main_loop_run(ml);
	}
#endif
#ifdef GLUTMAINLOOP
	glutMainLoop();
#endif

#ifdef IVYMAINLOOP
	IvyMainLoop ();
#endif
	return 0;
}

