/* Test the performance of async i/o
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Jan 2020


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
    (See accompanying file Licence.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

//! Seconds to run the benchmark
static constexpr int BENCHMARK_DURATION = 10;

/* MSVC in Release build on Windows 10.

Note that the IOCP without locking enables IOCP immediate completions.
Whereas the IOCP with locking disables IOCP immediate completions. This
makes only the IOCP with locking results comparable to ASIO.


Benchmarking Null i/o multiplexer unsynchronised with 1 handles ...
   per-handle create 6.2e-06 cancel 0 destroy 4.7e-06
     total i/o min 100 max 1.472e+06 mean 120.738 stddev 266.984
             @ 50% 100 @ 95% 200 @ 99% 200 @ 99.9% 400 @ 99.99% 2500
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 100 max 1.472e+06 mean 120.738 stddev 266.984
             @ 50% 100 @ 95% 200 @ 99% 200 @ 99.9% 400 @ 99.99% 2500
   total results collected = 48710655

Benchmarking Null i/o multiplexer unsynchronised with 4 handles ...
   per-handle create 2.45e-06 cancel 2.5e-08 destroy 1.6e-06
     total i/o min 300 max 686900 mean 389.223 stddev 370.963
             @ 50% 400 @ 95% 525 @ 99% 725 @ 99.9% 1200 @ 99.99% 6475
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 300 max 686900 mean 389.223 stddev 370.963
             @ 50% 400 @ 95% 525 @ 99% 725 @ 99.9% 1200 @ 99.99% 6475
   total results collected = 47837180

Benchmarking Null i/o multiplexer unsynchronised with 16 handles ...
   per-handle create 1.825e-06 cancel 0 destroy 7.1875e-07
     total i/o min 1200 max 973700 mean 1433.72 stddev 728.065
             @ 50% 1325 @ 95% 1943.75 @ 99% 2631.25 @ 99.9% 4793.75 @ 99.99% 22375
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 1200 max 973700 mean 1433.72 stddev 728.065
             @ 50% 1325 @ 95% 1943.75 @ 99% 2631.25 @ 99.9% 4793.75 @ 99.99% 22375
   total results collected = 49184752

Benchmarking Null i/o multiplexer unsynchronised with 64 handles ...
   per-handle create 6.78125e-07 cancel 0 destroy 1.32812e-07
     total i/o min 4600 max 522400 mean 5657.96 stddev 1619.09
             @ 50% 5139.06 @ 95% 7929.69 @ 99% 10550 @ 99.9% 17779.7 @ 99.99% 48004.7
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 4600 max 522400 mean 5657.96 stddev 1619.09
             @ 50% 5139.06 @ 95% 7929.69 @ 99% 10550 @ 99.9% 17779.7 @ 99.99% 48004.7
   total results collected = 47251392

Benchmarking Null i/o multiplexer synchronised with 1 handles ...
   per-handle create 1.05e-05 cancel 0 destroy 7.7e-06
     total i/o min 100 max 278700 mean 197.164 stddev 168.029
             @ 50% 200 @ 95% 300 @ 99% 300 @ 99.9% 600 @ 99.99% 3200
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 100 max 278700 mean 197.164 stddev 168.029
             @ 50% 200 @ 95% 300 @ 99% 300 @ 99.9% 600 @ 99.99% 3200
   total results collected = 26737663

Benchmarking Null i/o multiplexer synchronised with 4 handles ...
   per-handle create 2.825e-06 cancel 0 destroy 9.25e-07
     total i/o min 400 max 357600 mean 548.041 stddev 314.785
             @ 50% 500 @ 95% 700 @ 99% 1000 @ 99.9% 1800 @ 99.99% 7850
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 400 max 357600 mean 548.041 stddev 314.785
             @ 50% 500 @ 95% 700 @ 99% 1000 @ 99.9% 1800 @ 99.99% 7850
   total results collected = 27656188

Benchmarking Null i/o multiplexer synchronised with 16 handles ...
   per-handle create 8.6875e-07 cancel 6.25e-09 destroy 2.625e-07
     total i/o min 1600 max 652100 mean 1927.42 stddev 688.299
             @ 50% 1800 @ 95% 2600 @ 99% 3575 @ 99.9% 6243.75 @ 99.99% 24637.5
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 1600 max 652100 mean 1927.42 stddev 688.299
             @ 50% 1800 @ 95% 2600 @ 99% 3575 @ 99.9% 6243.75 @ 99.99% 24637.5
   total results collected = 28229616

Benchmarking Null i/o multiplexer synchronised with 64 handles ...
   per-handle create 6.78125e-07 cancel 1.5625e-09 destroy 3.45312e-07
     total i/o min 6500 max 1.23258e+07 mean 7831.4 stddev 18620.9
             @ 50% 6995.31 @ 95% 11178.1 @ 99% 13909.4 @ 99.9% 27435.9 @ 99.99% 94923.4
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 6500 max 1.23258e+07 mean 7831.4 stddev 18620.9
             @ 50% 6995.31 @ 95% 11178.1 @ 99% 13909.4 @ 99.9% 27435.9 @ 99.99% 94923.4
   total results collected = 27656128

Warming up ...

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 1 handles ...
   per-handle create 5.23e-05 cancel 1.68e-05 destroy 2.02e-05
     total i/o min 1800 max 9.6511e+06 mean 2663.23 stddev 6638.02
             @ 50% 2200 @ 95% 5900 @ 99% 6600 @ 99.9% 10800 @ 99.99% 36700
  initiate i/o min 400 max 8.8199e+06 mean 751.918 stddev 5312.44
             @ 50% 600 @ 95% 1800 @ 99% 2000 @ 99.9% 4000 @ 99.99% 18500
completion i/o min 1200 max 3.0832e+06 mean 1811.31 stddev 2528.3
             @ 50% 1500 @ 95% 4000 @ 99% 4500 @ 99.9% 7400 @ 99.99% 25200
   total results collected = 3468287

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 4 handles ...
   per-handle create 2.2875e-05 cancel 2.5e-08 destroy 7e-06
     total i/o min 3500 max 371100 mean 6936.17 stddev 3587.11
             @ 50% 5200 @ 95% 12300 @ 99% 14350 @ 99.9% 22775 @ 99.99% 58400
  initiate i/o min 2000 max 361900 mean 4725.03 stddev 2537.74
             @ 50% 3650 @ 95% 8225 @ 99% 9700 @ 99.9% 14950 @ 99.99% 42825
completion i/o min 1100 max 215400 mean 2111.14 stddev 1365.2
             @ 50% 1500 @ 95% 4075 @ 99% 4650 @ 99.9% 9175 @ 99.99% 34625
   total results collected = 3583996

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 16 handles ...
   per-handle create 1.29437e-05 cancel 6.25e-09 destroy 2.76438e-05
     total i/o min 9700 max 4.2575e+06 mean 20583.8 stddev 9944.7
             @ 50% 16006.3 @ 95% 36425 @ 99% 42600 @ 99.9% 58800 @ 99.99% 107531
  initiate i/o min 8200 max 4.2478e+06 mean 18540.7 stddev 9094.75
             @ 50% 14481.3 @ 95% 32487.5 @ 99% 38062.5 @ 99.9% 50587.5 @ 99.99% 93925
completion i/o min 1100 max 255200 mean 1943.07 stddev 1406.6
             @ 50% 1412.5 @ 95% 3900 @ 99% 4468.75 @ 99.9% 11468.8 @ 99.99% 42962.5
   total results collected = 4079600

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 64 handles ...
   per-handle create 1.49281e-05 cancel 1.5625e-09 destroy 9.025e-06
     total i/o min 34700 max 692700 mean 77425 stddev 33692.3
             @ 50% 63237.5 @ 95% 139083 @ 99% 161567 @ 99.9% 202941 @ 99.99% 292984
  initiate i/o min 32700 max 690400 mean 75414.1 stddev 32856.5
             @ 50% 61657.8 @ 95% 134970 @ 99% 156755 @ 99.9% 194389 @ 99.99% 275211
completion i/o min 1100 max 180500 mean 1910.91 stddev 1594.1
             @ 50% 1468.75 @ 95% 4026.56 @ 99% 4673.44 @ 99.9% 16151.6 @ 99.99% 54842.2
   total results collected = 4259776

Benchmarking llfio::pipe_handle and IOCP synchronised with 1 handles ...
   per-handle create 5.42e-05 cancel 0 destroy 1.76e-05
     total i/o min 2000 max 906000 mean 3098.49 stddev 1928.39
             @ 50% 2400 @ 95% 6500 @ 99% 7300 @ 99.9% 11400 @ 99.99% 38000
  initiate i/o min 500 max 888000 mean 937.688 stddev 978.843
             @ 50% 700 @ 95% 2100 @ 99% 2400 @ 99.9% 4600 @ 99.99% 20700
completion i/o min 1200 max 513900 mean 2060.8 stddev 1254.11
             @ 50% 1600 @ 95% 4300 @ 99% 4800 @ 99.9% 7700 @ 99.99% 25700
   total results collected = 2997247

Benchmarking llfio::pipe_handle and IOCP synchronised with 4 handles ...
   per-handle create 2.585e-05 cancel 0 destroy 9.925e-06
     total i/o min 3800 max 245100 mean 6241.72 stddev 2823.55
             @ 50% 5200 @ 95% 12100 @ 99% 14450 @ 99.9% 20975 @ 99.99% 52300
  initiate i/o min 2300 max 202800 mean 4269.66 stddev 2015.05
             @ 50% 3550 @ 95% 7975 @ 99% 9800 @ 99.9% 14475 @ 99.99% 38075
completion i/o min 1200 max 157300 mean 1872.07 stddev 1071.64
             @ 50% 1525 @ 95% 4000 @ 99% 4700 @ 99.9% 8900 @ 99.99% 28525
   total results collected = 3964924

Benchmarking llfio::pipe_handle and IOCP synchronised with 16 handles ...
   per-handle create 2.09937e-05 cancel 6.25e-09 destroy 1.37937e-05
     total i/o min 10900 max 1.1805e+06 mean 20535.9 stddev 9746.82
             @ 50% 16656.3 @ 95% 38012.5 @ 99% 44562.5 @ 99.9% 60831.3 @ 99.99% 132175
  initiate i/o min 9300 max 1.1781e+06 mean 18490.4 stddev 8875.57
             @ 50% 15068.8 @ 95% 33825 @ 99% 39806.3 @ 99.9% 52662.5 @ 99.99% 109144
completion i/o min 1100 max 341600 mean 1945.51 stddev 1597.54
             @ 50% 1500 @ 95% 4143.75 @ 99% 4781.25 @ 99.9% 14812.5 @ 99.99% 43850
   total results collected = 4095984

Benchmarking llfio::pipe_handle and IOCP synchronised with 64 handles ...
   per-handle create 9.59375e-06 cancel 1.5625e-09 destroy 3.45313e-06
     total i/o min 39100 max 1.4505e+06 mean 81997.4 stddev 34597.6
             @ 50% 66200 @ 95% 140930 @ 99% 159941 @ 99.9% 206616 @ 99.99% 316448
  initiate i/o min 36800 max 1.4431e+06 mean 79788.3 stddev 33637.6
             @ 50% 64507.8 @ 95% 136655 @ 99% 154813 @ 99.9% 197528 @ 99.99% 282281
completion i/o min 1100 max 1.0127e+06 mean 2109.12 stddev 2137.21
             @ 50% 1571.88 @ 95% 4207.81 @ 99% 4868.75 @ 99.9% 19932.8 @ 99.99% 64629.7
   total results collected = 3997632

Warming up ...

Benchmarking ASIO with pipes synchronised with 1 handles ...
   per-handle create 7.99e-05 cancel 3.2726 destroy 1.80356
     total i/o min 2500 max 3.514e+06 mean 4148.46 stddev 3612
             @ 50% 3200 @ 95% 7600 @ 99% 9000 @ 99.9% 36200 @ 99.99% 67000
  initiate i/o min 600 max 3.498e+06 mean 1133.85 stddev 3051.61
             @ 50% 900 @ 95% 1800 @ 99% 2400 @ 99.9% 21800 @ 99.99% 59500
completion i/o min 1600 max 310600 mean 2914.61 stddev 1614.86
             @ 50% 2200 @ 95% 5800 @ 99% 6700 @ 99.9% 10200 @ 99.99% 31900
   total results collected = 1752063

Benchmarking ASIO with pipes synchronised with 4 handles ...
   per-handle create 2.4e-05 cancel 1.21012 destroy 0.680531
     total i/o min 4800 max 473200 mean 9646.75 stddev 4707.28
             @ 50% 7925 @ 95% 15950 @ 99% 19850 @ 99.9% 54775 @ 99.99% 93375
  initiate i/o min 2800 max 306500 mean 6692.03 stddev 3709.62
             @ 50% 5625 @ 95% 10575 @ 99% 13450 @ 99.9% 49600 @ 99.99% 81675
completion i/o min 1600 max 460000 mean 2854.73 stddev 1773.49
             @ 50% 2200 @ 95% 5475 @ 99% 6450 @ 99.9% 13675 @ 99.99% 43000
   total results collected = 2367484

Benchmarking ASIO with pipes synchronised with 16 handles ...
   per-handle create 1.60688e-05 cancel 0.327641 destroy 0.187406
     total i/o min 14000 max 560600 mean 33731.1 stddev 14540.4
             @ 50% 28031.3 @ 95% 51256.3 @ 99% 67225 @ 99.9% 107613 @ 99.99% 171400
  initiate i/o min 11900 max 557400 mean 30488.7 stddev 13463.4
             @ 50% 25687.5 @ 95% 45731.3 @ 99% 61668.8 @ 99.9% 98712.5 @ 99.99% 162169
completion i/o min 1500 max 244500 mean 3142.47 stddev 1979.02
             @ 50% 2218.75 @ 95% 5487.5 @ 99% 6893.75 @ 99.9% 22100 @ 99.99% 52331.3
   total results collected = 2441200

Benchmarking ASIO with pipes synchronised with 64 handles ...
   per-handle create 1.45484e-05 cancel 0.0953018 destroy 0.0601118
     total i/o min 52400 max 949900 mean 111754 stddev 41214.4
             @ 50% 97982.8 @ 95% 181498 @ 99% 218020 @ 99.9% 288830 @ 99.99% 396869
  initiate i/o min 50000 max 940400 mean 109001 stddev 40275.5
             @ 50% 95726.6 @ 95% 176273 @ 99% 211186 @ 99.9% 278759 @ 99.99% 378156
completion i/o min 1600 max 365700 mean 2652.43 stddev 2774.02
             @ 50% 2109.38 @ 95% 5167.19 @ 99% 7621.88 @ 99.9% 27915.6 @ 99.99% 91671.9
   total results collected = 2883520
*/

