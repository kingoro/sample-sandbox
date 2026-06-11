# API

`utility_time.h`は正規化UTC、duration、monotonic tick、overflow安全な秒/ms/us/ns変換、
UTC比較と加減算、RFC3339 UTC format/parseを公開する。

Formatterは常に9桁のfractional secondを出力する。Parserはfractionなし、または1から9桁を
受理し、`Z`以外のoffsetと秒60を拒否する。
