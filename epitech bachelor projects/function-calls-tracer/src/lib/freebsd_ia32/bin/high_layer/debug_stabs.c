/*
** debug.c for  in /home/nico/lang/c/ftrace/src/api/bin/high_layer
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Wed Mar 22 23:35:49 2006 nicolas
** Last update Sun Apr 23 17:23:52 2006 dak
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "api/includes/prototypes/bin/high_layer/debug.h"
#include "api/includes/prototypes/prim_types.h"

#include "includes/error.h"

#include "stabs.h"

static t_stackelem	*stack = NULL;
static t_file		*files = NULL;
static int		filescount = 0;
static t_file		*currentfile = NULL;
static t_header		*headers = NULL;
static int		nheaders = 0;

static char		*GetStringTable(Elf32_Ehdr *);
static Elf32_Shdr	*GetELFSection(char *, Elf32_Ehdr *);
static void		Stabs_ParserInit(void);
static void		Stabs_ParserUninit(void);
static t_type		*Stabs_InsertTag(char *, char *);
static t_type		*Stabs_FindTag(char *);
static int		Stabs_GuessBaseTypeSize(char *);
static t_parse		*Stabs_GetNameAndTag(char *);
static void		Stabs_FreeNameAndTag(t_parse *);
static void		Stabs_PushInStack(char *);
static char		*Stabs_PopFromStack(void);
static void		Stabs_ParseChainTypes(char *, t_type *);
static void		Stabs_ParseLsym(char *);
static void		Stabs_ParseTypedef(t_parse *);
static void		Stabs_ParseVariable(t_parse *p);
static void		Stabs_ParseStructureName(char *);
static void		Stabs_ParseStructMembers(char *);
static void		Stabs_ParseStructure(char *, t_type *);
static void		Stabs_ParseArray(char *);
static void		Stabs_ParseFun(char *, unsigned int);
static void		Stabs_ParsePsym(char *);
static void		Stabs_ParseSo(char *);
static void		Stabs_ParseSline(int, unsigned int);
static void		Stabs_ParseGsym(char *);
static void		Stabs_ParseStsym(char *);
static void		Stabs_ParseLcsym(char *);
static void		Stabs_ParseSsym(char *);
static void		Stabs_ParseBincl(char *, int, int);
static void		Stabs_ParseExcl(char *, int, int);
static int		Stabs_GetFileNum(char *);
static int		Stabs_GetTypeNum(char *);
static void		Stabs_FillType(char *, int *);
static char		*Stabs_ResolveTag(int, int, int);
static t_type		*_Stabs_ResolveTag(int, int, int, int *);
static debugs_t		*Stabs_BuildDebugsList(void);
static int		Stabs_ResolveSize(int, int, int);
static t_type		*_Stabs_ResolveSize(int, int, int, int *);

/*
 *********************************************************
 * ELF parsing
 *********************************************************
 */

static char	*GetStringTable(Elf32_Ehdr *elf)
{
  Elf32_Shdr	*s;

  s = (Elf32_Shdr *) ((char *) elf + elf->e_shoff +
                     elf->e_shstrndx * elf->e_shentsize);
  return ((char *) elf + s->sh_offset);
}

static Elf32_Shdr	*GetELFSection(char *name, Elf32_Ehdr *elf)
{
  Elf32_Shdr		*s;
  char			*str;
  int			i;

  str = GetStringTable(elf);
  for (i = 0; i < elf->e_shnum; i++)
    {
      s = (Elf32_Shdr *) ((char *) elf + elf->e_shoff +
			  (i * elf->e_shentsize));
      if (!strcmp(str + s->sh_name, name))
	return (s);
    }
  return (NULL);
}

/*
 *********************************************************
 * STABS {Un}Initialization
 *********************************************************
 */

static void	Stabs_ParserInit(void)
{
  stack = NULL;
  files = NULL;
  filescount = 0;
  currentfile = NULL;
  headers = NULL;
  nheaders = 0;
}

