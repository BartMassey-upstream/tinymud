/************************************************************************/
/* TinyTalk: the real good stuff (network connections, main loop).	*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*	    	1.1 [ 1/25/90] : Changed for systems without FD_SET.	*/
/*		1.2 [ 1/26/90] : Added "quiet" and "nologin" control.	*/
/*		1.3 [ 1/27/90] : Increased max number of 'quiet' lines.	*/
/*		1.4 [ 2/ 2/90] : Moved out special output handling.     */
/*				 Changed connection processing.		*/
/*		1.5 [ 2/ 5/90] : Modify to handle switching MUDs more   */
/*				 cleanly.  Fix gag problem.		*/
/*		1.6 [ 2/16/90] : Integrated support for System V and    */
/*				 HP-UX, from Andy Norman and Kipp       */
/*				 Hickman.				*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <stdio.h>

  /* For some odd systems, which don't put this in errno.h. */

extern int errno;

  /* For BSD 4.2 systems. */

#ifndef FD_ZERO
#define DESCR_MASK int
#define FD_ZERO(p) (*p = 0)
#define FD_SET(n,p) (*p |= (1<<(n)))
#define FD_ISSET(n,p) (*p & (1<<(n)))
#else
#define DESCR_MASK fd_set		/* For BSD 4.3 systems. */
#endif

  /* End of BSD 4.2 systems. */

#define REFRESH_TIME 500000		/* Microseconds */

extern char *index(), *malloc();
extern struct hostent *gethostbyname();
extern unsigned long inet_addr();
extern world_rec *find_world();

static world_rec *current_world;
static int current_socket;
static int connected;			/* Are we connected anywhere? */
hugestr current_output;			/* Queued as messages arrive. */
static int need_refresh;		/* Does input line need refresh? */
static int done;			/* Are we all done? */
static int use_magic_login;		/* Auto-logins enabled? */
static int flush_pending_output;	/* For portals. */

do_stuff(initial_world)
  world_rec *initial_world;
{
  int count;
  DESCR_MASK readers, exceptions;
  struct timeval refresh_timeout;
  struct timeval *tv;

  flush_pending_output = FALSE;

  connected = FALSE;
  if (!connect_to(initial_world))
    die("%% Couldn't connect to initial world.\n");

  *current_output = '\0';		/* No output yet. */

  done = FALSE;

  need_refresh = 0;			/* No keyboard typing yet. */

  magic_login();			/* Log us in, if possible. */

  do {
    flush_pending_output = FALSE;	/* No output is pending, ignore any */
					/* attempts to flush it. */

    FD_ZERO(&readers);
    FD_ZERO(&exceptions);
    FD_SET(0, &readers);		/* Check standard input. */
    FD_SET(current_socket, &readers);	/* Check socket. */

    if (need_refresh) {
      refresh_timeout.tv_sec = 0;
      refresh_timeout.tv_usec = REFRESH_TIME;
      tv = &refresh_timeout;
    }
    else
      tv = NULL;

    count = select(current_socket + 1, &readers, NULL, &exceptions, tv);
    if (count == -1) {
      if (errno != EINTR)		/* Control-Z will do this. */
	perror("select");
    }
    else if (count == 0)
      do_refresh();
    else {
      if (FD_ISSET(current_socket, &readers))
	handle_socket_input();
      else if (FD_ISSET(0, &readers)) {
	if (need_refresh)
	  do_refresh();
	handle_keyboard_input();
      }
      else
	fprintf(stderr,"%% ??select??");
    }

  } while (!done);

  disconnect();
  cleanup_keyboard();
}

int connect_to(w)			/* Try to make a connection. */
  register world_rec *w;
{
  struct in_addr host_address;
  struct sockaddr_in socket_address;
  int err;

  register world_rec *temp_world;	/* Use temporaries until we know */
  register int temp_socket;		/* that we've succeeded! */

  temp_world = w;
  if (!get_host_address(w->address, &host_address)) { /* Can't do it. */
    return(0);
  }

  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(atoi(w->port));
  bcopy(&host_address, &socket_address.sin_addr, sizeof(struct in_addr));

  temp_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (temp_socket < 0) {
    perror("% Couldn't open socket");
    return(0);
  }

  err = connect(temp_socket, &socket_address, sizeof(struct sockaddr_in));
  if (err < 0) {
    perror("% Couldn't connect to socket");
    return(0);
  }

#ifdef FNDELAY
  fcntl(temp_socket, F_SETFL, FNDELAY);    /* Do we need this? */
#endif

  if (connected)			/* Disconnect from any open world. */
    disconnect();

  current_world = temp_world;
  current_socket = temp_socket;

  connected = TRUE;
  return(1);				/* Success! */
}

