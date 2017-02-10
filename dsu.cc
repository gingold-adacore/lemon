#include <iostream>
#include <iomanip>

#include "dsu.h"
#include "soc.h"
#include "devices.h"
#include "outputs.h"
#include "sparc.h"
#include "osdep.h"
#include "breakpoint.h"
#include "loader.h"

using namespace std;

static list<forwarder *> poll_queue;

void
register_poll (forwarder *p)
{
  poll_queue.push_back (p);
}

static bool
execute_polls (void)
{
  bool res = false;
  for (auto p : poll_queue)
    if (p->poll_enabled)
      res |= p->poll ();
  return res;
}

class dsu4 : public dsu
{
 public:
  dsu4 (soc *parent, word base) : dsu (parent, base) {}
  void disp_info (void);
  virtual void init (bool reset);

  virtual void set_entry (word addr);
  virtual void stop (void);
  virtual void go (void);
  virtual void ahb_traces (int num);
  virtual void ahb_set (bool en);
  virtual void ahb_mask (bool en, bool mast, unsigned num);
  virtual void cache_sync (word addr);

  virtual void reset (void);
  virtual void release (void);

  //  Single step for processor NUM.
  void step (int num);

  word read_reg (word off);
  void write_reg (word off, word val);
private:

  word read_reg (int cpu, word off) { return read_reg ((cpu << 24) + off); }
  void write_reg (int cpu, word off, word val)
  {
    write_reg ((cpu << 24) + off, val);
  }
  word read_asi (int cpu, int asi, word off);

  void wait_event (void);
  void disp_event (int cpu);

  word ahb_idx_mask;
};

class leon4 : public Cpu
{
 public:
  leon4(unsigned int num, dsu4 &parent_dsu) :
    parent_dsu (parent_dsu),
    dsu_base (num << 24),
    itrace_mask (0) { name = "cpu" + dec (num); };

  virtual void disp_itrace (unsigned int nbr);
  virtual void set_itrace (bool en);
  
  virtual void hwatch_disp (void);
  virtual void hwatch_set (int num, word addr, bool data);
  virtual void hwatch_clear (int num);

  virtual void disp_cache_config (void);
  virtual void set_cache (bool en);
  virtual void dump_cache_content (bool i_d);
  virtual void disp_regs (void);
  virtual void disp_bt (void);
  virtual void set_entry (word addr);
  virtual void cache_flush (bool i_d);
  virtual void step (void);

  virtual void cache_sync (word addr);

  virtual void set_gpr (unsigned reg, word value);
  virtual word get_gpr (unsigned reg);

  void release (void);
  void dcache_flush (void);
  void icache_flush (void);

  void init (void);
  void reset (void);
private:
  word map_cpu_gpr (int cwp, int n);

  //  Read/write a per-cpu DSU register
  word read_dsu_reg (word off)
  {
    return parent_dsu.read_reg (dsu_base + off);
  }
  void write_dsu_reg (word off, word val)
  {
    parent_dsu.write_reg (dsu_base + off, val);
  }
  word read_asi (int asi, word off)
  {
    write_dsu_reg (DSU_ASI, asi);
    return read_dsu_reg (ASI_DIAG + off);
  }
  void write_asi (int asi, word off, word val)
  {
    write_dsu_reg (DSU_ASI, asi);
    write_dsu_reg (ASI_DIAG + off, val);
  }

  word read_cpu_gpr (int cwp, int n);
  void disp_frame (word pc, word sp);

  int get_num (void) { return dsu_base >> 24; }

  dsu4 &parent_dsu;
  word dsu_base;
  word itrace_mask;

  word asr17;
  int nwin; // Number of windows.  Set by init().
};

dsu *
find_dsu (soc *s)
{
  dsu *res = nullptr;

  for (auto d: s->get_devices ())
    {
      dsu *r;
      unsigned int id = d->get_id ();

      if (id_to_vid (id) == VENDOR_GAISLER)
	{
	  switch (id_to_did (id))
	    {
	    case DEVICE_DSU4:
	    case DEVICE_DSU3:
	      {
		ahb_device *ddev = dynamic_cast<ahb_device *>(d);
		word base = bar_to_base (ddev->get_pnp ().bar[0],
					 ddev->get_parent ()->base);
		r = new dsu4 (s, base);
	      }
	      break;
	    default:
	      r = nullptr;
	    }
	}
      else
	r = nullptr;

      if (r == nullptr)
	continue;
      if (res != nullptr)
	cerr << "More than one DSU" << endl;
      else
	res = r;
    }
  return res;
}

