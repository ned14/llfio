import os, sys, platform, subprocess
	
architectures = ["generic", "x86", "x64", "ARMv7"]

env = Environment()
#print env['TOOLS']
if sys.platform=="win32" and 'INCLUDE' not in os.environ:
    env = Environment(tools=['mingw', 'msvs'])
#print env['TOOLS']
AddOption('--postfix', dest='postfix', nargs=1, default='_test', help='appends a string to the DLL name')
AddOption('--debugbuild', dest='debug', nargs='?', const=True, help='enable debug build')
AddOption('--static', dest='static', nargs='?', const=True, help='build a static library rather than shared library')
AddOption('--useclang', dest='useclang', nargs='?', const=True, help='use clang if it is available')
AddOption('--usegcc', dest='usegcc', nargs='?', const=True, help='use gcc if it is available')
AddOption('--force32', dest='force32', help='force 32 bit build on 64 bit machine')
AddOption('--sse', dest='sse', nargs=1, type='int', default=1, help='set SSE used (0-4) on 32 bit x86. Defaults to 1 (SSE1).')
AddOption('--avx', dest='avx', nargs=1, type='int', default=0, help='set AVX used (0-2) on x86/x64. Defaults to 0 (No AVX).')

# Force scons to always use absolute paths in everything (helps debuggers to find source files)
env['CCCOM']   =    env['CCCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['SHCCCOM'] =  env['SHCCCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['CXXCOM']  =   env['CXXCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
env['SHCXXCOM']= env['SHCXXCOM'].replace('$CHANGED_SOURCES','$SOURCES.abspath')
architecture="generic"
env['CPPPATH']=[]
env['CPPDEFINES']=[]
env['CCFLAGS']=[]
env['CXXFLAGS']=[]
env['LIBS']=[]
env['LINKFLAGS']=[]

# Am I in a 32 or 64 bit environment? Note that not specifying --sse doesn't set any x86 or x64 specific options
# so it's good to go for ANY platform
if sys.platform=="win32" and 'INCLUDE' in os.environ:
    # Even the very latest scons still screws this up :(
    env['ENV']['INCLUDE']=os.environ['INCLUDE']
    env['ENV']['LIB']=os.environ['LIB']
    env['ENV']['PATH']=os.environ['PATH']
else:
    if sys.platform=="win32": env['ENV']['PATH']=os.environ['PATH']
    # Test the build environment
    def CheckHaveClang(cc):
        cc.Message("Checking if clang is available ...")
        try:
            temp=env['CC']
        except:
            temp=[]
        cc.env['CC']="clang"
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
        cc.env['CC']="gcc"
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
    conf=Configure(env, { "CheckHaveClang" : CheckHaveClang, "CheckHaveGCC" : CheckHaveGCC, "CheckHaveVisibility" : CheckHaveVisibility, "CheckHaveCPP11Features" : CheckHaveCPP11Features, "CheckHaveBoost" : CheckHaveBoost } )
    if env.GetOption('useclang') and conf.CheckHaveClang():
        env['CC']="clang"
        env['CXX']="clang++"
    if env.GetOption('usegcc') and conf.CheckHaveGCC():
        env['CC']="gcc"
        env['CXX']="g++"
    if not conf.CheckLib("rt", "clock_gettime") and not conf.CheckLib("c", "clock_gettime"):
        print "WARNING: Can't find clock_gettime() in librt or libc, code may not fully compile if your system headers say that this function is available"
    if conf.CheckHaveVisibility():
        env['CPPFLAGS']+=["-fvisibility=hidden"]        # All symbols are hidden unless marked otherwise
        env['CXXFLAGS']+=["-fvisibility-inlines-hidden" # All inlines are always hidden
                             ]
    else:
        print "Disabling -fvisibility support"

    #if conf.CheckHaveCPP11Features():
    #    env['CXXFLAGS']+=["-std=c++11"]
    #else:
    #    print "Disabling C++11 support"
	
    if not conf.CheckHaveBoost():
    	old=env['CPPPATH']
    	boostpath=os.path.abspath(os.path.join(os.getcwd(), "../boost"))
    	while not os.path.exists(boostpath) and len(boostpath):
    		boostpath=os.path.dirname(boostpath)
    	if len(boostpath):
        	env['CPPPATH']+=[boostpath]
    	#env['LIB']+=['../boost/stage/lib/']
    	if not conf.CheckHaveBoost():
    		print("ERROR: I need the Boost libraries, either in the system or in a boost directory just above mine")
    		sys.exit(1)

    env=conf.Finish()

# Build
mylibrary=None
buildvariants={}
for architecture in architectures:
    for buildtype in ["Debug", "Release"]:
        env['VARIANT']=architecture+"/"+buildtype
        mylibraryvariant=SConscript("SConscript", variant_dir=env['VARIANT'], duplicate=False, exports="env")
        buildvariants[(buildtype, architecture)]=mylibraryvariant
# What architecture am I on?
architecture="generic"
if env['CC']=='cl':
	# We're on windows
	if os.environ.has_key('LIBPATH'):
		if not env.GetOption('force32') and -1!=os.environ['LIBPATH'].find("\\amd64"):
			architecture="x64"
		elif -1!=os.environ['LIBPATH'].find("\\arm"):
			architecture="ARMv7"
		else:
			architecture="x86"
else:
	# We're on POSIX, so ask the compiler what it thinks it's building
	output=''
	try:
		output=subprocess.check_output([env['CC'], "--target-help"])
	except:
		try:
			output=subprocess.check_output([env['CC'], "--version"])
		except:
			output=platform.machine()
	if not env.GetOption('force32') and ('x64' in output or 'x86_64' in output):
		architecture="x64"
	elif 'ARM' in output:
		architecture="ARMv7"
	else:
		for a in ['i386', 'i486', 'i586', 'i686']:
			if a in output:
				architecture="x86"
				break

print "*** Build variant preferred by environment is", "Debug" if env.GetOption("debug") else "Release", architecture, "using compiler", env['CC']
mylibrary=buildvariants[("Debug" if env.GetOption("debug") else "Release", architecture)]
Default([x[0] for x in mylibrary.values()])
mylibrary=mylibrary['mylib'][0]

# Set up the MSVC project files
if 'win32'==sys.platform:
    includes = [ "NiallsCPP11Utilities.hpp" ]
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
    msvssolution = env.MSVSSolution("NiallsCPP11Utilities.sln", projects=msvsprojs, variant=variants)
    Depends(msvssolution, msvsprojs)
    Alias("msvcproj", msvssolution)

Return("mylibrary")
