# Architecture

Platform adapterは`clock_gettime`以外のpolicyを持たない。同期sourceの選択、availability、
uncertaintyは`service/time`が管理する。