word
dsu4::read_reg (word off)
{
  return parent->get_link ()->read_word (base + off);
}

void
dsu4::write_reg (word off, word val)
{
  parent->get_link ()->write_word (base + off, val);
}

word
dsu4::read_asi (int cpu, int asi, word off)
{
  write_reg (cpu, DSU_ASI, asi);
  return read_reg (cpu, ASI_DIAG + off);
}

static void
disp_tt (int tt)
{
  switch (tt)
    {
    case  0: cout << "reset"; break;
    case  1: cout << "instruction access exception"; break;
    case  2: cout << "illegal instruction"; break;
    case  3: cout << "privileged instruction"; break;
    case  4: cout << "fpu disabled"; break;
    case  5: cout << "window overflow"; break;
    case  6: cout << "window underdlow"; break;
    case  7: cout << "memory address not aligned"; break;
    case  8: cout << "fpu exception"; break;
    case  9: cout << "data access exception"; break;
    case 10: cout << "tag overflow"; break;
    case 11: cout << "watchpoint detected"; break;
    case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16:
    case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c:
    case 0x1d: case 0x1e: case 0x1f:
      cout << "asynchronous interrupt"; break;
    case 0x24: cout << "coprocessor disabled"; break;
    case 0x2a: cout << "divide exception"; break;
    default:
      if (tt >= 0x80)
	cout << "software trap instruction";
      else
	cout << "unknown trap";
      break;
    }
}

static reg_desc psr_desc ("psr",
			  {
			    new fdesc ("IMPL", 28, 4),
			    new fdesc ("VER", 24, 4),
			    new fdesc ("ICC", 20, 4),
			    new fdesc ("EC", 13),
			    new fdesc ("EF", 12),
			    new fdesc ("PIL", 8, 4),
			    new fdesc ("S", 7),
			    new fdesc ("PS", 6),
			    new fdesc ("ET", 5),
			    new fdesc ("CWP", 0, 5)
			  } );

void
dsu4::disp_info (void)
{
  cout << " break ss: " << hex8 << read_reg (BREAK);
  cout << " debug mode mask: " << hex8 << read_reg (MASK) << endl;

  for (int i = 0; i < get_ncpus (); i++)
    {
      cout << "CPU#" << i << ":" << endl;
      word ctrl = read_reg (i, CTRL);
      cout << " control: " << hex8 << ctrl;
      word trap = read_reg (i, DSU_TRAP);
      cout << " trap: " << hex8 (trap) << "  ";
      disp_tt ((trap >> 4) & 0xff);
      cout << endl;
      cout << " PW:" << ((ctrl & CTRL_PW) ? '1' : '0');
      cout << " HL:" << ((ctrl & CTRL_HL) ? '1' : '0');
      cout << " PE:" << ((ctrl & CTRL_PE) ? '1' : '0');
      cout << " EB:" << ((ctrl & CTRL_EB) ? '1' : '0');
      cout << " EE:" << ((ctrl & CTRL_EE) ? '1' : '0');
      cout << " DM:" << ((ctrl & CTRL_DM) ? '1' : '0');
      cout << " BZ:" << ((ctrl & CTRL_BZ) ? '1' : '0');
      cout << " BX:" << ((ctrl & CTRL_BX) ? '1' : '0');
      cout << " BS:" << ((ctrl & CTRL_BS) ? '1' : '0');
      cout << " BW:" << ((ctrl & CTRL_BW) ? '1' : '0');
      cout << " BE:" << ((ctrl & CTRL_BE) ? '1' : '0');
      cout << " TE:" << ((ctrl & CTRL_TE) ? '1' : '0');
      cout << endl;
      cout << " time cntrl: " << hex8 << read_reg (i, TIME) << endl;

      cout << "   psr: " << hex8 << read_reg (i, PSR);
      cout << " wim: " << hex8 << read_reg (i, WIM);
      cout << "   tbr: " << hex8 << read_reg (i, TBR);
      cout << "   pc : " << hex8 << read_reg (i, PC);
      cout << " npc: " << hex8 << read_reg (i, NPC) << endl;
      cout << " asr17: " << hex8 << read_reg (i, ASR17);
      cout << " ccr: " << hex8 << read_asi (i, 2, 0);
      cout << " i-ccr: " << hex8 << read_asi (i, 2, 8);
      cout << " d-ccr: " << hex8 << read_asi (i, 2, 12) << endl;
    }
}

