import os, sys, platform, subprocess
	
architectures = ["generic"]

env = Environment()
#print env['TOOLS']

architecture="generic"
ARMcrosscompiler=False
if env['CC']=='cl':
	# We're on windows
	if os.environ.has_key('LIBPATH'):
		if not env.GetOption('force32') and -1!=os.environ['LIBPATH'].find("\\amd64"):
			architecture="x64"
		elif -1!=os.environ['LIBPATH'].find("\\arm"):
			architecture="ARMv7"
		else:
			architecture="x86"
		architectures.append(architecture)
else:
	# We're on POSIX, so ask the compiler what it thinks it's building
	output=''
	try:
		output=subprocess.check_output([env['CXX'], "--target-help"])
	except:
		try:
			output=subprocess.check_output([env['CXX'], "--version"])
		except:
			output=platform.machine()
	if 'armelf' in output:
		architectures+=["ARMv7"]
		architecture="ARMv7"
	# Choose Intel in preference as otherwise it's probably a cross-compiler
	if 'x64' in output or 'x86_64' in output:
		architectures+=["x86", "x64"]
		architecture="x64"
	else:
		for a in ['i386', 'i486', 'i586', 'i686']:
			if a in output:
				architectures+=["x86"]
				architecture="x86"
				break
	# If not ARMv7 support, see if there is a cross-compiler
	if "ARMv7" not in architectures:
		output=''
		try:
			output=subprocess.check_output(['arm-linux-gnueabi-g++', "--target-help"])
		except:
			try:
				output=subprocess.check_output(['arm-linux-gnueabi-g++', "--version"])
			except:
				output=platform.machine()
		if 'armelf' in output:
			architectures+=["ARMv7"]
			ARMcrosscompiler=True
print "*** Architectures detected as being available:", architectures
print "*** Minimum architecture is:", architecture

if sys.platform=="win32" and 'INCLUDE' not in os.environ:
    env = Environment(tools=['mingw', 'msvs'])
#print env['TOOLS']
AddOption('--postfix', dest='postfix', nargs=1, default='_test', help='appends a string to the DLL name')
AddOption('--debugbuild', dest='debug', action='store_const', const=1, help='enable debug build')
AddOption('--optdebugbuild', dest='debug', action='store_const', const=2, help='enable optimised debug build')
AddOption('--static', dest='static', nargs='?', const=True, help='build a static library rather than shared library')
AddOption('--useclang', dest='useclang', nargs=1, type='str', help='use clang if it is available')
AddOption('--usegcc', dest='usegcc', nargs=1, type='str', help='use gcc if it is available')
AddOption('--usethreadsanitize', dest='usethreadsanitize', nargs='?', const=True, help='use thread sanitiser')
AddOption('--usegcov', dest='usegcov', nargs='?', const=True, help='use GCC coverage')
AddOption('--force32', dest='force32', help='force 32 bit build on 64 bit machine')
AddOption('--archs', dest='archs', nargs=1, type='str', default='min', help='which architectures to build, comma separated. all means all. Defaults to min.')
if 'x86' in architectures:
	AddOption('--sse', dest='sse', nargs=1, type='int', default=2, help='set SSE used (0-4) on 32 bit x86. Defaults to 2 (SSE2).')
	AddOption('--avx', dest='avx', nargs=1, type='int', default=0, help='set AVX used (0-2) on x86/x64. Defaults to 0 (No AVX).')
if 'ARMv7' in architectures:
	AddOption('--fpu', dest='fpu', nargs=1, type='str', default='neon', help='sets FPU used on ARMv7. Defaults to neon.')
	AddOption('--thumb', dest='thumb', nargs='?', const=True, help='generate ARMv7 Thumb instructions instead of normal.')
if env.GetOption('archs')!='min' and env.GetOption('archs')!='all':
	archs=env.GetOption('archs').split(',')
	for arch in archs:
		assert arch in architectures
	architectures=archs
if architecture=='x64' and env.GetOption('force32'): architecture='x86'

