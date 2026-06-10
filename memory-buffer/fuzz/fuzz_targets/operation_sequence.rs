#![no_main]

use core::ptr;
use libfuzzer_sys::fuzz_target;
use memory_buffer::{
    MB_CONTEXT_SIZE, MbBufferInfo, MbResult, mb_alloc, mb_free, mb_get_info, mb_init, mb_map,
    mb_read, mb_reset, mb_set_length, mb_unmap, mb_write,
};

#[repr(C, align(8))]
struct Storage([u8; MB_CONTEXT_SIZE]);

fuzz_target!(|input: &[u8]| {
    let mut storage = Box::new(Storage([0; MB_CONTEXT_SIZE]));
    let mut arena = Box::new([0u8; 256]);
    let mut handles = [0u32; 8];
    let mut mapped = [false; 8];

    assert_eq!(
        unsafe {
            mb_init(
                storage.0.as_mut_ptr().cast(),
                storage.0.len(),
                arena.as_mut_ptr(),
                arena.len(),
            )
        },
        MbResult::Ok
    );

    for command in input.chunks(4).take(1024) {
        let opcode = command.first().copied().unwrap_or(0) % 9;
        let index = usize::from(command.get(1).copied().unwrap_or(0)) % handles.len();
        let value = usize::from(command.get(2).copied().unwrap_or(0));
        let length = usize::from(command.get(3).copied().unwrap_or(0)) % 32;
        let context = storage.0.as_mut_ptr().cast();

        unsafe {
            match opcode {
                0 => {
                    if handles[index] == 0 {
                        let result = mb_alloc(context, (value % 64) + 1, &mut handles[index]);
                        assert!(matches!(result, MbResult::Ok | MbResult::OutOfMemory));
                    }
                }
                1 => {
                    let result = mb_free(context, handles[index]);
                    if result == MbResult::Ok {
                        handles[index] = 0;
                        mapped[index] = false;
                    }
                }
                2 => {
                    let source = [command.get(2).copied().unwrap_or(0); 32];
                    let _ = mb_write(context, handles[index], value % 64, source.as_ptr(), length);
                }
                3 => {
                    let mut destination = [0u8; 32];
                    let _ = mb_read(
                        context,
                        handles[index],
                        value % 64,
                        destination.as_mut_ptr(),
                        length,
                    );
                }
                4 => {
                    let _ = mb_set_length(context, handles[index], value % 64);
                }
                5 => {
                    let mut data = ptr::null_mut();
                    let mut capacity = 0;
                    let result = mb_map(
                        context,
                        handles[index],
                        &mut data,
                        &mut capacity,
                    );
                    if result == MbResult::Ok {
                        mapped[index] = true;
                        if capacity != 0 {
                            data.write(command.get(3).copied().unwrap_or(0));
                        }
                    }
                }
                6 => {
                    let result = mb_unmap(context, handles[index]);
                    if result == MbResult::Ok {
                        mapped[index] = false;
                    }
                }
                7 => {
                    let mut info = MbBufferInfo {
                        length: 0,
                        capacity: 0,
                        mapped: 0,
                        reserved: [0; 7],
                    };
                    let _ = mb_get_info(context, handles[index], &mut info);
                }
                _ => {
                    if !mapped.iter().any(|value| *value) {
                        let result = mb_reset(context);
                        assert_eq!(result, MbResult::Ok);
                        handles.fill(0);
                    }
                }
            }
        }
    }
});