void
dsu4::stop (void)
{
  //  Stop all cpus:

  //  Set the BW bit.
  for (int i = 0; i < get_ncpus (); i++)
    {
      word ctrl = read_reg (i, CTRL);
      write_reg (i, CTRL, ctrl | CTRL_BW | CTRL_BS | CTRL_BE | CTRL_BZ);
    }

  //  Set break now.
  write_reg (BREAK, 0xffff);

  //  Check if in debug mode (and clear PE).
  for (int i = 0; i < get_ncpus (); i++)
    {
      word ctrl = read_reg (i, CTRL);
      if ((ctrl & CTRL_PW) != 0)
	continue;
      if (!(ctrl & CTRL_DM))
	{
	  cout << "CPU#" << i << " not in debug mode" << endl;
	  continue;
	}
      //  Clear PE (processor error).
      write_reg (i, CTRL, ctrl & ~CTRL_PE);
    }
}

void
dsu4::init (bool reset)
{
  unsigned int num = 0;

  //  Count number of cpus, and create them.
  word prev = read_reg (MASK);
  write_reg (MASK, 0xffff);
  word cur = read_reg (MASK);
  write_reg (MASK, prev);

  while (cur & 0x1)
    {
      cpus.push_back (new leon4(num++, *this));
      cur >>= 1;
    }

  //  Reset all cpus.
  if (reset)
    this->reset ();
  else
    this->stop ();

  //  Every cpu stop all cpus if it enters debug mode.
  write_reg (MASK, 0xffff);

  //  Init CPUs.
  for (auto c: cpus)
    static_cast<leon4*>(c)->init ();

  //  AHB trace size
  word tbcr = read_reg (AHB_TB_CTRL);
  write_reg (AHB_TB_CTRL, tbcr & ~TBCR_EN);
  word ahb_idx = read_reg (AHB_TB_INDEX);
  write_reg (AHB_TB_INDEX, 0xffffffff);
  ahb_idx_mask = read_reg (AHB_TB_INDEX);
  write_reg (AHB_TB_INDEX, ahb_idx);
  write_reg (AHB_TB_CTRL, tbcr);
}

void
dsu4::reset (void)
{
  stop ();

  //  Powerdown all slave cpus
  for (auto c: cpus)
    static_cast<leon4*>(c)->reset ();
}

void
leon4::reset (void)
{
  cout << "Reset " << get_name () << endl;
  
  word ctrl = read_dsu_reg (CTRL);

  //  Enable trace, break on errors
  ctrl |= CTRL_BE | CTRL_BW | CTRL_BS | CTRL_BZ | CTRL_TE;

  if (get_num () == 0)
    {
      //  Master CPU cannot be halted or down.
      write_dsu_reg (CTRL, ctrl & ~(CTRL_PW | CTRL_HL));
    }
  else
    {
      //  Slave CPUs: powerdown.
      if (!(ctrl & CTRL_PW))
	{
	  write_dsu_reg (CTRL, ctrl | CTRL_HL | CTRL_PE);

	  ctrl = read_dsu_reg (CTRL);
	  if (!(ctrl & CTRL_PW))
	    cout << "failed to powerdown cpu#" << get_name () << endl;
	}
      write_dsu_reg (CTRL, ctrl & ~(CTRL_HL | CTRL_PE));
    }

  //  Enable cache.
  set_cache (true);
}

void
dsu4::set_entry (word addr)
{
  /* Init all cpus.  */
  for (auto c: cpus)
    c->set_entry (addr);
}

void
leon4::set_entry (word addr)
{
  write_dsu_reg (PC, addr);
  write_dsu_reg (NPC, addr + 4);
  write_dsu_reg (PSR, 0x80);
  write_dsu_reg (WIM, 0);

#if 0
  //  Flush icache only if powered.
  cout << "flush cache on cpu#" << get_num () << endl;
  word ccr = read_asi (2, 0);
  if ((ccr & 1) == 1)
    icache_flush ();
#endif
}

