/*
 * Triplet.hh
 *
 * Template implementation of the Triplet abstract data type.
 * The class also provides basic three-dimensional vector operations.
 */

#include <array>
#include <cstddef>
#include <algorithm>

/**
 * A generic three-element tuple.
 *
 * In this experiment, Triplet is also treated as a three-dimensional
 * vector, supporting vector addition, subtraction, scalar multiplication
 * and dot product.
 *
 * T is the element type of the triplet.
 */
template<typename T>
class Triplet {
public:
  Triplet (T a, T b, T c) : data_{a, b, c} {}

  T &operator[] (std::size_t i) {
    return data_[i];
  }

  const T &operator[] (std::size_t i) const
  {
    return data_[i];
  }

  Triplet operator+ (const Triplet &rhs) const
  {
    return Triplet (data_[0] + rhs.data_[0],
                    data_[1] + rhs.data_[1],
                    data_[2] + rhs.data_[2]);
  }

  Triplet operator- (const Triplet &rhs) const
  {
    return Triplet (data_[0] - rhs.data_[0],
                    data_[1] - rhs.data_[1],
                    data_[2] - rhs.data_[2]);
  }

  /* Scalar multiplication: vector * scalar.  */
  Triplet operator* (const T& scalar)  const
  {
    return Triplet(
                   data_[0] * scalar,
                   data_[1] * scalar,
                   data_[2] * scalar);
  }

  /* Scalar multiplication: scalar * vector.  */
  friend Triplet
  operator* (const T &scalar, const Triplet &v) {
    return v * scalar;
  }

  /* Dot product of two three-dimensional vectors.  */
  T operator* (const Triplet &rhs) const {
    return data_[0] * rhs.data_[0]
       + data_[1] * rhs.data_[1]
       + data_[2] * rhs.data_[2];
  }

  T max () const {
    return *std::max_element (data_.begin (), data_.end ());
  }

  T min() const {
    return *std::min_element( data_.begin (), data_.end ());
  }

  /* Return true if the elements are in nondecreasing order.  */
  bool is_ascending() const {
    return data_[0] <= data_[1] && data_[1] <= data_[2];
  }

  /* Return true if the elements are in nonincreasing order.  */
  bool is_descending() const {
    return data_[0] >= data_[1] && data_[1] >= data_[2];
  }
  
private:
  std::array<T, 3> data_;  
};
