#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <alloca.h>

#include "api/includes/prototypes/bin/high_layer/debug.h"
#include "api/includes/prototypes/prim_types.h"
#include "includes/error.h"

#include "dwarf.h"

static t_dwarf		dwarf;
static t_file		*files = NULL;
static int		nfiles = 0;
static t_file		*currentfile = NULL;
static t_function	*currentfunction = NULL;

static unsigned int	Dwarf2_Read_LEB128(unsigned char *, int *, int);
static int		Dwarf2_FindAbbrevOff(int);
static t_abbrev		*Dwarf2_GetAbbrevInfos(int);
static debugs_t		*Dwarf2_BuildDebugsList(void);
static void		Dwarf2_ReadDebugInfo(void);
static void		Dwarf2_AddFile(unsigned char *);
static void		Dwarf2_AddNamedType(unsigned char *, int, int);
static void		Dwarf2_AddAnonType(int, int, int);
static void		Dwarf2_AddLinkedType(unsigned char *, int, int);
static void		Dwarf2_AddLine(unsigned int, int);
static void		Dwarf2_ParseDebugLine(int);
static char		*Dwarf2_ResolveType(int, int, unsigned int *);
static void		Dwarf2_AddFunction(unsigned char *, int, int);
static void		Dwarf2_AddParameter(unsigned char *, int);

/*
 *********************************************************
 * ELF parsing
 *********************************************************
 */

static char	*GetStringTable(Elf32_Ehdr *elf)
{
  Elf32_Shdr	*s;

  s = (Elf32_Shdr *) ((char *) elf + elf->e_shoff +
                     elf->e_shstrndx * elf->e_shentsize);
  return ((char *) elf + s->sh_offset);
}

static Elf32_Shdr	*GetELFSection(char *name, Elf32_Ehdr *elf)
{
  Elf32_Shdr		*s;
  char			*str;
  int			i;

  str = GetStringTable(elf);
  for (i = 0; i < elf->e_shnum; i++)
    {
      s = (Elf32_Shdr *) ((char *) elf + elf->e_shoff +
			  (i * elf->e_shentsize));
      if (!strcmp(str + s->sh_name, name))
	return (s);
    }
  return (NULL);
}

/*
 *********************************************************
 * Cleanup
 *********************************************************
 */

static void	Dwarf2_ParserInit(void)
{
  files = NULL;
  nfiles = 0;
  currentfile = NULL;
  currentfunction = NULL;
}

static void	Dwarf2_ParserUninit(void)
{
  int		i;
  int		j;
  int		k;
  t_type	*t;
  t_type	*next;

  for (i = 0; i < nfiles; i++)
    {
      free(files[i].f_name);
      free(files[i].f_lines);
      for (j = 0; j < files[i].f_nfunctions; j++)
	{
	  free(files[i].f_functions[j].f_name);
	  for (k = 0; k < files[i].f_functions[j].f_nargs; k++)
	    free(files[i].f_functions[j].f_args[k].a_name);
	  free(files[i].f_functions[j].f_args);
	}
      free(files[i].f_functions);
      for (t = files[i].f_types; t != NULL; )
	{
	  free(t->t_name);
	  next = t->t_next;
	  free(t);
	  t = next;
	}
    }
  free(files);
}

/*
 *********************************************************
 * API
 *********************************************************
 */

