/*
** bin.h for  in /home/nico/lang/c/ftrace/src/api/includes/prototypes/bin/high_layer
** 
** Made by nicolas
** Mail   <n.cormier@gmail.com>
** 
** Started on  Wed Mar 22 17:21:47 2006 nicolas
** Last update Sun Apr  9 11:14:10 2006 nicolas
*/

#ifndef __BIN_HIGH_LAYER_BIN_H__
# define __BIN_HIGH_LAYER_BIN_H__

# include "api/includes/types/bin.h"
# include "api/includes/types/depend.h"
# include "api/includes/types/prim_types.h"


depends_t*	bin_get_depends_list(bin_obj_t*);
int		bin_contain(bin_obj_t*, addr_t);
int		bin_get_dependload_addr(bin_obj_t*, addr_t*);

#endif /* __BIN_HIGH_LAYER_BIN_H__ */
