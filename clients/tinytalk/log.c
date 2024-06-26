/************************************************************************/
/* TinyTalk logging.							*/
/*									*/
/*	Version 1.0 [ 1/25/90] : Initial implementation by ABR.		*/
/*		1.1 [ 2/ 5/90] : Added explicit file flushing.		*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

static FILE *log_file;
static string log_file_name;
static int log_on, log_me;

init_logging()
{
  strcpy(log_file_name, "~/tiny.log");	/* In home directory. */
  expand_filename(log_file_name);
  log_on = 0;				/* Don't log output */
  log_me = 0;				/* Don't log input either */
  log_file = NULL;			/* No log file is open. */
}

enable_logging(name)			/* Enable logging.  Null name means */
  char *name;				/* use current default. */
{
  if (*name != '\0') {
    strcpy(log_file_name, name);
    expand_filename(log_file_name);
  }

  if (log_file != NULL)			/* Close current log file */
    fclose(log_file);

  log_file = fopen(log_file_name, "a");	/* Append to log file */
  log_on = TRUE;
  if (log_file == NULL) {
    fprintf(stderr,"%% Could not open log file %s.\n", log_file_name);
    log_on = FALSE;
  }
}

disable_logging()
{
  log_on = FALSE;			/* Turn off logging */
  if (log_file != NULL)	{		/* Close file if open */
    fclose(log_file);
    log_file = NULL;
  }
}

flush_logfile()				/* Flush log file to disk. */
{
  if (log_file != NULL)
    fflush(log_file);
}

enable_logme()
{
  log_me = TRUE;
}

disable_logme()
{
  log_me = FALSE;
}

log_output(s)				/* From server */
  char *s;
{
  if (log_on && (log_file != NULL)) {
    fputs(s, log_file);
    fputc('\n', log_file);
  }
}

log_input(s)				/* From keyboard */
  char *s;
{
  if (log_on && log_me && (log_file != NULL)) {
    fputs(s, log_file);
    fputc('\n', log_file);
  }
}