/* GCC 7 in Release build on Linux kernel 5.3.

Benchmarking Null i/o multiplexer unsynchronised with 1 handles ...
   per-handle create 4.5e-06 cancel 2e-07 destroy 2.5e-06
     total i/o min 0 max 354609 mean 67.6987 stddev 200.835
             @ 50% 100 @ 95% 100 @ 99% 100 @ 99.9% 300 @ 99.99% 6400
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 0 max 354609 mean 67.6987 stddev 200.835
             @ 50% 100 @ 95% 100 @ 99% 100 @ 99.9% 300 @ 99.99% 6400
   total results collected = 86642687

Benchmarking Null i/o multiplexer unsynchronised with 4 handles ...
   per-handle create 2.15e-06 cancel 7.5e-08 destroy 5e-07
     total i/o min 100 max 1.35974e+06 mean 227.069 stddev 423.142
             @ 50% 200 @ 95% 325 @ 99% 450 @ 99.9% 800.25 @ 99.99% 12575.2
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 100 max 1.35974e+06 mean 227.069 stddev 423.142
             @ 50% 200 @ 95% 325 @ 99% 450 @ 99.9% 800.25 @ 99.99% 12575.2
   total results collected = 85409788

Benchmarking Null i/o multiplexer unsynchronised with 16 handles ...
   per-handle create 4.125e-07 cancel 1.875e-08 destroy 3.25e-07
     total i/o min 600 max 862723 mean 823.572 stddev 809.781
             @ 50% 750.062 @ 95% 1156.31 @ 99% 1543.88 @ 99.9% 10319.1 @ 99.99% 27325.9
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 600 max 862723 mean 823.572 stddev 809.781
             @ 50% 750.062 @ 95% 1156.31 @ 99% 1543.88 @ 99.9% 10319.1 @ 99.99% 27325.9
   total results collected = 90095600

Benchmarking Null i/o multiplexer unsynchronised with 64 handles ...
   per-handle create 2.03141e-07 cancel 3.125e-09 destroy 1.125e-07
     total i/o min 2400 max 1.01543e+06 mean 3266.72 stddev 2007.68
             @ 50% 2981.34 @ 95% 4851.73 @ 99% 6573.52 @ 99.9% 18866.1 @ 99.99% 53393.6
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 2400 max 1.01543e+06 mean 3266.72 stddev 2007.68
             @ 50% 2981.34 @ 95% 4851.73 @ 99% 6573.52 @ 99.9% 18866.1 @ 99.99% 53393.6
   total results collected = 87556032

Benchmarking Null i/o multiplexer synchronised with 1 handles ...
   per-handle create 8.1e-06 cancel 1e-07 destroy 1.5e-06
     total i/o min 0 max 977826 mean 111.005 stddev 307.363
             @ 50% 100 @ 95% 200 @ 99% 200 @ 99.9% 400 @ 99.99% 10700
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 0 max 977826 mean 111.005 stddev 307.363
             @ 50% 100 @ 95% 200 @ 99% 200 @ 99.9% 400 @ 99.99% 10700
   total results collected = 46320639

Benchmarking Null i/o multiplexer synchronised with 4 handles ...
   per-handle create 1.07525e-06 cancel 5e-08 destroy 4.25e-07
     total i/o min 200 max 1.62614e+06 mean 316.776 stddev 602.117
             @ 50% 300 @ 95% 400 @ 99% 600 @ 99.9% 1100 @ 99.99% 14775
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 200 max 1.62614e+06 mean 316.776 stddev 602.117
             @ 50% 300 @ 95% 400 @ 99% 600 @ 99.9% 1100 @ 99.99% 14775
   total results collected = 49463292

Benchmarking Null i/o multiplexer synchronised with 16 handles ...
   per-handle create 5.68812e-07 cancel 1.875e-08 destroy 2.875e-07
     total i/o min 900 max 1.33044e+06 mean 1129.93 stddev 946.038
             @ 50% 1043.88 @ 95% 1556.31 @ 99% 2081.31 @ 99.9% 11894.2 @ 99.99% 32181.9
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 900 max 1.33044e+06 mean 1129.93 stddev 946.038
             @ 50% 1043.88 @ 95% 1556.31 @ 99% 2081.31 @ 99.9% 11894.2 @ 99.99% 32181.9
   total results collected = 51576816

Benchmarking Null i/o multiplexer synchronised with 64 handles ...
   per-handle create 2.34391e-07 cancel 1.5625e-09 destroy 1.125e-07
     total i/o min 3800 max 1.02053e+06 mean 4373.35 stddev 2127.53
             @ 50% 4029.75 @ 95% 6181.42 @ 99% 10033 @ 99.9% 23319.2 @ 99.99% 59809.6
  initiate i/o min 0 max 0 mean 0 stddev 0
             @ 50% 0 @ 95% 0 @ 99% 0 @ 99.9% 0 @ 99.99% 0
completion i/o min 3800 max 1.02053e+06 mean 4373.35 stddev 2127.53
             @ 50% 4029.75 @ 95% 6181.42 @ 99% 10033 @ 99.9% 23319.2 @ 99.99% 59809.6
   total results collected = 50986944

Warming up ...

Benchmarking ASIO with pipes synchronised with 1 handles ...
   per-handle create 3.4301e-05 cancel 0.448608 destroy 0.199209
     total i/o min 1400 max 1.01183e+06 mean 2457.17 stddev 1998.98
             @ 50% 1800 @ 95% 4501 @ 99% 5800 @ 99.9% 19001 @ 99.99% 52301
  initiate i/o min 0 max 425011 mean 278.651 stddev 886.372
             @ 50% 100 @ 95% 900 @ 99% 1500 @ 99.9% 8200 @ 99.99% 26200
completion i/o min 1200 max 1.01013e+06 mean 2078.52 stddev 1593.43
             @ 50% 1600 @ 95% 3600 @ 99% 4500 @ 99.9% 17400 @ 99.99% 40901
   total results collected = 3436543

Benchmarking ASIO with pipes synchronised with 4 handles ...
   per-handle create 4.48012e-05 cancel 0.304211 destroy 0.18801
     total i/o min 1800 max 737820 mean 4816.84 stddev 3238.06
             @ 50% 3975.25 @ 95% 9075 @ 99% 13250.2 @ 99.9% 30075.8 @ 99.99% 73876.8
  initiate i/o min 300 max 475213 mean 1664.63 stddev 1973.36
             @ 50% 1100 @ 95% 4450.5 @ 99% 5475 @ 99.9% 17450.2 @ 99.99% 46401.8
completion i/o min 1200 max 555915 mean 3052.21 stddev 1898.61
             @ 50% 2850 @ 95% 4550 @ 99% 7125.25 @ 99.9% 21025.2 @ 99.99% 51452
   total results collected = 5185532

Benchmarking ASIO with pipes synchronised with 16 handles ...
   per-handle create 1.91943e-05 cancel 0.111726 destroy 0.107368
     total i/o min 3000 max 2.24566e+06 mean 14583 stddev 9893.31
             @ 50% 12538 @ 95% 28319.6 @ 99% 37076.1 @ 99.9% 66533.1 @ 99.99% 136385
  initiate i/o min 1200 max 2.20816e+06 mean 6852.63 stddev 6927.4
             @ 50% 5100.06 @ 95% 18325.6 @ 99% 22525.7 @ 99.9% 41082.4 @ 99.99% 88033.8
completion i/o min 1300 max 2.16076e+06 mean 7630.37 stddev 5250.77
             @ 50% 7256.38 @ 95% 11500.2 @ 99% 19356.8 @ 99.9% 38957.2 @ 99.99% 96196.2
   total results collected = 6258672

Benchmarking ASIO with pipes synchronised with 64 handles ...
   per-handle create 1.17987e-05 cancel 0.0257407 destroy 0.0205903
     total i/o min 7100 max 3.19148e+06 mean 56624.7 stddev 35695.9
             @ 50% 46837.2 @ 95% 100850 @ 99% 134705 @ 99.9% 208246 @ 99.99% 494421
  initiate i/o min 5000 max 3.17368e+06 mean 33487.7 stddev 30373.1
             @ 50% 21632 @ 95% 80023.9 @ 99% 109373 @ 99.9% 166306 @ 99.99% 345306
completion i/o min 1300 max 1.40714e+06 mean 23037 stddev 15035.4
             @ 50% 21644.4 @ 95% 37263.5 @ 99% 51470.1 @ 99.9% 95350.9 @ 99.99% 246896
   total results collected = 6094784
*/

