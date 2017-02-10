/* This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <list>
#include <iostream>
#include <fstream>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "lemon.h"
#include "links.h"
#include "devices.h"
#include "menu.h"
#include "soc.h"
#include "dsu.h"
#include "outputs.h"
#include "parse.h"
#include "loader.h"
#include "sparc.h"
#include "osdep.h"
#include "breakpoint.h"
#include "spim.h"

using namespace std;

// The main board.
static soc *board;

static bool quit = false;
static bool flag_forward = true;

word
bar_to_base (word bar, word bus_base)
{
  word addr = bar >> 20;

  switch (bar_to_typ (bar))
    {
    case BAR_AHB_MEM:
      return addr << 20;
    case BAR_AHB_IO:
    case BAR_APB_IO:
      return bus_base + (addr << 8);
    default:
      return bad_base;
    }
}

static void
cmd_devices (bool all)
{
  if (0)
    board->disp_devices ();
  else
    for (auto d: board->get_devices ())
      {
	bool en = all;

	//  APB devices are slaves.
	if (!en && dynamic_cast<apb_device*>(d) != nullptr)
	  en = true;
	//  AHB with a BAR are displayed.
	if (!en)
	  {
	    ahb_device *ad = dynamic_cast<ahb_device*>(d);
	    if (ad != nullptr)
	      {
		if (ad->get_pnp ().bar[0] != 0)
		  en = true;
	      }
	  }
	if (en)
	  d->disp_device ("");
      }
}

static dsu *board_dsu;

static void
cmd_greth_regs (apb_device *dev)
{
  word base = bar_to_base (dev->get_pnp ().bar,
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  dev->disp_device ("");
  cout << " control: " << hex8(link->read_word (base + 0x00)) << endl;
  cout << " status : " << hex8(link->read_word (base + 0x04)) << endl;
  cout << " MAC    : " << hex8(link->read_word (base + 0x08)) << endl;
  cout << " MAC    : " << hex8(link->read_word (base + 0x0c)) << endl;
  cout << " MDIO   : " << hex8(link->read_word (base + 0x10)) << endl;
  cout << " TxDesc : " << hex8(link->read_word (base + 0x14)) << endl;
  cout << " RxDesc : " << hex8(link->read_word (base + 0x18)) << endl;
  cout << " EDCL IP: " << hex8(link->read_word (base + 0x1c)) << endl;
  cout << " Hash   : " << hex8(link->read_word (base + 0x20)) << endl;
  cout << " Hash   : " << hex8(link->read_word (base + 0x24)) << endl;
}

static void
cmd_greth_edcl (apb_device *dev, menu_item_arg &args)
{
  word base = bar_to_base (dev->get_pnp ().bar,
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  word ctr = link->read_word (base + 0);

  //  Skip if EDCL not available.
  if (!(ctr >> 31))
    {
      cout << "No edcl on device" << endl;
      return;
    }

  cmd_arg_ipaddr *arg = dynamic_cast<cmd_arg_ipaddr *>(args.get_arg (0));

  if (arg->present)
    {
      unsigned int addr = unpack_be32 (arg->addr.a);
      link->write_word (base + 0x1c, addr);
    }
  else
    {
      cout << "edcl: ";
      cout << "buf size: " << (1 << ((ctr >> 28) & 7)) << " KB";
      cout << ", " << (((ctr >> 14) & 1) ? "disabled" : "enabled");
      cout << endl;

      word mac = link->read_word (base + 0x28);
      cout << " MAC: " << hex2 ((mac >> 8) & 0xff) << ":";
      cout << hex2 ((mac >> 0) & 0xff) << ":";
      mac = link->read_word (base + 0x2c);
      cout << hex2 ((mac >> 24) & 0xff) << ":";
      cout << hex2 ((mac >> 16) & 0xff) << ":";
      cout << hex2 ((mac >> 8) & 0xff) << ":";
      cout << hex2 ((mac >> 0) & 0xff) << endl;

      word ip = link->read_word (base + 0x1c);
      cout << " IP: " << dec ((ip >> 24) & 0xff) << ".";
      cout << dec ((ip >> 16) & 0xff) << ".";
      cout << dec ((ip >> 8) & 0xff) << ".";
      cout << dec ((ip >> 0) & 0xff);
      cout << endl;
    }
}

static reg_desc gptimer_conf_desc
("gptimer_conf",
 {
   new fdesc ("EV", 13),
   new fdesc ("ES", 12),
   new fdesc ("EL", 11),
   new fdesc ("EE", 10),
   new fdesc ("DF", 9),
   new fdesc ("SI", 8),
   new fdesc ("IRQ", 3, 5),
   new fdesc ("TIMERS", 0, 3)
 });

static reg_desc gptimer_control_desc
("gptimer_ctrl",
 {
   new fdesc ("WS", 8),
   new fdesc ("WN", 7),
   new fdesc ("DH", 6),
   new fdesc ("CH", 5),
   new fdesc ("IP", 4),
   new fdesc ("IE", 3),
   new fdesc ("LD", 2),
   new fdesc ("RS", 1),
   new fdesc ("EN", 0)
 });

static void
cmd_gptimer_regs (apb_device *dev)
{
  word base = bar_to_base (dev->get_pnp ().bar,
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  dev->disp_device ("");
  word config = link->read_word (base + 0x08);
  cout << "scaler value:  " << hex8(link->read_word (base + 0x00)) << endl;
  cout << "scaler reload: " << hex8(link->read_word (base + 0x04)) << endl;
  cout << "configuration: " << hex8(config);
  gptimer_conf_desc.disp (cout, "  ", config);

  cout << "timer latch:   " << hex8(link->read_word (base + 0x0c)) << endl;
  for (unsigned int i = 0; i < (config & 7); i++)
    {
      word baddr = base + (i << 4);
      cout << "timer" << i << " counter: "
	   << hex8(link->read_word (baddr + 0x0)) << endl;
      cout << "timer" << i << " reload:  "
	   << hex8(link->read_word (baddr + 0x4)) << endl;
      word ctrl = link->read_word (baddr + 0x8);
      cout << "timer" << i << " control: " << hex8(ctrl);
      gptimer_control_desc.disp (cout, "  ", ctrl);
      cout << "timer" << i << " latch:   "
	   << hex8(link->read_word (baddr + 0xc)) << endl;
    }
}

static void
cmd_gptimer_reset (apb_device *dev)
{
  word base = bar_to_base (dev->get_pnp ().bar,
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  /* Read config, as we need to know the last timer.  */
  word config = link->read_word (base + 0x08);
  /* Address of the last timer.  */
  word ltimer = base + (((config & 7) - 1) << 4);
  /* Set reload to 1.  */
  link->write_word (ltimer + 0x4, 10000000);
  /* Enable timer, reload, don't disable watchdog.  */
  link->write_word (ltimer + 0x8, 0x105);
  if (link->read_word (ltimer + 0x8) & 0x100)
    cout << "watchdog not implemented" << endl;
  link->write_word (ltimer + 0x8, 0x005);

  /* Disable freeze.  */
  // link->write_word (base + 0x8, 0x200);

  /* What to do ?  Should we wait ?  */
}

