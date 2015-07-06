///
/// \file eqns_mhd_adiabatic.h
/// \author Jonathan Mackey
/// Declaration of the adiabatic magnetohydrodynamics equations and flux solver classes
///
/// History:
///
/// - 2009-10-20 Started on the file (moved from old equations.h)
///
/// - 2010.12.23 JM: Added SetAvgState() function to eqns_mhd_ideal class.
///
#ifndef EQNS_MHD_ADIABATIC_H
#define EQNS_MHD_ADIABATIC_H


#include "defines/functionality_flags.h"
#include "defines/testing_flags.h"


#include "eqns_base.h"

/// Class describing the equations of Ideal MHD
///This has global scope, so every code I write that uses
///the MHD equations can call the functions in this class.
///
//class mhd_idealEqn : public EulerEqn {
class eqns_mhd_ideal : virtual public eqns_base {
  public:
   eqns_mhd_ideal(int);
   ~eqns_mhd_ideal();
   
   /// \brief Converts from primitive to conserved variables.   
   virtual void PtoU(const double *, ///< pointer to Primitive variables.
		    double *,       ///< pointer to conserved variables.
		    const double    ///< Gas constant gamma.
		    );
   
   /// \brief convert from conserved to primitive variables. 
   virtual int UtoP(const double*, ///< pointer to conserved variables.
		    double*,       ///< pointer to Primitive variables.
		    const double   ///< Gas constant gamma.
		    );
   
   /// \brief Converts from primitive and conserved variables to corresponding flux.
   ///
   ///This assumes that the direction has been set correctly.
   ///
   virtual void PUtoFlux(const double *, ///< pointer to Primitive variables.
		const double *,         ///< pointer to conserved variables.
		double *               ///< Pointer to flux variable.
		);
   
   /// \brief Converts from conserved variables to flux. 
   virtual void UtoFlux(const double*, ///< Pointer to conserved variables state vector.
		       double*,       ///< Pointer to flux variable state vector.
		       const double   ///< Gas constant gamma.
		       );

   ///
   /// Hydro sound speed sqrt(gamma*P_g/rho)
   ///
   double chydro(const double *, ///< Pointer to primitive variables.
		 const double   ///< Gas constant gamma.
		 );

   ///
   /// \brief Calculate fast magnetic wavespeed in currently set direction.
   /// \retval cf(>0) Success
   /// \retval -1     Failure
   ///
   double cfast(const double*, ///< Pointer to primitive variables.
		const double   ///< Gas constant gamma.
		);
   /// \brief Calculate fast magnetosonic wavespeed from components of state vector. 
   double cfast_components(const double, ///< density
			   const double, ///< pressure
			   const double, ///< X-component of B-field
			   const double, ///< By
			   const double, ///< Bz
			   const double  ///< gas constant gamma.
			   );
   
   /// \brief Calculate slow magnetic wavespeed in currently set direction.
   /// \retval cs(>0) Success
   /// \retval -1     Failure
    ///
   double cslow(const double*, ///< Pointer to primitive variables.
		const double    ///< Gas constant gamma.
		);
   
   /// \brief Returns the fastest wavespeed for the relevant equations.
   /// 
   /// For eqns_mhd_ideal it returns the fast speed in the currently set 
   /// direction.  Note this is not necessarily the fastest wavespeed
   /// in any direction, as the fast speed is greatest perpendicular to
   /// the magnetic field.
   /// 
   inline double maxspeed(const double *p, ///< Pointer to primitive variables.
			  const double g   ///< Gas constant gamma.
			  ) {return(cfast(p,g));}
   

   /// \brief Rearranges a vector for a rotation by pi/2
   /// rotates the two vectors of velocity and b-field.
    ///
   void rotate(double *, ///< State vector
	       enum axes, ///< Initial orientation.
	       enum axes  ///< Final Orientation.
	       );
   /// \brief Rotates a state vector through theta radians.
   /// Note this is equivalent to rotating the axes through -theta radians.
   /// The rotation is happening in the x-y plane, so if it is called in a 
   /// 3d setting it is equivalent to a rotation about the z-axis.  This
   /// version rotates the fluid velocity and b-field components.
   /// 
   void rotateXY(double *, ///< State vector
	       double    ///< angle to rotate through in 2d plane.
	       );

