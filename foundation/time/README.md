# Time Foundation

OS、clock、heap、thread、I/Oに依存しないUTC、duration、monotonic tickの型と純粋変換を
提供する。公開入口は `include/utility_time.h`。

- UTCはUnix epochからの `int64_t` 秒と、0以上1,000,000,000未満のnanosecondで表す。
- RFC3339はUTC `Z` のみを扱い、西暦0001年から9999年のGregorian calendarに対応する。
- Timezone offset、IANA timezone database、DST、leap secondは扱わない。
- timeout、retry、Event TimerはUTCではなくmonotonic tickを使う。
- RTC device、NTP/PTP/GPS clientはdriverまたはserviceなど外部層の責務である。

```sh
make foundation-time-test
make foundation-time-static-analysis
```
