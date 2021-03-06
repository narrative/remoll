#include "G4Tubs.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4VSolid.hh"
#include "G4Material.hh"

#ifdef G4MULTITHREADED
#include "G4MTRunManager.hh"
#else
#include "G4RunManager.hh"
#endif

#include "G4GenericMessenger.hh"

#include "G4GeometryManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

#include "Randomize.hh"

#include "remollBeamTarget.hh"
#include "remollMultScatt.hh"

#include <math.h>

#define __MAX_MAT 100
#define Euler 0.5772157

// Initialize static geometry objects
G4VPhysicalVolume* remollBeamTarget::fTargetMother = 0;
std::vector <G4VPhysicalVolume *> remollBeamTarget::fTargetVolumes;

G4double remollBeamTarget::fLH2Length   = -1e9;
G4double remollBeamTarget::fZpos        = -1e9;
G4double remollBeamTarget::fLH2pos      = -1e9;
G4double remollBeamTarget::fTotalLength = 0.0;

remollBeamTarget::remollBeamTarget()
: fBeamE(gDefaultBeamE),fBeamCurr(gDefaultBeamCur),fBeamPol(gDefaultBeamPol),
  fOldRaster(true),fRasterX(5.0*mm),fRasterY(5.0*mm),
  fX0(0.0),fY0(0.0),fTh0(0.0),fPh0(0.0),
  fdTh(0.0),fdPh(0.0),fCorrTh(0.0),fCorrPh(0.0)
{
    UpdateInfo();

    // Create new multiple scattering
    fMS = new remollMultScatt();

    fEcut = 1e-6*MeV;

    fDefaultMat = new G4Material("Default_proton"   , 1., 1.0, 1e-19*g/mole);

    fAlreadyWarned = false;

    // Create generic messenger
    fMessenger = new G4GenericMessenger(this,"/remoll/","Remoll properties");
    fMessenger->DeclareMethodWithUnit("targlen","cm",&remollBeamTarget::SetTargetLen,"Target length").SetStates(G4State_Idle);
    fMessenger->DeclareMethodWithUnit("targpos","cm",&remollBeamTarget::SetTargetPos,"Target position").SetStates(G4State_Idle);

    fMessenger->DeclarePropertyWithUnit("beamcurr","microampere",fBeamCurr,"Beam current");
    fMessenger->DeclarePropertyWithUnit("beamene","GeV",fBeamE,"Beam energy");

    fMessenger->DeclareProperty("oldras",fOldRaster,"Old (no ang corln) or new (ang corl) raster");
    fMessenger->DeclarePropertyWithUnit("rasx","cm",fRasterX,"Square raster width x (horizontal)");
    fMessenger->DeclarePropertyWithUnit("rasy","cm",fRasterY,"Square raster width y (vertical)");

    fMessenger->DeclarePropertyWithUnit("beam_x0","cm",fX0,"beam initial position in x (horizontal)");
    fMessenger->DeclarePropertyWithUnit("beam_y0","cm",fY0,"beam initial position in y (vertical)");
    fMessenger->DeclarePropertyWithUnit("beam_ph0","deg",fPh0,"beam initial direction in x (horizontal)");
    fMessenger->DeclarePropertyWithUnit("beam_th0","deg",fTh0,"beam initial direction in y (vertical)");
    fMessenger->DeclarePropertyWithUnit("beam_corrph","deg",fCorrPh,"beam correlated angle (horizontal)");
    fMessenger->DeclarePropertyWithUnit("beam_corrth","deg",fCorrTh,"beam correlated angle (vertical)");
    fMessenger->DeclarePropertyWithUnit("beam_dph","deg",fdPh,"beam gaussian spread in x (horizontal)");
    fMessenger->DeclarePropertyWithUnit("beam_dth","deg",fdTh,"beam gaussian spread in y (vertical)");
}

remollBeamTarget::~remollBeamTarget()
{
    delete fMessenger;
    delete fMS;
}

G4double remollBeamTarget::GetEffLumin(){
    return fEffMatLen*fBeamCurr/(e_SI*coulomb);
}

