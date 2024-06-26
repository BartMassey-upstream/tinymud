/************************************************************************/
/* TinyTalk utilities.							*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*		1.1 [ 1/25/90] : Moved terminal control here.		*/
/*		1.2 [ 1/25/90] : Modified to set termcap var 'ospeed'.	*/
/*		1.3 [ 2/ 2/90] : Added comparestr() and hiliting.	*/
/*		1.4 [ 2/ 5/90] : Added expand_filename() for $#@! UNIX. */
/*				 Disable its fancy stuff because there  */
/*				 are no shared libraries.		*/
/*		1.5 [ 2/16/90] : Integrated support for System V and    */
/*				 HP-UX, from Andy Norman and Kipp       */
/*				 Hickman.				*/
/*									*/
/************************************************************************/

#include "tl.h"

#ifdef HPUX

#include <sys/ioctl.h>
#include <termio.h>

#else /* not HPUX */
#ifdef SYSVTTY

#include <termio.h>
static int first_time = 1;
static struct termio old_tty_state;

#else /* BSD */

#include <sys/ioctl.h>
#include <sgtty.h>

#endif /* SYSVTTY */
#endif /* HPUX */

#include <pwd.h>
#include <stdio.h>

#undef FANCY

extern char *getenv(), *index();
extern struct passwd *getpwnam();

#ifdef TERMCAP
extern short ospeed;
static int have_hilite;
static char hilite_on_string[16], hilite_off_string[16];
static char write_buffer[64];
static char *write_buffer_ptr;
#endif

uppercase(s, t)
  register char *s, *t;
{
  while (*s) {
    *t = (((*s >= 'a') && (*s <= 'z')) ? *s-32 : *s);
    s++;
    t++;
  }
  *t = '\0';
}

int equalstr(s, t)
  char *s, *t;
{
  string su, tu;

  uppercase(s,su);
  uppercase(t,tu);
  return (!strcmp(su,tu));
}

int comparestr(s, t)
  char *s, *t;
{
  string su, tu;

  uppercase(s,su);
  uppercase(t,tu);
  return (strcmp(su,tu));
}

stripstr(s)				/* Strip leading & trailing */
  register char *s;			/* spaces from a string. */
{
  string temp;
  register char *from;

  if (s[0] == ' ') {			/* Remove leading spaces. */
    from = s + 1;
    while (*from == ' ')
      from++;
    strcpy(temp, from);
    strcpy(s, temp);
  }

  from = s + strlen(s) - 1;
  if (*from == ' ') {
    --from;
    while (*from == ' ')
      --from;
    *(from+1) = '\0';
  }
}

#ifdef HPUX

cbreak_noecho_mode()
{
  struct termio blob;

  if (ioctl(0, TCGETA, &blob) == -1)
    perror("TCGETA ioctl");

  blob.c_lflag &= ~ECHO;
  blob.c_lflag &= ~ECHOE;
  blob.c_lflag &= ~ICANON;
  blob.c_cc[VMIN] = 0;
  blob.c_cc[VTIME] = 0;

  if (ioctl(0, TCSETAF, &blob) == -1)
    perror("TCSETAF ioctl");

}

cooked_echo_mode ()
{
  struct termio blob;

  if (ioctl(0, TCGETA, &blob) == -1)
    perror("TCGETA ioctl");
  
  blob.c_lflag |= ECHO;
  blob.c_lflag |= ECHOE;
  blob.c_lflag |= ICANON;

  if (ioctl(0, TCSETAF, &blob) == -1)
    perror("TCSETAF ioctl");
  
}

#else /* not HPUX */

#ifdef SYSVTTY

cbreak_noecho_mode()			/* When we're running... */
{
  struct termio blob;

  if (first_time) {
    ioctl(0, TCGETA, &old_tty_state);
    first_time = 0;
  }

  ioctl(0, TCGETA, &blob);
  blob.c_cc[VMIN] = 0;
  blob.c_cc[VTIME] = 0;
  blob.c_iflag = IGNBRK | IGNPAR | ICRNL;
  blob.c_oflag = OPOST | ONLCR;
  blob.c_lflag = ISIG;
  ioctl(0, TCSETA, &blob);
}

