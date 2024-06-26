/************************************************************************/
/* TinyTalk command line processing.					*/
/*									*/
/*	Version 1.0 [ 1/25/90] : Initial implementation by ABR.		*/
/*		1.1 [ 1/26/90] : Added additional commands.		*/
/*		1.2 [ 1/26/90] : Added additional commands.		*/
/*		1.3 [ 2/ 2/90] : Yet more commands; use binary search.	*/
/*		1.4 [ 2/ 5/90] : A few more commands; world switching.	*/
/*		1.5 [ 2/ 5/90] : Fixed /savehilite | /loadhilite.	*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

extern char *index();
extern world_rec *find_world(), *get_default_world(), *get_world_header();

  /* There are enough commands now that a hash table or binary search */
  /* is preferable.  We'll use a binary search.			      */

int handle_wrap_command(), handle_nowrap_command(), handle_log_command(),
    handle_nolog_command(), handle_logme_command(), handle_def_command(),
    handle_undef_command(), handle_savedef_command(), handle_nologme_command(),
    handle_listdef_command(), handle_loaddef_command(),
    handle_quiet_command(), handle_noquiet_command(), handle_login_command(),
    handle_nologin_command(), handle_help_command(), handle_quit_command(),
    handle_intr_command(), handle_nointr_command(), handle_beep_command(),
    handle_nobeep_command(), handle_stty_command(), handle_nostty_command(),
    handle_gag_command(), handle_nogag_command(), handle_savegag_command(),
    handle_loadgag_command(), handle_listgag_command(),
    handle_hilite_command(), handle_help2_command(), handle_recall_command(),
    handle_nohilite_command(), handle_loadhilite_command(),
    handle_savehilite_command(), handle_listhilite_command(),
    handle_whisper_command(), handle_nowhisper_command(),
    handle_listworlds_command(), handle_world_command();

int handle_macro_command();

typedef struct cmd_entry {
  char *name;
  int (*func)();
} cmd_entry;

  /* It is IMPORTANT that the commands be in alphabetical order! */

static cmd_entry cmd_table[] =
{
  { "BEEP", handle_beep_command },
  { "DEF", handle_def_command },
  { "GAG", handle_gag_command },
  { "HELP", handle_help_command },
  { "HELP2", handle_help2_command },
  { "HILITE", handle_hilite_command },
  { "INTR", handle_intr_command },
  { "LISTDEF", handle_listdef_command },
  { "LISTGAG", handle_listgag_command },
  { "LISTHILITE", handle_listhilite_command },
  { "LISTWORLDS", handle_listworlds_command },
  { "LOADDEF", handle_loaddef_command },
  { "LOADGAG", handle_loadgag_command },
  { "LOADHILITE", handle_loadhilite_command },
  { "LOG", handle_log_command },
  { "LOGIN", handle_login_command },
  { "LOGME", handle_logme_command },
  { "NOBEEP", handle_nobeep_command },
  { "NOGAG", handle_nogag_command },
  { "NOHILITE", handle_nohilite_command },
  { "NOINTR", handle_nointr_command },
  { "NOLOG", handle_nolog_command },
  { "NOLOGIN", handle_nologin_command },
  { "NOLOGME", handle_nologme_command },
  { "NOQUIET", handle_noquiet_command },
  { "NOSTTY", handle_nostty_command },
  { "NOWHISPER", handle_nowhisper_command },
  { "NOWRAP", handle_nowrap_command },
  { "QUIET", handle_quiet_command },
  { "QUIT", handle_quit_command },
  { "RECALL", handle_recall_command },
  { "SAVEDEF", handle_savedef_command },
  { "SAVEGAG", handle_savegag_command },
  { "SAVEHILITE", handle_savehilite_command },
  { "STTY", handle_stty_command },
  { "UNDEF", handle_undef_command },
  { "WHISPER", handle_whisper_command },
  { "WORLD", handle_world_command },
  { "WRAP", handle_wrap_command }
};

