/*
 *	IvyThroughput
 *
 *	Copyright (C) 2008
 *	Centre d'�tudes de la Navigation A�rienne
 */


// g++ ivythroughput.cpp -o ivythroughput -L/usr/local/lib64/ -Wl,-rpath,/usr/local/lib64/ -livy -lpcrecpp

/* SCENARIO

  � traitement des options :
    -v (affi version) -b bus, -r regexp file, -m message file, -n : nombre de recepteurs
    -t [type de test => ml (memory leak), tp (throughput), dx (gestion des deconnexions
       intenpestives)

    test memory leak
  � fork d'un emetteur et d'un (ou plusieurs) recepteur : le recepteur s'abonne � toutes les regexps,
    se desabonne, se reabonne etc etc en boucle : on teste que l'empreinte m�moire
    de l'emetteur ne grossisse pas

    test throughput :
  � fork d'un emetteur et d'un ou plusieurs recepteurs : les recepteurs s'abonnent � toutes les regexps
  � l'emetteur envoie en boucle tous les messages du fichier de message
  � l'emetteur note le temps d'envoi des messages
  � l'emetteur envoie un die all et quitte

  UTILISATION TYPIQUES:
  pour la deconnexion intenpestive :
  ivythroughput -R 500 -t dx -d 40 -n 2
*/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <signal.h>

#include "ivysocket.h"
#include "ivy.h"
#include "timer.h"
#include "ivyloop.h"
#include <pcrecpp.h>

#include <string>
#include <list>
#include <map>

typedef std::list<std::string>  ListOfString;
typedef std::list<pid_t> ListOfPid;
typedef std::map<unsigned int, bool> MapUintToBool;
typedef std::list<MsgRcvPtr> ListOfMsgRcvPtr;

typedef struct {
  unsigned int currentBind;
  unsigned int totalBind;
} InfoBind;
typedef std::map<pcrecpp::string, InfoBind> MapBindByClnt;

#define MILLISEC 1000.0

typedef enum  {memoryLeak1, memoryLeak2, throughput, disconnect} KindOfTest ;


typedef struct {
  ListOfMsgRcvPtr	*bindIdList;
  const ListOfString    *regexps;
  unsigned int		inst;
} MlDataStruct;



extern char *optarg;
extern int   optind, opterr, optopt;

void recepteur_tp (const char* bus, KindOfTest kod, unsigned int inst,
		   const ListOfString& regexps, unsigned int exitAfter);
void recepteur_ml (const char* bus, KindOfTest kod, unsigned int inst,
		   const ListOfString& regexps);
void emetteur (const char* bus, KindOfTest kod, int testDuration,
	       const ListOfString& messages, int regexpSize);

bool getMessages (const char*fileName, ListOfString &messages, unsigned int numMess);
bool getRegexps (const char*fileName, ListOfString &regexps, unsigned int numReg);

double currentTime();
void binCB( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event ) ;
void congestCB ( IvyClientPtr app, void *user_data, IvyApplicationEvent event ) ;
void stopCB (TimerId id, void *user_data, unsigned long delta);
void sendAllMessageCB (TimerId id, void *user_data, unsigned long delta);
void recepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void recepteurCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void startOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void endOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[]);
void desabonneEtReabonneCB (TimerId id, void *user_data, unsigned long delta);
void changeRegexpCB (TimerId id, void *user_data, unsigned long delta);
void exitCB (TimerId id, void *user_data, unsigned long delta);
void doNothingAndSuicideCB (TimerId id, void *user_data, unsigned long delta);

unsigned int nbMess=0, nbReg=0, numClients =1, globalInst, numRegexps=1e6, numMessages=1e6;
MapUintToBool   recReady;
KindOfTest   kindOfTest = throughput;
bool	     regexpAreUniq = false;