class managed_irqmp : public managed_device
{
 public:
  managed_irqmp (apb_device *dev) : dev (dev), base (0)
  {
    base = bar_to_base (dev->get_pnp ().bar,
			dev->get_parent ()->base);
  }
  virtual void reset (void);
  void disp_regs (void);
private:
  apb_device *dev;
  word base;
};

void
managed_irqmp::reset (void)
{
  dsu_link *link = dev->get_parent ()->get_link ();

  word status = link->read_word (base + 0x10);
  int nbr_proc = (status >> 28) + 1;
  for (int i = 0; i < nbr_proc; i++)
    {
      //  Set force to 0.
      link->write_word (base + 0x80 + 4 * i, 0);
      //  Set mask to 0.
      link->write_word (base + 0x40 + 4 * i, 0);
    }
  //  Clear all pending interrupts.
  link->write_word (base + 0x0c, ~(word)0);

  //  Be sure cpu#0 is powered
  link->write_word (base + 0x10, 1);
}

void
managed_irqmp::disp_regs (void)
{
  word base = bar_to_base (dev->get_pnp ().bar,
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  dev->disp_device ("");
  cout << "level:   " << hex8(link->read_word (base + 0x00)) << endl;
  cout << "pending: " << hex8(link->read_word (base + 0x04)) << endl;
  cout << "clear:   " << hex8(link->read_word (base + 0x0c)) << endl;
  word status = link->read_word (base + 0x10);
  cout << "status:  " << hex8(status) << endl;
  int nbr_proc = (status >> 28) + 1;
  for (int i = 0; i < nbr_proc; i++)
    {
      cout << "cpu#" << i << " mask:  "
	   << hex8(link->read_word (base + 0x40 + 4 * i)) << endl;
      cout << "cpu#" << i << " force: "
	   << hex8(link->read_word (base + 0x80 + 4 * i)) << endl;
      cout << "cpu#" << i << " ack:   "
	   << hex8(link->read_word (base + 0xc0 + 4 * i)) << endl;
    }
}

static void
dump_memory (int sz, menu_item_arg &args)
{
  cmd_arg_expr *arg0 = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));
  cmd_arg_expr *arg1 = dynamic_cast<cmd_arg_expr *>(args.get_arg (1));

  word addr = arg0->value;
  word len;

  if (arg1->present)
    len = arg1->value;
  else
    len = 64;

  while (len > 0)
    {
      word l = len > 16 ? 16 : len;
      unsigned char buf[16];

      cout << hex8 << addr << " ";
      if (!board->get_link ()->read (addr, (l + 3) / 4, buf))
	{
	  cout << "link error" << endl;
	  break;
	}
      if (sz == 1)
	{
	  for (word i = 0; i < l; i++)
	    cout << " " << hex2 << (buf[i] & 0xff);
	}
      else
	{
	  for (word i = 0; i < l; i += 4)
	    cout << " " << hex8 << unpack_be32 (buf + i);
	}

      for (word i = l; i < 16; i++)
	cout << "   ";
      cout << "  ";
      for (word i = 0; i < l; i++)
	{
	  unsigned char c = buf[i];
	  cout << char ((c >= 0x20 && c <= 0x7e) ? c : '.');
	}
      cout << endl;

      addr += l;
      len -= l;
    }
}

