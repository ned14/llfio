#!/usr/bin/python
import json,sys
sys.stdout.write(json.dumps(codecs.getreader('ascii')(sys.stdin).read()))
