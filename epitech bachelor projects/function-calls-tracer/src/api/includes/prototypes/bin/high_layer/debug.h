/*
** debug.h for  in /home/nico/lang/c/ftrace/src/api/includes/prototypes/bin/high_layer
** 
** Made by nicolas
** Mail   <n.cormier@gmail.com>
** 
** Started on  Wed Mar 22 17:25:02 2006 nicolas
** Last update Sun Apr  9 09:54:54 2006 nicolas
*/

#ifndef __BIN_HIGH_LAYER_DEBUG_H__
# define __BIN_HIGH_LAYER_DEBUG_H__

# include "api/includes/types/bin.h"
# include "api/includes/types/core.h"
# include "api/includes/types/prim_types.h"


debugs_t*	debug_new_from_bin(bin_obj_t*);
int		debug_del(debugs_t*);

debug_t*		debug_get_frm_addr(debugs_t*, addr_t);
debug_t*		debug_get_frm_label(debugs_t*, char*);


#endif /* __BIN_HIGH_LAYER_DEBUG_H__ */