static void
cmd_wmem (menu_item_arg &args)
{
  cmd_arg_expr *arg0 = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));
  cmd_arg_expr *arg1 = dynamic_cast<cmd_arg_expr *>(args.get_arg (1));

  word addr = arg0->value;
  word val = arg1->value;

  cout << "write " << hex8 << val << " at " << hex8 << addr << endl;

  board->get_link ()->write_word (addr, val);
}

static void
cmd_disa (menu_item_arg &args)
{
  cmd_arg_expr *arg0 = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));
  cmd_arg_expr *arg1 = dynamic_cast<cmd_arg_expr *>(args.get_arg (1));

  word addr = arg0->value;
  word len;

  if (arg1->present)
    len = arg1->value;
  else
    len = 64;

  for (; len != 0; len--)
    {
      word insn = board->get_link ()->read_word (addr);
      cout << hex8 (addr) << ": " << hex8 (insn) << "  ";
      cout << disa_sparc (addr, insn) << endl;
      addr += 4;
    }
}

static void
cmd_load (menu_item_arg &args)
{
  cmd_arg_file *arg = dynamic_cast<cmd_arg_file *>(args.get_arg (0));
  load_elf (board_dsu, arg->filename.c_str (), true);
}

static void
cmd_loadsym (menu_item_arg &args)
{
  cmd_arg_file *arg = dynamic_cast<cmd_arg_file *>(args.get_arg (0));

  load_elf (nullptr, arg->filename.c_str (), false);
}

class managed_ddr2spa : public managed_device
{
 public:
  managed_ddr2spa (ahb_device *dev) : dev (dev), base (0)
  {
    base = bar_to_base (dev->get_pnp ().bar[1],
			dev->get_parent ()->base);
  }
  virtual void reset (void);
private:
  ahb_device *dev;
  word base;
};

void
managed_ddr2spa::reset (void)
{
  dsu_link *link = dev->get_parent ()->get_link ();

  //  Values from dbinit2.c for leon4 itx.
  link->write_word (base + 0x0, 0x96a08616);
  link->write_word (base + 0x8, 0x13650000);
  link->write_word (base + 0xc, 0x0000017f);
}

class managed_ddrspa : public managed_device
{
 public:
  managed_ddrspa (ahb_device *dev) : dev (dev), base (0)
  {
    base = bar_to_base (dev->get_pnp ().bar[1],
			dev->get_parent ()->base);
  }
  virtual void reset (void);
private:
  ahb_device *dev;
  word base;
};

void
managed_ddrspa::reset (void)
{
  dsu_link *link = dev->get_parent ()->get_link ();

  cout << "init ddrspa at " << hex8 (base) << endl;
  //  Values from prom.h for leon3-diligent-xc3s1600e.  */
  word w = link->read_word (base + 0x0);
  w = (w & 0x7c1fffff) | 0x01010000;
  link->write_word (base + 0x0, w); // 0xe6a06e60);
}

static reg_desc ddr2cfg1_desc
("ddr2cfg1",
 {
   new fdesc ("REFEn", 31),
     new fdesc ("OCD", 30),
     new fdesc ("EMR", 28, 2),
     new fdesc ("BS3", 27),
     new fdesc ("TRCD", 26),
     new fdesc ("BanSZ", 23, 3),
     new fdesc ("ColSZ", 21, 2),
     new fdesc ("Cmd", 18, 3),
     new fdesc ("PR", 17),
     new fdesc ("IN", 16),
     new fdesc ("CE", 15),
     new fdesc ("Refresh", 0, 15)
     });

static reg_desc ddr2cfg2_desc
("ddr2cfg2",
 {
   new fdesc ("PHYTech", 18, 8),
     new fdesc ("BIG", 17),
     new fdesc ("FTV", 16),
     new fdesc ("REG5", 15),
     new fdesc ("Width", 12, 3),
     new fdesc ("Freq", 0, 12)
     });

