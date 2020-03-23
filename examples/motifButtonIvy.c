#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <Xm/PushB.h>
#include <Ivy/ivy.h>
#include <Ivy/ivyxtloop.h>

void myMotifCallback(Widget w,XtPointer client_d,XtPointer call_d){
  // sends the string on the bus
  IvySendMsg (*((char**)client_d));
}

void textCallback(IvyClientPtr app, void *user_data, int argc, char *argv[]){
  printf("%s sent %s\n",IvyGetApplicationName(app),argv[0]);
  // updates the string we want to send
  *((char **)user_data)=argv[0];
}

void DieCallback (IvyClientPtr app, void *data, int id){ exit(0); }

int main(int argc,char *argv[]){
  Widget toplevel,pushb;
  XtAppContext app_context;
  Arg myargs[10];
  char *bus=getenv("IVYBUS");
  char *tosend="foo";
  toplevel=XtAppInitialize(&app_context,"Ivy Button",NULL,0,&argc,argv,NULL,myargs,0);
  pushb=XmCreatePushButton(toplevel,"send message",myargs,1);
  XtManageChild(pushb);
  XtAddCallback(pushb,XmNactivateCallback,myMotifCallback,&tosend);
  XtRealizeWidget(toplevel);
  IvyXtChannelAppContext(app_context);
  IvyInit("IvyMotif","IvyMotif connected",NULL,NULL,DieCallback,NULL);
  IvyBindMsg(textCallback,&tosend,"^Ivy Button text=(.*)");
  IvyStart(bus);
  XtAppMainLoop(app_context);
  return 0;
}
