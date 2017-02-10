#ifndef LEMON_H_
#define LEMON_H_

//  A Leon word (32 bits).
typedef unsigned int word;
typedef signed int sword;

inline void
pack_be32 (unsigned char *data, word val)
{
  data[0] = val >> 24;
  data[1] = val >> 16;
  data[2] = val >> 8;
  data[3] = val >> 0;
}

inline word
unpack_be32 (const unsigned char *data)
{
  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3] << 0);
}

inline word
unpack_be16 (const unsigned char *data)
{
  return (data[0] << 8) | (data[1] << 0);
}

enum bar_type {
  BAR_APB_IO = 0x01,
  BAR_AHB_MEM = 0x02,
  BAR_AHB_IO = 0x03
};

const word bad_base = 0xffffffff;

inline bar_type
bar_to_typ (word bar)
{
  return (bar_type)(bar & 0x0f);
}

inline unsigned int
id_to_vid (unsigned int id)
{
  return id >> 24;
}

inline unsigned int
id_to_did (unsigned int id)
{
  return (id >> 12) & 0xfff;
}

inline unsigned int
id_to_ver (unsigned int id)
{
  return (id >> 5) & 0x1f;
}

inline unsigned int
id_to_irq (unsigned int id)
{
  return id & 0x1f;
}

word bar_to_base (word bar, word bus_base);

#endif /* LEMON_H_ */