#define LLFIO_ENABLE_TEST_IO_MULTIPLEXERS 1

#include "../../include/llfio/llfio.hpp"

#include "quickcpplib/algorithm/small_prng.hpp"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <typeinfo>
#include <vector>

#if __has_include("../asio/asio/include/asio.hpp")
#define ENABLE_ASIO 1
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#endif
#include "../asio/asio/include/asio.hpp"
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic pop
#endif
#endif

namespace llfio = LLFIO_V2_NAMESPACE;

struct test_results
{
  int handles{0};
  double creation{0}, cancel{0}, destruction{0};
  struct summary_t
  {
    double min{0}, mean{0}, max{0}, _50{0}, _95{0}, _99{0}, _999{0}, _9999{0}, variance{0};
  } initiate, completion, total;
  size_t total_readings{0};
};

LLFIO_TEMPLATE(class C, class... Args)
LLFIO_TREQUIRES(LLFIO_TPRED(std::is_constructible<C, int, Args...>::value))
test_results do_benchmark(int handles, Args &&...args)
{
  const bool doing_warm_up = (handles < 0);
  handles = abs(handles);
  struct timing_info
  {
    std::chrono::high_resolution_clock::time_point initiate, write, read;

    timing_info() = default;
    timing_info(std::chrono::high_resolution_clock::time_point i)
        : initiate(i)
    {
    }
    double ns_total(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(read - initiate).count() - overhead; }
    double ns_initiate(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(write - initiate).count() - overhead; }
    double ns_completion(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(read - write).count() - overhead; }
  };
  std::vector<std::vector<timing_info>> timings(handles);

  long long overhead = INT_MAX;
  auto create1 = std::chrono::high_resolution_clock::now();
  if(C::launch_writer_thread)
  {
    for(size_t n = 0; n < 1000; n++)
    {
      auto old = create1;
      create1 = std::chrono::high_resolution_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(create1 - old).count();
      if(diff != 0 && diff < overhead)
      {
        overhead = diff;
      }
    }
  }
  else
  {
    overhead = 0;
  }
  auto ios = C(handles, std::forward<Args>(args)...);
  auto create2 = std::chrono::high_resolution_clock::now();
  std::atomic<int> latch(1);
  std::thread writer_thread;
  if(C::launch_writer_thread)
  {
    writer_thread = std::thread([&]() {
      --latch;
      for(;;)
      {
        int v;
        while((v = latch.load(std::memory_order_relaxed)) == 0)
        {
          // std::this_thread::yield();
        }
        if(v < 0)
        {
          return;
        }
        latch.fetch_sub(1, std::memory_order_relaxed);
        for(int x = 0; x < handles; x++)
        {
          timings[x].back().write = std::chrono::high_resolution_clock::now();
          ios.write(x);
        }
      }
    });
    while(latch == 1)
    {
      std::this_thread::yield();
    }
  }

  for(;;)
  {
    for(size_t n = 0; n < 1024; n++)
    {
      for(int x = 0; x < handles; x++)
      {
        timings[x].emplace_back();  // this may reallocate, very expensive
      }
      // Begin all the reads
      for(int x = 0; x < handles; x++)
      {
        timings[x].back().initiate = std::chrono::high_resolution_clock::now();
        auto completion = ios.read(x);
        if(timings[x].size() > 1)
        {
          if(!C::launch_writer_thread)
          {
            timings[x][timings[x].size() - 2].write = timings[x][timings[x].size() - 2].initiate;
          }
          timings[x][timings[x].size() - 2].read = completion;
        }
      }
      // Do the writes
      latch.store(1, std::memory_order_relaxed);
      // Reap the completions
      ios.check();
    }
    if(std::chrono::duration_cast<std::chrono::seconds>(timings[0].back().initiate - timings[0].front().initiate).count() >=
       (doing_warm_up ? 3U : BENCHMARK_DURATION))
    {
      latch = -1;
      break;
    }
  }
  auto cancel1 = std::chrono::high_resolution_clock::now();
  ios.cancel();
  auto cancel2 = std::chrono::high_resolution_clock::now();
  auto destroy1 = std::chrono::high_resolution_clock::now();
  ios.destroy();
  auto destroy2 = std::chrono::high_resolution_clock::now();
  if(C::launch_writer_thread)
  {
    writer_thread.join();
  }

  test_results ret;
  ret.creation = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(create2 - create1).count()) / handles / 1000000000.0;
  ret.cancel = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(cancel2 - cancel1).count()) / handles / 1000000000.0;
  ret.destruction = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(destroy2 - destroy1).count()) / handles / 1000000000.0;

  ret.initiate.min = DBL_MAX;
  ret.completion.min = DBL_MAX;
  ret.total.min = DBL_MAX;
  for(auto &reader : timings)
  {
    // Pop the last item, whose read value will be zero
    reader.pop_back();
    for(timing_info &timing : reader)
    {
      assert(timing.ns_total(overhead) >= 0);
      assert(timing.ns_initiate(overhead) >= 0);
      assert(timing.ns_completion(overhead) >= 0);
      if(timing.ns_total(overhead) < ret.total.min)
      {
        ret.total.min = timing.ns_total(overhead);
      }
      if(timing.ns_initiate(overhead) < ret.initiate.min)
      {
        ret.initiate.min = timing.ns_initiate(overhead);
      }
      if(timing.ns_completion(overhead) < ret.completion.min)
      {
        ret.completion.min = timing.ns_completion(overhead);
      }
      if(timing.ns_total(overhead) > ret.total.max)
      {
        ret.total.max = timing.ns_total(overhead);
      }
      if(timing.ns_initiate(overhead) > ret.initiate.max)
      {
        ret.initiate.max = timing.ns_initiate(overhead);
      }
      if(timing.ns_completion(overhead) > ret.completion.max)
      {
        ret.completion.max = timing.ns_completion(overhead);
      }
      ret.total.mean += timing.ns_total(overhead);
      ret.initiate.mean += timing.ns_initiate(overhead);
      ret.completion.mean += timing.ns_completion(overhead);
      ++ret.total_readings;
    }
    std::sort(reader.begin(), reader.end(), [&](const timing_info &a, const timing_info &b) { return a.ns_total(overhead) < b.ns_total(overhead); });
    ret.total._50 += reader[(size_t)((reader.size() - 1) * 0.5)].ns_total(overhead);
    ret.total._95 += reader[(size_t)((reader.size() - 1) * 0.95)].ns_total(overhead);
    ret.total._99 += reader[(size_t)((reader.size() - 1) * 0.99)].ns_total(overhead);
    ret.total._999 += reader[(size_t)((reader.size() - 1) * 0.999)].ns_total(overhead);
    ret.total._9999 += reader[(size_t)((reader.size() - 1) * 0.9999)].ns_total(overhead);
    std::sort(reader.begin(), reader.end(), [&](const timing_info &a, const timing_info &b) { return a.ns_initiate(overhead) < b.ns_initiate(overhead); });
    ret.initiate._50 += reader[(size_t)((reader.size() - 1) * 0.5)].ns_initiate(overhead);
    ret.initiate._95 += reader[(size_t)((reader.size() - 1) * 0.95)].ns_initiate(overhead);
    ret.initiate._99 += reader[(size_t)((reader.size() - 1) * 0.99)].ns_initiate(overhead);
    ret.initiate._999 += reader[(size_t)((reader.size() - 1) * 0.999)].ns_initiate(overhead);
    ret.initiate._9999 += reader[(size_t)((reader.size() - 1) * 0.9999)].ns_initiate(overhead);
    std::sort(reader.begin(), reader.end(), [&](const timing_info &a, const timing_info &b) { return a.ns_completion(overhead) < b.ns_completion(overhead); });
    ret.completion._50 += reader[(size_t)((reader.size() - 1) * 0.5)].ns_completion(overhead);
    ret.completion._95 += reader[(size_t)((reader.size() - 1) * 0.95)].ns_completion(overhead);
    ret.completion._99 += reader[(size_t)((reader.size() - 1) * 0.99)].ns_completion(overhead);
    ret.completion._999 += reader[(size_t)((reader.size() - 1) * 0.999)].ns_completion(overhead);
    ret.completion._9999 += reader[(size_t)((reader.size() - 1) * 0.9999)].ns_completion(overhead);
  }
  ret.total.mean /= ret.total_readings;
  ret.initiate.mean /= ret.total_readings;
  ret.completion.mean /= ret.total_readings;
  ret.total._50 /= timings.size();
  ret.total._95 /= timings.size();
  ret.total._99 /= timings.size();
  ret.total._999 /= timings.size();
  ret.total._9999 /= timings.size();
  ret.initiate._50 /= timings.size();
  ret.initiate._95 /= timings.size();
  ret.initiate._99 /= timings.size();
  ret.initiate._999 /= timings.size();
  ret.initiate._9999 /= timings.size();
  ret.completion._50 /= timings.size();
  ret.completion._95 /= timings.size();
  ret.completion._99 /= timings.size();
  ret.completion._999 /= timings.size();
  ret.completion._9999 /= timings.size();
  for(auto &reader : timings)
  {
    for(timing_info &timing : reader)
    {
      ret.total.variance += (timing.ns_total(overhead) - ret.total.mean) * (timing.ns_total(overhead) - ret.total.mean);
      ret.initiate.variance += (timing.ns_initiate(overhead) - ret.initiate.mean) * (timing.ns_initiate(overhead) - ret.initiate.mean);
      ret.completion.variance += (timing.ns_completion(overhead) - ret.completion.mean) * (timing.ns_completion(overhead) - ret.completion.mean);
    }
  }
  ret.total.variance /= ret.total_readings;
  ret.initiate.variance /= ret.total_readings;
  ret.completion.variance /= ret.total_readings;
  return ret;
}

