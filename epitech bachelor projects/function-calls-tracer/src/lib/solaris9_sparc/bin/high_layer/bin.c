/*
** bin.c for  in /home/nico/lang/c/ftrace/src/api/bin/high_layer
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Wed Mar 22 23:34:14 2006 nicolas
** Last update Wed Apr 26 23:52:50 2006 nicolas
*/

#include <stdlib.h>
#include <string.h>

#include "api/includes/prototypes/bin/high_layer/bin.h"
#include "api/includes/prototypes/bin/low_layer.h"

#include "elfobj.h"

list_t*	bin_get_depends_list(bin_obj_t* obj)
{
  /*
  ** A Partir De DT_NEEDED...
  */
  /*
   * Find the library with the given name, and return its full pathname.
   * The returned string is dynamically allocated.  Generates an error
   * message and returns NULL if the library cannot be found.
   *
   * If the second argument is non-NULL, then it refers to an already-
   * loaded shared object, whose library search path will be searched.
   *
   * The search order is:
   *   LD_LIBRARY_PATH
   *   rpath in the referencing file
   *   ldconfig hints
   *   /lib:/usr/lib
   */
  /*
  ** On solaris we use -> proc_get_depends_list.
  */
  obj = NULL;
  return (NULL);
}

/*
** Check if addr is a part of obj.
*/
int	bin_contain(bin_obj_t* obj, addr_t addr)
{
  addr_t	seg;
  int		i;
  elfobj_t*	luse;

  luse = obj->luse;
  for (i = 0; i < luse->header->e_phnum; i++)
    {
      seg = luse->program_headers[i]->p_vaddr + luse->base;
      if (addr >= seg && addr < seg + luse->program_headers[i]->p_memsz)
	return (1);
    }
  return (0);
}


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

int	bin_get_dependload_addr(bin_obj_t* obj, addr_t* addr)
{
  elfobj_t*	elfobj = NULL;
  char*		sections[] = { ".symtab", ".dynsym" , NULL };
  char*		name;
  int		i;
  Elf_Shdr*	section;
  Elf_Sym*	start, * end;

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
	  if (ELF_ST_TYPE(start->st_info) != STT_FUNC && ELF_ST_TYPE(start->st_info) != STT_NOTYPE)
	    continue;
	  name = (char*) ((char*) elfobj->header +
			 elfobj->section_headers[section->sh_link]->sh_offset +
			 start->st_name);
	  if (!strcmp("main", name))
	  {
	    *addr = start->st_value + elfobj->base;
	    return (0);
	  }
	}
    }
  return (-1);
}
