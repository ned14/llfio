#!/usr/bin/python
import json,sys,codecs
sys.stdout.write(json.dumps(codecs.getreader('ascii')(sys.stdin).read()))