void remollBeamTarget::UpdateInfo()
{
    fLH2Length   = -1e9;
    fZpos        = -1e9;
    fLH2pos      = -1e9;
    fTotalLength = 0.0;

    // Can't calculate anything without mother
    if( !fTargetMother ) return;
    fZpos = fTargetMother->GetFrameTranslation().z();

    for (std::vector<G4VPhysicalVolume *>::iterator
        it = fTargetVolumes.begin(); it != fTargetVolumes.end(); it++) {

        // Assume everything is non-nested tubes
	if( !dynamic_cast<G4Tubs *>( (*it)->GetLogicalVolume()->GetSolid() ) ){
	    G4cerr << "ERROR:  " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		":  Target volume not made of G4Tubs" << G4endl; 
	    exit(1);
	}

	if( (*it)->GetLogicalVolume()->GetMaterial()->GetName() == "LiquidHydrogen" ){

// TODO This doesn't work when moving to static
//	    if( fLH2Length >= 0.0 ){
//		G4cerr << "ERROR:  " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
//		    ":  Multiply defined LH2 volumes" << G4endl;
//		exit(1);
//	    }

	    fLH2Length = ((G4Tubs *) (*it)->GetLogicalVolume()->GetSolid())->GetZHalfLength()*2.0
		*(*it)->GetLogicalVolume()->GetMaterial()->GetDensity();

	    fLH2pos    = (*it)->GetFrameTranslation().z();

	    fTotalLength += ((G4Tubs *) (*it)->GetLogicalVolume()->GetSolid())->GetZHalfLength()*2.0
		*(*it)->GetLogicalVolume()->GetMaterial()->GetDensity();
	}
    }
}


void remollBeamTarget::SetTargetLen(G4double z)
{
    // Loop over target volumes
    for (std::vector<G4VPhysicalVolume *>::iterator
        it = fTargetVolumes.begin(); it != fTargetVolumes.end(); it++) {

        G4GeometryManager::GetInstance()->OpenGeometry((*it));

        // If liquid hydrogen tubs
        G4Material* material = (*it)->GetLogicalVolume()->GetMaterial();
	if (material->GetName() == "LiquidHydrogen")
	{
            G4VSolid* solid = (*it)->GetLogicalVolume()->GetSolid();
            G4Tubs* tubs = dynamic_cast<G4Tubs*>(solid);
            // Change the length of the target volume
            if (tubs) tubs->SetZHalfLength(z/2.0);

	} else {

	    G4cerr << "WARNING " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		": volume other than cryogen has been specified, but handling not implemented" << G4endl;
	    // Move position of all other volumes based on half length change

	    /*
	    G4ThreeVector pos = (*it)->GetFrameTranslation();

	    if( pos.z() < fLH2pos ){
		pos = pos + G4ThreeVector(0.0, 0.0, (fLH2Length-z)/2.0 );
	    } else {
		pos = pos - G4ThreeVector(0.0, 0.0, (fLH2Length-z)/2.0 );
	    }

	    (*it)->SetTranslation(pos);
	    */
	}
	G4GeometryManager::GetInstance()->CloseGeometry(true, false, (*it));
    }


    G4RunManager* runManager = G4RunManager::GetRunManager();
    runManager->GeometryHasBeenModified();

    UpdateInfo();
}

void remollBeamTarget::SetTargetPos(G4double z)
{
    //G4double zshift = z-(fZpos+fLH2pos);

    for (std::vector<G4VPhysicalVolume *>::iterator
        it = fTargetVolumes.begin(); it != fTargetVolumes.end(); it++ ) {

        G4GeometryManager::GetInstance()->OpenGeometry((*it));

        G4Material* material = (*it)->GetLogicalVolume()->GetMaterial();
	if (material->GetName() == "LiquidHydrogen") {
	    // Change the length of the target volume
	    (*it)->SetTranslation(G4ThreeVector(0.0, 0.0, z-fZpos));

	} else {

	    G4cerr << "WARNING " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		": volume other than cryogen has been specified, but handling not implemented" << G4endl;

	    // Move position of all other volumes based on half length change

	    /*
	    G4ThreeVector prespos = (*it)->GetFrameTranslation();

	    G4ThreeVector pos = prespos + G4ThreeVector(0.0, 0.0, zshift );

	    (*it)->SetTranslation(prespos);
	    */
	}
	G4GeometryManager::GetInstance()->CloseGeometry(true, false, (*it));
    }

    G4RunManager* runManager = G4RunManager::GetRunManager();
    runManager->GeometryHasBeenModified();

    UpdateInfo();
}


////////////////////////////////////////////////////////////////////////////////////////////
//  Sampling functions

