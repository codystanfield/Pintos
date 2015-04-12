#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include "kernel/syscall.h"

int
main (int argc, char **argv)
{
  int i;

  //for (i = 0; i < 2; i++){
    //printf("IMA FORKING MY PROCESS.\n");
    exec("insult");
//  }
  //printf ("\n");

  return EXIT_SUCCESS;
}
