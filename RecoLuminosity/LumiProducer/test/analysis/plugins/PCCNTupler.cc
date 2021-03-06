// ----------------------------------------------------------------------
// PCCNTupler
// ---------

#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>

#include "PCCNTupler.h"

#include "CondFormats/Alignment/interface/Definitions.h"
#include "CondFormats/RunInfo/interface/RunInfo.h"

#include "DataFormats/SiPixelDetId/interface/PixelBarrelName.h"
#include "DataFormats/SiPixelDetId/interface/PixelEndcapName.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"

#include "DataFormats/SiPixelCluster/interface/SiPixelCluster.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
DEFINE_FWK_MODULE(PCCNTupler);


#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>

using namespace std;
using namespace edm;
using namespace reco;

// ----------------------------------------------------------------------
PCCNTupler::PCCNTupler(edm::ParameterSet const& iConfig): 
    fVerbose(iConfig.getUntrackedParameter<int>("verbose", 0)),
    fPrimaryVertexCollectionLabel(iConfig.getUntrackedParameter<InputTag>("vertexCollLabel", edm::InputTag("offlinePrimaryVertices"))), 
    fPixelClusterLabel(iConfig.getUntrackedParameter<InputTag>("pixelClusterLabel", edm::InputTag("siPixelClusters"))), 
    fPileUpInfoLabel(edm::InputTag("addPileupInfo")),
    saveType(iConfig.getUntrackedParameter<string>("saveType")),
    sampleType(iConfig.getUntrackedParameter<string>("sampleType")),
    includeVertexInformation(iConfig.getUntrackedParameter<bool>("includeVertexInformation",1)),
    includePixels(iConfig.getUntrackedParameter<bool>("includePixels",1))
{
    cout << "----------------------------------------------------------------------" << endl;
    cout << "--- PCCNTupler constructor" << endl;

    nPrint = 0;
    edm::Service<TFileService> fs;


    tree = fs->make<TTree>("tree","Pixel Cluster Counters");
    tree->Branch("run",&runNo,"run/I");
    tree->Branch("LS",&LSNo,"LS/I");
    tree->Branch("LN",&LNNo,"LN/I");
    tree->Branch("timeStamp_begin",&timeStamp_begin,"timeStamp_begin/i");
    tree->Branch("timeStamp_end",&timeStamp_end,"timeStamp_end/i");
    if (saveType=="Event"){
        tree->Branch("event",&eventNo,"eventNo/i");
        tree->Branch("orbit",&orbitNo,"orbitNo/I");
    }

    pileup = fs->make<TH1F>("pileup","pileup",100,0,100);
    if(includeVertexInformation){
        tree->Branch("nGoodVtx","map<int,int>",&nGoodVtx); 
        recoVtxToken=consumes<reco::VertexCollection>(fPrimaryVertexCollectionLabel);
        if (saveType=="Event"){
            tree->Branch("nVtx",&nVtx,"nVtx/I");
            tree->Branch("goodVertex",&goodVertex,"goodVertex[nVtx]/O");
            tree->Branch("vtx_nTrk",&vtx_nTrk,"vtx_nTrk[nVtx]/I");
            tree->Branch("vtx_ndof",&vtx_ndof,"vtx_ndof[nVtx]/I");
            tree->Branch("vtx_x",&vtx_x,"vtx_x[nVtx]/F");
            tree->Branch("vtx_y",&vtx_y,"vtx_y[nVtx]/F");
            tree->Branch("vtx_z",&vtx_z,"vtx_z[nVtx]/F");
            tree->Branch("vtx_chi2",&vtx_chi2,"vtx_chi2[nVtx]/F");
            tree->Branch("vtx_normchi2",&vtx_normchi2,"vtx_normchi2[nVtx]/F");
            tree->Branch("vtx_isValid",&vtx_isValid,"vtx_isValid[nVtx]/O");
            tree->Branch("vtx_isFake",&vtx_isFake,"vtx_isFake[nVtx]/O");
        }

    }

    if(includePixels){
        tree->Branch("BXNo","map<int,int>",&BXNo);
        tree->Branch("nPixelClusters","map<std::pair<int,int>,int>",&nPixelClusters);
        tree->Branch("nClusters",     "map<std::pair<int,int>,int>",&nClusters);
        //tree->Branch("nPixelClusters","map<int,int>",&nPixelClusters);
        //tree->Branch("nClusters","map<int,int>",&nClusters);
        tree->Branch("layers","map<int,int>",&layers);
        // dead modules from Run 1
        //deadModules[0] = 302125076; 
        //deadModules[1] = 302125060;
        //deadModules[2] = 302197516;
        //deadModules[3] = 344019460;
        //deadModules[4] = 344019464;
        //deadModules[5] = 344019468;  
        pixelToken=consumes<edmNew::DetSetVector<SiPixelCluster> >(fPixelClusterLabel);
    }

    if(sampleType=="MC"){
        pileUpToken=consumes<std::vector< PileupSummaryInfo> >(fPileUpInfoLabel);
        tree->Branch("nPU",&nPU,"nPU/I");
    }
}

