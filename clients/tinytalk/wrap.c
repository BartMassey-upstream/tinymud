/************************************************************************/
/* TinyTalk word wrapping.						*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*		1.1 [ 2/ 5/90] : Move input line erasure into here.	*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

extern char *rindex();

static int current_wrap_column;
static int default_wrap_column;

print_with_wrap(s)
  register char *s;
{
  hugestr temp;
  register char *place;
  int loc;

  erase_keyboard_input(FALSE);		/* Clear current line. */

  if (current_wrap_column == -1)
    current_wrap_column = default_wrap_column;

  if ((current_wrap_column == 0) || strlen(s) < current_wrap_column) {
    puts(s);				/* No wrap. */
    log_output(s);
    return;
  }

  strcpy(temp, s);
  do {
    temp[current_wrap_column] = '\0';
    place = rindex(temp,' ');
    if (place == NULL) {		/* Can't wrap, give up */
      puts(temp);
      log_output(temp);
      loc = current_wrap_column;
    }
    else {
      *place = '\0';			/* Terminate string */
      puts(temp);			/* and output it. */
      log_output(temp);
      loc = place - temp + 1;
    }
    strcpy(temp, s + loc);		/* Rest of string. */
    strcpy(s, temp);			/* Should strip double spaces? */
  } while (strlen(s) >= current_wrap_column);

  if (strlen(s) != 0) {
    puts(s);
    log_output(s);
  }
}

enable_wrap(column)			/* Doesn't just set column, because */
  int column;				/* might not have initted keyboard. */
{
  current_wrap_column = ((column == 0) ? -1 : column);
}

disable_wrap()
{
  current_wrap_column = 0;
}

set_default_wrap(column)
  int column;
{
  default_wrap_column = column;
}
