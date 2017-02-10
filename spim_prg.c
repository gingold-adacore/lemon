typedef unsigned int word;

struct spim
{
  volatile unsigned int config;
  volatile unsigned int control;
  volatile unsigned int status;
  volatile unsigned int rx;
  volatile unsigned int tx;
};

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
  SPI_FLASH_RES  = 0xab,
};

enum spim_flash_sr
{
  SPI_FLASH_SR_WIP = (1 << 0),
  SPI_FLASH_SR_WEL = (1 << 1),
  SPI_FLASH_SR_SRWD = (1 << 7)
};

static word spim_rxtx (struct spim *spim, word cmd);
static void spim_space (struct spim *spim);
static int spim_start (struct spim *spim);
static void spim_done (unsigned int status) __attribute__ ((noreturn));

void
_start (struct spim *spim, unsigned char *buf, word file_len)
{
  word addr = 0;

  if (!spim_start (spim))
    spim_done (1);

  while (1)
    {
      unsigned len;
      unsigned i;
      word rx;

      if (file_len >= 256)
	len = 256;
      else
	len = file_len;
      if (len == 0)
	break;

      rx = spim_rxtx (spim, SPI_FLASH_WREN);
      spim_space (spim);

      spim_rxtx (spim, SPI_FLASH_PP);
      //  24 bit address
      spim_rxtx (spim, (addr >> 16) & 0xff);
      spim_rxtx (spim, (addr >> 8) & 0xff);
      spim_rxtx (spim, (addr >> 0) & 0xff);
      for (i = 0; i < len; i++)
	spim_rxtx (spim, buf[i] & 0xff);
      spim_space (spim);

      spim_rxtx (spim, SPI_FLASH_RDSR);
      while ((spim_rxtx (spim, 0) & SPI_FLASH_SR_WIP) != 0)
	;
      spim_space (spim);

      addr += len;
      file_len -= len;
      buf += len;
    }

  spim_rxtx (spim, SPI_FLASH_WRDI);
  spim_space (spim);
  spim->control = 0;
  spim_done (0);
}

static word
spim_rxtx (struct spim *spim, word cmd)
{
  spim->tx = cmd;
  while (!(spim->status & SPIM_DONE))
    ;
  word res = spim->rx;
  spim->status = SPIM_DONE;
  return res;
}

static int
spim_start (struct spim *spim)
{
  word rx;
  word status = spim->status;

  if (!(status & SPIM_INIT))
    return 0;
  if (status & (SPIM_BUSY | SPIM_DONE))
    return 0;

  spim->control = SPIM_USRC | SPIM_CSN;
  //  Just for clocking.
  rx = spim_rxtx (spim, 0);
  spim->control = SPIM_USRC;

  return 1;
}

static void
spim_space (struct spim *spim)
{
  spim->control = SPIM_USRC | SPIM_CSN;
  //  Just for clocking.
  spim_rxtx (spim, 0);
  spim->control = SPIM_USRC;
}

static void
spim_done (unsigned int status)
{
  while (1)
    asm volatile ("mov %0, %%o0; ta 1" : : "r" (status));
}
