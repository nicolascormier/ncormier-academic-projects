/*
** low_layer.h for  in /home/nico/lang/c/ftrace/src/api/includes/prototypes/bin
** 
** Made by nicolas
** Mail   <n.cormier@gmail.com>
** 
** Started on  Wed Mar 22 17:10:57 2006 nicolas
** Last update Wed Apr  5 23:02:29 2006 nicolas
*/

#ifndef __BIN_LOW_LAYER_H__
# define __BIN_LOW_LAYER_H__

# include "api/includes/types/bin.h"
# include "api/includes/types/prim_types.h"

bin_obj_t*	bin_new(char*, addr_t);
int		bin_del(bin_obj_t*);
int		bin_read(bin_obj_t*, addr_t, unsigned int, char*);


#endif /* __BIN_LOW_LAYER_H__ */
