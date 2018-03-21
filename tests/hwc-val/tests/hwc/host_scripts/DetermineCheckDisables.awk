# DetermineCheckDisables.awk
# Generate a list of checks to be reduced to warnings from input of "git log" in hwc.
#
BEGIN{
    CheckDisplayMode="eCheckDisplayMode ";
    CheckPanelFitterMode="eCheckPanelFitterMode ";
    CheckHwcVersion="eCheckHwcVersion ";
    CheckPlaneBlending="eCheckPlaneBlending ";
    CheckSfFallback="eCheckSfFallback ";
}

/Change-Id: Ic601206b19f63774cdeec0ae7ddbd74429e65b9b/ { CheckDisplayMode=""; }
/Change-Id: Id8289d9a6471be45d8a033630bec5f29d8e6cefa/ { CheckPanelFitterMode=""; }
/Change-Id: Ibedc9643125015968b62257478c2e29b113da4f3/ { CheckHwcVersion=""; } # Can't check HWC version on old (L-MR1) commits
/Change-Id: I8a3c4ec9a1bf2f4722b4884505c9431d103be3cc/ { CheckPlaneBlending=""; } # Expect blending errors if hardware alpha support not available
#/Change-Id: Ie31406c7881c20b7583e4551ba7110fc921755b3/ { CheckSfFallback=""; } # With two-stage fallback composer available, SF fallback is not allowed

END{
    printf(CheckDisplayMode);
    printf(CheckPanelFitterMode);
    printf(CheckHwcVersion);
    printf(CheckPlaneBlending);
    printf(CheckSfFallback);
    printf("\n");
}
