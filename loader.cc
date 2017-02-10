#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "loader.h"
#include "lemon.h"
#include "outputs.h"

using namespace std;

struct elf32_external_ehdr
{
  unsigned char e_ident[16];
  unsigned char e_type[2];
  unsigned char e_machine[2];
  unsigned char e_version[4];
  unsigned char e_entry[4];
  unsigned char e_phoff[4];
  unsigned char e_shoff[4];
  unsigned char e_flags[4];
  unsigned char e_ehsize[2];
  unsigned char e_phentsize[2];
  unsigned char e_phnum[2];
  unsigned char e_shentsize[2];
  unsigned char e_shnum[2];
  unsigned char e_shstrndx[2];
};

#define EI_CLASS 4
#define ELFCLASS32 1

#define EI_DATA 5
#define ELFDATA2MSB 2

#define EI_VERSION 6
#define EV_CURRENT 1

#define EM_SPARC 2

struct elf32_external_shdr
{
  unsigned char sh_name[4];
  unsigned char sh_type[4];
  unsigned char sh_flags[4];
  unsigned char sh_addr[4];
  unsigned char sh_offset[4];
  unsigned char sh_size[4];
  unsigned char sh_link[4];
  unsigned char sh_info[4];
  unsigned char sh_addralign[4];
  unsigned char sh_entsize[4];
};

typedef unsigned int Elf32_Word;
typedef unsigned int Elf32_Addr;
typedef unsigned int Elf32_Off;

struct elf32_shdr
{
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
};

#define SHF_WRITE (1 << 0)
#define SHF_ALLOC (1 << 1)
#define SHF_EXECINSTR (1 << 2)

#define SHT_SYMTAB	2
#define SHT_NOBITS	8

#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2

struct elf32_external_sym
{
  unsigned char st_name[4];
  unsigned char st_value[4];
  unsigned char st_size[4];
  unsigned char st_info[1];
  unsigned char st_other[1];
  unsigned char st_shndx[2];
};

class elf_file
{
public:
  elf_file (const char *filename);
  bool check_elf (void);
  void read_shdr (elf32_shdr &shdr, unsigned int s);
  unsigned char *read_section (const elf32_shdr &shdr);
  unsigned int get_shstrndx (void) { return unpack_be16 (ehdr.e_shstrndx); }
  unsigned int get_shnum (void) { return unpack_be16 (ehdr.e_shnum); }
  word get_entry (void) { return unpack_be32 (ehdr.e_entry); }
private:
  const char *filename;
  ifstream file;
  elf32_external_ehdr ehdr;
  Elf32_Off shoff;
};

elf_file::elf_file (const char *filename) :
  filename (filename), file (filename, ios::in | ios::binary)
{
}

bool
elf_file::check_elf (void)
{
  if (!file.is_open ())
    {
      cerr << filename << ": unable to open" << endl;
      return false;
    }

  file.read ((char *)&ehdr, sizeof (ehdr));
  if (ehdr.e_ident[0] != 0x7f
      || ehdr.e_ident[1] != 'E'
      || ehdr.e_ident[2] != 'L'
      || ehdr.e_ident[3] != 'F')
    {
      cerr << filename << ": not an ELF file" << endl;
      return false;
    }
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS32
      || ehdr.e_ident[EI_DATA] != ELFDATA2MSB
      || ehdr.e_ident[EI_VERSION] != EV_CURRENT)
    {
      cerr << filename << ": not ELF32 big-endian" << endl;
      return false;
    }

  if (unpack_be16 (ehdr.e_machine) != EM_SPARC)
    {
      cerr << filename << ": not a SPARC binary" << endl;
      return false;
    }
  if (unpack_be16 (ehdr.e_shentsize) != sizeof (elf32_external_shdr))
    {
      cerr << filename << ": bad ehdr value" << endl;
      return false;
    }

  shoff = unpack_be32 (ehdr.e_shoff);
  return true;
}

void
elf_file::read_shdr (elf32_shdr &shdr, unsigned int s)
{
  elf32_external_shdr raw;

  if (s >= get_shnum ())
    throw "bad sh index";

  file.seekg (shoff + s * sizeof (elf32_external_shdr));
  file.read ((char *)&raw, sizeof (elf32_external_shdr));

  shdr.sh_name = unpack_be32 (raw.sh_name);
  shdr.sh_type = unpack_be32 (raw.sh_type);
  shdr.sh_flags = unpack_be32 (raw.sh_flags);
  shdr.sh_addr = unpack_be32 (raw.sh_addr);
  shdr.sh_offset = unpack_be32 (raw.sh_offset);
  shdr.sh_size = unpack_be32 (raw.sh_size);
  shdr.sh_link = unpack_be32 (raw.sh_link);
  shdr.sh_info = unpack_be32 (raw.sh_info);
  shdr.sh_addralign = unpack_be32 (raw.sh_addralign);
  shdr.sh_entsize = unpack_be32 (raw.sh_entsize);
}