# Force scons to always use absolute paths in everything (helps debuggers to find source files)
env['CCCOM']   =    env['CCCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['SHCCCOM'] =  env['SHCCCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['CXXCOM']  =   env['CXXCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['SHCXXCOM']= env['SHCXXCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['CPPPATH']=[]
env['CPPDEFINES']=[]
env['CPPFLAGS']=[]
env['CCFLAGS']=[]
env['CXXFLAGS']=[]
env['LIBS']=[]
env['LIBPATH']=[]
env['RPATH']=[]
env['LINKFLAGS']=[]

# Am I in a 32 or 64 bit environment? Note that not specifying --sse doesn't set any x86 or x64 specific options
# so it's good to go for ANY platform
if sys.platform=="win32" and 'INCLUDE' in os.environ:
    # Even the very latest scons still screws this up :(
    env['ENV']['INCLUDE']=os.environ['INCLUDE']
    env['ENV']['LIB']=os.environ['LIB']
    env['ENV']['PATH']=os.environ['PATH']
else:
    if sys.platform=="win32":
        env['ENV']['PATH']=os.environ['PATH']
        env['CPPDEFINES']+=["WIN32"]
    # Test the build environment
    def CheckHaveClang(cc):
        cc.Message("Checking if clang is available ...")
        try:
            temp=env['CC']
        except:
            temp=[]
        cc.env['CC']=env.GetOption('useclang')
        result=cc.TryLink('int main(void) { return 0; }\n', '.c')
        cc.env['CC']=temp
        cc.Result(result)
        return result
    def CheckHaveGCC(cc):
        cc.Message("Checking if gcc is available ...")
        try:
            temp=env['CC']
        except:
            temp=[]
        cc.env['CC']=env.GetOption('usegcc')
        result=cc.TryLink('int main(void) { return 0; }\n', '.c')
        cc.env['CC']=temp
        cc.Result(result)
        return result
    def CheckHaveVisibility(cc):
        cc.Message("Checking for symbol visibility support ...")
        try:
            temp=cc.env['CPPFLAGS']
        except:
            temp=[]
        cc.env['CPPFLAGS']=temp+["-fvisibility=hidden"]
        result=cc.TryCompile('struct __attribute__ ((visibility("default"))) Foo { int foo; };\nint main(void) { Foo foo; return 0; }\n', '.cpp')
        cc.env['CPPFLAGS']=temp
        cc.Result(result)
        return result
    def CheckHaveOpenMP(cc):
        cc.Message("Checking for OpenMP support ...")
        try:
            temp=cc.env['CPPFLAGS']
        except:
            temp=[]
        cc.env['CPPFLAGS']=temp+["-fopenmp"]
        result=cc.TryCompile('#include <omp.h>\nint main(void) { return 0; }\n', '.cpp')
        cc.env['CPPFLAGS']=temp
        cc.Result(result)
        return result
    def CheckHaveCPP11Features(cc):
        cc.Message("Checking if can enable C++11 features ...")
        try:
            temp=cc.env['CPPFLAGS']
        except:
            temp=[]
        cc.env['CPPFLAGS']=temp+["-std=c++11"]
        result=cc.TryCompile('struct Foo { static const int gee=5; Foo(const char *) { } Foo(Foo &&a) { } };\nint main(void) { Foo foo(__func__); static_assert(Foo::gee==5, "Death!"); return 0; }\n', '.cpp')
        cc.env['CPPFLAGS']=temp
        cc.Result(result)
        return result
    def CheckHaveBoost(cc):
        cc.Message("Checking for Boost C++ libraries ...")
        try:
            temp=cc.env['CPPFLAGS']
        except:
            temp=[]
        result=cc.TryCompile('#include "boost/mpl/vector.hpp"\n', '.cpp')
        cc.Result(result)
        return result
    conf=Configure(env, { "CheckHaveClang" : CheckHaveClang, "CheckHaveGCC" : CheckHaveGCC, "CheckHaveVisibility" : CheckHaveVisibility, "CheckHaveOpenMP" : CheckHaveOpenMP, "CheckHaveCPP11Features" : CheckHaveCPP11Features, "CheckHaveBoost" : CheckHaveBoost } )
    if env.GetOption('useclang') and conf.CheckHaveClang():
        env['CC']="clang"
        env['CXX']=env.GetOption('useclang')
    if env.GetOption('usegcc') and conf.CheckHaveGCC():
        env['CC']="gcc"
        env['CXX']=env.GetOption('usegcc')
    if env.GetOption('usethreadsanitize'):
        env['CPPFLAGS']+=["-fsanitize=thread", "-fPIC"]
        env['LINKFLAGS']+=["-fsanitize=thread"]
        env['LIBS']+=["tsan"]
    if env.GetOption('usegcov'):
        env['CPPFLAGS']+=["-fprofile-arcs", "-ftest-coverage"]
        env['LIBS']+=["gcov"]
        env['CPPDEFINES']+=["NDEBUG"] # Disables some code only there to aid debugger display
    if not conf.CheckLib("rt", "clock_gettime") and not conf.CheckLib("c", "clock_gettime"):
        print "WARNING: Can't find clock_gettime() in librt or libc, code may not fully compile if your system headers say that this function is available"
    if conf.CheckHaveVisibility():
        env['CPPFLAGS']+=["-fvisibility=hidden"]        # All symbols are hidden unless marked otherwise
        env['CXXFLAGS']+=["-fvisibility-inlines-hidden" # All inlines are always hidden
                             ]
    else:
        print "Disabling -fvisibility support"
    if conf.CheckHaveOpenMP():
        env['CPPFLAGS']+=["-fopenmp"]
        env['LINKFLAGS']+=["-fopenmp"]
    else:
        print "Disabling OpenMP support"

    #if conf.CheckHaveCPP11Features():
    #    env['CXXFLAGS']+=["-std=c++11"]
    #else:
    #    print "Disabling C++11 support"
	
    boostpath=os.path.abspath(os.path.join(os.getcwd(), "boost"))
    while not os.path.exists(boostpath):
    	#print(boostpath)
    	boostpath=os.path.dirname(os.path.dirname(boostpath))
    	if len(boostpath)<4: break
    	boostpath=os.path.join(boostpath, "boost")
    if len(boostpath)>4 and os.path.exists(boostpath):
    	env['CPPPATH']+=[boostpath]
    	env['LIBPATH']+=[os.path.join(boostpath, 'stage', 'lib')]
    	env['RPATH']+=[os.path.join(boostpath, 'stage', 'lib')]
    if not conf.CheckHaveBoost():
    	print("ERROR: I need the Boost libraries, either in the system or in a boost directory just above mine")
    	sys.exit(1)

    env=conf.Finish()

# Build
mylibrary=None
buildvariants={}
for arch in architectures:
    for buildtype in ["Debug", "Release"]:
        env['VARIANT']=arch+"/"+buildtype
        Export(importedenv=env, ARMcrosscompiler=ARMcrosscompiler)
        mylibraryvariant=env.SConscript("SConscript", variant_dir=env['VARIANT'], duplicate=False)
        buildvariants[(buildtype, arch)]=mylibraryvariant

if env.GetOption('archs')=='min':
	print "*** Build variant preferred by environment is", "Debug" if env.GetOption("debug") else "Release", architecture, "using compiler", env['CC']
	mylibrary=buildvariants[("Debug" if env.GetOption("debug") else "Release", architecture)]
	#print(mylibrary)
	Default([x[0] for x in mylibrary.values()])
else:
	#print(buildvariants)
	mylibrary=[x.values()[0][0] for x in buildvariants.values()]
	#print(mylibrary)
	Default(mylibrary)

# Set up the MSVC project files
if 'win32'==sys.platform:
    includes = [ "afio/afio.hpp" ]
    variants = []
    projs = {}
    for buildvariant, output in buildvariants.items():
        variant = buildvariant[0]+'|'+("Win32" if buildvariant[1]=="x86" else buildvariant[1])
        variants+=[variant]
        for program, builditems in output.items():
            if not program in projs: projs[program]={}
            projs[program][variant]=builditems
    variants.sort()
    msvsprojs = []
    for program, items in projs.items():
        buildtargets = items.items()
        buildtargets.sort()
        #print buildtargets
        #print [str(x[1][0][0]) for x in buildtargets]
        msvsprojs+=env.MSVSProject(program+env['MSVSPROJECTSUFFIX'], srcs=items.values()[0][1], incs=includes, misc="Readme.txt", buildtarget=[x[1][0][0] for x in buildtargets], runfile=[str(x[1][0][0]) for x in buildtargets], variant=[x[0] for x in buildtargets], auto_build_solution=0)
    msvssolution = env.MSVSSolution("afio.sln", projects=msvsprojs, variant=variants)
    Depends(msvssolution, msvsprojs)
    Alias("msvcproj", msvssolution)

Return("mylibrary")
