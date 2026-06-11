# Architecture

Time Foundationは値型と純粋変換だけを所有する。system clock取得、RTC device access、
network同期、timezone databaseは依存方向を逆転させないため含めない。

UTCはwall clock表示と同期snapshotに使う。timeout、retry backoff、Event Timer、
scheduler elapsed timeにはclock adjustmentの影響を受けないmonotonic nanosecondを使う。
