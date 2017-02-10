#include "soc.h"
#include "dsu.h"
#include "menu.h"

class managed_spim : public managed_device
{
 public:
  managed_spim (ahb_device *dev) : dev (dev), link (nullptr), base (0)
  {
    link = dev->get_parent ()->get_link ();
    base = bar_to_base (dev->get_pnp ().bar[0],
			dev->get_parent ()->base);
  }
  virtual void reset (void) { }

  void cmd_spim_regs(void);
  void cmd_spim_id (void);
  void cmd_spim_status (void);
  void cmd_spim_sig (void);
  void cmd_spim_wren (void);
  void cmd_spim_wrdi (void);
  void cmd_spim_raw_write (menu_item_arg &args);
  void cmd_spim_se (menu_item_arg &args);
  void cmd_spim_erase (void);
  void cmd_spim_write (dsu *adsu, menu_item_arg &args);
private:
  word read_reg (word reg) { return link->read_word (base + reg); }
  void write_reg (word reg, word val) { link->write_word (base + reg, val); }

  bool spim_start (void);
  word spim_rxtx (word cmd);
  void spim_space (void);
  bool enable_write (void);
  void wait_end_of_write (void);
  
  ahb_device *dev;
  dsu_link *link;
  word base;
};

