use core::cmp;
use core::ffi::c_void;
use core::mem::{align_of, size_of};
use core::ptr;

/// 1 contextで同時に管理できる最大バッファ数。
pub const MB_MAX_BUFFERS: usize = 64;
/// C側で確保する`mb_context_t`のbyte数。
pub const MB_CONTEXT_SIZE: usize = 4096;
/// C側で確保する`mb_context_t`に必要なalignment。
pub const MB_CONTEXT_ALIGNMENT: usize = 8;

const MAX_BUFFERS: usize = MB_MAX_BUFFERS;
const INVALID_OFFSET: usize = usize::MAX;
const HANDLE_INDEX_BITS: u32 = 8;
const HANDLE_INDEX_MASK: u32 = (1 << HANDLE_INDEX_BITS) - 1;
const CONTEXT_MAGIC: u32 = 0x4D42_5546;
const CONTEXT_ALIGNMENT: usize = MB_CONTEXT_ALIGNMENT;

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
/// すべてのC ABI操作が返す結果code。
pub enum MbResult {
    Ok = 0,
    InvalidArgument = 1,
    NotInitialized = 2,
    OutOfMemory = 4,
    InvalidHandle = 5,
    OutOfBounds = 6,
    Busy = 7,
}

#[repr(C)]
#[derive(Clone, Copy)]
/// 割り当て済みバッファの公開状態。
pub struct MbBufferInfo {
    pub length: usize,
    pub capacity: usize,
    pub mapped: u8,
    pub reserved: [u8; 7],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Slot {
    offset: usize,
    length: usize,
    capacity: usize,
    generation: u32,
    map_count: u32,
}

impl Slot {
    const EMPTY: Self = Self {
        offset: INVALID_OFFSET,
        length: 0,
        capacity: 0,
        generation: 1,
        map_count: 0,
    };

