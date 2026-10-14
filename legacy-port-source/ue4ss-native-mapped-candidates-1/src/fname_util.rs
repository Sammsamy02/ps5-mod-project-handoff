use alloc::vec::Vec;
use crate::resolvers::{AsyncContext,ResolveError};
pub use crate::util::utf16_pattern;
pub use crate::xrefs::{scan_xrefs,scan_xcalls};
pub fn root_functions(ctx:&AsyncContext<'_>,addresses:&[u64])->Result<Vec<u64>,ResolveError>{
 Ok(addresses.iter().map(|r|->Result<_,ResolveError>{Ok(ctx.image().get_root_function(*r)?.map(|f|f.range.start))}).collect::<Result<Vec<_>,_>>()?.into_iter().flatten().collect())
}
