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
// Author: Arman Pazouki
// =============================================================================
//
// Model file to generate a Cylinder, as a FSI body, a sphere, as a non-fsi 
// body, fluid, and boundary. The cyliner is dropped on the water. Water is not
// steady and is modeled initially a cube of falling particles.
// parametrization of this model relies on params_demo_FSI_cylinderDrop.h
// =============================================================================

// General Includes
#include <iostream>
#include <fstream>
#include <string>
#include <limits.h>
#include <vector>
#include <ctime>
#include <assert.h>
#include <stdlib.h>  // system

// Chrono Parallel Includes
#include "chrono_parallel/physics/ChSystemParallel.h"

//#include "chrono_utils/ChUtilsVehicle.h"
#include "chrono/utils/ChUtilsGeometry.h"
#include "chrono/utils/ChUtilsCreators.h"
#include "chrono/utils/ChUtilsGenerators.h"
#include "chrono/utils/ChUtilsInputOutput.h"

// Chrono general utils
#include "chrono/core/ChFileutils.h"
#include "chrono/core/ChTransform.h"  //transform acc from GF to LF for post process

// Chrono fsi includes
#include "chrono_fsi/ChDeviceUtils.cuh"
#include "chrono_fsi/ChFsiTypeConvert.h"
#include "chrono_fsi/ChParams.cuh"
#include "chrono_fsi/ChSystemFsi.h"
#include "chrono_fsi/UtilsFsi/ChUtilsGeneratorFsi.h"
#include "chrono_fsi/UtilsFsi/ChUtilsPrintSph.h"
#include "chrono_fsi/include/utils.h"

#define haveFluid 1

// Chrono namespaces
using namespace chrono;
using namespace chrono::collision;

using std::cout;
using std::endl;

std::ofstream simParams;

// =============================================================================

//----------------------------
// output directories and settings
//----------------------------

const std::string out_dir = "FSI_OUTPUT"; //"../FSI_OUTPUT";
const std::string pov_dir_fluid = out_dir + "/povFilesFluid";
const std::string pov_dir_mbd = out_dir + "/povFilesHmmwv";
bool povray_output = true;
int out_fps = 30;

Real contact_recovery_speed = 1; ///< recovery speed for MBD

//----------------------------
// dimention of the box and fluid
//----------------------------

Real hdimX = 14;  // 5.5;
Real hdimY = 1.75;

Real hthick = 0.25;
Real basinDepth = 2;

Real fluidInitDimX = 2;
Real fluidHeight = 1.4;  // 2.0;

