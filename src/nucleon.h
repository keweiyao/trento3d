// TRENTO: Reduced Thickness Event-by-event Nuclear Topology
// Copyright 2015 Jonah E. Bernhard, J. Scott Moreland
// MIT License

#ifndef NUCLEON_H
#define NUCLEON_H

#include <boost/math/constants/constants.hpp>

#include "fast_exp.h"
#include "fwd_decl.h"
#include "random.h"

// >>> Weiyao, include random field generator
#include "rft2d.h"
// <<< Weiyao

namespace trento {

class Nucleon;

/// \rst
/// Encapsulates properties shared by all nucleons: transverse thickness
/// profile, cross section, fluctuations.  Responsible for sampling
/// nucleon-nucleon participation with given `\sigma_{NN}`.
/// \endrst
class NucleonProfile {
 public:
  /// Instantiate from the configuration.
  explicit NucleonProfile(const VarMap& var_map);

  /// The radius at which the nucleon profile is truncated.
  double radius() const;

  /// The maximum impact parameter for participation.
  double max_impact() const;

  /// Randomly fluctuate the profile.  Should be called prior to evaluating the
  /// thickness function for a new nucleon.
  void fluctuate();

  /// Compute the thickness function at a (squared) distance from the profile
  /// center.
  double thickness(double distance_sqr) const;

  /// Randomly determine if a pair of nucleons participates.
  bool participate(Nucleon& A, Nucleon& B) const;

  // >>> Weiyao
  double substructure(int ic, int jc, int i, int j) const;
  // <<< Weiyao
  

 private:
  // >>> Weiyao
  /// internal 2d random field generator and field configuration
  RFT::rft2d field_generator;
  // <<< Weiyao

  /// Width of Gaussian thickness function.
  const double width_sqr_;

  /// Truncate the Gaussian at this radius.
  const double trunc_radius_sqr_;

  /// Maximum impact parameter for participants.
  const double max_impact_sqr_;

  /// Cache (-1/2w^2) for use in the thickness function exponential.
  /// Yes, this actually makes a speed difference...
  const double neg_one_div_two_width_sqr_;

  /// Dimensionless parameter set to reproduce the inelastic nucleon-nucleon
  /// cross section \sigma_{NN}.  Calculated in constructor.
  const double cross_sec_param_;

  /// Fast exponential for calculating the thickness profile.
  const FastExp<double> fast_exp_;

  /// Fluctuation distribution.
  std::gamma_distribution<double> fluct_dist_;

  /// Thickness function prefactor = fluct/(2*pi*w^2)
  double prefactor_;
};

/// \rst
/// Represents a single nucleon.  Stores its transverse position and whether or
/// not it's a participant.  These properties are globally readable, but can
/// only be set through ``Nucleus`` and ``NucleonProfile``.
/// \endrst
class Nucleon {
 public:
  /// Only a default constructor is necessary\---the class is designed to be
  /// constructed once and repeatedly updated.
  Nucleon() = default;

  /// The transverse \em x position.
  double x() const;

  /// The transverse \em y position.
  double y() const;

  // >>> Weiyao
  /// random field indices
  int fi() const;
  int fj() const;
  // <<< Weiyao

  /// Whether or not this nucleon is a participant.
  bool is_participant() const;

 private:
  /// A Nucleus must be able to set its Nucleon positions.
  friend class Nucleus;

  /// The NucleonProfile samples participants so must be able to set
  /// participation status.
  friend bool NucleonProfile::participate(Nucleon&, Nucleon&) const;

  /// Set the transverse position and reset participant status to false.
  void set_position(double x, double y);

  /// Mark as a participant.
  void set_participant();

  /// Internal storage of the transverse position.
  double x_, y_;

  // >>> Weiyao
  /// Internal storage of the the center point of gaussian random field patch
  int fi_, fj_;
  // <<< Weiyao

  /// Internal storage of participant status.
  bool participant_;
};

// These functions are short, called very often, and account for a large
// fraction of the total computation time, so request inlining.

// Nucleon inline member functions

inline double Nucleon::x() const {
  return x_;
}

inline double Nucleon::y() const {
  return y_;
}

inline int Nucleon::fi() const {
  return fi_;
}

inline int Nucleon::fj() const {
  return fj_;
}

inline bool Nucleon::is_participant() const {
  return participant_;
}

inline void Nucleon::set_position(double x, double y) {
  x_ = x;
  y_ = y;
  participant_ = false;
  // >>> Weiyao, Set random field patch indices
  fi_ = random::engine()%1950+25;
  fj_ = random::engine()%1950+25;
  // <<< Weiyao
}

inline void Nucleon::set_participant() {
  participant_ = true;
}

// NucleonProfile inline member functions

inline double NucleonProfile::radius() const {
  return std::sqrt(trunc_radius_sqr_);
}

inline double NucleonProfile::max_impact() const {
  return std::sqrt(max_impact_sqr_);
}

inline void NucleonProfile::fluctuate() {
  prefactor_ = fluct_dist_(random::engine) *
     math::double_constants::one_div_two_pi / width_sqr_;
}

inline double NucleonProfile::thickness(double distance_sqr) const {
  if (distance_sqr > trunc_radius_sqr_)
    return 0.;
  return prefactor_ * fast_exp_(neg_one_div_two_width_sqr_*distance_sqr);
}

inline double NucleonProfile::substructure(int ic, int jc, int i, int j) const {
	return field_generator.get_field(ic+i, jc+j);
}

inline bool NucleonProfile::participate(Nucleon& A, Nucleon& B) const {
  // If both nucleons are already participants, there's nothing to do.
  if (A.is_participant() && B.is_participant())
    return true;

  double dx = A.x() - B.x();
  double dy = A.y() - B.y();
  double distance_sqr = dx*dx + dy*dy;

  // Check if nucleons are out of range.
  if (distance_sqr > max_impact_sqr_)
    return false;

  // The probability is
  //   P = 1 - exp(...)
  // which we could sample as
  //   P > U
  // where U is a standard uniform (0, 1) random number.  We can also compute
  //   1 - P = exp(...)
  // and then sample
  //   (1 - P) > (1 - U)
  // or equivalently
  //   (1 - P) < U

  // >>> Weiyao
  // 1. To change to flucutating proton, the cross_sec_param_ computing function should be modified
  // 2. The 1-P will be changed to 
  // exp( - Kf * exp(cross_param - 0.25*b^2/w^2))
  // Kf = sum(fA*fB*exp(-(x^2+y^2)/w^2))*dx*dy/pi/w^2
  double Kf = field_generator.calculate_fluct_norm(A.fi(), A.fj(), B.fi(), B.fj(), dx, dy);
  auto one_minus_prob = std::exp(
      -Kf*std::exp(cross_sec_param_ - .25*distance_sqr/width_sqr_));

  // Sample one random number and decide if this pair participates.
  if (one_minus_prob < random::canonical<double>()) {
    A.set_participant();
    B.set_participant();
    return true;
  }

  return false;
}

}  // namespace trento

#endif  // NUCLEON_H