template <class C, class... Args> void benchmark(llfio::path_view csv, size_t max_handles, const char *desc, Args &&...args)
{
  std::vector<test_results> results;
  for(int n = 1; n <= max_handles; n <<= 2)
  {
    std::cout << "\nBenchmarking " << desc << " with " << n << " handles ..." << std::endl;
    auto res = do_benchmark<C>(n, std::forward<Args>(args)...);
    res.handles = n;
    results.push_back(res);
    std::cout << "   per-handle create " << res.creation << " cancel " << res.cancel << " destroy " << res.destruction;
    std::cout << "\n     total i/o min " << res.total.min << " max " << res.total.max << " mean " << res.total.mean << " stddev " << sqrt(res.total.variance);
    std::cout << "\n             @ 50% " << res.total._50 << " @ 95% " << res.total._95 << " @ 99% " << res.total._99 << " @ 99.9% " << res.total._999
              << " @ 99.99% " << res.total._9999;
    std::cout << "\n  initiate i/o min " << res.initiate.min << " max " << res.initiate.max << " mean " << res.initiate.mean << " stddev "
              << sqrt(res.initiate.variance);
    std::cout << "\n             @ 50% " << res.initiate._50 << " @ 95% " << res.initiate._95 << " @ 99% " << res.initiate._99 << " @ 99.9% "
              << res.initiate._999 << " @ 99.99% " << res.initiate._9999;
    std::cout << "\ncompletion i/o min " << res.completion.min << " max " << res.completion.max << " mean " << res.completion.mean << " stddev "
              << sqrt(res.completion.variance);
    std::cout << "\n             @ 50% " << res.completion._50 << " @ 95% " << res.completion._95 << " @ 99% " << res.completion._99 << " @ 99.9% "
              << res.completion._999 << " @ 99.99% " << res.completion._9999;
    std::cout << "\n   total results collected = " << res.total_readings << std::endl;
  }
  std::ofstream of(csv.path());
  of << "Handles";
  for(auto &i : results)
  {
    of << "," << i.handles;
  }
  of << "\nCreates";
  for(auto &i : results)
  {
    of << "," << i.creation;
  }
  of << "\nCancels";
  for(auto &i : results)
  {
    of << "," << i.cancel;
  }
  of << "\nDestroys";
  for(auto &i : results)
  {
    of << "," << i.destruction;
  }
  of << "\n";

  of << "\n\"Total Mins\"";
  for(auto &i : results)
  {
    of << "," << i.total.min;
  }
  of << "\n\"Total Maxs\"";
  for(auto &i : results)
  {
    of << "," << i.total.max;
  }
  of << "\n\"Total Means\"";
  for(auto &i : results)
  {
    of << "," << i.total.mean;
  }
  of << "\n\"Total Stddevs\"";
  for(auto &i : results)
  {
    of << "," << sqrt(i.total.variance);
  }
  of << "\n\"Total 50%s\"";
  for(auto &i : results)
  {
    of << "," << i.total._50;
  }
  of << "\n\"Total 95%s\"";
  for(auto &i : results)
  {
    of << "," << i.total._95;
  }
  of << "\n\"Total 99%s\"";
  for(auto &i : results)
  {
    of << "," << i.total._99;
  }
  of << "\n\"Total 99.9%s\"";
  for(auto &i : results)
  {
    of << "," << i.total._999;
  }
  of << "\n\"Total 99.99%s\"";
  for(auto &i : results)
  {
    of << "," << i.total._9999;
  }
  of << "\n";

  of << "\n\"Initiate Mins\"";
  for(auto &i : results)
  {
    of << "," << i.initiate.min;
  }
  of << "\n\"Initiate Maxs\"";
  for(auto &i : results)
  {
    of << "," << i.initiate.max;
  }
  of << "\n\"Initiate Means\"";
  for(auto &i : results)
  {
    of << "," << i.initiate.mean;
  }
  of << "\n\"Initiate Stddevs\"";
  for(auto &i : results)
  {
    of << "," << sqrt(i.initiate.variance);
  }
  of << "\n\"Initiate 50%s\"";
  for(auto &i : results)
  {
    of << "," << i.initiate._50;
  }
  of << "\n\"Initiate 95%s\"";
  for(auto &i : results)
  {
    of << "," << i.initiate._95;
  }
  of << "\n\"Initiate 99%s\"";
  for(auto &i : results)
  {
    of << "," << i.initiate._99;
  }
  of << "\n\"Initiate 99.9%s\"";
  for(auto &i : results)
  {
    of << "," << i.initiate._999;
  }
  of << "\n\"Initiate 99.99%s\"";
  for(auto &i : results)
  {
    of << "," << i.initiate._9999;
  }
  of << "\n";

  of << "\n\"Completion Mins\"";
  for(auto &i : results)
  {
    of << "," << i.completion.min;
  }
  of << "\n\"Completion Maxs\"";
  for(auto &i : results)
  {
    of << "," << i.completion.max;
  }
  of << "\n\"Completion Means\"";
  for(auto &i : results)
  {
    of << "," << i.completion.mean;
  }
  of << "\n\"Completion Stddevs\"";
  for(auto &i : results)
  {
    of << "," << sqrt(i.completion.variance);
  }
  of << "\n\"Completion 50%s\"";
  for(auto &i : results)
  {
    of << "," << i.completion._50;
  }
  of << "\n\"Completion 95%s\"";
  for(auto &i : results)
  {
    of << "," << i.completion._95;
  }
  of << "\n\"Completion 99%s\"";
  for(auto &i : results)
  {
    of << "," << i.completion._99;
  }
  of << "\n\"Completion 99.9%s\"";
  for(auto &i : results)
  {
    of << "," << i.completion._999;
  }
  of << "\n\"Completion 99.99%s\"";
  for(auto &i : results)
  {
    of << "," << i.completion._9999;
  }
  of << "\n";
}

