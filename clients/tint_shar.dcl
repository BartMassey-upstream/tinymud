$!
$! This is a DCL shell archive
$! to de-archive it, just run it as a DCL script
$!
$ write sys$output "MAKEFILE."
$ copy sys$input MAKEFILE.
$ deck /dollars="SHARC_EOF_MARK"
CC	=	cc
LINK	=	link
LIBRARY	=	tint.opt/opt
RM	=	delete/nolog
PUR	=	purge/nolog

tint.exe: tint.obj tint_cmd.obj tint_filt.obj tint_netc.obj tint.opt 
	$(LINK) tint,tint_cmd,tint_filt,tint_netc,$(LIBRARY)

tint.obj: tint.h tint.c
	$(CC) tint

tint_cmd.obj: tint_cmd.c tint.h
	$(CC) tint_cmd

tint_filt.obj: tint_filt.c tint.h
	$(CC) tint_filt

tint_netc.obj: tint_netc.c tint.h
    $(CC) tint_netc

clean:
	$(RM) tint*.obj;*
	$(PUR) tint.exe
SHARC_EOF_MARK
$ write sys$output "TINT.C"
$ copy sys$input TINT.C
$ deck /dollars="SHARC_EOF_MARK"
/*

	Tinymud INTerface program, by me. Feb 16-17 '90
	I hereby release this code into the public domain, I make no guarantees
	as to this software's reliability. If you make some neato mods, drop me
	a note. Andrew Molitor aka bob.

		AMOLITOR@EAGLE.WESLEYAN.EDU

	If you find a bug, you're basically on your own. If??? Hah!

 */

/*    Changelog:
Andrew Molitor: (amolitor@eagle.wesleyan.edu):
  Some cosmetic changes both to code and output, with parameters for macros.

Nick Castellano (ncastellano@eagle.wesleyan.edu):
  Fixed a bug in \gag which caused a blank line to be printed every time a 
   gagged person spoke.  Fixed using global variable 'gagged' to supress
   the unwanted newline.
  Fixed restoration of full scrolling region on program termination.
  Fixed the mangled #define for RESTORE_CURSOR.  Not sure why this worked
   at all as it was, all run together.
  Cleaned up termination of \help and \dump.
  Installed \version command.
  Installed \spawn command.  This invlolved moving all the vt220 codes to
   tint.h, so tint.h is now #included in tint.c.  Appropriate change was made
   in the makefile.
  Installed \save and \load commands to save/load macro and gag lists.
  Installed support for startup file, fixed bugs in load routine.
  Named the program TINT, in the spirit of another famous program from 
   Wes.  I took this liberty since the program didn't seem to have a name.

Andrew Molitor (amolitor@eagle.wesleyan.edu):
  Change to expand_macro() in tint_filt.c to support embedding carriage 
   returns in a macro definition -- a single % is expanded the a CR LF
   pair, and a double %% expands to a single %.
  Fixed bug in expand_macro() which caused the cooked line to be prematurely
   terminated sometimes.
  Modified the handling of the ``found a #, but its not a parameter'' case of
   expand_macro().  The effect of this is to convert all pairs ## in a macro 
   definition into a single # in the expanded macro. This allows you to put 
   things like ##12591 into a macro. This expands to #12591. Without this mod,
   the first # would have been passed direct, the second, being followed by a
   digit (1) would be expanded as the first parameter to the macro (if any) 
   or discarded (if no parameters given).
  There's a printf("Done = TRUE") or something hidden in expand_macro() 
   somewhere. This was debugging code and has been deleted.

Kevin Rainier (krainier@eagle.wesleyan.edu):

  Added tint_netc.c -- allows input of host by either octets or name.  Allows
   input of port by either number or service (ie: smtp, telnet, but NOT
   "tinymud").
*/

#include <stdio.h>
#include <ctype.h>
#include <descrip.h>
#include <types.h>

/* These are includes for the Multinet routines. They may be hidden
in some directory like ``MULTINET_ROOT:[MULTINET.INCLUDES]'' on your
system  */

#include <netdb.h>
#include <in.h>
#include <iodef.h>
#include <socket.h>
#include "tint.h"

handle_output();

/* I/O status block for read requests */

unsigned short iosb[4];

/* current character co-ordinates in the output window */

int out_x,out_y;

/* a generic word buffer for word wrapping */

char word[80];

/* buffer for raw output from tinymud  */

char buffer[5000];

/* Channel number for my socket to TinyMud */

unsigned short tmud_chan;

/* gag flag */

int gagged;

/* load handler to load default.mud */

extern int load_handler();

/********************************************

	DO IT TO IT

*********************************************/

main(argc,argv)

