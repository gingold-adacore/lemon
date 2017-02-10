#include <iostream>
#include <fstream>
#include "spim.h"
#include "outputs.h"
#include "osdep.h"
#include "loader.h"

using namespace std;

enum spim_reg
{
  SPIM_CONFIG = 0,
  SPIM_CONTROL = 4,
  SPIM_STATUS = 8,
  SPIM_RX = 0x0c,
  SPIM_TX = 0x10
};

void
managed_spim::cmd_spim_regs (void)
{
  dev->disp_device ("");

  cout << "config:  " << hex8(read_reg (SPIM_CONFIG)) << endl;
  cout << "control: " << hex8(read_reg (SPIM_CONTROL)) << endl;
  cout << "status:  " << hex8(read_reg (SPIM_STATUS)) << endl;
}

enum spim_status
{
  SPIM_DONE = (1 << 0),
  SPIM_BUSY = (1 << 1),
  SPIM_INIT = (1 << 2),
  SPIM_ERR  = (1 << 2)
};

enum spim_control
{
  SPIM_USRC = (1 << 0),
  SPIM_IEN  = (1 << 1),
  SPIM_EAS  = (1 << 2),
  SPIM_CSN  = (1 << 3),
  SPIM_RST  = (1 << 4)
};

enum spim_cmd
{
  SPI_FLASH_WREN = 0x06,
  SPI_FLASH_WRDI = 0x04,
  SPI_FLASH_RDID = 0x9f,
  SPI_FLASH_RDSR = 0x05,
  SPI_FLASH_PP   = 0x02,
  SPI_FLASH_SE   = 0xd8,
  SPI_FLASH_BE   = 0xc7,
  SPI_FLASH_RES  = 0xab,
};

enum spim_flash_sr
{
  SPI_FLASH_SR_WIP = (1 << 0),
  SPI_FLASH_SR_WEL = (1 << 1),
  SPI_FLASH_SR_SRWD = (1 << 7)
};

word
managed_spim::spim_rxtx (word cmd)
{
  write_reg (SPIM_TX, cmd);
  while (!(read_reg (SPIM_STATUS) & SPIM_DONE))
    ;
  word res = read_reg (SPIM_RX);
  write_reg (SPIM_STATUS, SPIM_DONE);
  return res;
}

bool
managed_spim::spim_start (void)
{
  word status = read_reg (SPIM_STATUS);
  if (!(status & SPIM_INIT))
    {
      cout << "spim not initialized" << endl;
      return false;
    }
  if (status & SPIM_BUSY)
    {
      cout << "spim is busy" << endl;
      return false;
    }
  if (status & SPIM_DONE)
    write_reg (SPIM_STATUS, SPIM_DONE);

  word rx;

  write_reg (SPIM_CONTROL, SPIM_USRC | SPIM_CSN);
  //  Just for clocking.
  rx = spim_rxtx (0);
  write_reg (SPIM_CONTROL, SPIM_USRC);

  return true;
}

void
managed_spim::spim_space (void)
{
  word rx;

  write_reg (SPIM_CONTROL, SPIM_USRC | SPIM_CSN);
  //  Just for clocking.
  rx = spim_rxtx (0);
  write_reg (SPIM_CONTROL, SPIM_USRC);
}

