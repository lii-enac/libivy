#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int             optind = 1;
char            *optarg = (char *)NULL;

int	getopt (int argc, char **argv, char *optstring)
{
    int		cur_option;		/* Current option		*/
    char	*cp;			/* Character pointer		*/
    static int	GetOptionPosition = 1;

    if (GetOptionPosition == 1)
    {

/* Check for out of range, correct start character and not single */

	if ((optind >= argc) || (*argv[optind] != '-') || !argv[optind][1])
	    return EOF;

	if (!strcmp (argv[optind], "--"))
	    return EOF;
    }

/* Get the current character from the current argument vector */

    cur_option = argv[optind][GetOptionPosition];

/* Validate it */

    if ((cur_option == ':') ||
	((cp = strchr (optstring, cur_option)) == (char *)NULL))
    {

/* Move to the next offset */

	if (!argv[optind][++GetOptionPosition])
	{
	    optind++;
	    GetOptionPosition = 1;
	}

	return '?';
    }

/* Parameters following ? */

    optarg = (char *)NULL;

    if (*(++cp) == ':')
    {
	if (argv[optind][GetOptionPosition + 1])
	    optarg = &argv[optind++][GetOptionPosition + 1];

	else if (++optind >= argc)
	{
	    optarg = (char *)NULL;
	    GetOptionPosition = 1;
	    return '?';
	}

	else
	    optarg = argv[optind++];

	GetOptionPosition = 1;
    }

    else if (!argv[optind][++GetOptionPosition])
    {
	GetOptionPosition = 1;
	optind++;
    }

    return cur_option;
}
