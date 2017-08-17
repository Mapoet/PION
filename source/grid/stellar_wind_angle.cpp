/// Modifications:
/// - 2017.07.[20-31] RK/JM: Getting it working.
/// - 2017.08.[15-?] RK: reworked to include trilinear interpolation
/// of density, to accommodate for addition of Teff dimension to
/// setup_tables

#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"
#include "tools/reporting.h"
#include "tools/mem_manage.h"
#include "tools/interpolate.h"
#include "constants.h"
#ifdef TESTING
#include "tools/command_line_interface.h"
#endif // TESTING

#include "grid/grid_base_class.h"
#include "grid/stellar_wind_angle.h"
#include "microphysics/microphysics_base.h"
#include <sstream>


#include "constants.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdio>
using namespace std;


// ##################################################################
// ##################################################################


// Constructor
stellar_wind_angle::stellar_wind_angle()
{
	// Constants for wind functions
	stellar_wind_angle::c_gamma = 0.35;
	stellar_wind_angle::c_xi    = -0.43;
	
    // Number of points in theta, omega and Teff vectors
    stellar_wind_angle::npts_theta = 25;
    stellar_wind_angle::npts_omega = 25;
    stellar_wind_angle::npts_Teff = 20;

    // Generate interpolating tables
	setup_tables();
}


// ##################################################################
// ##################################################################


// Destructor
stellar_wind_angle::~stellar_wind_angle()
{

}


// ##################################################################
// ##################################################################


// Function to replace pow(a, b) - exp(b*log(a)) is twice as fast
// (also added fn to stellar_wind - probably already inherited from stellar_wind_evolution?)
double stellar_wind_angle::pow_fast(
	double a,
	double b
	)
{
	return exp(b*log(a));
}


// ##################################################################
// ##################################################################


// Generate interpolating tables for wind density function
void stellar_wind_angle::setup_tables()
{   
    //
	// Set up theta vector
    //
    
	cout << "Setting up interpolating tables" << endl;

    // Min, mid and max values of theta - 5pts between min and mid, 20pts between mid and max
    double theta_min = 0.1;
    double theta_mid = 60.0;
    double theta_max = 89.9;
    
	theta_vec.resize(npts_theta);
    
    for (int k = 0; k < npts_theta; k++)
    {
        if (k <= 4) theta_vec[k] = (theta_min + k*((theta_mid - theta_min)/4.0))*(pconst.pi()/180.0);
        else theta_vec[k] = (theta_mid + (k - 4)*((theta_max - theta_mid)/(npts_theta - 5)))*(pconst.pi()/180.0);
	}
    
    cout << "Theta vector generated" << endl;

    
	//
    // Set up omega vector
    //
    
	log_mu_vec.resize(npts_omega);

    // Iterate log(mu) values - evenly spaced
    for (int i = 0; i < npts_omega; i++) log_mu_vec[npts_omega - i - 1] = -4.0 + i*(4.0/npts_omega);

	omega_vec.resize(npts_omega);
    
    // Iterate omega values - log spacing - spacing decreases as omega approaches 1
    for (int j = 0; j < npts_omega; j++) omega_vec[j] = 1 - pow_fast(10, log_mu_vec[j]);

    cout << "Omega vector generated" << endl;


    //
    // Set up Teff vector
    //

    // Temperature ranges from Eldridge et al. (2006, MN, 367, 186) (K)
    double T0 = 3600, T1 = 6000, T2 = 8000, T3 = 10000, T4 = 20000, T5 = 22000;

    Teff_vec.resize(npts_Teff);
    
    // Set values - based on plots for alpha and delta vs. Teff
    for (int i = 0; i < npts_Teff; i++){
        if (i == 0)             Teff_vec[i] = T0;
        if (1 <= i && i <= 5)   Teff_vec[i] = T0 + i*((T1 - T0)/6);
        if (i == 6)             Teff_vec[i] = T1;
        if (7 <= i && i <= 9)   Teff_vec[i] = T1 + (i - 6)*((T2 - T1)/4);
        if (i == 10)            Teff_vec[i] = T2;
        if (11 <= i && i <= 13) Teff_vec[i] = T2 + (i - 10)*((T3 - T2)/4);
        if (i == 14)            Teff_vec[i] = T3;
        if (i == 15)            Teff_vec[i] = T4;
        if (16 <= i && i <= 18) Teff_vec[i] = T4 + (i - 15)*((T5 - T4)/4);
        if (i == 19)            Teff_vec[i] = T5;
        }
    
    cout << "Teff vector generated" << endl;


    //
    // Write delta table
    //
    
	delta_vec.resize(npts_omega);
    for (int i = 0; i < npts_omega; i++) delta_vec[i].resize(npts_Teff);
    
    // Set values - delta(omega, Teff)
    for (int i = 0; i < npts_omega; i++){
        for (int j = 0; j < npts_Teff; j++){
            delta_vec[i][j] = fn_delta(omega_vec[i], Teff_vec[j]);
        }
    }

    cout << "Delta table generated" << endl;


    //
    // Write alpha table
    //
	
    alpha_vec.resize(npts_omega);
    
    for(int i = 0; i < npts_omega; i++){
        alpha_vec[i].resize(npts_theta);
        for(int j = 0; j < npts_theta; j++){
            alpha_vec[i][j].resize(npts_Teff);
        }
    }
    
    // Set values - alpha(omega, theta, Teff)
    for(int i = 0; i < npts_omega; i++){
        for(int j = 0; j < npts_theta; j++){
            for(int k = 0; k < npts_Teff; k++){
                alpha_vec[i][j][k] = fn_alpha(omega_vec[i], theta_vec[j], Teff_vec[k]);
            }
        }
    }
	
    cout << "Alpha table generated" << endl;
    
    return;
}


