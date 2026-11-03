### CRAB config to produce efficiency/fake-rate histograms (HITrackCorrections) for the
### 2023 HYDJET prompt (CMSSW_13_2_16_patch1) vs rereco (CMSSW_17_0_0_pre7) comparison.
###
### Usage (from this test/ directory, in a release that can read BOTH samples, i.e. >= CMSSW_17_0_0_pre7):
###   SAMPLE=prompt crab submit -c crab_config_HYDJET2023_compare.py
###   SAMPLE=rereco crab submit -c crab_config_HYDJET2023_compare.py
###   SAMPLE=prompt2024 crab submit -c crab_config_HYDJET2023_compare.py   (2024: CMSSW_14_1_9_patch3)
###   SAMPLE=rereco2024 crab submit -c crab_config_HYDJET2023_compare.py   (2024: CMSSW_17_0_0_pre7)
###
### BEFORE SUBMITTING:
###  1. Fill the GlobalTags below with the ones each sample was reconstructed with, e.g.
###       edmProvDump <one file of the dataset> | grep -i -A2 globaltag
###  2. Set outLFNDirBase / storageSite to a place you can write to.
###  3. Run one job interactively first (see README in this directory / instructions from the analyst).

import os
from WMCore.Configuration import Configuration

SAMPLES = {
    'prompt': {
        'dataset':   '/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_13_2_16_patch1_RECO_2026_10_27-4306658f456933c14c26b2d86ea9ab26/USER',
        'globalTag': 'FILL_ME',  # GT used for the CMSSW_13_2_16_patch1 RECO
    },
    'rereco': {
        'dataset':   '/MinBias_Drum5F_5p36TeV_hydjet/phys_heavyions-HYDJET_2023Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-ff87f12058bfd417ba6d2a189e1266f4/USER',
        'globalTag': 'FILL_ME',  # GT used for the CMSSW_17_0_0_pre7 RECO
    },
    # 2024 PbPb HYDJET, same GEN-SIM and same GT for both releases
    'prompt2024': {
        'dataset':   '/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_14_1_9_patch3_RECO_2026_10_27-943058267aeaca9e9e8960ca73d2b070/USER',
        'globalTag': '141X_mcRun3_2024_realistic_HI_v20',
    },
    'rereco2024': {
        'dataset':   '/Hydjet_MinBias_TuneCELLO_5p36TeV_pythia8/phys_heavyions-HYDJET_2024Run3_CMSSW_17_0_0_pre7_RECO_2026_10_27-b3a7db92047aba45682b092f1c5750de/USER',
        'globalTag': '141X_mcRun3_2024_realistic_HI_v20',
    },
}

which = os.environ.get('SAMPLE', 'prompt')
if which not in SAMPLES:
    raise ValueError("Set SAMPLE to one of %s (got '%s')" % (sorted(SAMPLES), which))
s = SAMPLES[which]
if s['globalTag'] == 'FILL_ME':
    raise ValueError("Fill the GlobalTag for sample '%s' in %s" % (which, __file__))

config = Configuration()

config.section_('General')
config.General.workArea        = 'crab_HYDJET_compare'
config.General.requestName     = 'HYDJET2023_trkEff_%s' % which
config.General.transferLogs    = True
config.General.transferOutputs = True

config.section_('JobType')
config.JobType.pluginName  = 'Analysis'
config.JobType.psetName    = 'run_PbPb_cfg.py'
config.JobType.pyCfgParams = ['sample=MC_RecoDebug',
                              'n=-1',
                              'usePixelTrks=False',
                              'applyHLT=False',   # 2023 HYDJET MC has no HLT_HIMinimumBiasHF1AND* path
                              'globalTag=%s' % s['globalTag'],
                              'outputFile=trk_%s.root' % which]
# centrality table read by run_PbPb_cfg.py via sqlite_file:
config.JobType.inputFiles  = ['CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db']
config.JobType.maxMemoryMB = 4000

config.section_('Data')
config.Data.inputDataset  = s['dataset']
config.Data.inputDBS      = 'phys03'
config.Data.splitting     = 'FileBased'
config.Data.unitsPerJob   = 5
config.Data.publication   = False
config.Data.outLFNDirBase = '/store/group/phys_heavyions/%s/TrackingStudies2023' % os.environ.get('USER', 'FILL_ME')

config.section_('Site')
config.Site.storageSite = 'T2_CH_CERN'