int main(int argc, char *argv[])
{
  int c;
  int testDuration = 10;
  char *bus ;
  char  regexpFile[1024] = "testivy/regexp.txt";
  char  messageFile[1024] = "testivy/messages.ivy";
  ListOfString messages, regexps;
  pid_t        pid;
  ListOfPid    recPid;

  const char* helpmsg =
    "[options] \n"
    "\t -b bus\tdefines the Ivy bus to which to connect to, defaults to 127:2010\n"
    "\t -v \t prints the ivy relase number\n\n"
    "\t -t \t type of test :  ml or ml2 (memory leak) or tp (throughput)\n"
    "\t -r \t regexfile\tread list of regexp's from file (default to testivy/regexp.txt)\n"
    "\t -R \t restrict to R firsts regexps instead of all the regexp in the regexp file \n"
    "\t -p \t each client will postpend regexp with uniq string to "
             "simulate N clients with differents regexps\n"
    "\t -m \t messageFile\tread list of messages from file (default to testivy/messages.ivy)\n"
    "\t -M \t restrict to M firsts messages instead of all the message in the message file \n"
    "\t -n \t number of clients\n"
    "\t -d \t duration of the test in seconds\n" ;


  if (getenv("IVYBUS") != NULL) {
    bus = strdup (getenv("IVYBUS"));
  } else {
    bus = strdup ("127.0.0.1:2000") ;
  }

  while ((c = getopt(argc, argv, "vpb:r:m:R:M:n:t:d:")) != EOF)
    switch (c) {
    case 'b':
      strcpy (bus, optarg);
      break;
    case 'v':
      printf("ivy c library version %d.%d\n",IVYMAJOR_VERSION, IVYMINOR_VERSION);
      break;
    case 'p':
      regexpAreUniq = true;
      break;
    case 't':
      if (strcasecmp (optarg, "ml") == 0) {
	kindOfTest = memoryLeak1;
      } else if (strcasecmp (optarg, "ml1") == 0) {
	kindOfTest = memoryLeak1;
      } else if (strcasecmp (optarg, "ml2") == 0) {
	kindOfTest = memoryLeak2;
      } else if (strcasecmp (optarg, "tp") == 0) {
	kindOfTest = throughput;
      } else if (strcasecmp (optarg, "dx") == 0) {
	kindOfTest = disconnect;
      } else {
	printf("usage: %s %s",argv[0],helpmsg);
	exit(1);
      }
      break;
    case 'r':
      strcpy (regexpFile, optarg);
      break;
    case 'm':
      strcpy (messageFile, optarg);
      break;
    case 'R':
       numRegexps = atoi (optarg);
      break;
    case 'M':
       numMessages = atoi (optarg);
      break;
    case 'n':
      numClients = atoi (optarg);
      break;
    case 'd':
      testDuration = atoi (optarg);
      break;
    default:
      printf("usage: %s %s",argv[0],helpmsg);
      exit(1);
    }

  if (!getRegexps (regexpFile, regexps, numRegexps))
    {return (1);};

  if (kindOfTest != memoryLeak1) {
    if (!getMessages (messageFile, messages, numMessages))
      {return (1);};
  }



  for (unsigned int i=0; i< numClients; i++) {
    if ((pid = fork ()) == 0) {
      // fils
#ifdef __APPLE__
      usleep (200 * 1000);
#endif
      switch  (kindOfTest) {
      case throughput :
	recepteur_tp (bus, kindOfTest, i, regexps, 0);
	break;
      case memoryLeak1 :
	recepteur_ml (bus, kindOfTest, i, regexps);
	break;
      case memoryLeak2 :
	recepteur_tp (bus, kindOfTest, i, regexps, testDuration-5);
	break;
      case disconnect :
	recepteur_tp (bus, kindOfTest, i, regexps, testDuration);
	break;
      }
      exit (0);
    } else {
      recPid.push_back (pid);
      recReady[i]=false;
    }
  }

  emetteur  (bus, kindOfTest, testDuration, messages, regexps.size());

  ListOfPid::iterator  iter;
  for (iter=recPid.begin(); iter != recPid.end(); iter++) {
    kill (*iter, SIGTERM);
  }

  for (iter=recPid.begin(); iter != recPid.end(); iter++) {
    waitpid (*iter, NULL, 0);
  }

  return (0);
}