// ##################################################################
// ##################################################################


double stellar_wind_angle::beta(const double Teff)
{
  //
  // Eldridge et al. (2006, MN, 367, 186).
  // V_inf = sqrt(beta)*V_esc
  //
  double beta;

  if (Teff <= 3600.0)
    beta = 0.125;
  else if (Teff >= 22000.0)
    beta = 2.6;
  else {
    //
    // Linear interpolation for beta from Eldridge et al. Table 1.
    //
    double b0, b1, T0, T1;
    if      (Teff<6000.0) {
      T0 = 3600.0; b0 = 0.125;
      T1 = 6000.0; b1 = 0.5;
    }
    else if (Teff <8000.0) {
      T0 = 6000.0; b0 = 0.5;
      T1 = 8000.0; b1 = 0.7;
    }
    else if (Teff <10000.0) {
      T0 = 8000.0; b0 = 0.7;
      T1 = 10000.0; b1 = 1.3;
    }
    else if (Teff <20000.0) {
      T0 = 10000.0; b0=1.3;
      T1 = 20000.0; b1=1.3;
    }
    else {
      T0 = 20000.0; b0 = 1.3;
      T1 = 22000.0; b1 = 2.6;
    }
    beta = b0 + (Teff-T0)*(b1-b0)/(T1-T0);
  }

  return beta;
};


// ##################################################################
// ##################################################################


// Integrand for integral in delta function
double stellar_wind_angle::integrand(
	double theta, // Co-latitude angle (radians)
	double omega, // Omega (v_rot/v_esc)
	double Teff // Teff (K)
    )
{
	return fn_alpha(omega, theta, Teff) * pow_fast(1.0 - omega*sin(theta), c_xi) * sin(theta);
} 


// ##################################################################
// ##################################################################


// Simpson's rule integration scheme
double  stellar_wind_angle::integrate_Simpson(
    const double min, ///< lower limit
    const double max, ///< upper limit
    const long int npt,    ///< Number of points (must be even)
    const double omega, ///<  omega
	const double Teff ///< Teff (K)
    )
{

  //
  // Simpson's rule  int=(f(0)+4f(1)+2f(2)+...+2f(n-2)+4f(n-1)+f(n))*h/3
  // where n is even.
  //
  double hh = (max-min)/npt;
  double ans = 0.0;
  //
  // f(0) lower limit
  //
  ans += integrand(min, omega, Teff);
  //
  // f(N) upper limit
  //
  ans += integrand(max, omega, Teff);
  //
  // Intermediate points.
  //
  int wt = 4; double x=0.0;
  for (long int i=1; i<npt; i++) {
    x = min + i*hh;
    ans += wt*integrand(x, omega, Teff);
    wt = 6-wt;
  }
  //
  // finally multiply by hh/3.0
  //
  ans *= hh/3.0;
  return ans;
} 


// ##################################################################
// ##################################################################


