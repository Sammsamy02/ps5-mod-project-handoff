use core::slice;
use scanner::{Pattern,Xref};
// Pointer validity and nonoverlap remain caller obligations, as in C span APIs.
unsafe fn span<'a>(p:*const u8,n:usize)->Option<&'a [u8]>{
 if n>isize::MAX as usize || (p.is_null() && n!=0){return None;}
 Some(if n==0 {&[]} else {unsafe{slice::from_raw_parts(p,n)}})
}
unsafe fn output(values:&[usize],out:*mut u64,capacity:usize,needed:*mut usize)->i32{
 if needed.is_null() || capacity>isize::MAX as usize/core::mem::size_of::<u64>() || (out.is_null() && capacity!=0){return 1;}
 unsafe{needed.write(values.len())};if values.len()>capacity{return 3;}
 for (i,a) in values.iter().enumerate(){unsafe{out.add(i).write(*a as u64)}};0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_pattern_scan(data:*const u8,length:usize,base:u64,pattern:*const u8,pattern_length:usize,out:*mut u64,capacity:usize,needed:*mut usize)->i32{
 let Some(data)= (unsafe{span(data,length)}) else{return 1;};
 let Some(text)= (unsafe{span(pattern,pattern_length)}) else{return 1;};
 if base.checked_add(length as u64).is_none(){return 1;}
 let Ok(text)=core::str::from_utf8(text) else{return 2;};
 let Ok(p)=Pattern::new(text) else{return 2;};
 let mut found=scanner::scan_pattern(&[&p],base as usize,data);
 found[0].sort_unstable();unsafe{output(&found[0],out,capacity,needed)}
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_xref_scan(data:*const u8,length:usize,base:u64,target:u64,out:*mut u64,capacity:usize,needed:*mut usize)->i32{
 let Some(data)= (unsafe{span(data,length)}) else{return 1;};
 if base.checked_add(length as u64).is_none(){return 1;}
 let mut found=scanner::scan_xref(&[&Xref(target as usize)],base as usize,data);
 found[0].sort_unstable();unsafe{output(&found[0],out,capacity,needed)}
}
