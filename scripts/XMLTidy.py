#!/usr/bin/python3
# Tidy and repair malformed XML like that produced by CATCH under multithreaded use :)
# (C) 2015 Niall Douglas http://www.nedprod.com/
# Created: 22nd Feb 2015

import sys, lxml.etree

if len(sys.argv)<2:
  print("Usage: "+sys.argv[0]+" <input> [<output>]", file=sys.stderr)
  sys.exit(1)
inputpath=sys.argv[1]
if len(sys.argv)>2:
  outputpath=sys.argv[2]
else:
  outputpath=None

parser=lxml.etree.XMLParser(recover=True)
tree=lxml.etree.parse(inputpath, parser)
if parser.error_log:
  print("Errors during XML parsing:", file=sys.stderr)
  print(parser.error_log, file=sys.stderr)
newxml=lxml.etree.tostring(tree.getroot())

if outputpath is None:
  print(newxml)
else:
  with open(outputpath, 'wb') as oh:
    oh.write(newxml)