static reg_desc ddr2cfg3_desc
("ddr2cfg3",
 {
   new fdesc ("PLL", 29, 2),
     new fdesc ("TRP", 28),
     new fdesc ("tWR", 23, 5),
     new fdesc ("TRFC", 18, 5),
     new fdesc ("RD", 16, 2),
     new fdesc ("inc/dec delay", 8, 8),
     new fdesc ("update delay", 0, 8)
     });

static void
cmd_ddr2spa_regs (ahb_device *dev)
{
  word base = bar_to_base (dev->get_pnp ().bar[1],
			   dev->get_parent ()->base);
  dsu_link *link = dev->get_parent ()->get_link ();

  word cfg1 = link->read_word (base + 0x0);
  cout << "DDR2CFG1: " << hex8 (cfg1) << endl;
  ddr2cfg1_desc.disp (cout, "  ", cfg1);

  word cfg2 = link->read_word (base + 0x4);
  cout << "DDR2CFG2: " << hex8 (cfg2) << endl;
  ddr2cfg2_desc.disp (cout, "  ", cfg2);

  word cfg3 = link->read_word (base + 0x8);
  cout << "DDR2CFG3: " << hex8 (cfg3) << endl;
  ddr2cfg3_desc.disp (cout, "  ", cfg3);

  word cfg4 = link->read_word (base + 0xc);
  cout << "DDR2CFG4: " << hex8 << cfg4 << endl;
  cout << " " << (((cfg4 >> 8) & 1) ? "8" : "4") << " banks";
  cout << ", dqs offset: " << ((cfg4 >> 0) & 0xff) << endl;
}

class managed_uart : public managed_device, public forwarder
{
 public:
  managed_uart (apb_device *dev, string &nam)
  {
    base = bar_to_base (dev->get_pnp ().bar,
			dev->get_parent ()->base);
    link = dev->get_parent ()->get_link ();
    name = nam;
    poll_enabled = false;
    register_poll (this);
  }
  virtual void reset (void);
  virtual bool poll (void);
  void forward (void);
  void disp_regs (void);
private:
  dsu_link *link;
  word base;
  string name;
};

void
managed_uart::reset (void)
{
  link->write_word (base + 0x0c, 0x6c); // 115200 at 100Mhz
  link->write_word (base + 0x08, 3); // Enable

  //  Enable forwarding.
  if (flag_forward)
    this->forward ();
}

bool
managed_uart::poll (void)
{
  bool res = false;

  // Limit the loop.
  for (int i = 0; i < 16; i++)
    {
      word status = link->read_word (base + 0x04);
      // printf ("uart : %08x at %08x\n", status, base);
      if ((status & (1 << 2)))
	break;

      //  Fifo is not empty
      word c = link->read_word (base + 0x10);
      // printf (" word: %08x\n", c);
      cout << char (c) << flush;
      res = true;
    }
  return res;
}

void
managed_uart::disp_regs (void)
{
  cout << "Uart at " << hex8 (base) << endl;

  word status = link->read_word (base + 0x04);
  cout << "  Status:  " << hex8 (status) << endl;
  word control = link->read_word (base + 0x08);
  cout << "  Control: " << hex8 (control) << endl;
}

void
managed_uart::forward (void)
{
  cout << name << ": forward enabled" << endl;
  link->write_word (base + 0x08, (1 << 11) | 3); // Enable, debug
  this->poll_enabled = true;
}

static bool auto_regs = true;

static void
cmd_si (Cpu *cpu, menu_item_arg &args)
{
  menu_repeat ();

  cpu->step ();
  if (auto_regs)
    cpu->disp_regs ();
}

static void
cmd_go (menu_item_arg &args)
{
  cmd_arg_expr *arg0 = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));

  if (arg0->present)
    board_dsu->set_entry (arg0->value);
  else
    menu_repeat ();

  board_dsu->go ();
}

static void
cmd_hwatch (Cpu &cpu, menu_item_arg &args)
{
  cmd_arg_expr *arg = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));

  if (arg->present)
    hwatch_create (cpu, arg->value);
  else
    cpu.hwatch_disp ();
}

static void
cmd_ihist (Cpu &cpu, menu_item_arg &args)
{
  cmd_arg_expr *arg = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));

  if (arg->present)
    cpu.disp_itrace (arg->value);
  else
    cpu.disp_itrace (16);
}

static void
cmd_ahb (menu_item_arg &args)
{
  cmd_arg_bool *arg = dynamic_cast<cmd_arg_bool *>(args.get_arg (0));

  if (arg->present)
    board_dsu->ahb_set (arg->value);
  //  else if (parse_eq (arg, "+master"))
  //    board_dsu->ahb_mask (true, true, parse_expr (arg));
  //  else if (parse_eq (arg, "-master"))
  //    board_dsu->ahb_mask (false, true, parse_expr (arg));
  else
    {
      word cnt = 16;
      //if (!end_of_arg (arg))
      //   cnt = parse_expr (arg);
      //check_no_arg (arg);
      board_dsu->ahb_traces (cnt);
    }
}

