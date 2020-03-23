/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Bind syntax for extracting message comtent 
 *  using regexp or other 
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id: ivybind.c 3627 2015-01-07 14:01:47Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
/* Module de gestion de la syntaxe des messages Ivy */
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h> 
#include <string.h>
#include <stdarg.h>

#ifdef WIN32
#ifndef __MINGW32__
#include <crtdbg.h>
#endif
#endif


#ifdef USE_PCRE_REGEX
#include <pcre.h>
#else  /* we don't USE_PCRE_REGEX */
#define MAX_MSG_FIELDS 200
#include <regex.h>
#endif /* USE_PCRE_REGEX */

#include "list.h"
#include "ivybind.h"

static int err_offset;

#ifdef USE_PCRE_REGEX
	static const char *err_buf;
#else  /* we don't USE_PCRE_REGEX */
	static char err_buf[4096];
#endif /* USE_PCRE_REGEX */

struct _binding {
#ifdef USE_PCRE_REGEX
	pcre *regexp;
	pcre_extra *inspect;
	int nb_match;
	int *ovector;
	int ovectorsize;
#else  /* we don't USE_PCRE_REGEX */
	regex_t regexp;						/* la regexp sous forme machine */
	regmatch_t match[MAX_MSG_FIELDS+1];	/* resultat du match */
#endif /* USE_PCRE_REGEX */
	};

/* classes de messages emis par l'application utilise pour le filtrage */
typedef struct _filtred_word * FiltredWordPtr;
struct _filtred_word {                       /* requete d'emission d'un client */
        FiltredWordPtr next;
        const char *word;           /* entete de regexp a conserver */
};

static FiltredWordPtr messages_classes =0 ;
/* regexp d'extraction du mot clef des regexp client pour le filtrage des regexp , ca va c'est clair ??? */
static IvyBinding token_extract =0;

IvyBinding IvyBindingCompile( const char * expression,  int *erroffset, const char **errmessage )
{
/*    static int called = 0;  */
/*    called ++;  */
/*    if ((called %1000) == 0) {  */
/*      printf ("DBG> IvyBindingCompile called =%d\n", called);  */
/*    }  */
	int capture_count=0;
	IvyBinding bind=0;
#ifdef USE_PCRE_REGEX
	pcre *regexp;
	regexp = pcre_compile(expression, PCRE_OPT,&err_buf,&err_offset,NULL);
	if ( regexp != NULL )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			if ( ! bind ) 
			{
				perror( "IvyBindingCompile malloc error: ");
				exit(-1);
			}
			memset( bind, 0, sizeof(*bind ) );
			bind->regexp = regexp;
			bind->inspect = pcre_study(regexp,0,&err_buf);
			if (err_buf!=NULL)
				{
					printf("Error studying %s, message: %s\n",expression,err_buf);
				}
			pcre_fullinfo( bind->regexp, bind->inspect, PCRE_INFO_CAPTURECOUNT, &capture_count );
			if ( bind->ovector != NULL )
				free( bind->ovector );
			// + 1 pour la capture totale
			bind->ovectorsize = (capture_count+1) * 3;
			bind->ovector = (int *) malloc( sizeof( int )* bind->ovectorsize);
		}
		else
		{
		*erroffset = err_offset;
		*errmessage = err_buf;
		printf("Error compiling '%s', %s\n", expression, err_buf);
		}
#else  /* we don't USE_PCRE_REGEX */
	regex_t regexp;
	int reg;
	reg = regcomp(&regexp, expression, REGCOMP_OPT|REG_EXTENDED);
	if ( reg == 0 )
		{
			bind = (IvyBinding)malloc( sizeof( struct _binding ));
			if ( ! bind ) 
			{
				perror( "IvyBindingCompile malloc error: ");
				exit(-1);
			}
			memset( bind, 0, sizeof(*bind ) );
			bind->regexp = regexp;
		}
		else
		{
		regerror (reg, &regexp, err_buf, sizeof(err_buf) );
		*erroffset = err_offset;
		*errmessage = err_buf;
		printf("Error compiling '%s', %s\n", expression, err_buf);
		}
#endif /* USE_PCRE_REGEX */
	return bind;
}

void IvyBindingFree( IvyBinding bind )
{
/*   static int called = 0; */
/*   called ++; */
/*   if ((called %1000) == 0) { */
/*     printf ("DBG> IvyBindingFree called =%d\n", called); */
/*   } */
	if( bind == NULL ) return;
#ifdef USE_PCRE_REGEX
	if ( bind->ovector != NULL )
				free( bind->ovector );
			
  if (bind->inspect!=NULL)
    pcre_free(bind->inspect);
  pcre_free(bind->regexp);
#else  /* we don't USE_PCRE_REGEX */
  regfree( &bind->regexp );
#endif /* USE_PCRE_REGEX */
  free ( bind );
}