static void	Stabs_ParserUninit(void)
{
  int		i;
  int		j;
  int		k;
  t_type	*t;
  t_type	*next;

  for (i = 0; i < filescount; i++)
    {
      free(files[i].f_name);
      free(files[i].f_lines);
      for (j = 0; j < files[i].f_nfunctions; j++)
	{
	  free(files[i].f_functions[j].f_name);
	  for (k = 0; k < files[i].f_functions[j].f_nargs; k++)
	    free(files[i].f_functions[j].f_argstype[k].a_name);
	  free(files[i].f_functions[j].f_argstype);
	}
      free(files[i].f_functions);
      for (t = files[i].f_types; t != NULL; )
	{
	  free(t->t_name);
	  next = t->t_next;
	  free(t);
	  t = next;
	}
      free(files[i].f_aliases);
    }
  free(files);
  for (i = 0; i < nheaders; i++)
    {
      free(headers[i].h_path);
      free(headers[i].h_from);
    }
  free(headers);
}

/*
 *********************************************************
 * API
 *********************************************************
 */

debugs_t	*Stabs_debug_new_from_bin(bin_obj_t *elf)
{
  Elf32_Shdr	*s;
  Elf32_Ehdr	*e;
  t_StabH	*st;
  int		i;
  char		*v;
  unsigned int	curfuncaddr;
  char		*headers;
  char		*symtable;
  int		nentries;
  int		depth;
  debugs_t	*list;

  Stabs_ParserInit();
  s = GetELFSection(".stab", elf->map);
  if (s == NULL)
    return (NULL);
  headers = (char *) ((char *) elf->map + s->sh_offset);
  nentries = s->sh_size / sizeof(t_StabH);
  e = (Elf32_Ehdr *) elf->map;
  s = (Elf32_Shdr *) ((char *) elf->map + e->e_shoff +
		      s->sh_link * e->e_shentsize);
  symtable = ((char *) elf->map + s->sh_offset);
  for (i = 0, curfuncaddr = 0, depth = 0; i < nentries; i++)
    {
      st = (t_StabH *) (headers + (i * sizeof(*st)));
      v = symtable + st->n_strx;
      switch (st->n_type)
	{
	case (N_BINCL):
	  depth++;
	  Stabs_ParseBincl(v, depth, st->n_value);
	  break;

	case (N_LSYM):
	  Stabs_ParseLsym(v);
	  break;

	case (N_GSYM):
	  Stabs_ParseGsym(v);
	  break;

	case (N_PSYM):
	  Stabs_ParsePsym(v);
	  break;

	case (N_STSYM):
	  Stabs_ParseStsym(v);
	  break;

	case (N_LCSYM):
	  Stabs_ParseLcsym(v);
	  break;

	case (N_SSYM):
	  Stabs_ParseSsym(v);
	  break;

	case (N_FUN):
	  if (*v == 0)
	    break;
	  Stabs_ParseFun(v, st->n_value);
	  curfuncaddr = st->n_value;
	  break;

	case (N_SO):
	  depth = 0;
	  if ((*v == 0) || (v[strlen(v) - 1] == '/'))
	    break;
	  Stabs_ParseSo(v);
	  break;

	case (N_SLINE):
	  Stabs_ParseSline(st->n_desc, curfuncaddr + st->n_value);
	  break;

	case (N_EXCL):
	  depth++;
	  Stabs_ParseExcl(v, depth, st->n_value);
	  break;
	}
    }
   list = Stabs_BuildDebugsList();
   Stabs_ParserUninit();
   return (list);
}

int		Stabs_debug_del(debugs_t *list)
{
  debug_t	*d;
  debugs_t	*args;
  arg_t		*arg;

  for (; list != NULL; )
    {
      d = list->value;
      free(d->name);
      free(d->file);
      free(d->ret->name);
      free(d->ret->type);
      free(d->ret);
      for (args = d->args; args != NULL; )
	{
	  arg = args->value;
	  free(arg->name);
	  free(arg->type);
	  args = list_del(args, args->value);
	  free(arg);
	}
      list = list_del(list, list->value);
      free(d);
    }
  return (0);
}

