/************************************************************************/
/* TinyTalk macro handling.						*/
/*									*/
/*	Version 1.0 [ 1/25/90] : Initial implementation by ABR.		*/
/*		1.1 [ 1/26/90] : Added support for listing macros.	*/
/*		1.2 [ 1/27/90] : Fixed bug in add_macro.		*/
/*		1.3 [ 2/ 5/90] : Fixed error message in remove_macro.	*/
/*									*/
/************************************************************************/

#include "tl.h"
#include <stdio.h>

#define MAX_ARGS 4			/* Watch out in process_macro */
					/* if you change this. */

#define NAME_LEN 31			/* Size of a macro's name */
typedef char namestr[NAME_LEN+1];	/* Also max arg size; if changed, */
					/* look at process_macro. */

typedef struct macro {
  struct macro *next, *prev;		/* Linked list of macros */
  namestr the_name;
  char *the_body;			/* Dynamically allocated */
} macro;

extern char *malloc();

macro *find_macro();

static macro *header;

init_macros()
{
  header = NULL;
}

add_macro(name, body)
  register char *name;
  char *body;
{
  register macro *new;

  if (strlen(name) > NAME_LEN)		/* Silently truncate long names */
    name[NAME_LEN] = '\0';

  if (find_macro(name) != NULL) {
    fprintf(stderr, "%% Macro %s is already defined.\n", name);
    return;
  }

  new = (macro *) malloc(sizeof(struct macro));	/* Allocate space */
  new->prev = NULL;			/* Add at front of list */
  new->next = header;
  if (new->next != NULL)		/* If not at tail of list, */
    new->next->prev = new;		/* Set up backlink. */
  header = new;

  strcpy(new->the_name, name);
  new->the_body = malloc(strlen(body) + 1);
  strcpy(new->the_body, body);
}

remove_macro(name)			/* Undefine a macro */
  register char *name;
{
  register macro *where;

  if (strlen(name) > NAME_LEN)		/* Silently truncate long names */
    name[NAME_LEN] = '\0';

  where = find_macro(name);
  if (where == NULL) {
    fprintf(stderr, "%% Macro %s was not defined.\n", name);
    return;
  }

  if (where->prev == NULL)		/* Unlink from head */
    header = where->next;
  else
    where->prev->next = where->next;

  if (where->next != NULL)		/* If not at end, fix back link */
    where->next->prev = where->prev;

  free(where->the_body);		/* Free allocated storage. */
  free(where);
}

write_macros(name)			/* Write macros to a file. */
  char *name;
{
  register FILE *macro_file;
  register macro *where;

  expand_filename(name);
  macro_file = fopen(name, "w");	/* Open file */
  if (macro_file == NULL) {
    fprintf(stderr,"%% Could not write macros to %s .\n", name);
    return;
  }

  where = header;
  while (where != NULL) {		/* Scan linked list, outputting */
    fputs("/def ", macro_file);		/* the macros. */
    fputs(where->the_name, macro_file);
    fputs(" = ", macro_file);
    fputs(where->the_body, macro_file);
    fputc('\n', macro_file);
    where = where->next;
  }

  fclose(macro_file);
}

process_macro(name, args, deststr)	/* Process a macro invocation. */
  char *name, *args, *deststr;		/* Warning: no length checking! */
{
  register macro *where;
  register char *source, *dest, ch;
  namestr arg_ary[MAX_ARGS];
  int i;

  if (strlen(name) > NAME_LEN)		/* Silently truncate long names */
    name[NAME_LEN] = '\0';

  where = find_macro(name);
  if (where == NULL) {
    fprintf(stderr,"%% Macro %s is not defined.\n", name);
    deststr[0] = '\0';
    return;
  }

  for (i=0; i<MAX_ARGS; i++)		/* Zero all arguments. */
    arg_ary[i][0] = '\0';

  sscanf(args, "%31s %31s %31s %31s", arg_ary[0], arg_ary[1], arg_ary[2],
	 arg_ary[3]);

  source = where->the_body;		/* Start copying macro text. */
  dest = deststr;

  while (*source != '\0') {
    if (*source != '%')			/* If not special char, just copy. */
      *dest++ = *source++;
    else {				/* Look at next char. */
      source++;
      ch = *source;

      if ((ch == '\\') || (ch == ';')) { /* Backslash or ";" ==> newline. */
	*dest++ = '\n';
	source++;
      }
      else if (ch == '%') {		/* Double % ==> single %. */
	*dest++ = '%';
	source++;
      }
      else if (ch == '*') {		/* Entire command line. */
	strcpy(dest, args);
	dest += strlen(args);
	source++;
      }
      else if ((ch >= '1') && (ch <= '4')) { /* Argument insertion. */
	strcpy(dest, arg_ary[ch - '1']);
	dest += strlen(arg_ary[ch - '1']);
	source++;
      }
      else {				/* Don't know what it was.  Guess %. */
	*dest++ = '%';			/* Don't advance source here. */
      }
    }
  }

  *dest++ = '\n';			/* Add a newline. */
  *dest = '\0';				/* Terminate the expansion. */
}

macro *find_macro(name)
  register char *name;
{
  register macro *where;

  where = header;
  while ((where != NULL) && (!equalstr(where->the_name, name)))
    where = where->next;

  return (where);
}

list_all_macros(full)
  int full;
{
  register macro *where;

  where = header;
  while (where != NULL) {
    if (full)
      printf("%% %s = %s\n", where->the_name, where->the_body);
    else
      printf("%% %s\n", where->the_name);
    where = where->next;
  }
}
