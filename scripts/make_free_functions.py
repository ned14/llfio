#!/usr/bin/python
# Look for all static member functions marked AFIO_MAKE_FREE_FUNCTION and make them free
# (C) 2017 Niall Douglas

from __future__ import print_function
import sys, glob, re

for header in glob.glob("../include/afio/*/*.hpp"):
    with open(header, "rt") as ih:
        lines = ih.readlines()
    current_class = None
    functions_to_be_freed = []
    functions_to_be_freed_begin = -1
    functions_to_be_freed_end = -1
    for lineidx in range(0, len(lines)):
        if lines[lineidx].startswith('class'):
            current_class = lines[lineidx][6:lines[lineidx].find(':')]
            current_class = current_class.replace('AFIO_DECL', '').lstrip().rstrip()
        if 'AFIO_MAKE_FREE_FUNCTION' in lines[lineidx] and re.match('\s*AFIO_MAKE_FREE_FUNCTION', lines[lineidx]):
            function = ''
            for n in range(1, 100):
               function += lines[lineidx+n]
               if lineidx+n+1 >= len(lines):
                   print(lines[lineidx:])
                   raise Exception()
               if '{' in lines[lineidx+n+1] or ';' in lines[lineidx+n]:
                   break
            for n in range(1, 100):
               if '//!' in lines[lineidx-n] or '/*!' in lines[lineidx-n]:
                   docs = ''.join(lines[lineidx-n:lineidx])
                   # Remove any indent
                   docs = docs.replace('\n  ', '\n')[2:]
                   break
            functions_to_be_freed.append((current_class, function, docs))
        elif '// BEGIN make_free_functions.py' in lines[lineidx]:
            functions_to_be_freed_begin = lineidx
        elif '// END make_free_functions.py' in lines[lineidx]:
            functions_to_be_freed_end = lineidx
    if functions_to_be_freed:
        if functions_to_be_freed_begin != -1:
            del lines[functions_to_be_freed_begin+1:functions_to_be_freed_end]
        else:
            # Place just before the last AFIO_V2_NAMESPACE_END
            functions_to_be_freed_begin = len(lines)-1
            while not lines[functions_to_be_freed_begin].startswith('AFIO_V2_NAMESPACE_END'):
                functions_to_be_freed_begin = functions_to_be_freed_begin - 1
            lines.insert(functions_to_be_freed_begin, '// END make_free_functions.py\n\n')
            lines.insert(functions_to_be_freed_begin, '// BEGIN make_free_functions.py\n')
        idx = functions_to_be_freed_begin + 1
        for classname, function, docs in functions_to_be_freed:
            #print((classname, function))
            function = function.replace('virtual ', '')
            function = function.replace('override', '')
            function = function.replace('AFIO_HEADERS_ONLY_MEMFUNC_SPEC ', '')
            function = function.replace('AFIO_HEADERS_ONLY_VIRTUAL_SPEC ', '')
            completion_template = 'template <class CompletionRoutine>' in function
            function = function.replace('template <class CompletionRoutine> ', '')
            is_static = 'static ' in function
            function = function.replace('static ', '')
            function = function.lstrip()
            if not function.startswith('inline'):
                function = 'inline ' + function
            if completion_template:
                function = 'template <class CompletionRoutine> ' + function
            def replace(function, item):
                function = function.replace(' '+item+' ', ' '+classname+'::'+item+' ')
                function = function.replace('('+item+' ', '('+classname+'::'+item+' ')
                function = function.replace('<'+item+',', '<'+classname+'::'+item+',')
                function = function.replace('<'+item+'>', '<'+classname+'::'+item+'>')
                function = function.replace(' '+item+'>', ' '+classname+'::'+item+'>')
                return function
            function = replace(function, 'path_view_type')
            function = replace(function, 'extent_type')
            function = replace(function, 'size_type')
            function = replace(function, 'buffers_type')
            function = replace(function, 'const_buffers_type')
            function = function.replace('path_view_type()', classname+'::path_view_type()')
            function = function.replace(' io_result<', ' '+classname+'::io_result<')
            function = function.replace(' io_request<', ' '+classname+'::io_request<')
            function = function.replace('(io_request<', '('+classname+'::io_request<')
            function = function.replace('<io_state_ptr<', '<'+classname+'::io_state_ptr<')
            function = function.replace(' mode ', ' '+classname+'::mode ')
            function = function.replace(' mode::', ' '+classname+'::mode::')
            function = function.replace(' creation ', ' '+classname+'::creation ')
            function = function.replace(' creation::', ' '+classname+'::creation::')
            function = function.replace(' caching ', ' '+classname+'::caching ')
            function = function.replace(' caching::', ' '+classname+'::caching::')
            function = function.replace(' flag ', ' '+classname+'::flag ')
            function = function.replace(' flag::', ' '+classname+'::flag::')
            if function[-2] == ';':
                function=function[:-2]
            elif '}' in function:
                function = function[:function.rfind('{')] + function[function.rfind('}')+1:]
            function = function.rstrip()
            idx1 = function.rfind('const')
            idx2 = function.rfind(')')
            is_const = False
            if idx1 > idx2:
                function = function[:idx1] + function[idx1+6:]
                function = function.rstrip()
                is_const = True
            idx1 = function.rfind('= 0')
            if idx1 > idx2:
                function = function[:idx1] + function[idx1+3:]
                function = function.rstrip()
            if is_static:
                call = function
                impl = "\n{\n  return "+classname+"::"
            else:
                call = function
                idx1 = function.find('(')
                idx2 = function.rfind(')')
                function = function[:idx1+1] + (('const ' + classname) if is_const else classname) + (' &self, ' if idx1+1 != idx2 else ' &self') + function[idx1+1:]
                impl = "\n{\n  return self."
                idx1 = docs.find('\\param')
                if idx1 != -1:
                    docs = docs[:idx1] + '\param self The object whose member function to call.\n' + docs[idx1:]
            
            #print(call)
            call = re.sub(r'^.+?(?= [^ ]+\()', '', call)  # remove anything before the function call
            #print(call)
            call = re.sub(r'(?<=\) ).*', '', call)  # remove anything after the function call
            #print(call)
            call = re.sub(r'".*?"', '', call)  # remove any string literals
            #print(call)
            call = re.sub(r'\s?=.+?(?:\(.*?\))?(?=[,)])', '', call)  # remove all default initialisers
            #print(call)
            impl += re.match(r'.+\(', call).group(0).lstrip()
            first = True
            for par in re.finditer(r'\w*[,)]', call):
                if not first:
                    impl += ' '
                else:
                    first = False
                p = par.group(0)
                if p == ')':
                    impl += ')'
                else:
                    impl += 'std::forward<decltype('+p[:-1]+')>('+p[:-1]+')'+p[-1]
            impl += ";\n}\n"
            lines.insert(idx, function+impl)
            lines.insert(idx, docs)
            idx = idx + 2
        with open(header, "wt") as oh:
            oh.writelines(lines)
        