    fn active(&self) -> bool {
        self.offset != INVALID_OFFSET
    }
}

#[repr(C)]
struct Context {
    magic: u32,
    reserved: u32,
    arena: *mut u8,
    arena_len: usize,
    slots: [Slot; MAX_BUFFERS],
}

// mb_context_tを静的確保するC側を変更せずContextを拡張できるよう、
// 公開storageには意図的に余裕を持たせる。
const PUBLIC_CONTEXT_SIZE: usize = MB_CONTEXT_SIZE;
const _: () = assert!(size_of::<Context>() <= PUBLIC_CONTEXT_SIZE);
const _: () = assert!(align_of::<Context>() <= CONTEXT_ALIGNMENT);
const _: () = assert!(CONTEXT_ALIGNMENT == align_of::<u64>());
const _: () = assert!(size_of::<MbResult>() == size_of::<u32>());
const _: () = assert!(size_of::<MbBufferInfo>() == (2 * size_of::<usize>()) + 8);
const _: () = assert!(core::mem::offset_of!(MbBufferInfo, length) == 0);
const _: () = assert!(core::mem::offset_of!(MbBufferInfo, capacity) == size_of::<usize>());
const _: () = assert!(core::mem::offset_of!(MbBufferInfo, mapped) == 2 * size_of::<usize>());

fn context<'a>(storage: *mut c_void) -> Result<&'a mut Context, MbResult> {
    if storage.is_null() {
        return Err(MbResult::InvalidArgument);
    }
    // SAFETY: mb_initがalignmentを検証し、このstorageへContextを配置している。
    // すべての公開関数は、呼出側が直列化した排他的accessを前提とする。
    let ctx = unsafe { &mut *storage.cast::<Context>() };
    if ctx.magic != CONTEXT_MAGIC {
        return Err(MbResult::NotInitialized);
    }
    Ok(ctx)
}

fn decode_handle(handle: u32) -> Option<(usize, u32)> {
    let encoded_index = handle & HANDLE_INDEX_MASK;
    if encoded_index == 0 {
        return None;
    }
    let index = (encoded_index - 1) as usize;
    let generation = handle >> HANDLE_INDEX_BITS;
    if index >= MAX_BUFFERS || generation == 0 {
        return None;
    }
    Some((index, generation))
}

fn encode_handle(index: usize, generation: u32) -> u32 {
    (generation << HANDLE_INDEX_BITS) | ((index as u32) + 1)
}

fn slot_for_handle(ctx: &mut Context, handle: u32) -> Result<&mut Slot, MbResult> {
    let (index, generation) = decode_handle(handle).ok_or(MbResult::InvalidHandle)?;
    let slot = &mut ctx.slots[index];
    if !slot.active() || slot.generation != generation {
        return Err(MbResult::InvalidHandle);
    }
    Ok(slot)
}

fn next_generation(current: u32) -> u32 {
    let mut next = current.wrapping_add(1) & 0x00FF_FFFF;
    if next == 0 {
        next = 1;
    }
    next
}

fn find_region(ctx: &Context, capacity: usize) -> Option<usize> {
    if capacity == 0 || capacity > ctx.arena_len {
        return None;
    }

    let mut candidate = 0usize;
    loop {
        let mut nearest_offset = ctx.arena_len;
        let mut nearest_end = ctx.arena_len;

        for slot in &ctx.slots {
            if slot.active() && slot.offset >= candidate && slot.offset < nearest_offset {
                nearest_offset = slot.offset;
                nearest_end = slot.offset.checked_add(slot.capacity)?;
            }
        }

        if capacity <= nearest_offset.saturating_sub(candidate) {
            return Some(candidate);
        }
        if nearest_offset == ctx.arena_len {
            return None;
        }
        candidate = nearest_end;
        if candidate > ctx.arena_len {
            return None;
        }
    }
}

fn checked_range(offset: usize, length: usize, limit: usize) -> bool {
    offset.checked_add(length).is_some_and(|end| end <= limit)
}

fn ranges_overlap(
    first: usize,
    first_len: usize,
    second: usize,
    second_len: usize,
) -> Option<bool> {
    let first_end = first.checked_add(first_len)?;
    let second_end = second.checked_add(second_len)?;
    Some(first < second_end && second < first_end)
}

#[unsafe(no_mangle)]
/// 制御storageを初期化し、呼出側所有のデータarenaへ関連付ける。
///
/// # Safety
///
/// すべてのpointerは、指定したsizeだけ有効なmemoryを参照しなければならない。
/// control storageとarenaは重複してはならない。
/// contextとarenaへのaccessは呼出側で直列化しなければならない。
pub unsafe extern "C" fn mb_init(
    storage: *mut c_void,
    storage_size: usize,
    arena: *mut u8,
    arena_len: usize,
) -> MbResult {
    if storage.is_null()
        || arena.is_null()
        || storage_size < PUBLIC_CONTEXT_SIZE
        || !(storage as usize).is_multiple_of(CONTEXT_ALIGNMENT)
        || arena_len == 0
        || ranges_overlap(
            storage as usize,
            PUBLIC_CONTEXT_SIZE,
            arena as usize,
            arena_len,
        ) != Some(false)
    {
        return MbResult::InvalidArgument;
    }

    let ctx_ptr = storage.cast::<Context>();
    let ctx = Context {
        magic: CONTEXT_MAGIC,
        reserved: 0,
        arena,
        arena_len,
        slots: [Slot::EMPTY; MAX_BUFFERS],
    };
    // SAFETY: storageのnull、size、Context alignmentは上で検証済み。
    unsafe { ptr::write(ctx_ptr, ctx) };
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// map中のバッファがないことを確認し、すべてのhandleを無効化する。
///
/// # Safety
///
/// `storage`には[`mb_init`]で初期化したcontextが必要であり、accessは呼出側で
/// 直列化しなければならない。
pub unsafe extern "C" fn mb_reset(storage: *mut c_void) -> MbResult {
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    if ctx.slots.iter().any(|slot| slot.map_count != 0) {
        return MbResult::Busy;
    }
    for slot in &mut ctx.slots {
        slot.offset = INVALID_OFFSET;
        slot.length = 0;
        slot.capacity = 0;
        slot.generation = next_generation(slot.generation);
    }
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// 領域を割り当て、世代付きhandleを返す。
///
/// # Safety
///
/// `storage`は初期化済み、`out_handle`は書込み可能でなければならない。
/// accessは呼出側で直列化しなければならない。
pub unsafe extern "C" fn mb_alloc(
    storage: *mut c_void,
    capacity: usize,
    out_handle: *mut u32,
) -> MbResult {
    if out_handle.is_null() || capacity == 0 {
        return MbResult::InvalidArgument;
    }
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let index = match ctx.slots.iter().position(|slot| !slot.active()) {
        Some(index) => index,
        None => return MbResult::OutOfMemory,
    };
    let offset = match find_region(ctx, capacity) {
        Some(offset) => offset,
        None => return MbResult::OutOfMemory,
    };

    let slot = &mut ctx.slots[index];
    slot.offset = offset;
    slot.length = 0;
    slot.capacity = capacity;
    slot.map_count = 0;
    // SAFETY: out_handleはnull検証済みで、出力先memoryはC側が所有している。
    unsafe { out_handle.write(encode_handle(index, slot.generation)) };
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// map貸出中でなければバッファを解放する。
///
/// # Safety
///
/// `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
pub unsafe extern "C" fn mb_free(storage: *mut c_void, handle: u32) -> MbResult {
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count != 0 {
        return MbResult::Busy;
    }
    slot.offset = INVALID_OFFSET;
    slot.length = 0;
    slot.capacity = 0;
    slot.generation = next_generation(slot.generation);
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// バッファへbyte列をcopyし、必要に応じて論理長を伸ばす。
///
/// # Safety
///
/// `source`は`length` byte読取り可能でなければならない。`storage`には
/// 初期化済みcontextが必要で、accessは呼出側で直列化する。
pub unsafe extern "C" fn mb_write(
    storage: *mut c_void,
    handle: u32,
    offset: usize,
    source: *const u8,
    length: usize,
) -> MbResult {
    if source.is_null() && length != 0 {
        return MbResult::InvalidArgument;
    }
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let arena = ctx.arena;
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count != 0 {
        return MbResult::Busy;
    }
    if !checked_range(offset, length, slot.capacity) {
        return MbResult::OutOfBounds;
    }
    if length != 0 {
        // SAFETY: sourceはnullでなく、書込み先範囲は境界検証済み。
        // 同じarena内からのcopyも許可するため、意図的にptr::copyを使う。
        unsafe { ptr::copy(source, arena.add(slot.offset + offset), length) };
    }
    slot.length = cmp::max(slot.length, offset + length);
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// 初期化済みbyte列をバッファから呼出側所有memoryへcopyする。
///
/// # Safety
///
/// `destination`は`length` byte書込み可能でなければならない。`storage`には
/// 初期化済みcontextが必要で、accessは呼出側で直列化する。
pub unsafe extern "C" fn mb_read(
    storage: *mut c_void,
    handle: u32,
    offset: usize,
    destination: *mut u8,
    length: usize,
) -> MbResult {
    if destination.is_null() && length != 0 {
        return MbResult::InvalidArgument;
    }
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let arena = ctx.arena;
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count != 0 {
        return MbResult::Busy;
    }
    if !checked_range(offset, length, slot.length) {
        return MbResult::OutOfBounds;
    }
    if length != 0 {
        // SAFETY: destinationはnullでなく、読取り元範囲は境界検証済み。
        // 同じarena内へのcopyも許可するため、意図的にptr::copyを使う。
        unsafe { ptr::copy(arena.add(slot.offset + offset), destination, length) };
    }
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// capacityを変更せず論理長だけを変更する。
///
/// # Safety
///
/// `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
/// 論理長を伸ばす場合、新たに有効化する範囲はmap pointerなどで
/// 初期化済みでなければならない。
pub unsafe extern "C" fn mb_set_length(
    storage: *mut c_void,
    handle: u32,
    length: usize,
) -> MbResult {
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count != 0 {
        return MbResult::Busy;
    }
    if length > slot.capacity {
        return MbResult::OutOfBounds;
    }
    slot.length = length;
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// バッファの論理長、capacity、map状態を返す。
///
/// # Safety
///
/// `out_info`は書込み可能でなければならない。`storage`には初期化済みcontextが
/// 必要で、accessは呼出側で直列化する。
pub unsafe extern "C" fn mb_get_info(
    storage: *mut c_void,
    handle: u32,
    out_info: *mut MbBufferInfo,
) -> MbResult {
    if out_info.is_null() {
        return MbResult::InvalidArgument;
    }
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    let info = MbBufferInfo {
        length: slot.length,
        capacity: slot.capacity,
        mapped: u8::from(slot.map_count != 0),
        reserved: [0; 7],
    };
    // SAFETY: out_infoはnull検証済みで、出力先memoryはC側が所有している。
    unsafe { out_info.write(info) };
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// 対応する[`mb_unmap`]まで、arenaへの排他的な直接accessを一時貸出する。
///
/// # Safety
///
/// 出力pointerは書込み可能でなければならない。呼出側は返却されたpointerの
/// 貸出期間を守り、context accessを直列化しなければならない。
pub unsafe extern "C" fn mb_map(
    storage: *mut c_void,
    handle: u32,
    out_data: *mut *mut u8,
    out_capacity: *mut usize,
) -> MbResult {
    if out_data.is_null() || out_capacity.is_null() {
        return MbResult::InvalidArgument;
    }
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let arena = ctx.arena;
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count != 0 {
        return MbResult::Busy;
    }
    slot.map_count = 1;
    // SAFETY:両方の出力先はnullでなく、slot範囲はarena内に存在する。
    unsafe {
        out_data.write(arena.add(slot.offset));
        out_capacity.write(slot.capacity);
    }
    MbResult::Ok
}

#[unsafe(no_mangle)]
/// [`mb_map`]で開始した直接access貸出を1回分終了する。
///
/// # Safety
///
/// `storage`には初期化済みcontextが必要で、accessは呼出側で直列化する。
/// 最後のunmapより前に、mapしたpointerの利用者をすべて停止しなければならない。
pub unsafe extern "C" fn mb_unmap(storage: *mut c_void, handle: u32) -> MbResult {
    let ctx = match context(storage) {
        Ok(ctx) => ctx,
        Err(error) => return error,
    };
    let slot = match slot_for_handle(ctx, handle) {
        Ok(slot) => slot,
        Err(error) => return error,
    };
    if slot.map_count == 0 {
        return MbResult::InvalidArgument;
    }
    slot.map_count -= 1;
    MbResult::Ok
}

#[cfg(all(not(feature = "std"), not(test)))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    // Cへlinkするno_std libraryはABIをまたいでunwindできない。
    // 最終link側は利用環境に合ったfault方針へ置き換えてよい。
    loop {
        core::hint::spin_loop();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::mem::MaybeUninit;
    use core::ops::{Deref, DerefMut};
    use std::boxed::Box;

    #[repr(C, align(8))]
    struct Storage([u8; PUBLIC_CONTEXT_SIZE]);

    struct Fixture {
        storage: *mut Storage,
        arena: *mut [u8; 128],
    }

    impl Deref for Fixture {
        type Target = Storage;

        fn deref(&self) -> &Self::Target {
            // SAFETY: Fixtureがallocationを所有し、Dropまで解放しない。
            unsafe { &*self.storage }
        }
    }

    impl DerefMut for Fixture {
        fn deref_mut(&mut self) -> &mut Self::Target {
            // SAFETY: test内でFixtureを排他的に保持している。
            unsafe { &mut *self.storage }
        }
    }

    impl Drop for Fixture {
        fn drop(&mut self) {
            // SAFETY:両pointerはBox::into_rawで作成し、ここで一度だけ回収する。
            unsafe {
                drop(Box::from_raw(self.storage));
                drop(Box::from_raw(self.arena));
            }
        }
    }

    /// 正常に初期化された128 byte arena付きcontextを作る共通fixture。
    fn initialized() -> (Fixture, ()) {
        let storage = Box::into_raw(Box::new(Storage([0; PUBLIC_CONTEXT_SIZE])));
        let arena = Box::into_raw(Box::new([0; 128]));
        assert_eq!(
            unsafe {
                mb_init(
                    (*storage).0.as_mut_ptr().cast(),
                    (*storage).0.len(),
                    (*arena).as_mut_ptr(),
                    (*arena).len(),
                )
            },
            MbResult::Ok
        );
        (Fixture { storage, arena }, ())
    }

    /// 【正常系】確保、offset付きwrite、read、情報取得、解放の基本経路を検証する。
    ///
    /// offset 2から4 byte書くため、論理長が6になることも確認する。
    /// 最後の二重freeでは、解放済みhandleが再利用できない契約を確認する。
    #[test]
    fn alloc_write_read_and_free() {
        let (mut storage, _arena) = initialized();
        let mut handle = 0;
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 16, &mut handle),
                MbResult::Ok
            );
            let input = [1, 2, 3, 4];
            assert_eq!(
                mb_write(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    2,
                    input.as_ptr(),
                    input.len()
                ),
                MbResult::Ok
            );

            let mut output = [0; 4];
            assert_eq!(
                mb_read(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    2,
                    output.as_mut_ptr(),
                    output.len()
                ),
                MbResult::Ok
            );
            assert_eq!(output, input);

            let mut info = MaybeUninit::<MbBufferInfo>::uninit();
            assert_eq!(
                mb_get_info(storage.0.as_mut_ptr().cast(), handle, info.as_mut_ptr()),
                MbResult::Ok
            );
            let info = info.assume_init();
            assert_eq!(info.length, 6);
            assert_eq!(info.capacity, 16);
            assert_eq!(mb_free(storage.0.as_mut_ptr().cast(), handle), MbResult::Ok);
            assert_eq!(
                mb_free(storage.0.as_mut_ptr().cast(), handle),
                MbResult::InvalidHandle
            );
        }
    }

    /// 【準正常系】mapによる直接access貸出中は、所有権を壊す操作を拒否する。
    ///
    /// unmap後には通常状態へ戻り、freeできることまでを一連で確認する。
    #[test]
    fn mapped_buffer_cannot_be_freed_or_written() {
        let (mut storage, _arena) = initialized();
        let mut handle = 0;
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 16, &mut handle),
                MbResult::Ok
            );
            let mut data = ptr::null_mut();
            let mut capacity = 0;
            assert_eq!(
                mb_map(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    &mut data,
                    &mut capacity
                ),
                MbResult::Ok
            );
            assert_eq!(capacity, 16);
            assert_eq!(
                mb_free(storage.0.as_mut_ptr().cast(), handle),
                MbResult::Busy
            );
            assert_eq!(
                mb_write(storage.0.as_mut_ptr().cast(), handle, 0, data, 1),
                MbResult::Busy
            );
            let mut output = 0;
            assert_eq!(
                mb_read(storage.0.as_mut_ptr().cast(), handle, 0, &mut output, 0),
                MbResult::Busy
            );
            assert_eq!(
                mb_unmap(storage.0.as_mut_ptr().cast(), handle),
                MbResult::Ok
            );
            assert_eq!(mb_free(storage.0.as_mut_ptr().cast(), handle), MbResult::Ok);
        }
    }

    /// 【準正常系】解放した空き領域を再利用しつつ、古いhandleを拒否する。
    ///
    /// 世代番号が機能し、同じslotへ別バッファが入ってもuse-after-freeに
    /// ならないことを検証する。
    #[test]
    fn reuses_holes_and_rejects_stale_handles() {
        let (mut storage, _arena) = initialized();
        let mut first = 0;
        let mut second = 0;
        let mut replacement = 0;
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 32, &mut first),
                MbResult::Ok
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 32, &mut second),
                MbResult::Ok
            );
            assert_eq!(mb_free(storage.0.as_mut_ptr().cast(), first), MbResult::Ok);
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 24, &mut replacement),
                MbResult::Ok
            );
            assert_ne!(first, replacement);
            assert_eq!(
                mb_set_length(storage.0.as_mut_ptr().cast(), first, 1),
                MbResult::InvalidHandle
            );
        }
    }

    /// 【異常系】mb_initの全入力条件について、不正値を拒否する。
    ///
    /// null storage、null arena、storage不足、alignment違反、空arena、領域重複を
    /// 順に確認する。
    #[test]
    fn invalid_initialization_arguments_are_rejected() {
        let mut storage = Storage([0; PUBLIC_CONTEXT_SIZE]);
        let mut raw_storage = [0u8; PUBLIC_CONTEXT_SIZE + 8];
        let mut arena = [0u8; 8];
        unsafe {
            assert_eq!(
                mb_init(
                    ptr::null_mut(),
                    PUBLIC_CONTEXT_SIZE,
                    arena.as_mut_ptr(),
                    arena.len()
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_init(
                    storage.0.as_mut_ptr().cast(),
                    PUBLIC_CONTEXT_SIZE,
                    ptr::null_mut(),
                    arena.len()
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_init(
                    storage.0.as_mut_ptr().cast(),
                    PUBLIC_CONTEXT_SIZE - 1,
                    arena.as_mut_ptr(),
                    arena.len()
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_init(
                    raw_storage.as_mut_ptr().add(1).cast(),
                    PUBLIC_CONTEXT_SIZE,
                    arena.as_mut_ptr(),
                    arena.len()
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_init(
                    storage.0.as_mut_ptr().cast(),
                    PUBLIC_CONTEXT_SIZE,
                    arena.as_mut_ptr(),
                    0
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_init(
                    storage.0.as_mut_ptr().cast(),
                    PUBLIC_CONTEXT_SIZE,
                    storage.0.as_mut_ptr(),
                    8
                ),
                MbResult::InvalidArgument
            );
        }
    }

    /// 【異常系】null contextと未初期化contextを区別して通知する。
    #[test]
    fn uninitialized_and_null_contexts_are_rejected() {
        let mut storage = Storage([0; PUBLIC_CONTEXT_SIZE]);
        let mut handle = 0;
        unsafe {
            assert_eq!(
                mb_alloc(ptr::null_mut(), 1, &mut handle),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 1, &mut handle),
                MbResult::NotInitialized
            );
            assert_eq!(
                mb_reset(storage.0.as_mut_ptr().cast()),
                MbResult::NotInitialized
            );
        }
    }

    /// 【準正常・異常系】不正な確保要求と管理slot枯渇時の応答を検証する。
    ///
    /// 0 byte、null出力先、arena超過は即時拒否し、64 slot使用後はarenaに
    /// 空きが残っていてもOutOfMemoryを返す契約を確認する。
    #[test]
    fn allocation_limits_and_invalid_outputs_are_rejected() {
        let (mut storage, _arena) = initialized();
        let mut handles = [0u32; MAX_BUFFERS];
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 0, &mut handles[0]),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 1, ptr::null_mut()),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 129, &mut handles[0]),
                MbResult::OutOfMemory
            );
            for handle in &mut handles {
                assert_eq!(
                    mb_alloc(storage.0.as_mut_ptr().cast(), 1, handle),
                    MbResult::Ok
                );
            }
            let mut overflow = 0;
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 1, &mut overflow),
                MbResult::OutOfMemory
            );
        }
    }

    /// 【異常系】copy APIと論理長設定がpointer・capacity・整数overflowを検査する。
    ///
    /// 正常writeを間に入れ、read側の「論理長を越えた読取り」も独立して確認する。
    #[test]
    fn copy_and_length_boundaries_are_checked() {
        let (mut storage, _arena) = initialized();
        let mut handle = 0;
        let input = [1u8, 2, 3, 4];
        let mut output = [0u8; 4];
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 4, &mut handle),
                MbResult::Ok
            );
            assert_eq!(
                mb_write(storage.0.as_mut_ptr().cast(), handle, 0, ptr::null(), 1),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_write(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    2,
                    input.as_ptr(),
                    input.len()
                ),
                MbResult::OutOfBounds
            );
            assert_eq!(
                mb_write(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    0,
                    input.as_ptr(),
                    input.len()
                ),
                MbResult::Ok
            );
            assert_eq!(
                mb_read(storage.0.as_mut_ptr().cast(), handle, 0, ptr::null_mut(), 1),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_read(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    2,
                    output.as_mut_ptr(),
                    output.len()
                ),
                MbResult::OutOfBounds
            );
            assert_eq!(
                mb_set_length(storage.0.as_mut_ptr().cast(), handle, 5),
                MbResult::OutOfBounds
            );
            assert_eq!(
                mb_write(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    usize::MAX,
                    input.as_ptr(),
                    1
                ),
                MbResult::OutOfBounds
            );
        }
    }

    /// 【準正常・異常系】map貸出数、reset、出力pointerの契約を検証する。
    ///
    /// 二重mapを拒否し、貸出中のresetとlength変更はBusyになる。
    /// reset後は以前のhandleが無効になることまで確認する。
    #[test]
    fn map_contract_and_reset_are_enforced() {
        let (mut storage, _arena) = initialized();
        let mut handle = 0;
        let mut data = ptr::null_mut();
        let mut capacity = 0;
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 8, &mut handle),
                MbResult::Ok
            );
            assert_eq!(
                mb_map(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    ptr::null_mut(),
                    &mut capacity
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_map(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    &mut data,
                    ptr::null_mut()
                ),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_get_info(storage.0.as_mut_ptr().cast(), handle, ptr::null_mut()),
                MbResult::InvalidArgument
            );
            assert_eq!(
                mb_map(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    &mut data,
                    &mut capacity
                ),
                MbResult::Ok
            );
            assert_eq!(
                mb_map(
                    storage.0.as_mut_ptr().cast(),
                    handle,
                    &mut data,
                    &mut capacity
                ),
                MbResult::Busy
            );
            assert_eq!(
                mb_set_length(storage.0.as_mut_ptr().cast(), handle, 1),
                MbResult::Busy
            );
            assert_eq!(mb_reset(storage.0.as_mut_ptr().cast()), MbResult::Busy);
            assert_eq!(
                mb_unmap(storage.0.as_mut_ptr().cast(), handle),
                MbResult::Ok
            );
            assert_eq!(
                mb_unmap(storage.0.as_mut_ptr().cast(), handle),
                MbResult::InvalidArgument
            );
            assert_eq!(mb_reset(storage.0.as_mut_ptr().cast()), MbResult::Ok);
            assert_eq!(
                mb_free(storage.0.as_mut_ptr().cast(), handle),
                MbResult::InvalidHandle
            );
        }
    }

    /// 【境界値】handle符号化、世代番号wrap、範囲計算overflowを直接検証する。
    #[test]
    fn handle_and_generation_boundaries_are_checked() {
        assert_eq!(decode_handle(0), None);
        assert_eq!(decode_handle(1), None);
        assert_eq!(decode_handle(encode_handle(MAX_BUFFERS, 1)), None);
        assert_eq!(next_generation(0x00FF_FFFF), 1);
        assert!(!checked_range(usize::MAX, 1, usize::MAX));
        assert_eq!(ranges_overlap(0, 8, 8, 8), Some(false));
        assert_eq!(ranges_overlap(0, 8, 7, 8), Some(true));
        assert_eq!(ranges_overlap(usize::MAX, 1, 0, 1), None);
    }

    /// 【準正常系】slotには空きがあっても、arenaを使い切れば確保に失敗する。
    #[test]
    fn exhausted_arena_reports_out_of_memory() {
        let (mut storage, _arena) = initialized();
        let mut first = 0;
        let mut second = 0;
        let mut third = 0;
        unsafe {
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 64, &mut first),
                MbResult::Ok
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 64, &mut second),
                MbResult::Ok
            );
            assert_eq!(
                mb_alloc(storage.0.as_mut_ptr().cast(), 1, &mut third),
                MbResult::OutOfMemory
            );
        }
    }
}
