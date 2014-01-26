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
if os.path.exists("html/afio"):
    shutil.rmtree("html/afio", True)
os.mkdir("generated")

cmd = doxygen_xml2qbk_cmd
cmd = cmd + " --xml %s"
cmd = cmd + " --start_include boost/afio/"
cmd = cmd + " --convenience_header_path ../../../boost/afio/"
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
    os.system(cmd % (i, "class_"+path2identifier("doxy/doxygen_output/xml/classboost_1_1afio_1_1", i)));
# Add all structs
for i in glob.glob("doxy/doxygen_output/xml/structboost_1_1afio_1_1*.xml"):
    os.system(cmd % (i, "struct_"+path2identifier("doxy/doxygen_output/xml/structboost_1_1afio_1_1", i)));
# Add all namespaces
for i in glob.glob("doxy/doxygen_output/xml/namespaceboost_1_1*.xml"):
    os.system(cmd % (i, "namespace_"+path2identifier("doxy/doxygen_output/xml/namespaceboost_1_1", i)));
# Add all groups
for i in glob.glob("doxy/doxygen_output/xml/group__*.xml"):
    os.system(cmd % (i, "group_"+path2identifier("doxy/doxygen_output/xml/group__", i)));
    
# Patch broken index term generation
for i in glob.glob("generated/struct_async_data_op_req_3_01*.qbk"):
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
        si1=t.find("[section:async_data_op_req<")
        if si1!=-1:
            si1+=26;
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
            postfix=postfix.replace(",", "_")
            postfix=postfix.replace(" ", "")
            t=t[:si1]+postfix+t[si2:]
        ih.seek(0)
        ih.truncate(0)
        ih.write(t)

# Use either bjam or b2 or ../../../b2 (the last should be done on Release branch)
os.system("..\\..\\..\\b2.exe") 
