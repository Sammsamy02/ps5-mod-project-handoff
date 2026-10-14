// metal_bcn_check.swift -- the GPU as an INDEPENDENT texture decoder (Mac-only, read-only, generic).
//
// The Mac's own GPU decodes the blocks in hardware (Apple Metal, BC formats), so this
// decoder shares no code with bcn_swap.py (the writer) or bcn_decode.py (the Python
// reference). Public API only.
//
//   metal_bcn_check compare FORMAT BEFORE AFTER
//       decodes every block of both files and checks AFTER == BEFORE with R and B
//       exchanged on EVERY texel (all 16 texels of every block). Prints one JSON line.
//   metal_bcn_check decode FORMAT IN OUT
//       writes RGBA8 (round(v*255)) for every texel, 16 texels per block in row-major
//       order inside the block (1 texel per 'block' for the uncompressed formats).
//       Prints a JSON line with the count of channel values that were not exact k/255.
//
// FORMAT: PF_DXT1 PF_DXT3 PF_DXT5 PF_BC7 PF_B8G8R8A8 PF_R8G8B8A8.
// Exit 0 only when the run is complete (compare: and exact); 1 on any failure.
import Foundation
import Metal

func fail(_ s: String) -> Never {
    print("{\"decoder\":\"metal\",\"error\":\"\(s)\"}")
    exit(1)
}

let args = CommandLine.arguments
if args.count != 5 { fail("usage: compare|decode FORMAT A B") }
let cmd = args[1], fmt = args[2]
let formats: [String: (MTLPixelFormat, Int, Bool)] = [
    "PF_DXT1": (.bc1_rgba, 8, true), "PF_DXT3": (.bc2_rgba, 16, true), "PF_DXT5": (.bc3_rgba, 16, true),
    "PF_BC7": (.bc7_rgbaUnorm, 16, true), "PF_B8G8R8A8": (.bgra8Unorm, 4, false),
    "PF_R8G8B8A8": (.rgba8Unorm, 4, false)]
guard let (pf, bb, compressed) = formats[fmt] else { fail("unsupported format \(fmt)") }
guard let dev = MTLCreateSystemDefaultDevice() else { fail("no Metal device") }
if compressed && !dev.supportsBCTextureCompression { fail("GPU has no BC support") }
guard let a = FileManager.default.contents(atPath: args[3]) else { fail("cannot read input A") }
if a.count == 0 || a.count % bb != 0 { fail("input A is empty or not whole blocks") }
let n = a.count / bb
let perRow = compressed ? min(n, 4096) : min(n, 16384)
let rows = (n + perRow - 1) / perRow
let w = compressed ? 4 * perRow : perRow
let h = compressed ? 4 * rows : rows

func makeTexture(_ d: Data) -> MTLTexture {
    let desc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: pf, width: w, height: h, mipmapped: false)
    desc.usage = [.shaderRead]
    desc.storageMode = .shared
    guard let t = dev.makeTexture(descriptor: desc) else { fail("texture allocation failed") }
    var padded = d
    let need = perRow * rows * bb
    if padded.count < need { padded.append(Data(count: need - padded.count)) }
    padded.withUnsafeBytes { p in
        t.replace(region: MTLRegionMake2D(0, 0, w, h), mipmapLevel: 0, withBytes: p.baseAddress!,
                  bytesPerRow: perRow * bb)
    }
    return t
}