// Phi' function
double stellar_wind_angle::fn_phi(
	double omega, // omega (v_rot/v_esc)
	double theta, // Co-latitude angle (radians)
	double Teff // Teff (K)
	)
{
	return (omega/(22.0*pconst.sqrt2()*sqrt(beta(Teff)))) * sin(theta) * pow_fast(1.0 - omega*sin(theta), -c_gamma);
}


// ##################################################################
// ##################################################################


// Alpha function
double stellar_wind_angle::fn_alpha(
	double omega, // Omega (v_rot/v_esc)
	double theta, // Co-latitude angle (radians)
	double Teff // Teff (K)
	)
{
	return pow_fast(cos(fn_phi(omega, theta, Teff)) + pow_fast(tan(theta),-2.0) * 
		   (1.0 + c_gamma*( omega*sin(theta) / (1.0 - omega*sin(theta)) )) * fn_phi(omega, theta, Teff) *
		   sin(fn_phi(omega, theta, Teff)), -1.0);
} // the cotan term will diverge here if theta = 0.0


// ##################################################################
// ##################################################################


// Delta function
double stellar_wind_angle::fn_delta(
	double omega, // Omega (v_rot/v_esc)
	double Teff // Teff (K)
	)
{
	return 2.0*pow_fast(integrate_Simpson(0.001, pconst.pi()/2.0, 230, omega, Teff), -1.0);
} // 230 points determined to give sufficient accuracy


// ##################################################################
// ##################################################################


// Terminal wind velocity function
double stellar_wind_angle::fn_v_inf(
	double omega, // Omega (v_rot/v_esc)
	double v_esc, // Escape velocity (cm/s)
	double theta, // Co-latitude angle (radians)
	double Teff // Teff (K)
	)
{
	return sqrt(beta(Teff)) * v_esc * pow_fast(1.0 - omega*sin(theta), c_gamma);
}


// ##################################################################
// ##################################################################


// Analytic wind density function
double stellar_wind_angle::fn_density(
	double omega, // Omega (v_rot/v_esc)
	double v_esc, // Escape velocity (cm/s)
	double mdot, // Mass loss rate (g/s)
	double radius, // Radius (cm)
	double theta, // Co-latitude angle (radians)
	double Teff // Teff (K)
    )
{
    return (mdot * fn_alpha(omega, theta, Teff) * fn_delta(omega, Teff) * pow_fast(1.0 - omega*sin(theta), c_xi)) / (8.0 * pconst.pi() * pow_fast(radius, 2.0) * fn_v_inf(omega, v_esc, theta, Teff));
}


// ##################################################################
// ##################################################################


// Interpolated wind density function
double stellar_wind_angle::fn_density_interp(
	double omega, // Omega (v_rot/v_esc)
	double v_esc, // Escape velocity (cm/s)
	double mdot, // Mass loss rate (g/s)
	double radius, // Radius (cm)
	double theta, // Co-latitude angle (radians)
	double Teff // Teff (K)
    )
{
    //
    // Use tables to interpolate the value of delta
    //

    double delta_interp;

    // Vector for delta interpolation vector sizes
    vector<size_t> delta_vec_size (2);
    delta_vec_size[0] = npts_omega;
    delta_vec_size[1] = npts_Teff;
    
    // Vector for delta input (omega, Teff)
    vector<double> delta_input (2);
    delta_input[0] = omega;
    delta_input[1] = Teff;
    
    delta_interp = root_find_bilinear_vec(omega_vec, Teff_vec, delta_vec, delta_vec_size, delta_input);
    
    
    //
    // Use tables to interpolate the value of alpha
    //

    double alpha_interp;

    // Vector for delta interpolation vector sizes
    vector<size_t> alpha_vec_size (3);
    alpha_vec_size[0] = npts_omega;
    alpha_vec_size[1] = npts_theta;
    alpha_vec_size[2] = npts_Teff;
    
    // Vector for delta input (omega, Teff)
    vector<double> alpha_input (3);
    alpha_input[0] = omega;
    alpha_input[1] = theta;
    alpha_input[2] = Teff;
    
    alpha_interp = root_find_trilinear_vec(omega_vec, theta_vec, Teff_vec, alpha_vec, alpha_vec_size, alpha_input);


    //
    // Return interpolated density
    //
    
    double result = (mdot * alpha_interp * delta_interp * pow_fast(1.0 - omega*sin(theta), c_xi));
    result /= (8.0 * pconst.pi() * pow_fast(radius, 2.0) * fn_v_inf(omega, v_esc, theta, Teff));

    return result;
}