void
leon4::set_gpr (unsigned reg, word value)
{
  if (reg == REG_SPARC_G0)
    return;
  word addr = map_cpu_gpr (0, reg);
  write_dsu_reg(addr, value);
}

word
leon4::get_gpr (unsigned reg)
{
  return read_cpu_gpr(0, reg);
}


void
dsu4::disp_event (int cpu)
{
  word ctrl = read_reg (cpu, CTRL);
  cout << "cpu#" << cpu << ": ";
  if (ctrl & CTRL_PE)
    cout << "error mode" << endl;
  else if (ctrl & CTRL_EB)
    cout << "external break" << endl;
  else if (ctrl & CTRL_DM)
    cout << "debug mode" << endl;

  word trap = read_reg (cpu, DSU_TRAP);
  cout << "trap: (" << hex8 (trap) << ") ";

  word tt = (trap >> 4) & 0xff;
  disp_tt (tt);
  cout << endl;
}

void
dsu4::wait_event (void)
{
  int timeout = 1;

  while (1)
    {
      for (int i = 0; i < get_ncpus (); i++)
	{
	  word ctrl = read_reg (i, CTRL);
	  if (ctrl & CTRL_DM)
	    {
	      disp_event (i);
	      return;
	    }
	}

      if (user_stop)
	{
	  cout << "User interrupt!" << endl;
	  stop ();
	  break;
	}

      if (execute_polls ())
	timeout = 1;
      else
	timeout *= 2;
      if (timeout > 20)
	timeout = 20;
      usleep (timeout * 1000);
    }
}

void
leon4::step (void)
{
  parent_dsu.step (get_num ());
}

void
dsu4::step (int num)
{
  ahb_set (true);

  //  Set SS flag, remove break-now flag.
  word ss = read_reg (BREAK);
  word msk = read_reg (MASK);
  ss |= 1 << (16 + num);
  ss &= ~ (1 << num);
  cout << "sstep on " << num << "  " << hex8 (ss) << endl;
  word msk1 = msk & ~(1 << num);
  write_reg (MASK, msk1);
  write_reg (BREAK, ss);

  ahb_set (false);
  wait_event ();
  write_reg (MASK, msk);
}

void
dsu4::go (void)
{
  insert_all_bp ();
  ahb_set (true);

  //  Remove break-now flag, remove SS flag.
  word ss = read_reg (BREAK);
  word mask = (1 << get_ncpus ()) - 1;
  ss &= (~mask) & 0xffff;
  write_reg (BREAK, ss);

  wait_event ();

  ahb_set (false);
  remove_all_bp ();
}

void
leon4::release (void)
{
  word ctrl = read_dsu_reg (CTRL);

  //  Disable trace, break on errors
  ctrl &= ~(CTRL_BE | CTRL_BW | CTRL_BS | CTRL_BZ | CTRL_TE);

  write_dsu_reg (CTRL, ctrl);
}
void
dsu4::release (void)
{
  for (auto c: cpus)
    static_cast<leon4*>(c)->release ();
  write_reg (BREAK, 0);
}

word
leon4::map_cpu_gpr (int cwp, int n)
{
  if (n < 8)
    {
      /* %g0 - %g7.  */
      return IU_REGS + n * 4 + (nwin * 64);
    }
  else if (n < 32)
    {
      /* %i0-%i7, %l0-%l7, and %o0-%o7.  */
      return IU_REGS + ((cwp * 64) + n * 4) % (nwin * 64);
    }
  else
    abort ();
}

word
leon4::read_cpu_gpr (int cwp, int n)
{
  if (n == 0)
    {
      /* %g0 is always 0.  */
      return 0;
    }
  else
    {
      word addr = map_cpu_gpr (cwp, n);
      return read_dsu_reg (addr);
    }
}

