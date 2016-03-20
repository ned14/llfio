#!/usr/bin/python3
# Calculate boost.afio build times under various configs
# (C) 2015 Niall Douglas
# Created: 12th March 2015
#[  [`--link-test --fast-build debug`][][[footnote ASIO has a link error without `link=static`]][fails]]
#[               [`--link-test debug`][][][]]
#[         [`--link-test --lto debug`][[]][][]]
#[       [`--link-test pch=off debug`][][][]]
#[[`--link-test --fast-build release`][][[footnote ASIO has a link error without `link=static`]][fails]]
#[             [`--link-test release`][][][]]
#[       [`--link-test --lto release`][][][]]

import os, sys, subprocess, time, shutil, platform

if len(sys.argv)<2:
  print("Usage: "+sys.argv[0]+" <toolset>", file=sys.stderr)
  sys.exit(1)
if not os.path.exists("b2") and not os.path.exists("b2.exe"):
  print("ERROR: Need to run me from boost root directory please", file=sys.stderr)
print(os.getcwd())
shutil.rmtree("bin.v2", True)
  
onWindows="Windows" in platform.system()

configs=[
  ["--c++14 --link-test --fast-build debug", None],
  ["--c++14 --link-test debug", None],
  ["--c++14 --link-test --lto debug", None],
  ["--c++14 --link-test pch=off debug", None],
  ["--c++14 --link-test --fast-build release", None],
  ["--c++14 --link-test release", None],
  ["--c++14 --link-test --lto release", None],
  ["standalone_singleabi", None],
  ["standalone_multiabi", None]
]
for config in configs:
  print("\n\nConfig: "+config[0])
  if config[0]=="standalone_singleabi" or config[0]=="standalone_multiabi":
    if onWindows:
      test_all="test_all.exe"
      tocall="alltests_msvc.bat" if "msvc" in sys.argv[1] else "alltests_gcc.bat"
    else:
      test_all="test_all"
      tocall="alltests_gcc.sh"
    if config[0]=="standalone_singleabi":
      tocall="standalone_"+tocall
    else:
      tocall="multiabi_"+tocall
    basedir=os.getcwd()
    env=dict(os.environ)
    if not onWindows:
      tocall="./"+tocall
      env['CXX']=sys.argv[1]
      env['CXX']=env['CXX'].replace('gcc', 'g++')
      env['CXX']=env['CXX'].replace('clang', 'clang++')
    try:
      os.chdir("libs/afio")
      shutil.rmtree(test_all, True)
      if subprocess.call(tocall, env=env, shell=True):
        config[1]="FAILED"
        continue
      shutil.rmtree(test_all, True)
      print("\n\nStarting benchmark ...")
      begin=time.perf_counter()
      subprocess.call(tocall, env=env, shell=True)
      end=time.perf_counter()
    finally:
      os.chdir(basedir)
  else:
    shutil.rmtree("bin.v2/libs/afio", True)
    if subprocess.call([os.path.abspath("b2"), "toolset="+sys.argv[1], "libs/afio/test", "-j", "8"]+config[0].split(" ")):
      config[1]="FAILED"
      continue
    shutil.rmtree("bin.v2/libs/afio", True)
    print("\n\nStarting benchmark ...")
    begin=time.perf_counter()
    subprocess.call([os.path.abspath("b2"), "toolset="+sys.argv[1], "libs/afio/test"]+config[0].split(" "))
    end=time.perf_counter()
  mins=int((end-begin)/60)
  secs=int((end-begin)%60);
  config[1]="%dm%ss" % (mins, secs)
  print("Config %s took %dm%ss" % (config[0], mins, secs))

print("\n\n")
for config in configs:
  print(config)
  