debug_t		*Stabs_debug_get_frm_addr(debugs_t *list, addr_t addr)
{
  debugs_t	*d;
  debugs_t	*first;
  debug_t	*elem;

  for (d = list, first = list; d != NULL; d = d->next)
    {
      elem = d->value;
      if (elem->addr == addr)
	return (elem);
      if (first->prev == d)
	break;
    }
  return (NULL);
}

debug_t		*Stabs_debug_get_frm_label(debugs_t *list, char *label)
{
  debugs_t	*d;
  debugs_t	*first;
  debug_t	*elem;

  for (d = list, first = list; d != NULL; d = d->next)
    {
      elem = d->value;
      if (strcmp(elem->name, label) == 0)
	return (elem);
      if (first->prev == d)
	break;
    }
  return (NULL);
}

/*
 *********************************************************
 * Build debugs_t list
 *********************************************************
 */

static debugs_t	*Stabs_BuildDebugsList(void)
{
  debugs_t	*list;
  debug_t	*elem;
  int		i;
  int		j;
  int		k;
  arg_t		*arg;

  for (i = 0, list = NULL; i < filescount; i++)
    {
      for (j = 0; j < files[i].f_nfunctions; j++)
	{
	  elem = malloc(sizeof(*elem));
	  elem->file = strdup(files[i].f_name);

	  elem->name = strdup(files[i].f_functions[j].f_name);
	  elem->addr = files[i].f_functions[j].f_addr;
	  for (k = 0, elem->line = 0; k < files[i].f_nlines - 1; k++)
	    if ((files[i].f_lines[k].l_addr >= elem->addr) &&
		(elem->addr <= files[i].f_lines[k + 1].l_addr))
	      {
		elem->line = files[i].f_lines[k].l_line;
		break;
	      }

	  arg = malloc(sizeof(*arg));
	  memset(arg, 0, sizeof(*arg));

	  arg->name = strdup(files[i].f_functions[j].f_name);;
	  arg->type = Stabs_ResolveTag(files[i].f_functions[j].f_rettype[0],
				       files[i].f_functions[j].f_rettype[1],
				       i);

	  arg->size = Stabs_ResolveSize(files[i].f_functions[j].f_rettype[0],
					files[i].f_functions[j].f_rettype[1],
					i);

	  elem->ret = arg;
	  elem->args = NULL;

	  for (k = 0; k < files[i].f_functions[j].f_nargs; k++)
	    {
	      arg = malloc(sizeof(*arg));
	      memset(arg, 0, sizeof(*arg));

	      arg->type = Stabs_ResolveTag(files[i].f_functions[j].f_argstype[k].a_type[0],
					   files[i].f_functions[j].f_argstype[k].a_type[1],
					   i);
	      arg->size = Stabs_ResolveSize(files[i].f_functions[j].f_argstype[k].a_type[0],
					    files[i].f_functions[j].f_argstype[k].a_type[1],
					    i);
	      arg->name = strdup(files[i].f_functions[j].f_argstype[k].a_name);

	      arg->order = k;

	      elem->args = list_add(elem->args, arg);
	    }
	  list = list_add(list, elem);
	}
    }
  return (list);
}

/*
 *********************************************************
 * Types list management
 *********************************************************
 */

static int		Stabs_GetFileNum(char *tag)
{
  t_parsecontext	context;
  int			fnum;

  CONTEXT_NEW(&context, tag);

  *strchr(CONTEXT_BUF(&context), ',') = 0;
  CONTEXT_BUF(&context)++;
  fnum = atoi(CONTEXT_BUF(&context));

  CONTEXT_FREE(&context);
  return (fnum);
}

static int		Stabs_GetTypeNum(char *tag)
{
  t_parsecontext	context;
  int			tnum;

  CONTEXT_NEW(&context, tag);

  *strchr(CONTEXT_BUF(&context), ')') = 0;
  CONTEXT_BUF(&context) = strchr(CONTEXT_BUF(&context), ',') + 1;
  tnum = atoi(CONTEXT_BUF(&context));

  CONTEXT_FREE(&context);
  return (tnum);
}

static void	Stabs_FillType(char *tag, int *types)
{
  types[0] = Stabs_GetFileNum(tag);
  types[1] = Stabs_GetTypeNum(tag);
}

