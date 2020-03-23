/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Timers for select-based main loop
 *
 *	Authors: François-Régis Colin <fcolin@cena.dgac.fr>
 *
 *	$Id: timer.h 3591 2013-06-20 17:23:52Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#ifndef IVYTIMER_H
#define IVYTIMER_H


#ifdef __cplusplus
extern "C" {
#endif
	
/* Module de gestion des timers autour d'un select */

typedef struct _timer *TimerId;
typedef void (*TimerCb)( TimerId id , void *user_data, unsigned long delta );

/* API  le temps est en millisecondes */
#define TIMER_LOOP -1			/* timer en boucle infinie */
TimerId TimerRepeatAfter( int count, long timeout, TimerCb cb, void *user_data );

void TimerModify( TimerId id, long timeout );

void TimerRemove( TimerId id );


 //  implemetation of gettimeofday for windows
#ifdef WIN32
#include "time.h"
struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

/* Interface avec select */

struct timeval *TimerGetSmallestTimeout();

void TimerScan();
#ifdef __cplusplus
}
#endif
#endif

