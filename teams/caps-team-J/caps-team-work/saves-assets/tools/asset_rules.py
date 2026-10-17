#!/usr/bin/env python3
"""Per-class rule table for the PC-mod asset classifier (Mac-only, generic for UE5 IoStore titles).

A rule says what a PC-cooked package of that class needs on the target console build, and how
strong the evidence is. No game, mod or asset names appear here: only engine/plugin class paths.

Verdicts (worst wins when a package has several findings):
  AS_IS         loads unchanged; two sources, one of them a console launch that checked this kind of
                asset itself (none yet: earlier launches are user-visual, see 'evidence').
  LIKELY_AS_IS  no platform data found and a same-class sample of the game's own console package
                has the same shape (or an earlier launch showed it working, user-visual only); NOT
                proven per asset (one test launch needed before it is on by default).
  CONVERT       needs a derived copy made by a named converter at install time (mod files stay
                untouched); the converter's own proof level is in CONVERTERS.
  UNKNOWN       not enough evidence (no PC or no console sample of this class yet): fail closed.
  UNSUPPORTED   cannot work on the target build as things stand (missing engine class, PC-only
                shader code without a route, editor-only package, ...).
"""

AS_IS, LIKELY_AS_IS, CONVERT, UNKNOWN, UNSUPPORTED = 'AS_IS', 'LIKELY_AS_IS', 'CONVERT', 'UNKNOWN', 'UNSUPPORTED'
SEVERITY = {AS_IS: 0, LIKELY_AS_IS: 1, CONVERT: 2, UNKNOWN: 3, UNSUPPORTED: 4}


def worst(verdicts):
    vs = list(verdicts)
    return max(vs, key=lambda v: SEVERITY[v]) if vs else UNKNOWN


# Converters the project has (or would need). status: console-proven | mac-only | not-written.
CONVERTERS = {
    'texture-ps5-tiling': {
        'status': 'console-tested, user-visual (launch H 2026-10-13: "mostly PASS")',
        'what': 'Re-tile each mip of a linear PC texture into the console GPU layout (lossless); '
                'rule and modes derived from the game\'s own console textures.',
        'tools': ['mod-manager/material-fallback/texture_package.py derive_ps5', 'ps5_tiling.py',
                  'pak-overlay/paktx.py install --substitute'],
        'evidence': 'launch H 2026-10-13 (earlier session): 30 derived textures installed; user saw Leon\'s '
                    'textures, "some squares" still to classify (REPORT-cap12-launch-H.md)'},
    'material-reparent': {
        'status': 'console-tested, user-visual (MICs, launch H); mac-only (mod-only base Materials, hide materials)',
        'what': 'Point a mod material instance at a game material that has console shaders, keeping '
                'the mod\'s textures and parameters; mod-only base Materials route through their MICs.',
        'tools': ['mod-manager/material-fallback/reparent_plan.py', 'mic_reparent.py', 'derive_substitutes.py'],
        'evidence': 'launch H (earlier session): 10 re-pointed MICs installed, user saw Leon\'s textures; '
                    'launch B: [zi] parents=5/5 par_wrong=0'},
    'shader-route-niagara': {
        'status': 'not-written',
        'what': 'Niagara GPU emitters carry compiled compute shaders for the PC; no route exists to '
                'console shaders (would need re-pointing to a game system, like materials).',
        'tools': [], 'evidence': 'none yet'},
    'bp-logic-loader': {
        'status': 'not-written',
        'what': 'Run UE4SS BPModLoaderMod on the console: case-insensitive directory keys, LogicMods '
                'paks mounted (#7 route), config.lua read through the mod source, BP events called.',
        'tools': [], 'evidence': 'host harness only (agent-GN)'},
}

RULES = {}


def _rule(classes, kind, verdict, converter=None, evidence='', notes=''):
    for c in classes:
        RULES[c] = {'kind': kind, 'verdict': verdict, 'converter': converter, 'evidence': evidence, 'notes': notes}


E = '/Script/Engine/'
_rule([E + 'SkeletalMesh'], 'skeletal_mesh', LIKELY_AS_IS, evidence=(
    'Mac: one independent decoder (Team A mesh_sections) reads the PC and the console form with the same layout '
    '(pair_compare); earlier launch H (user-visual): Leon\'s PC body meshes showed with his textures; no launch '
    'checked each mesh'),
    notes='materials it uses are judged separately; cloth sub-objects make the cloth part UNKNOWN')
_rule([E + 'Texture2D'], 'texture', CONVERT, 'texture-ps5-tiling',
      evidence='Mac: every game console texture probed is GPU-tiled (HasOptData), every PC mod texture is '
               'linear; earlier launch H (user-visual)')