debugs_t	*debug_new_from_bin(bin_obj_t *elf)
{
  Elf32_Shdr	*dinfo;
  Elf32_Shdr	*dabbrev;
  Elf32_Shdr	*dstr;
  Elf32_Shdr	*dline;
  debugs_t	*list;

  Dwarf2_ParserInit();
  dinfo = GetELFSection(".debug_info", elf->map);
  if (dinfo == NULL)
    return (NULL);
  dwarf.d_dinfo = (unsigned char *) elf->map + dinfo->sh_offset;
  dwarf.d_dinfosize = dinfo->sh_size;

  dabbrev = GetELFSection(".debug_abbrev", elf->map);
  if (dabbrev == NULL)
    return (NULL);
  dwarf.d_dabbrev = (unsigned char *) elf->map + dabbrev->sh_offset;
  dwarf.d_dabbrevsize = dabbrev->sh_size;

  dstr = GetELFSection(".debug_str", elf->map);
  if (dstr != NULL)
    {
      dwarf.d_dstr = (unsigned char *) elf->map + dstr->sh_offset;
      dwarf.d_dstrsize = dstr->sh_size;
    }

  dline = GetELFSection(".debug_line", elf->map);
  if (dline != NULL)
    {
      dwarf.d_dline = (unsigned char *) elf->map + dline->sh_offset;
      dwarf.d_dlinesize = dline->sh_size;
    }

  Dwarf2_ReadDebugInfo();

  list = Dwarf2_BuildDebugsList();
  Dwarf2_ParserUninit();
  return (list);
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

  printf("get_frm_label\n");
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

/*
 *********************************************************
 * Build debugs_t list
 *********************************************************
 */

static debugs_t	*Dwarf2_BuildDebugsList(void)
{
  debugs_t	*list;
  debug_t	*elem;
  int		i;
  int		j;
  int		k;
  arg_t		*arg;

  for (i = 0, list = NULL; i < nfiles; i++)
    {
      for (j = 0; j < files[i].f_nfunctions; j++)
	{
	  elem = malloc(sizeof(*elem));
	  elem->file = strdup(files[i].f_name);

	  elem->name = strdup(files[i].f_functions[j].f_name);
	  elem->addr = files[i].f_functions[j].f_addr;
	  for (k = 0, elem->line = 0; k < files[i].f_nlines - 1; k++)
	    if ((files[i].f_lines[k].l_addr >= elem->addr) &&
		(elem->addr <= files[i].f_lines[k + 1].l_addr))
	      {
		elem->line = files[i].f_lines[k].l_line;
		break;
	      }

	  arg = malloc(sizeof(*arg));
	  memset(arg, 0, sizeof(*arg));

	  arg->name = strdup(files[i].f_functions[j].f_name);;
	  arg->type = Dwarf2_ResolveType(files[i].f_functions[j].f_rettype,
					i, &arg->size);

	  elem->ret = arg;
	  elem->args = NULL;

	  for (k = 0; k < files[i].f_functions[j].f_nargs; k++)
	    {
	      arg = malloc(sizeof(*arg));
	      memset(arg, 0, sizeof(*arg));

	      arg->type = Dwarf2_ResolveType(files[i].f_functions[j].f_args[k].a_typeoff,
					    i, &arg->size);
	      arg->name = strdup(files[i].f_functions[j].f_args[k].a_name);

	      arg->order = k;

	      elem->args = list_add(elem->args, arg);
	    }
	  list = list_add(list, elem);
	}
    }
  return (list);
}

/*
 *********************************************************
 * LEB128 processing
 *********************************************************
 */

static unsigned int	Dwarf2_Read_LEB128(unsigned char *data,
					   int *length_return, int sign)
{
  unsigned int		result;
  unsigned int		num_read;
  int			shift;
  unsigned char		byte;

  result = 0;
  shift = 0;
  num_read = 0;
  do
    {
      byte = * data ++;
      num_read ++;

      result |= (byte & 0x7f) << shift;

      shift += 7;

    }
  while (byte & 0x80);

  if (length_return != NULL)
    *length_return = num_read;

  if (sign && (shift < 32) && (byte & 0x40))
    result |= -1 << shift;

  return (result);
}

/*
 *********************************************************
 * Abbreviations processing
 *********************************************************
 */

static int	Dwarf2_FindAbbrevOff(int index)
{
  int		i;
  int		ret;
  unsigned char	v;

  for (i = dwarf.d_cuh->abbrevoff; i < dwarf.d_dabbrevsize; )
    {
      v = dwarf.d_dabbrev[i];
      if (v == 0)
	{
	  i++;
	  continue;
	}
      ret = Dwarf2_Read_LEB128(&v, NULL, 0);
      if (ret == index)
	return (i);
      for (; i < dwarf.d_dabbrevsize - 1; i++)
	if ((dwarf.d_dabbrev[i] == 0) && (dwarf.d_dabbrev[i + 1] == 0))
	  break;
      i += 2;
    }

  return (-1);
}

static t_abbrev		*Dwarf2_GetAbbrevInfos(int offset)
{
  int			i;
  t_abbrev		*a;
  unsigned char		v;

  a = malloc(sizeof(*a));
  if (a == NULL)
    errexit("Not enough memory\n");
  a->a_natts = 0;
  a->a_atts = NULL;
  a->a_forms = NULL;
  v = dwarf.d_dabbrev[offset + 1];
  a->a_tag = Dwarf2_Read_LEB128(&v, NULL, 0);
  v = dwarf.d_dabbrev[offset + 2];
  a->a_children = Dwarf2_Read_LEB128(&v, NULL, 0);
  for (i = 3; dwarf.d_dabbrev[offset + i] != 0; )
    {
      a->a_atts = realloc(a->a_atts, (a->a_natts + 1) * sizeof(*a->a_atts));
      if (a->a_atts == NULL)
	errexit("Not enough memory\n");
      a->a_forms = realloc(a->a_forms, (a->a_natts + 1) * sizeof(*a->a_forms));
      if (a->a_forms == NULL)
	errexit("Not enough memory\n");
      v = dwarf.d_dabbrev[offset + i++];
      a->a_atts[a->a_natts] = Dwarf2_Read_LEB128(&v, NULL, 0);
      v = dwarf.d_dabbrev[offset + i++];
      a->a_forms[a->a_natts] = Dwarf2_Read_LEB128(&v, NULL, 0);
      a->a_natts++;
    }
  return (a);
}

static void	Dwarf2_FreeAbbrevInfos(t_abbrev *a)
{
  free(a->a_atts);
  free(a->a_forms);
  free(a);
}

/*
 *********************************************************
 * FORMS processing
 *********************************************************
 */

static int	Dwarf2_ReadForm(t_forminfo *f, int form, int offset)
{
  int		len;

  memset(f, 0, sizeof(*f));
  switch (form)
    {
    case (DW_FORM_addr):
      memcpy(&f->f_ivalue, &dwarf.d_dinfo[offset], sizeof(f->f_ivalue));
      f->f_type = FORMTYPE_INT;
      return (dwarf.d_cuh->addrsize);

    case (DW_FORM_flag):
      f->f_cvalue = dwarf.d_dinfo[offset];
      f->f_type = FORMTYPE_CHAR;
      return (1);

    case (DW_FORM_string):
      f->f_svalue = &dwarf.d_dinfo[offset];
      f->f_type = FORMTYPE_STRING;
      for (len = 0; dwarf.d_dinfo[offset + len] != 0; len++)
	;
      return (len + 1);

    case (DW_FORM_strp):
      f->f_svalue = &dwarf.d_dstr[dwarf.d_dinfo[offset]];
      f->f_type = FORMTYPE_STRING;
      return (4);

    case (DW_FORM_data1):
      f->f_cvalue = dwarf.d_dinfo[offset];
      f->f_type = FORMTYPE_CHAR;
      return (1);

    case (DW_FORM_data2):
      memcpy(&f->f_shvalue, &dwarf.d_dinfo[offset], sizeof(f->f_shvalue));
      f->f_type = FORMTYPE_SHORT;
      return (2);

    case (DW_FORM_data4):
      memcpy(&f->f_ivalue, &dwarf.d_dinfo[offset], sizeof(f->f_ivalue));
      f->f_type = FORMTYPE_INT;
      return (4);

    case (DW_FORM_ref4):
      memcpy(&f->f_ivalue, &dwarf.d_dinfo[offset], sizeof(f->f_ivalue));
      f->f_cvalue = dwarf.d_dinfo[f->f_ivalue];
      f->f_type = FORMTYPE_INT;
      return (4);

    case (DW_FORM_block1):
      f->f_cvalue = dwarf.d_dinfo[offset];
      memcpy(f->f_b1value, &dwarf.d_dinfo[offset + 1], f->f_cvalue);
      f->f_type = FORMTYPE_BLOCK1;
      return (f->f_cvalue + 1);

    case (DW_FORM_sdata):
      {
	int len;
	f->f_ivalue = Dwarf2_Read_LEB128(&dwarf.d_dinfo[offset], &len, 0);
	f->f_type = FORMTYPE_INT;
	return (1);
      }
    }
  return (0);
}

/*
 *********************************************************
 * debug_info
 *********************************************************
 */

static int	Dwarf2_ParseDebugInfo(t_abbrev *a, int offset)
{
  int		j;
  t_forminfo	f;
  int		size;
  int		type;
  unsigned char	*name;
  int		oldoff;
  int		address;
  int		stmt;

  switch (a->a_tag)
    {
    case (DW_TAG_compile_unit):
      for (j = 0, stmt = 0; j < a->a_natts; j++)
	{
	  memset(&f, 0, sizeof(f));
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_name):
	      Dwarf2_AddFile(f.f_svalue);
	      break;

	    case (DW_AT_stmt_list):
	      stmt = f.f_ivalue;
	      break;
	    }
	}
      Dwarf2_ParseDebugLine(stmt);
      break;

    case (DW_TAG_base_type):
      oldoff = offset - dwarf.d_cuhoff - 1;
      for (j = 0, size = 0, name = NULL; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_name):
	      name = f.f_svalue;
	      break;

	    case (DW_AT_byte_size):
	      size = (f.f_type == FORMTYPE_CHAR) ? f.f_cvalue : f.f_shvalue;
	      break;
	    }
	}
      Dwarf2_AddNamedType(name, oldoff, size);
      break;

    case (DW_TAG_pointer_type):
      oldoff = offset - dwarf.d_cuhoff - 1;
      for (j = 0, type = 0, size = 0; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_type):
	      type = f.f_ivalue;
	      break;

	    case (DW_AT_byte_size):
	      size = (f.f_type == FORMTYPE_CHAR) ? f.f_cvalue : f.f_shvalue;
	      break;
	    }
	}
      Dwarf2_AddAnonType(oldoff, size, type);
      break;

    case (DW_TAG_typedef):
      oldoff = offset - dwarf.d_cuhoff - 1;
      for (j = 0, type = 0, name = NULL; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_type):
	      type = f.f_ivalue;
	      break;

	    case (DW_AT_name):
	      name = f.f_svalue;
	      break;
	    }
	}
      Dwarf2_AddLinkedType(name, oldoff, type);
      break;

    case (DW_TAG_structure_type):
      oldoff = offset - dwarf.d_cuhoff - 1;
      for (j = 0, type = 0, name = NULL; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_type):
	      type = f.f_ivalue;
	      break;

	    case (DW_AT_name):
	      name = f.f_svalue;
	      break;
	    }
	}
      if (name == NULL)
	Dwarf2_AddNamedType((unsigned char *) "unknown", oldoff, 13);
      else
	Dwarf2_AddNamedType(name, oldoff, 13);
      break;

    case (DW_TAG_subprogram):
      for (j = 0, type = 0, name = NULL, address = 0; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_type):
	      type = f.f_ivalue;
	      break;

	    case (DW_AT_name):
	      name = f.f_svalue;
	      break;

	    case (DW_AT_low_pc):
	      address = f.f_ivalue;
	      break;
	    }
	}
      if (address != 0)
	Dwarf2_AddFunction(name, type, address);
      break;

    case (DW_TAG_formal_parameter):
      for (j = 0, type = 0, name = NULL; j < a->a_natts; j++)
	{
	  offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
	  switch (a->a_atts[j])
	    {
	    case (DW_AT_name):
	      name = f.f_svalue;
	      break;

	    case (DW_AT_type):
	      type = f.f_ivalue;
	      break;
	    }
	}
      Dwarf2_AddParameter(name, type);
      break;

    default:
      for (j = 0; j < a->a_natts; j++)
	offset += Dwarf2_ReadForm(&f, a->a_forms[j], offset);
    }

  return (offset);
}