//------------------------------------------------------------------
// Fills in paramsH with simulation parameters.
//------------------------------------------------------------------
void SetupParamsH(fsi::SimParams* paramsH, Real hdimX, Real hdimY, Real hthick, Real basinDepth, Real fluidInitDimX, Real fluidHeight) {
	paramsH->sizeScale = 1;  // don't change it.
	paramsH->HSML = 0.2;
	paramsH->MULT_INITSPACE = 1.0;
	paramsH->epsMinMarkersDis = .001;
	paramsH->NUM_BOUNDARY_LAYERS = 3;
	paramsH->toleranceZone = paramsH->NUM_BOUNDARY_LAYERS
			* (paramsH->HSML * paramsH->MULT_INITSPACE);
	paramsH->BASEPRES = 0;
	paramsH->LARGE_PRES = 0;
	paramsH->deltaPress;
	paramsH->multViscosity_FSI = 1;
	paramsH->gravity =  mR3(0, 0, -9.81);
	paramsH->bodyForce3 =  mR3(0, 0, 0);
	paramsH->rho0 = 1000;
	paramsH->markerMass = pow(paramsH->MULT_INITSPACE * paramsH->HSML, 3) * paramsH->rho0;
	paramsH->mu0 = .001;
	paramsH->v_Max = 1; // Arman, I changed it to 0.1 for vehicle. Check this
	paramsH->EPS_XSPH = .5f;
	paramsH->dT = 1e-3;
	paramsH->tFinal = 2;
	paramsH->timePause = 0;
	paramsH->kdT = 5;  // I don't know what is kdT
	paramsH->gammaBB = 0.5;
	paramsH->binSize0;           // will be changed
	paramsH->rigidRadius;        // will be changed
	paramsH->densityReinit = 0;
	paramsH->enableTweak = 1;
	paramsH->enableAggressiveTweak = 0;
	paramsH->tweakMultV = 0.1;
	paramsH->tweakMultRho = .002;
	paramsH->bceType = ADAMI;  // ADAMI, mORIGINAL
	paramsH->cMin =  mR3(-hdimX, -hdimY, -basinDepth - hthick);
	paramsH->cMax =  mR3(hdimX, hdimY, basinDepth);
	//****************************************************************************************
	// printf("a1  paramsH->cMax.x, y, z %f %f %f,  binSize %f\n", paramsH->cMax.x, paramsH->cMax.y, paramsH->cMax.z, 2 *
	// paramsH->HSML);
	 int3 side0 =  mI3(
			floor((paramsH->cMax.x - paramsH->cMin.x) / (2 * paramsH->HSML)),
			floor((paramsH->cMax.y - paramsH->cMin.y) / (2 * paramsH->HSML)),
			floor((paramsH->cMax.z - paramsH->cMin.z) / (2 * paramsH->HSML)));
	 Real3 binSize3 =  mR3((paramsH->cMax.x - paramsH->cMin.x) / side0.x,
			(paramsH->cMax.y - paramsH->cMin.y) / side0.y,
			(paramsH->cMax.z - paramsH->cMin.z) / side0.z);
	paramsH->binSize0 = (binSize3.x > binSize3.y) ? binSize3.x : binSize3.y;
	//	paramsH->binSize0 = (paramsH->binSize0 > binSize3.z) ? paramsH->binSize0 : binSize3.z;
	paramsH->binSize0 = binSize3.x; // for effect of distance. Periodic BC in x direction. we do not care about paramsH->cMax y and z.
	paramsH->cMax = paramsH->cMin + paramsH->binSize0 *  mR3(side0);
	paramsH->boxDims = paramsH->cMax - paramsH->cMin;
	//****************************************************************************************
	paramsH->cMinInit =  mR3(-fluidInitDimX, paramsH->cMin.y,
			-basinDepth + 1.0 * paramsH->HSML);  // 3D channel
	paramsH->cMaxInit =  mR3(fluidInitDimX, paramsH->cMax.y,
			paramsH->cMinInit.z + fluidHeight);
	//****************************************************************************************
	//*** initialize straight channel
	paramsH->straightChannelBoundaryMin = paramsH->cMinInit; // mR3(0, 0, 0);  // 3D channel
	paramsH->straightChannelBoundaryMax = paramsH->cMaxInit; // SmR3(3, 2, 3) * paramsH->sizeScale;
	//************************** modify pressure ***************************
	//		paramsH->deltaPress = paramsH->rho0 * paramsH->boxDims * paramsH->bodyForce3;  //did not work as I expected
	paramsH->deltaPress =  mR3(0);//Wrong: 0.9 * paramsH->boxDims * paramsH->bodyForce3; // viscosity and boundary shape should play a roll

	// modify bin size stuff
	//****************************** bin size adjustement and contact detection stuff *****************************
	 int3 SIDE =  mI3(
			int((paramsH->cMax.x - paramsH->cMin.x) / paramsH->binSize0 + .1),
			int((paramsH->cMax.y - paramsH->cMin.y) / paramsH->binSize0 + .1),
			int((paramsH->cMax.z - paramsH->cMin.z) / paramsH->binSize0 + .1));
	 Real mBinSize = paramsH->binSize0; // Best solution in that case may be to change cMax or cMin such that periodic
									  // sides be a multiple of binSize
	//**********************************************************************************************************
	paramsH->gridSize = SIDE;
	// paramsH->numCells = SIDE.x * SIDE.y * SIDE.z;
	paramsH->worldOrigin = paramsH->cMin;
	paramsH->cellSize =  mR3(mBinSize, mBinSize, mBinSize);

	std::cout << "******************** paramsH Content" << std::endl;
	std::cout << "paramsH->sizeScale: " << paramsH->sizeScale << std::endl;
	std::cout << "paramsH->HSML: " << paramsH->HSML << std::endl;
	std::cout << "paramsH->bodyForce3: ";
	printStruct(paramsH->bodyForce3);
	std::cout << "paramsH->gravity: ";
	printStruct(paramsH->gravity);
	std::cout << "paramsH->rho0: " << paramsH->rho0 << std::endl;
	std::cout << "paramsH->mu0: " << paramsH->mu0 << std::endl;
	std::cout << "paramsH->v_Max: " << paramsH->v_Max << std::endl;
	std::cout << "paramsH->dT: " << paramsH->dT << std::endl;
	std::cout << "paramsH->tFinal: " << paramsH->tFinal << std::endl;
	std::cout << "paramsH->timePause: " << paramsH->timePause << std::endl;
	std::cout << "paramsH->timePauseRigidFlex: " << paramsH->timePauseRigidFlex
			<< std::endl;
	std::cout << "paramsH->densityReinit: " << paramsH->densityReinit
			<< std::endl;
	std::cout << "paramsH->cMin: ";
	printStruct(paramsH->cMin);
	std::cout << "paramsH->cMax: ";
	printStruct(paramsH->cMax);
	std::cout << "paramsH->MULT_INITSPACE: " << paramsH->MULT_INITSPACE
			<< std::endl;
	std::cout << "paramsH->NUM_BOUNDARY_LAYERS: " << paramsH->NUM_BOUNDARY_LAYERS
			<< std::endl;
	std::cout << "paramsH->toleranceZone: " << paramsH->toleranceZone
			<< std::endl;
	std::cout << "paramsH->NUM_BCE_LAYERS: " << paramsH->NUM_BCE_LAYERS
			<< std::endl;
	std::cout << "paramsH->solidSurfaceAdjust: " << paramsH->solidSurfaceAdjust
			<< std::endl;
	std::cout << "paramsH->BASEPRES: " << paramsH->BASEPRES << std::endl;
	std::cout << "paramsH->LARGE_PRES: " << paramsH->LARGE_PRES << std::endl;
	std::cout << "paramsH->deltaPress: ";
	printStruct(paramsH->deltaPress);
	std::cout << "paramsH->nPeriod: " << paramsH->nPeriod << std::endl;
	std::cout << "paramsH->EPS_XSPH: " << paramsH->EPS_XSPH << std::endl;
	std::cout << "paramsH->multViscosity_FSI: " << paramsH->multViscosity_FSI
			<< std::endl;
	std::cout << "paramsH->rigidRadius: ";
	printStruct(paramsH->rigidRadius);
	std::cout << "paramsH->binSize0: " << paramsH->binSize0 << std::endl;
	std::cout << "paramsH->boxDims: ";
	printStruct(paramsH->boxDims);
	std::cout << "paramsH->gridSize: ";
	printStruct(paramsH->gridSize);
	std::cout << "********************" << std::endl;
}