_rule([E + 'Material', E + 'MaterialInstanceConstant'], 'material', CONVERT, 'material-reparent',
      evidence='Mac: PC materials carry PCD3D_SM5/SM6 shader maps, game materials SP_PS5; '
               'earlier launches: B (checker as predicted), H (re-pointed MICs, user-visual)')
_rule([E + 'MaterialFunction', E + 'MaterialFunctionMaterialLayer', E + 'MaterialFunctionMaterialLayerBlend',
       E + 'MaterialFunctionMaterialLayerInstance', E + 'MaterialParameterCollection'],
      'material_aux', UNKNOWN, evidence='no PC sample yet',
      notes='editor-side graph pieces; normally stripped in cooked builds')
_rule([E + 'TextureCube', E + 'VolumeTexture', E + 'Texture2DArray', E + 'TextureCubeArray',
       E + 'TextureLightProfile'], 'texture_other', UNSUPPORTED, 'texture-ps5-tiling',
      notes='the tiling converter handles 2D textures only')
_rule([E + 'TextureRenderTarget2D', '/Script/Engine/CanvasRenderTarget2D'], 'pure_data', UNKNOWN,
      evidence='console sample has no texel data; no PC sample yet')

PURE_DATA = [E + c for c in (
    'Skeleton', 'SubsurfaceProfile', 'PhysicsAsset', 'SkeletalBodySetup', 'BodySetup',
    'PhysicsConstraintTemplate', 'DataTable', 'CompositeDataTable', 'CurveTable', 'CompositeCurveTable',
    'CurveFloat', 'CurveVector', 'CurveLinearColor', 'UserDefinedEnum', 'StringTable', 'DataAsset',
    'PrimaryDataAsset', 'AnimCurveMetaData', 'BlendProfile', 'SkeletalMeshSocket', 'SoundClass',
    'SoundMix', 'SoundAttenuation', 'SoundConcurrency', 'SoundSubmix', 'ForceFeedbackEffect')] + [
    '/Script/CoreUObject/UserDefinedStruct', '/Script/PhysicsCore/PhysicalMaterial',
    '/Script/IKRig/IKRigDefinition', '/Script/IKRig/IKRigEffectorGoal', '/Script/IKRig/IKRetargeter',
    '/Script/IKRig/IKRetargetGlobalSettings', '/Script/IKRig/RetargetOpStack', '/Script/IKRig/RetargetRootSettings',
    '/Script/EnhancedInput/InputAction', '/Script/EnhancedInput/InputMappingContext',
    '/Script/RigLogicModule/DNAAsset']
_rule(PURE_DATA, 'pure_data', LIKELY_AS_IS,
      evidence='reflected property data only (unversioned, same engine schema as the game); generic checks: '
               'no bulk data, no shader code, classes present in the console build',
      notes='needs the same game version (property layout comes from the game\'s own classes)')
RULES[E + 'PhysicsAsset']['notes'] += '; cooked collision geometry (bulk data) makes it UNKNOWN'
RULES['/Script/RigLogicModule/DNAAsset']['notes'] = ('rides inside face skeletal meshes; launch H showed Leon\'s face '
                                                      '(user-visual); face animation not checked')

_rule([E + 'BlueprintGeneratedClass', E + 'AnimBlueprintGeneratedClass',
       '/Script/UMG/WidgetBlueprintGeneratedClass', '/Script/ControlRig/ControlRigBlueprintGeneratedClass',
       '/Script/RigVM/RigVMBlueprintGeneratedClass', '/Script/CoreUObject/Function'],
      'blueprint', LIKELY_AS_IS, evidence=(
          'script bytecode + property data, no platform data; the game\'s own console AnimBlueprints have the '
          'same package shape as a PC one (pair_compare)'),
      notes='as a game-asset replacement it loads like any package; as a UE4SS LogicMod it also needs '
            'bp-logic-loader (not written)')
RULES['/Script/CoreUObject/Function']['kind'] = 'sub'

_rule([E + c for c in ('AnimSequence', 'AnimMontage', 'AnimComposite', 'AnimStreamable', 'BlendSpace',
                       'BlendSpace1D', 'AimOffsetBlendSpace', 'AimOffsetBlendSpace1D', 'PoseAsset',
                       'AnimBoneCompressionSettings', 'AnimCurveCompressionSettings', 'MirrorDataTable')],
      'animation', UNKNOWN, evidence='no PC sample and no console sample on the Mac yet (fetch plan F1)',
      notes='expected portable: codec bit streams are not platform specific, and a codec class missing from '
            'the console build is caught by the script-import check')
_rule([E + 'StaticMesh'], 'static_mesh', UNKNOWN,
      evidence='no PC sample; console samples in fetch plan F1 (Nanite and non-Nanite)',
      notes='Nanite and distance-field data are expected portable; the console build runs ray tracing '
            '(RayTracingMode=Full in the game\'s console config), whose cooked data is the open question')