static void
cmd_tracecom (menu_item_arg &args)
{
  cmd_arg_bool *arg = dynamic_cast<cmd_arg_bool *>(args.get_arg (0));

  if (arg->present)
    trace_com = arg->value;
  else
    cout << "trace com is " << trace_com << endl;
}

static void
cmd_sbreak (menu_item_arg &args)
{
  cmd_arg_expr *arg0 = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));

  if (arg0->present)
    {
      breakpoint *bp = sbreak_create (*board_dsu, arg0->value);
      cout << "Breakpoint #" << bp->id () << " added" << endl;
    }
  else
    {
      std::list<breakpoint *> bps = get_all_breakpoints ();

      if (bps.size() == 0)
	cout << "No breakpoints" << endl;
      else
	for (auto bp: bps)
	  {
	    cout << "#" << bp->id () << " at 0x" << hex8 (bp->get_addr ());
	    cout << (bp->enabled () ? " enabled" : " disabled") << endl;
	  }
    }
}

static void
create_menu_devices (menu_item_submenu *parent)
{
  unsigned int num_apbuart = 0;
  unsigned int num_greth = 0;
  unsigned int num_ddr2spa = 0;
  unsigned int num_ddrspa = 0;
  unsigned int num_gptimer = 0;
  unsigned int num_irqmp = 0;
  unsigned int num_spim = 0;

  for (auto d: board->get_devices ())
    {
      unsigned int id = d->get_id ();

      if (id_to_vid (id) == VENDOR_GAISLER)
	{
	  switch (id_to_did (id))
	    {
	    case DEVICE_APBUART:
	      {
		string name = "uart" + dec (num_apbuart++);
		apb_device *ad = dynamic_cast<apb_device *>(d);

		auto m = new managed_uart (ad, name);
		if (num_apbuart == 1)
		  board->add_managed_device (m);

		parent->add
		  (new menu_item_submenu
		   (strdup (name.c_str ()), "disp uart registers",
		    {
		      new menu_item_arg
			("reset", "reset uart",
			 { },
			 [m](menu_item_arg &args) { m->reset (); }),
		      new menu_item_arg
			("forward", "forward uart",
			 { },
			 [m](menu_item_arg &args) { m->forward (); })
			}, [m](void) { m->disp_regs (); }));
	      }
	      break;
	    case DEVICE_GRETH:
	      {
		apb_device *ad = dynamic_cast<apb_device *>(d);
		//  There is both an AHB master and an APB slave for a greth
		//  (as it does DMA).  Consider only APB.
		if (ad != nullptr)
		  {
		    string name = "greth" + dec (num_greth++);
		    parent->add
		      (new menu_item_submenu
		       (strdup (name.c_str ()), "disp greth registers",
			{ new menu_item_arg
			    ("edcl", "display or set edcl",
			     { new cmd_arg_ipaddr
				 ("ipaddr", true, "IP addre")
			     }, [ad](menu_item_arg &args)
			     {cmd_greth_edcl (ad, args); })
			}, [ad](void) { cmd_greth_regs (ad); }));
		  }
	      }
	      break;
	    case DEVICE_DDR2SPA:
	      {
		string name = "ddr2spa" + dec (num_ddr2spa++);
		ahb_device *ad = dynamic_cast<ahb_device *>(d);

		auto m = new managed_ddr2spa (ad);
		board->add_managed_device (m);

		parent->add
		  (new menu_item_submenu
		   (strdup (name.c_str ()), "disp ddr2spa registers",
		    {
		      new menu_item_arg
			("reset", "initialize ddr2spa",
			 { },
			 [m](menu_item_arg &args) { m->reset (); })
			}, [ad](void) { cmd_ddr2spa_regs (ad); }));
	      }
	      break;
	    case DEVICE_DDRSPA:
	      {
		string name = "ddrspa" + dec (num_ddrspa++);
		ahb_device *ad = dynamic_cast<ahb_device *>(d);

		auto m = new managed_ddrspa (ad);
		board->add_managed_device (m);
	      }
	      break;
	    case DEVICE_GPTIMER:
	      {
		apb_device *ad = dynamic_cast<apb_device *>(d);
		if (ad != nullptr)
		  {
		    string name = "gptimer" + dec (num_gptimer++);
		    parent->add
		      (new menu_item_submenu
		       (strdup (name.c_str ()), "disp gptimer registers",
			{
			  new menu_item_arg
			    ("reset", "initialize ddr2spa",
			     { },
			     [ad](menu_item_arg &args)
			     { cmd_gptimer_reset (ad);})
			}, [ad](void) { cmd_gptimer_regs (ad); }));
		  }
	      }
	      break;
	    case DEVICE_IRQMP:
	      {
		apb_device *ad = dynamic_cast<apb_device *>(d);
		if (ad != nullptr)
		  {
		    string name = "irqmp" + dec (num_irqmp++);
		    auto m = new managed_irqmp (ad);
		    board->add_managed_device (m);
		    parent->add
		      (new menu_item_submenu
		       (strdup (name.c_str ()), "disp irqmp registers",
			{
			  new menu_item_arg
			    ("reset", "initialize irqmp",
			     { },
			     [m](menu_item_arg &args) { m->reset ();})
			},
			[m](void) { m->disp_regs ();}));
		  }
	      }
	      break;
	    case DEVICE_SPIM:
	      {
		ahb_device *ad = dynamic_cast<ahb_device *>(d);
		if (ad != nullptr)
		  {
		    auto m = new managed_spim (ad);
		    string name = "spim" + dec (num_spim++);
		    parent->add
		      (new menu_item_submenu
		       (strdup (name.c_str ()), "disp spim registers",
			{
			  new menu_item_arg
			    ("id", "read flash id",
			     { },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_id ();}),
			  new menu_item_arg
			    ("status", "read flash status",
			     { },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_status ();}),
			  new menu_item_arg
			    ("sig", "read flash sig",
			     { },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_sig ();}),
			  new menu_item_arg
			    ("wren", "flash write enable",
			     { },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_wren ();}),
			  new menu_item_arg
			    ("wrdi", "flash write disable",
			     { },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_wrdi ();}),
			  new menu_item_arg
			    ("rawwrite", "flash raw write (slow)",
			     { new cmd_arg_file ("file", false, "filename") },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_raw_write (args);}),
			  new menu_item_arg
			    ("write", "flash write",
			     { new cmd_arg_file ("file", false, "filename") },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_write (board_dsu, args);}),
			  new menu_item_arg
			    ("se", "sector erase",
			     { new cmd_arg_expr ("sector", false, "sector") },
			     [m](menu_item_arg &args)
			     { m->cmd_spim_se (args);}),
			  new menu_item_arg
			    ("erase", "erase flash",
			     { },
			     [m](menu_item_arg &args) { m->cmd_spim_erase ();})
			}, [m](void) { m->cmd_spim_regs (); }));
		  }
	      }
	      break;
	    }
	}
    }
}

