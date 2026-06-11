# Time Service

UTCのavailability、synchronization、source、uncertainty、last sync monotonic tickを
呼出側所有stateで管理する。heap、thread、I/Oを使わない。

`now`は同期UTCをmonotonic elapsed分だけ進め、未同期、monotonic逆行、overflowをerrorに
する。uncertaintyはlast sync時点のsnapshot値であり、経過時間に応じて自動増加しない。
Timezone、RTC device、NTP/PTP/GPS clientはこのserviceの責務外である。