//------------------------------------------------------------------
// function to set some simulation settings from command line
//------------------------------------------------------------------

void SetArgumentsForMbdFromInput(int argc, char* argv[], int& threads,
	int& max_iteration_sliding, int& max_iteration_bilateral, 
	int& max_iteration_normal, int& max_iteration_spinning) {
	if (argc > 1) {
		const char* text = argv[1];
		threads = atoi(text);
	}
	if (argc > 2) {
		const char* text = argv[2];
		max_iteration_sliding = atoi(text);
	}
	if (argc > 3) {
		const char* text = argv[3];
		max_iteration_bilateral = atoi(text);
	}
	if (argc > 4) {
		const char* text = argv[4];
		max_iteration_normal = atoi(text);
	}
	if (argc > 5) {
		const char* text = argv[5];
		max_iteration_spinning = atoi(text);
	}
}

//------------------------------------------------------------------
// function to set the solver setting for the 
//------------------------------------------------------------------

void InitializeMbdPhysicalSystem(ChSystemParallelDVI& mphysicalSystem, ChVector<> gravity, int argc,
		char* argv[]) {
	// Desired number of OpenMP threads (will be clamped to maximum available)
	int threads = 1;
	// Perform dynamic tuning of number of threads?
	bool thread_tuning = true;

	//	uint max_iteration = 20;//10000;
	int max_iteration_normal = 0;
	int max_iteration_sliding = 200;
	int max_iteration_spinning = 0;
	int max_iteration_bilateral = 100;

	// ----------------------
	// Set params from input
	// ----------------------

	SetArgumentsForMbdFromInput(argc, argv, threads, max_iteration_sliding,
		max_iteration_bilateral, max_iteration_normal, max_iteration_spinning);

	// ----------------------
	// Set number of threads.
	// ----------------------

	//  omp_get_num_procs();
	int max_threads = omp_get_num_procs();
	if (threads > max_threads)
		threads = max_threads;
	mphysicalSystem.SetParallelThreadNumber(threads);
	omp_set_num_threads(threads);
	cout << "Using " << threads << " threads" << endl;

	mphysicalSystem.GetSettings()->perform_thread_tuning = thread_tuning;
	mphysicalSystem.GetSettings()->min_threads = std::max(1, threads / 2);
	mphysicalSystem.GetSettings()->max_threads = int(3.0 * threads / 2);

	// ---------------------
	// Print the rest of parameters
	// ---------------------

	simParams << endl << " number of threads: " << threads << endl
			<< " max_iteration_normal: " << max_iteration_normal << endl
			<< " max_iteration_sliding: " << max_iteration_sliding << endl
			<< " max_iteration_spinning: " << max_iteration_spinning << endl
			<< " max_iteration_bilateral: " << max_iteration_bilateral << endl
			<< endl;

	// ---------------------
	// Edit mphysicalSystem settings.
	// ---------------------

	double tolerance = 0.1;  // 1e-3;  // Arman, move it to paramsH
	// double collisionEnvelop = 0.04 * paramsH->HSML;
	mphysicalSystem.Set_G_acc(gravity);

	mphysicalSystem.GetSettings()->solver.solver_mode = SLIDING; // NORMAL, SPINNING
	mphysicalSystem.GetSettings()->solver.max_iteration_normal =
			max_iteration_normal;        // max_iteration / 3
	mphysicalSystem.GetSettings()->solver.max_iteration_sliding =
			max_iteration_sliding;      // max_iteration / 3
	mphysicalSystem.GetSettings()->solver.max_iteration_spinning =
			max_iteration_spinning;    // 0
	mphysicalSystem.GetSettings()->solver.max_iteration_bilateral =
			max_iteration_bilateral;  // max_iteration / 3
	mphysicalSystem.GetSettings()->solver.use_full_inertia_tensor = true;
	mphysicalSystem.GetSettings()->solver.tolerance = tolerance;
	mphysicalSystem.GetSettings()->solver.alpha = 0; // Arman, find out what is this
	mphysicalSystem.GetSettings()->solver.contact_recovery_speed =
			contact_recovery_speed;
	mphysicalSystem.ChangeSolverType(APGD);  // Arman check this APGD APGDBLAZE
	//  mphysicalSystem.GetSettings()->collision.narrowphase_algorithm = NARROWPHASE_HYBRID_MPR;

	//    mphysicalSystem.GetSettings()->collision.collision_envelope = collisionEnvelop;   // global collisionEnvelop
	//    does not work. Maybe due to sph-tire size mismatch
	mphysicalSystem.GetSettings()->collision.bins_per_axis = _make_int3(40, 40,
			40);  // Arman check
}

