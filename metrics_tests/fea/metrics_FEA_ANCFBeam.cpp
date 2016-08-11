// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Antonio Recuero, Conlain Kelly, Radu Serban
// =============================================================================
//
// ANCF gradient-deficient beam element
//
// Successful execution of this unit test may validate: this element's internal
// force, distributed gravity, inertia, and numerical integration implementations.
//
// Special attention must be paid to the number of Gauss points for gravity. For
// successful verification, this number must be 2.
// =============================================================================
#include <cstdio>
#include <cmath>
#include <string>

#include "chrono/core/ChFileutils.h"
#include "chrono/core/ChMathematics.h"
#include "chrono/core/ChVector.h"
#include "chrono/physics/ChLoadContainer.h"
#include "chrono/physics/ChSystem.h"
#include "chrono/solver/ChSolverMINRES.h"
#include "chrono/solver/ChSolverPMINRES.h"
#include "chrono/timestepper/ChTimestepper.h"
#include "chrono/utils/ChUtilsInputOutput.h"
#include "chrono/utils/ChUtilsValidation.h"
#include "chrono_fea/ChElementBeamANCF.h"
#include "chrono_fea/ChLinkDirFrame.h"
#include "chrono_fea/ChLinkPointFrame.h"
#include "chrono_fea/ChLoadsBeam.h"
#include "chrono_fea/ChMesh.h"

#include "../BaseTest.h"

using namespace chrono;
using namespace chrono::fea;

// ====================================================================================

// Test class
class ANCFBeamTest : public BaseTest {
  public:
    ANCFBeamTest(const std::string& testName, const std::string& testProjectName)
        : BaseTest(testName, testProjectName), m_execTime(0) {}

    ~ANCFBeamTest() {}

    // Override corresponding functions in BaseTest
    virtual bool execute() override;
    virtual double getExecutionTime() const override { return m_execTime; }

  private:
    double m_execTime;
};