   ///  Returns Internal Energy (per unit mass, so 'Temperature'), given primitive variable vector. 
   virtual double eint(const double *, ///< Primitive State Vector.
		       const double    ///< gas EOS gamma.
		       );
   ///  Returns Enthalpy (per unit mass), given primitive variable vector. 
   virtual double Enthalpy(const double *, ///< State Vector.
			   const double ///< gas EOS gamma.
			   ) {rep.error("ENTHALPY FOR Ideal MHD EQNS",456); return 456.789;}
   /// \brief Returns Total Energy (per unit volume), given primitive variable vector. 
   virtual double Etot(const double *, ///< State Vector.
		       const double    ///< gas EOS gamma.
		       );

   /// \brief Returns Total Pressure (per unit Volume), given primitive variable vector. 
   virtual double Ptot(const double *, ///< Primitive State Vector.
		       const double    ///< gas EOS gamma.
		       );

   /// \brief Given a pressure ratio and initial density, calculate adiabatic final density.
   virtual double AdiabaticRho(const double, ///< New to Old pressure ratio
			       const double, ///< Old Density
			       const double  ///< gas EOS gamma.
			       );

  ///
  /// Set Values for mean velocity, pressure, density, B-field.
  ///
  virtual void SetAvgState(const double *, ///< Mean Primitive var. state vector
			   const double ///< Gas constant gamma.
			   );
  protected:
};


/// \brief Ideal MHD Equations with the GLM method for divergence cleaning. 
/// 
/// \section References
/// Dedner et al., 2002, J.C.P., 175, 645.
///
class eqns_mhd_mixedGLM : virtual public eqns_mhd_ideal
{
  public:
   eqns_mhd_mixedGLM(int);
   ~eqns_mhd_mixedGLM();
   /// \brief Sets the hyperbolic wavespeed ch for the Psi variable. 
   /// 
   /// \f[ c_{\mbox{hyp}} = \mbox{CFL} (dx/dt) \;.\f]
   /// From Dedner, below eq.41.
    ///
   void GLMsetPsiSpeed(const double, ///< CFL coefficient.
		       const double, ///< dx, the cell size
		       const double  ///< dt, the timestep.
		       );
   
   /// \brief Converts from primitive to conserved variables. 
   /// 
   /// Psi conserved variable is same as primitive, so just call 
   /// the mhd_ideal function and copy Psi into the conserved variable.
   ///
   void PtoU(const double *, ///< pointer to Primitive variables.
	     double *,       ///< pointer to conserved variables.
	     const double    ///< Gas constant gamma.
	     );
   
   /// \brief convert from conserved to primitive variables.
   /// 
   /// Psi conserved variable is same as primitive, so just call 
   /// the eqns_mhd_ideal function and copy Psi into the primitive variable.
   /// 
   int UtoP(const double*, ///< pointer to conserved variables.
	    double*, ///< pointer to Primitive variables.
	    const double    ///< Gas constant gamma.
	    );
   
   /// \brief Calculates the source term contribution to updating Psi
   /// 
   /// The source function is calculated separately via operator-splitting.
   /// Uses Dedner et al., eq.45:
   /// \f[ \psi^{n+1} = \exp(-\delta t_n c_h^2/c_p^2)\, \psi^{n*} \f]
   /// solved from the differential equation 
   /// \f$ \partial\psi/\partial t = -(c_h^2/c_p^2)\psi \f$..  I am 
   /// using Dedner's variable \f$ c_r = c_p^2/c_h \f$ in place of \f$c_p\f$.
   /// 
   void GLMsource(double *, ///< Primitive Psi variable.
		  const double ///< timestep
		  );

  protected:
   enum primitive eqSI;
   enum conserved eqPSI;
   double GLM_chyp; ///< Hyperbolic Wave speed of Psi, the GLM variable.
   double GLM_cr;   ///< Parameter (below eq.46 in Dedner) 
              ///< \f$ c_r = c_{\mbox{par}}^2/c_{\mbox{hyp}} \f$.
};

#endif // EQNS_MHD_ADIABATIC_H