void
leon4::disp_regs (void)
{
  word psr = read_dsu_reg (PSR);
  word cwp = psr & 0x1f;

  cout << "   [cpu:" << name << " cwp=" << cwp << "  nwin=" << nwin << "]";
  cout << endl;
  for (int i = 0; i < 8; i++)
    {

      if (i == 6)
	cout << "  sp";
      else
	cout << "  o" << i;
      cout << ": ";
      word o_reg = read_cpu_gpr (cwp, 8 + i);
      cout << hex8 (o_reg);

      cout << "  l" << i << ": ";
      word l_reg = read_cpu_gpr (cwp, 16 + i);
      cout << hex8 (l_reg);

      if (i == 6)
	cout << "  fp";
      else
	cout << "  i" << i;
      cout << ": ";
      word i_reg = read_cpu_gpr (cwp, 24 + i);
      cout << hex8 (i_reg);

      cout << "  g" << i << ": ";
      word g_reg = read_cpu_gpr (cwp, i);
      cout << hex8 (g_reg) << endl;
    }
  cout << " psr: " << hex8 (psr);
  psr_desc.disp (cout, "  ", psr);
  cout << " wim: " << hex8 (read_dsu_reg (WIM));
  cout << " tbr: " << hex8 (read_dsu_reg (TBR)) << endl;
  word pc = read_dsu_reg (PC);
  cout << " pc : " << hex8 (pc) << " " << symbolize (pc) << endl;
  word insn = parent_dsu.get_link ()->read_word (pc);
  cout << "    [" << hex8 (insn) << "]  " << disa_sparc (pc, insn) << endl;
  cout << " npc: " << hex8 (read_dsu_reg (NPC)) << endl;
}

void
leon4::disp_frame (word pc, word sp)
{
  cout << hex8 (pc) << "  " << symbolize (pc);
  cout << "  (sp: " << hex8 (sp) << ")" << endl;

  word insn = parent_dsu.get_link ()->read_word (pc);
  //cout << "     [" << hex8 (insn) << "]  " << disa_sparc (pc, insn) << endl;
  cout << "  " << disa_sparc (pc, insn) << endl;
}

void
leon4::disp_bt (void)
{
  word psr = read_dsu_reg (PSR);
  word cwp = psr & 0x1f;
  word wim = read_dsu_reg (WIM);
  word last_pc;
  word last_sp;

  last_pc = read_dsu_reg (PC);
  last_sp = read_cpu_gpr (cwp, 14);
  disp_frame (last_pc, last_sp);

  //  From registers.
  while (1)
    {
      last_pc = read_cpu_gpr (cwp, 31);
      last_sp = read_cpu_gpr (cwp, 30);

      if (last_pc == 0 || last_sp == 0)
	return;

      disp_frame (last_pc, last_sp);

      // Previous frame.
      cwp++;
      if (cwp == nwin)
	cwp = 0;
      if ((wim & (1 << cwp)) != 0)
	break;
    }

  //  From memory.
  while (1)
    {
      word prev_sp = last_sp;
      last_pc = parent_dsu.get_link ()->read_word (last_sp + 60);
      last_sp = parent_dsu.get_link ()->read_word (last_sp + 56);

      if (last_pc == 0 || last_sp == 0 || prev_sp == last_sp)
	return;

      disp_frame (last_pc, last_sp);
    }
}


static reg_desc ccr_desc ("ccr",
			  {
			    new fdesc ("STE", 30),
			    new fdesc ("PS", 28),
			    new fdesc ("TB", 24, 4),
			    new fdesc ("DS", 23),
			    new fdesc ("FD", 22),
			    new fdesc ("FI", 21),
			    new fdesc ("FT", 19),
			    new fdesc ("ST", 17),
			    new fdesc ("IB", 16),
			    new fdesc ("IP", 15),
			    new fdesc ("DP", 14),
			    new fdesc ("ITE", 12, 2),
			    new fdesc ("IDE", 10, 2),
			    new fdesc ("DTE", 8, 2),
			    new fdesc ("DDE", 6, 2),
			    new fdesc ("DF", 5),
			    new fdesc ("IF", 4),
			    new fdesc ("DCS", 2, 2),
			    new fdesc ("ICS", 0, 2)
			  } );

class reg_desc_ccfg : public reg_desc
{
 public:
  reg_desc_ccfg (const char *name, std::list<base_desc *> fields) :
    reg_desc (name, fields) {}
  virtual void disp (std::ostream &stream, std::string pfx, word value);
};

void
reg_desc_ccfg::disp (std::ostream &stream, std::string pfx, word value)
{
  reg_desc::disp (stream, pfx, value);

  stream << pfx;

  const word ways = extract ("WAYS", value);
  if (ways == 0)
    stream << "direct mapped";
  else
    stream << dec (ways + 1) << " ways";

  const word ls = extract ("LSIZE", value);
  stream << ", line size: " << dec (4 << ls) << " bytes";

  const word ws = extract ("WSIZE", value);
  stream << ", capacity: " << dec ((ways + 1) * (1 << ws)) << " KB";

  const word mmu = extract ("M", value);
  stream << ", " << (mmu == 0 ? "no mmu" : "mmu");

  stream << endl;
}

