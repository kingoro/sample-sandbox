# Byte API

`ut_byte_reader_t`と`ut_byte_writer_t`へ呼出側bufferを設定し、型とendianを
明示した関数で順次処理する。容量不足時はbuffer、出力値、offsetを変更しない。
