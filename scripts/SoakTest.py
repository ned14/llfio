#!/usr/bin/python
# Repeatedly runs a process until it crashes
# (C) 2014 Niall Douglas
# File created: Feb 2014

import subprocess, sys

if len(sys.argv)<2:
    raise Exception("Need to specify a process to run!")

n=1
while True:
    print("\n"+str(n)+": Running process "+sys.argv[1]+" ...")
    subprocess.check_call(sys.argv[1])
    n=n+1
    
