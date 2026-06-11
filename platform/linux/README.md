# Linux Time Platform

`CLOCK_MONOTONIC`と`CLOCK_REALTIME`をTime Foundation型へ変換する薄いadapter。
公開headerはPOSIX型へ依存せずstrict C11でincludeできる。独自snapshot callback注入に
よりsystem errorと不正clock値を決定的に試験できる。

`/dev/rtc`操作は実装しない。RTCはLinux device driver層の責務である。
