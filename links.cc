#include <iostream>
#include <libusb-1.0/libusb.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

#include "links.h"
#include "outputs.h"

using namespace std;

bool trace_com = false;

static void
trace_ascii (const char *pfx, const unsigned char *buf, int len)
{
  while (len > 0)
    {
      int l = len > 64 ? 64 : len;
      cout << pfx << ": ";
      for (int i = 0; i < l; i++)
	cout << buf[i];
      cout << endl;
      len -= l;
      buf += l;
    }
}

word
dsu_link::read_word (word addr)
{
  unsigned char data[4];

  if (!read (addr, 1, data))
    throw link_error (addr);

  return unpack_be32 (data);
}

void
dsu_link::write_word (word addr, word val)
{
  unsigned char data[4];

  pack_be32 (data, val);

  if (!write (addr, 1, data))
    throw link_error (addr);
}

bool
usb_dsu_link::open (void)
{
  int r;

  r = libusb_init(NULL);
  if (r < 0)
    {
      cerr << "failed to initialise libusb\n";
      return false;
    }

  devh = libusb_open_device_with_vid_pid(NULL, 0x1781, 0x0aa0);
  if (devh == NULL)
    {
      cerr << "Could not find any DSU device\n";
      return false;
    }

  r = libusb_claim_interface(devh, 0);
  if (r < 0)
    {
      cerr << "usb_claim_interface error " << r << endl;
      return false;
    }

  libusb_device *dev = libusb_get_device (devh);
  int max_size = libusb_get_max_packet_size (dev, 1);
  int speed = libusb_get_device_speed (dev);

  max_len = max_size - 8;
  cout << "max_size: " << max_size << ", speed: " << speed << ", max_len: "
       << max_len << endl;
  return true;
}

void
usb_dsu_link::close (void)
{
  if (devh != NULL)
    {
      libusb_release_interface(devh, 0);
      libusb_close(devh);
      devh = NULL;
    }
}

void
usb_dsu_link::trace (const char *pfx, const unsigned char *buf, int len)
{
#if 0
  word cw = unpack_be32 (buf + 2);

  cout << pfx << ": off=" << hex4 ((buf[0] << 8) | buf[1]);
  cout << ", seq=" << hex4 (cw >> 18);
  cout << ", rw=" << hex1 ((cw >> 17) & 1);
  cout << ", len=" << hex4 ((cw >> 7) & 0x3ff);
  cout << ", unused=" << hex2 (cw & 0x7f);
  cout << ", addr=" << hex8 (unpack_be32 (buf + 6));
  cout << endl;

  buf += 10;
  len -= 10;
#endif
  while (len > 0)
    {
      int l = len > 16 ? 16 : len;
      cout << pfx << ":";
      for (int i = 0; i < l; i++)
	cout << " " << hex2 (buf[i]);
      cout << endl;
      len -= l;
      buf += l;
    }
}

bool
usb_dsu_link::read (word addr, unsigned int nwords, unsigned char *res)
{
  int r;
  unsigned char tx_data[8];
  int tfr;
  unsigned int len = nwords << 2;

  pack_be32 (&tx_data[0], addr);
  pack_be32 (&tx_data[4], len);
  if (trace_com)
    trace ("R>", tx_data, 8);
  r = libusb_bulk_transfer
    (devh, LIBUSB_ENDPOINT_OUT | 1, tx_data, 8, &tfr, 10);
  if (r != 0 || tfr != 8)
    return false;
  r = libusb_bulk_transfer (devh, LIBUSB_ENDPOINT_IN | 1, res, len, &tfr, 10);
  if (trace_com)
    trace ("R>", res, len);
  if (r != 0 || tfr != len)
    return false;
  return true;
}

bool
usb_dsu_link::write (word addr, unsigned int nwords, const unsigned char *buf)
{
  int r;
  unsigned char tx_data[512];
  int tfr;
  unsigned int len = nwords << 2;

  pack_be32 (&tx_data[0], addr);
  pack_be32 (&tx_data[4], len | 0x80000000);
  memcpy (tx_data + 8, buf, len);

  if (trace_com)
    trace ("W>", tx_data, len + 8);

  r = libusb_bulk_transfer
    (devh, LIBUSB_ENDPOINT_OUT | 1, tx_data, len + 8, &tfr, 10);
  if (r != 0 || tfr != len + 8)
    return false;
  return true;
}

