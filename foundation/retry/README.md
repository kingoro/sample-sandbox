# Retry Foundation

最大試行回数と固定/指数backoffを管理するC11状態機械である。公開入口は
`include/utility_retry.h`。heap、thread、clock、sleep、I/Oは扱わない。
呼出側がtimeoutやbackoff待ちを実装する場合は、wall clock UTCではなくmonotonic clockを
使用する。
