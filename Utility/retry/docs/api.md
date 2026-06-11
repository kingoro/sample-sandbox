# Retry API

`ut_retry_begin_attempt()`で試行を開始し、失敗時は`ut_retry_failure()`から
次回delayを得る。成功時は`ut_retry_success()`で同じpolicyを再利用できる。
