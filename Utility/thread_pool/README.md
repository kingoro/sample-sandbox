# Thread Pool Utility

POSIX pthreadによる固定worker数・固定長queueのCPU job実行基盤。公開入口は
`include/utility_thread_pool.h`。画像処理等のCPU job向けで、I/O event dispatch
には使用しない。storageは呼出側が所有し、heapは使用しない。

Shutdownは並行submitに安全だが、join/destroy前には全producerを停止・joinする。
Immediate shutdownで破棄されたpending jobのcontextはpoolが解放しない。
