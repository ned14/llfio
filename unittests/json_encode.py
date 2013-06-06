#!/usr/bin/python
import json,sys
sys.stdout.write(json.dumps(sys.stdin.read()))