// ----------------------------------------------------------------------
PCCNTupler::~PCCNTupler() { }  

// ----------------------------------------------------------------------
void PCCNTupler::endJob() { 
    cout << "==>PCCNTupler> Succesfully gracefully ended job" << endl;
}

// ----------------------------------------------------------------------
void PCCNTupler::beginJob() {

}


void PCCNTupler::beginLuminosityBlock(edm::LuminosityBlock const& lumi, edm::EventSetup const& isetup){
    firstEvent = true;
}

void PCCNTupler::endLuminosityBlock(edm::LuminosityBlock const& lumi, edm::EventSetup const& isetup){
    tree->Fill();
}


// ----------------------------------------------------------------------
void PCCNTupler::analyze(const edm::Event& iEvent, 
            const edm::EventSetup& iSetup)  {

    using namespace edm;
    using reco::VertexCollection;

    eventCounter++;

    saveAndReset=false;
    sameEvent = (eventNo==(int)iEvent.id().event());
    sameLumiNib = true; // FIXME where is this info?
    sameLumiSect = (LSNo==(int)iEvent.getLuminosityBlock().luminosityBlock());
    
    // When arriving at the new LS, LN or event the tree 
    // must be filled and branches must be reset.
    // The final entry is saved in the deconstructor.
    saveAndReset = (saveType=="LumiSect" && !sameLumiSect)
                || (saveType=="LumiNib" && !sameLumiNib)
                || (saveType=="Event" && !sameEvent);


    if(   !saveAndReset && !sameLumiSect
       && !sameLumiNib  && !sameEvent) {
        std::cout<<"Diff LS, LN and Event, but not saving/resetting..."<<std::endl;
    }

    if(saveAndReset){
        SaveAndReset();
    }

    if(sampleType=="MC"){
        edm::Handle<std::vector< PileupSummaryInfo> > pileUpInfo;
        iEvent.getByToken(pileUpToken, pileUpInfo);
        std::vector<PileupSummaryInfo>::const_iterator PVI;
        for(PVI = pileUpInfo->begin(); PVI != pileUpInfo->end(); ++PVI) {
            int pu_bunchcrossing = PVI->getBunchCrossing();
            //std::cout<<"pu_bunchcrossing getPU_NumInteractions getTrueNumInteractions "<<pu_bunchcrossing<<" "<<PVI->getPU_NumInteractions()<<" "<<PVI->getTrueNumInteractions()<<std::endl;
            if(pu_bunchcrossing == 0) {
                nPU=PVI->getPU_NumInteractions();
                pileup->Fill(nPU);
            }
        }
    }

    // Get the Run, Lumi Section, and Event numbers, etc.
    runNo   = iEvent.id().run();
    LSNo    = iEvent.getLuminosityBlock().luminosityBlock();
    //LNNo    = -99; // FIXME need the luminibble
    eventNo = iEvent.id().event();
    bxNo    = iEvent.bunchCrossing();
    timeStamp_local = iEvent.time().unixTime();
    if(timeStamp_end  <timeStamp_local) timeStamp_end   =timeStamp_local;
    if(timeStamp_begin>timeStamp_local) timeStamp_begin =timeStamp_local;
    orbitNo = iEvent.orbitNumber();
    //LNNo    = ((int) (orbitNo/pow(2,12)) % 64);
    //int LNNo2    = iEvent.nibble;
    LNNo    = ((int) (orbitNo >> 12) % 64); // FIXME need the luminibble
   
    bxModKey.first=bxNo;
    bxModKey.second=-1;
   
    if((BXNo.count(bxNo)==0||nGoodVtx.count(bxNo)==0) && !(BXNo.count(bxNo)==0&&nGoodVtx.count(bxNo)==0)){
        std::cout<<"BXNo and nGoodVtx should have the same keys but DO NOT!!!"<<std::endl;
    }
    
    if(BXNo.count(bxNo)==0){
        BXNo[bxNo]=0;
    }

    if(nGoodVtx.count(bxNo)==0){
        nGoodVtx[bxNo]=0;
    }

    BXNo[bxNo]=BXNo[bxNo]+1;
    // add the vertex information

    if(includeVertexInformation){
        edm::Handle<reco::VertexCollection> recVtxs;
        iEvent.getByToken(recoVtxToken,recVtxs);
        
   
        if(recVtxs.isValid()){
            nVtx=recVtxs->size();
            int ivtx=0;
            for(reco::VertexCollection::const_iterator v=recVtxs->begin(); v!=recVtxs->end(); ++v){
                goodVertex[ivtx] = false;
                vtx_nTrk[ivtx] = v->tracksSize();
                vtx_ndof[ivtx] = (int)v->ndof();
                vtx_x[ivtx] = v->x();
                vtx_y[ivtx] = v->y();
                vtx_z[ivtx] = v->z();
                vtx_chi2[ivtx] = v->chi2();
                vtx_normchi2[ivtx] = v->normalizedChi2();
                vtx_isValid[ivtx] = v->isValid();
                vtx_isFake[ivtx] = v->isFake();
                if(vtx_ndof[ivtx] > 4 && vtx_isValid[ivtx] && (vtx_isFake[ivtx] == 0)){
                    if(vtx_nTrk[ivtx] > 0){
                        nGoodVtx[bxNo]=nGoodVtx[bxNo]+1;
                        goodVertex[ivtx] = true;
                    }
                }
                ivtx++;
            }
        }
    }


    // -- Does this belong into beginJob()?
    ESHandle<TrackerGeometry> TG;
    iSetup.get<TrackerDigiGeometryRecord>().get(TG);

    // -- FED
    edm::eventsetup::EventSetupRecordKey recordKey(edm::eventsetup::EventSetupRecordKey::TypeTag::findType("RunInfoRcd"));
    if (recordKey.type() == edm::eventsetup::EventSetupRecordKey::TypeTag()) {
        //record not found
        cout << "Record \"RunInfoRcd" << "\" does not exist " << endl;
        fFED1 = 0; 
        fFED2 = (0x1 << 12) ; 
    } else {
        edm::ESHandle<RunInfo> runInfoHandle;
        iSetup.get<RunInfoRcd>().get(runInfoHandle);
    }
    // -- Pixel cluster
    if(includePixels){
        edm::Handle< edmNew::DetSetVector<SiPixelCluster> > hClusterColl;
        iEvent.getByToken(pixelToken,hClusterColl);
        
        const edmNew::DetSetVector<SiPixelCluster>& clustColl = *(hClusterColl.product());
        
        
        // ----------------------------------------------------------------------
        // -- Clusters without tracks

        for (TrackerGeometry::DetContainer::const_iterator it = TG->dets().begin(); it != TG->dets().end(); it++){
            //if (dynamic_cast<PixelGeomDetUnit*>((*it)) != 0){ 
                DetId detId = (*it)->geographicalId();

                bxModKey.second=detId();

                // -- clusters on this det
                edmNew::DetSetVector<SiPixelCluster>::const_iterator isearch = clustColl.find(detId);
                if (isearch != clustColl.end()) {  // Not an empty iterator
                    edmNew::DetSet<SiPixelCluster>::const_iterator  di;
                    for (di = isearch->begin(); di != isearch->end(); ++di) {
                        if(nPixelClusters.count(bxModKey)==0){
                            nPixelClusters[bxModKey]=0;
                        }
                        nPixelClusters[bxModKey] = nPixelClusters[bxModKey]+1;
                    }

                    int nCluster = isearch->size();
                    if(nClusters.count(bxModKey)==0){
                        nClusters[bxModKey]=0;
                    }
                    nClusters[bxModKey] += nCluster;

                    if (detId.subdetId() == PixelSubdetector::PixelBarrel) {
                        PixelBarrelName detName = PixelBarrelName(detId);
                        int layer = detName.layerName();
                        if(layers.count(detId())==0){
                            layers[detId()]=layer;
                        }
                    } else {
                        assert(detId.subdetId() == PixelSubdetector::PixelEndcap);
                        PixelEndcapName detName = PixelEndcapName(detId);
                        int disk = detName.diskName();
                        if(layers.count(detId())==0){
                            layers[detId()]=disk+3;
                        }
                    }
                }
            //}
        }
    }
} 


void PCCNTupler::Reset() {
    nVtx = 0;
    nPixelClusters.clear();
    nClusters.clear();
    layers.clear();
    BXNo.clear();
    nGoodVtx.clear();
    eventCounter=0;
    timeStamp_end=0;
    timeStamp_begin=-1;
}

void PCCNTupler::SaveAndReset() {
    if(!firstEvent) tree->Fill();
    Reset();
    firstEvent=false;
}
