//! Additive optional property leaf. Binding wire enums preserve unknown/Off.
use core::mem::{size_of,align_of};
use crate::{ps_scan_abi::PsMappedImageViewV1,mapped_api::from_view,resolvers::AsyncContext,build_properties::{self,BuildConfiguration,Stats}};
#[repr(C)]#[derive(Clone,Copy,Debug,Default)]pub struct PsMappedPropertyResult{pub value:u32,pub reserved:u32}
const _:()={assert!(size_of::<PsMappedPropertyResult>()==8);assert!(align_of::<PsMappedPropertyResult>()==4);};
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ps_resolve_mapped_property_v1(view:*const PsMappedImageViewV1,property_id:u32,out:*mut PsMappedPropertyResult)->i32{
 if out.is_null()||(out as usize)%align_of::<PsMappedPropertyResult>()!=0{return 1;}
 unsafe{out.write(PsMappedPropertyResult::default())};
 if view.is_null()||(view as usize)%align_of::<PsMappedImageViewV1>()!=0||!matches!(property_id,1|2){return 1;}
 let Ok(image)=(unsafe{from_view(&*view)})else{return 2;};
 let ctx=AsyncContext{exe:image.image()};
 let value=if property_id==1{build_properties::resolve_build_configuration(&ctx).map(|c|match c{BuildConfiguration::Shipping=>1,BuildConfiguration::Development=>2,BuildConfiguration::Test=>3})}else{build_properties::resolve_stats(&ctx).map(|s|match s{Stats::Off=>1,Stats::On=>2})};
 match value{Ok(value)=>{unsafe{out.write(PsMappedPropertyResult{value,reserved:0})};0},Err(_)=>3}
}
