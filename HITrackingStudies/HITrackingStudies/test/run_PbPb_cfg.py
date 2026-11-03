###Options to run
'''
To run it, please, do e.g.:
cmsRun run_PbPb_cfg.py sample="MC_RecoDebug" n=100 usePixelTrks=False  runOverStreams=False 

sample="MC_RecoDebug","MC_Reco_AOD","MC_MiniAOD","Data_Reco_AOD","Data_MiniAOD"
n=integer number of events
runOverStreams=False or True
usePixelTrks=False or True

IMPORTANT: only run runOverStreams=True together with sample="Data_Reco_AOD". FIXME: this option is not working for now

To change input files, please, look at pbpb.py file


for Quick change between 2023 and 2024 data, check the the follwoing items

process.GlobalTag --> Needs to change for 2023 and 2024
process.hltMB.HLTPaths --> Needs to change for 2023 and 2024
process.TFileService --> This is just the name of produced root file, you can change or not
'''

import FWCore.ParameterSet.Config as cms

process = cms.Process('TRACKANA')
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 100
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load('HITrackingStudies.HITrackingStudies.HITrackCorrectionAnalyzer_cfi')

import FWCore.ParameterSet.VarParsing as VarParsing
options = VarParsing.VarParsing('analysis')

options.register ('sample',
                  "MC_RecoDebug", # default value
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.string,          # string, int, bool or float
                  "sample")
options.register ('n',
                  50, # default value
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.int,          # string, int, bool or float
                  "n")
options.register ('runOverStreams',
                  False, # default value
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.bool,          # string, int, bool or float
                  "runOverStreams")
options.register ('usePixelTrks',
                  False, # default value
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.bool,          # string, int, bool or float
                  "usePixelTrks")
options.register ('globalTag',
                  "", # default value: empty means use the GlobalTag hardcoded below
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.string,          # string, int, bool or float
                  "globalTag")
options.register ('applyHLT',
                  True, # default value
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.bool,          # string, int, bool or float
                  "apply the hltMB trigger selection (set False for MC without the requested HLT paths)")
options.register ('eventList',
                  "", # default value: empty means process all events
                  VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.string,          # string, int, bool or float
                  "text file with one run:lumi:event per line; only these events are processed")
# Also available from VarParsing('analysis'):
#   inputFiles=file1.root,file2.root  -> overrides the file lists in pbpb.py
#   outputFile=name.root              -> overrides the TFileService file name
options.parseArguments()

from pbpb import pbpb_mc_recodebug as pbpb
if options.sample == "MC_Reco_AOD":
    from pbpb import pbpb_mc_reco_aod as pbpb
if options.sample == "Data_Reco_AOD":
    from pbpb import pbpb_data_reco_aod as pbpb
if options.sample == "Data_Reco_AOD" and options.runOverStreams==True :
    from pbpb import pbpb_data_reco_aod_streams as pbpb
if options.sample == "Data_MiniAOD":
    from pbpb import pbpb_data_miniaod as pbpb
if options.sample == "Data_MiniAOD" and options.runOverStreams==True :
    from pbpb import pbpb_data_miniaod_streams as pbpb
if options.sample == "MC_MiniAOD":
    from pbpb import pbpb_mc_miniaod as pbpb


process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(options.n)
)

process.options = cms.untracked.PSet(
    wantSummary = cms.untracked.bool(True),
)

#process.TFileService = cms.Service("TFileService", fileName = cms.string('2023_Hydjet_MC_MAOD_PixelTracks.root'))
#process.TFileService = cms.Service("TFileService", fileName = cms.string('2023_Hydjet_MC_RecoDebug_GeneralTracks.root'))
#process.TFileService = cms.Service("TFileService", fileName = cms.string('2023_data_MAOD_GeneralTracks_nhits2Check_True.root'))
#process.TFileService = cms.Service("TFileService", fileName = cms.string('2024_Run387853_data_Streamer_MAOD_GeneralTracks.root'))
process.TFileService = cms.Service("TFileService", fileName = cms.string('2024_HydjetMC_GeneralTracks_MAOD.root'))
if options.outputFile != "output.root" :
    process.TFileService.fileName = cms.string(options.outputFile)
#process.TFileService = cms.Service("TFileService", fileName = cms.string('2024_Data__PixelTracks_New.root')
#process.TFileService = cms.Service("TFileService", fileName = cms.string('HybridZS_2023Era_606_EvtFilter.root'))

process.load("SimTracker.TrackAssociation.trackingParticleRecoTrackAsssociation_cfi")
process.tpRecoAssocGeneralTracks = process.trackingParticleRecoTrackAsssociation.clone()
process.tpRecoAssocGeneralTracks.label_tr = cms.InputTag("generalTracks")

if options.usePixelTrks == True:
    process.tpRecoAssocGeneralTracks.label_tr = cms.InputTag("hiConformalPixelTracks")

