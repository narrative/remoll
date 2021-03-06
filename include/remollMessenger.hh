#ifndef remollMessenger_HH
#define remollMessenger_HH

#include "remolltypes.hh"

#include "G4UImessenger.hh"

/*!
 *   Global messenger class
 */

class remollDetectorConstruction;
class remollGlobalField;
class remollBeamTarget;

class G4UIcmdWithAnInteger;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAString;
class G4UIcmdWithABool;

class G4VModularPhysicsList;

class remollMessenger : public G4UImessenger {

    private:
        // Singleton pointer
        static remollMessenger* gInstance;
        // Private constructor
        remollMessenger();

    public:
       	// Public destructor
        virtual ~remollMessenger();
        // Static instance getter
       	static remollMessenger* GetInstance();

	void SetPhysList( G4VModularPhysicsList *l ){ fPhysicsList = l; }

	void SetNewValue(G4UIcommand* cmd, G4String newValue);

    private:

	G4VModularPhysicsList *fPhysicsList;

	G4UIcmdWithAnInteger *seedCmd;
	G4UIcmdWithABool     *opticalCmd;
};

#endif//remollMessenger_HH























