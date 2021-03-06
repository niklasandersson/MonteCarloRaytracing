#ifndef RAY_H
#define RAY_H

#include "glm/glm.hpp"


#define EPSILON 0.000001


class Ray {

public:
  Ray(const glm::vec3 origin, const glm::vec3 direction);

  glm::vec3 getOrigin() const;
  glm::vec3 getDirection() const;
  glm::vec3 getInversedDirection() const;
  glm::vec3 getAsVector() const { return glm::normalize(origin_ + direction_); }
  glm::vec2 getAngles() const;

protected:

private:
  const glm::vec3 origin_;
  const glm::vec3 direction_;
  const glm::vec3 inversedDirection_;

};


#endif // RAY_H
