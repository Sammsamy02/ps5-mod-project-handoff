// Genuine pinned engine_version.rs BuildConfiguration/Stats fingerprint.
// Marker table, gates, candidates, costs, thresholds and axis selection preserved.
use core::str::FromStr;
use alloc::{vec::Vec,format};
use itertools::Itertools;
use crate::{engine_version::EngineVersion,resolvers::{AsyncContext,ResolveError},util};
const fn v(major: u16, minor: u16) -> EngineVersion {
    EngineVersion { major, minor }
}

#[rustfmt::skip]
const VERSIONS: &[EngineVersion] = &[
    v(4, 7), v(4, 8), v(4, 9), v(4, 10), v(4, 11), v(4, 12),
    v(4, 13), v(4, 14), v(4, 15), v(4, 16), v(4, 17), v(4, 18),
    v(4, 19), v(4, 20), v(4, 21), v(4, 22), v(4, 23), v(4, 24),
    v(4, 25), v(4, 26), v(4, 27), v(5, 0), v(5, 1), v(5, 2),
    v(5, 3), v(5, 4), v(5, 5), v(5, 6), v(5, 7), v(5, 8),
];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BuildConfiguration {
    /// Also Debug and DebugGame
    Development,
    Test,
    Shipping,
}
impl FromStr for BuildConfiguration {
    type Err = ResolveError;
    fn from_str(s: &str) -> core::result::Result<Self, Self::Err> {
        match s {
            "Development" => Ok(Self::Development),
            "Test" => Ok(Self::Test),
            "Shipping" => Ok(Self::Shipping),
            _ => Err(ResolveError::new_msg(
                "expected Development, Test or Shipping",
            )),
        }
    }
}

/// The `STATS` macro. Defaults off in Test and Shipping, but any target can
/// force it back on, so it doesn't follow from the configuration.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Stats {
    Off,
    On,
}
impl FromStr for Stats {
    type Err = ResolveError;
    fn from_str(s: &str) -> core::result::Result<Self, Self::Err> {
        match s {
            "Off" => Ok(Self::Off),
            "On" => Ok(Self::On),
            _ => Err(ResolveError::new_msg("expected On or Off")),
        }
    }
}

#[derive(Debug, Clone, Copy)]
struct Build {
    version: Option<&'static EngineVersion>,
    config: BuildConfiguration,
    stats: Stats,
}

/// A preprocessor condition a marker sits behind. Another axis costs a variant
/// here, not a column on every row.
#[derive(Clone, Copy, PartialEq)]
enum Gate {
    /// `#if !UE_BUILD_SHIPPING`
    NotShipping,
    /// `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`
    DevOnly,
    /// `#if UE_BUILD_TEST || UE_BUILD_SHIPPING`
    TestOrShipping,
    /// `#if UE_BUILD_SHIPPING`
    ShippingOnly,
    /// `#if STATS`
    WithStats,
}
use Gate::*;

impl Gate {
    fn allows(self, build: &Build) -> bool {
        use BuildConfiguration::*;
        match self {
            NotShipping => build.config != Shipping,
            DevOnly => build.config == Development,
            TestOrShipping => build.config != Development,
            ShippingOnly => build.config == Shipping,
            WithStats => build.stats == Stats::On,
        }
    }
}

struct Marker {
    string: &'static str,
    gates: &'static [Gate],
    first: Option<EngineVersion>,
    last: Option<EngineVersion>,
}

impl Marker {
    fn covers(&self, build: &Build) -> bool {
        let after_first = match (&self.first, build.version) {
            (None, _) => true,
            (Some(_), None) => false,
            (Some(first), Some(version)) => version >= first,
        };
        let before_last = match (&self.last, build.version) {
            (None, _) | (_, None) => true,
            (Some(last), Some(version)) => version <= last,
        };
        after_first && before_last && self.gates.iter().all(|gate| gate.allows(build))
    }
}

