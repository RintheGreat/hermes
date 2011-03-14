#define HERMES_REPORT_WARN
#define HERMES_REPORT_INFO
#define HERMES_REPORT_VERBOSE
#define HERMES_REPORT_FILE "application.log"
#include "hermes1d.h"

//  This example solves the eigenvalue problem for the neutron diffusion equation.
//
//  PDE: -(D.u')' + Sa.u = 1/k.nSf.u.
//
//  Interval: .
//
//  DC: Homogenous Neumann on the left, Newton "albedo.u + D.u' = 0".
//
//  The following parameters can be changed:
int N_subdiv_inner = 2;                           // Equidistant subdivision of the inner core macroelement.
int N_subdiv_outer = 2;                           // Equidistant subdivision of the outer core macroelement.
int N_subdiv_reflector = 1;                       // Equidistant subdivision of the reflector macroelement.
int P_init_inner = 3;                             // Initial polynomial degree in inner core (material 0).
int P_init_outer = 3;                             // Initial polynomial degree in outer core (material 1).
int P_init_reflector = 3;                         // Initial polynomial degree in reflector (material 2).
int Max_SI = 1000;                                // Max. number of eigenvalue iterations.
int N_SLN = 2;                                    // Number of solutions.
double K_EFF = 1.0;                               // Initial approximation.

// Geometry and materials.
const int N_MAT = 3;			                        // Number of macroelements with different materials.
const int N_GRP = 1;			                        // Number of energy groups in multigroup approximation.
double interfaces[N_MAT+1] = { 0, 50, 100, 125 }; // Coordinates of material regions interfaces [cm].
int Marker_inner = 0;                             // Material marker for inner core elements.
int Marker_outer = 1;                             // Material marker for outer core elements.
int Marker_reflector = 2;                         // Material marker for reflector elements.

// Newton's method.
double NEWTON_TOL = 1e-5;                         // Tolerance.
int NEWTON_MAX_ITER = 150;                        // Max. number of Newton iterations.
double TOL_SI = 1e-8;                             // Tolerance for the source (eigenvalue) iteration.

MatrixSolverType matrix_solver = SOLVER_UMFPACK;  // Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
                                                  // SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.

// Boundary conditions.
double Val_neumann_left = 0.0;		// Total reflection on the left (zero Neumann).
double Val_albedo_right = 0.5; 		// Vacuum on the right.

// Physical properties of each material type.
static double D[N_GRP][N_MAT] = 	
	{ { 0.650, 0.750, 1.150 } };		// Diffusion coefficient.
static double Sa[N_GRP][N_MAT] = 	
	{ { 0.120, 0.100, 0.010 } };		// Absorption cross-section.
static double nSf[N_GRP][N_MAT] = 
	{ { 0.185, 0.150, 0.000 } };		// Fission-yield cross section (\nu \Sigma_f).
static double chi[N_GRP] = 
	{ 1.0 };				                // Fission spectrum (for multigroup calculations).

// Other physical properties.
static double nu = 2.43; 		      // Nean number of neutrons released by fission.
static double eps = 3.204e-11;		// Nean energy release of each fission evt [J].



// Calculate \int \nu \Sigma_f(x) u(x) over an element 'e'.
double calc_elem_fission_yield(Element *e)
{
  // Get solution values at quadrature points.
  double val_phys[MAX_EQN_NUM][MAX_QUAD_PTS_NUM];
  double der_phys[MAX_EQN_NUM][MAX_QUAD_PTS_NUM];
  // This is enough since "nSf" is constant in elements.
  int order = e->p;   
  e->get_solution_quad(0,  order, val_phys, der_phys);
  // Get quadrature weights.
  double phys_x[MAX_QUAD_PTS_NUM];
  double phys_weights[MAX_QUAD_PTS_NUM];
  int pts_num;
  create_phys_element_quadrature(e->x1, e->x2, order, phys_x, phys_weights, &pts_num); 
  // Numerical quadrature in element 'e'.
  int n_grp = e->n_eq;
  double yield = 0;
  for (int i = 0; i < pts_num; i++) {
    double val = 0;
    int m = e->marker;
    for (int g = 0; g < n_grp; g++) val += nSf[g][m] * val_phys[g][i];
    yield += val * phys_weights[i];
  }
  return yield;
}

// Calculate \int_\Omega \nu \Sigma_f(x) u(x) over the entire space->
double calc_fission_yield(Space* space)
{
  double fis_yield = 0;
  Iterator *I = new Iterator(space);
  Element *e;
  while ((e = I->next_active_element()) != NULL) {
    fis_yield += calc_elem_fission_yield(e);
  }
  delete I;
  return fis_yield;
}

// Normalize the eigenfunction representing the neutron flux so that the total
// power it generates equals to 'desired_power' [W].
void normalize_to_power(Space* space, double desired_power)
{
  // Calculate total power generated by the computed flux 'u': 
  // P(u) = \eps \int_\Omega \Sigma_f(x) u(x).
  double P = eps * calc_fission_yield(space) / nu;
	
  // Calculate normalization constant 'c', so that P(c u) = 'desired_power'.
  double c = desired_power / P;
	
  // Multiply the computed flux by the normalization constant.
  multiply_dofs_with_constant(space, c);
}

// Weak forms
#include "forms.cpp"



