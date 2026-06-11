# ID Foundation

0を予約した`uint32_t` request ID generator。公開入口は
`include/utility_id.h`。単一thread専用で、使用中IDとの衝突管理は呼出側責務。