static t_type	*Stabs_InsertTag(char *name, char *tag)
{
  t_type	*t;

  t = malloc(sizeof(*t));
  if (t == NULL)
    errexit("Not enough memory\n");
  t->t_fnum = Stabs_GetFileNum(tag);
  t->t_tnum = Stabs_GetTypeNum(tag);
  if (name != NULL)
    t->t_name = strdup(name);
  else
    t->t_name = NULL;
  t->t_size = Stabs_GuessBaseTypeSize(name);
  t->t_parenttype = NULL;
  t->t_next = currentfile->f_types;
  t->t_ptr = 0;
  t->t_flags = TYPEFLAGS_UNDEF;
  currentfile->f_types = t;
  return (t);
}

static t_type	*Stabs_FindTag(char *tag)
{
  t_type	*t;
  int		fnum;
  int		tnum;

  fnum = Stabs_GetFileNum(tag);
  tnum = Stabs_GetTypeNum(tag);
  for (t = currentfile->f_types; t != NULL; t = t->t_next)
    if ((t->t_fnum == fnum) && (t->t_tnum == tnum))
      return (t);
  return (NULL);
}

static int		Stabs_GuessBaseTypeSize(char *name)
{
  static t_basetype	btypes[] =
    {
      { "int", sizeof(int) },
      { "char", sizeof(char) },
      { "long int", sizeof(long int) },
      { "unsigned int", sizeof(unsigned int) },
      { "long unsigned int", sizeof(long unsigned int) },
      { "long long int", sizeof(long long int) },
      { "long long unsigned int", sizeof(long long unsigned int) },
      { "short int", sizeof(short int) },
      { "short unsigned int", sizeof(short unsigned int) },
      { "signed char", sizeof(signed char) },
      { "unsigned char", sizeof(unsigned char) },
      { "float", sizeof(float) },
      { "double", sizeof(double) },
      { "long double", sizeof(long double) },
      { "void", sizeof(void) },
      { NULL, -1 }
    };
  int			i;

  for (i = 0; (name != NULL) && (btypes[i].b_name != NULL); i++)
    if (strcmp(btypes[i].b_name, name) == 0)
      return (btypes[i].b_size);
  return (0);
}

/*
 *********************************************************
 * NAME/TAG management
 *********************************************************
 */

static t_parse		*Stabs_GetNameAndTag(char *buf)
{
  t_parse		*p;
  t_parsecontext	context;
  char			*ptr;

  p = malloc(sizeof(*p));
  if (p == NULL)
    errexit("Not enough memory\n");
  CONTEXT_NEW(&context, buf);
  p->p_descriptor = *(strchr(CONTEXT_BUF(&context), ':') + 1);
  p->p_name = p->p_origname = strdup(strtok(CONTEXT_BUF(&context), ":"));
  p->p_tag = p->p_origtag = strdup(strtok(NULL, "="));
  ptr = strtok(NULL, "");
  if (ptr != NULL)
    p->p_next = p->p_orignext = strdup(ptr);
  else
    p->p_next = p->p_orignext = NULL;
  CONTEXT_FREE(&context);
  return (p);
}

static void	Stabs_FreeNameAndTag(t_parse *p)
{
  free(p->p_origname);
  free(p->p_origtag);
  free(p->p_orignext);
  free(p);
}

/*
 *********************************************************
 * Stack Management
 *********************************************************
 */

static void	Stabs_PushInStack(char *buf)
{
  t_stackelem	*s;

  s = malloc(sizeof(*s));
  if (s == NULL)
    errexit("Not enough memory\n");
  s->s_buf = buf;
  s->s_next = stack;
  stack = s;
}

static char	*Stabs_PopFromStack(void)
{
  t_stackelem	*s;
  char		*buf;

  if (stack == NULL)
    return (NULL);
  s = stack;
  stack = s->s_next;
  buf = s->s_buf;
  free(s);
  return (buf);
}

/*
 *********************************************************
 * Chain types parsing
 *********************************************************
 */