_rule(['/Script/Niagara/' + c for c in ('NiagaraSystem', 'NiagaraEmitter', 'NiagaraScript',
                                        'NiagaraParameterCollection', 'NiagaraParameterCollectionInstance',
                                        'NiagaraEffectType')],
      'niagara', UNKNOWN, 'shader-route-niagara',
      evidence='no PC sample; console samples in fetch plan F1',
      notes='GPU emitters carry compiled shaders (UNSUPPORTED when PC shader tokens are found); CPU emitters '
            'are VM bytecode; renderers use materials (material-reparent)')
_rule([E + c for c in ('SoundWave', 'SoundCue', 'SoundWaveProcedural', 'DialogueWave')], 'audio', UNKNOWN,
      evidence='no PC sample; console samples in fetch plan F1',
      notes='compressed audio is stored per codec format name; supported if the console build decodes that '
            'format (learned from the game\'s own console SoundWaves)')
_rule(['/Script/FMODStudio/' + c for c in ('FMODEvent', 'FMODBank', 'FMODBus', 'FMODVCA', 'FMODSnapshot',
                                           'FMODSnapshotReverb', 'FMODPort')], 'audio', UNKNOWN,
      evidence='no PC sample',
      notes='these packages only point at FMOD banks; the sound itself is in .bank files outside the paks')
_rule([E + 'World', E + 'Level', E + 'MapBuildDataRegistry'], 'level', UNSUPPORTED,
      notes='maps carry built lighting (lightmap textures need tiling, reflection captures) and world '
            'partition data; no converter or test yet')
_rule(['/Script/LevelSequence/LevelSequence', '/Script/HairStrandsCore/GroomAsset',
       '/Script/HairStrandsCore/GroomBindingAsset', '/Script/GeometryCollectionEngine/GeometryCollection',
       E + 'Font', E + 'FontFace', '/Script/MediaAssets/FileMediaSource', '/Script/MediaAssets/MediaPlayer',
       '/Script/MediaAssets/MediaTexture', '/Script/BinkMediaPlayer/BinkMediaPlayer',
       '/Script/OptimusCore/OptimusDeformer'], 'other', UNKNOWN, evidence='no samples yet')

# Sub-object classes that change a package's verdict even though the top-level class is known.
SUB_RULES = {
    '/Script/ClothingSystemRuntimeCommon/ClothingAssetCommon': (UNKNOWN, 'cloth simulation data: no PC sample yet'),
    '/Script/ChaosCloth/ChaosClothConfig': (UNKNOWN, 'cloth simulation data: no PC sample yet'),
    '/Script/ChaosCloth/ChaosClothSharedSimConfig': (UNKNOWN, 'cloth simulation data: no PC sample yet'),
    '/Script/Engine/MorphTarget': (LIKELY_AS_IS, 'morph targets: same class in the game\'s console meshes; '
                                                 'not console-tested from a PC mod'),
}


# Sub-object (non-primary) export classes covered by the evidence of each primary kind. Any other export
# class in a package makes it UNKNOWN (fail closed). 'export:' classes (instances of in-package classes,
# e.g. Blueprint default objects) are always allowed.
SUB_ALLOW = {
    'skeletal_mesh': {E + 'AnimCurveMetaData', '/Script/RigLogicModule/DNAAsset', E + 'MorphTarget',
                      E + 'SkeletalMeshSocket', '/Script/ClothingSystemRuntimeCommon/ClothingAssetCommon',
                      '/Script/ChaosCloth/ChaosClothConfig', '/Script/ChaosCloth/ChaosClothSharedSimConfig'},
    'texture': set(), 'material': set(), 'texture_other': set(),
    'pure_data': set(PURE_DATA),
}
# Blueprint packages hold their functions, construction scripts and component/widget templates.
BLUEPRINT_SUB_EXACT = {'/Script/CoreUObject/Function', '/Script/CoreUObject/ScriptStruct', E + 'SCS_Node',
                       E + 'SimpleConstructionScript', E + 'InheritableComponentHandler',
                       E + 'InputKeyDelegateBinding', E + 'InputActionDelegateBinding', E + 'InputAxisDelegateBinding',
                       E + 'TimelineTemplate', E + 'ComponentDelegateBinding', E + 'AnimCurveMetaData'}
BLUEPRINT_SUB_PREFIXES = ('/Script/UMG/', '/Script/MovieScene/', '/Script/MovieSceneTracks/')


def sub_allowed(kind, cls):
    if cls is None:
        return False
    if cls.startswith('export:'):
        return True
    if kind == 'blueprint':
        return cls in BLUEPRINT_SUB_EXACT or cls.startswith(BLUEPRINT_SUB_PREFIXES) or cls.endswith('Component')
    return cls in SUB_ALLOW.get(kind, set())


def rule_for(class_path):
    return RULES.get(class_path)
