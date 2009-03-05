#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "api/includes/prototypes/bin/high_layer/debug.h"
#include "api/includes/prototypes/prim_types.h"

#define DEBUGTYPE_DWARF2	0
#define DEBUGTYPE_STABS		1

static char	debugtype = -1;

debugs_t	*Dwarf2_debug_new_from_bin(bin_obj_t *);
debugs_t	*Stabs_debug_new_from_bin(bin_obj_t *);
int		Dwarf2_debug_del(debugs_t *);
int		Stabs_debug_del(debugs_t *);
debug_t		*Dwarf2_debug_get_frm_addr(debugs_t *, addr_t);
debug_t		*Stabs_debug_get_frm_addr(debugs_t *, addr_t);
debug_t		*Dwarf2_debug_get_frm_label(debugs_t *, char *);
debug_t		*Stabs_debug_get_frm_label(debugs_t *, char *);

debugs_t	*debug_new_from_bin(bin_obj_t *elf)
{
  debugs_t	*list;

  list = Dwarf2_debug_new_from_bin(elf);
  if (list == NULL)
    {
      list = Stabs_debug_new_from_bin(elf);
      debugtype = DEBUGTYPE_STABS;
    }
  else
    debugtype = DEBUGTYPE_DWARF2;

  return (list);
}

int	debug_del(debugs_t *list)
{
  if (debugtype == DEBUGTYPE_DWARF2)
    return (Dwarf2_debug_del(list));
  return (Stabs_debug_del(list));
}

debug_t		*debug_get_frm_addr(debugs_t *list, addr_t addr)
{
  if (debugtype == DEBUGTYPE_DWARF2)
    return (Dwarf2_debug_get_frm_addr(list, addr));
  return (Stabs_debug_get_frm_addr(list, addr));
}

debug_t		*debug_get_frm_label(debugs_t *list, char *label)
{
  if (debugtype == DEBUGTYPE_DWARF2)
    return (Dwarf2_debug_get_frm_label(list, label));
  return (Stabs_debug_get_frm_label(list, label));
}
