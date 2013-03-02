import os, sys, platform

Import("env")
env=env.Clone()
architecture=env['VARIANT'][:env['VARIANT'].find('/')]
debugbuild="Debug" in env['VARIANT']
if env['CC']=='cl':
    if architecture=="x86":
        if   env.GetOption('sse')==1: env['CCFLAGS']+=[ "/arch:SSE" ]
        elif env.GetOption('sse')>=2: env['CCFLAGS']+=[ "/arch:SSE2" ]
        if   env.GetOption('sse')>=3: env['CPPDEFINES']+=[("__SSE3__", 1)]
        if   env.GetOption('sse')>=4: env['CPPDEFINES']+=[("__SSE4__", 1)]
    if architecture=="x86" or architecture=="x64":
        if   env.GetOption('avx')==1: env['CCFLAGS']+=[ "/arch:AVX" ]
else:
    if architecture=="x86":
        if env.GetOption('sse'):
            env['CCFLAGS']+=["-mfpmath=sse"]
            if env.GetOption('sse')>1: env['CCFLAGS']+=["-msse%s" % str(env.GetOption('sse'))]
            else: env['CCFLAGS']+=["-msse"]
    if architecture=="x86" or architecture=="x64":
        if env.GetOption('avx'):
            env['CCFLAGS']+=["-mfpmath=avx"]
            if env.GetOption('avx')>1: env['CCFLAGS']+=["-mavx%s" % str(env.GetOption('avx'))]
            else: env['CCFLAGS']+=["-mavx"]

# Am I building a debug or release build?
if debugbuild:
    env['CPPDEFINES']+=["DEBUG", "_DEBUG"]
else:
    env['CPPDEFINES']+=["NDEBUG"]

# Am I building for Windows or POSIX?
if env['CC']=='cl':
    env['CPPDEFINES']+=["WIN32", "_WINDOWS", "UNICODE", "_UNICODE"]
    env['CXXFLAGS']+=["/EHsc"]
    env['CCFLAGS']+=["/GF"]             # Eliminate duplicate strings
    env['CCFLAGS']+=["/Gy"]             # Seperate COMDATs
    env['CCFLAGS']+=["/Zi"]             # Program database debug info
    if debugbuild:
        env['CCFLAGS']+=["/Od", "/MTd"]
    else:
        env['CCFLAGS']+=["/O2", "/MT"]
    env['LIBS']+=[]
    env['LINKFLAGS']+=["/DEBUG"]                # Output debug symbols
    env['LINKFLAGS']+=["/LARGEADDRESSAWARE"]    # Works past 2Gb
    env['LINKFLAGS']+=["/DYNAMICBASE"]          # Doesn't mind being randomly placed
    env['LINKFLAGS']+=["/NXCOMPAT"]             # Likes no execute
    env['LINKFLAGS']+=["/OPT:REF"]              # Seems to puke on load on WinXP without
    env['LINKFLAGS']+=["/MANIFEST"]             # Be UAC compatible
    env['LINKFLAGSEXE']=env['LINKFLAGS'][:]

    env['LINKFLAGS']+=["/VERSION:1.00.0"]       # Version

    if not debugbuild:
        env['LINKFLAGS']+=["/OPT:ICF"]  # Eliminate redundants
else:
    env['CPPDEFINES']+=["DISABLE_SYMBOLMANGLER"] # libstdc++ doesn't have emplace()
    env['CCFLAGS']+=["-fstrict-aliasing", "-fargument-noalias", "-Wstrict-aliasing"]
    env['CCFLAGS']+=["-Wall", "-Wno-unused"]
    if debugbuild:
        env['CCFLAGS']+=["-O0", "-g"]
    else:
        env['CCFLAGS']+=["-O2", "-g"]
    env['CXXFLAGS']+=["-std=c++0x"]
    env['LINKFLAGS']+=[]
    env['LINKFLAGSEXE']=env['LINKFLAGS'][:]

outputs={}

# Build the NiallsCPP11Utilities DLL
sources = ["ErrorHandling.cpp", "MappedFileInfo.cpp", "StaticTypeRegistry.cpp"]
if "DISABLE_SYMBOLMANGLER" not in env['CPPDEFINES']: sources.append("SymbolMangler.cpp")
libobjects = env.SharedObject(sources, CPPDEFINES=env['CPPDEFINES']+["NIALLSCPP11UTILITIES_DLL_EXPORTS"])
if env.GetOption("static"):
    mylib = env.StaticLibrary("NiallsCPP11Utilities", source = libobjects)
    myliblib = mylib
else:
    mylib = env.SharedLibrary("NiallsCPP11Utilities", source = libobjects)
    if env['CC']=='cl': env.AddPostAction(mylib, 'mt.exe -nologo -manifest ${TARGET}.manifest -outputresource:$TARGET;2')
    myliblib = mylib
    if sys.platform=='win32':
        myliblib=mylib[1]
outputs['mylib']=(mylib, sources)

# Unit tests
sources = [ "unittests.cpp" ]
objects = env.Object("unittests", source = sources) # + [myliblib]
testlibs=[myliblib]
testprogram_cpp = env.Program("unittests", source = objects, LINKFLAGS=env['LINKFLAGSEXE'], LIBS = env['LIBS'] + testlibs)
outputs['unittests']=(testprogram_cpp, sources)

Return("outputs")
