use alloc::vec::Vec;
use crate::{resolvers::{AsyncContext,ResolveError,ensure_one},util,xrefs};
// Exact shared ELF/PE UGameEngineTick selection from pinned upstream game_loop.rs.
// Scheduler aggregation becomes synchronous; only actual metadata roots count.
pub fn resolve_ugameengine_tick(ctx:&AsyncContext<'_>)->Result<u64,ResolveError> {
 let strings:Vec<_>=["causeevent=\0","CAUSEEVENT \0"].into_iter().flat_map(|s|ctx.scan(util::utf16_pattern(s))).collect();
 let references=xrefs::scan_xrefs(ctx,&strings);
 let roots=references.into_iter().map(|r|ctx.image().get_root_function(r).map(|f|f.map(|f|f.range.start)).map_err(ResolveError::from)).collect::<Result<Vec<_>,_>>()?;
 ensure_one(roots.into_iter().flatten())
}