void
managed_spim::cmd_spim_id (void)
{
  if (!spim_start ())
    return;

  word rx;
  rx = spim_rxtx (SPI_FLASH_RDID);
  //  Result
  rx = spim_rxtx (0);
  cout << " " << hex2 (rx);
  rx = spim_rxtx (0);
  cout << " " << hex2 (rx);
  rx = spim_rxtx (0);
  cout << " " << hex2 (rx);
  cout << endl;
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_status (void)
{
  if (!spim_start ())
    return;

  word rx;
  rx = spim_rxtx (SPI_FLASH_RDSR);
  //  Result
  rx = spim_rxtx (0);
  cout << "status: " << hex2 (rx);
  cout << endl;
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_sig (void)
{
  if (!spim_start ())
    return;

  word rx;
  rx = spim_rxtx (SPI_FLASH_RES);
  //  Dummy bytes
  for (int i = 0; i < 3; i++)
    rx = spim_rxtx (0);
  //  Electronic signature
  rx = spim_rxtx (0);
  cout << "signature: " << hex2 (rx) << endl;
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_wren (void)
{
  if (!spim_start ())
    return;

  word rx;
  rx = spim_rxtx (SPI_FLASH_WREN);
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_wrdi (void)
{
  if (!spim_start ())
    return;

  word rx;
  rx = spim_rxtx (SPI_FLASH_WRDI);
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_raw_write (menu_item_arg &args)
{
  cmd_arg_file *arg = dynamic_cast<cmd_arg_file *>(args.get_arg (0));
  word addr = 0;
  std::ifstream is (arg->filename, ios::binary);
  
  if (!spim_start ())
    return;

  word rx;
  while (1)
    {
      char buf[256];
      size_t len;

      is.read (buf, sizeof (buf));
      len = is.gcount ();
      if (len == 0)
	break;

      rx = spim_rxtx (SPI_FLASH_WREN);
      spim_space ();

      cout << "flashing at " << hex8 (addr) << endl;
      spim_rxtx (SPI_FLASH_PP);
      //  24 bit address
      spim_rxtx ((addr >> 16) & 0xff);
      spim_rxtx ((addr >> 8) & 0xff);
      spim_rxtx ((addr >> 0) & 0xff);
      for (size_t i = 0; i < len; i++)
	spim_rxtx (buf[i] & 0xff);
      spim_space ();

      cout << "waiting..." << endl;
      spim_rxtx (SPI_FLASH_RDSR);
      while ((spim_rxtx (0) & SPI_FLASH_SR_WIP) != 0)
	;
      spim_space ();

      if (user_stop)
	{
	  cout << "interrupted!" << endl;
	  break;
	}
      addr += len;
    }
  is.close ();

  rx = spim_rxtx (SPI_FLASH_WRDI);
  spim_space ();
  write_reg (SPIM_CONTROL, 0);
}

bool
managed_spim::enable_write (void)
{
  if (!spim_start ())
    return false;

  word rx;
  rx = spim_rxtx (SPI_FLASH_WREN);
  spim_space ();

  spim_rxtx (SPI_FLASH_RDSR);
  rx = spim_rxtx (0);
  spim_space ();
  if (!(rx & SPI_FLASH_SR_WEL))
    {
      cout << "write enable not set" << endl;
      write_reg (SPIM_CONTROL, 0);
      return false;
    }
  return true;
}

void
managed_spim::wait_end_of_write (void)
{
  spim_rxtx (SPI_FLASH_RDSR);
  while ((spim_rxtx (0) & SPI_FLASH_SR_WIP) != 0)
    ;
  spim_space ();

  word rx;
  if (0)
    {
      spim_rxtx (SPI_FLASH_RDSR);
      rx = spim_rxtx (0);
      cout << "status: " << hex2 (rx) << endl;
      spim_space ();
    }
  rx = spim_rxtx (SPI_FLASH_WRDI);
  spim_space ();
  write_reg (SPIM_CONTROL, 0);
}

void
managed_spim::cmd_spim_se (menu_item_arg &args)
{
  cmd_arg_expr *arg = dynamic_cast<cmd_arg_expr *>(args.get_arg (0));
  word addr = arg->value;

  if (!enable_write ())
    return;

  cout << "erasing sector " << hex8 (addr) << endl;
  spim_rxtx (SPI_FLASH_SE);
  //  24 bit address
  spim_rxtx ((addr >> 16) & 0xff);
  spim_rxtx ((addr >> 8) & 0xff);
  spim_rxtx ((addr >> 0) & 0xff);
  spim_space ();

  cout << "waiting..." << endl;
  wait_end_of_write ();
}

void
managed_spim::cmd_spim_erase (void)
{
  if (!enable_write ())
    return;

  cout << "erasing..." << endl;
  spim_rxtx (SPI_FLASH_BE);
  spim_space ();

  wait_end_of_write ();
}

static const unsigned char spim_prg[] = {
#include "spim_prg.h"
};

void
managed_spim::cmd_spim_write (dsu *adsu, menu_item_arg &args)
{
  cmd_arg_file *arg = dynamic_cast<cmd_arg_file *>(args.get_arg (0));
  word prg_base = 0x40000000;
  word buf_base = 0x40001000;
  word file_len;

  //  Load loader.
  cout << "loading flasher" << endl;
  load_bin (*adsu, prg_base, spim_prg, sizeof (spim_prg));

  //  Load image
  cout << "loading " << arg->filename << endl;
  {
    std::ifstream is (arg->filename, ios::binary);
    word buf_addr = buf_base;

    while (1)
      {
	unsigned char buf[1024];
	is.read ((char *)buf, sizeof (buf));
	size_t len = is.gcount ();
	if (len == 0)
	  break;
	load_bin (*adsu, buf_addr, buf, len);
	buf_addr += len;
      }
    file_len = buf_addr - buf_base;
  }

  adsu->set_entry (prg_base);
  Cpu *cpu = adsu->get_cpus().front();
  cpu->set_gpr (REG_SPARC_O0, this->base);
  cpu->set_gpr (REG_SPARC_O1, buf_base);
  cpu->set_gpr (REG_SPARC_O2, file_len);

  cout << "Type go to flash" << endl;
#if 0
  cout << "flashing..." << endl;
  adsu->go ();
  word status = cpu->get_gpr (REG_SPARC_O0);
  if (status != 0)
    cout << "flash failed" << endl;
#endif
}