static void
cmd_reset (void)
{
  for (auto m: board->get_managed_devices ())
    m->reset ();
  board_dsu->reset ();
}

static void
cpu_add_menu (menu_item_submenu *m, Cpu *&cpu)
{
  m->add
    (new menu_item_arg
     ("si", "single step", { },
      [&cpu](menu_item_arg &args) { cmd_si(cpu, args); }));
  m->add
    (new menu_item_arg
     ("regs", "display registers", { },
      [&cpu](menu_item_arg &args) { cpu->disp_regs (); }));
  m->add
    (new menu_item_arg
     ("bt", "display backtrace", { },
      [&cpu](menu_item_arg &args) { cpu->disp_bt (); }));
  m->add
    (new menu_item_submenu
     ("itrace", "control insn trace",
      {
	new menu_item_arg
	  ("enable", "enable itrace", { },
	   [&cpu](menu_item_arg &args) { cpu->set_itrace (true); }),
	new menu_item_arg
	  ("disable", "disable itrace", { },
	   [&cpu](menu_item_arg &args) { cpu->set_itrace (false); }),
	  },
      [&cpu](void) { cpu->disp_itrace (16); }));
  m->add
    (new menu_item_arg
     ("ihist", "display insn trace",
      {
	new cmd_arg_expr ("len", true, "number of entries")
      },
      [&cpu](menu_item_arg &args) { cmd_ihist (*cpu, args); }));
  m->add
    (new menu_item_arg
     ("hwatch", "display hardware watchpoint",
      {
	new cmd_arg_expr ("addr", true, "breakpoint address")
      },
      [&cpu](menu_item_arg &args) { cmd_hwatch (*cpu, args); }));
  m->add
    (new menu_item_submenu
     ("cache", "display cache config",
      {
	new menu_item_arg
	  ("enable", "enable cache", { },
	   [&cpu](menu_item_arg &args) { cpu->set_cache (true); }),
	new menu_item_arg
	  ("disable", "disable cache", { },
	   [&cpu](menu_item_arg &args) { cpu->set_cache (false); }),
	new menu_item_arg
	  ("idump", "dump I cache", { },
	   [&cpu](menu_item_arg &args) { cpu->dump_cache_content (true); }),
	new menu_item_arg
	  ("ddump", "dump D cache", { },
	   [&cpu](menu_item_arg &args) { cpu->dump_cache_content (false); }),
	new menu_item_arg
	  ("iflush", "flush I cache", { },
	   [&cpu](menu_item_arg &args) { cpu->cache_flush (true); }),
	new menu_item_arg
	  ("dflush", "flush D cache", { },
	   [&cpu](menu_item_arg &args) { cpu->cache_flush (false); })
	},
      [&cpu](void) { cpu->disp_cache_config (); }));
}

