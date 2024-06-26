/************************************************************************/
/* TinyTalk global types and variables.					*/
/*									*/
/*	Version 1.0 [ 1/24/90] : Initial implementation by ABR.		*/
/*              1.1 [ 1/24/90] : Longer strings allowed.		*/
/*		1.2 [ 2/16/90] : Integrated support for System V and    */
/*				 HP-UX, from Andy Norman and Kipp       */
/*				 Hickman.				*/
/*									*/
/************************************************************************/

#define TRUE 1
#define FALSE 0

#define MAXSTRLEN 511
#define HUGESTRLEN (2*MAXSTRLEN)
#define SMALLSTR 31

typedef char string[MAXSTRLEN+1];
typedef char hugestr[HUGESTRLEN+1];
typedef char smallstr[SMALLSTR+1];

  /* A TinyMUD world/character record. */

typedef struct world_rec {
  struct world_rec *next;
  smallstr world, character, pass, address, port;
} world_rec;

#ifdef HPUX

#define rindex strrchr
#define index strchr
#define bcopy(a,b,c) memcpy((b),(a),(c))

#endif /* HPUX */