let src = """
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(coord::pixel, filter::nearest, address::clamp_to_edge);
kernel void cmp(texture2d<float> A [[texture(0)]], texture2d<float> B [[texture(1)]],
                device atomic_uint* c [[buffer(0)]], constant uint4& p [[buffer(1)]],
                uint2 g [[thread_position_in_grid]]) {
  if (g.x >= A.get_width() || g.y >= A.get_height()) return;
  uint blk = p.z ? (g.y / 4) * p.y + g.x / 4 : g.y * p.y + g.x;
  if (blk >= p.x) return;
  float2 q = float2(g) + 0.5;
  float4 a = A.sample(S, q);
  float4 b = B.sample(S, q);
  atomic_fetch_add_explicit(&c[0], 1u, memory_order_relaxed);
  if (!(b.x == a.z && b.y == a.y && b.z == a.x && b.w == a.w)) atomic_fetch_add_explicit(&c[1], 1u, memory_order_relaxed);
  if (a.x != a.z) atomic_fetch_add_explicit(&c[2], 1u, memory_order_relaxed);
}
kernel void dec(texture2d<float> A [[texture(0)]], device uchar4* o [[buffer(0)]],
                device atomic_uint* c [[buffer(2)]], constant uint4& p [[buffer(1)]],
                uint2 g [[thread_position_in_grid]]) {
  if (g.x >= A.get_width() || g.y >= A.get_height()) return;
  uint blk, k;
  if (p.z) { blk = (g.y / 4) * p.y + g.x / 4; k = (g.y % 4) * 4 + (g.x % 4); }
  else { blk = g.y * p.y + g.x; k = 0; }
  if (blk >= p.x) return;
  float4 a = A.sample(S, float2(g) + 0.5);
  float4 s = round(a * 255.0);
  if (any(abs(a * 255.0 - s) > 1e-3)) atomic_fetch_add_explicit(&c[0], 1u, memory_order_relaxed);
  o[blk * (p.z ? 16 : 1) + k] = uchar4(clamp(s, 0.0, 255.0));
}
"""
let lib: MTLLibrary
do { lib = try dev.makeLibrary(source: src, options: nil) } catch { fail("shader compile failed") }
guard let q = dev.makeCommandQueue() else { fail("no command queue") }
var params: [UInt32] = [UInt32(n), UInt32(perRow), compressed ? 1 : 0, 0]

// buffers: (index, buffer); the parameters always go to index 1
func run(_ name: String, _ textures: [MTLTexture], _ buffers: [(Int, MTLBuffer)]) {
    guard let f = lib.makeFunction(name: name), let ps = try? dev.makeComputePipelineState(function: f),
          let cb = q.makeCommandBuffer(), let enc = cb.makeComputeCommandEncoder() else { fail("pipeline failed") }
    enc.setComputePipelineState(ps)
    for (i, t) in textures.enumerated() { enc.setTexture(t, index: i) }
    for (i, b) in buffers { enc.setBuffer(b, offset: 0, index: i) }
    enc.setBytes(&params, length: 16, index: 1)
    let tg = MTLSize(width: 16, height: 16, depth: 1)
    enc.dispatchThreadgroups(MTLSize(width: (w + 15) / 16, height: (h + 15) / 16, depth: 1), threadsPerThreadgroup: tg)
    enc.endEncoding()
    cb.commit()
    cb.waitUntilCompleted()
    if cb.status != .completed { fail("GPU command failed") }
}

let ta = makeTexture(a)
if cmd == "compare" {
    guard let b = FileManager.default.contents(atPath: args[4]) else { fail("cannot read input B") }
    if b.count != a.count { fail("inputs differ in size") }
    let tb = makeTexture(b)
    guard let counters = dev.makeBuffer(length: 16, options: .storageModeShared) else { fail("buffer") }
    memset(counters.contents(), 0, 16)
    run("cmp", [ta, tb], [(0, counters)])
    let c = counters.contents().bindMemory(to: UInt32.self, capacity: 4)
    let texels = Int(c[0]), bad = Int(c[1]), differs = Int(c[2])
    let want = compressed ? 16 * n : n
    let exact = bad == 0 && texels == want && texels > 0
    print("{\"decoder\":\"metal:\(dev.name)\",\"format\":\"\(fmt)\",\"blocks\":\(n),\"texels_compared\":\(texels),"
          + "\"texels_expected\":\(want),\"mismatched_texels\":\(bad),\"texels_where_r_differs_from_b\":\(differs),"
          + "\"exact\":\(exact)}")
    exit(exact ? 0 : 1)
} else if cmd == "decode" {
    let outLen = (compressed ? 16 * n : n) * 4
    guard let ob = dev.makeBuffer(length: outLen, options: .storageModeShared),
          let cnt = dev.makeBuffer(length: 16, options: .storageModeShared) else { fail("buffer") }
    memset(cnt.contents(), 0, 16)
    run("dec", [ta], [(0, ob), (2, cnt)])
    let inexact = Int(cnt.contents().bindMemory(to: UInt32.self, capacity: 4)[0])
    let d = Data(bytes: ob.contents(), count: outLen)
    if !FileManager.default.createFile(atPath: args[4], contents: d) { fail("cannot write output") }
    print("{\"decoder\":\"metal:\(dev.name)\",\"format\":\"\(fmt)\",\"blocks\":\(n),\"texels\":\(outLen / 4),"
          + "\"inexact_texels\":\(inexact)}")
    exit(0)
} else {
    fail("unknown command")
}
