//                       MFEM Example 9 - Parallel Version//// Compile with: make ex9p//// Sample runs://    mpirun -np 4 ex9p -m ../data/periodic-segment.mesh -p 0 -dt 0.005//    mpirun -np 4 ex9p -m ../data/periodic-square.mesh -p 0 -dt 0.01//    mpirun -np 4 ex9p -m ../data/periodic-hexagon.mesh -p 0 -dt 0.01//    mpirun -np 4 ex9p -m ../data/periodic-square.mesh -p 1 -dt 0.005 -tf 9//    mpirun -np 4 ex9p -m ../data/periodic-hexagon.mesh -p 1 -dt 0.005 -tf 9//    mpirun -np 4 ex9p -m ../data/amr-quad.mesh -p 1 -rp 1 -dt 0.002 -tf 9//    mpirun -np 4 ex9p -m ../data/star-q3.mesh -p 1 -rp 1 -dt 0.004 -tf 9//    mpirun -np 4 ex9p -m ../data/disc-nurbs.mesh -p 1 -rp 1 -dt 0.005 -tf 9//    mpirun -np 4 ex9p -m ../data/disc-nurbs.mesh -p 2 -rp 1 -dt 0.005 -tf 9//    mpirun -np 4 ex9p -m ../data/periodic-square.mesh -p 3 -rp 2 -dt 0.0025 -tf 9 -vs 20//    mpirun -np 4 ex9p -m ../data/periodic-cube.mesh -p 0 -o 2 -rp 1 -dt 0.01 -tf 8////    MFEM_adv steady-state test runs//    1D currently not implemented.//    mpirun -np 4 MFEM_adv -m ../data/inline-segment.mesh -p 0 -dt 0.005 -col 1//    mpirun -np 4 MFEM_adv -m ../data/inline-segment.mesh -p 0 -dt 0.2 -s 1 -col 1//    mpirun -np 4 MFEM_adv -m ../data/inline-segment.mesh -p 0 -dt 5 -s 1 -col 1//    2D//    mpirun -np 4 MFEM_adv -m ../data/inline-quad.mesh -p 0 -dt 0.005 -col 1//    mpirun -np 4 MFEM_adv -m ../data/inline-quad.mesh -p 0 -dt 0.2 -s 1 -col 1//    mpirun -np 4 MFEM_adv -m ../data/inline-quad.mesh -p 0 -dt 5 -s 1 -col 1//    2D AMR//    mpirun -np 4 MFEM_adv -m ../data/amr-quad.mesh -p 0 -rp 1 -dt 0.005 -col 1//    mpirun -np 4 MFEM_adv -m ../data/amr-quad.mesh -p 0 -rp 1 -dt 0.2 -s 1 -col 1//    mpirun -np 4 MFEM_adv -m ../data/amr-quad.mesh -p 0 -rp 1 -dt 5 -s 1 -col 1//    High azimuthal resolution test.//    mpirun -np 4 ex23TRp -m ../data/inline-segment.mesh -p 0 -rp 0 -tf 2000000 -dt 1000 -s 1 -col 5 -sna 12//    mpirun -np 4 ex23TRp -m ../data/inline-quad.mesh -p 0 -rp 0 -tf 2000000 -dt 100000 -s 1 -col 5 -sna 12//    mpirun -np 4 ex23TRp -m ../data/amr-quad.mesh -p 0 -rp 0 -tf 2000000 -dt 2000000 -s 1 -col 5 -sna 12 //    mpirun -np 4 ex23TRp -m ../data/inline-quad.mesh -p 0 -rp 0 -tf 0.3 -dt 0.01 -s 1 -col 5 -sna 5//    mpirun -np 4 ex23TRp -m ../data/inline-quad.mesh -p 0 -rp 1 -tf 0.23 -dt 0.01 -s 1 -col 5 -sna 5     //    mpirun -np 2 exTRguts -m milan_data/ALE_quad.mesh -rs 1//    AWBS//    mpirun -n 4 exTRguts -m 'milan_data/ALE_quad.mesh' -rs 0 -sna 1 -Ngr 900 -s 13//    mpirun -n 4 exTRguts -m 'milan_data/ALE_quad.mesh' -rs 0 -sna 1 -Ngr 80 -s 24 ////    WEAK SCALING TEST//    mpirun -n 4 exTRguts -m 'milan_data/ALE_quad.mesh' -rs 1 -Ngr 80 -s 24 -Ar 10 -Ac 6 -vis//    Reasonable nonlocal plasma results (extreme nonlocality dt/dx high).//    mpirun -n 4 exTRguts -m '../data/inline-quad.mesh' -rs 2 -Ngr 500 -s 24 -vis -ne 2e23 -sna 4//    Same, but local conditions run (dt/dx low).//    mpirun -n 4 exTRguts -m '../data/inline-quad.mesh' -rs 2 -Ngr 500 -s 24 -vis -ne 2e25 -sna 4//// Description:  This example code solves the time-dependent advection equation//               du/dt + v.grad(u) = 0, where v is a given fluid velocity, and//               u0(x)=u(0,x) is a given initial condition.////               The example demonstrates the use of Discontinuous Galerkin (DG)//               bilinear forms in MFEM (face integrators), the use of explicit//               ODE time integrators, the definition of periodic boundary//               conditions through periodic meshes, as well as the use of GLVis//               for persistent visualization of a time-evolving solution. The//               saving of time-dependent data files for external visualization//               with VisIt (visit.llnl.gov) is also illustrated.// This problem seems to like an FFC rekaxation...#include "mfem.hpp"#include <fstream>#include <iostream>#include "TRgutsp.cpp"using namespace std;using namespace mfem;int main(int argc, char *argv[]){   // 1. Initialize MPI.   int num_procs, myid;   MPI_Init(&argc, &argv);   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);   MPI_Comm_rank(MPI_COMM_WORLD, &myid);   // 1.5 Parse mesh command-line option and initialize mesh.   const char *mesh_file = "./meshes/periodic-hexagon.mesh";    int ser_ref_levels = 2;   int par_ref_levels = 0;   int ode_solver_type = 3;   int SN_azi = 4;   int Ngr = 23;   double ne_ref = 1e28;   //int order = 3;   bool visualization = false;   //TRguts::AIR_parameters AIR = {1.5, "", "FA", 0.1, 0.01, 0.0, 100, 10, 0.0001, 6};   TRguts::AIR_parameters AIR = {2, "", "FA", 0.1, 0.01, 0.0, 100, 10, 0.0001, 10};   const char* temp_prerelax = NULL;   const char* temp_postrelax = NULL;   OptionsParser args(argc, argv);   args.AddOption(&mesh_file, "-m", "--mesh",                       "Mesh file to use.");   args.AddOption(&ser_ref_levels, "-rs", "--refine-serial",                       "Number of times to refine the mesh uniformly in serial.");   args.AddOption(&par_ref_levels, "-rp", "--refine-parallel",                       "Number of times to refine the mesh uniformly in parallel.");   args.AddOption(&ode_solver_type, "-s", "--ode-solver",                  "ODE solver: 1 - Backward Euler, 2 - SDIRK2, 3 - SDIRK3,\n\t"                  "\t 11 - Forward Euler, 12 - RK2, 13 - RK3 SSP, 14 - RK4,\n\t"                  "\t 22 - Imp. midpoint, 23 - A-stable SDIRK3, 24 - A-stable SDIRK4.");   args.AddOption(&SN_azi, "-sna", "--SN_azimuthal",                  "Number of discrete ordinates in azimuthal angle.");   args.AddOption(&Ngr, "-Ngr", "--Num_groups",                  "Number of velocity groups.");   args.AddOption(&ne_ref, "-ne", "--ne-reference",                  "Reference density of plasma.");   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",                  "--no-visualization",                  "Enable or disable GLVis visualization.");     // AIR parser.   args.AddOption(&(AIR.distance), "-Ad", "--AIR-distance",                  "Distance restriction neighborhood for AIR.");   args.AddOption(&(AIR.interp_type), "-Ai", "--AIR-interpolation",                  "Index for hypre interpolation routine.");   args.AddOption(&(AIR.coarsening), "-Ac", "--AIR-coarsening",                  "Index for hypre coarsening routine.");   args.AddOption(&(AIR.strength_tolC), "-AsC", "--AIR-strengthC",                  "Theta value determining strong connections for AIR (coarsening).");   args.AddOption(&(AIR.strength_tolR), "-AsR", "--AIR-strengthR",                  "Theta value determining strong connections for AIR (restriction).");   args.AddOption(&(AIR.filter_tolR), "-AfR", "--AIR-filterR",                  "Theta value eliminating small entries in restriction (after building).");   args.AddOption(&(AIR.filterA_tol), "-Af", "--AIR-filter",                  "Theta value to eliminate small connections in AIR hierarchy. Use -1 to specify O(h).");   args.AddOption(&(AIR.relax_type), "-Ar", "--AIR-relaxation",                  "Index for hypre relaxation routine.");   args.AddOption(&temp_prerelax, "-Ar1", "--AIR-prerelax",                  "String denoting prerelaxation scheme; e.g., FCC.");   args.AddOption(&temp_postrelax, "-Ar2", "--AIR-postrelax",                  "String denoting postrelaxation scheme; e.g., FFC.");   args.Parse();     if (temp_prerelax != NULL) AIR.prerelax = std::string(temp_prerelax);   if (temp_postrelax != NULL) AIR.postrelax = std::string(temp_postrelax);   if (!args.Good())   {      if (myid == 0)      {         args.PrintUsage(cout);      }      MPI_Finalize();      return 1;   }   // 3. Read the serial mesh from the given mesh file on all processors. We can   //    handle geometrically periodic meshes in this code.   Mesh *mesh = new Mesh(mesh_file, 1, 1);   int dim = mesh->Dimension();   // 5. Refine the mesh in serial to increase the resolution. In this example   //    we do 'ser_ref_levels' of uniform refinement, where 'ser_ref_levels' is   //    a command-line parameter. If the mesh is of NURBS type, we convert it   //    to a (piecewise-polynomial) high-order mesh.   for (int lev = 0; lev < ser_ref_levels; lev++)   {      mesh->UniformRefinement();   }   if (mesh->NURBSext)   {      mesh->SetCurvature(1);      //mesh->SetCurvature(max(order, 1));   }   Vector bb_min, bb_max;    mesh->GetBoundingBox(bb_min, bb_max, 1);   //mesh->GetBoundingBox(bb_min, bb_max, max(order, 1));   // 6. Define the parallel mesh by a partitioning of the serial mesh. Refine   //    this mesh further in parallel to increase the resolution. Once the   //    parallel mesh is defined, the serial mesh can be deleted.   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);   delete mesh;   for (int lev = 0; lev < par_ref_levels; lev++)   {      pmesh->UniformRefinement();   }   // Run TR example on the ParMesh pmesh.   TRguts::TRmain(pmesh, AIR, ne_ref,                   bb_min, bb_max,                   Ngr, ode_solver_type, SN_azi, visualization);   delete pmesh;    MPI_Finalize();   return 0;}