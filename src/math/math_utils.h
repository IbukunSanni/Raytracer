#ifndef RAYTRACER_SRC_MATH_MATH_UTILS_H_
#define RAYTRACER_SRC_MATH_MATH_UTILS_H_

const double kPi = 3.14159265;

//---------------------------------------------------------------------
template <typename T>
inline T DegreesToRadians(T angle) {
  return angle * T(kPi) / T(180.0);
}

//---------------------------------------------------------------------
template <typename T>
inline T RadiansToDegrees(T angle) {
  return angle * T(180.0) / T(kPi);
}

#endif  // RAYTRACER_SRC_MATH_MATH_UTILS_H_