process.load("SimTracker.TrackAssociatorProducers.quickTrackAssociatorByHits_cfi")
process.quickTrackAssociatorByHits.SimToRecoDenominator = cms.string('reco')

process.load("SimTracker.TrackerHitAssociation.tpClusterProducerDefault_cfi")
process.tpClusterProducer  = process.tpClusterProducerDefault.clone()

# Input source
process.source = cms.Source("PoolSource",
    duplicateCheckMode = cms.untracked.string("noDuplicateCheck"),
    fileNames =  cms.untracked.vstring(pbpb),
    skipEvents = cms.untracked.uint32(0),
    secondaryFileNames = cms.untracked.vstring()
)
if options.runOverStreams == True :
    process.source = cms.Source("NewEventStreamFileReader",
            fileNames =  cms.untracked.vstring(pbpb),
            skipEvents = cms.untracked.uint32(0)
    )
if len(options.inputFiles) > 0 :
    process.source.fileNames = cms.untracked.vstring(options.inputFiles)
if options.eventList != "" :
    _evts = [l.strip() for l in open(options.eventList) if l.strip() and not l.startswith('#')]
    process.source.eventsToProcess = cms.untracked.VEventRange(*_evts)

### centrality ###
process.load("RecoHI.HiCentralityAlgos.CentralityBin_cfi")
process.centralityBin.Centrality = cms.InputTag("hiCentrality")
process.centralityBin.centralityVariable = cms.string("HFtowers")

### Track cuts ###
# input collections
process.HITrackCorrections.centralitySrc = cms.InputTag("centralityBin","HFtowers")
process.HITrackCorrections.trackSrc = cms.InputTag("generalTracks")
if options.usePixelTrks == True:
    process.HITrackCorrections.trackSrc = cms.InputTag("hiConformalPixelTracks")
process.HITrackCorrections.vertexSrc = cms.InputTag("offlinePrimaryVertices")
process.HITrackCorrections.qualityString = cms.string("highPurity")
process.HITrackCorrections.pfCandSrc = cms.InputTag("particleFlow")
process.HITrackCorrections.jetSrc = cms.InputTag("ak4CaloJets")
# options
process.HITrackCorrections.useCentrality = True
process.HITrackCorrections.applyTrackCuts = True
if options.usePixelTrks == True:
    process.HITrackCorrections.applyTrackCuts = False
process.HITrackCorrections.fillNTuples = False
process.HITrackCorrections.applyVertexZCut = False
process.HITrackCorrections.doVtxReweighting = False
process.HITrackCorrections.doCaloMatched = False
# cut values
process.HITrackCorrections.dxyErrMax = 3.0 #default 3.0
process.HITrackCorrections.dzErrMax = 3.0 # default 3.0
process.HITrackCorrections.ptErrMax = 9999999999999.0 # no cut basically
process.HITrackCorrections.nhitsMin = 0 # default 0 
process.HITrackCorrections.chi2nMax = 9999999999999.0 # no cut basically
process.HITrackCorrections.reso = 0.5 # not applying, just ignore

#process.HITrackCorrections.crossSection = 1.0 #1.0 is no reweigh
#algo
process.HITrackCorrections.algoParameters = cms.vint32(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46)
# vertex reweight parameters
process.HITrackCorrections.vtxWeightParameters = cms.vdouble(0.0306789, 0.427748, 5.16555, 0.0228019, -0.02049, 7.01258 )
###
from Configuration.AlCa.GlobalTag import GlobalTag
if (options.sample == "MC_MiniAOD" or options.sample == "MC_RecoDebug" or options.sample == "MC_Reco_AOD"):
    #process.GlobalTag = GlobalTag(process.GlobalTag, '132X_mcRun3_2023_realistic_HI_v9', '') # for 2023 
    #process.GlobalTag = GlobalTag(process.GlobalTag, '141X_mcRun3_2024_realistic_HI_v5', '') # for 2024
    process.GlobalTag = GlobalTag(process.GlobalTag, '141X_mcRun3_2024_realistic_HI_v8', '') # for 2024
    process.GlobalTag.snapshotTime = cms.string("9999-12-31 23:59:59.000")
    process.GlobalTag.toGet.extend([
        cms.PSet(record = cms.string("HeavyIonRcd"),
                 tag = cms.string("CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal"),
                 connect = cms.string("sqlite_file:CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db"),
                 label = cms.untracked.string("HFtowers")
        ),
    ])