int get_host_address(name, addr)	/* Get a host address. */
  register char *name;
  register struct in_addr *addr;
{
  struct hostent *blob;
  union {				/* %#@!%!@%#!@ idiot who designed */
    long signed_thingy;			/* the inetaddr routine.... */
    unsigned long unsigned_thingy;
  } thingy;

  if (*name == '\0') {
    fprintf(stderr, "%% No host address specified.\n");
    return (0);
  }

  if ((*name >= '0') && (*name <= '9')) {	/* IP address. */
    addr->s_addr = inet_addr(name);
    thingy.unsigned_thingy = addr->s_addr;
    if (thingy.signed_thingy == -1) {
      fprintf(stderr, "%% Couldn't find host %s .\n", name);
      return (0);
    }
  }
  else {				/* Host name. */
    blob = gethostbyname(name);

    if (blob == NULL) {
      fprintf(stderr, "%% Couldn't find host %s .\n", name);
      return (0);
    }

    bcopy(blob->h_addr, addr, sizeof(struct in_addr));
  }

  return (1);				/* Success. */
}

disconnect()				/* Disconnect from current world. */
{
  if (connected) {
    flush_pending_output = TRUE;	/* Flush pending output.  This is */
    current_output[0] = '\0';		/* used when switching worlds. */
    close(current_socket);
  }

  connected = FALSE;
}

transmit(s, l)				/* Send a message over the socket. */
  char *s;
  int l;
{
  register int err;

  err = send(current_socket, s, l, 0);
  if (err == -1)
    perror("send failed");
}

receive(s)
  register char *s;
{
  register int count;

  count = recv(current_socket, s, MAXSTRLEN, 0);
  if (count == -1) {
    if (errno == EWOULDBLOCK)
      s[0] = '\0';
    else {
      perror("recv failed");
    }
  }
  else
    s[count] = '\0';

  if (count <= 0)			/* Don't ask me. */
    done = TRUE;
}

magic_login()
{
  string s;
  register world_rec *def;

  if (!use_magic_login)			/* Check if auto-login is enabled. */
    return;

  if (*(current_world->character) == '\0') { /* Try default world. */
    def = find_world("default");
    if (def == NULL) {			/* Just send CR for sync. */
      transmit("\n", 1);
      return;
    }
    else {
      sprintf(s,"\nconnect %s %s\n", def->character, def->pass);
      transmit(s, strlen(s));
      start_quiet();			/* Suppress login messages? */
      return;
    }
  }
  else {
    sprintf(s,"\nconnect %s %s\n", current_world->character,
	    current_world->pass);
    transmit(s, strlen(s));
    start_quiet();			/* Suppress login message? */
    return;
  }
}

clear_refresh_pending()
{
  need_refresh = 0;
}

set_refresh_pending()
{
  need_refresh = 1;
}

handle_socket_input()
{
  string blob;
  hugestr bigblob;
  register char *place;

  receive(blob);
  strcat(current_output, blob);		/* Concatenate this bunch of input. */

  place = index(current_output, '\n');	/* Output any whole lines. */
  if (place != NULL) {
    while (place != NULL) {
      *place = NULL;
      if (!special_hook(current_output))
	print_with_wrap(current_output);
      if (flush_pending_output) {	/* Flush the buffer. */
	flush_pending_output = FALSE;
	*current_output = '\0';
	return;
      }
      strcpy(bigblob, place + 1);	/* Rest of buffer. */
      strcpy(current_output, bigblob);	/* Copy it back to buffer. */
      place = index(current_output, '\n');
    }
  }

  if (strlen(current_output) > MAXSTRLEN) { /* Too long, flush it. */
    print_with_wrap(current_output);
    *current_output = '\0';		/* Flush the buffer. */
  }
}

enable_auto_login()
{
  use_magic_login = TRUE;
}

disable_auto_login()
{
  use_magic_login = FALSE;
}

set_done()				/* Used by /QUIT command to exit */
{					/* cleanly. */
  done = TRUE;
}
