#! /usr/bin/env python
# -*- coding: utf-8 -*-
# ===========================================================================
#  Use, modification and distribution is subject to the Boost Software License,
#  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
# ============================================================================

import os, sys, glob, shutil

os.chdir(os.path.dirname(sys.argv[0]))

if 'DOXYGEN' in os.environ:
    doxygen_cmd = os.environ['DOXYGEN']
else:
    doxygen_cmd = 'doxygen'

if 'DOXYGEN_XML2QBK' in os.environ:
    doxygen_xml2qbk_cmd = os.environ['DOXYGEN_XML2QBK']
else:
    doxygen_xml2qbk_cmd = 'doxygen_xml2qbk'

if os.path.exists("generated"):
    shutil.rmtree("generated", True)
if os.path.exists("disqus_identifiers"):
    shutil.rmtree("disqus_identifiers", True)
if os.path.exists("html/afio"):
    shutil.rmtree("html/afio", True)
if os.path.exists("doxy/doxygen_output"):
    shutil.rmtree("doxy/doxygen_output", True)
os.mkdir("generated")
os.mkdir("disqus_identifiers")
os.mkdir("doxy/doxygen_output")

cmd = doxygen_xml2qbk_cmd
cmd = cmd + " --xml %s"
cmd = cmd + " --start_include boost/afio/"
cmd = cmd + " --convenience_header_path ../../include/boost/afio/"
cmd = cmd + " --convenience_headers afio.hpp"
cmd = cmd + " --skip_namespace boost::afio::"
cmd = cmd + " --copyright copyright_block.qbk"
#cmd = cmd + " --output_style alt
cmd = cmd + ' --output_member_variables ""'
cmd = cmd + " > generated/%s.qbk"

def call_doxygen():
    os.chdir("doxy");
    os.system(doxygen_cmd)
    os.chdir("..")
    
def path2identifier(prefix, i):
    i=i[len(prefix):-4]
    return i.replace('__', '_')

call_doxygen()

# Add all classes
for i in glob.glob("doxy/doxygen_output/xml/classboost_1_1afio_1_1*.xml"):
    if "detail_1_1" not in i:
        os.system(cmd % (i, "class_"+path2identifier("doxy/doxygen_output/xml/classboost_1_1afio_1_1", i)));
# Add all structs
for i in glob.glob("doxy/doxygen_output/xml/structboost_1_1afio_1_1*.xml"):
    if "detail_1_1" not in i:
        os.system(cmd % (i, "struct_"+path2identifier("doxy/doxygen_output/xml/structboost_1_1afio_1_1", i)));
# Add all namespaces
#for i in glob.glob("doxy/doxygen_output/xml/namespaceboost_1_1afio_1_1*.xml"):
#    if "detail" not in i:
#        os.system(cmd % (i, "namespace_"+path2identifier("doxy/doxygen_output/xml/namespaceboost_1_1afio_1_1", i)));
# Add all groups
for i in glob.glob("doxy/doxygen_output/xml/group__*.xml"):
    if "detail_1_1" not in i:
        os.system(cmd % (i, "group_"+path2identifier("doxy/doxygen_output/xml/group__", i)));
    
# Patch broken index term generation
for i in glob.glob("generated/struct_io_req_3_01*.qbk"):
    with open(i, "r+b") as ih:
        t=ih.read();
        # Fix erroneous expansions of nested types
        t=t.replace("</primary></indexterm><indexterm><primary>", "::")
        # Fix failure to escape < and >
        it1=t.find("<indexterm><primary>")+20
        it2=t.find("</primary></indexterm>", it1)
        indexterm=t[it1:it2]
        indexterm=indexterm.replace("<", "&lt;")
        indexterm=indexterm.replace(">", "&gt;")
        t=t[:it1]+indexterm+t[it2:]
        # Fix failure to collapse section ids
        si1=t.find("[section:io_req<")
        if si1!=-1:
            si1+=15;
            si2=si1+1
            count=1
            while count>0:
                if t[si2]=='<': count+=1
                elif t[si2]=='>': count-=1
                si2+=1
            postfix=t[si1:si2]
            postfix=postfix.replace("<", "_")
            postfix=postfix.replace(">", "_")
            postfix=postfix.replace(",", "_")
            postfix=postfix.replace(" ", "")
            t=t[:si1]+postfix+t[si2:]
        ih.seek(0)
        ih.truncate(0)
        ih.write(t)
