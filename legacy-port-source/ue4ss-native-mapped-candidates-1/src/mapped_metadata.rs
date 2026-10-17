//! Exact bounded metadata gathering. This copies only existing supplied slices;
//! runtime addresses are lookup keys/base facts, never sources of raw pointers.
use alloc::{borrow::Cow,vec::Vec};
use crate::{OwnedImage,function_ranges::MetadataError};
pub(crate) fn bytes<'a>(image:&OwnedImage<'a>,address:u64,size:usize)->Result<Cow<'a,[u8]>,MetadataError>{
 if size==0||size>isize::MAX as usize{return Err(MetadataError::OutOfFile);}
 let end=address.checked_add(size as u64).ok_or(MetadataError::OutOfFile)?;
 let piece=|current:u64|{
  image.memory.sections.iter().find_map(|s|{
   let offset=usize::try_from(current.checked_sub(s.address)?).ok()?;
   if offset>=s.data.len(){return None;}
   let supplied:&'a[u8]=s.data;
   let remaining=supplied.get(offset..)?;
   let count=usize::try_from(end-current).ok()?.min(remaining.len());
   Some(&remaining[..count])
  })
 };
 let first=piece(address).ok_or(MetadataError::OutOfFile)?;
 if first.len()==size{return Ok(Cow::Borrowed(first));}
 // Prove contiguous complete supplied coverage BEFORE allocating or copying.
 let mut cursor=address;
 while cursor<end{let supplied=piece(cursor).ok_or(MetadataError::OutOfFile)?;
  if supplied.is_empty(){return Err(MetadataError::OutOfFile);}
  cursor=cursor.checked_add(supplied.len() as u64).ok_or(MetadataError::OutOfFile)?;
 }
 let mut gathered=Vec::with_capacity(size);cursor=address;
 while cursor<end{let supplied=piece(cursor).ok_or(MetadataError::OutOfFile)?;
  gathered.extend_from_slice(supplied);cursor+=supplied.len() as u64;
 }
 Ok(Cow::Owned(gathered))
}

// Header-only ELF gives an encoded frame pointer but no explicit extent. Match
// the original read-to-supplied-end behavior over adjacent supplied spans only.
pub(crate) fn to_end<'a>(image:&OwnedImage<'a>,address:u64)->Result<Cow<'a,[u8]>,MetadataError>{
 let mut end=address;
 loop{
  let next=image.memory.sections.iter().find_map(|s|{
   let offset=usize::try_from(end.checked_sub(s.address)?).ok()?;
   if offset>=s.data.len(){return None;}
   s.address.checked_add(s.data.len() as u64)
  });
  match next{Some(value)if value>end=>end=value,_=>break}
 }
 let size=usize::try_from(end.checked_sub(address).ok_or(MetadataError::OutOfFile)?).map_err(|_|MetadataError::OutOfFile)?;
 bytes(image,address,size)
}