static void	Stabs_ParseChainTypes(char *buf, t_type *prev)
{
  int		i;
  int		end;
  int		ptr;
  t_type	*t;
  char		*tmp;

  while ((buf != NULL) && (*buf != 0))
    {
      ptr = 0;
      switch (*buf)
	{
	case ('*'):
	  ptr = 1;
	case ('('):
	  /* On cherche le '=' suivant ou la fin de la chaine */
	  for (i = ptr, end = 0; (buf[i] != '=') && (buf[i] != 0); i++)
	    ;
	  if (buf[i] == 0)
	    end = 1;
	  buf[i] = 0;
	  i -= ptr;
	  /* On recherche si ya deja un tag existant */
	  t = Stabs_FindTag(buf + ptr);
	  /* On evite de pointer sur soi-meme (cas special du void) */
	  if ((t != prev) || (prev == NULL))
	    {
	      /* Si yen a pas, on le cree en anonyme */
	      if (t == NULL)
		t = Stabs_InsertTag(NULL, buf + ptr);
	      /* Le parent du precedent c'est le courant */
	      if (prev != NULL)
		{
		  prev->t_parenttype = t;
		  prev->t_ptr = ptr;
		}
	      prev = t;
	    }
	  /* On update la chaine */
	  if (end == 1)
	    return ;
	  buf += (++i + ptr);
	  break;

	case ('r'):
	  return ;
	  
	case ('a'):
	  buf++;
	  break;

	case ('x'):
	  /* Cross reference */
	  buf++;
	  switch (*buf)
	    {
	    case ('s'):
	      prev->t_flags = TYPEFLAGS_STRUCT;
	    case ('e'):
	      buf++;
	      tmp = strchr(buf, ':');
	      if (tmp != NULL)
		*tmp = 0;
	      prev->t_name = strdup(buf);
	      /* size d'un ptr */
	      prev->t_size = sizeof(void *);
	      for (; (*buf != 0) && (*buf != ':'); buf++)
		;
	      if (*buf == ':')
		buf++;
	      break;
	    }
	  break;

	case ('u'):
	case ('s'):
	case ('e'):
	  /* Structure/enum/union */
	  Stabs_ParseStructure(buf, prev);
	  prev->t_flags = TYPEFLAGS_STRUCT;
	  return ;

	default:
	  buf++;
	}
    }  
}


/*
 *********************************************************
 * Parse LSYM lines
 *********************************************************
 */

