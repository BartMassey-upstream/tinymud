/************************************************************************/
/* TinyTalk output special processing: gags and hiliting.		*/
/*									*/
/*	Version 1.0 [ 2/ 2/90] : Created by ABR.			*/
/*		1.1 [ 2/ 5/90] : Added whisper gagging.			*/
/*		1.2 [ 2/ 5/90] : Minor changes to fix file problems.	*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

extern char *index(), *malloc();

typedef struct gag_hilite_entry {
  struct gag_hilite_entry *next, *prev;
  smallstr person;
} gag_hilite_entry;

gag_hilite_entry *find_person();

static int hilite_pages, hilite_whispers, hilite_other;
static int gag_enabled, gag_whispers;
static gag_hilite_entry *gag_list, *hilite_list;

init_hiliting()
{
  init_hilite_utils();
  hilite_pages = FALSE;
  hilite_whispers = FALSE;
  hilite_other = TRUE;
  hilite_list = NULL;
}

init_gagging()
{
  gag_whispers = FALSE;
  gag_enabled = TRUE;
  gag_list = NULL;
}

int is_gagged(what, name)
  char *what, *name;
{
  register char *place;

  if (!gag_enabled)
    return (0);

  if (find_person(gag_list, name) != NULL) { /* Are they gagged? */
					/* Don't gag kills, arrivals, or */
					/* departures. */

    place = index(what, ' ');
    if (place != NULL) {
      if (!strcmp(place+1, "has arrived."))
	return (0);
      if (!strcmp(place+1, "has left."))
	return (0);
      if (!strcmp(place+1, "killed you!"))
	return (0);
      if (!strcmp(place+1, "tried to kill you!"))
	return (0);
    }
    return (1);
  }
  else
    return (0);
}

int should_hilite(what, name)
  char *what, *name;
{
  if (hilite_whispers && is_my_whisper(what))
    return (1);

  if (!hilite_other)
    return (0);

  if (find_person(hilite_list, name) != NULL) /* Are they marked for hilite? */
    return (1);
  else
    return (0);
}

int is_my_whisper(s)			/* Check if a string is a whisper. */
  char *s;				/* This is a 'whisper-to-me' type. */
{
  register char *p;

  p = index(s, ' ');
  if (p == NULL)			/* Whispers always have spaces. */
    return (0);

  if (strncmp(p+1, "whispers", 8))	/* They always have this. */
    return (0);

  if (s[strlen(s)-1] == '"')		/* They always end with a quote. */
    return (1);
  else
    return (0);
}

int is_other_whisper(s)			/* Check if a string is a whisper. */
  char *s;				/* This is a 'whisper-to-other'. */
{
  register char *p;

  if (!gag_whispers)			/* Not gagging whispers, ignore. */
    return (0);

  p = index(s, ' ');
  if (p == NULL)			/* Whispers always have spaces. */
    return (0);

  if (strncmp(p+1, "whispers something to ", 22))  /* They always have this. */
    return (0);

  if (s[strlen(s)-1] == '.')		/* They always end with a quote. */
    return (1);
  else
    return (0);
}

int should_hilite_pages()
{
  return (hilite_pages);
}

enable_hiliting()
{
  hilite_other = TRUE;
}

disable_hiliting()
{
  hilite_other = FALSE;
}

enable_page_hiliting()
{
  hilite_pages = TRUE;
}

disable_page_hiliting()
{
  hilite_pages = FALSE;
}

enable_whisper_hiliting()
{
  hilite_whispers = TRUE;
}

disable_whisper_hiliting()
{
  hilite_whispers = FALSE;
}

enable_gagging()
{
  gag_enabled = TRUE;
}

disable_gagging()
{
  gag_enabled = FALSE;
}

enable_whisper_gagging()
{
  gag_whispers = TRUE;
}

disable_whisper_gagging()
{
  gag_whispers = FALSE;
}

add_hilite(who)
  char *who;
{
  add_person(&hilite_list, who);
}

remove_hilite(who)
  char *who;
{
  remove_person(&hilite_list, who);
}