static int	Dwarf2_ReadCUH(int offset)
{
  dwarf.d_cuh = (Dwarf2_CUH *) &dwarf.d_dinfo[offset];
  dwarf.d_cuhoff = offset;
  return (offset + sizeof(*dwarf.d_cuh));
}

static void	Dwarf2_ReadDebugInfo(void)
{
  int		i;
  unsigned char	v;
  int		ret;
  t_abbrev	*a;
  int		childnbr;

  for (i = 0, childnbr = 0; i < dwarf.d_dinfosize; )
    {
      if (childnbr == 0)
	i = Dwarf2_ReadCUH(i);
      v = dwarf.d_dinfo[i++];
      ret = Dwarf2_Read_LEB128(&v, NULL, 0);
      if (ret == 0)
	{
	  if (childnbr == 0)
	    {
	      printf("End of child but .. no child!\n");
	      exit(0);
	    }
	  childnbr--;
	  continue;
	}
      a = Dwarf2_GetAbbrevInfos(Dwarf2_FindAbbrevOff(ret));
      childnbr += a->a_children;
      i = Dwarf2_ParseDebugInfo(a, i);
      Dwarf2_FreeAbbrevInfos(a);
    }
}

/*
 *********************************************************
 * debug_line
 *********************************************************
 */

static unsigned char	*Dwarf2_ReadString(int offset, int *readbytes)
{
  unsigned char		*ptr;

  ptr = &dwarf.d_dline[offset];
  if (*ptr == 0)
    {
      *readbytes = 1;
      return (NULL);
    }
  *readbytes = strlen((char *) ptr) + 1;
  return (ptr);
}

