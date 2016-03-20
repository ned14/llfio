#!/usr/bin/python3

import sys
from urllib.request import urlopen
from lxml import etree
from lxml.html import parse

if len(sys.argv)<3:
  print(sys.argv[0], "<output html file> <url>")
  sys.exit(1)
outfile=sys.argv[1]
url=sys.argv[2]

p=parse(urlopen(url)).getroot()
p.make_links_absolute(url)
matrix=p.xpath("//table[@id='configuration-matrix']")
matrixrows=matrix[0].xpath("tr")

# First <tr> is compilers, and colspan on first td is number of horizontal qualifiers
# Second <tr> first <td> is each build target, and rowspan is number of vertical qualifiers
columntocompiler=[]
for x in matrixrows[0]:
  for n in range(0, int(x.get("colspan"))):
    columntocompiler.append(x.text)
matrixrows=matrixrows[1:]
print(columntocompiler)

# Essentially we column collapse the matrix into HTML. Quite straightforward.
class Platform:
  def __init__(self, name):
    self.name=name
    self.rows=[]

platforms=[]
rowspan=0
for row in matrixrows:
  cols=row.xpath("td")
  outrow=""
  count=0
  for x in range(0, len(cols)):
    if x==0 and rowspan==0:
      rowspan=int(cols[x].get("rowspan"))
      platform=Platform(cols[x].text)
      platforms.append(platform)
      count+=0.5
    col=cols[x]
    atags=col.xpath("div/a")
    imgs=col.xpath("div/img")
    if len(atags)==0 and len(imgs)==0:
      col.set('valign', 'top')
      outrow+=etree.tostring(col).decode('utf-8')
    elif len(atags)!=0 and len(imgs)==0:
      # Format: <div><a class="model-link inside" href="CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=linux64-gcc-clang/"><img height="24" alt="Success" width="24" src="/static/5585b9ed/images/24x24/blue.png" tooltip="Success" /></a></div>
      #col=col[:]
      # Replace img src with something dynamic
      # Form: https://ci.nedprod.com/job/Maidsafe%20CRUX/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/badge/icon
      joburl=atags[0].get("href")
      imgs=atags[0].xpath("img")
      del imgs[0].attrib["width"]
      del imgs[0].attrib["height"]
      imgs[0].set('src', joburl+"badge/icon")
      imgs[0].set('align', "top")
      # Insert target name just inside end of div
      col.getchildren()[-1].getchildren()[-1].tail=" "+columntocompiler[len(columntocompiler)-(len(cols)-x)]
      outrow+=etree.tostring(col).decode('utf-8')
      count+=1
  if count==0.5:
    platform.rows.append(outrow[:outrow.find("</td>")+5])
  elif count:
    platform.rows.append(outrow)
  else:
    platform.rows.append("<td/>")
  rowspan-=1

with open(outfile, 'wt') as oh:
  oh.write('<html><body><table id="configuration-matrix" width="100%" border="1">\n')
  for platform in platforms:
    for y in range(0, len(platform.rows)):
      oh.write('  <tr>\n')
      oh.write('    '+platform.rows[y]+'\n')
      oh.write('  </tr>\n')
  oh.write('</table></body></html>\n')

