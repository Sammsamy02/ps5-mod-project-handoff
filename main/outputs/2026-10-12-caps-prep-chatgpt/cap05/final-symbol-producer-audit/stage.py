"""Source-only hash-pinned producer stage. Never invokes compiler or originals."""
import sys
sys.dont_write_bytecode=True
import hashlib,json
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3]
BASE=R/'outputs/2026-10-08-l26-package-A/pass-1/harness'
SOURCES={'writer':BASE/'tools/SharpProspero.Link/DynamicWriter.cs','harness':BASE/'Program.cs'}
PINS={'writer':'03a639acf1993636239dd9e22b6af0c9e16cb118d4d0022a97ce40ee98d32de1','harness':'686d4596a156d949cc026649c86359f46bf4309e9a7f2e8da3198984cd35f863'}
POST='''
        // Final-layout evidence only: never reconstruct addresses from a partial-link map.
        if (finalSymbolSink is not null)
        {
            var functions = new List<(string Name, ulong Address, ulong Size)>();
            foreach (ElfObject obj in resolution.Included)
                foreach (ElfSymbol sym in obj.Symbols)
                {
                    if (sym.IsUndefined || sym.Type != SymType.Func || sym.Name.Length == 0 || sym.Size == 0)
                        continue;
                    if (sym.SectionIndex <= 0 || sym.SectionIndex >= obj.Sections.Count
                        || resolution.DroppedSections.Contains((obj, sym.SectionIndex)))
                        continue;
                    ElfSection section = obj.Sections[sym.SectionIndex];
                    if (!section.IsAlloc || !section.IsExecutable || section.IsNoBits)
                        continue;
                    if (sym.Value > section.Size || sym.Size > section.Size - sym.Value)
                        throw new ElfLinkException("Final symbol range escapes its retained section: " + sym.Name);
                    ulong address = SymbolValue(resolution, importByName, SectionAddr, obj, sym, linkerDefined);
                    if (sym.Size > ulong.MaxValue - address)
                        throw new ElfLinkException("Final symbol range overflows: " + sym.Name);
                    functions.Add((sym.Name, address, sym.Size));
                }
            if (functions.Count == 0)
                throw new ElfLinkException("Final symbol producer found no bounded functions.");
            finalSymbolSink(functions);
        }
        return finalBytes;'''
HARNESS_PREFIX='''    object[]? finalSymbols = null;
    byte[] bytes = DynamicWriter.Write(explicitResolution, null, ModuleKind.Library,'''
HARNESS_CALL='''        libraryInitSymbol: "module_start", libraryFiniSymbol: "module_stop", explicitLifecycleOwnsArrays: true,
        finalSymbolSink: rows => finalSymbols = rows.Select(s => new {
            name = s.Name, addr = "0x" + s.Address.ToString("x"), size = "0x" + s.Size.ToString("x"),
            tier = "FINAL-LINK-BOUNDED-FUNCTION"
        }).ToArray());
    if (finalSymbols is null || finalSymbols.Length == 0)
        throw new InvalidDataException("Final symbol producer returned no bounded functions.");'''
WRITE='    using (var file = new FileStream(output, FileMode.CreateNew, FileAccess.Write, FileShare.None)) file.Write(bytes);'
HARNESS_POST='''
    // Exact final writer bytes, before SDK stamp/wrapper. Final package binding is a separate step.
    byte[] producerBytes = JsonSerializer.SerializeToUtf8Bytes(new {
        schema = 1, kind = "final-layout-symbol-producer",
        bare_elf_sha256 = Convert.ToHexStringLower(SHA256.HashData(bytes)), bare_elf_bytes = bytes.Length,
        address_convention = "final ELF virtual address; runtime module-relative only after base convention validation",
        input_object_sha256 = Convert.ToHexStringLower(SHA256.HashData(objectBytes)),
        contract_sha256 = Convert.ToHexStringLower(SHA256.HashData(contractBytes)),
        packager_assembly_sha256 = Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(System.Reflection.Assembly.GetExecutingAssembly().Location))),
        link_writer_assembly_sha256 = Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(typeof(DynamicWriter).Assembly.Location))),
        functions = finalSymbols,
        coverage = "retained named defined executable FUNC symbols with nonzero bounded size only; aliases retained; zero-size/absolute/discarded/linker-generated symbols excluded",
        packaged_prx_binding = "UNBOUND; owner must validate transforms and attach exact packaged PRX SHA",
        runtime_execution_proven = false
    });
    using (var file = new FileStream(output + ".final-symbols.json", FileMode.CreateNew, FileAccess.Write, FileShare.None)) file.Write(producerBytes);'''
TAIL='            dynamicAddr, dynamicBytes, comment, versionBlob);'
EDITS={'writer':[
 ('        string? libraryFiniSymbol = null, bool explicitLifecycleOwnsArrays = false)', '        string? libraryFiniSymbol = null, bool explicitLifecycleOwnsArrays = false,\n        Action<IReadOnlyList<(string Name, ulong Address, ulong Size)>>? finalSymbolSink = null)'),
 ('        return WriteFile(resolution, kind, entry, sectionData, SectionAddr, outputSections,','        byte[] finalBytes = WriteFile(resolution, kind, entry, sectionData, SectionAddr, outputSections,'),
 (TAIL,TAIL+POST)],'harness':[
 ('    if (File.Exists(output) || output == Path.GetFullPath(args[0]) || output == Path.GetFullPath(args[1]))','    if (File.Exists(output) || File.Exists(output + ".final-symbols.json") || output == Path.GetFullPath(args[0]) || output == Path.GetFullPath(args[1]))'),
 ('    byte[] bytes = DynamicWriter.Write(explicitResolution, null, ModuleKind.Library,',HARNESS_PREFIX),
 ('        libraryInitSymbol: "module_start", libraryFiniSymbol: "module_stop", explicitLifecycleOwnsArrays: true);',HARNESS_CALL),
 (WRITE,WRITE+HARNESS_POST)]}
def replace_exact(text,edits):
 for old,new in edits:
  if text.count(old)!=1:raise ValueError('anchor count must be1: '+old[:70])
  text=text.replace(old,new,1)
 return text
def transform(key,raw):
 if hashlib.sha256(raw).hexdigest()!=PINS[key]:raise ValueError('source hash drift: '+key)
 return replace_exact(raw.decode(),EDITS[key]).encode()
def inverse(key,derived):return replace_exact(derived.decode(),[(new,old) for old,new in reversed(EDITS[key])]).encode()
def main():
 rows=[];dest=H/'staged';dest.mkdir(exist_ok=True)
 for key,p in SOURCES.items():
  original=p.read_bytes();derived=transform(key,original);assert inverse(key,derived)==original
  out=dest/('DynamicWriter.cs' if key=='writer' else 'Program.cs');out.write_bytes(derived)
  assert p.read_bytes()==original
  rows.append({'key':key,'source':str(p.relative_to(R)),'source_sha256':PINS[key],'derived':str(out.relative_to(R)),'derived_sha256':hashlib.sha256(derived).hexdigest(),'replacements':len(EDITS[key]),'inverse_exact':True,'source_unchanged':True})
 (H/'stage-receipt.json').write_text(json.dumps({'label':'Source-only unapplied/uncompiled producer stage; not a symbol artifact','files':rows,'builds':0,'original_writes':0,'symbols_produced':0,'package_binder_implemented':False,'current_B7_dependency':False},indent=2)+'\n')
 print('PASS:2 source hashes,7 unique replacements,2 exact inverses,2 originals unchanged;0 builds/symbols produced')
if __name__=='__main__':main()
