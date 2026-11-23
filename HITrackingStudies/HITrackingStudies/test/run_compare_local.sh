#!/bin/bash
# Local prompt-reco vs rereco (CMSSW_17_0_0) tracking comparison for one year, on lxplus.
#
# Usage (from this test/ directory, after cmsenv in CMSSW_17_0_0 or later, with a valid grid proxy):
#   ./run_compare_local.sh 2023 [NFILES] [OUTDIR]
#   ./run_compare_local.sh 2024 [NFILES] [OUTDIR]
# NFILES: number of files (job numbers 1..NFILES) per dataset, default 10
# OUTDIR: default /tmp/$USER/compare<YEAR>
#
# Steps: file lists from DAS -> events common to both samples -> cmsRun on both (in parallel,
# only the common events) -> efficiency/fake-rate and track-quantity plots.
set -e

YEAR=${1:?give the year: 2023 or 2024}
NFILES=${2:-10}
OUT=${3:-/tmp/$USER/compare${YEAR}}

case $YEAR in
  2023)
    PROMPT_DS=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_13_2_16_patch1_RECO_2026_10_27-4306658f456933c14c26b2d86ea9ab26/USER
    RERECO_DS=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_RECO_2026_11_20-62a5f9c62f9bc79d872829fe458ba4d1/USER
    GT=132X_mcRun3_2023_realistic_HI_v12
    PROMPT_LABEL="13_2_16_patch1 (2023)";;
  2024)
    PROMPT_DS=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_14_1_9_patch3_RECO_2026_10_27-943058267aeaca9e9e8960ca73d2b070/USER
    RERECO_DS=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_RECO_2026_11_20-f9895636d4268b8129fbb0943a15b27a/USER
    GT=141X_mcRun3_2024_realistic_HI_v20
    PROMPT_LABEL="14_1_9_patch3 (2024)";;
  *) echo "year must be 2023 or 2024"; exit 1;;
esac
RERECO_LABEL="17_0_0 (${YEAR})"

[ -n "$CMSSW_BASE" ] || { echo "run cmsenv first"; exit 1; }
[ "${PLOTS_ONLY:-0}" = 1 ] || voms-proxy-info -exists >/dev/null 2>&1 || { echo "no valid grid proxy: run voms-proxy-init -voms cms"; exit 1; }
TESTDIR=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
cd "$OUT"
echo "Output in $OUT"

# PLOTS_ONLY=1 ./run_compare_local.sh YEAR [NFILES] [OUTDIR] : only redo the plots from existing outputs
if [ "${PLOTS_ONLY:-0}" != 1 ]; then

# 1. file lists: job numbers 1..NFILES, as local EOS paths
for s in prompt rereco; do
  ds=$PROMPT_DS; [ $s = rereco ] && ds=$RERECO_DS
  dasgoclient -query="file dataset=$ds instance=prod/phys03" \
    | awk -v n="$NFILES" '{ m = $0; sub(/.*_/, "", m); sub(/\.root$/, "", m); if (m ~ /^[0-9]+$/ && m+0 >= 1 && m+0 <= n) print }' \
    | sort -V | sed 's|^|file:/eos/cms|' > ${s}_files.txt
  echo "$s: $(wc -l < ${s}_files.txt) files"
  [ -s ${s}_files.txt ] || { echo "no files found for $s"; exit 1; }
done

# 2. events common to both samples
for s in prompt rereco; do
  for f in $(sed 's|^file:||' ${s}_files.txt); do edmFileUtil -e "$f"; done \
    | awk 'NF==4 && $3>0 {print $1":"$2":"$3}' | sort > ${s}_events.txt
done
comm -12 prompt_events.txt rereco_events.txt > common_events.txt
echo "events: prompt $(wc -l < prompt_events.txt), rereco $(wc -l < rereco_events.txt), common $(wc -l < common_events.txt)"
[ -s common_events.txt ] || { echo "no common events"; exit 1; }

# 3. cmsRun on both samples in parallel, common events only
cp "$TESTDIR"/CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db .
for s in prompt rereco; do
  cmsRun "$TESTDIR"/run_PbPb_cfg.py sample=MC_RecoDebug n=-1 applyHLT=False saveTree=True globalTag=$GT \
    inputFiles_load=${s}_files.txt eventList=common_events.txt outputFile=trk_${s}.root > ${s}.log 2>&1 &
done
echo "cmsRun running (logs: $OUT/prompt.log, $OUT/rereco.log) ..."
wait
grep -E "Exception|TrigReport Events total|TrigReport.*(HITrackCorrections|anaTrack)" prompt.log rereco.log || true
grep -q "Exception" prompt.log rereco.log && { echo "cmsRun failed, see logs"; exit 1; }

fi # PLOTS_ONLY

# 4. plots
cd "$TESTDIR"/plottingMacro
for c in "0 100" "0 10" "70 90"; do
  set -- $c
  root -l -b -q "plotHist_CompareTwoSamples.C(\"$OUT/trk_rereco.root\",\"$OUT/trk_prompt.root\",\"$RERECO_LABEL\",\"$PROMPT_LABEL\",$1,$2,\"$OUT/plots/Efficiency_cent${1}to${2}\")"
done
for c in "0 100" "0 10" "30 50" "70 90"; do
  set -- $c
  root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$OUT/trk_rereco.root\",\"$OUT/trk_prompt.root\",\"$RERECO_LABEL\",\"$PROMPT_LABEL\",$1,$2,\"$OUT/plots/TrackQuantities_cent${1}to${2}\")"
done
echo "Done. Plots in $OUT/plots/"
