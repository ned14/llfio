Herein lies an exploratory toy ACID key-value store written using
AFIO which lets you look up any BLOB value from some 128-bit key, and to
update as an atomic transaction up to 65,535 key-values at once.

It is purely to test the feasibility of one approach to implementing such
a store, and to test AFIO's design. Nobody should use this store for
anything serious.