static void	Dwarf2_AddLine(unsigned int addr, int line)
{
  t_line	*lines;
  int		nlines;

  lines = currentfile->f_lines;
  nlines = currentfile->f_nlines;
  lines = realloc(lines, (nlines + 1) * sizeof(*lines));
  if (lines == NULL)
    errexit("Not enough memory\n");
  lines[nlines].l_addr = addr;
  lines[nlines].l_line = line;
  currentfile->f_lines = lines;
  currentfile->f_nlines++;
}

static void	Dwarf2_ParseDebugLine(int stmt)
{
  int		offset;
  int		i;
  unsigned char	*buf;
  int		readbytes;
  char		*opcode_length;

  unsigned int	address;
  unsigned int	file;
  unsigned int	line;
  unsigned int	column;
  char		is_stmt;
  char		basic_block;
  char		end_sequence;

  dwarf.d_sp = (Dwarf2_SP *) &dwarf.d_dline[stmt];
  /* temporaire */
  opcode_length = alloca(dwarf.d_sp->opcode_base * sizeof(*opcode_length));
  offset = stmt;
  for (offset += sizeof(*dwarf.d_sp), i = 1; i < dwarf.d_sp->opcode_base; i++)
    {
      opcode_length[i] = *(char *) &dwarf.d_dline[offset];
      offset++;
    }
  while ((buf = Dwarf2_ReadString(offset, &readbytes)) != NULL)
    offset += readbytes;
  offset += readbytes;
  while ((buf = Dwarf2_ReadString(offset, &readbytes)) != NULL)
    {
      offset += readbytes;
      offset += 3;
    }
  offset += readbytes;

  address = 0;
  file = 1;
  line = 1;
  column = 0;
  is_stmt = dwarf.d_sp->default_is_stmt;
  basic_block = 0;
  end_sequence = 0;

  while ((offset - stmt) < (int) (dwarf.d_sp->length + 4))
    {
      int type;
      unsigned char opcode;

      opcode = *(unsigned char *) &dwarf.d_dline[offset];
      offset++;

      if (opcode < dwarf.d_sp->opcode_base)
	{
	  if (opcode == DW_EXTENDED_OPCODE)
	    type = LOP_EXTENDED;
	  else
	    if ((HIGHEST_STANDARD_OPCODE + 1) >= dwarf.d_sp->opcode_base)
	      {
		type = LOP_STANDARD;
	      }
	    else
	      {
		int opcnt;
		int oc, len;
		opcnt = opcode_length[opcode];
		for (oc = 0; oc < opcnt; oc++)
		  {
		    Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len, 0);
		    offset += len;
		  }
		type = LOP_DISCARD;
	      }
	}
      else
	{
	  type = LOP_SPECIAL;
	}

      if (type == LOP_DISCARD)
	{
	}
      else
	if (type == LOP_SPECIAL)
	  {
	    opcode = opcode - dwarf.d_sp->opcode_base;
	    address = address + dwarf.d_sp->minimum_instruction_length *
	      (opcode / dwarf.d_sp->line_range);
	    line = line + dwarf.d_sp->line_base +
	      opcode % dwarf.d_sp->line_range;
	    Dwarf2_AddLine(address, line);
	    basic_block = 0;
	  }
	else
	  if (type == LOP_STANDARD)
	    {
	      switch (opcode)
		{
		case DW_LNS_copy:
		  Dwarf2_AddLine(address, line);
		  basic_block = 0;
		  break;

		case DW_LNS_advance_pc:
		  {
		    int len, addr;

		    addr = Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len,
					      0);
		    offset += len;
		    address += dwarf.d_sp->minimum_instruction_length * addr;
		  }
		  break;

		case DW_LNS_advance_line:
		  {
		    int len, l;

		    l = Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len,
					      1);
		    offset += len;
		    line += l;
		  }
		  break;

		case DW_LNS_set_file:
		  {
		    int len;

		    file = Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len, 0);
		    offset += 4;
		  }
		  break;

		case DW_LNS_set_column:
		  {
		    int len;

		    column = Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len,
						0);
		    offset += 4;
		  }
		  break;

		case DW_LNS_negate_stmt:
		  is_stmt = !is_stmt;
		  break;

		case DW_LNS_set_basic_block:
		  basic_block = 1;
		  break;

		case DW_LNS_const_add_pc:
		  opcode = MAX_LINE_OP_CODE - dwarf.d_sp->opcode_base;
		  address = address + dwarf.d_sp->minimum_instruction_length *
		    (opcode / dwarf.d_sp->line_range);
		  break;

		case DW_LNS_fixed_advance_pc:
		  address += *(short *)&dwarf.d_dline;
		  offset += 2;
		  break;

		}
	    }
	  else
	    if (type == LOP_EXTENDED)
	      {
		int len, instrlength;
		char ext_opcode;

		instrlength = Dwarf2_Read_LEB128(&dwarf.d_dline[offset], &len,
						 0);
		offset += len;
		ext_opcode = *(char *) &dwarf.d_dline[offset];
		offset++;
		switch (ext_opcode)
		  {
		  case DW_LNE_end_sequence:
		    end_sequence = 1;
		    Dwarf2_AddLine(address, line);
		    address = 0;
                    file = 1;
                    line = 1;
                    column = 0;
                    is_stmt = dwarf.d_sp->default_is_stmt;
                    basic_block = 0;
                    end_sequence = 0;
		    break;

		  case DW_LNE_set_address:
		    memcpy(&address, &dwarf.d_dline[offset], sizeof(address));
		    offset += 4;
		    break;

		  case DW_LNE_define_file:
		    Dwarf2_AddLine(address, line);
		    break;
		  }
	      }
    }
}

