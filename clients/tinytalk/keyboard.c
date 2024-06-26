/************************************************************************/
/* TinyTalk keyboard handler.						*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*		1.1 [ 1/25/90] : Added minimal termcap support.		*/
/*		1.2 [ 1/26/90] : Clean up terminal state on ^C.		*/
/*		1.3 [ 2/ 2/90] : Allow interrupts to be ignored.	*/
/*				 Optionally use stty information.	*/
/*		1.4 [ 2/ 5/90] : Add minimal command recall.  Flush log	*/
/*				 file on suspend.  Fix problem with gag	*/
/*				 causing spurious input erasure.	*/
/*		1.5 [ 2/16/90] : Integrated support for System V and	*/
/*				 HP-UX, from Andy Norman and Kipp	*/
/*				 Hickman.				*/
/*		1.6 [ 6/25/90] : Fixed a bug in /logme; thanks to	*/
/*				 Jean-Remy Facq for reporting it and	*/
/*				 contributing a solution.  Also fixed a	*/
/*				 bug in recall of lines starting with	*/
/*				 //; thanks to "Insane Hermit" for	*/
/*				 reporting this (a LONG time ago).	*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <signal.h>

#ifdef HPUX

#include <sys/ioctl.h>
#include <termio.h>
#include <bsdtty.h>

#else /* not HPUX */
#ifdef SYSVTTY

#include <termio.h>

#else /* BSD */

#include <sys/ioctl.h>
#include <sgtty.h>

#endif /* SYSVTTY */
#endif /* HPUX */

#include <errno.h>
#include <stdio.h>

  /* For some odd systems, which don't put this in errno.h. */

extern int errno;

#ifdef TERMCAP
extern char *getenv();
static int have_clear;
static char start_of_line[16], clear_to_eol[16];  /* Warning, short! */
static char write_buffer[64];			  /* Also rather short.... */
static char *write_buffer_ptr;
#endif

#define DEFAULT_COLUMNS 80

static int keyboard_inited = FALSE;	/* Initialized yet? */

static int num_columns;			/* Number of columns on terminal. */
static int current_column;		/* Current input column (0->nc). */
static string keyboard_buffer;		/* Buffer for current input. */
static string recall_buffer;		/* Buffer for previous input. */
static int keyboard_pos;		/* Characters entered so far. */
static int recall_pos;			/* Characters on previous input. */
static int allow_intr;			/* Should we quit on INTR? */
static int recall_pending;		/* Flag: recall previous string. */
static int do_not_save_recall;		/* Flag: don't save recall buffer. */

static char k_delete, k_delete_2;	/* Backspace character(s). */
static char k_erase, k_erase_2;		/* Erase-line character(s). */
static char k_word;			/* Erase-word character. */
static char k_refresh;			/* Refresh-line character. */

sigint_handler()			/* Restore terminal state; cleanup. */
{
  if (allow_intr)			/* Don't exit if this is disabled. */
    die("\n\nInterrupt, exiting.\n");	/* Exit through error handler. */
}

sigtstp_handler()			/* Leave terminal alone! */
{
  flush_logfile();

  cooked_echo_mode();
  kill(getpid(),SIGSTOP);
  cbreak_noecho_mode();

  if (keyboard_pos != 0)		/* If there is input, we want to */
    set_refresh_pending();		/* refresh when we start back up. */
}

preinit_keyboard()
{
  allow_interrupts();
  no_use_stty();

  do_not_save_recall = FALSE;
  recall_pending = FALSE;
  recall_pos = 0;
}

allow_interrupts()
{
  allow_intr = TRUE;
}

disallow_interrupts()
{
  allow_intr = FALSE;
}

#ifdef HPUX

use_stty()
{
  struct termio te_blob;
  struct ltchars lt_blob;

  if (ioctl(0, TCGETA, &te_blob) == -1)
    perror("TCGETA ioctl");

  if (ioctl(0, TIOCGLTC, &lt_blob) == -1)
    perror("TIOCGLTC ioctl");

  k_delete = k_delete_2 = te_blob.c_cc[VERASE];
  k_erase = k_erase_2 = te_blob.c_cc[VKILL];

  k_word = lt_blob.t_werasc;
  k_refresh = lt_blob.t_rprntc;
}

#else /* not HPUX */
#ifdef SYSVTTY

use_stty()
{
  struct termio tcblob;

  if (ioctl(0, TCGETA, &tcblob) == -1)
    perror("TCGETA ioctl");

  k_delete = k_delete_2 = tcblob.c_cc[VERASE];
  k_erase = k_erase_2 = tcblob.c_cc[VKILL];

#ifdef sgi

  k_word = tcblob.c_cc[VWERASE];
  k_refresh = tcblob.c_cc[VRPRNT];

#else

  k_word = '\027';			/* Control-W. */
  k_refresh = '\022';			/* Control-R. */

#endif /* sgi */

}

