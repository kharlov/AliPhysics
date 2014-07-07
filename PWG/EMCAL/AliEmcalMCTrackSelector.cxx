//
// Class to select particles in MC events.
//
// Author: S. Aiola

#include "AliEmcalMCTrackSelector.h"

#include <TClonesArray.h>

#include "AliVEvent.h"
#include "AliMCEvent.h"
#include "AliAODMCParticle.h"
#include "AliMCParticle.h"
#include "AliStack.h"
#include "AliNamedArrayI.h"

#include "AliLog.h"

ClassImp(AliEmcalMCTrackSelector)

//________________________________________________________________________
AliEmcalMCTrackSelector::AliEmcalMCTrackSelector() : 
  AliAnalysisTaskSE("AliEmcalMCTrackSelector"),
  fParticlesOutName("MCParticlesSelected"),
  fOnlyPhysPrim(kTRUE),
  fRejectNK(kFALSE),
  fChargedMC(kFALSE),
  fOnlyHIJING(kFALSE),
  fEtaMax(1),
  fParticlesMapName(""),
  fInit(kFALSE),
  fParticlesIn(0),
  fParticlesOut(0),
  fParticlesMap(0),
  fEvent(0),
  fMC(0),
  fIsESD(kFALSE)
{
  // Constructor.
}

//________________________________________________________________________
AliEmcalMCTrackSelector::AliEmcalMCTrackSelector(const char *name) : 
  AliAnalysisTaskSE(name),
  fParticlesOutName("MCParticlesSelected"),
  fOnlyPhysPrim(kTRUE),
  fRejectNK(kFALSE),
  fChargedMC(kFALSE),
  fOnlyHIJING(kFALSE),
  fEtaMax(1),
  fParticlesMapName(""),
  fInit(kFALSE),
  fParticlesIn(0),
  fParticlesOut(0),
  fParticlesMap(0),
  fEvent(0),
  fMC(0),
  fIsESD(kFALSE)
{
  // Constructor.
}

//________________________________________________________________________
AliEmcalMCTrackSelector::~AliEmcalMCTrackSelector()
{
  // Destructor.
}

//________________________________________________________________________
void AliEmcalMCTrackSelector::UserCreateOutputObjects()
{
  // Create my user objects.
}

//________________________________________________________________________
void AliEmcalMCTrackSelector::UserExec(Option_t *) 
{
  // Main loop, called for each event.

  if (!fInit) {
    fEvent = InputEvent();
    if (!fEvent) {
      AliError("Could not retrieve event! Returning");
      return;
    }

    if (fEvent->InheritsFrom("AliESDEvent")) fIsESD = kTRUE;
    else fIsESD = kFALSE;

    TObject *obj = fEvent->FindListObject(fParticlesOutName);
    if (obj) { // the output array is already present in the array!
      AliFatal(Form("The output array %s is already present in the array!", fParticlesOutName.Data()));
      return;
    }
    else {  // copy the array from the standard ESD/AOD collections, and filter if requested      

      fParticlesOut = new TClonesArray("AliAODMCParticle");  // the output will always be of AliAODMCParticle, regardless of the input
      fParticlesOut->SetName(fParticlesOutName);
      fEvent->AddObject(fParticlesOut);

      fParticlesMapName = fParticlesOutName;
      fParticlesMapName += "_Map";
      fParticlesMap = new AliNamedArrayI(fParticlesMapName, 99999);
      
      if (!fIsESD) {
	fParticlesIn = static_cast<TClonesArray*>(InputEvent()->FindListObject(AliAODMCParticle::StdBranchName()));
	if (!fParticlesIn) {
	  AliFatal("Could not retrieve AOD MC particles! Returning");
	  return;
	}
	TClass *cl = fParticlesIn->GetClass();
	if (!cl->GetBaseClass("AliAODMCParticle")) {
	  AliFatal(Form("%s: Collection %s does not contain AliAODMCParticle!", GetName(), AliAODMCParticle::StdBranchName())); 
	  fParticlesIn = 0;
	  return;
	}
      }
    }

    fMC = MCEvent();
    if (!fMC) {
      AliFatal("Could not retrieve MC event! Returning");
      return;
    }

    fInit = kTRUE;
  }

  if (fIsESD) ConvertMCParticles();
  else CopyMCParticles();
}

