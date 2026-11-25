#!/bin/bash
# Local tracking comparison of two reconstructions of the same events, for one year, on lxplus.
#
# Usage (from this test/ directory, after cmsenv in CMSSW_17_0_0 or later, with a valid grid proxy):
#   [COMPARE=...] [TRACKS=...] ./run_compare_local.sh <YEAR> [NFILES] [OUTDIR]
#     YEAR   : 2023 or 2024
#     NFILES : number of files (job numbers 1..NFILES) per dataset, default 10
#     OUTDIR : default /tmp/$USER/compare<YEAR>[_<COMPARE>]
#
# COMPARE (sample 1 = blue = numerator of the ratios, sample 2 = red = reference):
#   prompt      (default) MC  : rereco CMSSW_17_0_0           vs prompt reco (13_2_16_patch1 / 14_1_9_patch3)
#   pre7                  MC  : rereco CMSSW_17_0_0           vs rereco CMSSW_17_0_0_pre7
#   data_prompt           data: rereco 17_0_0 with prompt GT  vs prompt reco
#   data_gt               data: rereco 17_0_0 with offline GT vs rereco 17_0_0 with prompt GT
# MC   : efficiency/fake-rate plots (HITrackCorrections) + track-quantity plots
# data : track-quantity plots only (no simulated truth)
#
# Optional environment variables:
#   TRACKS=pixel   pixel tracks instead of highPurity generalTracks (outputs *_pixel, plots Pixel_*)
#   PLOTS_ONLY=1   only redo the plots from existing outputs in OUTDIR
#   COPY_TO=/eos/user/r/<you>/somewhere   copy plots and logs there at the end (persistent, unlike /tmp)
#   COPY_ROOT=1    with COPY_TO, also copy the cmsRun output ROOT files (large)
#   GT_1=... GT_2=...  override the GlobalTag used in cmsRun for sample 1 / sample 2
# File and event lists of an earlier run in the same OUTDIR with the same NFILES are reused.
#
# Steps: file lists from DAS -> events common to both samples -> cmsRun on both (in parallel,
# only the common events) -> plots.
set -e

YEAR=${1:?give the year: 2023 or 2024}
NFILES=${2:-10}
COMPARE=${COMPARE:-prompt}

# ---------------------------------------------------------------- datasets
case $YEAR in
  2023)
    MC_PROMPT=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_13_2_16_patch1_RECO_2026_10_27-4306658f456933c14c26b2d86ea9ab26/USER
    MC_RERECO=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_RECO_2026_11_20-62a5f9c62f9bc79d872829fe458ba4d1/USER
    MC_PRE7=/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-ff87f12058bfd417ba6d2a189e1266f4/USER
    MC_GT=132X_mcRun3_2023_realistic_HI_v12
    MC_PROMPT_REL=13_2_16_patch1
    DATA_PROMPT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2023A_PromptReco_v2_2026_10_24-acd0e31c4b25dd82f9007318362a8c33/USER
    DATA_RERECO_PGT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2023A_CMSSW_17_0_0_170X_dataRun3_Prompt_v2_2026_10_24-c745711fa13e5293b9ff16449413f54d/USER
    DATA_RERECO_OGT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2023A_CMSSW_17_0_0_170X_dataRun3_v3_2026_10_24-5da7021447ca55c818f1d20d2d8e60ff/USER;;
  2024)
    MC_PROMPT=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_14_1_9_patch3_RECO_2026_10_27-943058267aeaca9e9e8960ca73d2b070/USER
    MC_RERECO=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_RECO_2026_11_20-f9895636d4268b8129fbb0943a15b27a/USER
    MC_PRE7=/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-b3a7db92047aba45682b092f1c5750de/USER
    MC_GT=141X_mcRun3_2024_realistic_HI_v20
    MC_PROMPT_REL=14_1_9_patch3
    DATA_PROMPT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2024B_PromptReco_v2_2026_10_24-acd0e31c4b25dd82f9007318362a8c33/USER
    DATA_RERECO_PGT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2024B_CMSSW_17_0_0_170X_dataRun3_Prompt_v2_2026_10_24-d93ebe3fb315e450fae22546bf235975/USER
    DATA_RERECO_OGT=/HIPhysicsRawPrime0/phys_heavyions_ops-HIPhysicsRawPrime0_HIRun2024B_CMSSW_17_0_0_170X_dataRun3_v3_2026_10_24-e2506aab423f82e4017a16fd2fdccf3b/USER;;
  *) echo "year must be 2023 or 2024"; exit 1;;
