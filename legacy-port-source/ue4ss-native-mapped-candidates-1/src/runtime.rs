#[cfg(feature="native-runtime")]
mod runtime {
 use core::alloc::{GlobalAlloc,Layout};
 unsafe extern "C" {fn malloc(n:usize)->*mut u8;fn free(p:*mut u8);fn abort()->!;}
 struct CAllocator;
 unsafe impl GlobalAlloc for CAllocator {
  unsafe fn alloc(&self,l:Layout)->*mut u8 {
   let align=l.align().max(core::mem::align_of::<usize>());
   let Some(n)=l.size().checked_add(align).and_then(|x|x.checked_add(core::mem::size_of::<usize>())) else {return core::ptr::null_mut();};
   let base=unsafe{malloc(n)};if base.is_null(){return base;}
   let start=base as usize+core::mem::size_of::<usize>();
   let aligned=(start+align-1)&!(align-1);
   unsafe{((aligned as *mut usize).sub(1)).write(base as usize)};aligned as *mut u8
  }
  unsafe fn dealloc(&self,p:*mut u8,_:Layout){unsafe{free(((p as *mut usize).sub(1)).read() as *mut u8)}}
 }
 #[global_allocator]static ALLOC:CAllocator=CAllocator;
 #[panic_handler]fn panic(_: &core::panic::PanicInfo)->!{unsafe{abort()}}
}

