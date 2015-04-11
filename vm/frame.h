#ifndef FRAME_H
#define FRAME_H

long long lookuptable[16];
struct fte{
  void* virtualAddress;
  int t_id;
}
fte frametable[1024];
