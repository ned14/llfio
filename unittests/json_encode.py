#!/usr/bin/python
import json,sys,codecs
sys.stdout.write(json.dumps(codecs.getreader('ISO-8859-1')(sys.stdin).read()))
