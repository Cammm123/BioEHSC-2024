#include "PDB.hpp"
#include "ZDOCK.hpp"
#include <memory>

namespace zdock {

class Pruning {
private:
  zdock::Structure receptor_, ligand_;   // zdock metadata
  std::unique_ptr<PDB> recpdb_, ligpdb_; // pdb file data
  ZDOCK zdock_;                          // zdock output
  double spacing_;                       // grid spacing
  int boxsize_;                          // grid size
  bool rev_, fixed_;                     // fixed / switched flags

  // precomputed transformation matrices
  Eigen::Transform<double, 3, Eigen::Affine> t0_, t1_, t2_;

  // Euler angles to Z-X-Z transformation matrix
  inline const Eigen::Transform<double, 3, Eigen::Affine>
  eulerRotation(const double (&r)[3], bool rev = false) const {
    Eigen::Transform<double, 3, Eigen::Affine> t;
    t = Eigen::AngleAxisd(r[0], Eigen::Vector3d::UnitZ()) *
        Eigen::AngleAxisd(r[1], Eigen::Vector3d::UnitX()) *
        Eigen::AngleAxisd(r[2], Eigen::Vector3d::UnitZ());
    return (rev ? t.inverse() : t);
  }

  // grid to actual translation ('circularized')
  inline const Eigen::Transform<double, 3, Eigen::Affine>
  boxTranslation(const int (&t)[3], bool rev = false) const {
    Eigen::Transform<double, 3, Eigen::Affine> ret;
    Eigen::Vector3d d;
    d << (t[0] >= boxsize_ / 2 ? t[0] - boxsize_ : t[0]),
        (t[1] >= boxsize_ / 2 ? t[1] - boxsize_ : t[1]),
        (t[2] >= boxsize_ / 2 ? t[2] - boxsize_ : t[2]);
    if (rev) {
      ret = Eigen::Translation3d(-spacing_ * d);
    } else {
      ret = Eigen::Translation3d(spacing_ * d);
    }
    return ret;
  }

public:
  Pruning(const std::string &zdockouput,
          const std::string &receptorpdb = "", // or grab from zdock.out
          const std::string &ligandpdb = ""    // or grab from zdock.out
  );

  // perform pruning
  void prune(const double cutoff);

  // calculate rmsd between two predictions
  double rmsd(const Prediction p1, const Prediction p2,
              const double ligsize = 1.0) const;

  // ligand pdb record to stdout
  void makeComplex(const size_t n);

  // perform actual ligand transformation
  inline const Eigen::Matrix<double, 3, Eigen::Dynamic>
  txLigand(const PDB &pdb, const Prediction &pred) const {
    Eigen::Transform<double, 3, Eigen::Affine> t;
    if (rev_) { // reverse (receptor was rotated, rather than ligand)
      t = eulerRotation(pred.rotation, true) * boxTranslation(pred.translation);
      return t1_ * t * t0_ * pdb.matrix();
    } else { // 'normal'
      t = Eigen::Translation3d(Eigen::Vector3d(receptor_.translation)) *
          boxTranslation(pred.translation, true) *
          eulerRotation(pred.rotation) * t2_;
      if (!fixed_) {
        t = eulerRotation(receptor_.rotation, true) * t;
      }
      return t * pdb.matrix();
    }
  }
};

} // namespace zdock