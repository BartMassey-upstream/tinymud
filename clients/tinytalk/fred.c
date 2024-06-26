/************************************************************************/
/* TinyTalk : Connect to a TinyMUD world.				*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*				 Contact rang@cs.wisc.edu for support.  */
/*									*/
/*		1.1 [ 2/ 2/90] : Added many new commands; reorganized   */
/*				 some files.				*/
/*									*/
/*   This program connects to a TinyMUD world via the TELNET protocol.  */
/* It provides the following features:					*/
/*									*/
/*   Portal recognition: Some TinyMUD systems have portals to other     */
/*                       worlds.  TinyTalk deals with these correctly.  */
/*									*/
/*   Word wrap: If word wrap is desired, it may be enabled.		*/
/*									*/
/*   Command refresh: When TinyMUD prints a message, the command line   */
/*		      typed so far is reprinted.			*/
/*									*/
/*   Macros: Simple macros may be defined.				*/
/*									*/
/*   Configuration file: Characters, parameters, and macros may be set  */
/*			 automatically.					*/
/*									*/
/*   Page handling: Pages may trigger beeps.				*/
/*									*/
/*   Gags: Messages from certain characters or robots may be ignored.	*/
/*									*/
/************************************************************************/

#include <stdio.h>
#include "tl.h"

extern world_rec *get_default_world(), *find_world();
extern char *malloc();

world_rec *boot_world();

main(argc, argv)
  int argc;
  char *argv[];
{
  world_rec *first_world;

  clear_quiet();			/* Initialize subsystems.	  */
  init_logging();			/* Don't do keyboard yet, though. */
  init_macros();
  enable_auto_login();
  enable_wrap(0);			/* Enable default wrapping. */
  set_beep(3);				/* Set default beep mode. */
  init_hiliting();
  init_gagging();
  preinit_keyboard();			/* Initialization before */
					/* config file is read. */

  read_configuration();
  first_world = boot_world(argc, argv);

  init_keyboard();
  do_stuff(first_world);
}

world_rec *boot_world(argc, argv)
  register int argc;
  register char *argv[];
{
  register world_rec *temp;

  if ((argc >= 2) && (!strcmp(argv[1], "-"))) { /* Option '-'. */
    disable_auto_login();
    --argc;
    argv++;
  }

  if (argc == 1) {			/* Try default world */
    temp = get_default_world();

    if (temp == NULL)
      die("%% You must specify a world; there is no default set.\n");
    else				/* Have a default. */
      return (temp);
  }
  else if (argc == 2) {			/* World specified. */
    if (argv[1][0] == '-') {		/* Check for '-' option */
      disable_auto_login();
      argv[1]++;
    }
    temp = find_world(argv[1]);
    if (temp == NULL)
      die("%% The world %s is unknown.\n", argv[1]);
    else
      return (temp);
  }
  else if (argc == 3) {			/* Port and address specified. */
    temp = (world_rec *) malloc(sizeof(world_rec)); /* Never reclaimed! */
    temp->next = NULL;
    strcpy(temp->world, "World");
    *(temp->character) = '\0';
    *(temp->pass) = '\0';
    if (argv[1][0] == '-') {		/* Disable auto login; otherwise */
      disable_auto_login();		/* we might use the default. */
      argv[1]++;
    }
    strcpy(temp->address, argv[1]);
    strcpy(temp->port, argv[2]);
    return (temp);
  }
  else
    die("Usage: %s [world]\n       %s address port\n", argv[0], argv[0]);

  return (NULL);
}
