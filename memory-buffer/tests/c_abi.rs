use core::mem::{MaybeUninit, align_of};
use core::ops::{Deref, DerefMut};
use core::ptr;
use memory_buffer::{
    MB_CONTEXT_ALIGNMENT, MB_CONTEXT_SIZE, MB_MAX_BUFFERS, MbBufferInfo, MbResult, mb_alloc,
    mb_free, mb_get_info, mb_init, mb_map, mb_read, mb_reset, mb_set_length, mb_unmap, mb_write,
};

#[repr(C, align(8))]
struct Storage([u8; MB_CONTEXT_SIZE]);

const _: () = assert!(align_of::<Storage>() >= MB_CONTEXT_ALIGNMENT);

struct Fixture {
    storage: *mut Storage,
    arena: *mut [u8; 256],
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

/// 公開ABIだけを使って、結合テスト用contextを初期化する。
fn fixture() -> (Fixture, ()) {
    let storage = Box::into_raw(Box::new(Storage([0; MB_CONTEXT_SIZE])));
    let arena = Box::into_raw(Box::new([0; 256]));
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

/// 【正常系・結合】C利用者が使う基本的なcopy lifecycleを公開ABI経由で検証する。
///
/// allocからfreeまでを1シナリオとして実行し、書込み後の論理長とデータ一致を確認する。
#[test]
fn normal_copy_lifecycle_works_through_public_abi() {
    let (mut storage, _arena) = fixture();
    let input = [0x10, 0x20, 0x30, 0x40];
    let mut output = [0; 4];
    let mut handle = 0;
    let mut info = MaybeUninit::<MbBufferInfo>::uninit();

    unsafe {
        assert_eq!(
            mb_alloc(storage.0.as_mut_ptr().cast(), 16, &mut handle),
            MbResult::Ok
        );
        assert_eq!(
            mb_write(
                storage.0.as_mut_ptr().cast(),
                handle,
                0,
                input.as_ptr(),
                input.len(),
            ),
            MbResult::Ok
        );
        assert_eq!(
            mb_get_info(storage.0.as_mut_ptr().cast(), handle, info.as_mut_ptr()),
            MbResult::Ok
        );
        assert_eq!(info.assume_init().length, input.len());
        assert_eq!(
            mb_read(
                storage.0.as_mut_ptr().cast(),
                handle,
                0,
                output.as_mut_ptr(),
                output.len(),
            ),
            MbResult::Ok
        );
        assert_eq!(output, input);
        assert_eq!(mb_free(storage.0.as_mut_ptr().cast(), handle), MbResult::Ok);
    }
}

/// 【準正常系・結合】DMA相当のmap貸出中は変更・解放できないことを検証する。
///
/// Driverがpointerへ直接書き込む状況を再現し、完了通知後のunmap、論理長設定、
/// freeまで正常に復帰できることを確認する。
#[test]
fn dma_loan_blocks_mutation_until_completion() {
    let (mut storage, _arena) = fixture();
    let mut handle = 0;
    let mut data = ptr::null_mut();
    let mut capacity = 0;

    unsafe {
        assert_eq!(
            mb_alloc(storage.0.as_mut_ptr().cast(), 32, &mut handle),
            MbResult::Ok
        );
        assert_eq!(
            mb_map(
                storage.0.as_mut_ptr().cast(),
                handle,
                &mut data,
                &mut capacity,
            ),
            MbResult::Ok
        );
        assert_eq!(capacity, 32);
        data.write(0x55);
        assert_eq!(
            mb_set_length(storage.0.as_mut_ptr().cast(), handle, 1),
            MbResult::Busy
        );
        assert_eq!(
            mb_free(storage.0.as_mut_ptr().cast(), handle),
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
        assert_eq!(
            mb_set_length(storage.0.as_mut_ptr().cast(), handle, 1),
            MbResult::Ok
        );
        assert_eq!(mb_free(storage.0.as_mut_ptr().cast(), handle), MbResult::Ok);
    }
}

/// 【準正常・異常系・結合】資源枯渇と契約違反が識別可能な結果codeになることを検証する。
///
/// 0 byte要求、slot枯渇、reset後の古いhandleという代表的な失敗軸を扱う。
#[test]
fn resource_pressure_and_contract_errors_are_visible() {
    let (mut storage, _arena) = fixture();
    let mut handles = [0; MB_MAX_BUFFERS];

    unsafe {
        assert_eq!(
            mb_alloc(storage.0.as_mut_ptr().cast(), 0, &mut handles[0]),
            MbResult::InvalidArgument
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
        assert_eq!(mb_reset(storage.0.as_mut_ptr().cast()), MbResult::Ok);
        assert_eq!(
            mb_free(storage.0.as_mut_ptr().cast(), handles[0]),
            MbResult::InvalidHandle
        );
    }
}
