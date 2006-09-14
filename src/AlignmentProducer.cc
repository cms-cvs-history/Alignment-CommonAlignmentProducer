
#include "Alignment/CommonAlignmentProducer/interface/AlignmentProducer.h"

// System include files
#include <memory>

// Framework
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"

// Conditions database
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"

// Geometry
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"
#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeomBuilderFromGeometricDet.h"

#include "Geometry/TrackingGeometryAligner/interface/GeometryAligner.h"
#include "CondFormats/Alignment/interface/Alignments.h"
#include "Alignment/TrackerAlignment/interface/MisalignmentScenarioBuilder.h"


#include "Alignment/CSA06AlignmentAlgorithm/interface/CSA06AlignmentAlgorithm.h"

#include "Alignment/CommonAlignmentParametrization/interface/AlignmentTransformations.h"

using namespace std;

//_____________________________________________________________________________
AlignmentProducer::AlignmentProducer(const edm::ParameterSet& iConfig) :
  theMaxLoops( iConfig.getUntrackedParameter<unsigned int>("maxLoops",0) ),
  stParameterSelector(iConfig.getParameter<std::string>("parameterSelector") ),
  stAlignableSelector(iConfig.getParameter<std::string>("alignableSelector") ),
  stAlgorithm(iConfig.getParameter<std::string>("algorithm")),
  stNFixAlignables(iConfig.getParameter<int>("nFixAlignables") ),
  stRandomShift(iConfig.getParameter<double>("randomShift")),
  stRandomRotation(iConfig.getParameter<double>("randomRotation")),
  doMisalignmentScenario(iConfig.getParameter<bool>("doMisalignmentScenario")),
  saveToDB(iConfig.getParameter<bool>("saveToDB"))
{

  edm::LogWarning("Alignment") << "[AlignmentProducer] Constructor called ...";

  theParameterSet=iConfig;

  nevent=0;

  // Tell the framework what data is being produced
  setWhatProduced(this);

  if (stAlgorithm=="CSA06AlignmentAlgorithm") {
    // get cfg for alignment algorithm
    edm::ParameterSet csa06Config 
      = iConfig.getParameter<edm::ParameterSet>( "CSA06AlignmentAlgorithm" );
    // create alignment algorithm
    theAlignmentAlgo = new CSA06AlignmentAlgorithm(csa06Config);
  }
  else
    throw cms::Exception("BadConfig") << "No valid alignment algorithm: " << stAlgorithm;

}


//_____________________________________________________________________________
// Close files, etc.

AlignmentProducer::~AlignmentProducer()
{

}


//_____________________________________________________________________________
// Produce tracker geometry

AlignmentProducer::ReturnType 
AlignmentProducer::produce( const TrackerDigiGeometryRecord& iRecord )
{

  edm::LogWarning("Alignment") << "[AlignmentProducer] At producer method ...";

  return theTracker;
  
}


//_____________________________________________________________________________
// Initialize algorithm