/*
#                                           _      _
#                                          | |    | |
#                  ___   _ __ ___     ___  | |_   | |_     ___   _   _   _ __
#                 / _ \ | '_ ` _ \   / _ \ | __|  | __|   / _ \ | | | | | '__|
#                |  __/ | | | | | | |  __/ \ |_   \ |_   |  __/ | |_| | | |
#                 \___| |_| |_| |_|  \___|  \__|   \__|   \___|  \__,_| |_|
*/
void emetteur (const char* bus, KindOfTest kod, int testDuration,
	       const ListOfString& messages, int regexpSize)
{
  printf ("DBG> emetteur start, pid=%d\n", getpid());
  IvyInit ("IvyThroughputEmit", "IvyThroughputEmit Ready", congestCB, NULL,NULL,NULL);
  //  double origin = currentTime();


  IvySetBindCallback (binCB, (void *) (regexpSize+2l));
  IvyBindMsg (recepteurReadyCB, (void *) &messages,
	      "^IvyThroughputReceive_(\\d+)\\s+Ready");

  TimerRepeatAfter (1, testDuration *1000, stopCB, NULL);

  IvyStart (bus);
  IvyMainLoop ();
}



/*
#                                             _ __    _
#                                            | '_ \  | |
#                 _ __    ___    ___    ___  | |_) | | |_     ___   _   _   _ __
#                | '__|  / _ \  / __|  / _ \ | .__/  | __|   / _ \ | | | | | '__|
#                | |    |  __/ | (__  |  __/ | |     \ |_   |  __/ | |_| | | |
#                |_|     \___|  \___|  \___| |_|      \__|   \___|  \__,_| |_|
*/
void recepteur_tp (const char* bus, KindOfTest kod, unsigned int inst,
		const ListOfString& regexps,  unsigned int exitAfter)
{
  std::string agentName = "IvyThroughputReceive_";
  std::stringstream stream ;
  stream << inst;
  agentName +=  stream.str();
  std::string agentNameReady (agentName + " Ready");
  //double origin = currentTime();
  globalInst = inst;

  printf ("DBG> recepteur_%d start, pid=%d\n", inst, getpid());
  IvyInit (agentName.c_str(), agentNameReady.c_str(), congestCB, NULL,NULL,NULL);

  unsigned int debugInt = 0;
  ListOfString::const_iterator  iter;
  for (iter=regexps.begin(); iter != regexps.end(); iter++) {
    debugInt++;
    pcrecpp::string reg = *iter;
    if (regexpAreUniq) { ((reg += "(") += stream.str()) += ")?";}
    IvyBindMsg (recepteurCB, (void *) long(inst), "%s", reg.c_str());
  }
  IvyBindMsg (startOfSeqCB, NULL, "^start(OfSequence)");
  IvyBindMsg (endOfSeqCB, NULL, "^end(OfSequence)");

  if (kod == memoryLeak2) {
    TimerRepeatAfter (1, exitAfter*1000, exitCB, NULL);
  } else if  (kod == disconnect) {
    TimerRepeatAfter (1, exitAfter*1000/3, doNothingAndSuicideCB, (void *) long(exitAfter));
  }

  //usleep (inst * 50 * 1000);
  IvyStart (bus);
  IvyMainLoop ();
}