//------------------------------------------------------------------
// Create the objects of the MBD system. Rigid bodies, and if fsi, their
// bce representation are created and added to the systems
//------------------------------------------------------------------

void CreateMbdPhysicalSystemObjects(ChSystemParallelDVI& mphysicalSystem, fsi::ChSystemFsi &myFsiSystem, chrono::fsi::SimParams* paramsH) {

	std::shared_ptr<chrono::ChMaterialSurface> mat_g(new chrono::ChMaterialSurface);
	// Set common material Properties
	mat_g->SetFriction(0.8);
	mat_g->SetCohesion(0);
	mat_g->SetCompliance(0.0);
	mat_g->SetComplianceT(0.0);
	mat_g->SetDampingF(0.2);

	// Ground body
	auto ground = std::make_shared<ChBody>(new collision::ChCollisionModelParallel);
	ground->SetIdentifier(-1);
	ground->SetBodyFixed(true);
	ground->SetCollide(true);

	ground->SetMaterialSurface(mat_g);

	ground->GetCollisionModel()->ClearModel();

	// Bottom box
	double hdimSide = hdimX / 4.0;
	double midSecDim = hdimX - 2 * hdimSide;

	// basin info
	double phi = CH_C_PI / 9;
	double bottomWidth = midSecDim - basinDepth / tan(phi); // for a 45 degree slope
	double bottomBuffer = .4 * bottomWidth;

	double inclinedWidth = 0.5 * basinDepth / sin(phi); // for a 45 degree slope

	double smallBuffer = .7 * hthick;
	double x1I = -midSecDim + inclinedWidth * cos(phi) - hthick * sin(phi)
			- smallBuffer;
	double zI = -inclinedWidth * sin(phi) - hthick * cos(phi);
	double x2I = midSecDim - inclinedWidth * cos(phi) + hthick * sin(phi)
			+ smallBuffer;


		// beginning third
	ChVector<> size1(hdimSide, hdimY, hthick);
	ChQuaternion<> rot1 = chrono::QUNIT;
	ChVector<> pos1(-midSecDim - hdimSide, 0, -hthick);
	chrono::utils::AddBoxGeometry(ground.get(), size1, pos1, rot1, true);

		// end third
	ChVector<> size2(hdimSide, hdimY, hthick);
	ChQuaternion<> rot2 = chrono::QUNIT;
	ChVector<> pos2(midSecDim + hdimSide, 0, -hthick);
	chrono::utils::AddBoxGeometry(ground.get(), size2, pos2, rot2, true);

		// basin
	ChVector<> size3(bottomWidth + bottomBuffer, hdimY, hthick);
	ChQuaternion<> rot3 = chrono::QUNIT;
	ChVector<> pos3(0, 0, -basinDepth - hthick);
	chrono::utils::AddBoxGeometry(ground.get(), size3, pos3, rot3, true);

		// slope 1
	ChVector<> size4(inclinedWidth, hdimY, hthick);
	ChQuaternion<> rot4 = Q_from_AngAxis(phi, ChVector<>(0, 1, 0));
	ChVector<> pos4(x1I, 0, zI);
	chrono::utils::AddBoxGeometry(ground.get(), size4, pos4, rot4, true);

		// slope 2
	ChVector<> size5(inclinedWidth, hdimY, hthick);
	ChQuaternion<> rot5 = Q_from_AngAxis(-phi, ChVector<>(0, 1, 0));
	ChVector<> pos5(x2I, 0, zI);
	chrono::utils::AddBoxGeometry(ground.get(), size5, pos5, rot5, true);
	ground->GetCollisionModel()->BuildModel();

	mphysicalSystem.AddBody(ground);

#if haveFluid

	// Add ground
	// ---------------------

		// beginning third
	chrono::fsi::utils::AddBoxBce(myFsiSystem.GetDataManager(),
				paramsH, ground, pos1, rot1, size1);

		// end third
	chrono::fsi::utils::AddBoxBce(myFsiSystem.GetDataManager(),
				paramsH, ground, pos2, rot2, size2);

		// basin
	chrono::fsi::utils::AddBoxBce(myFsiSystem.GetDataManager(),
				paramsH, ground, pos3, rot3, size3);

		// slope 1
	chrono::fsi::utils::AddBoxBce(myFsiSystem.GetDataManager(),
				paramsH, ground, pos4, rot4, size4);

		// slope 2
	chrono::fsi::utils::AddBoxBce(myFsiSystem.GetDataManager(),
				paramsH, ground, pos5, rot5, size5);


	// Add floating cylinder
	// ---------------------
	double cyl_length = 3.5;
	double cyl_radius = .55;
	ChVector<> cyl_pos = ChVector<>(0, 0, 0);
	ChQuaternion<> cyl_rot = chrono::Q_from_AngAxis(CH_C_PI / 3, VECT_Z);

	std::vector<std::shared_ptr<ChBody> > * FSI_Bodies = myFsiSystem.GetFsiBodiesPtr();
	chrono::fsi::utils::CreateCylinderFSI(myFsiSystem.GetDataManager(),
			mphysicalSystem, FSI_Bodies, paramsH, mat_g, paramsH->rho0, cyl_pos, cyl_rot, cyl_radius, cyl_length);

#endif

	// version 0, create one cylinder // note: rigid body initialization should come after boundary initialization

	// **************************
	// **** Test angular velocity
	// **************************

	auto body = std::make_shared<ChBody>(new collision::ChCollisionModelParallel);
	// body->SetIdentifier(-1);
	body->SetBodyFixed(false);
	body->SetCollide(true);
	body->SetMaterialSurface(mat_g);
	body->SetPos(ChVector<>(5, 0, 2));
	body->SetRot(chrono::Q_from_AngAxis(CH_C_PI / 3, VECT_Y) * chrono::Q_from_AngAxis(CH_C_PI / 6, VECT_X) *
	chrono::Q_from_AngAxis(CH_C_PI / 6, VECT_Z));
	//    body->SetWvel_par(ChVector<>(0, 10, 0));  // Arman : note, SetW should come after SetRot
	//
	double sphereRad = 0.3;
	double volume = utils::CalcSphereVolume(sphereRad);
	ChVector<> gyration = utils::CalcSphereGyration(sphereRad).Get_Diag();
	double density = paramsH->rho0;
	double mass = density * volume;
	body->SetMass(mass);
	body->SetInertiaXX(mass * gyration);
	//
	body->GetCollisionModel()->ClearModel();
	utils::AddSphereGeometry(body.get(), sphereRad);
	body->GetCollisionModel()->BuildModel();
	//    // *** keep this: how to calculate the velocity of a marker lying on a rigid body
	//    //
	//    ChVector<> pointRel = ChVector<>(0, 0, 1);
	//    ChVector<> pointPar = pointRel + body->GetPos();
	//    // method 1
	//    ChVector<> l_point = body->Point_World2Body(pointPar);
	//    ChVector<> velvel1 = body->RelPoint_AbsSpeed(l_point);
	//    printf("\n\n\n\n\n\n\n\n\n ***********   velocity1  %f %f %f \n\n\n\n\n\n\n ", velvel1.x, velvel1.y,
	//    velvel1.z);
	//
	//    // method 2
	//    ChVector<> posLoc = ChTransform<>::TransformParentToLocal(pointPar, body->GetPos(), body->GetRot());
	//    ChVector<> velvel2 = body->PointSpeedLocalToParent(posLoc);
	//    printf("\n\n\n\n\n\n\n\n\n ***********   velocity 2 %f %f %f \n\n\n\n\n\n\n ", velvel2.x, velvel2.y,
	//    velvel2.z);
	//
	//    // method 3
	//    ChVector<> velvel3 = body->GetPos_dt() + body->GetWvel_par() % pointRel;
	//    printf("\n\n\n\n\n\n\n\n\n ***********   velocity3  %f %f %f \n\n\n\n\n\n\n ", velvel3.x, velvel3.y,
	//    velvel3.z);
	//    //

	int numRigidObjects = mphysicalSystem.Get_bodylist()->size();
	mphysicalSystem.AddBody(body);

	// extra objects
	// -----------------------------------------
	// Add extra collision body to test the collision shape
	// -----------------------------------------
	//
	//  Real rad = 0.1;
	//  // NOTE: mass properties and shapes are all for sphere
	//  double volume = utils::CalcSphereVolume(rad);
	//  ChVector<> gyration = utils::CalcSphereGyration(rad).Get_Diag();
	//  double density = paramsH->rho0;
	//  double mass = density * volume;
	//  double muFriction = 0;
	//
	//
	//  for (Real x = -4; x < 2; x += 0.25) {
	//    for (Real y = -1; y < 1; y += 0.25) {
	//      auto mball = std::make_shared<ChBody>(new collision::ChCollisionModelParallel);
	//      ChVector<> pos = ChVector<>(-8.5, .20, 3) + ChVector<>(x, y, 0);
	//      mball->SetMaterialSurface(mat_g);
	//      // body->SetIdentifier(fId);
	//      mball->SetPos(pos);
	//      mball->SetCollide(true);
	//      mball->SetBodyFixed(false);
	//      mball->SetMass(mass);
	//      mball->SetInertiaXX(mass * gyration);
	//
	//      mball->GetCollisionModel()->ClearModel();
	//      utils::AddSphereGeometry(mball.get(), rad);  // O
	//                                                       //	utils::AddEllipsoidGeometry(body.get(), size);
	//                                                       // X
	//
	//      mball->GetCollisionModel()->SetFamily(100);
	//      mball->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(100);
	//
	//      mball->GetCollisionModel()->BuildModel();
	//      mphysicalSystem.AddBody(mball);
	//    }
	//  }
}

