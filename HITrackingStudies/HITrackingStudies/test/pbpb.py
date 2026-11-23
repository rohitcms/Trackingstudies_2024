#pbpb_mc_recodebug=[
    #~~~~~~~~~~~~~For 2023~~~~~~~~~~~~~~~
#    'root://cmsxrootd.fnal.gov//store/user/sarteaga/MinBias_Drum5F_5p36TeV_hydjet/MinBias_PbPb_5p36TeV_Hydjet_RECODEBUG_132X_v9/241010_191615/0000/MinBias_PbPb_5p36TeV_Hydjet_132X_v9_RECODEBUG_1.root',
#    'root://cmsxrootd.fnal.gov//store/user/sarteaga/MinBias_Drum5F_5p36TeV_hydjet/MinBias_PbPb_5p36TeV_Hydjet_RECODEBUG_132X_v9/241010_191615/0000/MinBias_PbPb_5p36TeV_Hydjet_132X_v9_RECODEBUG_2.root',
#    'root://cmsxrootd.fnal.gov//store/user/sarteaga/MinBias_Drum5F_5p36TeV_hydjet/MinBias_PbPb_5p36TeV_Hydjet_RECODEBUG_132X_v9/241010_191615/0000/MinBias_PbPb_5p36TeV_Hydjet_132X_v9_RECODEBUG_3.root',
#    'root://cmsxrootd.fnal.gov//store/user/sarteaga/MinBias_Drum5F_5p36TeV_hydjet/MinBias_PbPb_5p36TeV_Hydjet_RECODEBUG_132X_v9/241010_191615/0000/MinBias_PbPb_5p36TeV_Hydjet_132X_v9_RECODEBUG_4.root',
#    'root://cmsxrootd.fnal.gov//store/user/sarteaga/MinBias_Drum5F_5p36TeV_hydjet/MinBias_PbPb_5p36TeV_Hydjet_RECODEBUG_132X_v9/241010_191615/0000/MinBias_PbPb_5p36TeV_Hydjet_132X_v9_RECODEBUG_5.root'

    #~~~~~~~~~~~~~For 2024~~~~~~~~~~~~~~~
    #'root://store/group/phys_heavyions/cbennett/crabSubmit_RECODEBUG_updatedEra_updateGT_HYDJET_5360GeV_141X_2024-10-14/HYDJET_5360GeV_141X/HYDJET_5360GeV_RECODEBUG_updatedEra_updateGT_141X_2024-10-14/241015_033632/0000/HYDJET_5360GeV_RECODEBUG_1.root',
    #'root://store/group/phys_heavyions/cbennett/crabSubmit_RECODEBUG_updatedEra_updateGT_HYDJET_5360GeV_141X_2024-10-14/HYDJET_5360GeV_141X/HYDJET_5360GeV_RECODEBUG_updatedEra_updateGT_141X_2024-10-14/241015_033632/0000/HYDJET_5360GeV_RECODEBUG_2.root',
    #'root://store/group/phys_heavyions/cbennett/crabSubmit_RECODEBUG_updatedEra_updateGT_HYDJET_5360GeV_141X_2024-10-14/HYDJET_5360GeV_141X/HYDJET_5360GeV_RECODEBUG_updatedEra_updateGT_141X_2024-10-14/241015_033632/0000/HYDJET_5360GeV_RECODEBUG_3.root',
    #'root://store/group/phys_heavyions/cbennett/crabSubmit_RECODEBUG_updatedEra_updateGT_HYDJET_5360GeV_141X_2024-10-14/HYDJET_5360GeV_141X/HYDJET_5360GeV_RECODEBUG_updatedEra_updateGT_141X_2024-10-14/241015_033632/0000/HYDJET_5360GeV_RECODEBUG_4.root',
    #'root://store/group/phys_heavyions/cbennett/crabSubmit_RECODEBUG_updatedEra_updateGT_HYDJET_5360GeV_141X_2024-10-14/HYDJET_5360GeV_141X/HYDJET_5360GeV_RECODEBUG_updatedEra_updateGT_141X_2024-10-14/241015_033632/0000/HYDJET_5360GeV_RECODEBUG_5.root'
    
#]

# use this if you are giving txt file as input
# the list is looked up next to this file, so cmsRun can be started from any directory;
# if it is missing the list is empty (use inputFiles=... on the command line instead)
import os as _os
_listFile = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "2024_Hydjet_RecoDebug_GTV8.txt")
pbpb_mc_recodebug = [l.strip() for l in open(_listFile)] if _os.path.exists(_listFile) else []

pbpb_mc_reco_aod=[
    #~~~~~~~~~~~~~For 2023~~~~~~~~~~~~~~~
    'root://cmsxrootd.fnal.gov//store/mc/HINPbPbSpring23Reco/MinBias_Drum5F_5p36TeV_hydjet/AODSIM/NoPU_132X_mcRun3_2023_realistic_HI_v9-v2/2820000/00233439-15e0-406b-9363-da127ed271c0.root',
    'root://cmsxrootd.fnal.gov//store/mc/HINPbPbSpring23Reco/MinBias_Drum5F_5p36TeV_hydjet/AODSIM/NoPU_132X_mcRun3_2023_realistic_HI_v9-v2/2820000/02464949-cb4c-4b6e-a2e9-894f195ddec3.root'

    #~~~~~~~~~~~~~For 2024~~~~~~~~~~~~~~~
]