struct NoHandle final : public llfio::byte_io_handle
{
  using mode = typename llfio::byte_io_handle::mode;
  using creation = typename llfio::byte_io_handle::creation;
  using caching = typename llfio::byte_io_handle::caching;
  using flag = typename llfio::byte_io_handle::flag;
  using buffer_type = typename llfio::byte_io_handle::buffer_type;
  using const_buffer_type = typename llfio::byte_io_handle::const_buffer_type;
  using buffers_type = typename llfio::byte_io_handle::buffers_type;
  using const_buffers_type = typename llfio::byte_io_handle::const_buffers_type;
  template <class T> using io_request = typename llfio::byte_io_handle::template io_request<T>;
  template <class T> using io_result = typename llfio::byte_io_handle::template io_result<T>;

  NoHandle()
      : llfio::byte_io_handle(llfio::native_handle_type(llfio::native_handle_type::disposition::nonblocking | llfio::native_handle_type::disposition::readable |
                                                        llfio::native_handle_type::disposition::writable | llfio::native_handle_type::disposition::cache_reads |
                                                        llfio::native_handle_type::disposition::cache_writes |
                                                        llfio::native_handle_type::disposition::cache_metadata,
                                                        -2 /* fake being open */),
                              llfio::byte_io_handle::flag::multiplexable, nullptr)
  {
  }
  ~NoHandle()
  {
    this->_v._init = -1;  // fake closed
  }
  NoHandle(const NoHandle &) = delete;
  NoHandle(NoHandle &&) = default;
  virtual llfio::result<void> close() noexcept override
  {
    this->_v._init = -1;  // fake closed
    return llfio::success();
  }
  virtual io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, llfio::deadline d) noexcept override
  {
    (void) d;
    return reqs.buffers;
  }
};
LLFIO_V2_NAMESPACE_BEGIN
template <> struct construct<NoHandle>
{
  pipe_handle::path_view_type _path;
  pipe_handle::mode _mode = pipe_handle::mode::read;
  pipe_handle::creation _creation = pipe_handle::creation::if_needed;
  pipe_handle::caching _caching = pipe_handle::caching::all;
  pipe_handle::flag flags = pipe_handle::flag::none;
  const path_handle &base = path_discovery::temporary_named_pipes_directory();
  result<NoHandle> operator()() const noexcept { return success(); }
};
LLFIO_V2_NAMESPACE_END

