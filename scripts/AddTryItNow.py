#!/usr/bin/python

import glob, os, sys

if not os.path.exists("html/afio/try_it_now.html"):
    sys.exit(0)
with open("html/afio/try_it_now.html", "rb") as ih:
    try_it_now=ih.read()

def patch(i):
    with open(i, "r+b") as ih:
        t=ih.read();
        it1=0
        while 1:
            it1=t.find('<div class="spirit-nav">', it1)
            if it1==-1: break
            it1=it1+24
            t=t[:it1]+try_it_now+t[it1:]
        ih.seek(0)
        ih.truncate(0)
        ih.write(t)

patch("html/afio.html")
for i in glob.glob("html/afio/*.html"):
    patch(i)
for i in glob.glob("html/afio/*/*.html"):
    patch(i)
for i in glob.glob("html/afio/*/*/*.html"):
    patch(i)