for i in glob.glob("generated/class_enqueued_task*.qbk"):
    with open(i, "r+b") as ih:
        t=ih.read();
        # Fix failure to escape < and >
        it1=t.find("<indexterm><primary>")+20
        it2=t.find("</primary></indexterm>", it1)
        indexterm=t[it1:it2]
        indexterm=indexterm.replace("<", "&lt;")
        indexterm=indexterm.replace(">", "&gt;")
        t=t[:it1]+indexterm+t[it2:]
        # Fix failure to collapse section ids
        si1=t.find("[section:enqueued_task<")
        if si1!=-1:
            si1+=22;
            si2=si1+1
            count=1
            while count>0:
                if t[si2]=='<': count+=1
                elif t[si2]=='>': count-=1
                si2+=1
            postfix=t[si1:si2]
            postfix=postfix.replace("<", "_")
            postfix=postfix.replace(">", "_")
            postfix=postfix.replace("(", "_")
            postfix=postfix.replace(")", "_")
            postfix=postfix.replace(",", "_")
            postfix=postfix.replace(" ", "")
            t=t[:si1]+postfix+t[si2:]
        ih.seek(0)
        ih.truncate(0)
        ih.write(t)
for i in glob.glob("generated/class_future*.qbk"):
    with open(i, "r+b") as ih:
        t=ih.read();
        # Fix failure to escape < and >
        it1=t.find("<indexterm><primary>")+20
        it2=t.find("</primary></indexterm>", it1)
        indexterm=t[it1:it2]
        indexterm=indexterm.replace("<", "&lt;")
        indexterm=indexterm.replace(">", "&gt;")
        t=t[:it1]+indexterm+t[it2:]
        # Fix failure to collapse section ids
        si1=t.find("[section:future<")
        if si1!=-1:
            si1+=15;
            si2=si1+1
            count=1
            while count>0:
                if t[si2]=='<': count+=1
                elif t[si2]=='>': count-=1
                si2+=1
            postfix=t[si1:si2]
            postfix=postfix.replace("<", "_")
            postfix=postfix.replace(">", "_")
            postfix=postfix.replace("(", "_")
            postfix=postfix.replace(")", "_")
            postfix=postfix.replace(",", "_")
            postfix=postfix.replace(" ", "")
            t=t[:si1]+postfix+t[si2:]
        ih.seek(0)
        ih.truncate(0)
        ih.write(t)
        
# Patch failure to put in-class enums in the right section
for i in glob.glob("generated/class_*.qbk")+glob.glob("generated/struct_*.qbk"):
    with open(i, "r+b") as ih:
        t=ih.readlines()
    sections=[]
    stack=[]
    n=-1
    for line in t:
      n+=1
      if line[:9]=="[section:":
        stack.append(n)
      elif line[:9]=="[endsect]":
        if len(stack)==1:
          sections.append((stack[0], n))
        stack.pop()
    if len(sections)>2:
      print("There are more than two standalone sections in "+i+". This is bad.") 
    elif len(sections)>1:
      print("In "+i+" relocating "+t[sections[0][0]]+" into "+t[sections[1][0]])
      t.insert(sections[0][0], t.pop(sections[1][0]))
      with open(i, "w+b") as ih:
          ih.writelines(t)

def write_disqus_fragment(name, title):
    with open("disqus_identifiers/"+name+".html", "wt") as identh:
      identh.write("""<script type="text/javascript">
var disqus_identifier = '"""+name+"""';
var disqus_title = 'Boost.AFIO """+title+"""';
</script>
""")

# Patch all reference sections with Disqus commenting
for i in glob.glob("generated/*.qbk"):
    with open(i, "r+b") as ih:
        t=ih.readlines()
    n=-1
    skip=0
    for line in t:
      n+=1
      if skip:
          skip=skip-1
          continue
      if line[:9]=="[section:":
        name=line[9:-2];
        if name[-1]==']': name=name[:-1]
        firstspace=name.find(' ')
        if name[firstspace-1]=='<':
          firstspace=name.find('>', firstspace)+1
        title=name[firstspace+1:].replace('<', '&lt;').replace('>', '&gt;')
        name=name[:firstspace].replace('<', '_').replace('>', '_').replace(' ', '-')
        write_disqus_fragment(name, title)
        t.insert(n+1, """'''<?dbhtml-include href="disqus_identifiers/"""+name+""".html"?>'''
""")
      elif line[:9]=="[endsect]":
        t.insert(n, """'''<?dbhtml-include href="disqus_comments.html"?>'''
""")
        skip=2
    with open(i, "w+b") as ih:
        ih.writelines(t)
        
# Generate disqus html fragments for these sections
disqus_sections=[
    ("introduction", "Introduction"),
    ("design_rationale", "Design Introduction and Rationale"),
    ("overview", "Single page cheat sheet"),
    ("compilation", "Compilation"),
    ("hello_world", "Hello World, asynchronously!"),
    ("file_concat", "Concatenating files"),
    ("atomic_logging", "Achieving atomicity on the filing system"),
    ("filesystem_races", "Handling races on the filing system"),
    ("so_what", "What benefit does asynchronous file i/o bring me? A demonstration of AFIO's power")
]
for name, title in disqus_sections:
    write_disqus_fragment(name, title)
        
# Use either bjam or b2 or ../../../b2 (the last should be done on Release branch)
#os.system("..\\..\\b2.exe") 