class eth_dsu_link : public dsu_link
{
 public:
  eth_dsu_link (const char *ipaddr) : ipaddr (ipaddr), seq (0) {};
  bool open (void);
  bool read (word addr, unsigned int nwords, unsigned char *res);
  bool write (word addr, unsigned int nwords, const unsigned char *buf);
  void close (void) { }
  //  Maximum data payload for worst case.
  unsigned get_max_len (void) { return 200; }
 private:
  void write_header (unsigned char *pkt, unsigned int len, word addr, int rw);
  void trace_edcl (const char *pfx, const unsigned char *buf, int len);
  const char *ipaddr;
  struct sockaddr_in dest;
  unsigned int seq;
  int sock;
};

dsu_link *
create_eth_dsu_link (const char *ipaddr)
{
  return new eth_dsu_link (ipaddr);
}

bool
eth_dsu_link::open (void)
{
  struct in_addr addr;

  if (!::inet_aton (ipaddr, &addr))
    return false;

  //  Create the socket.
  sock = ::socket (PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    {
      perror ("cannot create socket");
      return false;
    }

  //  Bind it to any port.
  //  As EDCL replies with SPORT = DPORT, datagrams will be sent to the same
  //  port too.
  dest.sin_len = sizeof (dest);
  dest.sin_family = AF_INET;
  dest.sin_port = htons (1025);
  dest.sin_addr = addr;

  return true;
}

void
eth_dsu_link::write_header (unsigned char *pkt, unsigned int len,
			    word addr, int rw)
{
  //  Offset
  pkt[0] = 0;
  pkt[1] = 0;
  //  Control word
  pack_be32 (&pkt[2], (seq << 18) | (rw << 17) | (len << 7));
  //  Address.
  pack_be32 (&pkt[6], addr);
}

void
eth_dsu_link::trace_edcl (const char *pfx, const unsigned char *buf, int len)
{
  word cw = unpack_be32 (buf + 2);

  cout << pfx << ": off=" << hex4 ((buf[0] << 8) | buf[1]);
  cout << ", seq=" << hex4 (cw >> 18);
  cout << ", rw=" << hex1 ((cw >> 17) & 1);
  cout << ", len=" << hex4 ((cw >> 7) & 0x3ff);
  cout << ", unused=" << hex2 (cw & 0x7f);
  cout << ", addr=" << hex8 (unpack_be32 (buf + 6));
  cout << endl;

  buf += 10;
  len -= 10;

  while (len > 0)
    {
      int l = len > 16 ? 16 : len;
      cout << pfx << ":";
      for (int i = 0; i < l; i++)
	cout << " " << hex2 (buf[i]);
      cout << endl;
      len -= l;
      buf += l;
    }
}

bool
eth_dsu_link::read (word addr, unsigned int nwords, unsigned char *res)
{
  int r;
  unsigned char tx_data[10];
  unsigned char rx_data[1536];
  unsigned int len = nwords << 2;
  unsigned int app;
  struct pollfd fds;

  fds.fd = sock;
  fds.events = POLLIN;

  while (1)
    {
      write_header (tx_data, len, addr, 0);

      while (1)
	{
	  if (trace_com)
	    trace_edcl ("R>", tx_data, sizeof (tx_data));
	  r = ::sendto (sock, tx_data, sizeof (tx_data), 0,
			(struct sockaddr *)&dest, sizeof (dest));
	  if (r < 0)
	    {
	      perror ("send");
	      return false;
	    }

	  //  TODO: there is no timeout/retry.
	  //  OTOH, that won't work for IO with side effects.
	  r = poll (&fds, 1, 50);
	  if (r == 1)
	    break;
	}
      r = recv (sock, rx_data, sizeof (rx_data), 0);
      if (r < 0)
	return false;
      if (trace_com)
	trace_edcl ("R<", rx_data, r);
      if (r < 6)
	return false;
      app = unpack_be32 (rx_data + 2);
      if (app & (1 << 17))
	seq = app >> 18;
      else
	break;
    }
  seq++;
  if (((app >> 7) & 0x3ff) != len)
    return false;
  memcpy (res, rx_data + 10, len);
  return true;
}

bool
eth_dsu_link::write (word addr, unsigned int nwords, const unsigned char *buf)
{
  int r;
  unsigned char pkt[1536];
  unsigned int len = nwords << 2;
  unsigned int app;

  //  Set offset
  while (1)
    {
      write_header (pkt, len, addr, 1);
      memcpy (pkt + 10, buf, len);

      if (trace_com)
	trace_edcl ("W>", pkt, len + 10);
      r = ::sendto (sock, pkt, len + 10, 0,
		    (struct sockaddr *)&dest, sizeof (dest));
      if (r < 0)
	return false;

      //  TODO: there is no timeout/retry.
      //  OTOH, that won't work for IO with side effects.
      r = recv (sock, pkt, sizeof (pkt), 0);
      if (r < 0)
	return false;

      if (trace_com)
	trace_edcl ("W<", pkt, r);

      if (r < 6)
	return false;
      app = unpack_be32 (pkt + 2);
      if (app & (1 << 17))
	seq = app >> 18;
      else
	break;
    }
  seq++;
  return true;
}

bool
remote_dsu_link::open (void)
{
  struct in_addr addr;
  struct sockaddr_in dest;
  unsigned int port;
  char *p;

  p = strchr (daddr, ':');
  if (p == nullptr)
    {
      cerr << "remote link: missing ':' in --remote" << endl;
      return false;
    }
  *p = 0;
  if (p != daddr)
    {
      if (!::inet_aton (daddr, &addr))
	{
	  cerr << "cannot parse ip address" << endl;
	  return false;
	}
    }
  else
    addr.s_addr = htonl (INADDR_LOOPBACK);
  
  port = atoi (p + 1);

  //  Create the socket.
  sock = ::socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("cannot create socket");
      return false;
    }
  //  Connect.
  memset (&dest, 0, sizeof dest);
  dest.sin_len = sizeof (dest);
  dest.sin_family = AF_INET;
  dest.sin_port = htons (port);
  dest.sin_addr = addr;

  if (::connect (sock, (struct sockaddr *) &dest, sizeof (dest)) < 0)
    {
      ::perror ("cannot connect");
      ::close (sock);
      return false;
    }
  return true;
}