//________________________________________________________________________
void AliEmcalMCTrackSelector::ConvertMCParticles() 
{
  // Convert MC particles in MC AOD particles.

  // clear container (normally a null operation as the event should clean it already)
  fParticlesOut->Delete();

  // clear particles map
  fParticlesMap->Clear();

  const Int_t Nparticles = fMC->GetNumberOfTracks();
  const Int_t nprim = fMC->GetNumberOfPrimaries();

  if (fParticlesMap->GetSize() <= Nparticles) fParticlesMap->Set(Nparticles*2);

  // loop over particles
  for (Int_t iPart = 0, nacc = 0; iPart < Nparticles; iPart++) {

    AliMCParticle* part = static_cast<AliMCParticle*>(fMC->GetTrack(iPart));

    if (!part) continue;

    if (fEtaMax > 0. && TMath::Abs(part->Eta()) > fEtaMax) continue;
    
    if (fRejectNK && (part->PdgCode() == 130 || part->PdgCode() == 2112)) continue;
    
    if (fChargedMC && part->Charge() == 0) continue;

    Int_t genIndex = part->GetGeneratorIndex();
    if (fOnlyHIJING && genIndex != 0) continue;

    Bool_t isPhysPrim = fMC->IsPhysicalPrimary(iPart);
    if (fOnlyPhysPrim && !isPhysPrim) continue;

    fParticlesMap->AddAt(nacc, iPart);

    Int_t flag = 0;
    if (iPart < nprim) flag |= AliAODMCParticle::kPrimary;
    if (isPhysPrim) flag |= AliAODMCParticle::kPhysicalPrim;
    if (fMC->IsSecondaryFromWeakDecay(iPart)) flag |= AliAODMCParticle::kSecondaryFromWeakDecay;
    if (fMC->IsSecondaryFromMaterial(iPart)) flag |= AliAODMCParticle::kSecondaryFromMaterial;
    
    AliAODMCParticle *aodPart = new ((*fParticlesOut)[nacc]) AliAODMCParticle(part, iPart, flag);
    aodPart->SetGeneratorIndex(genIndex);    
    aodPart->SetStatus(part->Particle()->GetStatusCode());
    aodPart->SetMCProcessCode(part->Particle()->GetUniqueID());

    nacc++;
  }
}

//________________________________________________________________________
void AliEmcalMCTrackSelector::CopyMCParticles() 
{
  // Convert standard MC AOD particles in a new array, and filter if requested.

  if (!fParticlesIn) return;

  // clear container (normally a null operation as the event should clean it already)
  fParticlesOut->Delete();

  // clear particles map
  fParticlesMap->Clear();

  const Int_t Nparticles = fParticlesIn->GetEntriesFast();

  // loop over particles
  for (Int_t iPart = 0, nacc = 0; iPart < Nparticles; iPart++) {

    AliAODMCParticle* part = static_cast<AliAODMCParticle*>(fParticlesIn->At(iPart));

    if (!part) continue;

    if (fEtaMax > 0. && TMath::Abs(part->Eta()) > fEtaMax) continue;
    
    if (fRejectNK && (part->PdgCode() == 130 || part->PdgCode() == 2112)) continue;
    
    if (fChargedMC && part->Charge() == 0) continue;

    if (fOnlyHIJING && (part->GetGeneratorIndex() != 0)) continue;

    if (fOnlyPhysPrim && !part->IsPhysicalPrimary()) continue;

    fParticlesMap->AddAt(nacc, iPart);

    new ((*fParticlesOut)[nacc]) AliAODMCParticle(*part);	  

    nacc++;
  }
}
