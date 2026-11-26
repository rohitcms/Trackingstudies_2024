#!/bin/bash
# 2024 vs 2023 data comparison, separately for prompt reco and for the rereco (17_0_0, offline GT),
# using the ntuples already made by run_compare_local.sh (COMPARE=data_prompt and COMPARE=data_gt).
# Within a year, prompt reco and rereco contain the same events, so the event mix and the detector
# differences between the years are the same for both; whichever 2024/2023 ratio is closer to 1
# tells whether the rereco brings the years closer.
#
# Usage (from this test/ directory, after cmsenv):
#   [TRACKS=pixel] ./compare_years.sh [NORM] [BASEDIR]
#     NORM   : area (default, shapes) or event (tracks per event)
#     BASEDIR: where the compare<YEAR>_data_* directories are, default /tmp/$USER
set -e
NORM=${1:-area}
BASE=${2:-/tmp/$USER}
TRACKS=${TRACKS:-general}
case $TRACKS in general) SUF=""; PFX="";; pixel) SUF="_pixel"; PFX="Pixel_";; *) echo "TRACKS must be general or pixel"; exit 1;; esac
case $NORM in area|event) ;; *) echo "NORM must be area or event"; exit 1;; esac

P23=$BASE/compare2023_data_prompt/trk_prompt${SUF}.root
P24=$BASE/compare2024_data_prompt/trk_prompt${SUF}.root
R23=$BASE/compare2023_data_gt/trk_rereco_offlinegt${SUF}.root
R24=$BASE/compare2024_data_gt/trk_rereco_offlinegt${SUF}.root
for f in $P23 $P24 $R23 $R24; do [ -s "$f" ] || { echo "missing $f (run run_compare_local.sh with COMPARE=data_prompt / data_gt first)"; exit 1; }; done

OUT=$BASE/compare_years/plots
TESTDIR=$(cd "$(dirname "$0")" && pwd)
cd "$TESTDIR"/plottingMacro
NT="$NORM"; [ $NORM = area ] && NT="normalized to unit area"; [ $NORM = event ] && NT="per event"

echo "=== Prompt reco: 2024 vs 2023"
root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$P24\",\"$P23\",\"2024 prompt reco\",\"2023 prompt reco\",0,100,\"$OUT/${PFX}Prompt_2024vs2023_${NORM}\",\"$TRACKS\",\"$NORM\")"
echo "=== ReReco 17_0_0, offline GT: 2024 vs 2023"
root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$R24\",\"$R23\",\"2024 rereco\",\"2023 rereco\",0,100,\"$OUT/${PFX}ReRecoOfflineGT_2024vs2023_${NORM}\",\"$TRACKS\",\"$NORM\")"
echo "=== Overlay of the two 2024/2023 ratios"
root -l -b -q "plotRatioOverlay.C(\"$OUT/${PFX}Prompt_2024vs2023_${NORM}/trackQuantities.root\",\"Prompt reco\",\"$OUT/${PFX}ReRecoOfflineGT_2024vs2023_${NORM}/trackQuantities.root\",\"ReReco 17_0_0, offline GT\",\"\",\"\",\"$OUT/${PFX}Overlay_2024over2023_${NORM}\",\"2024 / 2023, ${NT}\")"
echo "Done. Plots in $OUT/"