template <class HandleType = NoHandle> struct benchmark_llfio
{
  using mode = typename HandleType::mode;
  using creation = typename HandleType::creation;
  using caching = typename HandleType::caching;
  using flag = typename HandleType::flag;
  using buffer_type = typename HandleType::buffer_type;
  using const_buffer_type = typename HandleType::const_buffer_type;
  using buffers_type = typename HandleType::buffers_type;
  using const_buffers_type = typename HandleType::const_buffers_type;
  template <class T> using io_request = typename HandleType::template io_request<T>;
  template <class T> using io_result = typename HandleType::template io_result<T>;

  static constexpr bool launch_writer_thread = !std::is_same<HandleType, NoHandle>::value;

  struct receiver_type final : public llfio::byte_io_multiplexer::io_operation_state_visitor
  {
    benchmark_llfio *parent{nullptr};
    HandleType read_handle;
    std::unique_ptr<llfio::byte[]> io_state_ptr;
    llfio::byte _buffer[sizeof(size_t)];
    buffer_type buffer;
    llfio::byte_io_multiplexer::io_operation_state *io_state{nullptr};
    std::chrono::high_resolution_clock::time_point when_read_completed;

    explicit receiver_type(benchmark_llfio *_parent, HandleType &&h)
        : parent(_parent)
        , read_handle(std::move(h))
        , io_state_ptr(std::make_unique<llfio::byte[]>(read_handle.multiplexer()->io_state_requirements().first))
        , buffer(_buffer, sizeof(_buffer))
    {
      memset(_buffer, 0, sizeof(_buffer));
    }
    receiver_type(const receiver_type &) = delete;
    receiver_type(receiver_type &&o) noexcept
        : parent(o.parent)
        , read_handle(std::move(o.read_handle))
        , io_state_ptr(std::move(o.io_state_ptr))
    {
      if(o.io_state != nullptr)
      {
        abort();
      }
      o.parent = nullptr;
    }
    receiver_type &operator=(const receiver_type &) = delete;
    receiver_type &operator=(receiver_type &&) = default;
    ~receiver_type()
    {
      if(io_state != nullptr)
      {
        if(!is_finished(io_state->current_state()))
        {
          abort();
        }
        io_state->~io_operation_state();
        io_state = nullptr;
      }
    }

    // Initiate the read
    void begin_io()
    {
      if(io_state != nullptr)
      {
        if(!is_finished(io_state->current_state()))
        {
          abort();
        }
      }
      buffer = {_buffer, sizeof(_buffer)};
      io_state = read_handle.multiplexer()->construct_and_init_io_operation({io_state_ptr.get(), 4096 /*lies*/}, &read_handle, this, {}, {},
                                                                            io_request<buffers_type>({&buffer, 1}, 0));
    }

    // Called when the read completes
    virtual bool read_completed(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/,
                                io_result<buffers_type> &&res) override
    {
      when_read_completed = std::chrono::high_resolution_clock::now();
      if(!res)
      {
        abort();
      }
      if(res)
      {
        if(res.value().size() != 1)
        {
          abort();
        }
      }
      return true;
    }

    // Called when the state for the read can be disposed
    virtual void read_finished(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/) override
    {
      io_state->~io_operation_state();
      io_state = nullptr;
    }
  };

  llfio::byte_io_multiplexer_ptr multiplexer;
  std::vector<HandleType> write_handles;
  std::vector<receiver_type> read_states;
  receiver_type *to_restart{nullptr};

  explicit benchmark_llfio(size_t count, llfio::byte_io_multiplexer_ptr (*make_multiplexer)())
  {
    multiplexer = make_multiplexer();
    read_states.reserve(count);
    // Construct read and write sides of the pipes
    llfio::filesystem::path::value_type name[64] = {'l', 'l', 'f', 'i', 'o', '_'};
    for(size_t n = 0; n < count; n++)
    {
      name[QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(name + 6, 58, (const char *) &n, sizeof(n))] = 0;
      auto h = llfio::construct<HandleType>{name, mode::read, creation::if_needed, caching::all, flag::multiplexable}().value();
      h.set_multiplexer(multiplexer.get()).value();
      read_states.emplace_back(this, std::move(h));
      write_handles.push_back(llfio::construct<HandleType>{name, mode::write, creation::open_existing, caching::all, flag::multiplexable}().value());
    }
  }
  std::chrono::high_resolution_clock::time_point read(unsigned which)
  {
    auto ret = read_states[which].when_read_completed;
    read_states[which].when_read_completed = {};
    read_states[which].begin_io();
    return ret;
  }
  void check()
  {
    size_t done = 0;
    do
    {
#if 0
      done = 0;
      for(auto &i : read_states)
      {
        if(i.io_state == nullptr || is_finished(multiplexer->check_io_operation(i.io_state)))
        {
          done++;
        }
      }
#else
      done += multiplexer->check_for_any_completed_io().value().initiated_ios_finished;
#endif
    } while(done < read_states.size());
  }
  void write(unsigned which)
  {
    llfio::byte c = llfio::to_byte(78);
    const_buffer_type b(&c, 1);
    write_handles[which].write(io_request<const_buffers_type>({&b, 1}, 0)).value();
  }
  void cancel()
  {
    for(auto &i : read_states)
    {
      if(i.io_state != nullptr)
      {
        (void) multiplexer->cancel_io_operation(i.io_state);
      }
    }
  }
  void destroy()
  {
    read_states.clear();
    multiplexer.reset();
  }
};

