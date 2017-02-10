#ifndef DEVICES_H_
#define DEVICES_H_

#define VENDOR_GAISLER	1
#define VENDOR_ESA	4

#define DEVICE_AHB2AHB  0x20
#define DEVICE_APBCTRL	0x06
#define DEVICE_APBUART  0x0c
#define DEVICE_IRQMP    0x0d
#define DEVICE_GPTIMER  0x11
#define DEVICE_GRETH	0x1d
#define DEVICE_DSU4	0x49
#define DEVICE_DSU3	0x04
#define DEVICE_DDR2SPA  0x2e
#define DEVICE_DDRSPA   0x25
#define DEVICE_SPIM     0x45

struct device_desc
{
  unsigned short device;
  unsigned char is_bridge;
  const char *name;
  const char *desc;
};

struct vendor_desc
{
  unsigned int vid;
  const char *name;
  const struct device_desc *devs;
};

extern const struct vendor_desc vendors[];

/* Return the description of a vendor given the VID (resp. of a device given
   both VENDOR and DID).
   Return NULL if not found.
   The function find_device will also return NULL if VENDOR is NULL.  */
const struct vendor_desc *find_vendor (unsigned int vid);
const struct device_desc *find_device (const struct vendor_desc *vendor,
				       unsigned int did);

#endif /* DEVICES_H_ */