// ##################################################################
// ##################################################################


void stellar_wind_angle::set_wind_cell_reference_state(
        class GridBaseClass *grid,
        struct wind_cell *wc,
        const struct wind_source *WS
        )
{
  //
  // In this function we set the density, pressure, velocity, and tracer values
  // for the reference state of the cell.  Every timestep the cell-values will
  // be reset to this reference state.
  //
  double pp[SimPM.ndim];
  CI.get_dpos(wc->c,pp);
  //rep.printVec("cell pos", pp, SimPM.ndim);
  //cout <<"dist="<<wc->dist<<"\n";

    //
    // 3D geometry, so either 3D-cartesian, 2D-axisymmetry, or 1D-spherical.
    //
    wc->p[RO] = fn_density_interp(pconst.sqrt2()*WS->v_rot/WS->v_esc, WS->v_esc, WS->Mdot, wc->dist, wc->theta, WS->Tw);

    //
    // Set pressure based on wind density/temperature at the stellar radius,
    // assuming adiabatic expansion outside Rstar, and that we don't care what
    // the temperature is inside Rstar (because this function will make it 
    // hotter than Teff, which is not realistic):
    //
    // rho_star = Mdot/(4.pi.R_star^2.v_inf),
    //   p_star = rho_star.k.T_star/(mu.m_p)
    // So then p(r) = p_star (rho(r)/rho_star)^gamma
    // ******************************************************************************
    // *********** WARNING MU=1 HERE, PROBABLY SHOULD BE O.6 (IONISED) 1.3 (NEUTRAL).
    // ******************************************************************************
    //
    wc->p[PG] = WS->Tw*pconst.kB()/pconst.m_p(); // taking mu = 1
    wc->p[PG] *= pow_fast(fn_density_interp(pconst.sqrt2()*WS->v_rot/WS->v_esc, WS->v_esc, WS->Mdot, WS->Rstar, wc->theta, WS->Tw), 1.0-SimPM.gamma);
    wc->p[PG] *= pow_fast(wc->p[RO], SimPM.gamma);


  //
  // NOW VELOCITIES: These should be cell-average values, which are
  // the values at the centre-of-volume, so we call the geometry-aware
  // grid functions.
  //

  // calculate terminal wind velocity
  double Vinf = fn_v_inf(pconst.sqrt2()*WS->v_rot/WS->v_esc, WS->v_esc, wc->theta, WS->Tw);
  //cout <<WS->v_esc<<"\n";

  cell *c = wc->c;

  // Velocity x component
  wc->p[VX] = Vinf*grid->difference_vertex2cell(WS->dpos,c,XX)/wc->dist;

  // Velocity y component (if 2D or 3D)
  if (SimPM.ndim>1)
    wc->p[VY] = Vinf*grid->difference_vertex2cell(WS->dpos,c,YY)/wc->dist;
  else
    wc->p[VY] = 0.0;

  // Velocity z component (if 3D)
  if (SimPM.ndim>2)
    wc->p[VZ] = Vinf*grid->difference_vertex2cell(WS->dpos,c,ZZ)/wc->dist;
  else
    wc->p[VZ] = 0.0;

  if (SimPM.eqntype!=EQEUL && SimPM.eqntype!=EQEUL_EINT)
    rep.error("Need to code B into winds model!",SimPM.eqntype);


  // update tracers
  for (int v=0;v<SimPM.ntracer;v++)
    wc->p[SimPM.ftr+v] = WS->tracers[v];
  //
  // HACK! HACK! HACK! HACK! HACK! HACK! HACK! HACK! HACK! HACK! HACK! HACK!
  // Assume the first tracer variable is the H+ ion fraction, and set it so
  // that it goes from y=1 at T>12500K to y=1.0e-7 at T<10000, with linear
  // interpolation.
  //
#ifdef HACK_WARNING
#error "REMOVE HACK setting ion fraction of H+ in winds"
#endif
  if (SimPM.ntracer>0) {
    if      (WS->Tw >1.25e4)
      wc->p[SimPM.ftr] = 1.0;
    else if (WS->Tw <1.00e4)
      wc->p[SimPM.ftr] = 1.0e-7;
    else
      wc->p[SimPM.ftr] = std::max((WS->Tw-1.0e4)*4e-4,1.0e-7);
  }

//#define TESTING
#ifdef SET_NEGATIVE_PRESSURE_TO_FIXED_TEMPERATURE
  //
  // Set the minimum temperature to be 10K in the wind...
  //
  if (MP) {
    if (MP->Temperature(wc->p,SimPM.gamma) <SimPM.EP.MinTemperature) {
      MP->Set_Temp(wc->p,SimPM.EP.MinTemperature,SimPM.gamma);
    }
  }
  else {
    // appropriate for a neutral medium.
    wc->p[PG] = max(static_cast<double>(wc->p[PG]), 
      SimPM.EP.MinTemperature*wc->p[RO]*pconst.kB()*(1.0-0.75*SimPM.EP.Helium_MassFrac)/pconst.m_p());
  }
#endif // SET_NEGATIVE_PRESSURE_TO_FIXED_TEMPERATURE
#ifdef TESTING
  cout << "\n";
#endif

  return;
}


