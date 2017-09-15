# BB-Tracer
Runtime Library for Tracing Basic Block Execution

Synopsis
========

For this runtime library to work, the program needs to be instrumented at three places, with the following instrumentation calls:
    *record_func_entry: at the entry of functions
    *record_callsite: after every callsite
    *record_bb_entry: at the entry of each basic block