/*
 *********************************************************
 * Files handling
 *********************************************************
 */

static void	Dwarf2_AddFile(unsigned char *filename)
{
  files = realloc(files, (nfiles + 1) * sizeof(*files));
  if (files == NULL)
    errexit("Not enough memory\n");
  files[nfiles].f_name = strdup((char *) filename);
  files[nfiles].f_types = NULL;
  files[nfiles].f_lines = NULL;
  files[nfiles].f_nlines = 0;
  files[nfiles].f_functions = NULL;
  files[nfiles].f_nfunctions = 0;
  currentfile = &files[nfiles++];
}

/*
 *********************************************************
 * Types handling
 *********************************************************
 */

static void	Dwarf2_AddNamedType(unsigned char *name, int offset, int size)
{
  t_type	*t;

  t = malloc(sizeof(*t));
  if (t == NULL)
    errexit("Not enough memory\n");
  t->t_name = strdup((char *) name);
  t->t_offset = offset;
  t->t_size = size;
  t->t_ptr = 0;
  t->t_other = 0;
  t->t_flags = TYPEFLAGS_UNDEF;
  t->t_next = currentfile->f_types;
  currentfile->f_types = t;
}

static void	Dwarf2_AddAnonType(int offset, int size, int pointer)
{
  t_type	*t;

  t = malloc(sizeof(*t));
  if (t == NULL)
    errexit("Not enough memory\n");
  t->t_name = strdup("");
  t->t_offset = offset;
  t->t_size = size;
  t->t_ptr = pointer;
  t->t_other = 0;
  t->t_flags = TYPEFLAGS_UNDEF;
  t->t_next = currentfile->f_types;
  currentfile->f_types = t;
}