static unsigned char *
put_hex (unsigned char *p, word v, int size)
{
  for (int i = size * 2 - 1; i >= 0; i--)
    *p++ = xdigits[(v >> (4 * i)) & 0x0f];
  return p;
}

static unsigned int
read_hex1 (unsigned char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 255;
}

static unsigned int
read_hex2 (unsigned char *p)
{
  unsigned int res;

  res = 0;
  for (int i = 0; i < 2; i++)
    {
      unsigned int d = read_hex1 (p[i]);
      if (d > 15)
	return ~0U;
      res = (res << 4) | d;
    }
  return res;
}

int
remote_dsu_link::send_recv (unsigned char *pkt, int len)
{
  unsigned char csum;
  int res;
  int off;

  pkt[len] = '#';

  csum = 0;
  for (int i = 1; i < len; i++)
    csum += pkt[i];
  put_hex (pkt + len + 1, csum, 1);
  len += 3;
  if (trace_com)
    trace_ascii (">", pkt, len);
  if (send (sock, pkt, len, 0) != len)
    return 0;

  //  Wait for ack.
  res = recv (sock, pkt, 1, 0);
  if (trace_com)
    trace_ascii ("<", pkt, res);
  if (res != 1 || pkt[0] != '+')
    return 0;

  //  Wait for answer (2 characters after '$').
  enum { S_dollar, S_body, S_c0, S_c1 } state = S_dollar;

  off = 0;
  csum = 0;
  unsigned char ecsum;
  while (1)
    {
      unsigned char buf[1024];
      int res;

      res = recv (sock, buf, sizeof (buf), 0);
      if (trace_com)
	trace_ascii ("<", buf, res);
      if (res < 1)
	return 0;
      for (int i = 0; i < res; i++)
	{
	  unsigned char c = buf[i];
	  switch (state)
	    {
	    case S_dollar:
	      if (c != '$')
		return 0;
	      state = S_body;
	      break;
	    case S_body:
	      if (c == '#')
		state = S_c0;
	      else
		{
		  csum += c;
		  pkt[off++] = c;
		}
	      break;
	    case S_c0:
	    case S_c1:
	      {
		unsigned char d = read_hex1 (c);
		if (d > 15)
		  return 0;
		if (state == S_c0)
		  {
		    ecsum = d << 4;
		    state = S_c1;
		  }
		else
		  {
		    ecsum |= d;

		    unsigned char ack = (ecsum == csum) ? '+' : '-';
		    if (trace_com)
		      trace_ascii (">", &ack, 1);
		    if (send (sock, &ack, 1, 0) != 1)
		      return 0;
		    if (ecsum != csum)
		      {
			off = 0;
			csum = 0;
			state = S_dollar;
		      }
		    else
		      return off;
		  }
		break;
	      }
	    }
	}
    }
}