pbpb_mc_miniaod=[
    #~~~~~~~~~~~~~For 2023~~~~~~~~~~~~~~~
    #'root://cmsxrootd.fnal.gov//store/mc/HINPbPbSpring23MiniAOD/MinBias_Drum5F_5p36TeV_hydjet/MINIAODSIM/NoPU_132X_mcRun3_2023_realistic_HI_v9-v2/2820000/02288831-b588-4380-bac8-9910f24691bd.root',
    #'root://cmsxrootd.fnal.gov//store/mc/HINPbPbSpring23MiniAOD/MinBias_Drum5F_5p36TeV_hydjet/MINIAODSIM/NoPU_132X_mcRun3_2023_realistic_HI_v9-v2/2820000/03f9d7b4-d340-47e0-9cf7-ce3596e0fc8c.root',
    #'/store/mc/HINPbPbSpring23MiniAOD/MinBias_Drum5F_5p36TeV_hydjet/MINIAODSIM/NoPU_132X_mcRun3_2023_realistic_HI_v9-v2/2820000/0509b077-65db-403f-979f-dd689754cfbd.root'
    
    #~~~~~~~~~~~~~For 2024~~~~~~~~~~~~~~~
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_43.root',
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_55.root',
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_38.root',
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_44.root',
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_59.root',
    'file:/eos/cms/store/group/phys_heavyions/cbennett/crabSubmit_miniAOD_HYDJET_5360GeV_141X_updatedEra_updatedGT_2024-10-24/HYDJET_5360GeV_141X/HYDJET_5360GeV_miniAOD_141X_updatedEra_updatedGT/241024_214306/0000/HYDJET_5360GeV_miniAOD_16.root'
]

pbpb_data_reco_aod=[
    #~~~~~~~~~~~~~FED ZS~~~~~~~~~~~~~~~
    #'/eos/cmsfile:/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_FEDlikeZS/recoPbPbprime2mini_RAW2DIGI_L1Reco_RECO_182.root'
    #'file:/eos/cms/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_FEDlikeZS/recoPbPbprime2mini_RAW2DIGI_L1Reco_RECO_606.root' #2024 era
    #'file:/eos/cms/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_FEDlikeZS/recoPbPbprime2miniEra2023_RAW2DIGI_L1Reco_RECO_606.root' #2023 era
    #~~~~~~~~~~~~~Hybrid ZS~~~~~~~~~~~~~~~
    #'file:/eos/cms/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_hybridZS/recoPbPbprime2mini_RAW2DIGI_L1Reco_RECO_182.root'
    #'file:/eos/cms/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_hybridZS/recoPbPbprime2mini_RAW2DIGI_L1Reco_RECO_606.root' #2024 era
    'file:/eos/cms/store/group/phys_heavyions/yuchenc/for_Raghunath/HITrackerNZS_hybridZS/recoPbPbprime2miniEra2023_RAW2DIGI_L1Reco_RECO_606.root' #2023 era
    
]

pbpb_data_miniaod=[
    #~~~~~~~~~~~~~For 2023~~~~~~~~~~~~~~~
    #'root://cmsxrootd.fnal.gov//store/hidata/HIRun2023A/HIPhysicsRawPrime0/MINIAOD/PromptReco-v2/000/374/668/00000/06179488-b7e6-44f6-bec9-eb242a290ffd.root',
    #'root://cmsxrootd.fnal.gov//store/hidata/HIRun2023A/HIPhysicsRawPrime0/MINIAOD/PromptReco-v2/000/374/668/00000/119881a4-1fdd-4462-818e-55b3ab3962f5.root'

    #~~~~~~~~~~~~~For 2024~~~~~~~~~~~~~~~
    #'root://cmsxrootd.fnal.gov//store/backfill/1/hidata/Tier0_HIREPLAY_2024/HIEphemeralZeroBias0/MINIAOD/PromptReco-v17225534/000/374/951/00000/0ae924fb-550b-4a14-b046-e6879e025dbc.root'
    'file:/afs/cern.ch/user/w/wangj/public/forRun/recoPbPbprime2mini_RAW2DIGI_L1Reco_RECO_PAT_run388769_ls0001.root'  
]

pbpb_data_reco_aod_streams=[
    #'file:/eos/cms/store/t0streamer/Data/HIPhysicsMinimumBias0/000/325/174/run325174_ls0001_streamHIPhysicsMinimumBias0_StorageManager.dat'
]

pbpb_data_miniaod_streams=[
    'file:/eos/cms/store/t0streamer/Data/PhysicsHIMinimumBias0/000/387/853/run387853_ls0086_streamPhysicsHIMinimumBias0_StorageManager.dat'
]
