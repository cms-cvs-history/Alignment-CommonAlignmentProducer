# AlCaReco for track based alignment using J/Psi->MuMu events
import FWCore.ParameterSet.Config as cms

import HLTrigger.HLTfilters.hltHighLevel_cfi
ALCARECOTkAlJpsiMuMuHLT = HLTrigger.HLTfilters.hltHighLevel_cfi.hltHighLevel.clone(
    andOr = True, ## choose logical OR between Triggerbits
    eventSetupPathsKey = 'TkAlJpsiMuMu',
    throw = False # tolerate triggers stated above, but not available
    )

# DCS partitions
# "EBp","EBm","EEp","EEm","HBHEa","HBHEb","HBHEc","HF","HO","RPC"
# "DT0","DTp","DTm","CSCp","CSCm","CASTOR","TIBTID","TOB","TECp","TECm"
# "BPIX","FPIX","ESp","ESm"
import DPGAnalysis.Skims.skim_detstatus_cfi
ALCARECOTkAlJpsiMuMuDCSFilter = DPGAnalysis.Skims.skim_detstatus_cfi.dcsstatus.clone(
    DetectorType = cms.vstring('TIBTID','TOB','TECp','TECm','BPIX','FPIX'),
    ApplyFilter  = cms.bool(True),
    AndOr        = cms.bool(True),
    DebugOn      = cms.untracked.bool(False)
)

ALCARECOTkAlJpsiMuMuGoodMuonSelector = cms.EDFilter("MuonSelector",
    src = cms.InputTag("muons"),
    cut = cms.string('isGlobalMuon = 1'),
    filter = cms.bool(True)                                
)

import Alignment.CommonAlignmentProducer.AlignmentTrackSelector_cfi
ALCARECOTkAlJpsiMuMu = Alignment.CommonAlignmentProducer.AlignmentTrackSelector_cfi.AlignmentTrackSelector.clone()
ALCARECOTkAlJpsiMuMu.filter = True ##do not store empty events

ALCARECOTkAlJpsiMuMu.applyBasicCuts = True
ALCARECOTkAlJpsiMuMu.ptMin = 0.8 ##GeV
ALCARECOTkAlJpsiMuMu.etaMin = -3.5
ALCARECOTkAlJpsiMuMu.etaMax = 3.5
ALCARECOTkAlJpsiMuMu.nHitMin = 0

ALCARECOTkAlJpsiMuMu.GlobalSelector.muonSource = 'ALCARECOTkAlJpsiMuMuGoodMuonSelector'
ALCARECOTkAlJpsiMuMu.GlobalSelector.applyIsolationtest = False
ALCARECOTkAlJpsiMuMu.GlobalSelector.applyGlobalMuonFilter = True

ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.applyMassrangeFilter = True
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.minXMass = 2.7 ##GeV
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.maxXMass = 3.4 ##GeV
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.daughterMass = 0.105 ##GeV (Muons)
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.applyChargeFilter = False
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.charge = 0
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.applyAcoplanarityFilter = False
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.acoplanarDistance = 1 ##radian
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.PDGMass = 3.096
ALCARECOTkAlJpsiMuMu.TwoBodyDecaySelector.numberOfCandidates = 5

seqALCARECOTkAlJpsiMuMu = cms.Sequence(ALCARECOTkAlJpsiMuMuHLT+ALCARECOTkAlJpsiMuMuDCSFilter+ALCARECOTkAlJpsiMuMuGoodMuonSelector+ALCARECOTkAlJpsiMuMu)