#else /* BSD */

use_stty()
{
  struct sgttyb sg_blob;
  struct ltchars lt_blob;

  if (ioctl(0, TIOCGETP, &sg_blob) == -1)
    perror("TIOCGETP ioctl");
  if (ioctl(0, TIOCGLTC, &lt_blob) == -1)
    perror("TIOCGLTC ioctl");

  k_delete = k_delete_2 = sg_blob.sg_erase;
  k_erase = k_erase_2 = sg_blob.sg_kill;

  k_word = lt_blob.t_werasc;
  k_refresh = lt_blob.t_rprntc;
}

#endif /* SYSVTTY */
#endif /* HPUX */

no_use_stty()
{
  k_delete = '\010';			/* Backspace. */
  k_delete_2 = '\177';			/* Delete. */

  k_erase = '\025';			/* Control-U. */
  k_erase_2 = '\030';			/* Control-X. */

  k_word = '\027';			/* Control-W. */

  k_refresh = '\022';			/* Control-R. */
}

init_keyboard()
{
#ifdef TIOCGWINSZ
  struct winsize size;
#endif
#ifdef HPUX
  struct sigvec hpvec;
#endif

  cbreak_noecho_mode();

#ifdef TERMCAP
  get_termcap_info();			/* Must precede TIOCGWINSZ code */
#else
  num_columns = DEFAULT_COLUMNS;	/* This is important! */
#endif

#ifdef TIOCGWINSZ

  if (ioctl(0, TIOCGWINSZ, &size) == -1)
    perror("TIOCGWINSZ ioctl");

  num_columns = size.ws_col;
  if (num_columns <= 20)		/* Suspicious, reset it. */
    num_columns = DEFAULT_COLUMNS;

#endif /* TIOCGWINSZ */

  set_default_wrap(num_columns);

  keyboard_pos = 0;			/* No input yet. */
  current_column = 0;			/* Left side of screen. */

#ifdef HPUX

  hpvec.sv_handler = (void (*) ()) sigint_handler;
  hpvec.sv_onstack = 0;
  hpvec.sv_mask = 0;
  sigvector(SIGINT,&hpvec,0);
  hpvec.sv_handler = (void (*) ()) sigtstp_handler;
  sigvector(SIGTSTP,&hpvec,0);

#else /* BSD */

  signal(SIGINT, sigint_handler);	/* Control-C. */
  signal(SIGTSTP, sigtstp_handler);	/* Control-Z. */

#endif

  keyboard_inited = TRUE;
}

#ifdef TERMCAP

get_termcap_info()
{
  char blob[1024];			/* As per termcap man page */
  char *terminal_name, *temp;

  terminal_name = getenv("TERM");
  if (terminal_name == NULL) {
    have_clear = FALSE;
    return;
  }

  if (tgetent(blob, terminal_name) == 1) {
    num_columns = tgetnum("co");	/* Try to get # of columns. */
    if (num_columns == -1)		/* Not specified? */
      num_columns = DEFAULT_COLUMNS;

    temp = start_of_line; 
    if (tgetstr("cr",&temp) == NULL) {
      start_of_line[0] = '\r';
      start_of_line[1] = '\0';
    }
    temp = clear_to_eol;
    if (tgetstr("ce",&temp) == NULL)
      have_clear = FALSE;
    else
      have_clear = TRUE;
  }
  else					/* Couldn't read termcap entry */
    have_clear = FALSE;
}

#endif

cleanup_keyboard()
{
  cooked_echo_mode();
  signal(SIGTSTP, NULL);
}

handle_keyboard_input()			/* Read input, one char at a time. */
{
  char ch_noreg;
  register char ch;
  int count;

  count = read(0, &ch_noreg, 1);	/* Read from stdin. */
  if (count == 0)			/* Huh?  Ignore this. */
    return;

  if (count == -1) {
    if (errno == EWOULDBLOCK)
      return;
    else
      die("%% Couldn't read keyboard.\n");
  }

  ch = ch_noreg;			/* For stupid compilers. */

  if (ch == '\n') {
    write(1, &ch_noreg, 1);
    process_buffer();
    if (recall_pending) {
      recall_pending = FALSE;

      keyboard_pos = recall_pos;
      bcopy(recall_buffer, keyboard_buffer, recall_pos); /* Retrieve buffer. */
      current_column = keyboard_pos % num_columns;

      refresh_entire_buffer();
    }
    else {
      keyboard_pos = 0;
      current_column = 0;
    }
  }
  else {
    if ((ch == k_delete) || (ch == k_delete_2)) /* Backspace. */
      handle_backspace();
    else if ((ch == k_erase) || (ch == k_erase_2)) { /* Cancel line. */
      keyboard_pos = 0;

      erase_keyboard_input(TRUE);
      clear_refresh_pending();

      current_column = 0;
    }
    else if (ch == k_word) {		/* Erase word (back to space). */
					/* This is very, very, VERY ugly. */

      handle_backspace();		/* Always delete at least one char. */

      while ((keyboard_pos != 0) && (keyboard_buffer[keyboard_pos-1] != ' '))
	handle_backspace();
    }
    else if (ch == k_refresh) {		/* Refresh entire input. */
      refresh_entire_buffer();
    }
    else if ((ch < ' ') || (keyboard_pos == MAXSTRLEN - 1)) { /* Beep! */
      write(1, "\007", 1);
    }
    else {
      keyboard_buffer[keyboard_pos++] = ch;
      current_column = (current_column + 1) % num_columns;
      write(1, &ch_noreg, 1);
    }
  }
}

