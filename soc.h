#ifndef SOC_H_
#define SOC_H_

#include <string>
#include <list>

#include "lemon.h"
#include "links.h"

using namespace std;

class bus;
class device;
class managed_device;

//  A soc describes all devices in the chip, as well as the dsu access.
class soc
{
public:
  // Create a soc using LINK to connect and an AHB PnP address AHB_BASE.
  soc (dsu_link *link, word ahb_base);

  //  Get connection link.
  dsu_link *get_link(void) { return link; }

  //  Add a new bus/device to the soc.
  void append (bus *b) { buses.push_back (b); }
  void append (device *dev) { all_devices.push_back (dev); }

  //  Get all devices that were probed.
  list<device *> get_devices(void) { return all_devices; }

  //  Get the list of managed devices, ie devices that are known.
  list<managed_device *> get_managed_devices(void) { return managed_devices; }

  //  Add to managed_devices list
  void add_managed_device (managed_device *dev)
    { managed_devices.push_back (dev); }

  //  Probe all buses and devices.
  void probe (void);

  //  Disp all devices, hierachically.
  void disp_devices (void);

  //  Return True is there is already a bus for BASE.
  //  Avoid infinite loop due to bridges.
  bool bus_already_known (word base);

private:
  dsu_link *link;
  std::list<bus *> buses;
  list<device *> all_devices;
  list<managed_device *> managed_devices;
};

//  Devices are attached to a bus (apb or ahb).
//  This is the base abstract class.
class bus
{
public:
  bus (soc *parent, word base) : base (base), parent (parent) {};
  virtual void probe (void) = 0;
  virtual void disp_devices (string pfx) = 0;
  dsu_link *get_link(void) { return parent->get_link(); }
  word base; // Bus base address
  soc *parent;
};

//  A soc device.
//  Base abstract class.
class device
{
 public:
  virtual void disp_device (string pfx) = 0;
  virtual word get_id (void) = 0;
};

//  Template for both ahb and apb buses and devices.
template <typename pnp_type>
class soc_bus {
public:
  class bus_device;

  class bus_ctrl : public bus
  {
  public:
    bus_ctrl (soc *parent, word base) : bus (parent, base) {};
    void probe (void);
    void disp_devices (string pfx)
    {
      for (auto d : children)
	d->disp_device (pfx);
    }
  private:
    unsigned int num;
    std::list<bus_device *> children;

    void append (bus_device *d)
    {
      children.push_back (d);
      parent->append (d);
    }
  };

  class bus_device : public device
  {
  public:
    bus_device (pnp_type pnp, unsigned int num, bus_ctrl *parent) :
      parent (parent), num (num), pnp (pnp) {};
    void disp_device (string pfx);
    word get_id (void);
    pnp_type get_pnp (void) { return pnp; }
    bus_ctrl *get_parent (void) { return parent; }
  private:
    bus_ctrl *parent;  // Bus for this device
    unsigned int num;  // Index for ths parent
    pnp_type pnp;
  };
};

//  APB bus and device.
struct apb_pnp
{
  word id;
  word bar;
};

typedef soc_bus<apb_pnp>::bus_ctrl apb_ctrl;
typedef soc_bus<apb_pnp>::bus_device apb_device;

//  AHB bus and device.
struct ahb_pnp
{
  word id;
  word res[3];
  word bar[4];
};

typedef soc_bus<ahb_pnp>::bus_ctrl ahb_ctrl;
typedef soc_bus<ahb_pnp>::bus_device ahb_device;

class managed_device
{
 public:
  virtual void reset (void) {}
};

#endif /* SOC_H_ */
