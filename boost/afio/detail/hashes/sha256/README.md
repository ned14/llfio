
SSE optimized SHA-256 algorithm
===============================

This algorithm uses SSE to perform four SHA-256 calculations in parallel.
The code was copied from the SHA-256 implementation used in OpenSolaris
and slightly modified.

In theory, this code would be four time as fast as the original algorithm.
However, SHA uses 'rotate' and SSE doesn't have any instructions to
accelerate packed rotates. We have to emulate rotates with left shift,
right shift and an logical or. Furthermore, the SSE code does have
a bit overhead when packing/unpacking the data.

Despite these downsides, the SSE algorithm is about twice as fast as the
original code. So if you need to calculate lots of SHA-256 in parallel,
feel free to try this code.

There are certainly areas where this algorithm could be improved, nither
of which I'm planing to work on:

 * pre-packing blocks/context state
 * intelligent prefetching
 * use pure assembler instead of intrinsics
