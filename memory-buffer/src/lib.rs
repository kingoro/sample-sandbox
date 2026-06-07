//! C ABIを持つ、ヒープを使わないメモリバッファ所有権ライブラリ。
//!
//! このcrateは、呼出側が提供したRAM arenaの割り当てと生存期間を管理する。
//! 格納データの意味、I/O、thread同期は責務に含まない。
//!
//! 正式な外部interfaceは`include/memory_buffer.h`のC ABIである。

#![cfg_attr(not(feature = "std"), no_std)]
#![deny(unsafe_op_in_unsafe_fn)]

mod buffer;

pub use buffer::{
    MB_CONTEXT_ALIGNMENT, MB_CONTEXT_SIZE, MB_MAX_BUFFERS, MbBufferInfo, MbResult, mb_alloc,
    mb_free, mb_get_info, mb_init, mb_map, mb_read, mb_reset, mb_set_length, mb_unmap, mb_write,
};
