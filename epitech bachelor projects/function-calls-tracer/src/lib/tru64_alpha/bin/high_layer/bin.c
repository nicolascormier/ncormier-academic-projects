/*
** bin.c for  in /home/nico/lang/c/ftrace/src/api/bin/high_layer
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Wed Mar 22 23:34:14 2006 nicolas
** Last update Thu Apr 27 12:47:00 2006 nicolas rimbeau
*/

#include <stdlib.h>
#include <string.h>

int printf(const char *format, ...);
int puts(const char* string);

#include "api/includes/prototypes/bin/high_layer/bin.h"
#include "api/includes/prototypes/bin/low_layer.h"

#include "coffobj.h"

list_t*	bin_get_depends_list(bin_obj_t* obj)
{
  /*
  ** A Partir De DT_NEEDED...
  */

  int		i = 0;
  int		j = 0;
  coffobj_t*	coff;
  int		dynstr_sz = 0;
  int		dynstr_entries = 0;
  char*		dynstr = NULL;
  char**	strs;
  dynhdr_t*	dynhdr = NULL;

  coff = (coffobj_t*)obj->luse;
  while (coff->section_headers[i] != NULL)
    {
      if (strncmp(coff->section_headers[i]->s_name, ".dynstr", 7) == 0)
	{
	  dynstr = (char*)(*((unsigned long *)coff->section_headers[i]->s_vaddr));
	  dynstr_sz = *((unsigned int *)coff->section_headers[i]->s_size);
	}
      else if (strncmp(coff->section_headers[i]->s_name, ".dynamic", 8) == 0)
	  dynhdr = (dynhdr_t *)*((unsigned long *)coff->section_headers[i]->s_vaddr);
      i++;
    }
  if (dynstr == NULL)
    return (NULL);

  for (i = 0; i < dynstr_sz; i++)
    if (dynstr[i] == 0)
      dynstr_entries++;
  
  strs = malloc(sizeof(char *) * dynstr_entries + 1);
  for (i = 0, j = 0; i < dynstr_sz; i++)
    {
      strs[j++] = dynstr + i;
      while (i < dynstr_sz && dynstr[i] != 0)
	i++;
    }
  strs[i] = 0;

  if (dynhdr != NULL)
    while (dynhdr->d_tag != 0)
      {
/* 	if (dynhdr->d_tag == 1) */
/* 	  printf("==> %s\n", strs[dynhdr->d_un.d_val]); */
	dynhdr++;
      }
  
  free(strs);
  return (NULL);
}

/*
** Check if addr is a part of obj.
*/
int	bin_contain(bin_obj_t* obj, addr_t addr)
{
  addr_t	seg;
  unsigned int	i, r;
  coffobj_t*	luse;

  luse = obj->luse;
  for (i = 0; i < (*(unsigned short *)(luse->header->f_nscns)); i++)
    {
      r = *((unsigned int*) luse->section_headers[i]->s_vaddr);
      seg = (ulong) r + (ulong) luse->header;
      if (addr >= seg && addr < seg + (*(unsigned int *)(luse->section_headers[i]->s_size)))
	return (1);
    }
  return (0);
}

int    bin_get_dependload_addr(bin_obj_t* obj, addr_t* addr)
{
  obj = NULL;
  addr = NULL;
  return (-1);
}
