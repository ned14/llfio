#!/usr/bin/python

spaces_per_tab = 4

import os, sys

for dirpath, dirnames, filenames in os.walk('.'):
	for filename in filenames:
		if filename[-4:]=='.hpp' or filename[-4:]=='.cpp':
			path=os.path.join(dirpath, filename)
			if os.path.exists(path+'.orig'):
				os.remove(path+'.orig')
			os.rename(path, path+'.orig')
			with open(path, 'wb') as oh:
				with open(path+'.orig', 'rb') as ih:
					print("Expanding tabs in", path, "...")
					for line in ih:
						oh.write(line.expandtabs(spaces_per_tab))
