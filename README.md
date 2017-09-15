# BB-Tracer
Runtime Library for Tracing Basic Block Execution

Synopsis
========

For this runtime library to work, the program needs to be instrumented at three places, with the following instrumentation calls:
* record_func_entry(BBID): at the entry of functions
* record_callsite(BBID): after every callsite
* record_bb_entry(BBID): at the entry of each basic block

BBID is a 64bit int representing a unique basic block: [16bit for DSO hash, 32bit for function id, 16bit for basic block id]