//------------------------------------------------------------------
// Function to save the povray files of the MBD
//------------------------------------------------------------------

void SavePovFilesMBD(fsi::ChSystemFsi & myFsiSystem, ChSystemParallelDVI& mphysicalSystem, chrono::fsi::SimParams* paramsH, int tStep,
		double mTime) {
	static double exec_time;
	int out_steps = std::ceil((1.0 / paramsH->dT) / out_fps);
	exec_time += mphysicalSystem.GetTimerStep();
	int num_contacts = mphysicalSystem.GetNcontacts();

	static int out_frame = 0;

	// If enabled, output data for PovRay postprocessing.
	if (povray_output && tStep % out_steps == 0) {
		// **** out fluid
		chrono::fsi::utils::PrintToFile(myFsiSystem.GetDataManager()->sphMarkersD1.posRadD,
				myFsiSystem.GetDataManager()->sphMarkersD1.velMasD,
				myFsiSystem.GetDataManager()->sphMarkersD1.rhoPresMuD,
				myFsiSystem.GetDataManager()->fsiGeneralData.referenceArray,
				pov_dir_fluid);

		// **** out mbd
		if (tStep / out_steps == 0) {
			const std::string rmCmd = std::string("rm ") + pov_dir_mbd
					+ std::string("/*.dat");
			system(rmCmd.c_str());
		}

		char filename[100];
		sprintf(filename, "%s/data_%03d.dat", pov_dir_mbd.c_str(),
				out_frame + 1);
		utils::WriteShapesPovray(&mphysicalSystem, filename);

		cout << "------------ Output frame:   " << out_frame + 1 << endl;
		cout << "             Sim frame:      " << tStep << endl;
		cout << "             Time:           " << mTime << endl;
		cout << "             Avg. contacts:  " << num_contacts / out_steps
				<< endl;
		cout << "             Execution time: " << exec_time << endl;

		out_frame++;
	}
}