bool
remote_dsu_link::read (word addr, unsigned int nwords, unsigned char *res)
{
  unsigned char pkt[1024];
  unsigned char *p = pkt;
  int len;

  if (8 * nwords > sizeof (pkt) - 20)
    return false;

  *p++ = '$';
  *p++ = 'm';
  p = put_hex (p, addr, 4);
  *p++ = ',';
  p = put_hex (p, 4 * nwords, 2);
  len = send_recv (pkt, p - pkt);
  if (pkt[0] == 'E' || len != 8 * nwords)
    return false;
  for (int i = 0; i < 4 * nwords; i++)
    res[i] = read_hex2 (pkt + 2 * i);
  return true;
}

bool
remote_dsu_link::write (word addr, unsigned int nwords,
			const unsigned char *buf)
{
  unsigned char pkt[1024];
  unsigned char *p = pkt;
  int len;

  if (8 * nwords > sizeof (pkt) - 16)
    return false;

  *p++ = '$';
  *p++ = 'M';
  p = put_hex (p, addr, 4);
  *p++ = ',';
  p = put_hex (p, 4 * nwords, 2);
  *p++ = ':';
  for (int i = 0; i < nwords * 4; i++)
    p = put_hex (p, buf[i], 2);
  len = send_recv (pkt, p - pkt);
  if (len != 2 || pkt[0] != 'O' || pkt[1] != 'K')
    return false;
  return true;
}

#ifdef HAVE_LIBURJTAG

extern "C" {
#include <urjtag/chain.h>
#include <urjtag/cmd.h>
#include <urjtag/tap.h>
#include <urjtag/log.h>
#include <urjtag/part.h>
#include <urjtag/data_register.h>
#include <urjtag/part_instruction.h>
#include <urjtag/tap_register.h>
};

bool
jtag_dsu_link::open (void)
{
  char *params = NULL;

  chain = urj_tap_chain_alloc ();
  if (chain == nullptr)
    {
      cerr << "cannot alloc memory" << endl;
      return false;
    }
  if (urj_tap_chain_connect (chain, cable, &params) != URJ_STATUS_OK)
    {
      cerr << "cannot connect: " << urj_error_describe () << endl;
      return false;
    }
  if (urj_cmd_test_cable (chain) != URJ_STATUS_OK)
    return false;

  urj_log_state.level = URJ_LOG_LEVEL_WARNING;
  if (urj_tap_detect (chain, 0) != URJ_STATUS_OK)
    return false;
  urj_log_state.level = URJ_LOG_LEVEL_NORMAL;

  for (int i = 0; i < chain->parts->len; i++)
    {
      urj_part_t *p = chain->parts->parts[i];

      if (strcmp (p->part, "xc3s1600e") == 0)
	{
	  cerr << "JTAG: select " << p->part << endl;
	  chain->active_part = i;
	  part = p;
	}
    }

  if (part == nullptr)
    {
      cerr << "cannot find jtag tap for leon" << endl;
      urj_part_parts_print
	(URJ_LOG_LEVEL_NORMAL, chain->parts, chain->active_part);
      return false;
    }

  if (urj_part_data_register_define (part, "UD1_CMD", 35) != URJ_STATUS_OK)
    {
      cerr << "cannot create data reg: " << urj_error_describe () << endl;
      return false;
    }
  user1 = urj_part_instruction_define (part, "USER1", "000010", "UD1_CMD");
  if (user1 == nullptr)
    {
      cerr << "cannot create instruction: " << urj_error_describe () << endl;
      return false;
    }
  if (urj_part_data_register_define (part, "UD2_DATA", 33) != URJ_STATUS_OK)
    {
      cerr << "cannot create data reg: " << urj_error_describe () << endl;
      return false;
    }
  user2 = urj_part_instruction_define (part, "USER2", "000011", "UD2_DATA");
  if (user2 == nullptr)
    {
      cerr << "cannot create instruction: " << urj_error_describe () << endl;
      return false;
    }
  return true;
}