#define NUM_COMMANDS (sizeof(cmd_table) / sizeof(cmd_entry))

handle_command(cmd_line, is_config)
  register char *cmd_line;
  int is_config;
{
  string cmd, args;
  register char *place;
  int (*handler)(), (*binary_search())();

  place = index(cmd_line, ' ');
  if (place == NULL) {
    strcpy(cmd, cmd_line + 1);		/* Only have command.  Ignore '/'. */
    args[0] = '\0';
  }
  else {
    *place = '\0';			/* Get rid of the space.... */
    strcpy(cmd, cmd_line + 1);
    strcpy(args, place + 1);
    *place = ' ';			/* Put the space back! */
  }

  stripstr(cmd);
  stripstr(args);

  if (cmd[0] == '\0')			/* Empty command, ignore it. */
    return;

  handler = binary_search(cmd);

  if (handler != NULL)
    (*handler)(args);
  else
    handle_macro_command(cmd, args);	/* Assume it's a macro. */
}

int (*binary_search(cmd))()
  register char *cmd;
{
  register int bottom, top, mid, value;

  bottom = 0;
  top = NUM_COMMANDS - 1;

  while (bottom <= top) {
    mid = bottom + ((top - bottom) / 2);
    value = comparestr(cmd, cmd_table[mid].name);
    if (value == 0)
      return (cmd_table[mid].func);
    else if (value < 0)
      top = mid - 1;
    else
      bottom = mid + 1;
  }

  return (NULL);
}

handle_wrap_command(args)		/* Enable word wrap. */
  register char *args;
{
  if (args[0] == '\0')			/* No arguments */
    enable_wrap(0);
  else {
    if ((args[0] < '0') || (args[0] > '9')) /* Verify starts numeric. */
      fprintf(stderr,"%% Invalid wrap value %s.\n", args);
    else
      enable_wrap(atoi(args));		/* If argument, use it. */
  }
}

handle_nowrap_command(args)		/* Disable word wrap. */
  char *args;
{
  disable_wrap();
}

handle_log_command(args)		/* Enable logging. */
  char *args;
{
  enable_logging(args);
}

handle_nolog_command(args)		/* Disable logging. */
  char *args;
{
  disable_logging();
}

handle_logme_command(args)		/* Enable input logging. */
  char *args;
{
  enable_logme();
}

handle_nologme_command(args)		/* Disable input logging. */
  char *args;
{
  disable_logme();
}

handle_def_command(args)		/* Define a macro. */
  register char *args;
{
  register char *place;
  string name, body;

  place = index(args, '=');
  if (place == NULL) {
    fprintf(stderr,"%% No '=' in macro definition.\n");
    return;
  }

  *place = '\0';
  strcpy(name, args);
  strcpy(body, place + 1);
  stripstr(name);
  stripstr(body);

  add_macro(name, body);
}

handle_undef_command(args)		/* Undefine a macro. */
  char *args;
{
  remove_macro(args);
}

handle_savedef_command(args)		/* Save macro definitions. */
  register char *args;
{
  if (args[0] == '\0')			/* No filename, use default. */
    strcpy(args, "~/tiny.macros");

  write_macros(args);
}

handle_macro_command(cmd, args)		/* Invoke a macro. */
  char *args;
{
  string expanded;

  process_macro(cmd, args, expanded);	  /* Expand the body */
  if (expanded[0] != '\0')		  /* Catch failure (or empty). */
    transmit(expanded, strlen(expanded)); /* and send it via link. */
}

handle_quiet_command(args)		/* Turn on portal suppression. */
  char *args;
{
  set_quiet();
}

handle_noquiet_command(args)		/* Turn off portal suppression. */
  char *args;
{
  clear_quiet();
}

handle_login_command(args)		/* Turn on automatic login. */
  char *args;
{
  enable_auto_login();
}

handle_nologin_command(args)		/* Turn off automatic login. */
  char *args;
{
  disable_auto_login();
}

