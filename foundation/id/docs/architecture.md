# ID Architecture

Generatorは1 fieldだけを持つ呼出側所有contextで、heapやglobal状態を使わない。
wrap後のID再利用可否はactive request集合を知る上位層が判断する。
