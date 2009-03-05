/*
** debug.c for  in /home/nico/lang/c/ftrace/src/api/bin/high_layer
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Wed Mar 22 23:35:49 2006 nicolas
** Last update Tue Apr 25 21:02:31 2006 nicolas cormier
*/

#include <string.h>
#include <stdlib.h>

#include "api/includes/prototypes/bin/high_layer/debug.h"
#include "api/includes/prototypes/prim_types.h"

#include "includes/error.h"

/*
 *********************************************************
 * API
 *********************************************************
 */

debugs_t	*debug_new_from_bin(bin_obj_t *coff)
{
  coff = NULL;
  return (NULL);
}

int		debug_del(debugs_t *list)
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

debug_t		*debug_get_frm_addr(debugs_t *list, addr_t addr)
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

debug_t		*debug_get_frm_label(debugs_t *list, char *label)
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