bool
jtag_dsu_link::set_cmd (word addr, int w)
{
  /* Write CMD register.  */
  part->active_instruction = user1;
  {
    char *d = user1->data_register->in->data;

    //  AHB address.
    for (int i = 0; i < 32; i++)
      d[i] = (addr >> i) & 1;
    //  Size = 10 (word)
    d[32] = 0;
    d[33] = 1;
    // W = 0
    d[34] = w;
  }

  if (urj_tap_chain_shift_instructions (chain) != URJ_STATUS_OK)
    {
      cerr << "JTAG: cannot shift IR: " << urj_error_describe () << endl;
      return false;
    }
  if (urj_tap_chain_shift_data_registers (chain, 1) != URJ_STATUS_OK)
    {
      cerr << "JTAG: cannot shift DR: " << urj_error_describe () << endl;
      return false;
    }

  /* Select DATA register.  */
  part->active_instruction = user2;
  if (urj_tap_chain_shift_instructions (chain) != URJ_STATUS_OK)
    {
      cerr << "JTAG: cannot shift IR: " << urj_error_describe () << endl;
      return false;
    }

  return true;
}

bool
jtag_dsu_link::read (word addr, unsigned int nwords, unsigned char *res)
{
  if (trace_com)
    cerr << "jtag read @" << hex8 (addr) << " " << hex2 (nwords) << endl;
  if (!set_cmd (addr, 0))
    return false;

  if (trace_com)
    cerr << "R<: " << urj_tap_register_get_string (user1->data_register->out)
	 << endl;

  for (unsigned int i = 0; i < nwords; i++)
    {
      {
	char *d = user2->data_register->in->data;
	//  SEQ = 1.
	d[32] = 1;
      }

      if (urj_tap_chain_shift_data_registers (chain, 1) != URJ_STATUS_OK)
	{
	  cerr << "JTAG: cannot shift DR: " << urj_error_describe () << endl;
	  return false;
	}

      if (trace_com)
	cerr << "R<: "
	     << urj_tap_register_get_string (user2->data_register->out);

      char *d = user2->data_register->out->data;
      word w = 0;
      for (int j = 0; j < 32; j++)
	w |= (d[j] & 1) << j;
      if (trace_com)
	cerr << ": " << hex8 (w) << endl;
      pack_be32 (res, w);
      res += 4;
    }
  return true;
}

bool
jtag_dsu_link::write (word addr, unsigned int nwords,
		      const unsigned char *buf)
{
  if (trace_com)
    cerr << "jtag write @" << hex8 (addr) << " " << hex2 (nwords) << endl;
  if (!set_cmd (addr, 1))
    return false;

  for (unsigned int i = 0; i < nwords; i++)
    {
      char *d = user2->data_register->in->data;
      {
	word w = unpack_be32 (buf);
	buf += 4;

	if (trace_com)
	  cerr << "J>: " << hex8 (w);
	for (int j = 0; j < 32; j++)
	  d[j] = (w >> j) & 1;
	//  SEQ = 1.
	d[32] = 1;
      }
      if (trace_com)
	cerr << " " << urj_tap_register_get_string (user2->data_register->in)
	     << endl;
      if (urj_tap_chain_shift_data_registers (chain, 1) != URJ_STATUS_OK)
	{
	  cerr << "JTAG: cannot shift DR: " << urj_error_describe () << endl;
	  return false;
	}
    }
  return true;
}

#endif /* HAVE_LIBURJTAG */
