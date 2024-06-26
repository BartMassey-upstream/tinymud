/************************************************************************/
/* TinyTalk output special processing.					*/
/*									*/
/*	Version 1.0 [ 2/ 2/90] : Extracted from main loop module; added */
/*				 new features for release 1.1.		*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

extern char *index();
extern world_rec *find_world();

#define MAX_QUIET 25			/* Max lines to quiet during login. */
#define MAX_BEEPS 5			/* Be reasonable!  Note--you must */
					/* change do_beep() if this is */
					/* increased. */

static int quiet_mode;			/* Suppress login messages? */
static int quiet_waiting;		/* Are we in a login process? */
static int quiet_counter;		/* So quietness eventually goes off. */

static int beep_count;			/* How many times should we beep? */

set_quiet()
{
  quiet_mode = TRUE;
}

clear_quiet()
{
  quiet_mode = FALSE;
  quiet_waiting = FALSE;		/* Need to initialize this, too. */
}

start_quiet()
{
  if (quiet_mode) {
    quiet_waiting = TRUE;
    quiet_counter = 0;
  }
}

stop_quiet()
{
  quiet_waiting = FALSE;
}

int keep_quiet(what)			/* Returns 1 if should suppress. */
  char *what;
{
  if (!strncmp(what, "Use the WHO command", 19)) {
    quiet_waiting = FALSE;		/* No longer waiting. */
    return (1);				/* Suppress this line. */
  }

  quiet_counter++;			/* Don't stay quiet forever, even */
  if (quiet_counter > MAX_QUIET) {	/* if we never see the right string. */
    quiet_waiting = FALSE;
    return (0);
  }
  else
    return (1);
}

  /* Quiet logins are also handled here. */

int special_hook(what)			/* Check for special thingies. */
  register char *what;
{
  smallstr name;

  if (quiet_waiting && keep_quiet(what))
    return (1);

  if (!strncmp(what, "#### Please reconnect to ", 25)) /* Portal? */
    return (handle_portal(what));

  if (!strncmp(what, "You sense that ", 15)) { /* Handle pages. */
    do_beep();
    if (should_hilite_pages())
      hilite_on();
    print_with_wrap(what);
    if (should_hilite_pages())
      hilite_off();
    return (1);
  }

  extract_first_word(what, name);	/* Get first word (speaker). */

  if (is_other_whisper(what))		/* Check for whispers to others. */
    return (1);

  if (should_hilite(what, name)) {	/* Check for hiliting. */
    hilite_on();
    print_with_wrap(what);
    hilite_off();
    return (1);
  }

  if (is_gagged(what, name))		/* Check for gagging. */
    return (1);

  return (0);				/* Otherwise, just pass it through. */
}

extract_first_word(what, name)
  char *what, *name;
{
  register char *place, save;

  place = index(what, ' ');
  if (place != NULL) {
    save = *place;
    *place = '\0';
  }

  strncpy(name, what, SMALLSTR);	/* Extract the name. */
  name[SMALLSTR] = '\0';

  if (place != NULL)			/* Restore killed character. */
    *place = save;
}

  /* Handle portals. */
  /* Format is: ... to WORLD@IP-addr (hostname) port ??? #### */

int handle_portal(what)
  char *what;
{
  string world, address, port, temp;
  int count;
  char *place, ch;
  register world_rec *where;

  count = sscanf(what, "#### Please reconnect to %s %s port %s ####",
		 world, address, port);
  if (count != 3)
    return (0);				/* Invalid format. */

  place = index(world, '@');		/* Strip off IP address. */
  *place = '\0';

  ch = *(place+1);
  if ((ch >= '0') && (ch <= '9'))	/* Use IP address if known. */
    strcpy(address, place+1);		/* May not have nameservice--use IP. */
  else {
    strcpy(temp, address+1);		/* Strip parens. */
    strcpy(address, temp);
    address[strlen(address)-1] = '\0';
  }

  where = find_world(world);		/* See if we know about it. */
  if (where == NULL) {			/* Nope, build a new one instead. */
    where = (world_rec *) malloc(sizeof(world_rec)); /* Never reclaimed! */
    where->next = NULL;
    strcpy(where->world, world);
    *(where->character) = '\0';
    *(where->pass) = '\0';
    strcpy(where->address, address);
    strcpy(where->port, port);
  }

  if (connect_to(where))
    magic_login();
  else
    fprintf(stderr,"%% Connection through portal failed.\n");

  return (1);				/* Yes, it was a portal. */
}

init_beep()
{
  beep_count = 0;
}

do_beep()
{
  if (beep_count != 0)
    write(1, "\007\007\007\007\007", beep_count);
}

set_beep(n)
  int n;
{
  if (n > MAX_BEEPS)
    beep_count = MAX_BEEPS;
  else
    beep_count = n;
}