void AlignmentProducer::beginOfJob( const edm::EventSetup& iSetup )
{

  edm::LogWarning("Alignment") << "[AlignmentProducer] At begin job ...";

  nevent=0;

  // Create the tracker geometry from ideal geometry (first time only)
  edm::ESHandle<DDCompactView> cpv;
  edm::ESHandle<GeometricDet> gD;
  iSetup.get<IdealGeometryRecord>().get( cpv );
  iSetup.get<IdealGeometryRecord>().get( gD );
  TrackerGeomBuilderFromGeometricDet trackerBuilder;
  theTracker  = boost::shared_ptr<TrackerGeometry>( trackerBuilder.build(&(*cpv),&(*gD)) );
  
  // create alignable tracker
  theAlignableTracker = new AlignableTracker( &(*gD), &(*theTracker) );

  // create alignment parameter builder
  edm::LogWarning("Alignment") <<"[AlignmentProducer] Creating AlignmentParameterBuilder";
  theAlignmentParameterBuilder = new AlignmentParameterBuilder(theAlignableTracker);

  // determine which parameters are fixed/aligned (local coordinates)
  static const unsigned int npar=6;
  std::vector<bool> sel(npar,false);
    edm::LogWarning("Alignment") <<"[AlignmentProducer] ParameterSelector: >" <<stParameterSelector<<"<"; 
  if (stParameterSelector.length()!=npar) {
    edm::LogError("Alignment") <<"[AlignmentProducer] ERROR: ParameterSelector vector has wrong size!";
    exit(1);
  }
  else {
    // shifts
    if (stParameterSelector.substr(0,1)=="1") 
      sel[RigidBodyAlignmentParameters::dx]=true;
    if (stParameterSelector.substr(1,1)=="1") 
      sel[RigidBodyAlignmentParameters::dy]=true;
    if (stParameterSelector.substr(2,1)=="1") 
      sel[RigidBodyAlignmentParameters::dz]=true;
    // rotations
    if (stParameterSelector.substr(3,1)=="1") 
      sel[RigidBodyAlignmentParameters::dalpha]=true;
    if (stParameterSelector.substr(4,1)=="1") 
      sel[RigidBodyAlignmentParameters::dbeta]=true;
    if (stParameterSelector.substr(5,1)=="1") 
      sel[RigidBodyAlignmentParameters::dgamma]=true;

    for (unsigned int i=0; i<npar; i++) {
      if (sel[i]==true) edm::LogWarning("Alignment") <<"[AlignmentProducer] Parameter "<< i <<" active.";
    }
  }

  // select alignables 
  edm::LogWarning("Alignment") <<"[AlignmentProducer] select alignables ...";
  theAlignmentParameterBuilder->addSelection(stAlignableSelector,sel);

  // fix alignables
  if (stNFixAlignables>0) theAlignmentParameterBuilder->fixAlignables(stNFixAlignables);

  // get alignables
  Alignables theAlignables = theAlignmentParameterBuilder->alignables();
  edm::LogWarning("Alignment") <<"[AlignmentProducer] got alignables: "<<theAlignables.size();

  // create AlignmentParameterStore 
  theAlignmentParameterStore = new AlignmentParameterStore(theAlignables);
  edm::LogWarning("Alignment") <<"[AlignmentProducer] AlignmentParameterStore created!";

  // Create misalignment scenario, apply to geometry
  if (doMisalignmentScenario) {
    edm::LogWarning("Alignment") <<"[AlignmentProducer] applying misalignment scenario ...";
    edm::ParameterSet scenarioConfig 
      = theParameterSet.getParameter<edm::ParameterSet>( "MisalignmentScenario" );
    MisalignmentScenarioBuilder scenarioBuilder( theAlignableTracker );
    scenarioBuilder.applyScenario( scenarioConfig );
  }
  else edm::LogWarning("Alignment") <<"[AlignmentProducer] NOT applying misalignment scenario!";

  // apply simple misalignment
  simpleMisalignment(theAlignables,sel,stRandomShift,stRandomRotation,true);
  edm::LogWarning("Alignment") <<"[AlignmentProducer] simple misalignment done!";

  // initialize alignment algorithm
  theAlignmentAlgo->initialize( iSetup, theAlignableTracker,
    theAlignmentParameterStore );
  edm::LogWarning("Alignment") <<"[AlignmentProducer] after call init algo...";

  // actually execute all misalignments
  edm::LogWarning("Alignment") <<"[AlignmentProducer] Now physically apply alignments to tracker geometry...";
  GeometryAligner aligner;
  std::auto_ptr<Alignments> alignments(theAlignableTracker->alignments());
  std::auto_ptr<AlignmentErrors> alignmentErrors(theAlignableTracker->alignmentErrors());
  aligner.applyAlignments<TrackerGeometry>( &(*theTracker),&(*alignments),&(*alignmentErrors));

}

//_____________________________________________________________________________
// Terminate algorithm

void AlignmentProducer::endOfJob()
{

  edm::LogWarning("Alignment") << "[AlignmentProducer] At end of job: terminating algorithm";
  theAlignmentAlgo->terminate();

  // write alignments to database

  if (saveToDB) {
    edm::LogWarning("Alignment") << "[AlignmentProducer] Writing Alignments to DB...";
    // Call service
    edm::Service<cond::service::PoolDBOutputService> poolDbService;
    if( !poolDbService.isAvailable() ) // Die if not available
	throw cms::Exception("NotAvailable") << "PoolDBOutputService not available";
    // get alignments+errors
    Alignments* alignments = theAlignableTracker->alignments();
    AlignmentErrors* alignmentErrors = theAlignableTracker->alignmentErrors();
    // Define callback tokens for the two records
    size_t alignmentsToken = poolDbService->callbackToken("Alignments");
    size_t alignmentErrorsToken = poolDbService->callbackToken("AlignmentErrors");
    // Store
    poolDbService->newValidityForNewPayload<Alignments>(alignments, 
      poolDbService->endOfTime(), alignmentsToken);
    poolDbService->newValidityForNewPayload<AlignmentErrors>(alignmentErrors, 
      poolDbService->endOfTime(), alignmentErrorsToken);
  }


}

//_____________________________________________________________________________
// Called at beginning of loop
void AlignmentProducer::startingNewLoop(unsigned int iLoop )
{

  edm::LogWarning("Alignment") << "[AlignmentProducer] Starting loop number " << iLoop;

}