list_hilite()
{
  list_people(hilite_list);
  if (hilite_other)
    puts("% ** Hiliting is enabled.");
  else
    puts("% ** Hiliting is disabled.");
  if (hilite_pages)
    puts("% ** Hiliting of pages is enabled.");
  else
    puts("% ** Hiliting of pages is disabled.");
  if (hilite_whispers)
    puts("% ** Hiliting of whispers is enabled.");
  else
    puts("% ** Hiliting of whispers is disabled.");
}

save_hilite(name)
  char *name;
{
  save_people("/hilite ", hilite_list, name, "~/tiny.hilite", TRUE);
}

add_gag(who)
  char *who;
{
  add_person(&gag_list, who);
}

remove_gag(who)
  char *who;
{
  remove_person(&gag_list, who);
}

list_gag()
{
  list_people(gag_list);
  if (gag_enabled)
    puts("% ** Gagging is enabled.");
  else
    puts("% ** Gagging is disabled.");
}

save_gag(name)
  char *name;
{
  save_people("/gag ", gag_list, name, "~/tiny.gag", FALSE);
}

add_person(list, name)
  gag_hilite_entry **list;
  register char *name;
{
  register gag_hilite_entry *new;

  if (strlen(name) > SMALLSTR)		/* Silently truncate long names */
    name[SMALLSTR] = '\0';

  if (find_person(*list, name) != NULL)	/* Already there, just leave 'em. */
    return;

  new = (gag_hilite_entry *) malloc(sizeof(struct gag_hilite_entry));
  new->prev = NULL;			/* Add at front of list */
  new->next = *list;
  if (new->next != NULL)		/* If not at tail of list, */
    new->next->prev = new;		/* Set up backlink. */
  *list = new;

  strcpy(new->person, name);
}

remove_person(list, name)
  gag_hilite_entry **list;
  register char *name;
{
  register gag_hilite_entry *where;

  if (strlen(name) > SMALLSTR)		/* Silently truncate long names */
    name[SMALLSTR] = '\0';

  where = find_person(*list, name);
  if (where == NULL) {
    fprintf(stderr, "%% Person %s was not in the list.\n", name);
    return;
  }

  if (where->prev == NULL)		/* Unlink from head */
    *list = where->next;
  else
    where->prev->next = where->next;

  if (where->next != NULL)		/* If not at end, fix back link */
    where->next->prev = where->prev;

  free(where);				/* Free allocated storage. */
}

gag_hilite_entry *find_person(list, name)
  gag_hilite_entry *list;
  register char *name;
{
  register gag_hilite_entry *where;

  where = list;
  while ((where != NULL) && (!equalstr(where->person, name)))
    where = where->next;

  return (where);
}

list_people(list)
  gag_hilite_entry *list;
{
  register gag_hilite_entry *where;

  where = list;
  while (where != NULL) {
    printf("%% %s\n", where->person);
    where = where->next;
  }
}

save_people(cmd, list, name, def, hilite_info)	/* Write people to a file. */
  char *cmd;
  gag_hilite_entry *list;
  char *name, *def;
  int hilite_info;
{
  register FILE *the_file;
  register gag_hilite_entry *where;

  if (name[0] == '\0')			/* Set default name if none given. */
    strcpy(name, def);

  expand_filename(name);
  the_file = fopen(name, "w");		/* Open file */
  if (the_file == NULL) {
    fprintf(stderr,"%% Could not write to %s .\n", name);
    return;
  }

  if (hilite_info) {
    fputs((hilite_pages ? "/hilite pages\n" : "/hilite nopages\n"), the_file);
    fputs((hilite_whispers ? "/hilite whispers\n" : "/hilite nowhispers\n"),
	  the_file);
    fputc('\n', the_file);
  };

  where = list;
  while (where != NULL) {		/* Scan linked list, outputting */
    fputs(cmd, the_file);		/* a command per person. */
    fputs(where->person, the_file);
    fputc('\n', the_file);
    where = where->next;
  }

  fclose(the_file);
}