static void	Dwarf2_AddLinkedType(unsigned char *name, int offset,
				     int pointer)
{
  t_type	*t;

  t = malloc(sizeof(*t));
  if (t == NULL)
    errexit("Not enough memory\n");
  t->t_name = strdup((char *) name);
  t->t_offset = offset;
  t->t_size = 0;
  t->t_ptr = 0;
  t->t_other = pointer;
  t->t_flags = TYPEFLAGS_UNDEF;
  t->t_next = currentfile->f_types;
  currentfile->f_types = t;
}

static char	*Dwarf2_ResolveType(int type, int index, unsigned int *size)
{
  int		ptr;
  t_type	*t;
  char		*buf;
  int		i;

  for (t = files[index].f_types, ptr = 0; t != NULL; t = t->t_next)
    {
    restart:
      if (t->t_offset == type)
	{
	  if (t->t_ptr != 0)
	    {
	      type = t->t_ptr;
	      t = files[index].f_types;
	      ptr++;
	      goto restart;
	    }
	  else
	    goto found;
	}
    }
  *size = 0;
  return (strdup("void"));

 found:
  *size = t->t_size;
  if (t->t_other != 0)
    Dwarf2_ResolveType(t->t_other, index, size);
  buf = malloc(strlen(t->t_name) + 1 + ptr + 1);
  if (buf == NULL)
    errexit("Not enough memory\n");
  if (*t->t_name == 0)
    {
      /* void, pointer_type sans name */
      free(buf);
      ptr++;
      buf = malloc(4 + 1 + ptr + 1);
      if (buf == NULL)
	errexit("Not enough memory\n");
      strcpy(buf, "void");
    }
  else
    strcpy(buf, t->t_name);
  if (ptr > 0)
    {
      *size = sizeof(void *);
      strcat(buf, " ");
      for (i = 0; i < ptr; i++)
	strcat(buf, "*");
    }
  return (buf);
}