esac
# data GlobalTags as given for these reprocessings; check with: edmProvDump <file> | grep -i -A1 globaltag
DATA_PROMPT_GT=170X_dataRun3_Prompt_v2
DATA_OFFLINE_GT=170X_dataRun3_v3   # GT of the offline-GT rereco (v4 gives the same ntuple: tracks are read from file)

# ---------------------------------------------------------------- comparison: sample 1 (numerator) vs sample 2 (reference)
case $COMPARE in
  prompt)      KIND=mc;   OUTSUF=""
               N1=rereco;           DS1=$MC_RERECO;       G1=$MC_GT;           L1="17_0_0 (${YEAR})"
               N2=prompt;           DS2=$MC_PROMPT;       G2=$MC_GT;           L2="${MC_PROMPT_REL} (${YEAR})";;
  pre7)        KIND=mc;   OUTSUF="_pre7"
               N1=rereco;           DS1=$MC_RERECO;       G1=$MC_GT;           L1="17_0_0 (${YEAR})"
               N2=pre7;             DS2=$MC_PRE7;         G2=$MC_GT;           L2="17_0_0_pre7 (${YEAR})";;
  data_prompt) KIND=data; OUTSUF="_data_prompt"
               N1=rereco_promptgt;  DS1=$DATA_RERECO_PGT; G1=$DATA_PROMPT_GT;  L1="ReReco, prompt GT (${YEAR})"
               N2=prompt;           DS2=$DATA_PROMPT;     G2=$DATA_PROMPT_GT;  L2="Prompt reco (${YEAR})";;
  data_gt)     KIND=data; OUTSUF="_data_gt"
               N1=rereco_offlinegt; DS1=$DATA_RERECO_OGT; G1=$DATA_OFFLINE_GT; L1="ReReco, offline GT (${YEAR})"
               N2=rereco_promptgt;  DS2=$DATA_RERECO_PGT; G2=$DATA_PROMPT_GT;  L2="ReReco, prompt GT (${YEAR})";;
  *) echo "COMPARE must be prompt, pre7, data_prompt or data_gt"; exit 1;;
esac
G1=${GT_1:-$G1}; G2=${GT_2:-$G2}
OUT=${3:-/tmp/$USER/compare${YEAR}${OUTSUF}}

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
echo "Comparison: $L1 [$N1, GT $G1]  vs  $L2 [$N2, GT $G2], $TRACKS tracks"
echo "Output in $OUT"

if [ "${PLOTS_ONLY:-0}" != 1 ]; then

# ---------------------------------------------------------------- 1+2. file lists and common events
if [ -s common_events.txt ] && [ -s ${N1}_files.txt ] && [ -s ${N2}_files.txt ] && [ "$(cat nfiles.txt 2>/dev/null)" = "$NFILES" ]; then
  echo "Reusing file and event lists in $OUT (NFILES=$NFILES): $(wc -l < common_events.txt) common events"
else
  for s in $N1 $N2; do
    ds=$DS1; [ $s = $N2 ] && ds=$DS2
    dasgoclient -query="file dataset=$ds instance=prod/phys03" \
      | awk -v n="$NFILES" '{ m = $0; sub(/.*_/, "", m); sub(/\.root$/, "", m); if (m ~ /^[0-9]+$/ && m+0 >= 1 && m+0 <= n) print }' \
      | sort -V | sed 's|^|file:/eos/cms|' > ${s}_files.txt
    echo "$s: $(wc -l < ${s}_files.txt) files"
    [ -s ${s}_files.txt ] || { echo "no files found for $s"; exit 1; }
  done
  for s in $N1 $N2; do
    for f in $(sed 's|^file:||' ${s}_files.txt); do edmFileUtil -e "$f"; done \
      | awk 'NF==4 && $3>0 {print $1":"$2":"$3}' | sort > ${s}_events.txt
  done
  comm -12 ${N1}_events.txt ${N2}_events.txt > common_events.txt
  echo "events: $N1 $(wc -l < ${N1}_events.txt), $N2 $(wc -l < ${N2}_events.txt), common $(wc -l < common_events.txt)"
  [ -s common_events.txt ] || { echo "no common events"; exit 1; }
  echo "$NFILES" > nfiles.txt
fi

