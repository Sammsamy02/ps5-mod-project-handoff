#[cfg(test)]
unsafe extern "C" {fn owned_tick_owner_controls(path:*const core::ffi::c_char)->i32;}
#[test]fn selected_cpp_owned_tick_path(){std::hint::black_box(patternsleuth_native_engine_version::ps_ugameengine_tick_file_image);let p=std::path::Path::new(env!("CARGO_MANIFEST_DIR")).parent().unwrap().join("tests/owned-tick.elf");let p=std::ffi::CString::new(p.to_str().unwrap()).unwrap();assert_eq!(unsafe{owned_tick_owner_controls(p.as_ptr())},0);}
