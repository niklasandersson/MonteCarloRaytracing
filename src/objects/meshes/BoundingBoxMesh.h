#ifndef BOUNDINGBOXMESH_H
#define BOUNDINGBOXMESH_H

#include <algorithm>

#include "glm/glm.hpp"

#include "BoxMesh.h"

class BoundingBoxMesh : public BoxMesh {

public:
  using BoxMesh::BoxMesh;

  glm::vec3 getNormal(const glm::vec3& position) const override;
};


#endif
