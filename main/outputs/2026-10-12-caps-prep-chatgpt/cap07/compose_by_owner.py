"""Mac-only all-or-nothing per-owner composition; no runtime or publication.

The supplied catalogue is a caller-selected scope. An unmatched placement is
explicit configuration, never evidence that a package is new. Physical base
UCAS identity, material compatibility and engine precedence remain unproved.
"""
import hashlib
from pathlib import Path
import tempfile


def compose(api, projector, bases, mods, unmatched_placements, scratch_dir):
    """Return ({base_id: {utoc, sidecar, builder_receipt}}, receipt).

    bases: dictionaries with base_id, utoc bytes, header bytes, ucas_size.
    mods: unchanged source ModSpecs. unmatched_placements: (mod_id, int PID)
    to base_id. scratch_dir must be an existing directory inside cap07.
    Every target is returned together or an exception is raised.
    """
    def require(ok, why):
        if not ok:
            raise ValueError(why)

    digest = lambda value: hashlib.sha256(value).hexdigest()
    scratch = Path(scratch_dir).resolve()
    require(scratch.is_dir() and scratch.is_relative_to(Path(__file__).resolve().parent),
            'scratch_must_remain_in_cap07')
    require(bases and mods, 'empty_composition_inputs')
    require(isinstance(unmatched_placements, dict), 'invalid_unmatched_placements')
    ordered = api.effective_mod_order(mods)
    require(len({m.mod_id for m in ordered}) == len(ordered), 'duplicate_mod_id')

    def identities():
        return [(m.mod_id, api.sha256_file(m.utoc_path), api.sha256_file(m.ucas_path)) for m in ordered]

    expected = [(m.mod_id, m.utoc_sha, m.ucas_sha) for m in ordered]
    require(identities() == expected, 'source_identity_changed')
    validated, owners, base_sources, container_ids = {}, {}, [], set()
    for source in bases:
        name = source['base_id']
        require(isinstance(name, str) and name and name not in validated, 'invalid_duplicate_base_id')
        raw = source.get('header')
        require(isinstance(raw, bytes) and raw, 'base_header_unavailable')
        require(isinstance(source['utoc'], bytes), 'base_utoc_must_be_immutable_bytes')
        size = source['ucas_size']
        require(type(size) is int, 'invalid_base_ucas_size')
        toc = api.parse_toc(source['utoc'])
        require(toc.container_id not in container_ids, 'duplicate_base_container_identity')
        container_ids.add(toc.container_id)
        api.validate_lookup(toc)
        api.validate_single_partition(toc, size, permit_tail_padding=True)
        header = api.parse_container_header(raw)
        require(header.container_id == int.from_bytes(toc.container_id, 'little'),
                'base_container_header_identity_mismatch')
        header_id = toc.container_id + bytes.fromhex('00000006')
        require(header_id in toc.ids, 'base_header_chunk_missing')
        require(api.blake3_20(raw) == toc.meta(toc.ids.index(header_id))[:20],
                'base_header_content_hash_mismatch')
        exports = {api.package_id_of_chunk(c) for c in toc.ids if c[11] == 1}
        require(exports == set(header.packages), 'base_export_store_coverage_mismatch')
        require(not any(pid in owners for pid in exports), 'ambiguous_base_package_owner')
        owners.update({pid:name for pid in exports})
        validated[name] = source
        base_sources.append({'base_id':name,'utoc_sha256':digest(source['utoc']),
            'header_sha256':digest(raw),'container_id_u64_hex':'%016x'%header.container_id,
            'declared_ucas_size':size,'physical_ucas_identity_verified':False})

    conflicts = api.plan_conflicts(ordered)
    require(not any(conflicts[k] for k in ('chunk_id_conflicts','virtual_path_conflicts','package_id_conflicts')),
            'mod_conflict_rejected')
    assignments, used_placements, mod_headers = {}, set(), {}
    for mod in ordered:
        header = api.parse_container_header(api.read_mod_header_bytes(mod))
        exports = {api.package_id_of_chunk(c) for c in mod.toc.ids if c[11] == 1}
        require(exports == set(header.packages), 'source_export_store_coverage_mismatch')
        mod_headers[mod.mod_id] = header
        for pid in sorted(exports):
            key = (mod.mod_id, pid)
            if pid in owners:
                require(key not in unmatched_placements, 'placement_cannot_override_known_owner')
                owner = owners[pid]
            else:
                require(key in unmatched_placements, 'unmatched_package_placement_required')
                owner = unmatched_placements[key]
                require(owner in validated, 'unknown_placement_owner')
                used_placements.add(key)
            assignments.setdefault(owner, {}).setdefault(mod.mod_id, set()).add(pid)
    require(used_placements == set(unmatched_placements), 'unused_or_invalid_unmatched_placement')
    available = set(owners) | {pid for h in mod_headers.values() for pid in h.packages}
    require(all(pid in available for h in mod_headers.values() for entry in h.packages.values()
                for pid in entry.imported_packages), 'external_import_unresolved')

    outputs, projections = {}, []
    with tempfile.TemporaryDirectory(prefix='compose-', dir=scratch) as temporary:
        folder = Path(temporary)
        for owner in sorted(assignments):
            projected = []
            for mod in ordered:
                keep = assignments[owner].get(mod.mod_id)
                if not keep:
                    continue
                utoc, ucas, projection_receipt = projector.project(api, mod, keep)
                # Integer filenames cannot escape the confined scratch directory.
                stem = folder/str(len(projections))
                utoc_path = stem.with_suffix('.utoc'); ucas_path = stem.with_suffix('.ucas')
                utoc_path.write_bytes(utoc); ucas_path.write_bytes(ucas)
                projected.append(api.ModSpec(mod.mod_id, str(utoc_path), str(ucas_path), mod.priority))
                projections.append({'base_id':owner,'mod_id':mod.mod_id,'projection':projection_receipt})
            base = validated[owner]
            utoc, sidecar, builder_receipt = api.build_overlay(
                base['utoc'], base['ucas_size'], base['header'], projected)
            api.validate_lookup(api.parse_toc(utoc))
            outputs[owner] = {'utoc':utoc,'sidecar':sidecar,'builder_receipt':builder_receipt}
        require(identities() == expected, 'source_identity_changed')

    receipt = {'schema':1,'label':'Mac-only','result':'offline_composition_verified',
        'runtime_mount_ready':False,'conflict_policy':'reject',
        'catalogue_scope':'supplied bases only; completeness not verified',
        'sources':{'bases':base_sources,'mods':[{'mod_id':m,'utoc_sha256':u,'ucas_sha256':c}
                                               for m,u,c in expected]},
        'unmatched_placements':[{'mod_id':m,'package_id':'%016x'%pid,'base_id':unmatched_placements[(m,pid)],
            'claim':'caller-directed placement; not proof of a new package'} for m,pid in sorted(used_placements)],
        'projections':projections,
        'outputs':[{'base_id':name,'utoc_sha256':digest(row['utoc']),
                    'sidecar_sha256':digest(row['sidecar']),'sidecar_bytes':len(row['sidecar'])}
                   for name,row in sorted(outputs.items())],
        'limits':['No disk publication or runtime mount is performed.',
                  'Physical base UCAS identity/EOF, complete catalogue and cross-container engine precedence remain unproved.',
                  'Material/export/class compatibility remains a separate gate.',
                  'Builder receipts reference temporary projected inputs; original identities and projection hashes are retained here.']}
    return outputs, receipt