unsigned char *
elf_file::read_section (const elf32_shdr &shdr)
{
  unsigned char *res = new unsigned char[shdr.sh_size];

  file.seekg (shdr.sh_offset);
  file.read ((char *)res, shdr.sh_size);

  return res;
}

static std::map<word, string> symbols_map;
static std::vector<pair<word, string&>> symbols_vec;

static bool
sym_compare(pair<word, string &> a, pair<word, string &> b)
{
  return a.first < b.first;
}

void
load_bin (dsu_link *link,
	  word addr, const unsigned char *buf, word len)
{
  int chunk_size = link->get_max_len ();
  unsigned off;

  //  Write by chunk
  for (off = 0; off + chunk_size <= len; off += chunk_size)
    {
      if (!link->write (addr + off, chunk_size / 4, buf + off))
	{
	  cerr << "write error" << endl;
	  break;
	}
    }

  if (off != len)
    {
      int r = len - off;
      unsigned char pad[2048];

      memset (pad, 0, sizeof (pad));
      memcpy (pad, buf + off, r);
      if (!link->write (addr + off, (r + 3) / 4, pad))
	cerr << "write error" << endl;
    }
}

void
load_elf (dsu *a_dsu, const char *filename, bool content)
{
  elf_file file (filename);

  if (!file.check_elf ())
    return;

  //  Read section strings
  elf32_shdr shstrsh;
  file.read_shdr (shstrsh, file.get_shstrndx ());
  unsigned char *shstr = file.read_section (shstrsh);
  dsu_link *link = content ? a_dsu->get_link () : nullptr;

  unsigned int symtab_idx = 0;
  for (int i = 0; i < file.get_shnum (); i++)
    {
      elf32_shdr shdr;
      file.read_shdr (shdr, i);

      if (content
	  && (shdr.sh_flags & SHF_ALLOC) != 0
	  && shdr.sh_type != SHT_NOBITS)
	{
	  Elf32_Word addr = shdr.sh_addr;

	  cout << "section: " << shstr + shdr.sh_name;
	  cout << " at " << hex8 << addr;
	  cout << ", size: " << hex8 << shdr.sh_size << endl;

	  unsigned char *buf = file.read_section (shdr);

	  load_bin (link, addr, buf, shdr.sh_size);

	  delete [] buf;
	}
      else if (shdr.sh_type == SHT_SYMTAB)
	symtab_idx = i;
    }

  //  Load symbols.
  if (symtab_idx != 0)
    {
      elf32_shdr symtab_shdr;
      file.read_shdr (symtab_shdr, symtab_idx);

      elf32_shdr strtab_shdr;
      file.read_shdr (strtab_shdr, symtab_shdr.sh_link);

      unsigned char *syms = file.read_section (symtab_shdr);
      unsigned char *strs = file.read_section (strtab_shdr);

      symbols_vec.clear();
      symbols_map.clear();

      for (int i = 0;
	   i < symtab_shdr.sh_size;
	   i += sizeof (elf32_external_sym))
	{
	  elf32_external_sym *s = (elf32_external_sym *)&syms[i];
	  word val = unpack_be32 (s->st_value);
	  char *name = (char *)strs + unpack_be32 (s->st_name);
	  unsigned int stt = s->st_info[0] & 0x0f;

	  if ((stt == STT_OBJECT || stt == STT_FUNC || stt == STT_NOTYPE)
	      && *name != 0)
	    symbols_map.insert (std::pair<word, string>(val, name));
	}

      delete [] syms;
      delete [] strs;

      cout << dec (symbols_map.size()) << " symbols" << endl;

      for (auto &s: symbols_map)
	symbols_vec.push_back (std::pair<word, string&>(s.first, s.second));
      sort (symbols_vec.begin (), symbols_vec.end (), sym_compare);
    }
  delete [] shstr;

  if (content)
    {
      word start = file.get_entry ();
      cout << "Entry point: " << hex8 (start) << endl;
      a_dsu->set_entry (start);
    }
}

string
symbolize (word addr)
{
  unsigned int hi = symbols_vec.size ();
  unsigned int lo = 0;

  //  No symbols.
  if (hi == 0)
    return hex8 (addr);
    else
      hi--;

  while (lo + 1 < hi)
    {
      unsigned int mid = (hi + lo) / 2;
      word v = symbols_vec[mid].first;
      if (addr < v)
	hi = mid - 1;
      else if (addr > v)
	lo = mid;
      else
	{
	  lo = hi = mid;
	  break;
	}
    }
  if (hi != lo)
    {
      if (addr >= symbols_vec[hi].first)
	lo = hi;
    }
  pair<word, string &> res = symbols_vec[lo];

  if (res.first == addr)
    return res.second;
  else
    return res.second + "+" + hex8 (addr - res.first);
}

void
disp_symbols (void)
{
  for (auto &s : symbols_vec)
    cout << hex8 (s.first) << ": " << s.second << endl;
}

void
load_bin (dsu &a_dsu, word addr, const unsigned char *buf, word len)
{
  load_bin (a_dsu.get_link (), addr, buf, len);
}
