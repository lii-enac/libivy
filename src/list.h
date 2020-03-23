/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2008
 *	Centre d'�tudes de la Navigation A�rienne
 *
 * 	Simple lists in C
 *
 *	Authors: Fran�ois-R�gis Colin <fcolin@cena.dgac.fr>
 *
 *	$Id: list.h 3301 2008-05-20 13:58:45Z fcolin $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
#if (__GNUC__ >= 3)
#define TYPEOF(p) typeof (p)
#else
#define  TYPEOF(p) void *
#endif

#define IVY_LIST_ITER( list, p, cond ) \
	p = list; \
	while ( p && (cond) ) p = p->next 



#define IVY_LIST_REMOVE( list, p ) \
	{ \
	TYPEOF(p)  toRemove; \
	if ( list == p ) \
		{ \
		list = p->next; \
		free(p);\
		} \
		else \
		{\
		toRemove = p;\
		IVY_LIST_ITER( list, p, ( p->next != toRemove ));\
		if ( p )\
			{\
			/* somme tricky swapping to use a untyped variable */\
			TYPEOF(p) suiv; \
			TYPEOF(p) prec = p;\
			p = toRemove;\
			suiv = p->next;\
			p = prec;\
			p->next = suiv;\
			free(toRemove);\
			}\
		} \
	}
/* 
on place le code d'initialisation de l'objet entre START et END 
pour eviter de chainer un objet non initialise
*/

		  /*		printf ("sizeof (*"#p") = %d\n", sizeof( *p )); \ */ \

#define IVY_LIST_ADD_START(list, p ) \
        if ((p = (TYPEOF(p)) (malloc( sizeof( *p ))))) \
		{ \
		memset( p, 0 , sizeof( *p ));

#define IVY_LIST_ADD_END(list, p ) \
		p->next = list; \
		list = p; \
		} \
		else \
		{ \
		perror( "IVY LIST ADD malloc" ); \
		exit(0); \
		}

#define IVY_LIST_EACH( list, p ) \
	for ( p = list ; p ; p = p -> next )

/* le pointeur next est sauvegarder dans nxt avant l'iteration */
#define IVY_LIST_EACH_SAFE( list, p, nxt )\
for ( p = list ; (nxt = p ? p->next: p ),p ; p = nxt )

#define IVY_LIST_IS_EMPTY( list ) \
  list == NULL

#define IVY_LIST_EMPTY( list ) \
	{ \
	TYPEOF(list) p; \
	while( list ) \
		{ \
		p = list;\
		list = list->next; \
		free(p);\
		} \
	}
