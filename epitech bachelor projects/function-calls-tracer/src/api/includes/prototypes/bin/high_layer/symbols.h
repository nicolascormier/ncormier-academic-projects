/*
** symbols.h for  in /home/nico/lang/c/ftrace/src/api/includes/prototypes/bin/high_layer
** 
** Made by nicolas
** Mail   <n.cormier@gmail.com>
** 
** Started on  Wed Mar 22 17:22:09 2006 nicolas
** Last update Thu Apr 27 13:03:54 2006 nicolas
*/

#ifndef __BIN_HIGH_LAYER_SYMBOLS_H__
# define __BIN_HIGH_LAYER_SYMBOLS_H__

# include "api/includes/types/bin.h"
# include "api/includes/types/prim_types.h"


syms_t*		syms_new(bin_obj_t*);
int		syms_del(syms_t*);

#define APPROACHED	0
#define EXACT		1
sym_t*		sym_get_frm_addr(syms_t*, addr_t, int);
sym_t*		sym_get_frm_label(syms_t*, char*);


#endif /* __BIN_HIGH_LAYER_SYMBOLS_H__ */
