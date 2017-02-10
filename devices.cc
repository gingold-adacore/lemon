#include <stddef.h>
#include "devices.h"

//  List of devices.  Must be ordered.
static const struct device_desc gaisler_devices[] = {
  { 0x003, 0, "leon3", NULL },
  { 0x004, 0, "dsu3", NULL },
  { 0x006, 0, "apbctrl", NULL },
  { 0x007, 0, "ahbuart", NULL },
  { 0x008, 0, "srctrl", NULL },
  { 0x009, 0, "sdctrl", NULL },
  { 0x00a, 0, "ssrctrl", NULL },
  { 0x00b, 0, "i2c2ahb", NULL },
  { 0x00c, 0, "apbuart", NULL },
  { 0x00d, 0, "irqmp", NULL },
  { 0x00e, 0, "ahbram", NULL },
  { 0x00f, 0, "ahbdpram", NULL },

  { 0x010, 0, "pciarb", NULL },
  { 0x011, 0, "gptimer", NULL },
  { 0x012, 0, "pcitarget", NULL },
  { 0x014, 0, "pcimtf", NULL },
  { 0x015, 0, "pcitrace", NULL },
  { 0x016, 0, "pcidma", NULL },
  { 0x017, 0, "ahbtrace", NULL },
  { 0x019, 0, "can_oc", NULL },
  { 0x01a, 0, "grgpio", NULL },
  { 0x01b, 0, "ahbrom", NULL },
  { 0x01c, 0, "ahbjtab", NULL },
  { 0x01d, 0, "greth", NULL },
  { 0x01f, 0, "grspw", NULL },

  { 0x020, 1, "ahb2ahb", NULL },
  { 0x021, 0, "grbusbdc", NULL },
  { 0x022, 0, "grusbdc", NULL },
  { 0x025, 0, "ddrspa", "single port 16/32/64 bit DDR controller" },
  { 0x026, 0, "grusbhc (ehc)", NULL },
  { 0x027, 0, "grusbhc (uhc)", NULL },
  { 0x028, 0, "i2cmst", NULL },
  { 0x029, 0, "grspw2", NULL },
  { 0x02d, 0, "spictrl", NULL },
  { 0x02e, 0, "ddr2spa", "single port 16/32/64 bit DDR2 controller" },

  { 0x035, 0, "grfifo", NULL },
  { 0x036, 0, "gradcdac", NULL },
  { 0x037, 0, "grpulse", NULL },
  { 0x03a, 0, "grversion", NULL },
  { 0x03d, 0, "grcan", NULL },
  { 0x03e, 0, "i2cslv", NULL },

  { 0x040, 0, "ahbmstem", NULL },
  { 0x041, 0, "ahbslvem", NULL },
  { 0x045, 0, "spimctrl", NULL },
  { 0x047, 0, "l4stat", NULL },
  { 0x048, 0, "leon4", NULL },
  { 0x049, 0, "dsu4", NULL },
  { 0x04a, 0, "grpwm", NULL },
  { 0x04b, 0, "l2cache", NULL },
  { 0x04d, 0, "gr1553b", NULL },
  { 0x04f, 0, "griommu", NULL },

  { 0x050, 0, "ftahbram", NULL },
  { 0x051, 0, "ftsrctrl", NULL },
  { 0x052, 0, "ahbstat", NULL },
  { 0x054, 0, "ftmctrl", NULL },
  { 0x055, 0, "ftsdctrl", NULL },
  { 0x056, 0, "ftsrctrl8", NULL },
  { 0x057, 0, "memscrub", NULL },
  { 0x058, 0, "ftsdctrl64", NULL },
  { 0x059, 0, "nandfctrl", NULL },
  { 0x05c, 0, "spi2ahb", NULL },

  { 0x060, 0, "apbps2", NULL },
  { 0x061, 0, "apbvga", NULL },
  { 0x062, 0, "logan", NULL },
  { 0x063, 0, "vgactrl", NULL },
  { 0x066, 0, "grsysmon", NULL },
  { 0x067, 0, "gracectrl", NULL },
  { 0x068, 0, "atf", NULL },

  { 0x073, 0, "graes", NULL },
  { 0x074, 0, "grecc", NULL },
  { 0x07b, 0, "graes_dma", NULL },
  { 0x07c, 0, "grpci2", NULL },

  { 0x087, 0, "grgpreg", NULL },

  { 0x093, 0, "rgmii", NULL },
  { 0x096, 1, "ahb2avla", NULL },
  { 0x000, 0, NULL, NULL}
};

static const struct device_desc esa_devices[] = {
  { 0x00f, 0, "mctrl", "8/16/32 bit PROM/SRAM/SDRAM controller" },
  { 0x010, 0, "pciarb", NULL },

  { 0x000, 0, NULL, NULL}
};

const struct vendor_desc vendors[] = {
  { VENDOR_GAISLER, "Gaisler", gaisler_devices },
  { VENDOR_ESA, "ESA", esa_devices },
  { 0, NULL, NULL }
};

const struct vendor_desc *
find_vendor (unsigned int vid)
{
  const struct vendor_desc *desc;

  for (desc = vendors; desc->vid; desc++)
    if (desc->vid == vid)
      return desc;
  return NULL;
}

const struct device_desc *
find_device (const struct vendor_desc *vendor, unsigned int did)
{
  const struct device_desc *desc;

  if (vendor == NULL)
    return NULL;

  for (desc = vendor->devs; desc->device != 0; desc++)
    if (desc->device == did)
      return desc;
  return NULL;
}

