#!/bin/bash
# Local prompt-reco vs rereco (CMSSW_17_0_0) tracking comparison for one year, on lxplus.
#
# Usage (from this test/ directory, after cmsenv in CMSSW_17_0_0 or later, with a valid grid proxy):
#   ./run_compare_local.sh 2023 [NFILES] [OUTDIR]
#   ./run_compare_local.sh 2024 [NFILES] [OUTDIR]
# NFILES: number of files (job numbers 1..NFILES) per dataset, default 10
# Optional environment variables:
#   PLOTS_ONLY=1        only redo the plots from existing outputs in OUTDIR
#   COPY_TO=/eos/user/r/<you>/somewhere   copy plots and logs there at the end (persistent, unlike /tmp)
#   COPY_ROOT=1         with COPY_TO, also copy the cmsRun output ROOT files (large)
#   TRACKS=pixel        pixel tracks (hiConformalPixelTracks, no track cuts) instead of highPurity generalTracks;
#                       outputs get a _pixel suffix and plots a Pixel_ prefix, so both can live in one OUTDIR.
#                       The file and event lists of an earlier run with the same NFILES are reused.
#   COMPARE=pre7        compare CMSSW_17_0_0 with the earlier CMSSW_17_0_0_pre7 rereco instead of prompt reco
#                       (default OUTDIR /tmp/$USER/compare<YEAR>_pre7)
# OUTDIR: default /tmp/$USER/compare<YEAR>
#
# Steps: file lists from DAS -> events common to both samples -> cmsRun on both (in parallel,
# only the common events) -> efficiency/fake-rate and track-quantity plots.
set -e

YEAR=${1:?give the year: 2023 or 2024}
NFILES=${2:-10}
COMPARE=${COMPARE:-prompt}
case $COMPARE in
  prompt) OUT=${3:-/tmp/$USER/compare${YEAR}};;
  pre7)   OUT=${3:-/tmp/$USER/compare${YEAR}_pre7};;
  *) echo "COMPARE must be prompt or pre7"; exit 1;;
esac

case $YEAR in
  2023)
    PROMPT_DS=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_13_2_16_patch1_RECO_2026_10_27-4306658f456933c14c26b2d86ea9ab26/USER
    RERECO_DS=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_RECO_2026_11_20-62a5f9c62f9bc79d872829fe458ba4d1/USER
    PRE7_DS=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-ff87f12058bfd417ba6d2a189e1266f4/USER
    GT=132X_mcRun3_2023_realistic_HI_v12
    PROMPT_LABEL="13_2_16_patch1 (2023)";;
  2024)
    PROMPT_DS=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_14_1_9_patch3_RECO_2026_10_27-943058267aeaca9e9e8960ca73d2b070/USER
    RERECO_DS=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_RECO_2026_11_20-f9895636d4268b8129fbb0943a15b27a/USER
    PRE7_DS=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-b3a7db92047aba45682b092f1c5750de/USER
    GT=141X_mcRun3_2024_realistic_HI_v20
    PROMPT_LABEL="14_1_9_patch3 (2024)";;
  *) echo "year must be 2023 or 2024"; exit 1;;
esac
RERECO_LABEL="17_0_0 (${YEAR})"
# reference sample (denominator of the ratios): prompt reco, or the 17_0_0_pre7 rereco
if [ "$COMPARE" = pre7 ]; then
  REF=pre7;   REF_DS=$PRE7_DS;   REF_LABEL="17_0_0_pre7 (${YEAR})"
else
  REF=prompt; REF_DS=$PROMPT_DS; REF_LABEL="$PROMPT_LABEL"
fi

TRACKS=${TRACKS:-general}
case $TRACKS in
  general) SUF="";       PFX="";       PIXOPT=False;;
  pixel)   SUF="_pixel"; PFX="Pixel_"; PIXOPT=True;;
  *) echo "TRACKS must be general or pixel"; exit 1;;
esac

[ -n "$CMSSW_BASE" ] || { echo "run cmsenv first"; exit 1; }
[ "${PLOTS_ONLY:-0}" = 1 ] || voms-proxy-info -exists >/dev/null 2>&1 || { echo "no valid grid proxy: run voms-proxy-init -voms cms"; exit 1; }
TESTDIR=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
cd "$OUT"
echo "Output in $OUT"

# PLOTS_ONLY=1 ./run_compare_local.sh YEAR [NFILES] [OUTDIR] : only redo the plots from existing outputs
if [ "${PLOTS_ONLY:-0}" != 1 ]; then

