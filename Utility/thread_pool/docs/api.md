# Thread Pool API

初期化後は`ut_thread_pool_submit()`を複数threadから呼べる。終了時はdrainまたは
immediate shutdownを選ぶ。shutdownとsubmitの競合は安全で、shutdownより前に
受付済みか、shutdown後のstate errorかに直列化される。

shutdown後は全producer threadを停止・joinし、submit呼出しが存在しない状態で
`join()`、`destroy()`の順に呼ぶ。join/destroyとsubmitの同時実行は契約違反である。
joinが途中で失敗した場合は、未join workerから再試行できる。
初期化途中のworker作成失敗でも、cleanup join失敗時はSTOPPING状態が残るため
`join()`、`destroy()`で回収する。JOINEDなら`destroy()`だけを再試行する。
Mutex初期化失敗時はUNINITIALIZEDで回収不要である。Condition初期化失敗後に
mutex破棄も失敗した場合はJOINEDかつ`mutex_initialized == true`となるため、
`destroy()`を再試行する。

Callback contextの寿命はcallback完了まで呼出側が保証する。Immediate shutdownは
pending callbackを実行せず、contextを解放しない。producer停止後かつworker join
成功後に、呼出側が破棄されたpending jobのcontextを回収する。

Destroyはcondition、mutexの順に処理し、成功済みprimitiveのinitialized fieldを
falseにする。途中失敗後の再試行では未破棄primitiveだけを処理する。
