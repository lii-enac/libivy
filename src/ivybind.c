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
#include <limits.h>

#ifdef WIN32
#ifndef __MINGW32__
#include <crtdbg.h>
#endif
#endif


#ifdef USE_PCRE_REGEX
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#else  /* we don't USE_PCRE_REGEX */
#define MAX_MSG_FIELDS 200
#include <regex.h>
#endif /* USE_PCRE_REGEX */

#include "list.h"
#include "ivybind.h"

static char err_buf[4096];
#ifndef USE_PCRE_REGEX
static int err_offset;
#endif

struct _binding {
#ifdef USE_PCRE_REGEX
	pcre2_code *regexp;
	pcre2_match_data *match_data;
	int nb_match;
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
	IvyBinding bind=0;
#ifdef USE_PCRE_REGEX
    int error_code;
    PCRE2_SIZE error_offset;
    pcre2_code *regexp = pcre2_compile((PCRE2_SPTR)expression,
        PCRE2_ZERO_TERMINATED, PCRE_OPT, &error_code, &error_offset, NULL);
    if (regexp != NULL)
    {
        bind = (IvyBinding)calloc(1, sizeof(struct _binding));
        if (!bind)
        {
            pcre2_code_free(regexp);
            perror("IvyBindingCompile malloc error: ");
            exit(-1);
        }
        bind->regexp = regexp;
        bind->match_data = pcre2_match_data_create_from_pattern(regexp, NULL);
        if (!bind->match_data)
        {
            pcre2_code_free(regexp);
            free(bind);
            fprintf(stderr, "IvyBindingCompile: cannot allocate match data\n");
            exit(-1);
        }
    }
    else
    {
        pcre2_get_error_message(error_code, (PCRE2_UCHAR *)err_buf, sizeof(err_buf));
        /* Keep the public Ivy API's int offset. */
        *erroffset = error_offset > INT_MAX ? INT_MAX : (int)error_offset;
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
  pcre2_match_data_free(bind->match_data);
  pcre2_code_free(bind->regexp);
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
    bind->nb_match = 0;
    nb_match = pcre2_match(bind->regexp, (PCRE2_SPTR)message,
        strlen(message), 0, 0, bind->match_data, NULL);
    if (nb_match < 1) return 0; /* no match */
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
    PCRE2_SIZE *ovector;
    PCRE2_SIZE start, end;
    *arglen = 0;
    *arg = NULL;
    if (bind->nb_match < 1 || argnum < 0 ||
        (uint32_t)argnum >= pcre2_get_ovector_count(bind->match_data))
        return;
    ovector = pcre2_get_ovector_pointer(bind->match_data);
    start = ovector[2 * (size_t)argnum];
    end = ovector[2 * (size_t)argnum + 1];
    /* Optional groups have no substring; never form message + PCRE2_UNSET. */
    if (start == PCRE2_UNSET || end == PCRE2_UNSET ||
        end < start || end - start > INT_MAX)
        return;
    *arglen = (int)(end - start);
    *arg = message + start;
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
	IvyBindingFree(token_extract);
	token_extract = NULL;
}
