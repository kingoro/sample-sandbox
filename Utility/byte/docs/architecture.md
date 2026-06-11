# Byte Architecture

ReaderとWriterを分離し、共通result codeだけを共有する。動的確保やglobal状態は
なく、contextを同時共有しない限り複数threadから独立して利用できる。