handle_help_command(args)		/* Give user some help. */
  char *args;
{
  puts("% Summary of commands:");
  puts("%   /WRAP      Enables word-wrap mode.");
  puts("%   /NOWRAP    Disables word-wrap mode.");
  puts("%");
  puts("%   /LOG       Enables file logging.");
  puts("%   /NOLOG     Disables file logging.");
  puts("%   /LOGME     Enables keyboard input logging.");
  puts("%   /NOLOGME   Disables keyboard input logging.");
  puts("%");
  puts("%   /QUIET     Prevents login messages from being displayed.");
  puts("%   /NOQUIET   Allows login messages to be displayed.");
  puts("%");
  puts("%   /LOGIN     Enables automatic login.");
  puts("%   /NOLOGIN   Prevents automatic login.");
  puts("%");
  puts("%   /DEF       Defines a macro.");
  puts("%   /UNDEF     Undefines a macro.");
  puts("%   /LISTDEF   Lists currently defined macros.");
  puts("%   /LOADDEF   Reads macro definitions from a file.");
  puts("%   /SAVEDEF   Saves macro definitions in a file.");
  puts("%   /macro     Invokes a macro.");
  puts("%");
  puts("%   /HELP2     For more help.");
}

handle_help2_command(args)		/* Give user some help. */
  char *args;
{
  puts("% Summary of commands:");
  puts("%   /QUIT      Quits TinyTalk.");
  puts("%   /INTR      Enables control-C.");
  puts("%   /NOINTR    Disnables control-C.");
  puts("%");
  puts("%   /BEEP      Enables beeps for pages.");
  puts("%   /NOBEEP    Disables beeps for pages.");
  puts("%");
  puts("%   /STTY      Reads system's keyboard setup.");
  puts("%   /NOSTTY    Uses default keyboard setup.");
  puts("%");
  puts("%   /GAG       Prevents messages from a user from being displayed.");
  puts("%              Also /NOGAG, /LISTGAG, /LOADGAG, and /SAVEGAG.");
  puts("%");
  puts("%   /HILITE    Hilites messages from a user.  The special arguments");
  puts("%              PAGE and WHISPER enable hiliting for those.");
  puts("%              Also /NOHILITE, /LISTHILITE, /LOADHILITE, and /SAVEHILITE.");
  puts("%");
}

handle_listdef_command(args)
  char *args;
{
  if (args[0] != '\0') {
    if (equalstr(args, "FULL"))
      list_all_macros(TRUE);
    else
      fprintf(stderr,"%% The only option to /listdef is 'full'.\n");
  }
  else
    list_all_macros(FALSE);
}

handle_loaddef_command(args)
  char *args;
{
  do_file_load("/DEF ", args, "~/tiny.macros");
}

handle_quit_command(args)
  char *args;
{
  set_done();
}

handle_beep_command(args)
  register char *args;
{
  if (args[0] == '\0')			/* No arguments, default to 3 beeps */
    set_beep(3);
  else {
    if ((args[0] < '0') || (args[0] > '9')) /* Verify starts numeric. */
      fprintf(stderr,"%% Invalid beep count %s.\n", args);
    else
      set_beep(atoi(args));		/* If argument, use it. */
  }
}

handle_nobeep_command(args)
  char *args;
{
  set_beep(0);
}

handle_intr_command(args)
  char *args;
{
  allow_interrupts();
}

handle_nointr_command(args)
  char *args;
{
  disallow_interrupts();
}

handle_stty_command(args)
  char *args;
{
  use_stty();
}

handle_nostty_command(args)
  char *args;
{
  no_use_stty();
}

handle_gag_command(args)
  char *args;
{
  if (args[0] == '\0')
    enable_gagging();
  else
    add_gag(args);
}

handle_nogag_command(args)
  char *args;
{
  if (args[0] == '\0')
    disable_gagging();
  else
    remove_gag(args);
}

handle_listgag_command(args)
  char *args;
{
  list_gag();
}

handle_loadgag_command(args)
  char *args;
{
  do_file_load("/GAG ", args, "~/tiny.gag");
}