// ##################################################################
// ##################################################################


int stellar_wind_angle::add_evolving_source(
  const double *pos,        ///< position (physical units).
  const double  rad,        ///< radius (physical units).
  const int    type,        ///< type (must be WINDTYPE_ANGLE).
  const double Rstar,       ///< Radius at which to get gas pressure from Teff
  const pion_flt *trv,        ///< Any (constant) wind tracer values.
  const string infile,      ///< file name to read data from.
  const double time_offset, ///< time offset = [t(sim)-t(wind_file)] (seconds)
  const double t_now,       ///< current simulation time, to see if src is active.
  const double update_freq, ///< frequency with which to update wind properties (seconds).
  const double t_scalefactor ///< wind evolves this factor times faster than normal
  )
{
  if (type != WINDTYPE_ANGLE) {
    rep.error("Bad wind type for evolving stellar wind (rotating star)!",type);
  }
  //
  // First we will read the file, and see when the source should
  // switch on in the simulation (it may not be needed for a while).
  //
#ifdef TESTING
  cout <<"\t\tsw-evo: adding source from file "<<infile<<"\n";
#endif

  //
  // Read in stellar evolution data
  // Format: time	M	L	Teff	Mdot	vrot
  //
  FILE *wf = 0;
  wf = fopen(infile.c_str(), "r");
  if (!wf) rep.error("can't open wind file, stellar_wind_angle",wf);
  // Skip first two lines
  char line[512];
  fgets(line,512,wf);
  //printf("%s",line);
  fgets(line,512,wf);
  //printf("%s",line);

  // Temp. variables for column values
  double t1=0.0, t2=0.0, t3=0.0, t4=0.0, t5=0.0, t6=0.0;
  while (fscanf(wf, "   %lE   %lE %lE %lE %lE %lE", &t1, &t2, &t3, &t4, &t5, &t6) != EOF){
    //cout.precision(16);
    //cout <<t1 <<"  "<<t2  <<"  "<< t3  <<"  "<< t4 <<"  "<< t5 <<"  "<< t6 <<"\n";
    // Set vector value
    time_evo.push_back(t1);
    M_evo.push_back(t2);
    L_evo.push_back(t3);
    Teff_evo.push_back(t4);
    Mdot_evo.push_back(t5);
    vrot_evo.push_back(t6);

    // Stellar radius
    t6 = sqrt( t3/ (4.0*pconst.pi()*pconst.StefanBoltzmannConst()*pow_fast(t4, 4.0)));
    
	// Hydrogen mass fraction (should make this a sim parameter?) 
	double H_X = 0.7;

	// Eddington luminosity (taking the opacity as the electron scattering cross section)
	double L_edd = (4.0*pconst.pi()*pconst.c()*pconst.G()*t2)/(0.2*(1 + H_X));

    // Escape velocity
    vesc_evo.push_back(sqrt(2.0*pconst.G()*t2*(1 - t3/L_edd)/t6));
    //cout <<t6<<"  "<<t4<<"  "<<t3<<"  "<<sqrt(2.0*pconst.G()*t2/t6)<<"\n";
    //rep.error("test",2);
  }
  fclose(wf);

  // Column length
  size_t Npt = time_evo.size();

  //
  // Next we set up the interpolation, first modifying the
  // time array using the time-offset so that it has the same zero
  // offset as the simulation time.
  //
  for (size_t i=0; i<Npt; i++) {
    //
    // times in the file are measured in seconds, so offset should be
    // in years.
    //
    time_evo[i] += time_offset;
    // scale times by scale factor.
    time_evo[i] /= t_scalefactor;
  }

  //
  // Some properties of the wind source are specific to this module,
  // such as the number of points in
  // the array, and the timings (offset, update frequency).  
  // They are stored in local data.
  //
  struct evolving_wind_data *temp=0;
  temp = mem.myalloc(temp,1);
  temp->Npt = Npt;
  temp->t = 0;
  temp->mdot = 0;
  temp->vinf = 0;
  temp->Teff = 0;

  //
  // Offset is not used in the code past here.  It's just here for I/O b/c a
  // restart will need to read the data-file again.
  // We reset all variables to be in seconds here.  So the interpolation uses
  // years, but everything else is in seconds.  Note that the global SWP struct
  // still has times in years though.
  //
  temp->offset = time_offset/t_scalefactor; // now in seconds
  temp->tstart = time_evo[0];            // in seconds (already scaled)
  temp->tfinish= time_evo[Npt-1];        // in seconds (already scaled)
  temp->update_freq = update_freq/t_scalefactor; // in seconds
  temp->t_next_update = max(temp->tstart,t_now);
#ifdef TESTING
  cout <<"\t\t tstart="<<temp->tstart;
  cout <<", next update="<<temp->t_next_update;
  cout <<", and tfinish="<<temp->tfinish<<"\n";
#endif

  //
  // We need to decide if the wind src is active yet.  If it is, then
  // we also set up a constant wind source for updating its
  // properties.  We set it to be active if the current time is
  // within update_freq of tstart.
  //
  double mdot=0.0, vesc=0.0, Twind=0.0, vrot=0.0;
  if ( ((t_now+temp->update_freq)>temp->tstart ||
        pconst.equalD(temp->tstart, t_now))
       && t_now<temp->tfinish) {
    temp->is_active = true;
    //
    // Get the current values for mdot, vinf, Teff, and setup a wind
    // source using the constant-wind function.
    //
    interpolate.root_find_linear_vec(time_evo, Teff_evo, t_now, Twind);
    interpolate.root_find_linear_vec(time_evo, Mdot_evo, t_now, mdot);
    interpolate.root_find_linear_vec(time_evo, vesc_evo, t_now, vesc);
    interpolate.root_find_linear_vec(time_evo, vrot_evo, t_now, vrot);
#ifdef TESTING
    cout <<"Source is Active\n";
#endif
  }
  else {
    cout <<"WARNING: Source is not yet active: tnow="<<t_now;
    cout <<", tstart=";
    cout <<temp->tstart<<". Setting wind source to INACTIVE.\n";
    temp->is_active = false;
    mdot=-100.0; vesc=-100.0; vrot=-100.0; Twind=-100.0;
  }

  //
  // Now add source using rotating star version.
  //
  add_rotating_source(pos,rad,type,mdot, vesc, vrot,Twind,Rstar,trv);
  temp->ws = wlist.back();

  //
  // So now we have all of the properties of the wind source in the
  // struct evolving_wind_data 'temp', and we have added it to the
  // list of constant wind sources, wlist[].  We can now add temp to
  // the list of evolving wind sources and return (so that wlist[i]
  // and wdata_evol[i] point to the same source).
  //
  wdata_evol.push_back(temp);
  //NSRC_TOTAL++;

  return 0;
}



