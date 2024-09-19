#!/usr/bin/env python3

from pwn import *
import sys

elf = ELF(sys.argv[1])
move_func_got = []
print("main =", hex(elf.symbols['main']))
# print("{:<12s} {:<10s} {:<10s}".format("Func", "GOT Offset", "Symbol Offset"))
for s in [ f"move_{i}" for i in range(1200)]:
   if s in elf.got:
      # print("{:<12s} {:<10x} {:<10x}".format(s, elf.got[s], elf.symbols[s]))
      move_func_got.append(elf.got[s])
print(move_func_got)