static void	Stabs_ParseLsym(char *buf)
{
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('T'):
    case ('t'):
      Stabs_ParseTypedef(p);
      break;

    case ('('):
      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse typedef
 *********************************************************
 */

static void	Stabs_ParseTypedef(t_parse *p)
{
  t_type	*t;

  t = Stabs_FindTag(p->p_tag + 1);
  if (t == NULL)
    t = Stabs_InsertTag(p->p_name, p->p_tag + 1);

  if (p->p_next == NULL)
    return ;

  Stabs_ParseChainTypes(p->p_next, t);
}

/*
 *********************************************************
 * Parse variable
 *********************************************************
 */

static void	Stabs_ParseVariable(t_parse *p)
{
  t_type	*t;

  t = Stabs_FindTag(p->p_tag);
  if (t == NULL)
    t = Stabs_InsertTag(NULL, p->p_tag);

  if (p->p_next == NULL)
    return ;

  Stabs_ParseChainTypes(p->p_next, t);
}

/*
 *********************************************************
 * Parse structure name
 *********************************************************
 */

static void	Stabs_ParseStructureName(char *buf)
{
  t_type	*t;
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  t = Stabs_FindTag(p->p_tag);
  if (t == NULL)
    t = Stabs_InsertTag(p->p_name, p->p_tag);
  t->t_flags = TYPEFLAGS_STRUCT;

  if (p->p_next == NULL)
    {
      Stabs_FreeNameAndTag(p);
      return ;
    }

  Stabs_ParseChainTypes(p->p_next, t);
  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse structure members
 *********************************************************
 */

static void	Stabs_ParseStructMembers(char *buf)
{
  char		*ptr;
  int		i;
  int		count;
  t_parse	*p;
  char		*context;

  while ((ptr = strtok_r(buf, ";", &context)) != NULL)
    {
      buf = NULL;
      for (i = strlen(ptr) - 1, count = 0; (i >= 0) && (count != 2); i--)
	if (ptr[i] == ',')
	  count++;
      if ((count != 2) || (i == -1))
	continue;
      ptr[i + 1] = 0;
      if (ptr[0] == ':')
	continue;
      switch (ptr[1])
	{
	case ('('):
	  p = Stabs_GetNameAndTag(ptr);
	  Stabs_ParseVariable(p);
	  Stabs_FreeNameAndTag(p);
	  *strchr(ptr, ':') = 0;
	  Stabs_ParseChainTypes(ptr, NULL);
	  break;

	default:
	  p = Stabs_GetNameAndTag(ptr);
	  Stabs_ParseVariable(p);
	  Stabs_FreeNameAndTag(p);
	  break;
	}
    }
}

/*
 *********************************************************
 * Parse structure/enum/union
 *********************************************************
 */

static void	Stabs_ParseStructure(char *buf, t_type *prev)
{
  char		*tmp;
  int		count;
  int		i;
  int		array;

  for (buf++; (*buf >= '0') && (*buf <= '9'); buf++)
    ;
  tmp = NULL;
  count = 0;
  /* autiste */
  prev->t_size = 13;
  while (*buf != 0)
    {
      array = 0;
      if (*buf == '=')
	switch (*(buf + 1))
	  {
	  case ('a'):
	    Stabs_ParseArray(buf);
	    break;

	  case ('u'):
	  case ('s'):
	    for (buf += 2; (*buf >= '0') && (*buf <= '9'); buf++)
	      ;
	    for (i = strlen(tmp) - 1; (i >= 0) && (tmp[i] != ';'); i--)
	      ;
	    Stabs_ParseStructureName(&tmp[i + 1]);
	    tmp[i + 1] = 0;
	    Stabs_PushInStack(tmp);
	    tmp = NULL;
	    count = 0;
	    continue;
	  }
      if (*buf == ';')
	if (*(buf + 1) == ';')
	  {
	    Stabs_ParseStructMembers(tmp);
	    free(tmp);
	    tmp = Stabs_PopFromStack();
	    if (tmp == NULL)
	      return ;
	    count = strlen(tmp);
	    buf += 2;
	    continue;
	  }
      tmp = realloc(tmp, count + 2);
      if (tmp == NULL)
	errexit("Not enough memory\n");
      tmp[count++] = *buf;
      tmp[count] = 0;
      buf++;
    }
  free(tmp);
}

/*
 *********************************************************
 * Parse array
 *********************************************************
 */

static void	Stabs_ParseArray(char *buf)
{
  int		i;
  int		j;
  int		k;

  /* code de gitan */
  for (i = 0; buf[i] != ';'; i++)
    ;
  for (j = i; buf[j] != '('; j++)
    ;
  for (k = j; buf[k] != ';'; k++)
    ;
  buf[i] = ':';
  memmove(buf + i + 1, buf + j, strlen(buf) - j + 1);
}

/*
 *********************************************************
 * Parse FUN lines
 *********************************************************
 */

static void	Stabs_ParseFun(char *buf, unsigned int addr)
{
  t_parse	*p;
  int		i;
  t_function	*functions;
  int		nfunctions;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('f'):
      for (i = strlen(p->p_tag) - 1; (i >= 0) && (p->p_tag[i] != ')'); i--)
	;
      p->p_tag[i + 1] = 0;
    case ('F'):
      p->p_tag++;

      functions = currentfile->f_functions;
      nfunctions = currentfile->f_nfunctions;
      functions = realloc(functions, (nfunctions + 1) * sizeof(*functions));
      if (functions == NULL)
	errexit("Not enough memory\n");
      functions[nfunctions].f_name = strdup(p->p_name);
      functions[nfunctions].f_addr = addr;
      functions[nfunctions].f_argstype = NULL;
      functions[nfunctions].f_nargs = 0;
      currentfile->f_functions = functions;
      currentfile->f_nfunctions++;

      Stabs_FillType(p->p_tag, functions[nfunctions].f_rettype);

      Stabs_ParseVariable(p);

      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse PSYM lines
 *********************************************************
 */

static void	Stabs_ParsePsym(char *buf)
{
  t_parse	*p;
  t_argument	*args;
  int		nargs;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('p'):
      p->p_tag++;

      args = currentfile->f_functions[currentfile->f_nfunctions - 1].f_argstype;
      nargs = currentfile->f_functions[currentfile->f_nfunctions - 1].f_nargs;
      args = realloc(args, (nargs + 1) * sizeof(*args));
      if (args == NULL)
	errexit("Not enough memory\n");
      args[nargs].a_name = strdup(p->p_name);
      Stabs_FillType(p->p_tag, args[nargs].a_type);
      currentfile->f_functions[currentfile->f_nfunctions - 1].f_argstype = args;
      currentfile->f_functions[currentfile->f_nfunctions - 1].f_nargs++;

      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse SO lines
 *********************************************************
 */

static void	Stabs_ParseSo(char *buf)
{
  files = realloc(files, (filescount + 1) * sizeof(*files));
  if (files == NULL)
    errexit("Not enough memory\n");
  files[filescount].f_name = strdup(buf);
  files[filescount].f_lines = NULL;
  files[filescount].f_nlines = 0;
  files[filescount].f_types = NULL;
  files[filescount].f_functions = NULL;
  files[filescount].f_nfunctions = 0;
  files[filescount].f_aliases = NULL;
  files[filescount].f_naliases = 0;
  currentfile = &files[filescount];
  filescount++;
}

/*
 *********************************************************
 * Parse SLINE lines
 *********************************************************
 */

static void	Stabs_ParseSline(int line, unsigned int addr)
{
  t_line	*lines;
  int		nlines;

  lines = currentfile->f_lines;
  nlines = currentfile->f_nlines;
  lines = realloc(lines, (nlines + 1) * sizeof(*lines));
  if (lines == NULL)
    errexit("Not enough memory\n");
  lines[nlines].l_line = line;
  lines[nlines].l_addr = addr;
  currentfile->f_lines = lines;
  currentfile->f_nlines++;
}

/*
 *********************************************************
 * Parse GSYM lines
 *********************************************************
 */

static void	Stabs_ParseGsym(char *buf)
{
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('G'):
      p->p_tag++;
      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse STSYM lines
 *********************************************************
 */

static void	Stabs_ParseStsym(char *buf)
{
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('S'):
      p->p_tag++;
      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse LCSYM lines
 *********************************************************
 */

static void	Stabs_ParseLcsym(char *buf)
{
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('S'):
      p->p_tag++;
      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse SSYM lines
 *********************************************************
 */

static void	Stabs_ParseSsym(char *buf)
{
  t_parse	*p;

  p = Stabs_GetNameAndTag(buf);
  switch (p->p_descriptor)
    {
    case ('T'):
    case ('t'):
      Stabs_ParseTypedef(p);
      break;

    case ('('):
      Stabs_ParseVariable(p);
      break;
    }

  Stabs_FreeNameAndTag(p);
}

/*
 *********************************************************
 * Parse BINCL lines
 *********************************************************
 */

static void	Stabs_ParseBincl(char *buf, int idx, int instance)
{
  headers = realloc(headers, (nheaders + 1) * sizeof(*headers));
  if (headers == NULL)
    errexit("Not enough memory\n");
  headers[nheaders].h_idx = idx;
  headers[nheaders].h_instance = instance;
  headers[nheaders].h_path = strdup(buf);
  headers[nheaders].h_from = strdup(currentfile->f_name);
  
  nheaders++;
}

/*
 *********************************************************
 * Parse EXCL lines
 *********************************************************
 */

static void	Stabs_ParseExcl(char *buf, int idx, int instance)
{
  int		i;
  int		j;
  int		naliases;
  t_alias	*aliases;

  for (i = 0; i < nheaders; i++)
    if ((strcmp(headers[i].h_path, buf) == 0) &&
	(headers[i].h_instance == instance))
      break;
  if (i == nheaders)
    return ;
  for (j = 0; j < filescount; j++)
    if (strcmp(files[j].f_name, headers[i].h_from) == 0)
      break;
  if (j == filescount)
    return ;

  aliases = currentfile->f_aliases;
  naliases = currentfile->f_naliases;
  aliases = realloc(aliases, (naliases + 1) * sizeof(*aliases));
  if (aliases == NULL)
    errexit("Not enough memory\n");
  aliases[naliases].a_fromidx = idx;
  aliases[naliases].a_toidx = headers[i].h_idx;
  aliases[naliases].a_fileidx = j;
  currentfile->f_aliases = aliases;
  currentfile->f_naliases++;
}

/*
 *********************************************************
 * Tag resolver
 *********************************************************
 */

static char	*Stabs_ResolveTag(int fnum, int tnum, int index)
{
  t_type	*t;
  int		i;
  int		ptr;
  int		pstruct;
  char		*type;

  for (t = NULL, ptr = 0, pstruct = 0; ; )
    {
      t = _Stabs_ResolveTag(fnum, tnum, index, &ptr);
      if ((t == NULL) || (t->t_name != NULL))
	break;
      if (files[index].f_naliases != 0)
	for (i = 0; i < files[index].f_naliases; i++)
	  if (files[index].f_aliases[i].a_fromidx == t->t_fnum)
	    {
	      fnum = files[index].f_aliases[i].a_toidx;
	      tnum = t->t_tnum;
	      index = files[index].f_aliases[i].a_fileidx;
	    }
    }

  if ((t == NULL) || (t->t_name == NULL))
    return ("unknown");
  if ((t->t_flags & TYPEFLAGS_STRUCT) == TYPEFLAGS_STRUCT)
    pstruct = 7;
  type = malloc(pstruct + strlen(t->t_name) + 1 + ptr + 1);
  if (type == NULL)
    errexit("Not enough memory\n");
  type[0] = 0;
  if (pstruct != 0)
    sprintf(type, "struct ");
  strcat(type, t->t_name);
  if (ptr != 0)
    strcat(type, " ");
  for (i = 0; i < ptr; i++)
    strcat(type, "*");
  return (type);
}

static t_type	*_Stabs_ResolveTag(int fnum, int tnum, int index, int *ptr)
{
  t_type	*t;
  t_type	*prev;

  for (t = files[index].f_types; t != NULL; t = t->t_next)
    if ((t->t_fnum == fnum) && (t->t_tnum == tnum))
      {
	for (prev = NULL; (t != NULL) && (t->t_name == NULL); prev = t, t = t->t_parenttype)
	  (*ptr) += t->t_ptr;
	if (t == NULL)
	  return (prev);
	return (t);
      }
  return (NULL);  
}

/*
 *********************************************************
 * Size resolver
 *********************************************************
 */

static int	Stabs_ResolveSize(int fnum, int tnum, int index)
{
  t_type	*t;
  int		i;
  int		ptr;
  int		pstruct;

  for (t = NULL, ptr = 0, pstruct = 0; ; )
    {
      t = _Stabs_ResolveSize(fnum, tnum, index, &ptr);
      if (t->t_parenttype == NULL)
	break;
      if (files[index].f_naliases != 0)
	for (i = 0; i < files[index].f_naliases; i++)
	  if (files[index].f_aliases[i].a_fromidx == t->t_fnum)
	    {
	      fnum = files[index].f_aliases[i].a_toidx;
	      tnum = t->t_tnum;
	      index = files[index].f_aliases[i].a_fileidx;
	    }
    }
  if (ptr != 0)
    return (sizeof(void *));
  return (t->t_size);
}

static t_type	*_Stabs_ResolveSize(int fnum, int tnum, int index, int *ptr)
{
  t_type	*t;
  t_type	*prev;

  for (t = files[index].f_types; t != NULL; t = t->t_next)
    if ((t->t_fnum == fnum) && (t->t_tnum == tnum))
      {
	for (prev = NULL; (t != NULL); prev = t, t = t->t_parenttype)
	  (*ptr) += t->t_ptr;
	if (t == NULL)
	  return (prev);
	return (t);
      }
  return (NULL);
}
