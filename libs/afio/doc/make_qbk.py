#! /usr/bin/env python
# -*- coding: utf-8 -*-
# ===========================================================================
#  Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
#  Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
#  Copyright (c) 2009-2012 Mateusz Loskot (mateusz@loskot.net), London, UK
# 
#  Use, modification and distribution is subject to the Boost Software License,
#  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
# ============================================================================

import os, sys

os.chdir(os.path.dirname(sys.argv[0]))

if 'DOXYGEN' in os.environ:
    doxygen_cmd = os.environ['DOXYGEN']
else:
    doxygen_cmd = 'doxygen'

if 'DOXYGEN_XML2QBK' in os.environ:
    doxygen_xml2qbk_cmd = os.environ['DOXYGEN_XML2QBK']
else:
    doxygen_xml2qbk_cmd = 'doxygen_xml2qbk'

if not os.path.exists("generated"):
    os.mkdir("generated")

cmd = doxygen_xml2qbk_cmd
cmd = cmd + " --xml doxy/doxygen_output/xml/%s.xml"
cmd = cmd + " --start_include boost/afio/"
cmd = cmd + " --convenience_header_path ../../../boost/afio/"
cmd = cmd + " --convenience_headers geometry.hpp,geometries/geometries.hpp,multi/multi.hpp"
cmd = cmd + " --skip_namespace boost::geometry::"
cmd = cmd + " --copyright src/copyright_block.qbk"
cmd = cmd + " > generated/%s.qbk"

def call_doxygen():
    os.chdir("doxy");
    os.system(doxygen_cmd)
    os.chdir("..")

def group_to_quickbook(section):
    os.system(cmd % ("group__" + section.replace("_", "__"), section))

def struct_to_quickbook(section):
    p = section.find("::")
    if p > 0:
        ns = section[:p]
        item = section[p+2:]
        section = ns + "_" + item
        os.system(cmd % ("structboost_1_1afio_1_1" + ns.replace("_", "__") + "_1_1" + item.replace("_", "__"), section))
    else:
        os.system(cmd % ("structboost_1_1afio_1_1" + section.replace("_", "__"), section))

def class_to_quickbook(section):
    p = section.find("::")
    if p > 0:
        ns = section[:p]
        item = section[p+2:]
        section = ns + "_" + item
        os.system(cmd % ("classboost_1_1afio_1_1" + ns.replace("_", "__") + "_1_1" + item.replace("_", "__"), section))
    else:
        os.system(cmd % ("classboost_1_1afio_1_1" + section.replace("_", "__"), section))

call_doxygen()

core_c = [ "async_file_io_dispatcher_base", "thread_pool", "detail::async_io_handle" ]
core_s = [ "async_data_op_req", "async_io_op", "async_path_op_req" ]


for i in core_c:
    class_to_quickbook(i)
for i in core_s:
    struct_to_quickbook(i)

# Use either bjam or b2 or ../../../b2 (the last should be done on Release branch)
os.system("..\\..\\..\\b2.exe") 
