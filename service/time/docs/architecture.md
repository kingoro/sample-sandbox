# Architecture

Clock取得はplatform/driverから渡されたsnapshotで行う。sourceはUNKNOWN、RTC、NTP、
PTP、GPS、MANUALを区別する。timeoutやretryはUTCではなくmonotonic clockを直接使う。
