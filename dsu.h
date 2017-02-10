#ifndef DSU_H_
#define DSU_H_

#include "soc.h"

class Cpu
{
 public:
  // Disp registers.
  virtual void disp_regs (void) = 0;

  // Disp a back trace.
  virtual void disp_bt (void) = 0;

  // Disp instruction trace
  virtual void disp_itrace (unsigned int num) = 0;

  // Enable or disable instruction tracing.
  virtual void set_itrace (bool en) = 0;

  // Display hardware watchpoint/breakpoint regs.
  virtual void hwatch_disp (void) = 0;

  // Set/remove a write watchpoint
  virtual void hwatch_set (int num, word addr, bool data) = 0;
  virtual void hwatch_clear (int num) = 0;

  // Disp cache config trace
  virtual void disp_cache_config (void) = 0;

  // Enable or disable cache.
  virtual void set_cache (bool en) = 0;

  //  Flush I or D cache.
  virtual void cache_flush (bool i_d) = 0;

  //  Flush one word.
  virtual void cache_sync (word addr) = 0;

  // Dump cache content
  virtual void dump_cache_content (bool i_d) = 0;

  // Set then entry point.
  virtual void set_entry (word addr) = 0;

  // Single step.
  virtual void step (void) = 0;

  // Read or write a register
  virtual void set_gpr (unsigned reg, word value) = 0;
  virtual word get_gpr (unsigned reg) = 0;

  std::string get_name (void) { return name; }
 protected:
  std::string name;
};

class dsu
{
 public:
  dsu (soc *parent, word base) : base (base), parent (parent) {}
  virtual void disp_info (void) = 0;

  // Initialize the dsu.  Compute number of cpus.
  virtual void init (bool reset) = 0;

  // Get hardware link to the dsu.
  dsu_link *get_link (void) { return parent->get_link (); }

  // Get number of cpus.
  int get_ncpus (void) { return cpus.size(); }

  // Set entry point.
  virtual void set_entry (word addr) = 0;

  // Resume execution
  virtual void go (void) = 0;

  // Stop execution.
  virtual void stop (void) = 0;

  // Flush icache for ADDR.
  virtual void cache_sync (word addr) = 0;

  // Disp AHB traces and registers
  virtual void ahb_traces (int num) = 0;

  // Enable or disable AHB traces.
  virtual void ahb_set (bool en) = 0;

  // Enable or disable traces mask for master NUM (if MAST is true) or
  // for slave NUM.  Note: enabling the mask means no traces.
  virtual void ahb_mask (bool en, bool mast, unsigned num) = 0;

  virtual void reset (void) = 0;

  //  Release all cpus: let them run without dsu monitoring.
  virtual void release (void) = 0;

  const std::list<Cpu *> &get_cpus (void) { return cpus; }
protected:
  word base;
  soc *parent;
  list<Cpu *>cpus;
};

class forwarder
{
 public:
  //  This function is called while the CPU are running to poll some devices
  //  (eg: to display uart output).
  virtual bool poll (void) = 0;

  //  If true, the forwarder is enabled.
  bool poll_enabled = false;
};

//  Search for a DSU in S.  Return NULL in case of error.
//  Emit an error message if more than one DSU.
extern dsu *find_dsu (soc *s);

extern void register_poll (forwarder *p);

//  DSU registers.
enum dsu_registers
  {
    CTRL	       = 0x000000,
    TIME	       = 0x000008,
    BREAK	       = 0x000020,
    MASK	       = 0x000024,
    AHB_TB_CTRL	       = 0x000040,
    AHB_TB_INDEX       = 0x000044,
    AHB_TB_FILTER_CTRL = 0x000048,
    AHB_TB_FILTER_MASK = 0x00004c,
    AHB_BP_ADDR1       = 0x000050,
    AHB_BP_MASK1       = 0x000054,
    AHB_BP_ADDR2       = 0x000058,
    AHB_BP_MASK2       = 0x00005c,
    INSTR_COUNT	       = 0x000060,
    INSTR_TB	       = 0x100000,
    INSTR_TB_CTRL0     = 0x110000,
    INSTR_TB_CTRL1     = 0x110004,
    AHB_TB	       = 0x200000,
    IU_REGS	       = 0x300000,
    FPU_REGS	       = 0x301000,
    Y		       = 0x400000,
    PSR		       = 0x400004,
    WIM		       = 0x400008,
    TBR		       = 0x40000c,
    PC		       = 0x400010,
    NPC		       = 0x400014,
    FSR		       = 0x400018,
    CPSR	       = 0x40001c,
    DSU_TRAP	       = 0x400020,
    DSU_ASI	       = 0x400024,
    ASR16	       = 0x400040,
    ASR17	       = 0x400044,
    ASR19	       = 0x40004c,
    ASR24	       = 0x400060,
    ASR25	       = 0x400064,
    ASI_DIAG	       = 0x700000,
  };

enum dsu_ctrl_registers
  {
    CTRL_TE = (1 << 0),
    CTRL_BE = (1 << 1),
    CTRL_BW = (1 << 2),
    CTRL_BS = (1 << 3),
    CTRL_BX = (1 << 4),
    CTRL_BZ = (1 << 5),
    CTRL_DM = (1 << 6),
    CTRL_EE = (1 << 7),
    CTRL_EB = (1 << 8),
    CTRL_PE = (1 << 9),
    CTRL_HL = (1 << 10),
    CTRL_PW = (1 << 11),
  };

enum dsu_tbcr_registers
  {
    TBCR_EN = (1 << 0),
    TBCR_DM = (1 << 1),
    TBCR_BR = (1 << 2),
    TBCR_BW = (3 << 3),
    TBCR_TF = (1 << 5),
    TBCR_TE = (1 << 6),
    TBCR_SF = (1 << 7),
    TBCR_DF = (1 << 8),
    TBCR_DT = (1 << 9),
    TBCR_DS = (1 << 10)
  };

enum leon_ccr_registers
  {
    CCR_ICS = (3 << 0),
    CCR_DCS = (3 << 2),
    CCR_IF = (1 << 4),
    CCR_DF = (1 << 5),
    CCR_DP = (1 << 14),
    CCR_IP = (1 << 15),
    CCR_IB = (1 << 16),
    CCR_FI = (1 << 21),
    CCR_FD = (1 << 22),
    CCR_DS = (1 << 23)
  };

enum sparc_regs : unsigned
{
  REG_SPARC_G0 = 0,
  REG_SPARC_O0 = 8,
  REG_SPARC_O1 = 9,
  REG_SPARC_O2 = 10
};
#endif /* DSU_H */
