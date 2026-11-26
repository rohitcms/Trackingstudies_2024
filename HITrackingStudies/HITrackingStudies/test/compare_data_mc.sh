#!/bin/bash
# Data vs MC, separately for prompt reco and for the 17_0_0 rereco with the prompt GT, per year,
# using the ntuples already made by run_compare_local.sh:
#   MC   : compare<YEAR>/trk_prompt.root, trk_rereco.root                       (COMPARE=prompt)
#   data : compare<YEAR>_data_prompt/trk_prompt.root, trk_rereco_promptgt.root  (COMPARE=data_prompt)
# Within data (and within MC) prompt reco and rereco contain the same events in the same format, so the
# differences between data and MC that are not due to reconstruction (event mix, MiniAOD vs AOD, detector
# simulation) are the same in both data/MC ratios; the change between them is the reconstruction effect.
#
# Usage (from this test/ directory, after cmsenv):
#   [TRACKS=pixel] ./compare_data_mc.sh <YEAR> [NORM] [BASEDIR]
#     YEAR   : 2023 or 2024
#     NORM   : area (default, shapes) or event (tracks per event)
#     BASEDIR: where the compare<YEAR>* directories are, default /tmp/$USER
set -e
YEAR=${1:?give the year: 2023 or 2024}
NORM=${2:-area}
BASE=${3:-/tmp/$USER}
TRACKS=${TRACKS:-general}
case $YEAR in 2023|2024) ;; *) echo "year must be 2023 or 2024"; exit 1;; esac
case $TRACKS in general) SUF=""; PFX="";; pixel) SUF="_pixel"; PFX="Pixel_";; *) echo "TRACKS must be general or pixel"; exit 1;; esac
case $NORM in area|event) ;; *) echo "NORM must be area or event"; exit 1;; esac

MCP=$BASE/compare${YEAR}/trk_prompt${SUF}.root
MCR=$BASE/compare${YEAR}/trk_rereco${SUF}.root
DP=$BASE/compare${YEAR}_data_prompt/trk_prompt${SUF}.root
DR=$BASE/compare${YEAR}_data_prompt/trk_rereco_promptgt${SUF}.root
for f in $MCP $MCR $DP $DR; do [ -s "$f" ] || { echo "missing $f (run run_compare_local.sh with COMPARE=prompt and COMPARE=data_prompt first)"; exit 1; }; done

OUT=$BASE/compare_data_mc/plots
TESTDIR=$(cd "$(dirname "$0")" && pwd)
cd "$TESTDIR"/plottingMacro
NT="$NORM"; [ $NORM = area ] && NT="normalized to unit area"; [ $NORM = event ] && NT="per event"

echo "=== Prompt reco: data vs MC ($YEAR)"
root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$DP\",\"$MCP\",\"Data, prompt reco\",\"MC, prompt reco\",0,100,\"$OUT/${PFX}Prompt_DataVsMC_${YEAR}_${NORM}\",\"$TRACKS\",\"$NORM\")"
echo "=== ReReco 17_0_0, prompt GT: data vs MC ($YEAR)"
root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$DR\",\"$MCR\",\"Data, rereco\",\"MC, rereco\",0,100,\"$OUT/${PFX}ReReco_DataVsMC_${YEAR}_${NORM}\",\"$TRACKS\",\"$NORM\")"
echo "=== Overlay of the two data/MC ratios ($YEAR)"
root -l -b -q "plotRatioOverlay.C(\"$OUT/${PFX}Prompt_DataVsMC_${YEAR}_${NORM}/trackQuantities.root\",\"Prompt reco\",\"$OUT/${PFX}ReReco_DataVsMC_${YEAR}_${NORM}/trackQuantities.root\",\"ReReco 17_0_0\",\"\",\"\",\"$OUT/${PFX}Overlay_DataOverMC_${YEAR}_${NORM}\",\"Data / MC ${YEAR}, ${NT}\")"
echo "Done. Plots in $OUT/"