if [ -s common_events.txt ] && [ -s ${REF}_files.txt ] && [ -s rereco_files.txt ] && [ "$(cat nfiles.txt 2>/dev/null)" = "$NFILES" ]; then
  echo "Reusing file and event lists in $OUT (NFILES=$NFILES): $(wc -l < common_events.txt) common events"
else

# 1. file lists: job numbers 1..NFILES, as local EOS paths
for s in $REF rereco; do
  ds=$REF_DS; [ $s = rereco ] && ds=$RERECO_DS
  dasgoclient -query="file dataset=$ds instance=prod/phys03" \
    | awk -v n="$NFILES" '{ m = $0; sub(/.*_/, "", m); sub(/\.root$/, "", m); if (m ~ /^[0-9]+$/ && m+0 >= 1 && m+0 <= n) print }' \
    | sort -V | sed 's|^|file:/eos/cms|' > ${s}_files.txt
  echo "$s: $(wc -l < ${s}_files.txt) files"
  [ -s ${s}_files.txt ] || { echo "no files found for $s"; exit 1; }
done

# 2. events common to both samples
for s in $REF rereco; do
  for f in $(sed 's|^file:||' ${s}_files.txt); do edmFileUtil -e "$f"; done \
    | awk 'NF==4 && $3>0 {print $1":"$2":"$3}' | sort > ${s}_events.txt
done
comm -12 ${REF}_events.txt rereco_events.txt > common_events.txt
echo "events: $REF $(wc -l < ${REF}_events.txt), rereco $(wc -l < rereco_events.txt), common $(wc -l < common_events.txt)"
[ -s common_events.txt ] || { echo "no common events"; exit 1; }
echo "$NFILES" > nfiles.txt

fi # lists

# check the track collection exists in both samples
if [ "$TRACKS" = pixel ]; then
  for s in $REF rereco; do
    f=$(head -1 ${s}_files.txt | sed 's|^file:||')
    edmDumpEventContent "$f" | grep -q hiConformalPixelTracks \
      || { echo "hiConformalPixelTracks not found in $s file $f"; exit 1; }
  done
fi

# 3. cmsRun on both samples in parallel, common events only
cp "$TESTDIR"/CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db .
for s in $REF rereco; do
  cmsRun "$TESTDIR"/run_PbPb_cfg.py sample=MC_RecoDebug n=-1 applyHLT=False saveTree=True globalTag=$GT usePixelTrks=$PIXOPT \
    inputFiles_load=${s}_files.txt eventList=common_events.txt outputFile=trk_${s}${SUF}.root > ${s}${SUF}.log 2>&1 &
done
echo "cmsRun running ($TRACKS tracks; logs: $OUT/${REF}${SUF}.log, $OUT/rereco${SUF}.log) ..."
wait
grep -E "Exception|TrigReport Events total|TrigReport.*(HITrackCorrections|anaTrack)" ${REF}${SUF}.log rereco${SUF}.log || true
if grep -q "Exception" ${REF}${SUF}.log rereco${SUF}.log; then echo "cmsRun failed, see logs"; exit 1; fi

fi # PLOTS_ONLY

# 4. plots
cd "$TESTDIR"/plottingMacro
for c in "0 100" "0 10" "70 90"; do
  set -- $c
  root -l -b -q "plotHist_CompareTwoSamples.C(\"$OUT/trk_rereco${SUF}.root\",\"$OUT/trk_${REF}${SUF}.root\",\"$RERECO_LABEL\",\"$REF_LABEL\",$1,$2,\"$OUT/plots/${PFX}Efficiency_cent${1}to${2}\",\"$TRACKS\")"
done
for c in "0 100" "0 10" "30 50" "70 90"; do
  set -- $c
  root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$OUT/trk_rereco${SUF}.root\",\"$OUT/trk_${REF}${SUF}.root\",\"$RERECO_LABEL\",\"$REF_LABEL\",$1,$2,\"$OUT/plots/${PFX}TrackQuantities_cent${1}to${2}\",\"$TRACKS\")"
done
echo "Done. Plots in $OUT/plots/"

# COPY_TO=/eos/user/... : copy plots, logs and printout-relevant files to a persistent place
if [ -n "${COPY_TO:-}" ]; then
  DEST="$COPY_TO/$(basename "$OUT")"
  mkdir -p "$DEST"
  cp -r "$OUT/plots" "$DEST/"
  cp "$OUT"/*.log "$OUT"/*_files.txt "$OUT"/common_events.txt "$DEST/" 2>/dev/null || true
  if [ "${COPY_ROOT:-0}" = 1 ]; then cp "$OUT"/trk_*.root "$DEST/"; fi
  echo "Copied to $DEST"
fi
