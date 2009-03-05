/*
** low_layer.c for  in /home/nico/lang/c/ftrace/lib/freebsd_ia32/bin
**
** Made by nicolas
** Mail   <n.cormier@gmail.com>
**
** Started on  Thu Mar 23 13:53:30 2006 nicolas
** Last update Wed Apr 26 13:36:16 2006 nicolas rimbeau
*/

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "coffobj.h"

#include "includes/error.h"

#include "api/includes/prototypes/bin/low_layer.h"


bin_obj_t*	bin_new(char* path, addr_t base)
{
  void*			data;
  struct stat		sb;
  int			fd;
  bin_obj_t*		ret;
  int			i;
  coffobj_t*		luse;

  /*
  ** TOFIX: vous ne vous servez pas de base ...
  */

  base = 0;

  /*
  ** Map the binary.
  */

  if ((fd = open(path, O_RDONLY)) == -1)
    return (NULL);
  if (fstat(fd, &sb) == -1)
    {
      close(fd);
      return (NULL);
    }
  data = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (data == MAP_FAILED)
    return (NULL);

  /*
  ** Check if the binary is really a coff.
  */

  if (*((short*)data) != MAGIC_COFF_HDR)
    {
      errno = EFTRACE;
      ftrace_errstr = "File is not a True64 Coff file.";
      munmap(data, sb.st_size);
      return (NULL);
    }

  /*
  ** Fill the return
  */

  ret = malloc(sizeof(bin_obj_t));
  if (ret == NULL)
    {
      munmap(data, sb.st_size);
      return (NULL);
    }
  ret->path = strdup(path);
  ret->map = data;
  ret->fd = -1;
  ret->size = sb.st_size;
  ret->luse = NULL;

  /*
  ** Fill luse.
  */

  luse = malloc(sizeof(coffobj_t));
  luse->header = (filehdr_t*) data;
  luse->section_headers = malloc(sizeof(struct scnhdr*) * (*(unsigned short *)(luse->header->f_nscns + 1)));
  if (luse == NULL)
    return (ret);
/*   printf("File header : [0x%p]\n", luse->header); */
/*   printf("Magic : [0x%x]\n", (*(unsigned short *)(luse->header->f_magic))); */
/*   printf("Sections : [%d]\n", (*(unsigned short *)(luse->header->f_nscns))); */

  /*
  ** Section headers.
  */

  for (i = 0; i < (*(unsigned short *)(luse->header->f_nscns)); i++)
    luse->section_headers[i] =
	(scnhdr_t*)((data + sizeof(filehdr_t) + sizeof(aouthdr_t)) + (i * sizeof(scnhdr_t)));
  luse->section_headers[i] = NULL;
  luse->header = data;

  ret->luse = luse;

  return (ret);

/*  luse_fill_failed: */
/*   bin_del(ret); */
/*   if (luse->section_headers) */
/*     free(luse->section_headers); */
/*   free(luse); */
  return (NULL);
}

int	bin_del(bin_obj_t* obj)
{
  coffobj_t*	luse;

  if (obj->path)
    free(obj->path);
  if (obj->map)
    {
      if (munmap(obj->map, obj->size) == -1)
	return (-1);
    }
  if (obj->fd == -1)
    {
      if (close(obj->fd) == -1)
	return (-1);
    }
  if (obj->luse)
    {
      luse = obj->luse;
      if (luse->section_headers)
	free(luse->section_headers);
      free(luse);
    }
  free(obj);
  return (0);
}

int	bin_read(bin_obj_t* obj, addr_t addr, unsigned int len, char* laddr)
{
  memcpy(laddr, obj->map + (int) addr, len);
  return (0);
}