bool ANCFBeamTest::execute() {
    // Create a Chrono::Engine physical system
    ChSystem my_system;
    unsigned int num_steps = 200;

    // Create a mesh, that is a container for groups of elements and
    // their referenced nodes.
    auto my_mesh = std::make_shared<ChMesh>();

    const double f_const = 5.0;  // Gerstmayr's paper's parameter
    double diam = 0.0;
    const double Ang_VelY = 4.0;
    const double beam_length = 1.0;
    unsigned int NElem = 4;
    double rho = 0.0;

    auto msection_cable = std::make_shared<ChBeamSectionCable>();
    diam = sqrt(1e-6 / CH_C_PI) * 2.0 * f_const;
    msection_cable->SetDiameter(diam);
    msection_cable->SetYoungModulus(1e9 / pow(f_const, 4));
    msection_cable->SetBeamRaleyghDamping(0.000);
    msection_cable->SetI(CH_C_PI / 4.0 * pow(diam / 2, 4));
    rho = 8000 / pow(f_const, 2);
    msection_cable->SetDensity(rho);

    // Create the nodes
    auto hnodeancf1 = std::make_shared<ChNodeFEAxyzD>(ChVector<>(0, 0, 0.0), ChVector<>(1, 0, 0));
    auto hnodeancf2 = std::make_shared<ChNodeFEAxyzD>(ChVector<>(beam_length / 4, 0, 0), ChVector<>(1, 0, 0));
    auto hnodeancf3 = std::make_shared<ChNodeFEAxyzD>(ChVector<>(beam_length / 2, 0, 0), ChVector<>(1, 0, 0));
    auto hnodeancf4 = std::make_shared<ChNodeFEAxyzD>(ChVector<>(3.0 * beam_length / 4, 0, 0), ChVector<>(1, 0, 0));
    auto hnodeancf5 = std::make_shared<ChNodeFEAxyzD>(ChVector<>(beam_length, 0, 0), ChVector<>(1, 0, 0));

    my_mesh->AddNode(hnodeancf1);
    my_mesh->AddNode(hnodeancf2);
    my_mesh->AddNode(hnodeancf3);
    my_mesh->AddNode(hnodeancf4);
    my_mesh->AddNode(hnodeancf5);

    // Create the element 1
    auto belementancf1 = std::make_shared<ChElementBeamANCF>();
    belementancf1->SetNodes(hnodeancf1, hnodeancf2);
    belementancf1->SetSection(msection_cable);
    my_mesh->AddElement(belementancf1);

    // Create the element 2
    auto belementancf2 = std::make_shared<ChElementBeamANCF>();
    belementancf2->SetNodes(hnodeancf2, hnodeancf3);
    belementancf2->SetSection(msection_cable);
    my_mesh->AddElement(belementancf2);

    // Create the element 3
    auto belementancf3 = std::make_shared<ChElementBeamANCF>();
    belementancf3->SetNodes(hnodeancf3, hnodeancf4);
    belementancf3->SetSection(msection_cable);
    my_mesh->AddElement(belementancf3);

    // Create the element 4
    auto belementancf4 = std::make_shared<ChElementBeamANCF>();
    belementancf4->SetNodes(hnodeancf4, hnodeancf5);
    belementancf4->SetSection(msection_cable);
    my_mesh->AddElement(belementancf4);

    auto mtruss = std::make_shared<ChBody>();
    mtruss->SetBodyFixed(true);

    auto constraint_hinge = std::make_shared<ChLinkPointFrame>();
    constraint_hinge->Initialize(hnodeancf1, mtruss);
    my_system.Add(constraint_hinge);

    // Cancel automatic gravity
    my_mesh->SetAutomaticGravity(false);

    // Remember to add the mesh to the system!
    my_system.Add(my_mesh);

    // Add external forces and initial conditions
    // Angular velocity initial condition
    hnodeancf1->SetPos_dt(ChVector<>(0, 0, -Ang_VelY * beam_length / NElem * 0.0));
    hnodeancf2->SetPos_dt(ChVector<>(0, 0, -Ang_VelY * beam_length / NElem * 1.0));
    hnodeancf3->SetPos_dt(ChVector<>(0, 0, -Ang_VelY * beam_length / NElem * 2.0));
    hnodeancf4->SetPos_dt(ChVector<>(0, 0, -Ang_VelY * beam_length / NElem * 3.0));
    hnodeancf5->SetPos_dt(ChVector<>(0, 0, -Ang_VelY * beam_length / NElem * 4.0));

    // First: loads must be added to "load containers",
    // and load containers must be added to your ChSystem
    auto mloadcontainer = std::make_shared<ChLoadContainer>();
    my_system.Add(mloadcontainer);

    // Add gravity (constant volumetric load): Use 2 Gauss integration points

    auto mgravity1 = std::make_shared<ChLoad<ChLoaderGravity>>(belementancf1);
    mgravity1->loader.SetNumIntPoints(2);
    mloadcontainer->Add(mgravity1);

    auto mgravity2 = std::make_shared<ChLoad<ChLoaderGravity>>(belementancf2);
    mgravity2->loader.SetNumIntPoints(2);
    mloadcontainer->Add(mgravity2);

    auto mgravity3 = std::make_shared<ChLoad<ChLoaderGravity>>(belementancf3);
    mgravity3->loader.SetNumIntPoints(2);
    mloadcontainer->Add(mgravity3);

    auto mgravity4 = std::make_shared<ChLoad<ChLoaderGravity>>(belementancf4);
    mgravity4->loader.SetNumIntPoints(2);
    mloadcontainer->Add(mgravity4);

    // Change solver settings
    my_system.SetSolverType(ChSystem::SOLVER_MINRES);
    my_system.SetSolverWarmStarting(true);  // this helps a lot to speedup convergence in this class of problems
    my_system.SetMaxItersSolverSpeed(200);
    my_system.SetMaxItersSolverStab(200);
    my_system.SetTolForce(1e-14);
    ChSolverMINRES* msolver = (ChSolverMINRES*)my_system.GetSolverSpeed();
    msolver->SetVerbose(false);
    msolver->SetDiagonalPreconditioning(true);

    my_system.SetEndTime(12.5);

    // Change type of integrator:
    my_system.SetIntegrationType(chrono::ChSystem::INT_EULER_IMPLICIT_LINEARIZED);  // fast, less precise
    //  my_system.SetIntegrationType(chrono::ChSystem::INT_HHT);  // precise,slower, might iterate each step

    // if later you want to change integrator settings:
    if (auto mystepper = std::dynamic_pointer_cast<ChTimestepperHHT>(my_system.GetTimestepper())) {
        mystepper->SetAlpha(0.0);
        mystepper->SetMaxiters(60);
        mystepper->SetAbsTolerances(1e-14);
    }

    // Mark completion of system construction
    my_system.SetupInitial();

    ChTimer<> timer;
    for (unsigned int it = 0; it < num_steps; it++) {
        timer.start();
        my_system.DoStepDynamics(0.0001);
        timer.stop();
    }

    m_execTime = timer.GetTimeSeconds();
    addMetric("num_steps", (int)num_steps);
    addMetric("avg_time_per_step", m_execTime / num_steps);

    return true;
}

// ====================================================================================

int main(int argc, char* argv[]) {
    std::string out_dir = "../METRICS";
    if (ChFileutils::MakeDirectory(out_dir.c_str()) < 0) {
        std::cout << "Error creating directory " << out_dir << std::endl;
        return 1;
    }

    ANCFBeamTest test("utest_FEA_ANCFBeam", "Chrono::FEA");
    test.setOutDir(out_dir);
    test.setVerbose(true);
    bool passed = test.run();
    test.print();

    return 0;
}