int argc;
char *argv[];

	{
	char *tst;
	int flags;
	long sts;
	unsigned short tmud_port;
	char *host_name;
	struct sockaddr_in sin;
	struct hostent *hp;
	struct servent *sp;

    /* Make connection...default is DAISY at 4201 */
    switch (argc) {
        case 1 : tmud_chan = netconnect("128.2.218.26","4201"); break;
        case 2 : tmud_chan = netconnect(argv[1],"4201");        break;
        case 3 : tmud_chan = netconnect(argv[1], argv[2]);      break;
        default: printf("Usage: tint <host> <port>\n");         exit(1);
    }
    if (tmud_chan==0) exit(1);

	/* initialize all stuff for i/o filters */

	init_filters();
        gagged = 0;

	/* Initialize the screen. Do your own scrolling region. It's easier */

	printf(HOME);printf(ERASE_EOS);
	printf(SCROLLING_REGION);
	out_x = out_y = 1;

	/* set up asynchronous handling for input from TinyMud */

	sys$qio(0,tmud_chan,IO$_READVBLK,&iosb,handle_output
		,0,buffer,5000,0,0,0,0);
                                    
        load_handler("defaultmud","",0);

	handle_input();

	/* We get back here only when Herr User exits, so tidy up and quit */

	printf(HOME);printf(ERASE_EOS);
	printf(FULL_SCROLLING_REGION);
	}

/**********************************************************************

	Get input from the user, cook it, and stuff it into the socket
connected to TinyMud. Continue until the user quits.

***********************************************************************/

handle_input()

	{
	char raw_buff[256],cooked_buff[512];
	int sts;
	int siz;

	for(;;)
		{
		/* Move the cursor to line 22, col 1 */

		move_to(22,1);
		printf(ERASE_EOS);
		printf("TINT>");
		gets(raw_buff);

		/* Process commands and/or embedded macros. This returns one of
		2 -- quit, 1 -- send cooked to TinyMud, or 0 -- do nothing */

		sts = filter_input(raw_buff,cooked_buff,512);
		if(sts == 2)
			{
			socket_close(tmud_chan);
			return;
			}
		if(sts == 1)
			{
			siz = strlen(cooked_buff);
			cooked_buff[siz] = (char) 13;
			siz++;
			cooked_buff[siz] = (char) 10;
			siz++;
			if( socket_write(tmud_chan,cooked_buff,siz) == -1)
				{	
		      		socket_perror("\nFailed send");
				}
			}
		}
	}

/*********************************************************************

	Called by AST every time TinyMud says something. Filter whatever
TinyMud has to say, spew it out on the screen, and queue up
another super-cool AST driven read request on the TinyMud socket, and quit.

**********************************************************************/

handle_output()

	{
	static char cooked_buffer[5000];

	if( iosb[1] == 0)
		return;

	if( iosb[0] == 1)
		{
		if( filter_output(buffer,cooked_buffer,5000) == 1)
			{
			display_output(iosb[1],cooked_buffer);
			}
                else
		        {
                        gagged = 1;
                        }
		}

	/* Set up another read request */

	sys$qio(0,tmud_chan,IO$_READVBLK,&iosb,handle_output
		,0,buffer,5000,0,0,0,0);

	/* return */
	}

/*************************************************************************

Routine for displaying TinyMud output. Basically just does word wrapping, and
tracks the cursor position for output. Remember, output is done asychronously,
so the cursor may not be where you left it. Be careful.

*************************************************************************/

display_output(count,buff)

short count;
char *buff;

	{
	int curr_char,i,j;
	int newln;
	char ch;

	printf(SAVE_CURSOR);

	move_to(out_y,out_x);
	/* copy the text out with word wrapping */

	curr_char = 0;  /* point to 1st character in buffer */

	for(;count > 0;)
		{
		/* skip spaces, and keep an eye out for carriage returns
			and line feeds. */

		newln = FALSE;
		for(; (isspace(ch = buff[curr_char])) && (count > 0);)
			{
			if( (ch == '\015') || (ch == '\012') )
				newln = TRUE;
			count--;
			curr_char++;
			}

		/* If skipped one or more CRs,LFs, then output a newline */
		if( newln ) newline();

		if(count <= 0) break;  /* If we fell off the end, quit */

		/* assemble a word of output */

		i = 0;
		for(; (i < 80) && !(isspace(ch = buff[curr_char]))
				&& (count > 0);)
			{
			word[i++] = buff[curr_char++];
			count--;
			}

		/* dump it out, on this line if it fits, else newline
			then dump it. Follow by a space */

		if( (i + out_x) > 80)
			{
			newline();
			}

		out_x += (i+1);
		for(j = 0; i > 0; i--)
			{
			putchar(word[j++]);
			}
		putchar(' ');
		}
	printf(RESTORE_CURSOR);
	}

/**********************************************************************

	Couple teeny little routines for display_output()

***********************************************************************/

/* routine to move the cursor to a position */

move_to(line,col)

int line,col;

	{
	printf("\033[%d;%dH",line,col);
	}

/* routine to do a new line in the output, and track all the garbage */