cooked_echo_mode()			/* When we exit... */
{
  if (!first_time) {
    ioctl(0, TCSETA, &old_tty_state);
  }
}

#else /* BSD */

cbreak_noecho_mode()			/* When we're running... */
{
  struct sgttyb blob;

  if (ioctl(0, TIOCGETP, &blob) == -1)
    perror("TIOCGETP ioctl");
  blob.sg_flags |= CBREAK;
  blob.sg_flags &= ~ECHO;
  if (ioctl(0, TIOCSETP, &blob) == -1)
    perror("TIOCSETP ioctl");

#ifdef TERMCAP
  ospeed = blob.sg_ospeed;
#endif /* TERMCAP */
}

cooked_echo_mode()			/* When we exit... */
{
  struct sgttyb blob;

  if (ioctl(0, TIOCGETP, &blob) == -1)
    perror("TIOCGETP ioctl");
  blob.sg_flags &= ~CBREAK;
  blob.sg_flags |= ECHO;
  if (ioctl(0, TIOCSETP, &blob) == -1)
    perror("TIOCSETP ioctl");
}

#endif /* SYSVTTY */
#endif /* HPUX */

die(why, s1, s2, s3, s4)
  char *why, *s1, *s2, *s3, *s4;
{
  cooked_echo_mode();			/* Reset terminal characteristics. */
  disable_logging();			/* Flush and close log file. */
  fprintf(stderr, why, s1, s2, s3, s4);
  exit(1);
}

init_hilite_utils()
{
#ifdef TERMCAP

  char blob[1024];			/* As per termcap man page */
  char *terminal_name, *temp;

  have_hilite = FALSE;

  terminal_name = getenv("TERM");
  if (terminal_name == NULL)
    return;

  if (tgetent(blob, terminal_name) != 1)
    return;

  temp = hilite_on_string;

  if (tgetstr("md",&temp) != NULL) {
    temp = hilite_off_string;
    if (tgetstr("me",&temp) != NULL)
      have_hilite = TRUE;
  }

  if (!have_hilite) {
    temp = hilite_on_string;

    if (tgetstr("so",&temp) != NULL) {
      temp = hilite_off_string;
      if (tgetstr("se",&temp) != NULL)
	have_hilite = TRUE;
    }
  }

#endif
}

#ifdef TERMCAP
output_one(c)
  char c;
{
  *(write_buffer_ptr++) = c;
}
#endif

hilite_on()
{
#ifdef TERMCAP

  if (have_hilite) {
    write_buffer_ptr = write_buffer;
    tputs(hilite_on_string, 1, output_one);
    write(1, write_buffer, write_buffer_ptr - write_buffer);
  }

#endif
}

hilite_off()
{
#ifdef TERMCAP

  if (have_hilite) {
    write_buffer_ptr = write_buffer;
    tputs(hilite_off_string, 1, output_one);
    write(1, write_buffer, write_buffer_ptr - write_buffer);
  }

#endif
}

expand_filename(s)
  string s;
{
  char *env;
  string temp;

  if (s[0] != '~')			/* Only affect stuff with tildes. */
    return;

  if (s[1] == '/') {			/* Current user's home directory. */
    env = getenv("HOME");
    if (env == NULL)
      temp[0] = '\0';
    else
      strcpy(temp, env);

    strcat(temp, s+1);			/* {home}/whatever */
    strcpy(s, temp);
  }

#ifdef FANCY

  else if (s[1] != '\0') {		/* Someone else's home directory. */
    struct passwd *tt;
    char *loc;

    loc = index(s,'/');
    if (loc != NULL) {			/* No slash ==> we don't care. */
      *loc = '\0';			/* Get rid of slash. */
      setpwent();
      tt = getpwnam(s+1);		/* Ignore leading tilde. */
      endpwent();
      *loc = '/';			/* Restore slash. */
      if (tt != NULL) {			/* Can't find them ==> don't care. */
	strcpy(temp, tt->pw_dir);
	strcat(temp, loc);		/* {their home}/whatever */
	strcpy(s, temp);
      }
    }
  }

#endif /* FANCY */

}