# ---------------------------------------------------------------- input format and collections
declare -A STYPE MVA
for s in $N1 $N2; do
  f=$(head -1 ${s}_files.txt | sed 's|^file:||')
  edmDumpEventContent "$f" > ${s}_content.txt
  if [ $KIND = mc ]; then
    STYPE[$s]=MC_RecoDebug
  elif grep -q '"generalTracks"' ${s}_content.txt; then
    STYPE[$s]=Data_Reco_AOD
  elif grep -q '"packedPFCandidates"' ${s}_content.txt; then
    STYPE[$s]=Data_MiniAOD
  else
    echo "cannot tell the data format of $s ($f): no generalTracks or packedPFCandidates"; exit 1
  fi
  MVA[$s]=True
  if [ ${STYPE[$s]} != Data_MiniAOD ] && ! grep -qE '"generalTracks" +"MVAValues"' ${s}_content.txt; then
    MVA[$s]=False; echo "$s: no generalTracks:MVAValues, running anaTrack without MVA"
  fi
  if [ $TRACKS = pixel ]; then
    pixcoll=hiConformalPixelTracks; [ ${STYPE[$s]} = Data_MiniAOD ] && pixcoll=hiPixelTracks
    grep -q "\"$pixcoll\"" ${s}_content.txt || { echo "$pixcoll not found in $s file $f"; exit 1; }
  fi
  echo "$s: ${STYPE[$s]}"
done
if [ "${STYPE[$N1]}" != "${STYPE[$N2]}" ]; then
  echo "WARNING: different data formats (${STYPE[$N1]} vs ${STYPE[$N2]}): MiniAOD stores tracks with reduced precision,"
  echo "         so differences can come from the format and not from the reconstruction."
fi

# ---------------------------------------------------------------- 3. cmsRun on both samples in parallel, common events only
cp "$TESTDIR"/CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db .
for s in $N1 $N2; do
  gt=$G1; [ $s = $N2 ] && gt=$G2
  cmsRun "$TESTDIR"/run_PbPb_cfg.py sample=${STYPE[$s]} n=-1 applyHLT=False saveTree=True globalTag=$gt \
    usePixelTrks=$PIXOPT trackMVA=${MVA[$s]} \
    inputFiles_load=${s}_files.txt eventList=common_events.txt outputFile=trk_${s}${SUF}.root > ${s}${SUF}.log 2>&1 &
done
echo "cmsRun running (logs: $OUT/${N1}${SUF}.log, $OUT/${N2}${SUF}.log) ..."
wait
grep -E "Exception|TrigReport Events total|TrigReport.*(HITrackCorrections|anaTrack)" ${N1}${SUF}.log ${N2}${SUF}.log || true
if grep -q "Exception" ${N1}${SUF}.log ${N2}${SUF}.log; then
  for s in $N1 $N2; do echo "== $s${SUF}.log:"; sed -n '/Begin Fatal Exception/,/End Fatal Exception/p' ${s}${SUF}.log | head -40; done
  echo "cmsRun failed, see logs"; exit 1
fi

fi # PLOTS_ONLY

# ---------------------------------------------------------------- 4. plots
cd "$TESTDIR"/plottingMacro
F1="$OUT/trk_${N1}${SUF}.root"; F2="$OUT/trk_${N2}${SUF}.root"
if [ $KIND = mc ]; then
  for c in "0 100" "0 10" "70 90"; do
    set -- $c
    root -l -b -q "plotHist_CompareTwoSamples.C(\"$F1\",\"$F2\",\"$L1\",\"$L2\",$1,$2,\"$OUT/plots/${PFX}Efficiency_cent${1}to${2}\",\"$TRACKS\")"
  done
fi
for c in "0 100" "0 10" "30 50" "70 90"; do
  set -- $c
  root -l -b -q "plotTrackQuantities_TwoSamples.C(\"$F1\",\"$F2\",\"$L1\",\"$L2\",$1,$2,\"$OUT/plots/${PFX}TrackQuantities_cent${1}to${2}\",\"$TRACKS\")"
done
echo "Done. Plots in $OUT/plots/"

# ---------------------------------------------------------------- copy to a persistent place
if [ -n "${COPY_TO:-}" ]; then
  DEST="$COPY_TO/$(basename "$OUT")"
  mkdir -p "$DEST"
  cp -r "$OUT/plots" "$DEST/"
  cp "$OUT"/*.log "$OUT"/*_files.txt "$OUT"/common_events.txt "$DEST/" 2>/dev/null || true
  if [ "${COPY_ROOT:-0}" = 1 ]; then cp "$OUT"/trk_*.root "$DEST/"; fi
  echo "Copied to $DEST"
fi
