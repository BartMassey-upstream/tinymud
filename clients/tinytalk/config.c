/************************************************************************/
/* TinyTalk configuration file support.					*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*		1.1 [ 1/25/90] : Added comments.			*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

extern char *malloc(), *getenv();

static world_rec *the_worlds, *default_world;

int read_configuration()
{
  string config_name;
  register FILE *config_file;

  get_file_name(config_name);
  config_file = fopen(config_name, "r");
  if (config_file == NULL) {
/*  fprintf(stderr, "Couldn't open configuration file %s .\n", config_name); */
    return (1);				/* Succeed even so. */
  }

  parse_configuration(config_file);	/* Ignore any errors. */

  fclose(config_file);			/* Hope it works.... */

  return (1);				/* Always succeed, this is optional. */
}

get_file_name(s)
  register char *s;
{
  register char *env;

  env = getenv("TINYTALK");		/* Check TINYTALK first. */
  if (env != NULL)
    strcpy(s, env);
  else
    strcpy(s, "~/.tinytalk");
  expand_filename(s);
}

parse_configuration(f)
  FILE *f;
{
  if (parse_worlds(f))			/* Returns 0 at EOF or error */
    parse_commands(f);
}

int parse_worlds(f)
  FILE *f;
{
  string line;
  int done, eof_flag;

  the_worlds = NULL;
  default_world = NULL;
  done = FALSE;
  eof_flag = FALSE;

  while (!done) {
    if (fgets(line, MAXSTRLEN, f) == NULL) {
      done = TRUE;
      eof_flag = TRUE;
      continue;
    }

    if (line[0] == ';')			/* Comment */
      continue;

    line[strlen(line)-1] = '\0';	/* Strip newline */

    if (line[0] == '\0') {		/* Blank line? */
      done = TRUE;
      continue;
    }

    if (!add_world(line))
      done = TRUE;
  }

  return (!eof_flag);
}

int add_world(line)			/* *** WARNING, LENGTH LIMITED *** */
  char *line;
{
  register world_rec *new;
  int count;

  new = (world_rec *) malloc(sizeof(world_rec));
  count = sscanf(line, "%31s %31s %31s %31s %31s", new->world, new->character,
		 new->pass, new->address, new->port);
  if (count == 3) {			/* No address info */
    *(new->address) = '\0';
    *(new->port) = '\0';
    goto done;
  }
  else if (count == 5) {		/* All info specified */
    if (!strcmp(new->world, "default"))
      fprintf(stderr,"Warning: address for default world ignored");
    goto done;
  }
  else {
    fprintf(stderr,"Could not parse: %s .\n", line);
    return (0);
  }

done:

  if ((the_worlds == NULL) && (count == 5))	/* Set up default world, */
    default_world = new;			/* if first one and all info. */

  new->next = the_worlds;
  the_worlds = new;

  return (1);
}

parse_commands(f)
  FILE *f;
{
  /* All commands ought to be the same as from the keyboard. */
  /* We hand them over there for processing. */

  string line;
  int done;

  done = FALSE;

  while (!done) {
    if (fgets(line, MAXSTRLEN, f) == NULL) {
      done = TRUE;
      break;
    }

    line[strlen(line)-1] = '\0';	/* Strip newline */

    if ((line[0] == '\0') || (line[0] == ';'))	/* Blank line or comment. */
      continue;				/* Ignore it. */

    if (*line != '/') {			/* Not a command! */
      fprintf(stderr,"Not a command: %s .\n", line);
      continue;
    }

    handle_command(line, TRUE);		/* Special-case errors since we */
					/* aren't started up yet. */
  }					/* Actually, doesn't do anything now. */
  
}

world_rec *get_default_world()
{
  return (default_world);
}

world_rec *find_world(name)
  register char *name;
{
  register world_rec *p;

  p = the_worlds;
  while ((p != NULL) && !(equalstr(p->world, name)))
    p = p->next;

  return(p);
}

world_rec *get_world_header()
{
  return (the_worlds);
}
