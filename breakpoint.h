#ifndef BREAKPOINT_H_
#define BREAKPOINT_H_

#include "lemon.h"
#include "dsu.h"

class breakpoint
{
 public:
  breakpoint (unsigned int id, word addr) :
    the_id (id), addr (addr), is_enabled (true) {}

  //  Break point identifier
  unsigned int id (void) { return the_id; }

  word get_addr (void) { return addr; }
  void enable (void) { is_enabled = true; }
  void disable (void) { is_enabled = false; }
  bool enabled (void) { return is_enabled; }

  virtual void insert (void) = 0;
  virtual void remove (void) = 0;
 private:
  unsigned int the_id;
  word addr;
  bool is_enabled;
};

breakpoint *sbreak_create (dsu &board_dsu, word addr);
breakpoint *hwatch_create (Cpu &cpu, word addr);
breakpoint *get_breakpoint_by_id (unsigned int id);

void insert_all_bp (void);
void remove_all_bp (void);

std::list<breakpoint *> get_all_breakpoints (void);

#endif /* BREAKPOINT_H_ */