if (options.sample == "Data_Reco_AOD" or options.sample == "Data_MiniAOD"):
    #process.GlobalTag = GlobalTag(process.GlobalTag, '132X_dataRun3_Prompt_v7', '') # 2023
    process.GlobalTag = GlobalTag(process.GlobalTag, '141X_dataRun3_Prompt_v3', '') # 2024
    process.GlobalTag.snapshotTime = cms.string("9999-12-31 23:59:59.000")
    process.GlobalTag.toGet.extend([
        cms.PSet(record = cms.string("HeavyIonRcd"),
                 tag = cms.string("CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal"),
                 connect = cms.string("sqlite_file:CentralityTable_HFtowers200_DataPbPb_periHYDJETshape_run3v1302x04_offline_Nominal.db"),
                 label = cms.untracked.string("HFtowers")
             ),
    ])
if options.globalTag != "" :
    process.GlobalTag.globaltag = cms.string(options.globalTag)
###
    
#forest style analyzers (anaTrack module) (not affected by HITrackCorrections code)
process.load('HITrackingStudies.AnalyzerCode.trackAnalyzer_cff')
process.anaTrack.useCentrality = True
process.anaTrack.trackSrc = 'generalTracks'
process.anaTrack.mvaSrc = cms.InputTag("generalTracks","MVAValues")
if options.usePixelTrks == True:
    process.anaTrack.trackSrc = 'hiConformalPixelTracks'
    process.anaTrack.doMVA = cms.bool(False)
process.anaTrack.doSimTrack = True
process.anaTrack.doSimVertex = True
process.anaTrack.fillSimTrack = True
process.anaTrack.doPFMatching = False
process.anaTrack.doHighestPtVertex = False
process.anaTrack.doTrackVtxWImpPar = False
process.anaSeq = cms.Sequence(process.anaTrack)
if (options.sample == "MC_Reco_AOD" or options.sample == "MC_MiniAOD" or options.sample == "Data_Reco_AOD" or options.sample == "Data_MiniAOD"):
    process.anaTrack.doSimTrack = False
    process.anaTrack.doSimVertex = False
    process.anaTrack.fillSimTrack = False
    if (options.sample == "MC_MiniAOD" or options.sample == "Data_MiniAOD"):
        process.load('HITrackingStudies.AnalyzerCode.unpackedTracksAndVertices_cfi')
        process.anaTrack.trackSrc = 'unpackedTracksAndVertices'
        process.anaTrack.vertexSrc = cms.vstring(['unpackedTracksAndVertices'])
        process.anaTrack.doMVA = cms.bool(False)
        process.hiPixelTracks = process.unpackedTracksAndVertices.clone(
                packedCandidates = cms.VInputTag("hiPixelTracks"),
                packedCandidateNormChi2Map = cms.VInputTag(""),
                secondaryVertices = ""
        )
        if options.usePixelTrks == True:
           process.anaTrack.trackSrc = 'hiPixelTracks'
           process.anaTrack.vertexSrc = cms.vstring(['hiPixelTracks'])
        process.anaSeq = cms.Sequence(process.unpackedTracksAndVertices * process.hiPixelTracks * process.anaTrack)

###trigger selection
import HLTrigger.HLTfilters.hltHighLevel_cfi
process.hltMB = HLTrigger.HLTfilters.hltHighLevel_cfi.hltHighLevel.clone()
#process.hltMB.HLTPaths = ["HLT_HIMinimumBiasHF1AND*"] # for 2023
process.hltMB.HLTPaths = ["HLT_HIMinimumBiasHF1AND*"] # for 2024 #
process.hltMB.andOr = cms.bool(True)  # True = OR, False = AND between the HLT paths
process.hltMB.throw = cms.bool(False) # throw exception on unknown path names
if options.applyHLT == False :
    process.hltMB = cms.EDFilter("HLTBool", result = cms.bool(True)) # pass-through: no trigger selection

'''
# event analysis
## it will work for AOD. For mAOD, offlinePrimaryVertices and path of hfCoincFilter_cff need to change
#process.load('HeavyIonsAnalysis.EventAnalysis.clusterCompatibilityFilter_cfi')
#process.load('HeavyIonsAnalysis.EventAnalysis.collisionEventSelection_cff')
#process.primaryVertexFilter.src = cms.InputTag("offlinePrimaryVertices")
#process.load('HeavyIonsAnalysis.Configuration.hfCoincFilter_cff')

# Define the event selection sequence
process.eventFilter = cms.Sequence(
    process.hfCoincFilter2Th4 *
    process.clusterCompatibilityFilter *
    process.primaryVertexFilter 
)
'''

process.p = cms.Path(
    #process.eventFilter *
    process.tpClusterProducer *
    process.quickTrackAssociatorByHits *
    process.tpRecoAssocGeneralTracks *
    process.centralityBin *
    process.hltMB *
    process.HITrackCorrections 
    #process.anaSeq ## comment if you want to save only histograms
)

if (options.sample == "MC_Reco_AOD" or options.sample == "Data_Reco_AOD" or options.sample == "MC_MiniAOD" or options.sample == "Data_MiniAOD"):
    process.p = cms.Path(
        #process.eventFilter *
        #process.hltMB *
        process.centralityBin *
        process.anaSeq
    )