// ##################################################################
// ##################################################################


int stellar_wind_angle::add_rotating_source(
      const double *pos, ///< position (cm from grid origin)
      const double rad,   ///< radius (cm)
      const int type,      ///< type (2=lat-dep.)
      const double mdot,   ///< Mdot (g/s)
      const double vesc,   ///< Vesc (cm/s)
      const double vrot,   ///< Vrot (cm/s)
      const double Twind,   ///< Wind Temperature (p_g.m_p/(rho.k_b))
      const double Rstar,   ///< Radius where T=Twind (to get gas pressure)
      const pion_flt *trv  ///< Tracer values of wind (if any)
      )
{
  struct wind_source *ws = 0;
  ws = mem.myalloc(ws,1);
  ws->id = wlist.size();
  ws->ncell = 0;
  ws->type = type;
  switch (type) {
  case WINDTYPE_ANGLE:
    cout <<"\tAdding rotating wind source as id="<<ws->id<<"\n";
    break;
  default:
    rep.error("What type of source is this?  add a new type?",type);
    break;
  }

  for (int v=0;v<SimPM.ndim;v++)
    ws->dpos[v] = pos[v];
  rep.printVec("ws->dpos",ws->dpos,SimPM.ndim);

  for (int v=SimPM.ndim;v<MAX_DIM;v++)
    ws->dpos[v] = VERY_LARGE_VALUE;

  ws->radius = rad;

  // all inputs in cgs units.
  ws->Mdot  = mdot;
  ws->v_esc = vesc;
  ws->Vinf  = vesc;
  ws->v_rot = vrot;

  ws->Tw    = Twind;
  ws->Rstar = Rstar;

  ws->tracers=0;
  ws->tracers = mem.myalloc(ws->tracers,SimPM.ntracer);
  for (int v=0;v<SimPM.ntracer; v++) {
    ws->tracers[v] = trv[v];
    cout <<"ws->tracers[v] = "<<ws->tracers[v]<<"\n";
  }
    
  ws->cells_added = false;
  if (!ws->wcells.empty())
    rep.error("wind_source: wcells not empty!",ws->wcells.size());

  //
  // Make sure the source position is compatible with the geometry:
  //
  if (SimPM.coord_sys==COORD_SPH) {
    if (!pconst.equalD(ws->dpos[Rsph],0.0))
      rep.error("Spherical symmetry but source not at origin!",
                ws->dpos[Rsph]);
  }
  if (SimPM.coord_sys==COORD_CYL && SimPM.ndim==2) {
    //
    // Axisymmetry
    //
    if (!pconst.equalD(ws->dpos[Rcyl],0.0))
      rep.error("Axisymmetry but source not at R=0!",ws->dpos[Rcyl]);
  }

  wlist.push_back(ws);
  nsrc++;
#ifdef TESTING
  cout <<"\tAdded wind source id="<<nsrc-1<<" to list of ";
  cout <<nsrc<<" elements.\n";
#endif // TESTING
  return ws->id;
}


