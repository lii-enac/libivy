/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2006
 *	Centre d'Études de la Navigation Aérienne
 *
 *	Bind syntax for extracting message comtent 
 *  using regexp or other 
 *
 *	Authors: François-Régis Colin <fcolin@cena.fr>
 *
 *	$Id: ivybind.h 3475 2011-02-08 16:38:55Z fcolin $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */
/* Module de gestion de la syntaxe des messages Ivy */

typedef struct _binding *IvyBinding;

#ifdef __cplusplus
extern "C" {
#endif

/* Mise en place des Filtrages */
int IvyBindingGetFilterCount();
void IvyBindingSetFilter( int argc, const char ** argv );
void IvyBindingAddFilter( const char * argv );
void IvyBindingRemoveFilter( const char * arg );
int IvyBindingFilter( const char *expression );
void IvyBindindFilterCheck( const char *message );

/* Creation, Compilation */
IvyBinding IvyBindingCompile( const char *expression, int *erroffset, const char **errmessage );
void IvyBindingFree( IvyBinding _bind );

/* Execution , extraction */
int IvyBindingExec( IvyBinding _bind, const char * message );
/* Get Argument */
void IvyBindingMatch( IvyBinding _bind, const char *message, int argnum, int *arglen, const char **arg );

/*
Liberation de memoire en fin d'execution 
*/
void IvyBindingTerminate();

#ifdef __cplusplus
}
#endif