handle_backspace()			/* Do a backspace.... */
{
  if (keyboard_pos != 0) {
    keyboard_pos--;

    if (current_column != 0) {		/* Can do BS, SPC, BS. */
      write(1, "\010 \010", 3);
      current_column--;
    }
    else {
      current_column = num_columns - 1;	/* Wrapped around. */
      do_refresh();			/* Will redraw the right part. */
    }
  }
}

process_buffer()
{
  register int kp = keyboard_pos;	/* Help the compiler optimize.	   */
					/* keyboard_pos is static, and	   */
					/* the compiler doesn't know if	   */
					/* transmit() or log_input() might */
					/* change it (they don't).	   */

  if (kp == 0)				/* Nothing typed. */
    return;

  if (keyboard_buffer[0] == '/') {
    if (keyboard_buffer[1] == '/') {	/* Two slashes, pass a single one. */
      keyboard_buffer[kp] = '\n';
      transmit(keyboard_buffer + 1, kp);
      keyboard_buffer[kp] = '\0';
      log_input(keyboard_buffer + 1);
    }
    else {				/* Single slash, it's a command. */
      keyboard_buffer[kp] = '\0';
      handle_command(keyboard_buffer, FALSE);
      log_input(keyboard_buffer);	/* Should this be logged? */
    }
  }
  else {
    keyboard_buffer[kp] = '\n';
    transmit(keyboard_buffer, kp + 1);
    keyboard_buffer[kp] = '\0';
    log_input(keyboard_buffer);
  }

  if (!do_not_save_recall) {		/* Don't destroy buffer on /RECALL! */
    recall_pos = kp;
    bcopy(keyboard_buffer, recall_buffer, kp); /* Save it away. */
  }
  else
    do_not_save_recall = FALSE;
}

refresh_entire_buffer()			/* Refresh all lines of buffer. */
{
  register int i;

  erase_keyboard_input(TRUE);		/* Start at beginning of line. */
					/* Dump full lines. */
  for (i = 0; i <= keyboard_pos - num_columns; i += num_columns) {
    write(1, keyboard_buffer+i, num_columns);
    write(1, "\n", 1);
  }

  do_refresh();			/* Handle possibly partial tail. */
}

do_refresh()
{
  int loc;				/* Place to start refresh from */
  int modulo;

  modulo = keyboard_pos % num_columns;	/* Number on last line */
  loc = keyboard_pos - modulo;

  if (modulo != 0)
    write(1, &keyboard_buffer[loc], modulo);

  clear_refresh_pending();
}

#ifdef TERMCAP
write_one(c)
  char c;
{
  *(write_buffer_ptr++) = c;
}
#endif

erase_keyboard_input(forced)
  int forced;				/* Forced output? */
{
  if ((!forced) && (current_column == 0))
    return;

#ifdef TERMCAP

  if (have_clear) {			/* Can we use termcap now? */
    write_buffer_ptr = write_buffer;
    tputs(start_of_line, 1, write_one);
    tputs(clear_to_eol, 1, write_one);
    write(1, write_buffer, write_buffer_ptr - write_buffer);
    goto end_termcap;
  }

  /* Drops through into non-TERMCAP code otherwise... */

#endif

  {
    register int temp;
    string buffer;			/* Should be long enough for 80*3. */
    register char *ptr;

    temp = current_column;
    ptr = buffer;

    while (temp != 0) {			/* Erase line, backwards. */
      *(ptr++) = '\010';
      *(ptr++) = ' ';
      *(ptr++) = '\010';
      temp--;
    }
    write(1, buffer, ptr - buffer);
  }

end_termcap:

  if (keyboard_pos != 0)		/* If there is input, schedule a */
    set_refresh_pending();		/* refresh event. */

}

avoid_recall()
{
  do_not_save_recall = TRUE;
}

do_keyboard_recall()
{
  if (!keyboard_inited)			/* Dummy, nothing to recall yet! */
    return;

  recall_pending = TRUE;
  do_not_save_recall = TRUE;
}