#[rustfmt::skip]
const MARKERS: &[Marker] = &[
    Marker { string: "FOpenGLRHILongGPUTaskPS",                            gates: &[], first: None,            last: Some(v(4, 7)) },
    Marker { string: "FPostProcessTonemapPS15",                            gates: &[], first: None,            last: Some(v(4, 7)) },
    Marker { string: "USE_VIGNETTE_COLOR",                                 gates: &[], first: None,            last: Some(v(4, 7)) },
    Marker { string: "p.BounceThresholdVelocity",                          gates: &[], first: None,            last: Some(v(4, 7)) },
    Marker { string: "r.AmbientOcclusionSampleSetQuality",                 gates: &[], first: None,            last: Some(v(4, 8)) },
    Marker { string: "r.LegacySingleThreadedRelevance",                    gates: &[], first: None,            last: Some(v(4, 8)) },
    Marker { string: "r.Shadow.DistanceFieldPenumbraSize",                 gates: &[], first: None,            last: Some(v(4, 8)) },
    Marker { string: "r.HalfResReflections",                               gates: &[], first: None,            last: Some(v(4, 9)) },
    Marker { string: "r.DepthOfFieldNearBlurSizeThreshold",                gates: &[], first: None,            last: Some(v(4, 10)) },
    Marker { string: "r.Editor.MovingPattern",                             gates: &[], first: None,            last: Some(v(4, 10)) },
    Marker { string: "r.OptimizeForUAVPerformance",                        gates: &[], first: None,            last: Some(v(4, 13)) },
    Marker { string: "r.PS4DumpShaderSDB",                                 gates: &[], first: None,            last: Some(v(4, 20)) },
    Marker { string: "r.PS4MixedModeShaderDebugInfo",                      gates: &[], first: None,            last: Some(v(4, 27)) },
    Marker { string: "ReceiveUninitializeComponent",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 7)) },
    Marker { string: "r.Upscale.Cylinder",                                 gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 8)) },
    Marker { string: "bAnimBranchingPointNeedsSort",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 10)) },
    Marker { string: "AvoidanceConsiderationRadius",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 17)) },
    Marker { string: "K2_SetActorRelativeTransform",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 17)) },
    Marker { string: "PeripheralVisionAngleDegrees",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 17)) },
    Marker { string: "RequestStimuliListenerUpdate",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 17)) },
    Marker { string: "SubduedSelectionOutlineColor",                       gates: &[], first: Some(v(4, 7)),   last: Some(v(4, 17)) },
    Marker { string: "r.RHIDeferredContextWidth",                          gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 8)) },
    Marker { string: "MatineeScreenshotOptions",                           gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 9)) },
    Marker { string: "r.MotionBlurDilate",                                 gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 10)) },
    Marker { string: "r.MotionBlurSmoothMax",                              gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 10)) },
    Marker { string: "r.MotionBlurNew",                                    gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 13)) },
    Marker { string: "r.AOFillGapsHighQuality",                            gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 15)) },
    Marker { string: "r.OcclusionQueryLocation",                           gates: &[], first: Some(v(4, 8)),   last: Some(v(4, 16)) },
    Marker { string: "r.CheckSRVTransitions",                              gates: &[], first: Some(v(4, 8)),   last: None },
    Marker { string: "MovieSceneBoundObjectInfo",                          gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 9)) },
    Marker { string: "UMovieSceneObjectManager",                           gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 9)) },
    Marker { string: "r.AOInnerGlobalDFClipmapDistance",                   gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 11)) },
    Marker { string: "r.MobileOnChipMSAA",                                 gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 14)) },
    Marker { string: "r.AOVisualizeGlobalDistanceField",                   gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 15)) },
    Marker { string: "s.PreloadPackageDependencies",                       gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 15)) },
    Marker { string: "s.AsyncIOBandwidthLimit",                            gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 16)) },
    Marker { string: "r.D3D12GraphicsAdapter",                             gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 19)) },
    Marker { string: "r.MobileHDR32bppMode",                               gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 24)) },
    Marker { string: "r.MobileDynamicPointLightsUseStaticBranch",          gates: &[], first: Some(v(4, 9)),   last: Some(v(4, 27)) },
    Marker { string: "r.MobileNumDynamicPointLights",                      gates: &[], first: Some(v(4, 9)),   last: Some(v(5, 0)) },
    Marker { string: "ULevelSequenceInstance",                             gates: &[], first: Some(v(4, 10)),  last: Some(v(4, 10)) },
    Marker { string: "FORWARD_QL_FORCE_FULLY_ROUGH",                       gates: &[], first: Some(v(4, 10)),  last: Some(v(4, 12)) },
    Marker { string: "UHapticFeedbackEffect",                              gates: &[], first: Some(v(4, 10)),  last: Some(v(4, 12)) },
    Marker { string: "UAutomatedLevelSequenceCapture",                     gates: &[], first: Some(v(4, 10)),  last: Some(v(4, 19)) },
    Marker { string: "D3D12.AllowDrawClears",                              gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 11)) },
    Marker { string: "a.UseBakedAdditiveAnimations",                       gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 11)) },
    Marker { string: "r.DetectAndWarnOfBadDrivers",                        gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 11)) },
    Marker { string: "r.Tonemapper.ScreenPercentage",                      gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 12)) },
    Marker { string: "r.CapsuleIndirectShadowMinVisibility",               gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 13)) },
    Marker { string: "r.TargetPrecompileFrameTime",                        gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 21)) },
    Marker { string: "r.UseAsyncShaderPrecompilation",                     gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 21)) },
    Marker { string: "r.EyeAdaptation.MethodOveride",                      gates: &[], first: Some(v(4, 11)),  last: Some(v(4, 23)) },
    Marker { string: "r.Tonemapper.GrainQuantization",                     gates: &[], first: Some(v(4, 11)),  last: Some(v(5, 2)) },
    Marker { string: "r.HighResScreenshotDelay",                           gates: &[], first: Some(v(4, 11)),  last: None },
    Marker { string: "r.DistanceFieldSpecularOcclusion",                   gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 12)) },
    Marker { string: "r.Shaders.AvoidFlowControl",                         gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 12)) },
    Marker { string: "r.Streaming.ShowWantedMips",                         gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 12)) },
    Marker { string: "r.Streaming.AnalysisIndex",                          gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 13)) },
    Marker { string: "r.RHICmdStateCacheEnable",                           gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 15)) },
    Marker { string: "r.AllReceiveDynamicCSM",                             gates: &[], first: Some(v(4, 12)),  last: Some(v(4, 18)) },
    Marker { string: "r.DriverDetectionMethod",                            gates: &[], first: Some(v(4, 12)),  last: None },
    Marker { string: "r.Mobile.EnableStaticAndCSMShadowReceivers",         gates: &[], first: Some(v(4, 12)),  last: None },
    Marker { string: "r.Tonemapper2084",                                   gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 14)) },
    Marker { string: "r.TonemapperACESInversion",                          gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 14)) },
    Marker { string: "r.TonemapperOutputGamut",                            gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 14)) },
    Marker { string: "r.Mobile.Shadow.CSMShaderCulling",                   gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 18)) },
    Marker { string: "r.UseProgramBinaryCache",                            gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 20)) },
    Marker { string: "r.Tonemapper.ConfigIndexOverride",                   gates: &[], first: Some(v(4, 13)),  last: Some(v(4, 23)) },
    Marker { string: "r.Android.DisableVulkanSupport",                     gates: &[], first: Some(v(4, 13)),  last: None },
    Marker { string: "r.CapsuleIndirectShadowSelfShadowIntensity",         gates: &[], first: Some(v(4, 14)),  last: Some(v(4, 14)) },
    Marker { string: "r.Streaming.ScaleTexturesByGlobalMyBias",            gates: &[], first: Some(v(4, 14)),  last: Some(v(4, 15)) },
    Marker { string: "enablehighdpi",                                      gates: &[], first: Some(v(4, 14)),  last: Some(v(4, 17)) },
    Marker { string: "r.Photography.PersistEffects",                       gates: &[], first: Some(v(4, 14)),  last: Some(v(4, 18)) },
    Marker { string: "r.HLOD.DistanceScale",                               gates: &[], first: Some(v(4, 14)),  last: Some(v(4, 19)) },
    Marker { string: "FMonoscopicFarFieldMaskPS",                          gates: &[], first: Some(v(4, 15)),  last: Some(v(4, 15)) },
    Marker { string: "r.SceneAlpha",                                       gates: &[], first: Some(v(4, 15)),  last: Some(v(4, 15)) },
    Marker { string: "r.Mobile.AllowMovableDirectionalLights",             gates: &[], first: Some(v(4, 15)),  last: Some(v(5, 4)) },
    Marker { string: "r.PostProcessingColorFormat",                        gates: &[], first: Some(v(4, 15)),  last: None },
    Marker { string: "r.CopySceneColorOncePerViewOnly",                    gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 16)) },
    Marker { string: "r.FastVRamLightAttenuation",                         gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 16)) },
    Marker { string: "AACF_DriveAttribute_DEPRECATED",                     gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 17)) },
    Marker { string: "AACF_DriveMaterial_DEPRECATED",                      gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 17)) },
    Marker { string: "r.AOHistoryMinConfidenceScale",                      gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 19)) },
    Marker { string: "SMU_OnlyTickPoseWhenRendered",                       gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 20)) },
    Marker { string: "r.BinaryShaderCacheLogging",                         gates: &[], first: Some(v(4, 16)),  last: Some(v(4, 21)) },
    Marker { string: "r.TransientResourceAliasing.RenderTargets",          gates: &[], first: Some(v(4, 17)),  last: Some(v(4, 17)) },
    Marker { string: "r.UseUserShaderCache",                               gates: &[], first: Some(v(4, 17)),  last: Some(v(4, 21)) },
    Marker { string: "r.VT.NumMipsToExpandRequests",                       gates: &[], first: Some(v(4, 17)),  last: Some(v(4, 22)) },
    Marker { string: "r.Mobile.SceneColorFormat",                          gates: &[], first: Some(v(4, 17)),  last: None },
    Marker { string: "r.ConsoleTextScale",                                 gates: &[], first: Some(v(4, 18)),  last: Some(v(4, 18)) },
    Marker { string: "r.FastVRam.DistanceFieldAOConfidence",               gates: &[], first: Some(v(4, 18)),  last: Some(v(4, 19)) },
    Marker { string: "r.SaveShaderCache",                                  gates: &[], first: Some(v(4, 18)),  last: Some(v(4, 21)) },
    Marker { string: "r.AndroidDisableThreadedRenderingFirstLoad",         gates: &[], first: Some(v(4, 18)),  last: None },
    Marker { string: "r.ViewDistanceScaleNoScalability",                   gates: &[], first: Some(v(4, 19)),  last: Some(v(4, 19)) },
    Marker { string: "r.vulkan.CpuWaitForFence",                           gates: &[], first: Some(v(4, 19)),  last: Some(v(4, 19)) },
    Marker { string: "r.DefaultFeature.SpotLightUnits",                    gates: &[], first: Some(v(4, 19)),  last: Some(v(4, 20)) },
    Marker { string: "r.Mobile.EnableMovableLightCSMShaderCulling",        gates: &[], first: Some(v(4, 19)),  last: None },
    Marker { string: "r.Mobile.SeparateMaskedPass",                        gates: &[], first: Some(v(4, 20)),  last: Some(v(4, 21)) },
    Marker { string: "r.Mobile.Shadow.CSMDebugHint",                       gates: &[], first: Some(v(4, 20)),  last: Some(v(5, 0)) },
    Marker { string: "r.Android.DisableASTCSupport",                       gates: &[], first: Some(v(4, 20)),  last: None },
    Marker { string: "r.CookOutUnusedDetailModeComponents",                gates: &[], first: Some(v(4, 20)),  last: None },
    Marker { string: "r.ViewDistanceScale.SecondaryScale",                 gates: &[], first: Some(v(4, 20)),  last: None },
    Marker { string: "r.VT.TLSTranscodeCodecCacheSize",                    gates: &[], first: Some(v(4, 21)),  last: Some(v(4, 22)) },
    Marker { string: "r.Mobile.ForceFullPrecisionInPS",                    gates: &[], first: Some(v(4, 21)),  last: Some(v(4, 27)) },
    Marker { string: "r.Mobile.SkyLightPermutation",                       gates: &[], first: Some(v(4, 21)),  last: Some(v(5, 4)) },
    Marker { string: "r.Mobile.AllowDitheredLODTransition",                gates: &[], first: Some(v(4, 21)),  last: None },
    Marker { string: "au.BypassVirtualizeWhenSilent",                      gates: &[], first: Some(v(4, 22)),  last: Some(v(4, 22)) },
    Marker { string: "r.Vulkan.EnableTessellation",                        gates: &[], first: Some(v(4, 22)),  last: Some(v(4, 22)) },
    Marker { string: "p.CullPhiVisualizeDistance",                         gates: &[], first: Some(v(4, 22)),  last: Some(v(4, 23)) },
    Marker { string: "TaskGraph.EnablePowerSavingThreadPriorityReduction", gates: &[], first: Some(v(4, 22)),  last: Some(v(4, 25)) },
    Marker { string: "net.MaxNetStringSize",                               gates: &[], first: Some(v(4, 22)),  last: None },
    Marker { string: "p.ComputeConstraintsUseAny",                         gates: &[], first: Some(v(4, 23)),  last: Some(v(4, 23)) },
    Marker { string: "p.GatherVerbosePhysicsStats",                        gates: &[], first: Some(v(4, 23)),  last: Some(v(4, 23)) },
    Marker { string: "bRestrictLocalization",                              gates: &[], first: Some(v(4, 23)),  last: Some(v(4, 25)) },
    Marker { string: "r.Mobile.AllowPixelDepthOffset",                     gates: &[], first: Some(v(4, 23)),  last: None },
    Marker { string: "r.Mobile.SupportGPUScene",                           gates: &[], first: Some(v(4, 23)),  last: None },
    Marker { string: "r.VT.EvictFileCache",                                gates: &[], first: Some(v(4, 23)),  last: None },
    Marker { string: "p.Chaos.ImmPhys.DeltaTime",                          gates: &[], first: Some(v(4, 24)),  last: Some(v(4, 24)) },
    Marker { string: "p.ChaosParticleParallelFor",                         gates: &[], first: Some(v(4, 24)),  last: Some(v(4, 24)) },
    Marker { string: "r.Water.ConstantWaterDepth",                         gates: &[], first: Some(v(4, 24)),  last: Some(v(4, 24)) },
    Marker { string: "r.Mobile.UseGPUSceneTexture",                        gates: &[], first: Some(v(4, 24)),  last: Some(v(4, 27)) },
    Marker { string: "log.flushInterval",                                  gates: &[], first: Some(v(4, 24)),  last: None },
    Marker { string: "TestLockFreeWorker",                                 gates: &[], first: Some(v(4, 25)),  last: Some(v(4, 25)) },
    Marker { string: "r.AnisotropicBRDF",                                  gates: &[], first: Some(v(4, 25)),  last: Some(v(4, 25)) },
    Marker { string: "fc.NumFileCacheBlocks",                              gates: &[], first: Some(v(4, 25)),  last: Some(v(4, 27)) },
    Marker { string: "Freezing_bWithRayTracing",                           gates: &[], first: Some(v(4, 25)),  last: Some(v(5, 0)) },
    Marker { string: "r.Android.DisableVulkanSM5Support",                  gates: &[], first: Some(v(4, 25)),  last: None },
    Marker { string: "fx.Niagara.BatchGPUTickSubmit",                      gates: &[], first: Some(v(4, 26)),  last: Some(v(4, 26)) },
    Marker { string: "fx.Niagara.ConcurrentGPUTickInit",                   gates: &[], first: Some(v(4, 26)),  last: Some(v(4, 26)) },
    Marker { string: "p.CollisionCullDistance",                            gates: &[], first: Some(v(4, 26)),  last: Some(v(4, 26)) },
    Marker { string: "r.SupportAnisotropicMaterials",                      gates: &[], first: Some(v(4, 26)),  last: Some(v(4, 26)) },
    Marker { string: "D3D12.GlobalViewHeapBlockSize",                      gates: &[], first: Some(v(4, 26)),  last: Some(v(5, 0)) },
    Marker { string: "r.VolumetricCloud.HzbCulling",                       gates: &[], first: Some(v(4, 26)),  last: Some(v(5, 2)) },
    Marker { string: "p.Chaos.Solver.SleepEnabled",                        gates: &[], first: Some(v(4, 26)),  last: Some(v(5, 3)) },
    Marker { string: "r.ContactShadows.NonShadowCastingIntensity",         gates: &[], first: Some(v(4, 26)),  last: None },
    Marker { string: "r.Mobile.ShadingPath",                               gates: &[], first: Some(v(4, 26)),  last: None },
    Marker { string: "r.FASTBuild.Shader.BatchSize",                       gates: &[], first: Some(v(4, 27)),  last: Some(v(4, 27)) },
    Marker { string: "r.WPOPrimitivesOutputVelocity",                      gates: &[], first: Some(v(4, 27)),  last: Some(v(4, 27)) },
    Marker { string: "r.ShaderCompiler.JobCache",                          gates: &[], first: Some(v(4, 27)),  last: Some(v(5, 5)) },
    Marker { string: "s.EnforcePackageCompatibleVersionCheck",             gates: &[], first: Some(v(4, 27)),  last: None },
    Marker { string: "r.Lumen.ProbeHierarchy.Depth",                       gates: &[], first: Some(v(5, 0)),   last: Some(v(5, 0)) },
    Marker { string: "r.Nanite.SphereCullingFrustum",                      gates: &[], first: Some(v(5, 0)),   last: Some(v(5, 1)) },
    Marker { string: "r.MaterialEnableControlFlow",                        gates: &[], first: Some(v(5, 0)),   last: Some(v(5, 2)) },
    Marker { string: "r.Nanite.OptimizedRelevance",                        gates: &[], first: Some(v(5, 0)),   last: Some(v(5, 3)) },
    Marker { string: "r.Lumen.IrradianceFieldGather",                      gates: &[], first: Some(v(5, 0)),   last: Some(v(5, 7)) },
    Marker { string: "r.DemotedLocalMemoryWarning",                        gates: &[], first: Some(v(5, 0)),   last: None },
    Marker { string: "r.Nanite.AllowComputeMaterial",                      gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 1)) },
    Marker { string: "r.Strata.AsyncClassification",                       gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 1)) },
    Marker { string: "r.Strata.Debug.VisualizeMode",                       gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 1)) },
    Marker { string: "gc.LockBehavior",                                    gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 2)) },
    Marker { string: "p.Chaos.Solver.ValidateGraph",                       gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 2)) },
    Marker { string: "r.GlobalDistanceField.Debug",                        gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 3)) },
    Marker { string: "p.net.TargetNumBufferedCmds",                        gates: &[], first: Some(v(5, 1)),   last: Some(v(5, 5)) },
    Marker { string: "r.Mobile.ShadingModelsMask",                         gates: &[], first: Some(v(5, 1)),   last: None },
    Marker { string: "s.LargeMemoryDataMaxPoolLength",                     gates: &[], first: Some(v(5, 1)),   last: None },
    Marker { string: "s.RemoveUnreachableObjectsOnGT",                     gates: &[], first: Some(v(5, 1)),   last: None },
    Marker { string: "gc.DumpMemoryStats",                                 gates: &[], first: Some(v(5, 2)),   last: Some(v(5, 2)) },
    Marker { string: "r.RectLightAtlas.Translucent",                       gates: &[], first: Some(v(5, 2)),   last: Some(v(5, 3)) },
    Marker { string: "r.DynamicRes.DynamicFrameTime",                      gates: &[], first: Some(v(5, 2)),   last: Some(v(5, 5)) },
    Marker { string: "r.SubstrateBackCompatibility",                       gates: &[], first: Some(v(5, 2)),   last: Some(v(5, 6)) },
    Marker { string: "net.BitReader.EnsureOnOverflow",                     gates: &[], first: Some(v(5, 2)),   last: None },
    Marker { string: "r.MaterialEditor.LWCTruncateMode",                   gates: &[], first: Some(v(5, 2)),   last: None },
    Marker { string: "s.SkipChangelistCompatibilityVersionCheck",          gates: &[], first: Some(v(5, 2)),   last: None },
    Marker { string: "s.IasMaxHttpConnectionCount",                        gates: &[], first: Some(v(5, 3)),   last: Some(v(5, 3)) },
    Marker { string: "r.DX11NVAfterMathDumpWaitTime",                      gates: &[], first: Some(v(5, 3)),   last: Some(v(5, 4)) },
    Marker { string: "r.DX12NVAfterMathDumpWaitTime",                      gates: &[], first: Some(v(5, 3)),   last: Some(v(5, 4)) },
    Marker { string: "r.PathTracing.Override.Depth",                       gates: &[], first: Some(v(5, 3)),   last: Some(v(5, 4)) },
    Marker { string: "r.ManyLights.HairVoxelTraces",                       gates: &[], first: Some(v(5, 4)),   last: Some(v(5, 4)) },
    Marker { string: "r.ManyLights.LightFunctions",                        gates: &[], first: Some(v(5, 4)),   last: Some(v(5, 4)) },
    Marker { string: "r.ManyLights.WorldSpaceTraces",                      gates: &[], first: Some(v(5, 4)),   last: Some(v(5, 4)) },
    Marker { string: "D3D12.SamplerWarningThreshold",                      gates: &[], first: Some(v(5, 4)),   last: Some(v(5, 5)) },
    Marker { string: "r.PathTracing.CloudMapEnable",                       gates: &[], first: Some(v(5, 5)),   last: Some(v(5, 5)) },
    Marker { string: "r.Nanite.SkinningBuffers.Defrag",                    gates: &[], first: Some(v(5, 5)),   last: Some(v(5, 6)) },
    Marker { string: "r.MegaLights.Volume.Debug",                          gates: &[], first: Some(v(5, 5)),   last: Some(v(5, 7)) },
    Marker { string: "r.Vulkan.Bindless.BlockSize",                        gates: &[], first: Some(v(5, 5)),   last: Some(v(5, 7)) },
    Marker { string: "net.QueuedBatchTimeoutSeconds",                      gates: &[], first: Some(v(5, 6)),   last: Some(v(5, 6)) },
    Marker { string: "r.LensFlareBlurComputeShader",                       gates: &[], first: Some(v(5, 6)),   last: Some(v(5, 6)) },
    Marker { string: "r.MegaLights.DownsampleFactor",                      gates: &[], first: Some(v(5, 6)),   last: Some(v(5, 6)) },
    Marker { string: "r.Substrate.BlendableGBuffer",                       gates: &[], first: Some(v(5, 6)),   last: Some(v(5, 6)) },
    Marker { string: "au.DirectProceduralRendering",                       gates: &[], first: Some(v(5, 7)),   last: Some(v(5, 7)) },
    Marker { string: "p.Chaos.MinParallelTaskSize",                        gates: &[], first: Some(v(5, 7)),   last: Some(v(5, 7)) },
    Marker { string: "p.Chaos.SingleThreadPushData",                       gates: &[], first: Some(v(5, 7)),   last: Some(v(5, 7)) },
    Marker { string: "s.ImportTypeHierarchyEnabled",                       gates: &[], first: Some(v(5, 7)),   last: Some(v(5, 7)) },
    Marker { string: "D3D12.ResidencyDebugBudgetMB",                       gates: &[], first: Some(v(5, 8)),   last: None },
    Marker { string: "D3D12.ResourcesStartResident",                       gates: &[], first: Some(v(5, 8)),   last: None },
    Marker { string: "au.metasound.dump_poly_types",                       gates: &[], first: Some(v(5, 8)),   last: None },
    Marker { string: "gc.PauseGCFreeMemThresholdMB",                       gates: &[], first: Some(v(5, 8)),   last: None },

    Marker { string: "AudioComponent Dump",                                                                            gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "CLEANSCREENSHOTS",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "CONTENTCOMPARISON",                                                                              gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "CompressionState",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "ConsoleHelp.html",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "DUMPMATERIALSTATS",                                                                              gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "DUMPPARTICLECOUNTS",                                                                             gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "DebugTrackedTextures",                                                                           gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "Dry audio isolated",                                                                             gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "Dump Shadow Setup:",                                                                             gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "DumpBTUsageStats",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "FATALSCRIPTWARNINGS",                                                                            gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "INTRINSICCLASSES",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "Memory.StaleTest",                                                                               gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "Memory.UsePurgatory",                                                                            gates: &[NotShipping],  first: None,           last: None },
    Marker { string: "gc.FindStaleClusters",                                                                           gates: &[NotShipping],  first: None,           last: None },

    Marker { string: "Debug.OOMMemReport",                                                                             gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "GameplayTags.PackingTest",                                                                       gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "GameplayTags.PrintReport",                                                                       gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "Reattach.MaterialInstances",                                                                     gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "Reattach.Materials",                                                                             gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "TaskGraph.Randomize",                                                                            gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "TimedMemReport.Delay",                                                                           gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "gc.StressTestGC",                                                                                gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "net.TestObjRefSerialize",                                                                        gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "p.DebugTimeDiscrepancy",                                                                         gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "p.ShowInitialOverlaps",                                                                          gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "p.VisualizeMovement",                                                                            gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "r.LimitRenderingFeatures",                                                                       gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "r.MotionBlurFiltering",                                                                          gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "r.RenderTimeFrozen",                                                                             gates: &[DevOnly],       first: None,           last: None },
    Marker { string: "r.Shadow.FreezeCamera",                                                                          gates: &[DevOnly],       first: None,           last: None },

    Marker { string: "Time slicing cannot be disabled in Test or Shipping builds.  SetAllowTimeSlicing does nothing.", gates: &[TestOrShipping], first: Some(v(4, 11)), last: Some(v(4, 21)) },
    Marker { string: "Debug viewmodes not allowed in Test or Shipping builds.",                                        gates: &[TestOrShipping], first: Some(v(4, 26)), last: Some(v(5, 5)) },
    Marker { string: "TransientUserTexture",                                                                           gates: &[TestOrShipping], first: Some(v(5, 5)),  last: None },

    Marker { string: "Plugin commandlet disabled in shipping mode.",                                                   gates: &[ShippingOnly],      first: None,           last: None },
    Marker { string: "Logging interval in shipping. If set, this overrides archive.FlushInterval",                     gates: &[ShippingOnly],      first: Some(v(4, 24)), last: None },
    Marker { string: "log.flushInterval.Shipping",                                                                     gates: &[ShippingOnly],      first: Some(v(4, 24)), last: None },
    Marker { string: "Engine.VerifyLoadMapWorldCleanup.Severity.Shipping",                                             gates: &[ShippingOnly],      first: Some(v(5, 1)),  last: None },
    Marker { string: "Engine.VerifyLoadMapWorldCleanup.TraceMode.Shipping",                                            gates: &[ShippingOnly],      first: Some(v(5, 1)),  last: None },
    Marker { string: "GameFeaturePlugin.LeakedAssetTrace.Severity.Shipping",                                           gates: &[ShippingOnly],      first: Some(v(5, 2)),  last: None },
    Marker { string: "GameFeaturePlugin.LeakedAssetTrace.TraceMode.Shipping",                                          gates: &[ShippingOnly],      first: Some(v(5, 2)),  last: None },
    Marker { string: "LevelStreaming.Profiling.Enabled.Shipping",                                                      gates: &[ShippingOnly],      first: Some(v(5, 3)),  last: None },
    Marker { string: "PluginManager.LeakedAssetTrace.Severity.Shipping",                                               gates: &[ShippingOnly],      first: Some(v(5, 4)),  last: None },
    Marker { string: "PluginManager.LeakedAssetTrace.TraceMode.Shipping",                                              gates: &[ShippingOnly],      first: Some(v(5, 4)),  last: None },

    Marker { string: "Cycle counters (flat)",                              gates: &[WithStats], first: None, last: None },
    Marker { string: "Cycle counters (hierarchy)",                         gates: &[WithStats], first: None, last: None },
    Marker { string: "Dumps RHI memory stats to the log",                  gates: &[WithStats], first: None, last: None },
    Marker { string: "Empty stat command!",                                gates: &[WithStats], first: None, last: None },
    Marker { string: "FSimpleDelegateGraphTask.StatCmd",                   gates: &[WithStats], first: None, last: None },
    Marker { string: "FSimpleDelegateGraphTask.StatsToGame",               gates: &[WithStats], first: None, last: None },
    Marker { string: "Frame Messages Condensed",                           gates: &[WithStats], first: None, last: None },
    Marker { string: "Here is the brief list of stats console commands",   gates: &[WithStats], first: None, last: None },
    Marker { string: "If true, dump stat packets.",                        gates: &[WithStats], first: None, last: None },
    Marker { string: "Particle Dynamic Memory Stats",                      gates: &[WithStats], first: None, last: None },
    Marker { string: "ParticleData,Total(Bytes),FMath::Max(Bytes)",        gates: &[WithStats], first: None, last: None },
    Marker { string: "RHI resource memory (not tracked by our allocator)", gates: &[WithStats], first: None, last: None },
];