handle_savegag_command(args)
  char *args;
{
  save_gag(args);
}

handle_hilite_command(args)
  register char *args;
{
  if (args[0] == '\0')
    enable_hiliting();
  else if (equalstr(args, "PAGE") || equalstr(args, "PAGES"))
    enable_page_hiliting();
  else if (equalstr(args, "WHISPER") || equalstr(args, "WHISPERS"))
    enable_whisper_hiliting();
  else if (equalstr(args, "NOPAGE") || equalstr(args, "NOPAGES"))
    disable_page_hiliting();
  else if (equalstr(args, "NOWHISPER") || equalstr(args, "NOWHISPERS"))
    disable_whisper_hiliting();
  else
    add_hilite(args);
}

handle_nohilite_command(args)
  register char *args;
{
  if (args[0] == '\0')
    disable_hiliting();
  else if (equalstr(args, "PAGE") || equalstr(args, "PAGES"))
    disable_page_hiliting();
  else if (equalstr(args, "WHISPER") || equalstr(args, "WHISPERS"))
    disable_whisper_hiliting();
  else
    remove_hilite(args);
}

handle_listhilite_command(args)
  char *args;
{
  list_hilite();
}

handle_loadhilite_command(args)
  char *args;
{
  do_file_load("/HILITE ", args, "~/tiny.hilite");
}

handle_savehilite_command(args)
  char *args;
{
  save_hilite(args);
}

  /* Generic routine to load commands of a specified type from a file. */

do_file_load(cmd, args, default_name)
  register char *cmd;
  char *args, *default_name;
{
  register FILE *cmdfile;
  int done;
  string line;
  char temp[20];			/* Big enough for any command. */
  int length;

  length = strlen(cmd);			/* Length of expected command. */

  if (args[0] == '\0')			/* No filename, use default. */
    strcpy(args, default_name);

  expand_filename(args);

  cmdfile = fopen(args, "r");
  if (cmdfile == NULL)
    fprintf(stderr,"%% Couldn't open file %s.\n", args);
  else {
    done = FALSE;

    while (!done) {
      if (fgets(line, MAXSTRLEN, cmdfile) == NULL)
	done = TRUE;
      else {
	if (line[0] != '\0')		/* Get rid of newline. */
	  line[strlen(line)-1] = '\0';

	if ((line[0] == '\0') || (line[0] == ';')) /* Blank or comment. */
	  continue;

	strncpy(temp, line, length);	/* Length of command (incl. space). */
	temp[length] = '\0';		/* Terminate after command. */
	if (!equalstr(temp, cmd)) {
	  fprintf(stderr,"%% Illegal line: %s.\n", line);
	  done = TRUE;
	}
	else {
	  handle_command(line,FALSE); /* Process this recursively. */
	}
      }
    }

    fclose(cmdfile);
  }
}

handle_whisper_command(args)
  char *args;
{
  disable_whisper_gagging();
}

handle_nowhisper_command(args)
  char *args;
{
  enable_whisper_gagging();
}

handle_recall_command(args)
  char *args;
{
  avoid_recall();			/* Don't recall this! */
  do_keyboard_recall();			/* Recall last keyboard buffer. */
}

handle_listworlds_command(args)
  char *args;
{
  register world_rec *p;

  p = get_world_header();
  while (p != NULL) {
    printf("%% %s\n", p->world);
    p = p->next;
  }
}

handle_world_command(args)
  char *args;
{
  char *place, ch;
  register world_rec *where;

  if (args[0] == '\0') {
    where = get_default_world();
    if (where == NULL) {
      fprintf(stderr, "%% No default world is set.");
      return;
    }
  }
  else {
    where = find_world(args);
    if (where == NULL) {
      fprintf(stderr, "%% The world %s is unknown.", args);
      return;
    }
  }

  if (connect_to(where))
    magic_login();
  else
    fprintf(stderr,"%% Connection to world %s failed.\n", where->world);
}
