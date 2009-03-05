/*
** symbols.c for  in /home/nico/lang/c/ftrace/src/api/bin
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Wed Mar 22 23:38:20 2006 nicolas
** Last update Thu Apr 27 13:09:49 2006 nicolas rimbeau
*/

#include <stdlib.h>
#include <string.h>

int	printf(char* format, ...);

#include "coffobj.h"

#include "api/includes/prototypes/bin/low_layer.h"
#include "api/includes/prototypes/bin/high_layer/symbols.h"
#include "api/includes/prototypes/prim_types.h"

/*
** Get section from name.
*/

#if 0
static struct scnhdr*	get_coffsection(bin_obj_t* obj, char* name)
{
  unsigned int		i;
  struct scnhdr*	ret = NULL;
  coffobj_t*		coffobj = NULL;

  coffobj = (coffobj_t*) obj->luse;
  for (i = 0; i < *((unsigned int *)coffobj->header->f_nscns); i++)
    if (!strcmp(coffobj->section_headers[i]->s_name, name))
      ret = coffobj->section_headers[i];
  return (ret);
}
#endif

syms_t*			get_coff_localsymbols(coffobj_t* luse, struct hdr_ext* sym, syms_t* list)
{
  unsigned int		i;
  char*			ptr;
  char*			ss;
  sym_t*		symbol;


  struct base_sym*	local_struct;
  char**		local_strs;

  local_strs = malloc(sizeof(char *) * (*((unsigned int *)sym->h_isymMax) + 1));
  memset(local_strs, 0, (*((unsigned int *)sym->h_isymMax) + 1) * sizeof(char *));
  ss = (char *)(((void *)luse->header) + (*((unsigned long *)sym->h_cbSsOffset)));
  ptr = ss;

  for (i = 0; i < *((unsigned int *)sym->h_isymMax); i++, ptr++)
    {
      local_strs[i] = ptr;
      while (*ptr)
	ptr++;
    }

  local_struct = (struct base_sym *)(((void *)luse->header) + *((unsigned long *)sym->h_cbSymOffset));
  for (i = 0; i < *((unsigned int *)sym->h_isymMax); i++, local_struct++)
    {
      if (local_struct->index != 0xFFFFF	&&	/* indexNil */
	  local_struct->iss   != 0xFFFFFFFF	&&	/* issNil */
	  local_struct->st == 6			&&	/* scNil */
	  local_struct->sc != 0)			/* stNil */
	{
	  symbol = malloc(sizeof(sym_t));
	  symbol->addr = local_struct->value;
	  symbol->name = ss + local_struct->iss;
	  while (*(symbol->name - 1))
	    symbol->name--;
	  if (strchr(symbol->name, ':') != NULL)
	    {
	      free(symbol);
	      continue;
	    }
	  symbol->size = local_struct->value;
	  symbol->size = 100;
	  printf("%s (0x%lx)\n", symbol->name, symbol->addr);
	  list = list_add(list, symbol);
	}
    }
  free(local_strs);
  return (list);
}

syms_t*			get_coff_externalsymbols(coffobj_t* luse, struct hdr_ext* sym, syms_t* list)
{
  unsigned int		i;
  char*			ptr;
  sym_t*		symbol;

  struct ext_sym*	ext_struct;
  char**		ext_strs;

  ext_strs = malloc(sizeof(char *) * (*((unsigned int *)sym->h_iextMax) + 1));
  memset(ext_strs, 0, (*((unsigned int *)sym->h_iextMax) + 1) * sizeof(char *));
  ptr = (char *)(((void *)luse->header) + (*((unsigned long *)sym->h_cbSsExtOffset)));
  for (i = 0; i < *((unsigned int *)sym->h_iextMax); i++, ptr++)
    {
      ext_strs[i] = ptr;
      while (*ptr)
	ptr++;
    }

  ext_struct = (struct ext_sym *)(((void *)luse->header) + *((unsigned long *)sym->h_cbExtOffset));
  for (i = 0; i < *((unsigned int *)sym->h_iextMax); i++)
    {
      if (ext_struct->es_asym.index != 0xFFFFF   &&	/* indexNil */
	  ext_struct->es_asym.iss   != (uint) -1 &&	/* issNil */
	  ext_struct->es_asym.sc    != 0         &&	/* scNil */
	  ext_struct->es_asym.st    != 0)		/* stNil */
	{
	  symbol = malloc(sizeof(sym_t));
	  symbol->name = ext_strs[ext_struct->es_asym.index];
	  symbol->size = 0;
	  symbol->addr = ext_struct->es_asym.value;
	  printf("[%d] %s (0x%lx)\n", i, symbol->name, symbol->addr);
	  list = list_add(list, symbol);
	}
      ext_struct++;
    }
  free(ext_strs);
  return (list);
}

syms_t*			syms_new(bin_obj_t* obj)
{
  /* Liste de symbols */
  coffobj_t*		luse;
  syms_t*		list = NULL;

  /* Symbolic header */
  struct hdr_ext*	sym;

  luse = (coffobj_t*) obj->luse;

  /* Si la variable f_symptr est egale a 0, l'objet ne contient pas de symbol */
  if (*((unsigned int *)luse->header->f_symptr) == 0)
    return (NULL);

  /* Structure indiquant ou trouver les symbols */
  sym = (struct hdr_ext *)(((void *)luse->header) + *((unsigned long *)luse->header->f_symptr));
  if (*((short *)sym->h_magic) != MAGIC_COFF_SYM_HDR)
    return (NULL);

  list = get_coff_localsymbols(luse, sym, list);
  list = get_coff_externalsymbols(luse, sym, list);

  return (list);
}

int	syms_del(syms_t* syms)
{
  list_t*	list = NULL;
  sym_t*	sym;

  list = (list_t*) syms;
  for (; list; )
    {
      sym = (sym_t*) list->value;
      list = list_del(list, sym);
      free(sym);
    }

  return (-1);
}

sym_t*	sym_get_frm_addr(syms_t* syms, addr_t addr, int exact)
{
  list_t*	list;
  sym_t*	sym, * ret = NULL;

  /*
  ** TODO: add exact.
  */
  exact = 0;
  /*
  ** Try to find a symbol whose virtual range includes the target address
  ** else take a symbol with the highest address less than or equal to the
  ** target
  */
  for (list = (list_t*) syms; list; list = list->next)
    {
      sym = (sym_t*) list->value;

      if (sym->addr <= addr)
	{
	  if (sym->size)
	    {
	      if (sym->size + sym->addr > addr)
		{
		  ret = sym;
		  break;
		}
	    }
	  else
	    {
	      if (ret == NULL || ret->addr < sym->addr)
		ret = sym;
	    }
	}
      if (list == ((list_t*) syms)->prev)
	break;
    }
  if (ret)
    return (ret);
  return (NULL);
}

sym_t*	sym_get_frm_label(syms_t* syms, char* label)
{
  list_t*	list;
  sym_t*	sym;

  for (list = (list_t*) syms; list; list = list->next)
    {
      sym = (sym_t*) list->value;

      if (!strcmp(sym->name, label))
	return (sym);

      if (list == ((list_t*) syms)->prev)
	break;
    }
  return (NULL);
}