remollVertex remollBeamTarget::SampleVertex(SampType_t samp)
{
    remollVertex thisvert;

    G4double rasx = G4RandFlat::shoot(fX0 - fRasterX/2.0, fX0 + fRasterX/2.0);
    G4double rasy = G4RandFlat::shoot(fY0 - fRasterY/2.0, fY0 + fRasterY/2.0);

    // Sample where along target weighted by density (which roughly corresponds to A
    // or the number of electrons, which is probably good enough for this

    // Figure out how far along the target we got
    switch( samp ){
	case kCryogen: 
	    fSampLen = fLH2Length;
	    break;

        case kWalls:
	    G4cerr << "ERROR" << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		": scattering from cell walls has been specified, but handling not implemented" << G4endl;
	    exit(1);
	    break;

	    /*
	case kWalls:
	    fSampLen = fTotalLength-fLH2Length;
	    break;
	    */
	case kFullTarget:
	    fSampLen = fTotalLength;
	    break;
    }

    G4double ztrav = G4RandFlat::shoot(0.0, fSampLen);


    G4bool isLH2;
    G4bool foundvol = false;
    G4Material *mat;
    G4double zinvol;

    G4double cumz   = 0.0;
    G4double radsum = 0.0;

    int      nmsmat = 0;
    double   msthick[__MAX_MAT];
    double   msA[__MAX_MAT];
    double   msZ[__MAX_MAT];

    // Figure out the material we are in and the radiation length we traversed
    std::vector<G4VPhysicalVolume *>::iterator it;
    for(it = fTargetVolumes.begin(); it != fTargetVolumes.end() && !foundvol; it++ ){
	mat = (*it)->GetLogicalVolume()->GetMaterial();
	if( mat->GetName() == "LiquidHydrogen" ) { 
	    isLH2 = true; 
	} else { 
	    isLH2 = false;
	    G4cerr << "WARNING " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		": volume not LH2 has been specified, but handling not implemented" << G4endl;

	} 

	G4double len = ((G4Tubs *) (*it)->GetLogicalVolume()->GetSolid())->GetZHalfLength()*2.0*mat->GetDensity();
	switch( samp ){
	    case kCryogen: 
		/*
		if( !isLH2 ){
		    radsum += len/mat->GetDensity()/mat->GetRadlen();
		} else {
		*/
		foundvol = true;
		zinvol = ztrav/mat->GetDensity();
		radsum += zinvol/mat->GetRadlen();
		//}
		break;

	    case kWalls:

		G4cerr << "WARNING " << __PRETTY_FUNCTION__ << " line " << __LINE__ <<
		": scattering from cell walls has been specified, but handling not implemented" << G4endl;
		/*
		if( isLH2 ){
		    radsum += len/mat->GetDensity()/mat->GetRadlen();
		} else {
		    if( ztrav - cumz < len ){
			foundvol = true;
			zinvol = (ztrav - cumz)/mat->GetDensity();
			radsum += zinvol/mat->GetRadlen();
		    } else {
			radsum += len/mat->GetDensity()/mat->GetRadlen();
			cumz   += len;
		    }
		}
		*/
		break;

	    case kFullTarget:
		if( ztrav - cumz < len ){
		    foundvol = true;
		    zinvol = (ztrav - cumz)/mat->GetDensity();
		    radsum += zinvol/mat->GetRadlen();
		} else {
		    radsum += len/mat->GetDensity()/mat->GetRadlen();
		    cumz   += len;
		}
		break;
	}

	if( mat->GetBaseMaterial() ){
	    G4cerr << __FILE__ << " " << __PRETTY_FUNCTION__ << ":  The material you're using isn't" <<
		" defined in a way we can use for multiple scattering calculations" << G4endl;
	    G4cerr << "Aborting" << G4endl; 
	    exit(1);
	}

	if( foundvol ){
	    // For our vertex
	    thisvert.fMaterial = mat;
	    thisvert.fRadLen   = radsum;

	    // For our own info
	    fTravLen = zinvol;
	    fRadLen = radsum;
	    fVer    = G4ThreeVector( rasx, rasy, 
		      zinvol - (*it)->GetFrameTranslation().z() + fZpos 
		       - ((G4Tubs *) (*it)->GetLogicalVolume()->GetSolid())->GetZHalfLength() );

	    G4double masssum = 0.0;
	    const G4int *atomvec = mat->GetAtomsVector();
	    const G4ElementVector *elvec = mat->GetElementVector();
	    const G4double *fracvec = mat->GetFractionVector();

	    for( unsigned int i = 0; i < elvec->size(); i++ ){
		// FIXME:  Not sure why AtomsVector would ever return null
		// but it does - SPR 2/5/13.  Just going to assume unit
		// weighting for now if that is the case
		if( atomvec ){
		    masssum += (*elvec)[i]->GetA()*atomvec[i];
		} else {
		    masssum += (*elvec)[i]->GetA();
		}
		msthick[nmsmat] = mat->GetDensity()*zinvol*fracvec[i];
		msA[nmsmat] = (*elvec)[i]->GetA()*mole/g;
		msZ[nmsmat] = (*elvec)[i]->GetZ();

		nmsmat++;
	    }

	    fEffMatLen = (fSampLen/len)* // Sample weighting
	      mat->GetDensity()*((G4Tubs *) (*it)->GetLogicalVolume()->GetSolid())->GetZHalfLength()*2.0*Avogadro/masssum; // material thickness
	} else {
	    const G4ElementVector *elvec = mat->GetElementVector();
	    const G4double *fracvec = mat->GetFractionVector();
	    for( unsigned int i = 0; i < elvec->size(); i++ ){

		msthick[nmsmat] = len*fracvec[i];
		msA[nmsmat] = (*elvec)[i]->GetA()*mole/g;
		msZ[nmsmat] = (*elvec)[i]->GetZ();
		nmsmat++;
	    }
	}
    }

    if( !foundvol ){
	if( !fAlreadyWarned ){
	    G4cerr << "WARNING: " << __PRETTY_FUNCTION__ << " line " << __LINE__ << ": Could not find sampling volume" << G4endl;
	    fAlreadyWarned = true;
	}

	thisvert.fMaterial = fDefaultMat;
	thisvert.fRadLen   = 0.0;
    }
    
    // Sample multiple scattering + angles
    G4double msth, msph;
    G4double bmth, bmph;

    if( nmsmat > 0 ){
	fMS->Init( fBeamE, nmsmat, msthick, msA, msZ );
	msth = fMS->GenerateMSPlane();
	msph = fMS->GenerateMSPlane();
    } else {
	msth = 0.0;
	msph = 0.0;
    }

    assert( !std::isnan(msth) && !std::isnan(msph) );

    if(fOldRaster){
      bmth = G4RandGauss::shoot(fTh0, fdTh);
      bmph = G4RandGauss::shoot(fPh0, fdPh);
      
      if( fRasterX > 0 ){ bmth += fCorrTh*(rasx-fX0)/fRasterX/2; }
      if( fRasterY > 0 ){ bmph += fCorrPh*(rasy-fY0)/fRasterY/2; }
      
      // Initial direction
      fDir = G4ThreeVector(0.0, 0.0, 1.0);
      
      fDir.rotateY( bmth); // Positive th pushes to positive X (around Y-axis)
      fDir.rotateX(-bmph); // Positive ph pushes to positive Y (around X-axis)
    } else{
      G4ThreeVector bmVec = G4ThreeVector(fVer.x(),fVer.y(),-1*(-8000.0*mm-fVer.z())); // in mm
      fDir = G4ThreeVector(bmVec.unit());
    }

    fDir.rotateY(msth);
    fDir.rotateX(msph);

    // Sample beam energy based on radiation
    // We do this so it doesn't affect the weighting
    //
    // This can be ignored and done in a generator by itself

    G4double  Ekin = fBeamE - electron_mass_c2;
    G4double  bt   = fRadLen*4.0/3.0;
    G4double  prob_sample, eloss, sample, env, value, ref;

    G4double prob = 1.- pow(fEcut/Ekin,bt) - bt/(bt+1.)*(1.- pow(fEcut/Ekin,bt+1.))
	+ 0.75*bt/(2.+bt)*(1.- pow(fEcut/Ekin,bt+2.));
    prob = prob/(1.- bt*Euler + bt*bt/2.*(Euler*Euler+pi*pi/6.)); /* Gamma function */

    prob_sample = G4UniformRand();

    if (prob_sample <= prob) {
	do {
	    sample = G4UniformRand();
	    eloss = fEcut*pow(Ekin/fEcut,sample);
	    env = 1./eloss;
	    value = 1./eloss*(1.-eloss/Ekin+0.75*pow(eloss/Ekin,2))*pow(eloss/Ekin,bt);

	    sample = G4UniformRand();
	    ref = value/env;
	} while (sample > ref);

	fSampE = fBeamE - eloss;
	assert( fSampE > electron_mass_c2 );
    } else {
	fSampE = fBeamE;
    }


    thisvert.fBeamE = fSampE;

    assert( fBeamE >= electron_mass_c2 );

    return thisvert;
}