//_____________________________________________________________________________
// Called at end of loop

edm::EDLooper::Status 
AlignmentProducer::endOfLoop(const edm::EventSetup& iSetup, unsigned int iLoop)
{
  edm::LogWarning("Alignment") << "[AlignmentProducer] Ending loop " << iLoop;

  if ( iLoop == theMaxLoops-1 || iLoop >= theMaxLoops ) return kStop;
  else return kContinue;
}

//_____________________________________________________________________________
// Called at each event

edm::EDLooper::Status 
AlignmentProducer::duringLoop( const edm::Event& event, 
  const edm::EventSetup& setup )
{
  nevent++;

  edm::LogInfo("Alignment") << "[AlignmentProducer] New Event --------------------------------------------------------------";

  if ((nevent<100 && nevent%10==0) 
      ||(nevent<1000 && nevent%100==0) 
      ||(nevent<10000 && nevent%100==0) 
      ||(nevent<100000 && nevent%1000==0) 
      ||(nevent<10000000 && nevent%1000==0))
    edm::LogWarning("Alignment") << "[AlignmentProducer] Events processed: "<<nevent;

  // Run the refitter algorithm
  AlgoProductCollection m_algoResults = theAlignmentAlgo->refitTracks( event, setup );


  edm::LogInfo("Alignment") << "[AlignmentProducer] call algorithm for #Tracks: " << m_algoResults.size();
  // Run the alignment algorithm
  theAlignmentAlgo->run(  setup, m_algoResults );

  return kContinue;
}

// ----------------------------------------------------------------------------

void AlignmentProducer::
simpleMisalignment(Alignables alivec, std::vector<bool> sel, 
		   float shift, float rot, bool local)
{
  bool first=true;

  if (shift>0 || rot >0) {
    edm::LogWarning("Alignment") <<"[simpleMisalignment] Now doing misalignment ...";
    edm::LogWarning("Alignment") <<"[simpleMisalignment] adding random flat shift of max size " << shift;
    edm::LogWarning("Alignment") <<"[simpleMisalignment] adding random flat rot   of max size " << rot;

    for (vector<Alignable*>::const_iterator it=alivec.begin(); 
     it!=alivec.end(); it++) {
     Alignable* ali=(*it);
     vector<bool> mysel;
     // either
     mysel=ali->alignmentParameters()->selector();
     // or
     //mysel=sel;

     if (abs(shift)>0.00001) {
      AlgebraicVector s(3);
      s[0]=0; s[1]=0; s[2]=0;  
      if (mysel[RigidBodyAlignmentParameters::dx]) {
        s[0]=shift*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning x";
      }
      if (mysel[RigidBodyAlignmentParameters::dy]) {
        s[1]=shift*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning y";
      }
      if (mysel[RigidBodyAlignmentParameters::dz]) {
        s[2]=shift*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning z";
      }

      GlobalVector globalshift;
      if (local) {
        globalshift = ali->surface().toGlobal(Local3DVector(s[0],s[1],s[2]));
      }
      else {
        globalshift = Global3DVector(s[0],s[1],s[2]);
      }
      //edm::LogInfo("Alignment") <<"misalignment shift: " << globalshift;
      ali->move(globalshift);

      //AlignmentPositionError ape(dx,dy,dz);
      //ali->addAlignmentPositionError(ape);
      if (first) edm::LogWarning("Alignment") <<"yes adding shift!";
    }

    if (abs(rot)>0.00001) {
      AlgebraicVector r(3);
      r[0]=0; r[1]=0; r[2]=0;
      if (mysel[RigidBodyAlignmentParameters::dalpha]) {
        r[0]=rot*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning alpha";
      }
      if (mysel[RigidBodyAlignmentParameters::dbeta]) {
        r[1]=rot*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning beta ";
      }
      if (mysel[RigidBodyAlignmentParameters::dgamma]) {
        r[2]=rot*double(random()%1000-500)/500.;
	if (first) edm::LogWarning("Alignment") <<"Misaligning gamma";
      }
      AlignmentTransformations TkAT;
      Surface::RotationType mrot = TkAT.rotationType(TkAT.rotMatrix3(r));
      if (local) ali->rotateInLocalFrame(mrot);
      else ali->rotateInGlobalFrame(mrot);
      //edm::LogInfo("Alignment") <<"misalignment rot: " << mrot;

      //ali->addAlignmentPositionErrorFromRotation(mrot);
      if (first) edm::LogWarning("Alignment") <<"yes adding rot!\n";

    }

    first=false;
   }
  }
  else edm::LogWarning("Alignment") <<"[simpleMisalignment] No Misalignment applied!";


}