int IvyBindingExec( IvyBinding bind, const char * message )
{
	int nb_match = 0;
	if( bind == NULL ) return nb_match;
#ifdef USE_PCRE_REGEX
	
	nb_match = pcre_exec(
					bind->regexp,
					bind->inspect,
					message,
					strlen(message),
					0, /* debut */
					0, /* no other regexp option */
					bind->ovector,
					bind->ovectorsize);
	if (nb_match<1) return 0; /* no match */
	bind->nb_match = nb_match;
#else  /* we don't USE_PCRE_REGEX */
	{
		int index;
	memset( bind->match, -1, sizeof(bind->match )); /* work around bug !!!*/
	nb_match = regexec (&bind->regexp, message, MAX_MSG_FIELDS, bind->match, 0);
	if (nb_match == REG_NOMATCH)
		return 0;
	for (index = 1; index < MAX_MSG_FIELDS; index++ )
	{
		if ( bind->match[index].rm_so != -1 )
			nb_match++;
	}
	}
#endif /* USE_PCRE_REGEX */
	return nb_match;
}

void IvyBindingMatch( IvyBinding bind, const char *message, int argnum, int *arglen, const char **arg)
{
	if( bind == NULL ) return;
#ifdef USE_PCRE_REGEX
	
		*arglen = bind->ovector[2*argnum+1]- bind->ovector[2*argnum];
		*arg =   message + bind->ovector[2*argnum];
#else  /* we don't USE_PCRE_REGEX */
	
	regmatch_t* p;

	p = &bind->match[argnum];
	if ( p->rm_so != -1 ) {
			*arglen = p->rm_eo - p->rm_so;
			*arg = message + p->rm_so;
	} else { /* ARG VIDE */
			*arglen = 0;
			*arg = NULL;
	}
#endif /* USE_PCRE_REGEX */

}

/*filter Expression Bind  */

void IvyBindingSetFilter( int argc, const char **argv)
{
	int i;
	for ( i = 0 ; i < argc; i++ )
	{
	IvyBindingAddFilter( argv[i] );
	}

}

void IvyBindingAddFilter( const char *arg)
{
	const char *errbuf;
	int erroffset;
	if ( arg )
	{
	FiltredWordPtr word=0;
	IVY_LIST_ADD_START( messages_classes, word );
	word->word = strdup(arg);
  IVY_LIST_ADD_END( messages_classes, word );

	}
	/* compile the token extraction regexp */
	if ( !token_extract )
	{
		token_extract = IvyBindingCompile("^\\^([a-zA-Z_0-9-]+).*", & erroffset, & errbuf);
		if ( !token_extract )
		{
			printf("Error compiling Token Extract regexp: %s\n", errbuf);
		}
	}
}
void IvyBindingRemoveFilter( const char *arg)
{
	FiltredWordPtr word=0;
	FiltredWordPtr next=0;
	IVY_LIST_EACH_SAFE( messages_classes, word, next )
	{
		if ( strcmp( arg, word->word) == 0 )
			{
			free( (void*)word->word );
			IVY_LIST_REMOVE( messages_classes, word );
			}
	}
}
	
int IvyBindingFilter(const char *expression)
{
	FiltredWordPtr word=0;
	int err;
	int regexp_ok = 1; /* accepte tout par default */
	int tokenlen = 0;
	const char *token;
	
	if ( *expression =='^' && messages_classes !=0 )
	{
		regexp_ok = 0;
		
		/* extract token */
		err = IvyBindingExec( token_extract, expression );
		if ( err < 1 ) return 1;
		IvyBindingMatch( token_extract, expression , 1, &tokenlen, &token );

		IVY_LIST_ITER( messages_classes, word, strncmp( word->word, token, tokenlen ) != 0);

		if (word) {
		    return 1; 
		    }
		  /*		  else { */
		  /*printf ("DBG> %s eliminé [%s]\n", token, expression); */
		  /*} */
		
 	}
	return regexp_ok;
}
/* recherche si le message commence par un mot clef de la table */
void IvyBindindFilterCheck( const char *message )
{
	FiltredWordPtr word=0;
	IVY_LIST_ITER( messages_classes, word, strcmp( word->word, message ) != 0);

	if (word)
		{
		return; 
	}
	
	fprintf(stderr,"*** WARNING *** message '%s' not sent due to missing keyword in filter table!!!\n", message );    
}
void IvyBindingTerminate()
{
	FiltredWordPtr word=0;
	FiltredWordPtr next=0;
	
	IVY_LIST_EACH_SAFE( messages_classes, word, next )
  {
  free((void*) word->word );
  }
	IVY_LIST_EMPTY( messages_classes );
	messages_classes = 0;
}