const MIN_MARKERS_PRESENT: usize = 10;
const CONTRADICTION: usize = 5;
const OMISSION: usize = 1;
const MIN_MARGIN: usize = 4;

fn candidates() -> impl Iterator<Item = Build> {
    use BuildConfiguration::*;
    core::iter::once(None)
        .chain(VERSIONS.iter().map(Some))
        .cartesian_product([Development, Test, Shipping])
        .cartesian_product([Stats::Off, Stats::On])
        .map(|((version, config), stats)| Build {
            version,
            config,
            stats,
        })
}

fn costs(present: &[bool]) -> Vec<usize> {
    candidates()
        .map(|build| {
            MARKERS
                .iter()
                .zip(present)
                .map(|(marker, present)| match (marker.covers(&build), present) {
                    (false, true) => CONTRADICTION,
                    (true, false) => OMISSION,
                    _ => 0,
                })
                .sum::<usize>()
        })
        .collect_vec()
}

fn resolve_axis<T: PartialEq>(
    costs: &[usize],
    axis: impl Fn(&Build) -> T,
    what: &'static str,
) -> core::result::Result<T,ResolveError> {
    let best = costs.iter().position_min().unwrap();
    let value = axis(&candidates().nth(best).unwrap());
    let runner_up = candidates()
        .zip(costs)
        .filter(|(build, _)| axis(build) != value)
        .map(|(_, cost)| *cost)
        .min()
        .unwrap();
    if runner_up - costs[best] < MIN_MARGIN {
        return Err(ResolveError::new_msg(format!(
            "no {what} explains the markers better than its neighbour"
        )));
    }
    Ok(value)
}

fn fingerprint(ctx:&AsyncContext<'_>)->core::result::Result<Vec<usize>,ResolveError>{
    // Scheduler-only adapter. Same marker presence semantics, one bounded shared scan.
    let found=ctx.scan_many(&MARKERS.iter().map(|m|util::utf16_pattern(m.string)).collect_vec());
    let present=found.iter().map(|a|!a.is_empty()).collect_vec();
    if present.iter().filter(|p|**p).count()<MIN_MARKERS_PRESENT{
        return Err(ResolveError::new_msg("too few marker strings to fingerprint"));
    }
    Ok(costs(&present))
}
pub fn resolve_build_configuration(ctx:&AsyncContext<'_>)->core::result::Result<BuildConfiguration,ResolveError>{
    let costs=fingerprint(ctx)?;
    resolve_axis(&costs,|build|build.config,"build configuration")
}
pub fn resolve_stats(ctx:&AsyncContext<'_>)->core::result::Result<Stats,ResolveError>{
    let costs=fingerprint(ctx)?;
    resolve_axis(&costs,|build|build.stats,"stats setting")
}