int
main (int argc, char **argv)
{
  dsu_link *link;
  Cpu *cur_cpu = nullptr;
  const char *eth = nullptr;
  const char *jtag = nullptr;
  const char *remote = nullptr;
  bool usb = false;
  bool flag_reset = true;
  bool flag_probe = true;

  //  List of commands (from command line) to execute.
  list<string> init_cmds;

  for (int i = 1; i < argc; i++)
    if (strcmp (argv[i], "--eth") == 0)
      {
	i++;
	if (i >= argc)
	  {
	    cerr << "missing argument after --eth" << endl;
	    return 1;
	  }
	eth = argv[i];
      }
    else if (strcmp (argv[i], "--usb") == 0)
      usb = true;
    else if (strcmp (argv[i], "--jtag") == 0)
      {
	i++;
	if (i >= argc)
	  {
	    cerr << "missing argument after --jtag" << endl;
	    return 1;
	  }
	jtag = argv[i];
#ifndef HAVE_LIBURJTAG
	cerr << "-jtag not available (not compiled with urjtag)" << endl;
	return 2;
#endif

      }
    else if (strcmp (argv[i], "--remote") == 0)
      {
	i++;
	if (i >= argc)
	  {
	    cerr << "missing argument after --remote" << endl;
	    return 1;
	  }
	remote = argv[i];
      }
    else if (strcmp (argv[i], "-i") == 0)
      {
	i++;
	if (i >= argc)
	  {
	    cerr << "missing command after -i" << endl;
	    return 1;
	  }
	init_cmds.push_back (argv[i]);
      }
    else if (strcmp (argv[i], "--no-reset") == 0)
      flag_reset = false;
    else if (strcmp (argv[i], "--no-probe") == 0)
      flag_probe = false;
    else if (strcmp (argv[i], "--trace-com") == 0)
      trace_com = true;
    else if (strcmp (argv[i], "--no-forward") == 0)
      flag_forward = false;
    else
      {
	cerr << "unknown option '" << argv[i] << "'" << endl;
	return 1;
      }

  if (((eth != nullptr)
       + (usb ? 1 : 0)
       + (jtag != nullptr)
       + (remote != nullptr))
      > 1)
    {
      cerr << "more than one interface selected" << endl;
      return 1;
    }

  //  Create the link to the board.
  if (eth != nullptr)
    link = create_eth_dsu_link (eth);
#ifdef HAVE_LIBURJTAG
  else if (jtag != nullptr)
    link = new jtag_dsu_link (jtag);
#endif
  else if (remote != nullptr)
    link = new remote_dsu_link (remote);
  else
    link = new usb_dsu_link;

  //  Connect to the board.
  if (!link->open ())
    {
      cerr << "failed to create link" << endl;
      return 1;
    }

  //  Create the root.
  board = new soc (link, 0xfff00000);

  //  Probe all buses and all devices on the board.
  if (flag_probe)
    {
      word w = link->read_word (0xfffffff0);
      cout << "Board id: " << hex4 (w >> 16);
      cout << ", Build id: " << hex4 (w & 0xffff) << endl;

      board->probe ();
    }

  menu_item_submenu *main_menu = new menu_item_submenu
    ("menu", "lemon menu", { }, [](void) { });
  main_menu->add
    (new menu_item_arg
     ("help", "display help", { },
      [main_menu](menu_item_arg &args) { main_menu->execute_help (); }));
  main_menu->add
    (new menu_item_submenu
     ("devices", "list devices and bus",
      {
	new menu_item_arg
	  ("-all", "list all devices", { },
	   [](menu_item_arg &args) { cmd_devices (true); })
      },
      [](void) { cmd_devices (false); }));
  main_menu->add
    (new menu_item_arg
     ("quit", "quit monitor", { },
      [](menu_item_arg &args) { quit = true; }));
  main_menu->add
    (new menu_item_arg
     ("mem", "dump memory (by word)",
      {
	new cmd_arg_expr ("addr", false, "start address"),
	new cmd_arg_expr ("length", true, "number of bytes")
      },
      [](menu_item_arg &args) { dump_memory (4, args); }));
  main_menu->add
    (new menu_item_arg
     ("memb", "dump memory (by bytes)",
      {
	new cmd_arg_expr ("addr", false, "start address"),
	new cmd_arg_expr ("length", true, "number of bytes")
      },
      [](menu_item_arg &args) { dump_memory (1, args); }));
  main_menu->add
    (new menu_item_arg
     ("wmem", "write memory",
      {
	new cmd_arg_expr ("addr", false, "address"),
	new cmd_arg_expr ("value", false, "value (word)")
      },
      [](menu_item_arg &args) { cmd_wmem (args); }));
  main_menu->add
    (new menu_item_arg
     ("disa", "disassemble memory",
      {
	new cmd_arg_expr ("addr", false, "start address"),
	new cmd_arg_expr ("length", true, "number of insns")
      },
      [](menu_item_arg &args) { cmd_disa (args); }));
  main_menu->add
    (new menu_item_arg
     ("sbreak", "soft breakpoints",
      {
	new cmd_arg_expr ("addr", true, "breakpoint address")
      },
      [](menu_item_arg &args) { cmd_sbreak (args); }));
  main_menu->add
    (new menu_item_arg
     ("tracecom", "trace communication with the board",
      { new cmd_arg_bool ("enable", true, "enable/disable com traces") },
      cmd_tracecom));

  create_menu_devices (main_menu);

  board_dsu = find_dsu (board);

  if (board_dsu == nullptr)
    cerr << "warning: no dsu on the board" << endl;
  else
    {
      board_dsu->init (flag_reset);

      cur_cpu = board_dsu->get_cpus().front ();
      cout << "dsu: " << board_dsu->get_ncpus () << " cpus" << endl;
      main_menu->add
	(new menu_item_arg
	 ("dsu", "disp dsu registers", { },
	  [](menu_item_arg &args) { board_dsu->disp_info (); }));
      main_menu->add
	(new menu_item_arg
	 ("load", "load an elf file",
	  { new cmd_arg_file ("file", false, "filename to load") },
	  cmd_load));
      main_menu->add
	(new menu_item_arg
	 ("loadsym", "load symbols from elf file",
	  { new cmd_arg_file ("file", false, "filename to load") },
	  cmd_loadsym));
      main_menu->add
	(new menu_item_arg
	 ("sym", "display symbols",
	  { },
	  [](menu_item_arg &args) { disp_symbols (); }));
      main_menu->add
	(new menu_item_arg
	 ("go", "resume execution",
	  {
	    new cmd_arg_expr ("addr", true, "breakpoint address")
	  }, cmd_go));
      main_menu->add
	(new menu_item_arg
	 ("stop", "force stop execution", { },
	  [](menu_item_arg &args) { board_dsu->stop (); }));
      main_menu->add
	(new menu_item_arg
	 ("ahb", "display or set ahb trace",
	  { new cmd_arg_bool ("enable", true, "enable/disable ahb traces") },
	  cmd_ahb));
      main_menu->add
	(new menu_item_arg
	 ("reset", "reset the board", { },
	  [](menu_item_arg &args) { cmd_reset (); }));
      cpu_add_menu (main_menu, cur_cpu);

      int num_cpu = 0;
      for (auto c : board_dsu->get_cpus())
	{
	  string name = "cpu" + dec (num_cpu++);
	  menu_item_submenu *subm;

	  subm = new menu_item_submenu
	    (strdup (name.c_str()), "select cpu", { },
	     [c, &cur_cpu](void) { cur_cpu = c; });
      	  main_menu->add (subm);

	  cpu_add_menu (subm, c);
	}
    }

  if (flag_reset)
    cmd_reset ();

  if (init_cmds.size () > 0)
    {
      for (auto c: init_cmds)
	try
	  {
	    main_menu->execute (c);
	  }
	catch (parse_error &e)
	{
	  cerr << "error: " << e.get_msg () << endl;
	  return 1;
	}
      return 0;
    }

  //  Init readline.
  rl_initialize ();
  using_history ();
  read_history (NULL);

  //  Install C-c handler
  install_handler ();

  string prev;
  while (1)
    {
      char *l;
      string prompt = cur_cpu->get_name() + "> ";

      l = readline (prompt.c_str());
      if (l == NULL)
	break;

      try
	{
	  user_stop = 0;
	  if (*l != 0)
	    {
	      //  Non blank line.
	      add_history (l);

	      string ls = l;
	      main_menu->execute (ls);
	      if (menu_repeat_p ())
		prev = ls;
	    }
	  else if (!prev.empty ())
	    main_menu->execute (prev);
	}
      catch (link_error &e)
	{
	  cerr << "DSU error at " << hex8 << e.addr << endl;
	  cerr << "command aborted" << endl;
	}
      catch (parse_error &e)
	{
	  cerr << "error: " << e.get_msg () << endl;
	  cerr << "try help" << endl;
	}

      free (l);

      if (quit)
	break;
    }

  if (board_dsu != nullptr)
    board_dsu->release ();

  link->close ();

  write_history (NULL);
  return 0;
}