static reg_desc_ccfg ccfg_desc
("ccfgr",
 {
   new fdesc ("CL", 31),
     new fdesc ("REPL", 28, 2),
     new fdesc ("SN", 27),
     new fdesc ("WAYS", 24, 3),
     new fdesc ("WSIZE", 20, 4),
     new fdesc ("LR", 19),
     new fdesc ("LSIZE", 16, 3),
     new fdesc ("M", 3),
     });

void
leon4::disp_cache_config (void)
{
  word ccr = read_asi (2, 0);
  cout << " ccr: " << hex8 (ccr) << endl;
  ccr_desc.disp (cout, "  ", ccr);

  word iccr = read_asi (2, 8);
  cout << " i-ccr: " << hex8 (iccr) << endl;
  ccfg_desc.disp (cout, "  ", iccr);

  word dccr = read_asi (2, 12);
  cout << " d-ccr: " << hex8 (dccr) << endl;
  ccfg_desc.disp (cout, "  ", dccr);
}

void
leon4::cache_flush (bool i_d)
{
  if (i_d)
    icache_flush ();
  else
    dcache_flush ();
}

void
leon4::dcache_flush (void)
{
  word ccr = read_asi (2, 0);
  ccr |= CCR_FD;
  write_asi (2, 0, ccr);
  while ((read_asi (2, 0) & CCR_DP) != 0)
    ;
}

void
leon4::icache_flush (void)
{
  word ccr = read_asi (2, 0);
  ccr |= CCR_FI;
  write_asi (2, 0, ccr);
#if 0
  while ((read_asi (2, 0) & CCR_IP) != 0)
    ;
#endif
}

void
leon4::set_cache (bool en)
{
  //  Flush all.
  word ccr = read_asi (2, 0);
  ccr |= CCR_FI | CCR_FD;
  write_asi (2, 0, ccr);

  word val;
  if (en)
    {
      //  Enable it (snoop, burst)
      val = 0x81000f;
    }
  else
    {
      val = 0;
    }
  write_asi (2, 0, val);
}

void
leon4::dump_cache_content (bool i_d)
{
  int asi = i_d ? 0x0c : 0x0e;
  word ccr = read_asi (2, i_d ? 8 : 12);
  word log_linesz = ccfg_desc.extract ("LSIZE", ccr);
  word line_words = 1 << log_linesz;
  word log_waysz = ccfg_desc.extract ("WSIZE", ccr);
  word ways = 1 + ccfg_desc.extract ("WAYS", ccr);
  word sz = (1024 << log_waysz) * ways;

  word valid_mask = (1 << (1 << log_linesz)) - 1;
  word tag_mask = ~0U << (10 + log_waysz);

  for (word i = 0; i < sz; i += 4 * line_words)
    {
      word tag = read_asi (asi, i);
      if ((tag & valid_mask) == 0)
	continue;

      // cout << hex8(i) << " " << hex8(tag) << "] "; // Verbose
      cout << hex8 ((tag & tag_mask) | (i & ~tag_mask)) << ":";
      for (word k = 0; k < line_words; k++)
	if ((tag >> k) & 1)
	  cout << " " << hex8 (read_asi (asi + 1, i + k * 4));
	else
	  cout << " ........";
      cout << endl;
    }
}