#if ENABLE_ASIO
struct benchmark_asio_pipe
{
#ifdef _WIN32
  using handle_type = asio::windows::stream_handle;
#else
  using handle_type = asio::posix::stream_descriptor;
#endif
  static constexpr bool launch_writer_thread = true;

  struct read_state
  {
    char raw_buffer[1];
    handle_type read_handle;
    std::chrono::high_resolution_clock::time_point when_read_completed;

    explicit read_state(handle_type &&h)
        : read_handle(std::move(h))
    {
    }
    void begin_io()
    {
      read_handle.async_read_some(asio::buffer(raw_buffer, 1), [this](const auto & /*unused*/, const auto & /*unused*/) {
        when_read_completed = std::chrono::high_resolution_clock::now();
        begin_io();
      });
    }
  };

  llfio::optional<asio::io_context> multiplexer;
  std::vector<handle_type> write_handles;
  std::vector<read_state> read_states;

  explicit benchmark_asio_pipe(size_t count, int threads)
  {
    multiplexer.emplace(threads);
    read_states.reserve(count);
    write_handles.reserve(count);
    // Construct read and write sides of the pipes
    llfio::filesystem::path::value_type name[64] = {'l', 'l', 'f', 'i', 'o', '_'};
    for(size_t n = 0; n < count; n++)
    {
      using mode = typename llfio::pipe_handle::mode;
      using creation = typename llfio::pipe_handle::creation;
      using caching = typename llfio::pipe_handle::caching;
      using flag = typename llfio::pipe_handle::flag;

      name[QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(name + 6, 58, (const char *) &n, sizeof(n))] = 0;
      auto read_handle = llfio::construct<llfio::pipe_handle>{name, mode::read, creation::if_needed, caching::all, flag::multiplexable}().value();
      auto write_handle = llfio::construct<llfio::pipe_handle>{name, mode::write, creation::open_existing, caching::all, flag::multiplexable}().value();
#ifdef _WIN32
      read_states.emplace_back(handle_type(*multiplexer, read_handle.release().h));
      write_handles.emplace_back(handle_type(*multiplexer, write_handle.release().h));
#else
      read_states.emplace_back(handle_type(*multiplexer, read_handle.release().fd));
      write_handles.emplace_back(handle_type(*multiplexer, write_handle.release().fd));
#endif
    }
  }
  std::chrono::high_resolution_clock::time_point read(unsigned which)
  {
    auto ret = read_states[which].when_read_completed;
    read_states[which].when_read_completed = {};
    read_states[which].begin_io();
    return ret;
  }
  void check()
  {
    for(;;)
    {
      size_t count = 0;
      for(auto &i : read_states)
      {
        if(i.when_read_completed != std::chrono::high_resolution_clock::time_point())
        {
          count++;
        }
      }
      if(count == read_states.size())
      {
        break;
      }
      multiplexer->poll();  // supposedly does not return until no completions are pending
    }
  }
  void write(unsigned which)
  {
    char c = 78;
    write_handles[which].write_some(asio::buffer(&c, 1));
  }
  void cancel()
  {
    for(auto &i : read_states)
    {
      i.read_handle.cancel();
    }
    multiplexer->poll();  // does not return until no completions are pending
  }
  void destroy()
  {
    write_handles.clear();
    read_states.clear();
    multiplexer.reset();
  }
};
#endif

