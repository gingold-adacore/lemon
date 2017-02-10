#include <list>
#include <iostream>

#include "links.h"
#include "devices.h"
#include "soc.h"

static void
disp_bar (word bar, word base)
{
  word addr = bar >> 20;
  word mask = (~bar >> 4) & 0xfff;
  int typ = bar & 0xf;

  switch (typ)
    {
    case 0x1:
      printf ("0x%08x-0x%08x",
	      base + (addr << 8), base | ((addr | mask) << 8) | 0xff);
      printf (" (APB I/O)");
      break;
    case 0x2:
      printf ("0x%08x-0x%08x",
	      addr << 20, ((addr | mask) << 20) | 0xfffff);
      printf (" (AHB mem)");
      break;
    case 0x3:
      printf ("0x%08x-0x%08x",
	      base + (addr << 8), base | ((addr | mask) << 8) | 0xff);
      printf (" (AHB I/O)");
      break;
    default:
      printf (" unknown");
      break;
    }
  printf ("\n");
}

template<>
void
apb_device::disp_device (string pfx)
{
  const struct vendor_desc *vend = find_vendor (id_to_vid (pnp.id));
  const struct device_desc *dev = find_device (vend, id_to_did (pnp.id));
  unsigned int ver = id_to_ver (pnp.id);
  unsigned int irq = id_to_irq (pnp.id);

  cout << pfx;
  if (dev != NULL)
    cout << dev->name << " ";
  if (vend != NULL)
    cout << vend->name << " ";
  cout << " apb slv#" << (num & 63);
  if (irq != 0)
    cout << " irq#" << irq;
  cout << " ver:" << ver;
  cout << endl;
  // ::disp_device (pnp[0]);
  if (pnp.bar != 0)
    {
      cout << pfx << "    bar" << ": ";
      disp_bar (pnp.bar, parent->base);
    }
}

template<>
word
apb_device::get_id (void)
{
  return pnp.id;
}

template<>
void
apb_ctrl::probe (void)
{
  clog << "probing apb at " << hex << base << endl;

  for (int j = 0; j < 16; j++)
    {
      unsigned char data[8];
      apb_pnp pnp;

      if (!get_link ()->read (base + 0xff000 + (j << 3), 2, data))
	{
	  cerr << "cannot read" << endl;
	  return;
	}

      pnp.id = unpack_be32 (data);
      if (pnp.id == 0)
	continue;
      pnp.bar = unpack_be32 (data + 4);

      this->append (new apb_device (pnp, j, this));
    }
}

template<>
void
ahb_device::disp_device (string pfx)
{
  const struct vendor_desc *vend = find_vendor (id_to_vid (pnp.id));
  const struct device_desc *dev = find_device (vend, id_to_did (pnp.id));
  unsigned int ver = id_to_ver (pnp.id);
  unsigned int irq = id_to_irq (pnp.id);

  cout << pfx;
  if (dev != NULL)
    cout << dev->name << " ";
  if (vend != NULL)
    cout << vend->name << " ";
  cout << "  ahb " << (num < 64 ? "mst" : "slv") << "#" << (num & 63);
  cout << " irq#" << irq;
  cout << " ver#" << ver;
  cout << endl;
  // ::disp_device (pnp[0]);
  for (int i = 0; i < 4; i++)
    if (pnp.bar[i] != 0)
      {
	cout << pfx << "    bar" << i << ": ";
	disp_bar (pnp.bar[i], parent->base);
      }
}

template<>
word
ahb_device::get_id (void)
{
  return pnp.id;
}

template<>
void
ahb_ctrl::probe (void)
{
  clog << "probing ahb at " << hex << base << endl;

  for (int j = 0; j < 128; j++)
    {
      unsigned char data[32];
      ahb_pnp pnp;

      if (!get_link ()->read (base + 0xff000 + (j << 5), 8, data))
	{
	  cerr << "cannot read" << endl;
	  return;
	}

      pnp.id = unpack_be32 (data);
      if (pnp.id == 0)
	continue;
      for (int i = 0; i < 3; i++)
	pnp.res[i] = unpack_be32 (data + 4 + 4 * i);
      for (int i = 0; i < 4; i++)
	pnp.bar[i] = unpack_be32 (data + 16 + 4 * i);

      unsigned vid = id_to_vid (pnp.id);
      unsigned did = id_to_did (pnp.id);

      if (vid == VENDOR_GAISLER
	  && did == DEVICE_AHB2AHB)
	{
	  word base = bar_to_base (pnp.bar[3], this->base);

	  if (base != bad_base)
	    {
	      if (!parent->bus_already_known (base))
		parent->append (new ahb_ctrl (parent, base));
	    }
	}
      else if (vid == VENDOR_GAISLER
	       && did == DEVICE_APBCTRL)
	{
	  word base = bar_to_base (pnp.bar[0], this->base);

	  if (base != bad_base)
	    {
	      if (!parent->bus_already_known (base))
		parent->append (new apb_ctrl (parent, base));
	    }
	}

      this->append (new ahb_device (pnp, j, this));
    }
}

soc::soc (dsu_link *link, word ahb_base) : link (link)
{
  append (new ahb_ctrl (this, ahb_base));
}

bool
soc::bus_already_known (word base)
{
  for (auto d : this->buses)
    if (d->base == base)
      return true;
  return false;
}

void
soc::probe (void)
{
  for (auto b : buses)
    b->probe ();
}

void
soc::disp_devices (void)
{
  int n = 0;

  for (auto b : this->buses)
    {
      cout << "bus #" << n << " at " << hex << b->base << ":" << endl;
      b->disp_devices (" ");
      n++;
    }
}
