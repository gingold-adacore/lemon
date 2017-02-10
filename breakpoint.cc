#include <list>

#include "breakpoint.h"

using namespace std;

static unsigned int last_num = 0;
static list<breakpoint *> all_bps;

std::list<breakpoint *>
get_all_breakpoints (void)
{
  return all_bps;
}

class sbreak : public breakpoint
{
public:
  sbreak (dsu &board_dsu, unsigned int id, word addr) :
    breakpoint (id, addr), board_dsu (board_dsu) {}

  virtual void insert (void);
  virtual void remove (void);
 protected:
  dsu &board_dsu;
private:
  word prev_insn;
};

void
sbreak::insert (void)
{
  dsu_link *link = board_dsu.get_link ();
  prev_insn = link->read_word (get_addr ());
  link->write_word (get_addr (), 0x91d02001); // ta 1
  board_dsu.cache_sync (get_addr ());
}

void
sbreak::remove (void)
{
  dsu_link *link = board_dsu.get_link ();
  link->write_word (get_addr (), prev_insn);
  board_dsu.cache_sync (get_addr ());
}

breakpoint *
sbreak_create (dsu &board_dsu, word addr)
{
  sbreak *res = new sbreak (board_dsu, ++last_num, addr);

  all_bps.push_back (res);
  return res;
}

class hwatch : public breakpoint
{
public:
  hwatch (Cpu &cpu, unsigned int id, word addr) :
    breakpoint (id, addr), cpu (cpu) {}

  virtual void insert (void);
  virtual void remove (void);
private:
  Cpu &cpu;
};

void
hwatch::insert (void)
{
  cpu.hwatch_set (0, get_addr (), false);
}

void
hwatch::remove (void)
{
  cpu.hwatch_clear (0);
}

breakpoint *
hwatch_create (Cpu &cpu, word addr)
{
  hwatch *res = new hwatch (cpu, ++last_num, addr);

  all_bps.push_back (res);
  return res;
}

breakpoint *
get_breakpoint_by_id (unsigned int id)
{
  for (auto r: all_bps)
    if (r->id() == id)
      return r;
  return nullptr;
}

void
insert_all_bp (void)
{
  for (auto bp: all_bps)
    if (bp->enabled ())
      bp->insert ();
}

void
remove_all_bp (void)
{
  for (auto bp: all_bps)
    if (bp->enabled ())
      bp->remove ();
}