void recepteur_ml (const char* bus, KindOfTest kod, unsigned int inst,
		const ListOfString& regexps)
{
  std::string agentName = "IvyThroughputReceive_";
  std::stringstream stream ;
  stream << inst;
  agentName +=  stream.str();
  std::string agentNameReady (agentName + " Ready");
  //double origin = currentTime();
  globalInst = inst;
  static ListOfMsgRcvPtr bindIdList;
  static MlDataStruct mds;

  printf ("DBG> recepteur_%d start, pid=%d\n", inst, getpid());
  IvyInit (agentName.c_str(), agentNameReady.c_str(), congestCB, NULL,NULL,NULL);

  unsigned int debugInt = 0;
  ListOfString::const_iterator  iter;
  for (iter=regexps.begin(); iter != regexps.end(); iter++) {
    debugInt++;
    pcrecpp::string reg = *iter;
    if (regexpAreUniq) { (reg += " ") += stream.str();}
    bindIdList.push_back (IvyBindMsg (recepteurCB, (void *) long(inst), "%s", reg.c_str()));
  }
  IvyBindMsg (startOfSeqCB, NULL, "^start(OfSequence)");
  IvyBindMsg (endOfSeqCB, NULL, "^end(OfSequence)");

  mds.bindIdList = &bindIdList;
  mds.regexps = &regexps;
  mds.inst = inst;

  TimerRepeatAfter (1, 1000, desabonneEtReabonneCB, &mds);
  //TimerRepeatAfter (1, 1000, abonneEtDesabonneCB, &mds);

  IvyStart (bus);
  IvyMainLoop ();
}

// ===========================================================================



/*
#                  __ _          _      __  __                                 __ _
#                 / _` |        | |    |  \/  |                               / _` |
#                | (_| |   ___  | |_   | \  / |   ___   ___    ___     __ _  | (_| |   ___
#                 \__, |  / _ \ | __|  | |\/| |  / _ \ / __|  / __|   / _` |  \__, |  / _ \
#                  __/ | |  __/ \ |_   | |  | | |  __/ \__ \  \__ \  | (_| |   __/ | |  __/
#                 |___/   \___|  \__|  |_|  |_|  \___| |___/  |___/   \__,_|  |___/   \___|
*/

bool getMessages (const char*fileName, ListOfString &messages, unsigned int numMess)
{
  FILE *infile;
  char buffer [1024*64];
  pcrecpp::RE pcreg ("\"(.*)\"$");
  pcrecpp::string  aMsg;

  infile = fopen(fileName, "r");
  if (!infile) {
    fprintf (stderr, "impossible d'ouvrir %s en lecture\n", fileName);
    return false;
  }

  while ((fgets (buffer, sizeof (buffer), infile) != NULL) && (nbMess < numMess)) {
    if (pcreg.PartialMatch (buffer, &aMsg)) {
      messages.push_back (aMsg);
      nbMess++;
    }
  }
  fclose (infile);
  return (true);
}


/*
#                  __ _          _      _____            __ _                  _ __
#                 / _` |        | |    |  __ \          / _` |                | '_ \
#                | (_| |   ___  | |_   | |__) |   ___  | (_| |   ___  __  __  | |_) |  ___
#                 \__, |  / _ \ | __|  |  _  /   / _ \  \__, |  / _ \ \ \/ /  | .__/  / __|
#                  __/ | |  __/ \ |_   | | \ \  |  __/   __/ | |  __/  >  <   | |     \__ \
#                 |___/   \___|  \__|  |_|  \_\  \___|  |___/   \___| /_/\_\  |_|     |___/
*/
bool getRegexps (const char*fileName, ListOfString &regexps, unsigned int numReg)
{
  FILE *infile;
  char buffer [1024*64];
  pcrecpp::RE pcreg1 ("add regexp \\d+ : (.*)$");
  pcrecpp::RE pcreg2 ("(\\^.*)$");
  pcrecpp::string  aMsg;

  infile = fopen(fileName, "r");
  if (!infile) {
    fprintf (stderr, "impossible d'ouvrir %s en lecture\n", fileName);
    return false;
  }

  while ((fgets (buffer, sizeof (buffer), infile) != NULL) && (nbReg < numReg)) {
    if (pcreg1.PartialMatch (buffer, &aMsg)) {
      regexps.push_back (aMsg);
      nbReg++;
    } else if (pcreg2.PartialMatch (buffer, &aMsg)) {
      regexps.push_back (aMsg);
      nbReg++;
    }
  }
  fclose (infile);

  return (true);
}