void
leon4::cache_sync (word addr)
{
  word ccr = read_asi (2, 0);

  //  Return now if I-cache not enabled.
  if ((ccr & 1) == 0)
    return;

  int asi = 0x0c;
  word iccr = read_asi (2, 8);
  word log_linesz = ccfg_desc.extract ("LSIZE", iccr);
  word log_waysz = ccfg_desc.extract ("WSIZE", iccr); // Log way size in KB.
  word ways = 1 + ccfg_desc.extract ("WAYS", iccr); // Number of ways per set.
  word waysz = 1024 << log_waysz;
  word atag_mask = ~(waysz - 1);
  word lmask = (4 << log_linesz) - 1;
  word widx = 1 << ((addr & lmask) >> 2);

  //cout << "iflush " << hex8(addr) << " on cpu " << get_num ()
  //     << " lmask: " << hex8 (lmask) << " lwords: " << hex8(line_words)
  //     << endl;

  for (word i = 0; i < ways; i++)
    {
      word caddr = (i << (10 + log_waysz))
	| (addr & (waysz - 1) & ~lmask);
      word tag = read_asi (asi, caddr);

      //cout << " caddr: " << hex8(caddr) << " tag: " << hex8(tag)
      //     << " atag_mask: " << hex8(atag_mask) << " widx: " << widx << endl;
      if ((tag & atag_mask) == (addr & atag_mask)
	  && (tag & widx) != 0)
	{
	  //cout << " tag " << hex8(tag) << endl;
	  tag &= ~widx;
	  write_asi (asi, caddr, tag);
	}
    }
}

void
dsu4::cache_sync (word addr)
{
  for (auto c : cpus)
    c->cache_sync (addr);
}

static reg_desc ahbtbcr_desc
("tbcr",
 {
   new fdesc ("DCNT", 16, 16),
     new fdesc ("DS", 10),
     new fdesc ("DT", 9),
     new fdesc ("DF", 8),
     new fdesc ("SF", 7),
     new fdesc ("TE", 6),
     new fdesc ("TF", 5),
     new fdesc ("BW", 3, 2),
     new fdesc ("BR", 2),
     new fdesc ("DM", 1),
     new fdesc ("EN", 0)
     });

void
dsu4::ahb_traces (int nbr)
{
  word tbcr = read_reg (AHB_TB_CTRL);
  cout << "AHB trace buffer control: " << hex8 (tbcr) << endl;
  ahbtbcr_desc.disp (cout, "  ", tbcr);

  word tbidx = read_reg (AHB_TB_INDEX);
  cout << "AHB trace buffer index: " << hex8 (tbidx) << endl;
  cout << "  time cntrl: " << hex8 (read_reg (TIME)) << endl;
  cout << " filter mask: " << hex8 (read_reg (AHB_TB_FILTER_MASK)) << endl;

  if (nbr > ahb_idx_mask + 1)
    nbr = ahb_idx_mask + 1;
  cout << "    Bp TimeTag  W Tr Sz Br Mst Lk Rsp Data     Addr" << endl;
  for (word i = (tbidx - nbr * 16) & ahb_idx_mask;
       nbr != 0;
       i = (i + 16) & ahb_idx_mask, nbr--)
    {
      word w0 = read_reg (AHB_TB + i + 0);
      word w1 = read_reg (AHB_TB + i + 4);
      word w2 = read_reg (AHB_TB + i + 8);
      word w3 = read_reg (AHB_TB + i + 12);

      cout << hex4 (i) << ": ";
      cout << ((w0 >> 31) ? "*" : " ") << hex8 (w0 & 0x7fffffffU);
      cout << " " << (((w1 >> 15) & 1) ? "W" : "R");	// Hwrite
      cout << " " << dec ((w1 >> 13) & 3);		// Htrans
      cout << "  " << dec ((w1 >> 10) & 7);		// Hsize
      cout << "  " << dec ((w1 >> 7) & 7);		// Hburst
      cout << "  " << hex2 ((w1 >> 3) & 0xf);         	// Hmaster
      cout << "  " << (((w1 >> 2) & 2) ? "L" : " ");  	// Hmastlock
      cout << "  " << dec ((w1 >> 0) & 3);            	// Hresp
      cout << "   " << hex8 (w2) << " " << hex8 (w3) << endl;
    }
}

void
dsu4::ahb_set (bool en)
{
  word tbcr = read_reg (AHB_TB_CTRL);
  if (en)
    tbcr |= TBCR_EN;
  else
    tbcr &= ~TBCR_EN;
  write_reg (AHB_TB_CTRL, tbcr);
}

