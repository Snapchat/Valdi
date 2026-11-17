mod bigint;
mod bytes;
mod collections;
mod conversion;
mod ffi;
mod function;
mod handle;
mod marshaller;
mod promise;
mod raw;
mod registration;
mod runtime;
mod value;

pub use bigint::*;
pub use bytes::*;
pub use collections::*;
pub use conversion::*;
pub use function::*;
pub use handle::*;
pub use marshaller::*;
pub use promise::*;
pub use raw::*;
pub use registration::*;
pub use runtime::*;
pub use value::*;

#[cfg(test)]
mod tests {
    use super::{ValdiBuffer, ValdiSlice, ffi_boundary, valdi_rust_buffer_drop};

    #[test]
    fn borrowed_slice_does_not_copy() {
        let bytes = b"valdi";
        let slice = ValdiSlice::from_bytes(bytes);
        assert_eq!(slice.data, bytes.as_ptr());
        assert_eq!(unsafe { slice.as_bytes() }, bytes);
    }

    #[test]
    fn owned_buffer_round_trips() {
        let buffer = ValdiBuffer::from_vec(vec![1, 2, 3]);
        assert_eq!(
            unsafe { std::slice::from_raw_parts(buffer.data, buffer.len) },
            [1, 2, 3]
        );
        valdi_rust_buffer_drop(buffer);
    }

    #[test]
    fn ffi_boundary_translates_panics() {
        let status = ffi_boundary(|| panic!("boom"));
        assert_eq!(status.code, 1);
        valdi_rust_buffer_drop(status.message);
    }
}