/*
#                                              _______   _
#                                             |__   __| (_)
#                  ___   _   _   _ __   _ __     | |     _    _ __ ___     ___
#                 / __| | | | | | '__| | '__|    | |    | |  | '_ ` _ \   / _ \
#                | (__  | |_| | | |    | |       | |    | |  | | | | | | |  __/
#                 \___|  \__,_| |_|    |_|       |_|    |_|  |_| |_| |_|  \___|
*/
double currentTime()
{
  double current;

  struct timeval stamp;
  gettimeofday( &stamp, NULL );
  current = (double)stamp.tv_sec * MILLISEC + (double)(stamp.tv_usec/MILLISEC);
  return  current;
}


/*
#                 _       _             _____   ____
#                | |     (_)           / ____| |  _ \
#                | |__    _    _ __   | |      | |_) |
#                | '_ \  | |  | '_ \  | |      |  _ <
#                | |_) | | |  | | | | | |____  | |_) |
#                |_.__/  |_|  |_| |_|  \_____| |____/
*/
void binCB( IvyClientPtr app, void *user_data, int id, const char* regexp,  IvyBindEvent event )
{
  pcrecpp::string appName = IvyGetApplicationName( app );
  static MapBindByClnt bindByClnt;

  if (bindByClnt.find (appName) == bindByClnt.end()) {
    (bindByClnt[appName]).currentBind = 0;
    (bindByClnt[appName]).totalBind = (unsigned long) user_data;
  }

  switch ( event )
    {
    case IvyAddBind:
      (bindByClnt[appName]).currentBind ++;
      if ((bindByClnt[appName]).currentBind == (bindByClnt[appName]).totalBind) {
	printf("Application:%s ALL REGEXPS BINDED\n", appName.c_str());
      } else {
	//	printf("Application:%s bind [%d/%d]\n", appName.c_str(),
	//     (bindByClnt[appName]).currentBind, (bindByClnt[appName]).totalBind);
      }
      break;
    case IvyRemoveBind:
      if ((id % 10000) == 0) {
	printf("Application:%s bind '%d' REMOVED\n", appName.c_str(), id );
      }
      break;
    case IvyFilterBind:
      printf("Application:%s bind '%s' FILTRED\n", appName.c_str(), regexp );
      break;
    case IvyChangeBind:
      printf("Application:%s bind '%s' CHANGED\n", appName.c_str(), regexp );
      break;
    }
}



void congestCB ( IvyClientPtr app, void *user_data, IvyApplicationEvent event )
{
  pcrecpp::string appName = IvyGetApplicationName( app );

  switch ( event ) {
#if IVYMINOR_VERSION >= 11
  case IvyApplicationCongestion:
    printf("Application:%s : Congestion\n", appName.c_str());
    break;
  case IvyApplicationDecongestion:
    printf("Application:%s : DEcongestion\n", appName.c_str());
    break;
  case IvyApplicationFifoFull:
    printf("Application:%s : FIFO PLEINE, MESSAGES PERDUS !!!\n", appName.c_str());
    break;
#endif
  case IvyApplicationConnected:
#ifdef __APPLE__
    printf("Application:%s : Connected\n", appName.c_str());
#endif
  case IvyApplicationDisconnected:
#ifdef __APPLE__
    printf("Application:%s : Disconnected\n", appName.c_str());
#endif
    break;
  }
}




void stopCB (TimerId id, void *user_data, unsigned long delta)
{
 IvyStop ();
}


void sendAllMessageCB (TimerId id, void *user_data, unsigned long delta)
{
  ListOfString  *messages = (ListOfString *) user_data;
  double startTime = currentTime();
  unsigned int envoyes=0;

  IvySendMsg ("startOfSequence");
  ListOfString::iterator  iter;
  for (iter=messages->begin(); iter != messages->end(); iter++) {
    envoyes += IvySendMsg ("%s", (*iter).c_str());
  }
  IvySendMsg ("endOfSequence");

  printf ("[ivy %d.%d] envoyer [%d/%d] messages filtres par %d regexps a %d clients "
	  "prends %.1f secondes\n",
	  IVYMAJOR_VERSION, IVYMINOR_VERSION,
	  envoyes, nbMess, nbReg, numClients,
	  (currentTime()-startTime) / 1000.0) ;
  TimerRepeatAfter (1, 1000, sendAllMessageCB ,user_data);
}

void recepteurCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  //  unsigned long recN = (long) user_data;
  //  printf (".");
  //  if (!((argc == 1) && (strcmp (argv[0], "OfSequence")) == 0))
  nbMess++;
}


void recepteurReadyCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  ListOfString  *messages = (ListOfString *) user_data;
  unsigned int instance = atoi( *argv++ );

  recReady[instance] = true;
  bool readyToStart = true;

  for (unsigned int i=0; i< numClients; i++) {
    if (recReady[i]==false) {
      printf ("Emetteur :Recepteur[%d] OK,  manque recepteur [%d/%d]\n", instance, i, numClients-1);
      readyToStart = false;
    }
  }

  if (readyToStart == true) {
    if (kindOfTest != memoryLeak1) {
      TimerRepeatAfter (1, 100, sendAllMessageCB , messages);
      printf ("Emetteur : tous recepteurs prets : on envoie la puree !!\n");
    }
  } else {
    printf ("\n");
  }
}



void startOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  nbMess = 0;
}


void endOfSeqCB (IvyClientPtr app, void *user_data, int argc, char *argv[])
{
  //nbMess--;
  printf ("recepteur %d a recu %d messages\n", globalInst, nbMess);
}


void desabonneEtReabonneCB (TimerId id, void *user_data, unsigned long delta)
{
  MlDataStruct *mds = (MlDataStruct *) user_data;

  //  printf ("on entre dans desabonneEtReabonneCB\n");

  ListOfMsgRcvPtr::iterator  iter;

  // DESABONNE
  for (iter=mds->bindIdList->begin(); iter != mds->bindIdList->end(); iter++) {
    IvyUnbindMsg (*iter);
  }
  mds->bindIdList->clear ();

  // REABONNE
  ListOfString::const_iterator  iter2;
  for (iter2=mds->regexps->begin(); iter2 != mds->regexps->end(); iter2++) {
    pcrecpp::string reg = *iter2;
    mds->bindIdList->push_back (IvyBindMsg (recepteurCB, (void *) long(mds->inst),
					    "%s", reg.c_str()));
  }

  // CHANGE REGEXP
    for (iter=mds->bindIdList->begin(); iter != mds->bindIdList->end(); iter++) {
    IvyChangeMsg (*iter, "^Une regexp (BIDON)");
  }


  // DESABONNE
  for (iter=mds->bindIdList->begin(); iter != mds->bindIdList->end(); iter++) {
    IvyUnbindMsg (*iter);
  }
  mds->bindIdList->clear ();

  //TimerRepeatAfter (1, 1000, changeRegexpCB, mds);
}


void changeRegexpCB (TimerId id, void *user_data, unsigned long delta)
{
  MlDataStruct *mds = (MlDataStruct *) user_data;

  //  printf ("on entre dans abonneEtDesabonneCB\n");

  ListOfMsgRcvPtr::iterator  iter;


  for (iter=mds->bindIdList->begin(); iter != mds->bindIdList->end(); iter++) {
    IvyChangeMsg (*iter, "^Une regexp (BIDON)");
  }

  TimerRepeatAfter (1, 1000, changeRegexpCB, mds);
}

void exitCB (TimerId id, void *user_data, unsigned long delta)
{
  printf ("DBG> client exit\n");
  exit (0);
}

void doNothingAndSuicideCB (TimerId id, void *user_data, unsigned long delta)
{
  printf (".");
  long time = (long) user_data;
  time = ((time-2)/2) * 1000;
  printf ("DBG> client hanging for %ld milliseconds\n", time);
  usleep (time * 1000);
  printf ("DBG> client suicide\n");
  _exit(42);
  printf ("DBG> CECI NE DEVRAIT JAMAIS ETRE AFFICHE\n");
}
