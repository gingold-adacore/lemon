#include <stddef.h>
#include <signal.h>
#include <unistd.h>

volatile int user_stop;

static void
sig_handler (int sig)
{
  user_stop = 1;
}

void
install_handler (void)
{
  struct sigaction act;

  act.sa_handler = sig_handler;
  act.sa_flags = SA_RESTART;
  sigemptyset (&act.sa_mask);

  sigaction (SIGINT, &act, NULL);
}
