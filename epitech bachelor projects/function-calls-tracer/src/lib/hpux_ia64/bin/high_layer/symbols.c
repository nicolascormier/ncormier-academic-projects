/*
** symbols.c for  in /home/nico/lang/c/ftrace/src/api/bin
** 
** Made by nicolas
** Mail   <n.cormier@gmail.com>
** 
** Started on  Wed Mar 22 23:38:20 2006 nicolas
** Last update Tue Apr 25 09:32:53 2006 nicolas
*/

#include <stdlib.h>
#include <string.h>

#include "elfobj.h"

#include "api/includes/prototypes/bin/low_layer.h"
#include "api/includes/prototypes/bin/high_layer/symbols.h"
#include "api/includes/prototypes/prim_types.h"

/*
** Get section from name.
*/
static Elf_Shdr*	get_elfsection(bin_obj_t* obj, char* name)
{
  int		i;
  Elf_Shdr*	ret = NULL;
  elfobj_t*	elfobj = NULL;

  elfobj = (elfobj_t*) obj->luse;
  for (i = 0; i < elfobj->header->e_shnum; i++)
    if (!strcmp(elfobj->section_headers[i]->sh_name + elfobj->section_str, name))
      ret = elfobj->section_headers[i];
  return (ret);
}

syms_t*	syms_new(bin_obj_t* obj)
{
  elfobj_t*	elfobj = NULL;
  char*		sections[] = { ".symtab", ".dynsym" , NULL };
  int		i;
  Elf_Shdr*	section;
  Elf_Sym*	start, * end;
  syms_t*	list = NULL;
  sym_t*	sym;

  elfobj = (elfobj_t*) obj->luse;
  for (i = 0; sections[i]; i++)
    {
      section = get_elfsection(obj, sections[i]);
      if (!section)
	continue;
      start = (Elf_Sym *) ((char*) elfobj->header + section->sh_offset);
      end   = (Elf_Sym *) ((char*) elfobj->header + section->sh_offset + section->sh_size);
      /*
      ** Add every symbol to the list.
      */
      for (; start < end; start++)
	{
	  /*
	  ** J'ai virer le alloc ?! (TODO)
	  */
	  if (ELF_ST_TYPE(start->st_info) != STT_FUNC && ELF_ST_TYPE(start->st_info) != STT_NOTYPE)
	    continue;
	  /*
	  ** Skip unusable symbol.
	  */
	  if (start->st_value == 0x0)
	    continue;
	  sym = malloc(sizeof(sym_t));
	  if (sym == NULL)
	    goto syms_new_failed;
	  sym->addr = start->st_value + elfobj->base;
	  sym->size = start->st_size;
	  sym->name = (char*) ((char*) elfobj->header +
			 elfobj->section_headers[section->sh_link]->sh_offset +
			 start->st_name);
	  /* printf("Add Sym 0x%x size: %d name: %s\n", sym->addr, sym->size, sym->name); */
	  list = list_add(list, sym);
	  if (list == NULL)
	    goto syms_new_failed;
	}
    }
  return (list);

 syms_new_failed:
  for (; list; )
    {
      sym = (sym_t*) list->value;
      list = list_del(list, sym);
      free(sym);
    }
  return (NULL);
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

sym_t*	sym_get_frm_label(syms_t* syms, addr_t addr, int exact)
{
  list_t*	list;
  sym_t*	sym, * ret = NULL;

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
	      if (exact == APPROACHED)
		{
		  if (ret == NULL || ret->addr < sym->addr)
		    ret = sym;
		}
	      else if (sym->addr == addr)
		{
		  ret = sym;
		  break;
		}
	    }
	}
      if (list == ((list_t*) syms)->prev)
	break;
    }
  if(ret)
    return (ret);
  return (NULL);
}

sym_t*	sym_get_frm_addr(syms_t* syms, char* label)
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