/*
 *********************************************************
 * Functions handling
 *********************************************************
 */

static void	Dwarf2_AddFunction(unsigned char *name, int rettype, int addr)
{
  t_function	*functions;
  int		nfunctions;

  functions = currentfile->f_functions;
  nfunctions = currentfile->f_nfunctions;
  functions = realloc(functions, (nfunctions + 1) * sizeof(*functions));
  if (functions == NULL)
    errexit("Not enough memory\n");
  functions[nfunctions].f_name = strdup((char *) name);
  functions[nfunctions].f_addr = addr;
  functions[nfunctions].f_rettype = rettype;
  functions[nfunctions].f_args = NULL;
  functions[nfunctions].f_nargs = 0;
  currentfunction = &functions[nfunctions];
  currentfile->f_functions = functions;
  currentfile->f_nfunctions++;
}

/*
 *********************************************************
 * Parameters handling
 *********************************************************
 */

static void	Dwarf2_AddParameter(unsigned char *name, int type)
{
  t_argument	*args;
  int		nargs;

  if (name == NULL)
    return ;
  if (currentfunction == NULL)
    return ;
  args = currentfunction->f_args;
  nargs = currentfunction->f_nargs;
  args = realloc(args, (nargs + 1) * sizeof(*args));
  if (args == NULL)
    errexit("Not enough memory\n");
  if (name != NULL)
    args[nargs].a_name = strdup((char *) name);
  else
    args[nargs].a_name = NULL;
  args[nargs].a_typeoff = type;
  currentfunction->f_args = args;
  currentfunction->f_nargs++;
}