//------------------------------------------------------------------
// Print the simulation parameters: those pre-set and those set from 
// command line
//------------------------------------------------------------------

void printSimulationParameters(chrono::fsi::SimParams* paramsH) {
	simParams << " time_pause_fluid_external_force: "
			<< paramsH->timePause << endl
			<< " contact_recovery_speed: " << contact_recovery_speed << endl
			<< " maxFlowVelocity " << paramsH->v_Max << endl
			<< " time_step (paramsH->dT): " << paramsH->dT << endl
			<< " time_end: " << paramsH->tFinal << endl;
}

// =============================================================================

int main(int argc, char* argv[]) {
	time_t rawtime;
	struct tm* timeinfo;

	//(void) cudaSetDevice(0);

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	//****************************************************************************************
	// Arman take care of this block.
	// Set path to ChronoVehicle data files
	//  vehicle::SetDataPath(CHRONO_VEHICLE_DATA_DIR);
	//  vehicle::SetDataPath("/home/arman/Repos/GitBeta/chrono/src/demos/data/");
	//  SetChronoDataPath(CHRONO_DATA_DIR);

	// --------------------------
	// Create output directories.
	// --------------------------

	if (ChFileutils::MakeDirectory(out_dir.c_str()) < 0) {
		cout << "Error creating directory " << out_dir << endl;
		return 1;
	}

	if (povray_output) {
		if (ChFileutils::MakeDirectory(pov_dir_mbd.c_str()) < 0) {
			cout << "Error creating directory " << pov_dir_mbd << endl;
			return 1;
		}
	}

	if (ChFileutils::MakeDirectory(pov_dir_fluid.c_str()) < 0) {
		cout << "Error creating directory " << pov_dir_fluid << endl;
		return 1;
	}

	//****************************************************************************************
	const std::string simulationParams = out_dir
			+ "/simulation_specific_parameters.txt";
	simParams.open(simulationParams);
	simParams << " Job was submitted at date/time: " << asctime(timeinfo)
			<< endl;
	simParams.close();
	//****************************************************************************************
	bool mHaveFluid = false;
#if haveFluid
	mHaveFluid = true;
#endif
	// ***************************** Create Fluid ********************************************
	ChSystemParallelDVI mphysicalSystem;
	fsi::ChSystemFsi myFsiSystem(&mphysicalSystem, mHaveFluid);
	chrono::ChVector<> CameraLocation = chrono::ChVector<>(0, -10, 0);
	chrono::ChVector<> CameraLookAt = chrono::ChVector<>(0, 0, 0);

	chrono::fsi::SimParams* paramsH = myFsiSystem.GetSimParams();

		SetupParamsH(paramsH, hdimX, hdimY, hthick, basinDepth, fluidInitDimX, fluidHeight);
		printSimulationParameters(paramsH);
#if haveFluid
		Real initSpace0 = paramsH->MULT_INITSPACE * paramsH->HSML;
		utils::GridSampler<> sampler(initSpace0);
		chrono::fsi::Real3 boxCenter = chrono::fsi::mR3(0, 0, paramsH->cMin.z + 0.5 * basinDepth);
		boxCenter.z += 2 * paramsH->HSML;
		chrono::fsi::Real3 boxHalfDim = chrono::fsi::mR3(1.8, .9 * hdimY, 0.5 * basinDepth);
		utils::Generator::PointVector points = sampler.SampleBox(fsi::ChFsiTypeConvert::Real3ToChVector(boxCenter), fsi::ChFsiTypeConvert::Real3ToChVector(boxHalfDim));
		int numPart = points.size();
		for (int i = 0; i < numPart; i++) {
			myFsiSystem.GetDataManager()->AddSphMarker(chrono::fsi::mR3(points[i].x, points[i].y, points[i].z),
					chrono::fsi::mR3(0), chrono::fsi::mR4(paramsH->rho0, paramsH->BASEPRES, paramsH->mu0, -1));
		}

		int numPhases = myFsiSystem.GetDataManager()->fsiGeneralData.referenceArray.size(); //Arman TODO: either rely on pointers, or stack entirely, combination of '.' and '->' is not good
		if (numPhases != 0 ) {
			std::cout << "Error! numPhases is wrong, thrown from main\n" << std::endl;
			std::cin.get();
			return -1;
		} else {
			myFsiSystem.GetDataManager()->fsiGeneralData.referenceArray.push_back(mI4(0, numPart, -1, -1)); // map fluid -1, Arman : this will later be removed, relying on finalize function and automatic sorting
			myFsiSystem.GetDataManager()->fsiGeneralData.referenceArray.push_back(mI4(numPart, numPart, 0, 0)); // Arman : delete later
		}
#endif


	// ***************************** Create Rigid ********************************************

	ChVector<> gravity = ChVector<>(paramsH->gravity.x, paramsH->gravity.y, paramsH->gravity.z);
	InitializeMbdPhysicalSystem(mphysicalSystem, gravity, argc, argv);

	// This needs to be called after fluid initialization because I am using "numObjects.numBoundaryMarkers" inside it

	CreateMbdPhysicalSystemObjects(mphysicalSystem, myFsiSystem, paramsH);

	myFsiSystem.Finalize();

	// ***************************** Create Interface ********************************************

	//    assert(posRadH.size() == numObjects.numAllMarkers && "(2) numObjects is not set correctly");
	if (myFsiSystem.GetDataManager()->sphMarkersH.posRadH.size() != myFsiSystem.GetDataManager()->numObjects.numAllMarkers) {
		printf("\n\n\n\n Error! (2) numObjects is not set correctly \n\n\n\n");
		return -1;
	}

	//*** Add sph data to the physics system

	cout << " -- ChSystem size : " << mphysicalSystem.Get_bodylist()->size()
			<< endl;

	// ***************************** System Initialize ********************************************
	myFsiSystem.InitializeChronoGraphics(CameraLocation, CameraLookAt);


	double mTime = 0;

	DOUBLEPRECISION ?
			printf("Double Precision\n") : printf("Single Precision\n");
	int stepEnd = int(paramsH->tFinal / paramsH->dT);
	for (int tStep = 0; tStep < stepEnd + 1; tStep++) {
		printf("step : %d \n", tStep);


#if haveFluid
		myFsiSystem.DoStepDynamics_FSI();
#else
		myFsiSystem.DoStepDynamics_ChronoRK2();
#endif

		SavePovFilesMBD(myFsiSystem, mphysicalSystem, paramsH, tStep, mTime);

	}
//	ClearArraysH(posRadH, velMasH, rhoPresMuH, bodyIndex, referenceArray);

	return 0;
}