// ##################################################################
// ##################################################################



void stellar_wind_angle::update_source(
        class GridBaseClass *grid,
        struct evolving_wind_data *wd,
        const double t_now
        )
{
  //
  // We have a source that needs updating.  If it is not active, and
  // needs activating then we set that.
  //
  if (!wd->is_active) {
    cout <<"stellar_wind_angle::update_source() activating source id=";
    cout << wd->ws->id <<" at Simulation time t="<<t_now<<"\n";
    rep.printVec("Source position",wd->ws->dpos,SimPM.ndim);
    wd->is_active=true;
  }

  if (t_now < wd->tstart) {
    rep.error("Requested updating source, but it hasn't switched on yet!",wd->tstart-t_now);
  }

  wd->t_next_update = t_now+SimPM.dt;
  wd->t_next_update = min(wd->t_next_update, wd->tfinish);
  
  //
  // Now we update Mdot, Vinf, Teff by linear interpolation.
  //
  double mdot=0.0, vesc=0.0, Twind=0.0, vrot=0.0;
  interpolate.root_find_linear_vec(time_evo, Teff_evo, t_now, Twind);
  interpolate.root_find_linear_vec(time_evo, Mdot_evo, t_now, mdot);
  interpolate.root_find_linear_vec(time_evo, vesc_evo, t_now, vesc);
  interpolate.root_find_linear_vec(time_evo, vrot_evo, t_now, vrot);
  //
  // Assign new values to wd->ws (the wind source struct), converting
  // from log10 to actual values, and also unit conversions to cgs.
  //
  wd->ws->Mdot = mdot;  // already cgs.
  wd->ws->v_esc = vesc;  // this is in cm/s already.
  wd->ws->Vinf = vesc;  // this is in cm/s already.
  wd->ws->v_rot = vrot;  // this is in cm/s already.
  wd->ws->Tw   = Twind; // This is in K.

  //
  // Now re-assign state vector of each wind-boundary-cell with
  // updated values.
  //
  for (int i=0; i<wd->ws->ncell; i++) {
    set_wind_cell_reference_state(grid,wd->ws->wcells[i],wd->ws);
  }

  //
  // Now the source is updated, and the reference states are all set
  // for the new values, and the next update time has been set.  So
  // we can return.
  //
  return;
}