newline()
	{
        if (gagged == 0)
	        {
            	out_x = 1;  
         	out_y = (out_y >= 20 ? 20 : out_y+1);
         	printf("\n");
	        }
        else
	        {
                gagged = 0;
                }
	}

SHARC_EOF_MARK
$ write sys$output "TINT.DOC"
$ copy sys$input TINT.DOC
$ deck /dollars="SHARC_EOF_MARK"
Tinymud INTerface (TINT) for VMS, Version 1.3
=============================================

Invoking the Interface
----------------------

	There are two optional parameters, the first is the host.  You may specify
it either as a numeric Internet address (e.g. 128.2.218.26) or as a "host
name" (e.g. DAISY.LEARNING.CS.CMU.EDU).  The second is the port which may also
appear as a number (e.g. 4201) or a recognized service (e.g. telnet, smtp,
finger, etc.).  If one or both parameters does not appear, they default to
128.2.218.26 (DAISY) and 4201, respectively.

Using the Interface
-------------------

	There are two windows. One runs from the top of the screen down
to about line 20 -- this is where TinyMud output goes. The other runs from line
21 down to the bottom. You type input in lines 22 down, the interface puts
informational messages on line 21. At the TINT> prompt, if you type text
(without any \ characters in it), this text is sent unmodified to TinyMud.

	Apart from this, the interface has a few interface commands, and
supports very simple text macros.Both commands and macros begin with a \
character.

	The commands are: 