int main(void)
{
  std::cout << "Warming up ..." << std::endl;
  do_benchmark<benchmark_llfio<>>(-1, []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_null(2, true).value(); });
  benchmark<benchmark_llfio<>>("llfio-null-unsynchronised.csv", 64, "Null i/o multiplexer unsynchronised",  //
                               []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_null(1, false).value(); });
  benchmark<benchmark_llfio<>>("llfio-null-synchronised.csv", 64, "Null i/o multiplexer synchronised",  //
                               []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_null(2, true).value(); });

#ifdef _WIN32
  std::cout << "\nWarming up ..." << std::endl;
  do_benchmark<benchmark_llfio<llfio::pipe_handle>>(-1,  //
                                                    []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
  // No locking, enable IOCP immediate completions. ASIO can't compete with this.
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-unsynchronised.csv", 64, "llfio::pipe_handle and IOCP unsynchronised",  //
                                                 []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(1, false).value(); });
  // Locking enabled, disable IOCP immediate completions so it's a fair comparison with ASIO
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-synchronised.csv", 64, "llfio::pipe_handle and IOCP synchronised",  //
                                                 []() -> llfio::byte_io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
#endif

#if ENABLE_ASIO
  std::cout << "\nWarming up ..." << std::endl;
  do_benchmark<benchmark_asio_pipe>(-1, 2);
  benchmark<benchmark_asio_pipe>("asio-pipe-handle-synchronised.csv", 64, "ASIO with pipes synchronised", 2);
#endif

  return 0;
}