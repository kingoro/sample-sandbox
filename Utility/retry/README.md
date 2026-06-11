# Retry Utility

最大試行回数と固定/指数backoffを管理するC11状態機械である。公開入口は
`include/utility_retry.h`。heap、thread、clock、sleep、I/Oは扱わない。