\quit -- quit the program
\gag -- gag a person (fairly effectively -- deletes ANY stuff from TinyMud
	which starts with that person's name)
\ungag -- ungag a person
\help -- gives you help. Obviously.
\def -- define a text macro. The macro name must start with a \.
\version -- displays current version number.
\spawn -- spawns a subprocess.
\save -- save macro/gag list to a file.
\load -- load macro/gag list from a file.

Text Macros
-----------

	When you define a text macro, every time the interface detects the
macro name, it will expand it to the equivalent text before sending it off to
TinyMud. You can have parameters in your macro. To do this, you put #n 
(n = 1 single digit 1 though 9) in the equivalence string wherever you want a
parameter to appear. Then when you call the macro, you give it a parameter
list of things to substitute in for the #n's.


For example:

TINT>\def \hi=Hi There #1
TINT>say hi(ralph)

TINT>\def \hello=hi there #1, hows #2?
TINT>say \hello(ralph,the wife)

	If you have a parameter specified in the equivalence string, but do not
specify it in the parameter list, it is deleted.

NOTE: the space after \def is (currently) required.
NOTE: NO space is allowed between the macro name and the parameter list.
NOTE: in a \def, you MUST start the macro name with a \, and there must
be a space after \def. I.E.

\def \hi=stuff    -- this works

\def\hi=stuff
\def hi=stuff     - these don't
\defhi=stuff


Conventions For the \ Character
-------------------------------

	(I apologize for this, it just sort of happened :-)

	At the beginning of an input line:
		a single \ indicates a command (\gag etc...)
		a double \\ is interpreted as the beginning of a macro
		a triple \\\ is a literal \ character

	In the middle of a line
		a single \ indiactes start of macro
		a double \\ is a literal \ char.

For Example:

TINT>\def \qu=QUIT  <--- single slash -- hence the def command
TINT>\\qu           <--- this sends ``QUIT'' to TinyMud

but:

TINT>say I \qu   <--- sends ``say I QUIT'' to TinyMud



Compatibility and Porting Issues
--------------------------------

	This program is designed for vt220 terminals. I think it ought to work
OK on vt100's and anything compatible therewith. I have tried, somewhat
unsuccessfully, to keep the terminal dependent stuff segregated into #defines.
Be warned that quite a bit of stuff is hard-coded.

	The network software we use is Multinet. We have a socket library and a
$QIO interface, which sort of work. This program uses them. There are a lot of
include files for this system, my compiler can find them on my system, dunno
about yours.


ALL THE CODE FOR THIS PROGRAM IS HEREBY RELEASED INTO THE PUBLIC DOMAIN

			Andrew Molitor aka bob


Loading and Saving Macros and Gags
----------------------------------

	To save your current macros and gags to a file, enter "\save filename".
"\load filename" will read in a file that was saved.  These files can be 
edited with a text editor such as TPU or EMACS as well.  

	Upon startup, the program will look for a file called DEFAULTMUD.  It 
will load all the definitions in the file if it finds it, or display an error 
message if it doesn't.  If you don't want a startup file, ignore the message.
If you want to call your startup file something other than DEFAULTMUD then just
enter "define defaultmud newname" at your DCL prompt (or in your login.com).

Spawning a DCL Subprocess
-------------------------

	To spawn a new DCL process, simply enter the command "\spawn".



                         ncastellano@eagle.wesleyan.edu
                           a.k.a. entropy
                           a.k.a. enthalpy


SHARC_EOF_MARK
$ write sys$output "TINT.H"
$ copy sys$input TINT.H
$ deck /dollars="SHARC_EOF_MARK"
/*

        Feb 17, 1990
	Header file for Tinymud INTerface. by Andrew Molitor.
	This is released into the Public Domain. Enjoy

*/

#define VERSION "TINT version 1.5"

/* Escape sequences for vt220. These might work on a vt100 as well, but not on
most anything else. */

#define SCROLLING_REGION "\033[1;20r"   /* lines 0 through 20 will scroll */
#define FULL_SCROLLING_REGION "\033[1;24r"   /* entire screen will scroll */
#define ERASE_EOS "\033[J"
#define HOME "\033[H"
#define SAVE_CURSOR "\0337"
#define RESTORE_CURSOR "\0338"
#define CRLF "\015\012"

/* structure for the list of gags. Back-linked for simplicity, and because I
	like it that way */

struct gag
	{
	char *name;
	struct gag *back;
	struct gag *fwd;
	};

/* structure for the list of text macros. Back-linked for simplicity, and
	because I like it that way */

struct txt_mac
	{
	char *name;
	char *value;
	struct txt_mac *back;
	struct txt_mac *fwd;
	};

/* Structure for a built in command. We just have an array of these */

struct cmd
	{
	char *name;     /* command name */
	char *help_txt; /* a little help text for the command */
	int (*handler)(); /* address of this command's handler routine */
	};

/* function declarations */

char *skipspaces();
unsigned short netconnect(char *host, char *port);
SHARC_EOF_MARK
$ write sys$output "TINT.OPT"
$ copy sys$input TINT.OPT
$ deck /dollars="SHARC_EOF_MARK"
sys$share:vaxcrtl/share
multinet:multinet_socket_library.exe/share
SHARC_EOF_MARK
$ write sys$output "TINT_CMD.C"
$ copy sys$input TINT_CMD.C
$ deck /dollars="SHARC_EOF_MARK"
/*
	Feb 17 1990
	This software is released into the Public Domain. Enjoy.

			Andrew Molitor

	Tinymud INTerface command handlers
*/


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tint.h"

/*
	Table of built in commands
*/

int def_handler();
int gag_handler();
int help_handler();
int ungag_handler();
int quit_handler();
int dump_handler();
int version_handler();
int spawn_handler();
int save_handler();
int load_handler();

struct cmd builtins[]=
	{
	{"def","\macro_name=macro_text defines a text macro",def_handler},
	{"gag","person_name installs a gag",gag_handler},
	{"help","gives you this help",help_handler},
	{"ungag","person_name removes a gag",ungag_handler},
	{"quit","quits this program",quit_handler},
	{"dump","dumps gag and macro lists (debug cmd)",dump_handler},
        {"version","displays current version number",version_handler},
	{"spawn","spawns out a subprocess",spawn_handler},
        {"save","filename saves gag and macro lists",save_handler},
        {"load","filename loads gag and macro lists",load_handler},
	{NULL,NULL,NULL}
	};

extern struct gag *gag_list;
extern struct txt_mac *mac_list;

/*************************************************************************

	 The various command handlers

***************************************************************************/

/* define macro command:  \def name=string */

def_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	struct txt_mac *new_mac, *tmp;
	char *str,*name,*val;

	/* skip spaces before the name */

	for(;(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	if (*in_buff == '\0')
		{
		message("Macro name?");
		return(0);
		}
	if (*in_buff != '\\')
		{
		message("Start Macro name with \\");
		return(0);
		}

	in_buff++;  /* skip leading `/' */
	str = in_buff;  /* macro name */

	/* now terminate the name at the first white space or at the 
		`=' sign */

	for(;!(isspace(*in_buff)) && (*in_buff != '\0') && (*in_buff != '=');)
		in_buff++;

	if( *in_buff == '\0')
		{
		message("Invalid macro definition");
		return(0);
		}
	else if( isspace(*in_buff))
		{
		/* terminate the macro name here */

		*in_buff++ = '\0';

		/* skip ahead to the `=' sign, and skip over it */

		for(;(*in_buff != '=') && (*in_buff != '\0');) in_buff++;
		if( *in_buff == '\0')
			{
			message("Invalid macro definition");
			return(0);
			}
		in_buff++;
		}
	else  /* The macro name must end with the '=' */
		{
		*in_buff++ = '\0';
		}

	/* Do we already have a macro of this name? If so, redefine */

	if( (tmp = mac_list_srch(str)) != NULL )
		{
		message("Macro exists, redefining");
		val = malloc(strlen(in_buff)+1);
		if( val == NULL)
			{
			message("Could not redefine macro");
			}
		else
			{
			free(tmp->value);
			strcpy(val,in_buff);
			tmp->value = val;
			}
		return(0);
		}

	/* otherwise make a new macro */

	name = malloc(strlen(str)+1);
	val = malloc(strlen(in_buff)+1);
	new_mac = (struct txt_mac *)malloc(sizeof(struct txt_mac));

	if((new_mac == NULL) || (name == NULL) || (val == NULL))
		{
		message("Could not define macro");
		return(0);
		}

	strcpy(name,str);
	strcpy(val,in_buff);
	new_mac->name = name;
	new_mac->value = val;

	/* Now insert the new macro into the list */

	if(mac_list == NULL)
		{
		mac_list = new_mac;
		new_mac->fwd = new_mac->back = new_mac;
		}
	else
		{
		new_mac->fwd = mac_list->fwd;
		new_mac->back = mac_list;
		(mac_list->fwd)->back = new_mac;
		mac_list->fwd = new_mac;
		}


	return(0);
	}

/* help handler. Format:  \help  */

help_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	int i;
	char str[80];

	for(i=0 ; builtins[i].name != NULL ;i++)
		{
		sprintf(str,"\\%s %s  (press CR)",
			builtins[i].name , builtins[i].help_txt);
		message(str);
		getchar();
		}
	move_to(21,1);
        printf("\033[K");
	return(0);
	}

/* Add an item to the gag-list. Format: \gag person */

gag_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	struct gag *new_gag;
	char *str,*name;

	/* skip spaces in before the name */

	for(;(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	if (*in_buff == '\0')
		{
		message("Gag who?");
		return(0);
		}

	str = in_buff;

	/* now terminate the name at the first space */

	for(;!(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	*in_buff = '\0';
	name = malloc(strlen(str)+1);
	new_gag = (struct gag *)malloc(sizeof(struct gag));
	if((new_gag == NULL) || (name == NULL) )
		{
		message("Could not install gag");
		return(0);
		}

	strcpy(name,str);
	new_gag->name = name;

	/* Now insert the new gag into the list */

	if(gag_list == NULL)
		{
		gag_list = new_gag;
		new_gag->fwd = new_gag->back = new_gag;
		}
	else
		{
		new_gag->fwd = gag_list->fwd;
		new_gag->back = gag_list;
		(gag_list->fwd)->back = new_gag;
		gag_list->fwd = new_gag;
		}

	return(0);
	}

/* Remove an item to the gag-list. Format \ungag person */

ungag_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	char *str;
	struct gag *tmp;

	/* skip spaces in before the name */

	for(;(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	if (*in_buff == '\0')
		{
		message("Ungag who?");
		return(0);
		}

	str = in_buff;

	/* now terminate the name at the first space */

	for(;!(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	*in_buff = '\0';

	/* Now str points at the name of the un-gagee */

	if( (tmp = gag_list_srch(str)) == NULL )
		{
		message("Not Gagged");
		}
	else
		{
		/* delete the entry from the gag_list */

		if( tmp->fwd == tmp )  /* this is the only thing */
			{
			gag_list = NULL;
			}
		else
			{
			(tmp->back)->fwd = tmp->fwd;
			(tmp->fwd)->back = tmp->back;
			if( tmp == gag_list )
				gag_list = tmp->fwd;
			}
		free(tmp->name);
		free(tmp);
		}
	return(0);
	}

/* Dump the macro list and gag list. Debugging code. Format: \dump */

dump_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	struct gag *tmp;
	struct txt_mac *tmp2;

	if(gag_list != NULL)
		{
		printf(HOME);
		printf(ERASE_EOS);
		tmp=gag_list;
		printf("%s is gagged",tmp->name);
		tmp = tmp->fwd;
		for(;tmp != gag_list;)
			{
			printf("\n%s is gagged",tmp->name);
			tmp = tmp->fwd;
			}
		}
	message("For Macros -- press CR");
	getchar();
	if(mac_list != NULL)
		{
		printf(HOME);
		printf(ERASE_EOS);
		tmp2=mac_list;
		printf("%s == %s",tmp2->name,tmp2->value);
		tmp2 = tmp2->fwd;
		for(;tmp2 != mac_list;)
			{
			printf("\n%s == %s",tmp2->name,tmp2->value);
			tmp2 = tmp2->fwd;
			}
		}
        move_to(21,1);
        printf("\033[K");
	return(0);
	}
/* Load a macro list and gag list. Format: \load filename */

load_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
        FILE *infile;
        char line[256];
        char work[256];
	/* skip spaces in before the filename */

	for(;(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	if (*in_buff == '\0')
		{
		message("Load filename?");
		return(0);
		}

        if ((infile = fopen(in_buff,"r")) == NULL)
                {
                message("Error opening input file.");
                return(0);
	        }

        while (!(feof(infile)) && !(ferror(infile)))
	        {
                fgets(line,255,infile);
                if (line[strlen(line)-1] == '\n')
		  {
                  line[strlen(line)-1] = '\000';
	          }
                filter_input(line,work,strlen(line));
	        }

        if ((fclose(infile)) == EOF)
                {
                message("Error closing input file.");
                return(0);
	        }

        message("Loaded.");
	return(0);
	}

/* Save the macro list and gag list. Format: \save filename */

save_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
        FILE *outfile;

	struct gag *tmp;
	struct txt_mac *tmp2;

	/* skip spaces in before the filename */

	for(;(isspace(*in_buff)) && (*in_buff != '\0');) in_buff++;

	if (*in_buff == '\0')
		{
		message("Save filename?");
		return(0);
		}

        if ((outfile = fopen(in_buff,"w")) == NULL)
                {
                message("Error opening output file.");
                return(0);
	        }

	if(gag_list != NULL)
		{
		tmp=gag_list;
		fprintf(outfile,"\\gag %s",tmp->name);
		tmp = tmp->fwd;
		for(;tmp != gag_list;)
			{
			fprintf(outfile,"\n\\gag %s",tmp->name);
			tmp = tmp->fwd;
			}
		}
        fprintf(outfile,"\n");
	if(mac_list != NULL)
		{
		tmp2=mac_list;
		fprintf(outfile,"\\def \\%s=%s",tmp2->name,tmp2->value);
		tmp2 = tmp2->fwd;
		for(;tmp2 != mac_list;)
			{
			fprintf(outfile,"\n\\def \\%s=%s",tmp2->name,tmp2->value);
			tmp2 = tmp2->fwd;
			}
		}

        if ((fclose(outfile)) == EOF)
                {
                message("Error closing output file.");
                return(0);
	        }

        message("Saved.");
	return(0);
	}

/* handle the quit command. Format: \quit */

quit_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

	{
	return(2);
	}

/* handle the version command. Format: \version */

version_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

        {
        message(VERSION);
        return(0);
        }

/* handle the spawn command. Format: \spawn */

spawn_handler(in_buff,out_buff,size)
char *in_buff,*out_buff;
int size;

        {
        extern int out_x, out_y;
        printf(HOME);
        printf(ERASE_EOS);
        printf(FULL_SCROLLING_REGION);
        lib$spawn();
        printf(HOME);
        printf(ERASE_EOS);
        printf(SCROLLING_REGION);
        out_x = out_y = 1;
        return(0);
        }

/**********************************************************


	Routine for giving the user messages on line 21

***********************************************************/


message(str)

char *str;

	{
	move_to(21,1);
	printf("\033[K-->");
	printf(str);
	}








SHARC_EOF_MARK
$ write sys$output "TINT_FILT.C"
$ copy sys$input TINT_FILT.C
$ deck /dollars="SHARC_EOF_MARK"
/*

	Feb 17 1990
	By me, Andrew Molitor. This is released into the Public Domain.
		Enjoy.

	TinyMud interface I/O filters. Includes some generic string handling
	routines. One bolder than I might term them primitive parsing routines,
	but I shan't.

	The issue of '\' characters on comamnds works like this:

		At the start of line:
			a single \ must be followed by a command
			a pair of slashes must be followed by a macro name
			a trio of slashes is interpreted as a literal \ char
		In the middle of a line:
			a single \ must precede a macro name
			a double \ is interpreted as a literal \ char
				
*/

#define ERROR 1
#define OK 0

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tint.h"

char *expand_macro();

struct gag *gag_list;
struct gag *gag_list_srch();

struct txt_mac *mac_list;
struct txt_mac *mac_list_srch();

extern struct cmd builtins[];

/*******************************************************************

 This routine initializes all the filters. Not real complex.

********************************************************************/

init_filters()

	{
	gag_list = NULL;
	mac_list = NULL;
	}

/*************************************************************************

This filters the user's input. returns either 2 -- the interface program
should quit, 1 -- cooked data should be sent to TinyMud or 0 -- ignore cooked
data

**************************************************************************/

filter_input(in_buff,out_buff,size)

char *in_buff,*out_buff;
int size;

	{
	int i,done;
	char *cmd;

	/* skip spaces */

	in_buff = skipspaces(in_buff);

	/* Check for starting with \, but not \\ */

	if( (in_buff[0] == '\\') && (in_buff[1] != '\\'))
		{
		/* Find the command */

		cmd = in_buff+1;

		/* command name may end with white space or end of string */

		for(; !(isspace(*in_buff)) && (*in_buff != '\0');)
			in_buff++;

		if( *in_buff != '\0') /* if we didn't fall of the end */
			*in_buff++ = '\0';

		for(i=0 ; builtins[i].name != NULL ; i++)
			{
			if(strcmp(cmd,builtins[i].name) == 0)
				break;
			}

		if(builtins[i].name != NULL)
			{
			return(builtins[i].handler(in_buff,out_buff,size));
			}
		}
	else
		{
	/* This is not a command, so filter it into the output buffer */

		/* If we started with a \\, then skip the first one */

		if(in_buff[0] == '\\') in_buff++;

		size--; /* leave one space for a terminator */
		done = FALSE;
		for(;!done;)
			{
			/* copy text until you hit a macro */

			if(*in_buff != '\\')
				{
				*out_buff++ = *in_buff++;
				size--;
				done = (size <= 0) || (*in_buff == '\0');
				}
			else if((in_buff[0] == '\\') && (in_buff[1] == '\\'))
				{
				/* copy out a single / char */
				in_buff++;
				*out_buff++ = '\\';
				size--;
				done = (size <= 0);
				}
			else  /* attempt to expand the macro */
				{
				/* point to the macro's name */

				in_buff++;

				in_buff = expand_macro(in_buff,&out_buff,
					&size);
				done = (*in_buff == '\0') || (size <= 0);
				}
			}
		*out_buff = '\0';
		return(1);
		}
	}


/*********************************************************************

	Macro expansion routine. This receives a pointer to the macro name
(in), a pointer to a pointer to an output buffer, and a pointer to an integer
indicating how much space there remains in the output buffer.

	It expands the macro into the output buffer, adjusts the pointer to the
output buffer to point *after* the expanded macro, adjusts the integer to
reflect space taken up by the expanded macro, and returns a pointer to the
first character *after* the macro name (in the input buffer)

*********************************************************************/

char *expand_macro(in,out,siz)

char *in;
char **out;
int *siz;

	{
	char ch,*mac_name,*parm_list,*val,*parm;
	int i,done,parm_num,sts;
	struct txt_mac *tmp;

	mac_name = in;

	/* skip ahead to end of macro name, it will end with white space, the
		end of the buffer or a '(' (start of parameter list) */

	for(;!(isspace(*in)) && (*in != '\0') && (*in != '(');)
		in++;

	/* If we terminated at '(', we have a parameter list, deal with it */

	if( *in == '(' )
		{
		/* terminate macro name */
		*in++ = '\0';

		parm_list = in;

		/* go ahead to you hit the matching ')' */

		for(; (*in != ')') && (*in != '\0') ;) in++;

		/* If we fell of the end, error */

		if( *in == '\0')
			{
			message("Parameter list error, macro flushed");
			return(in);
			}
		else  /* otherwise, terminate the parameter list */
			{
			*in++ = '\0';
			}
		ch = ' ';
		}
	else if(*in != '\0')  /* if we didn't reach the end of input, do this */
		{
		/* in points to a white space char, which we want */
		ch = *in;
		*in++ = '\0';
		}

	/* mac_name points to the macro name. have we such a macro? */

	if((tmp = mac_list_srch(mac_name)) != NULL)
 		{
		/* If we've got it, expand it */

		done = FALSE;
		val = tmp->value;
		for(;!done;)
			{
			/* copy characters from val to *out */
			for(; (*val != '#') && (*val != '\0') && (*val != '%');)
				{
				*(*out)++ = *val++;
				(*siz)--;
				if( *siz <= 0) return(in);
				}

			if( *val == '\0' )
				{
				done = TRUE;
				}
			else if (*val == '%')   /* Embedded CR ?*/
				{
				val++;
				if( *val == '%' )   /* No, literal % */
					{
					val++;
					if( *siz < 2 ) return(in);
					*(*out)++ = '%';
					(*siz)--;
					}
				else
					{
					if( *siz < 3 ) return(in);
					*(*out)++ = '\015';   /* CR */
					*(*out)++ = '\012';   /* LF */
					(*siz) -= 2;
					}
				}
			else	/* deal with a parameter */
				{
				val++;
				if( isdigit(*val) )
					{
					parm_num = (*val) - '0';
					val++; /* skip past digit */

					/* get the parameter */

					parm = parm_list;
					sts = OK;
					for(; (parm_num > 1) ; parm_num--)
						{
						for(;(*parm!=',')
							&&(*parm != '\0');)
							parm++;

					/* If we fall off the end, error */

						if(*parm == '\0')
							{
							sts=ERROR;
							break;
							}
						else   /* skip the comma */
							parm++; 
						}
					/* parm should point at parameter */
					/* if so, copy it out */

					if(sts != ERROR)
						{
						for(;(*parm != ',')
							&& (*parm!= '\0');)
							{
							*(*out)++ = *parm++;
							(*siz)--;
							if( *siz <= 0)
								return(in);
							}
						}
					}
				else		/* Not really a parameter */
					{
					/* kludge so you can enter ##n */
					/* and get a #n. Convert all ##'s */
					/* to a single #.		*/

					if(*val == '#')
						val++;
					*(*out)++ = '#';
					(*siz)--;
					if(siz <= 0) return(in);
					}
				}
			}
		/* copy our last little character out */

		(**out) = ch;
		(*siz)--;
		(*out)++;
		}
	else
		{
		message("Unknown macro flushed");
		}

	return(in);
	}

/*  routine to search the macro list */

struct txt_mac *mac_list_srch(name)

char *name;

	{
	struct txt_mac *tmp;
	int found;

	tmp = mac_list;
	if( tmp == NULL ) return(NULL);

	found = FALSE;
	do
		{
		if( strcmp(tmp->name , name) == 0)
			{
			found = TRUE;
			break;
			}
		tmp = tmp->fwd;
		}
	while(tmp != mac_list);
	if(found)
		return(tmp);
	else
		return(NULL);
	}
/* routine to search the gag list */

struct gag *gag_list_srch(name)

char *name;

	{
	struct gag *tmp;
	int found;

	tmp = gag_list;
	if( tmp == NULL ) return(NULL);

	found = FALSE;
	do
		{
		if( strcmp(tmp->name , name) == 0)
			{
			found = TRUE;
			break;
			}
		tmp = tmp->fwd;
		}
	while(tmp != gag_list);
	if(found)
		return(tmp);
	else
		return(NULL);
	}

/***********************************************************************

This filters the output from TinyMud, implementing gags and whatnot.
I gues thats just gags, at the moment.
It returns either 1 (indicating ``display cooked data'') or 0 (indicating
that this stuff should be ignored

************************************************************************/

filter_output(in_buff,out_buff,size)

char *in_buff,*out_buff;
int size;

	{
	char *first_word;
	char ch;

	/* extract the first word in ``in_buff'' */

	first_word = in_buff;
	for(; (in_buff != '\0') && !(isspace(*in_buff)) ;) in_buff++;
	ch = *in_buff;
	*in_buff++ = '\0';

	if( gag_list_srch(first_word) == NULL)
		{
		/* no gag in effect, so get this stuff into out_buff */
		/* we know ``first_word'' will fit, since it fit in in_buff */

		for(;*first_word != '\0';)
			*out_buff++ = *first_word++;
		*out_buff++ = ch;
		if( ch == '\0' ) return(1);

		/* the rest gets copied below */
		}
	else
		{
		/* gag in effect */
		return(0);
		}

	for(; *in_buff != '\0' ;)
		*out_buff++ = *in_buff++;
	return(1);
	}

/********************************************************************

	String routines. Only one so far.

********************************************************************/

char *skipspaces(str)

char *str;

	{
	for(; isspace(*str) && (*str != '\0') ;) str++;
	return(str);
	}

SHARC_EOF_MARK
$ write sys$output "TINT_NETC.C"
$ copy sys$input TINT_NETC.C
$ deck /dollars="SHARC_EOF_MARK"
#include <iodef.h>
#include <stdio.h>

#include "multinet_root:[multinet.include.sys]types.h"
#include "multinet_root:[multinet.include.sys]socket.h"
#include "multinet_root:[multinet.include.netinet]in.h"
#include "multinet_root:[multinet.include]netdb.h"

#include "tint.h"

/* Given host and port, makes a connection */
/*    Returns 0 on failure, else channel connected to */
unsigned short
netconnect (host, port)
    char *host, *port;
{
    struct sockaddr_in sin;
    struct hostent *hp=0;
    struct servent *sp=0;
    char localhost[34];
    unsigned short int chan;

    printf("Trying...");

    /* Zero the sin structure to initialize it */
    bzero((char *) &sin, sizeof(sin));
    sin.sin_family = AF_INET;

    /* Lookup the host and initialize sin_addr */
    /* If no host specified, use local address */
    if ((host == 0) || (host[0] == '\0')) {
        if (gethostname(localhost,sizeof(localhost))<0) {
            fprintf(stderr,"Unable to find localhost.\n");
            return(0);
        }            
        hp = gethostbyname(localhost);
    } else {
        hp = gethostbyname(host);
    }
    /* Host not name, try treating it as octets */
    if (!hp) {      
        if ((sin.sin_addr.s_addr = inet_addr(host)) == -1) {
            fprintf(stderr,"Unknown internet host.\n");
            return(0);
        }
    } else {
        bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
    }

    /* Lookup the port */
    if ((port == 0) || (port[0] == '\0')) {
        sp = getservbyname("telnet", "tcp");        /* default */
    } else {
        sp = getservbyname(port, "tcp");            /* by name */
    }       
    if (sp == NULL) {                               /* maybe by number? */
        unsigned short nport;
        
        nport = atol(port);
        if (nport == 0) {
            fprintf(stderr,"Unknown port.\n");
            return(0);
        }
        sin.sin_port = htons(nport);
    } else {   
        sin.sin_port = sp->s_port;
    }

    /* Assign socket */
    chan = socket(sin.sin_family, SOCK_STREAM, 0);
    if (chan < 0) {
        socket_perror("socket");
        return(0);
    }

    /* Connect */
    if (connect(chan, &sin, sizeof(sin),0) < 0) {
        socket_perror("connect");
        return(0);
    }

    printf("Connected\n");
    
/*    Report some information about the connection
**  if (hp == NULL) hp = gethostbysockaddr(&sin, sizeof(sin), AF_INET);
**  if (hp != NULL && hp->h_name != NULL) {
**      printf("[%s, %s]\n",hp->h_name,inet_ntoa(sin.sin_addr));
**  }
*/

    return(chan);
}
SHARC_EOF_MARK