void
leon4::disp_itrace (unsigned int nbr)
{
  word itrace_num = itrace_mask + 1;

  word itcr0 = read_dsu_reg (INSTR_TB_CTRL0);

#if 0
  cout << " insn trace ctrl0: " << hex8 (itcr0) << endl;

  word itcr1 = read_dsu_reg (INSTR_TB_CTRL1);
  cout << " insn trace ctrl1: " << hex8 (itcr1) << endl;

  word icount = read_dsu_reg (INSTR_COUNT);
  cout << " insn trace count: " << hex8 (icount) << endl;
#endif

  word itp = itcr0 & 0xffff;

  if (nbr > itrace_num)
    nbr = itrace_num - 1;

  cout << "M TimeTag  Result   T E PC       Opcode" << endl;
  for (int i = (itp - nbr) & itrace_mask;
       i != itp;
       i = (i + 1) & itrace_mask)
    {
      word w0 = read_dsu_reg (INSTR_TB + 16 * i + 0);
      word res = read_dsu_reg (INSTR_TB + 16 * i + 4);
      word pc = read_dsu_reg (INSTR_TB + 16 * i + 8);
      word insn = read_dsu_reg (INSTR_TB + 16 * i + 12);

      if (i == ((itp - 1) & itrace_mask))
	cout << "->";
      else
	cout << (w0 >> 31 ? "*" : " ") << " ";
      cout << hex8 (w0 & 0x7fffffff);
      cout << " " << hex8 (res);
      cout << " " << (pc & 2 ? "T" : " ");
      cout << " " << (pc & 1 ? "E" : " ");
      pc &= ~3U;
      cout << " " << hex8 (pc);
      cout << " " << hex8 (insn) << "  " << disa_sparc (pc, insn) << endl;
    }
}

void
leon4::set_itrace (bool en)
{
  word cr = read_dsu_reg (CTRL);
  if (en)
    cr |= CTRL_TE;
  else
    cr &= ~CTRL_TE;
  write_dsu_reg (CTRL, cr);
}

void
leon4::hwatch_disp (void)
{
  int nwp = (asr17 >> 5) & 7;
  
  for (word i = 0; i < nwp; i++)
    {
      cout << "#" << hex1 (i) << " addr: ";
      word addr = read_dsu_reg (ASR24 + 2 * i);
      cout << hex8 (addr);
      word mask = read_dsu_reg (ASR25 + 2 * i);
      cout << "  mask: " << hex8 (mask) << endl;
    }
}

void
leon4::hwatch_set (int num, word addr, bool data)
{
   int nwp = (asr17 >> 5) & 7;

   if (num >= nwp)
     {
       cout << "no such watchpoint" << endl;
       return;
     }
   if (data)
     {
       write_dsu_reg (ASR24 + 2 * num, addr & ~3UL);
       write_dsu_reg (ASR25 + 2 * num, ~(word)3 | 1);
     }
   else
     {
       write_dsu_reg (ASR24 + 2 * num, (addr & ~3UL) | 1);
       write_dsu_reg (ASR25 + 2 * num, ~(word)3);
     }
}

void
leon4::hwatch_clear (int num)
{
   int nwp = (asr17 >> 5) & 7;

   if (num >= nwp)
     {
       cout << "no such watchpoint" << endl;
       return;
     }
   write_dsu_reg (ASR24 + 2 * num, 0);
   write_dsu_reg (ASR25 + 2 * num, 0);
}

void
leon4::init (void)
{
  //  Compute itrace size:
  //  1. Disable trace.  This is required to modify CTRL0.
  word cr = read_dsu_reg (CTRL);
  write_dsu_reg (CTRL, cr & ~CTRL_TE);

  //  2. Set counter to max, and read back.
  word icount = read_dsu_reg (INSTR_TB_CTRL0);
  write_dsu_reg (INSTR_TB_CTRL0, 0xffffU);
  word ncount = read_dsu_reg (INSTR_TB_CTRL0);
  itrace_mask = ncount;

  //  #3. Restore values.
  write_dsu_reg (INSTR_TB_CTRL0, icount);
  write_dsu_reg (CTRL, cr);

  //  Enable itrace
  set_itrace (true);
  asr17 = read_dsu_reg (ASR17);
  nwin = (asr17 & 0x1f) + 1;
}

void
dsu4::ahb_mask (bool en, bool mast, unsigned num)
{
  if (num > 15)
    return;

  word mask = read_reg (AHB_TB_FILTER_MASK);
  if (!mast)
    num <<= 16;
  if (en)
    mask |= num;
  else
    mask &= ~num;
  write_reg (AHB_TB_FILTER_MASK, mask);
  word mask1 = read_reg (AHB_TB_FILTER_MASK);
  if (mask != mask1)
    cout << "Warning: masking not implemented" << endl;
}
