#ifndef LINKS_H_
#define LINKS_H_

#include "lemon.h"

extern bool trace_com;

//  This exception is thrown in case of failure at the link level.
class link_error
{
 public:
  link_error (word addr) : addr (addr) {}
  word addr;
};

class dsu_link
{
public:
  //  Open the link.
  virtual bool open (void) = 0;

  //  Close the link.
  virtual void close (void) = 0;

  //  Read NWORDS words at ADDR and put them to RES.
  //  Return True for success.
  virtual bool read (word addr, unsigned int nwords, unsigned char *res) = 0;

  //  Write NWORDS words at ADDR from BUF.
  //  Return True for success.
  virtual bool write (word addr, unsigned int nwords,
		      const unsigned char *buf) = 0;

  //  Read one word, represented in host endianess.
  //  Throw link_error in case of failure.
  word read_word (word addr);

  //  Write one word (in host endianness).
  //  Throw link_error in case of failure.
  void write_word (word addr, word val);

  //  Maximum number of data bytes per packet.
  virtual unsigned get_max_len (void) = 0;
};

struct libusb_device_handle;

class usb_dsu_link : public dsu_link
{
public:
  bool open (void);
  void close (void);
  bool read (word addr, unsigned int nwords, unsigned char *res);
  bool write (word addr, unsigned int nwords, const unsigned char *buf);
  unsigned get_max_len (void) { return max_len; }
 private:
  void trace (const char *pfx, const unsigned char *buf, int len);
  unsigned int max_len;
  struct libusb_device_handle *devh = nullptr;
};

dsu_link *create_eth_dsu_link (const char *ipaddr);

class remote_dsu_link : public dsu_link
{
 public:
  remote_dsu_link (const char *daddr) : daddr (daddr) {};
  bool open (void);
  bool read (word addr, unsigned int nwords, unsigned char *res);
  bool write (word addr, unsigned int nwords, const unsigned char *buf);
  void close (void) { }
  unsigned get_max_len (void) { return 250; }
 private:
  int send_recv (unsigned char *pkt, int len);

  const char *daddr;
  int sock;
};

#ifdef HAVE_LIBURJTAG

typedef struct URJ_CHAIN urj_chain_t;
typedef struct URJ_PART_INSTRUCTION urj_part_instruction_t;
typedef struct URJ_PART urj_part_t;

class jtag_dsu_link : public dsu_link
{
 public:
  jtag_dsu_link (const char *cable) : cable (cable) {};
  bool open (void);
  bool read (word addr, unsigned int nwords, unsigned char *res);
  bool write (word addr, unsigned int nwords, const unsigned char *buf);
  void close (void) { }
  unsigned get_max_len (void) { return 1024; /* 1kB but no boundary cross. */ }
 private:
  bool set_cmd (word addr, int w);

  const char *cable;
  urj_chain_t *chain;
  urj_part_t *part = nullptr;
  urj_part_instruction_t *user1;
  urj_part_instruction_t *user2;
};

#endif /* HAVE_LIBURJTAG */

#endif /* LINKS_H_ */