int main() 
{
  // Three macroelements are defined above via the interfaces[] array.
  // poly_orders[]... initial poly degrees of macroelements.
  // material_markers[]... material markers of macroelements.
  // subdivisions[]... equidistant subdivision of macroelements.
  int poly_orders[N_MAT] = {P_init_inner, P_init_outer, P_init_reflector };
  int material_markers[N_MAT] = {Marker_inner, Marker_outer, Marker_reflector };
  int subdivisions[N_MAT] = {N_subdiv_inner, N_subdiv_outer, N_subdiv_reflector };

  // Boundary conditions.
  Hermes::vector<BCSpec *>DIR_BC_LEFT;
  Hermes::vector<BCSpec *>DIR_BC_RIGHT;
  
  // Create space.
  Space* space = new Space(N_MAT, interfaces, poly_orders, material_markers, subdivisions,
                           DIR_BC_LEFT, DIR_BC_RIGHT, N_GRP, N_SLN);
  // Enumerate basis functions, info for user.
  int ndof = Space::get_num_dofs(space);
  info("ndof: %d", ndof);

  // Initial approximation: u = 1.
  double K_EFF_old;
  double init_val = 1.0;
  set_vertex_dofs_constant(space, init_val);
  
  // Initialize the weak formulation.
  WeakForm wf;
  wf.add_matrix_form(jacobian_vol_inner, NULL, Marker_inner);
  wf.add_matrix_form(jacobian_vol_outer, NULL, Marker_outer);
  wf.add_matrix_form(jacobian_vol_reflector, NULL, Marker_reflector);
  wf.add_vector_form(residual_vol_inner, NULL, Marker_inner);
  wf.add_vector_form(residual_vol_outer, NULL, Marker_outer);
  wf.add_vector_form(residual_vol_reflector, NULL, Marker_reflector);
  wf.add_vector_form_surf(residual_surf_left, BOUNDARY_LEFT);
  wf.add_matrix_form_surf(jacobian_surf_right, BOUNDARY_RIGHT);
  wf.add_vector_form_surf(residual_surf_right, BOUNDARY_RIGHT);

  // Initialize the FE problem.
  bool is_linear = false;
  DiscreteProblem *dp = new DiscreteProblem(&wf, space, is_linear);

  // Source iteration (power method).
  for (int i = 0; i < Max_SI; i++)
  {	
    // Obtain fission source.
    int current_solution = 0, previous_solution = 1;
    copy_dofs(current_solution, previous_solution, space);

    // Obtain the number of degrees of freedom.
    int ndof = Space::get_num_dofs(space);

    // Fill vector coeff_vec using dof and coeffs arrays in elements.
    double *coeff_vec = new double[Space::get_num_dofs(space)];
    get_coeff_vector(space, coeff_vec);
  
    // Set up the solver, matrix, and rhs according to the solver selection.
    SparseMatrix* matrix = create_matrix(matrix_solver);
    Vector* rhs = create_vector(matrix_solver);
    Solver* solver = create_linear_solver(matrix_solver, matrix, rhs);
  
    int it = 1;
    while (1) {
      // Obtain the number of degrees of freedom.
      int ndof = Space::get_num_dofs(space);

      // Assemble the Jacobian matrix and residual vector.
      dp->assemble(coeff_vec, matrix, rhs);

      // Calculate the l2-norm of residual vector.
      double res_l2_norm = get_l2_norm(rhs);

      // Info for user.
      info("---- Newton iter %d, ndof %d, res. l2 norm %g", it, Space::get_num_dofs(space), res_l2_norm);

      // If l2 norm of the residual vector is within tolerance, then quit.
      // NOTE: at least one full iteration forced
      //       here because sometimes the initial
      //       residual on fine mesh is too small.
      if(res_l2_norm < NEWTON_TOL && it > 1) break;

      // Multiply the residual vector with -1 since the matrix 
      // equation reads J(Y^n) \deltaY^{n+1} = -F(Y^n).
      for(int i=0; i<ndof; i++) rhs->set(i, -rhs->get(i));

      // Solve the linear system.
      if(!solver->solve())
        error ("Matrix solver failed.\n");

      // Add \deltaY^{n+1} to Y^n.
      for (int i = 0; i < ndof; i++) coeff_vec[i] += solver->get_solution()[i];

      // If the maximum number of iteration has been reached, then quit.
      if (it >= NEWTON_MAX_ITER) error ("Newton method did not converge.");
      
      // Copy coefficients from vector y to elements.
      set_coeff_vector(coeff_vec, space);

      it++;
    }
    
    // Cleanup.
    delete matrix;
    delete rhs;
    delete solver;
    delete [] coeff_vec;
    
    // Update the eigenvalue.
    K_EFF_old = K_EFF;
    K_EFF = calc_fission_yield(space);		
    info("K_EFF_%d = %f", i, K_EFF);

    if (fabs(K_EFF - K_EFF_old)/K_EFF < TOL_SI) break;
  }

  // Plot the critical (i.e. steady-state) neutron flux.
  Linearizer l(space);
  l.plot_solution("solution.gp");
  
  // Normalize so that the absolute neutron flux generates 320 Watts of energy
  // (note that, using the symmetry condition at the origin, we've solved for  
  // flux only in the right half of the reactor).
  normalize_to_power(space, 320/2.);	

  // Plot the solution and space.
  l.plot_solution("solution_320W.gp");	
  space->plot("space.gp");

  info("K_EFF = %f", K_EFF);

  // Cleanup.
  delete dp;
  delete space;

  info("Done.");
  return 0;
}
