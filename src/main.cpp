#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <tuple>
#include <limits>
#include <sstream>
#include <fstream>

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtx/rotate_vector.hpp"

#include "Camera.h"
#include "Scene.h"
#include "Ray.h"
#include "objects/meshes/SphereMesh.h"
#include "objects/meshes/BoxMesh.h"
#include "objects/meshes/BoundingBoxMesh.h"
#include "objects/meshes/OrtPlaneMesh.h"
#include "objects/meshes/TriangleMesh.h"
#include "objects/OpaqueObject.h"
#include "objects/TransparentObject.h"

#include "objects/brdfs/BrdfLambertian.h"
#include "objects/brdfs/BrdfOrenNayar.h"

#include "thread/ThreadPool.h"
#include "thread/WorkItem.h"

#include "utils/Node.h"
#include "utils/random.h"
#include "utils/math.h"
#include "utils/image.h"
#include "utils/lightning.h"

#include "parser/Parser.h"
#include "parser/OBJParser.h"
#include "parser/Config.h"


Scene createScene() {
  Scene scene;

  OpaqueObject* boundingBox = new OpaqueObject{"boundingBox", new BoundingBoxMesh{glm::vec2{-10, 10}, glm::vec2{-10, 10}, glm::vec2{-10, 10}},
                                              new BrdfLambertian{1.0f},
                                              false,
                                               glm::vec3{1.0f, 1.0f, 1.0f}}; 
  scene.add(boundingBox);

  OpaqueObject* lightPlane3 = new OpaqueObject{"lightPlane3", new OrtPlaneMesh{glm::vec3{2,9, -2+6}, // upperLeftCorner
                                                                glm::vec3{2, 9, 2+6},                // lowerLeftCorner 
                                                                glm::vec3{-2, 9, 2+6}},              // lowerRightCorner
                                               new BrdfLambertian{1.0f},
                                               true,
                                               4.0f * glm::vec3{1.0f, 1.0f, 1.0f}};
  scene.add(lightPlane3);

  OpaqueObject* box2 = new OpaqueObject{"box2", new BoxMesh{glm::vec2{-9, -4}, glm::vec2{-10, 5}, glm::vec2{7, 9.5}},
                                        new BrdfOrenNayar{0.8f, 0.5f}, 
                                        false,
                                         glm::vec3{0.1f, 0.12f, 1.0f}};

  OpaqueObject* box1 = new OpaqueObject{"box1", new BoxMesh{glm::vec2{3, 9}, glm::vec2{-10, -8.5}, glm::vec2{0, 6}},
                                        new BrdfLambertian{1.0f}, 
                                        false,
                                        glm::vec3{(float)7/(float)255, (float)255/(float)255, (float)255/(float)255}};
  scene.add(box1);
  scene.add(box2);

  TransparentObject* sphere3 = new TransparentObject{"sphere3", new SphereMesh{glm::vec3{-4.5, -5.5f, 3.0f}, 4.5f}, // lowerRightCorner
                                           1.1f, 0.98f};
  scene.add(sphere3);

  OpaqueObject* sphere5 = new OpaqueObject{"sphere5", new SphereMesh{glm::vec3{8.8f, 2.0f, 1.0f}, 0.5f}, // lowerRightCorner
                                           new BrdfLambertian{1.0f},
                                           true,
                                           4.0f * glm::vec3{1.0f, 1.0f, 1.0f}};
  scene.add(sphere5);

  TransparentObject* sphere6 = new TransparentObject{"sphere6", new SphereMesh{glm::vec3{6.0f, -6.9f+1.5f, 3.0f}, 3.0f}, // lowerRightCorner
                                           1.0f, 0.0f};
  scene.add(sphere6);


  OBJParser<Parser> objParser;
  std::vector<float> vertices;
  std::vector<float> normals;
  std::vector<unsigned short> indices;
  // objParser.parseFile("assets/monkey.obj");
  // objParser.parseFile("assets/bunny.obj");
  // objParser.parseFile("assets/sphere.obj");
  objParser.parseFile("assets/diamond.obj");
  // objParser.parseFile("assets/test2.obj");
  objParser.postProcessing();
  vertices = objParser.getVertices();
  normals = objParser.getNormals();
  indices = objParser.getVertexIndices();
  std::vector<glm::vec3> objVertices;
  std::vector<glm::vec3> objNormals;
  for(unsigned int i=0; i<indices.size(); i++) {
    const unsigned int index = 3 * indices[i];
    glm::vec4 vert = (glm::vec4{vertices[index], vertices[index+1], vertices[index+2], 1.0f});
    glm::vec4 normal = (glm::vec4{normals[index], normals[index+1], normals[index+2], 1.0f});
    glm::vec3 translation{-5.12f, -10.0f, -3.0f};
    float scale = 2.0f;
    objVertices.push_back(glm::vec3{vert.x, vert.y, vert.z}*scale + translation);
    objNormals.push_back(glm::normalize(glm::vec3{normal.x, normal.y, normal.z}));
  }
  TransparentObject* diamond = new TransparentObject{"diamond", new TriangleMesh{objVertices, objNormals}, 
                                           1.2f, 0.5f, glm::vec3{(float)255/(float)255, (float)51/(float)255, (float)204/(float)255}};
  scene.add(diamond);


  scene.complete();

  return scene;
}


int main(const int argc, const char* argv[]) {

  const auto startTime = std::chrono::high_resolution_clock::now();

  Config& config = Config::getInstance();

  const unsigned int width = config.getValue<unsigned int>("width");
  const unsigned int height = config.getValue<unsigned int>("height");
  const unsigned int numberOfSamples = config.getValue<unsigned int>("numberOfSamples");
  const unsigned int numberOfShadowRays = config.getValue<unsigned int>("numberOfShadowRays");
  const float probabilityNotToTerminateRay = config.getValue<float>("probabilityNotToTerminateRay");
  std::cout << "width: " << width << std::endl;
  std::cout << "height: " << height << std::endl;
  std::cout << "numberOfSamples: " << numberOfSamples << std::endl;
  std::cout << "numberOfShadowRays: " << numberOfShadowRays << std::endl;
  std::cout << "probabilityNotToTerminateRay: " << probabilityNotToTerminateRay << std::endl;
  const glm::mat4 rotation2 = glm::rotate(-0.1f, glm::vec3{1.0f, 0.0f, 0.0f});
  glm::mat3 rotation = computeRotationMatrix(glm::normalize(glm::vec3{0.0f, 0.1f, 1.0f}));

  rotation[0][0] = rotation2[0][0];     rotation[0][1] = rotation2[0][1];     rotation[0][2] = rotation2[0][2];     
  rotation[1][0] = rotation2[1][0];     rotation[1][1] = rotation2[1][1];     rotation[1][2] = rotation2[1][2];     
  rotation[2][0] = rotation2[2][0];     rotation[2][1] = rotation2[2][1];     rotation[2][2] = rotation2[2][2]; 

  // rotation = glm::mat3{1.0f};

  Camera camera{glm::ivec2{width, height},   // pixels
                glm::vec2{0.01f, 0.01f},     // pixelSize
                glm::vec3{0.0f, -5.0f, -9.0f}, // position
                rotation,                    // rotation
                3.0f,                        // viewPlaneDistance
                numberOfSamples};            // superSampling

  const std::vector<Ray*> rays = camera.getRays();

  Scene scene = createScene();

  std::vector<unsigned char> image;
  image.resize(width * height * 4);
  std::fill(image.begin(), image.end(), 0);

  ThreadPool threadPool;
  // threadPool.setNumberOfWorkers(0); 

  std::mutex update;
  unsigned int columnCounter = 0;
  glm::vec3 globalMaxIntensity{0.0f, 0.0f, 0.0f};
  glm::vec3 globalMinIntensity{0.0f, 0.0f, 0.0f};

  const float rootImportance = 1.0f;

  for(unsigned x = 0; x < width; x++) {
    WorkItem* workItem = new WorkItem([&probabilityNotToTerminateRay, &update, &globalMaxIntensity, &globalMinIntensity, &columnCounter, 
                                       &image, &scene, &rays, &rootImportance, &numberOfSamples, &numberOfShadowRays, &height, &width, x]() {

      glm::vec3 localMaxIntensity{0.0f, 0.0f, 0.0f};
      glm::vec3 localMinIntensity{0.0f, 0.0f, 0.0f};

      for(unsigned y = 0; y < height; y++) {

        Node* roots[numberOfSamples];

        for(unsigned int s=0; s<numberOfSamples; s++) {

          Node* root = new Node{rays[numberOfSamples * width * y + numberOfSamples * x + s], rootImportance};
          roots[s] = root;
          
          const std::function<void(Node*)> traverse = [&probabilityNotToTerminateRay, &numberOfShadowRays, &scene, &traverse, &root](Node* node) {
            const Ray* ray = node->getRay();
            const std::pair<Object*, glm::vec3> intersection = scene.intersect(ray);

            if( intersection.first == nullptr ) { // No intersection found
              // const glm::vec3 direction = ray->getDirection();
              // const float importance = node->getImportance();
              // const glm::vec3 origin = ray->getOrigin();
              // std::cout << std::endl;
              // std::cout << "------------------------" << std::endl;
              // std::cout << "Root: " << (node == root) << std::endl;
              // std::cout << "Importance: "  << importance << std::endl;
              // std::cout << "Origin: "  << origin.x << " " << origin.y << " " << origin.z << std::endl;
              // std::cout << "Direction: "  << direction.x << " " << direction.y << " " << direction.z << std::endl;
              // std::cout << "Intersection: "  << intersection.second.x << " " << intersection.second.y << " " << intersection.second.z << std::endl;
              // throw std::invalid_argument{"No intersection found."};
            } else if( intersection.first->isLight() ) { // If intersecting object is a light source

              node->setIntensity(intersection.first->getIntensity());

            } else if( intersection.first->isTransparent() ) { // If intersecting object is transparent

              const glm::vec3 origin = ray->getOrigin();
              const glm::vec3 direction = ray->getDirection();
              glm::vec3 normal = intersection.first->getNormal(intersection.second);
              const glm::vec3 viewDirection = glm::normalize(origin); // TODO?: camera not always in origin

              const glm::vec3 reflection = glm::reflect(direction, normal);

              const float nodeRefractionIndex = node->getRefractionIndex();
              const float materialRefractionIndex = dynamic_cast<TransparentObject*>(intersection.first)->getRefractionIndex();

              float n1 = nodeRefractionIndex;
              float n2 = materialRefractionIndex;

              if( nodeRefractionIndex == materialRefractionIndex 
                  && 
                  node->getLastIntersectedObject() == intersection.first ) {

                // n1 = materialRefractionIndex;
                n2 = 1.0f; // Air
                // std::cout << "HEJ!" << std::endl;
                normal = -normal;
              } 

              const float refractionIndexRatio = n1 / n2; 
              const glm::vec3 refraction = glm::refract(direction, normal, refractionIndexRatio);

              // std::cout << "refraction: " << refraction.x << " " << refraction.y << " " << refraction.z << std::endl;
              // std::cout << "refractionIndexRatio: " << refractionIndexRatio << std::endl;

              const float importance = node->getImportance();
              const float transparency = dynamic_cast<TransparentObject*>(intersection.first)->getTransparancy();

              const glm::vec3 newReflectedOrigin = intersection.second + (normal - direction) * getEpsilon();
              const glm::vec3 newRefractedOrigin = intersection.second + (direction - normal) * getEpsilon();

              // TODO: Compute Fresnel in order to give the right porportions to the reflected and refracted part!

              if( importance > 0.001f ) {
              const float reflectedImportance = importance * (1.0f - transparency);
              node->setReflected(new Node{new Ray{newReflectedOrigin, reflection}, 
                                          reflectedImportance, intersection.first, n2});

              const float refractedImportance = importance * transparency;
              node->setRefracted(new Node{new Ray{newRefractedOrigin, refraction}, 
                                          refractedImportance, intersection.first, n2, true});

              traverse(node->getReflected());

              // const float brewster = std::atan2(n2 , n1);
              // const float brewster = std::asin(n1 / n2);
              // const float angle = std::acos(glm::dot(-direction, normal));
              // if( angle <= brewster ) {
                // std::cout << "GO!!" << std::endl;
                traverse(node->getRefracted());
              // } 
              // std::cout << "CODE!" << std::endl;

              const glm::vec3 color = intersection.first->getIntensity();
              const glm::vec3 intensity = (node->getReflected()->getIntensity() * reflectedImportance 
                                         + node->getRefracted()->getIntensity() * refractedImportance) / importance ;

              node->setIntensity(intensity * color);

              delete node->getReflected();
              delete node->getRefracted();
              }

            } else { // If intersecting object is opaque and not a light source

              const glm::vec2 randomAngles = getRandomAngles();
              
              if( !shouldTerminateRay(randomAngles.y, probabilityNotToTerminateRay) || node == root ) {

                const glm::vec3 normal = intersection.first->getNormal(intersection.second);
                const glm::vec3 direction = ray->getDirection();
                
                const glm::vec3 directionFlipped = -direction;

                glm::vec2 d1 = {std::acos(directionFlipped.z), 
                                std::atan2(directionFlipped.y, directionFlipped.x)};

                glm::vec2 normalAngles = {std::acos(normal.z), 
                                          std::atan2(normal.y, normal.x)};

                const glm::vec2 incomingAngles = d1 - normalAngles;
                const glm::vec2 outgoingAngles = randomAngles;

                const glm::vec2 reflectionAngles = normalAngles + randomAngles;

                const glm::vec3 reflection = glm::vec3{std::sin(reflectionAngles.x) * std::cos(reflectionAngles.y),
                                                       std::sin(reflectionAngles.x) * std::sin(reflectionAngles.y),
                                                       std::cos(reflectionAngles.x)};

                const float importance = node->getImportance();

                const float brdf = dynamic_cast<OpaqueObject*>(intersection.first)->computeBrdf(intersection.second, incomingAngles, outgoingAngles);

                const glm::vec3 newReflectedOrigin = intersection.second + (normal - direction) * getEpsilon();

                const float childImportance = importance * brdf * M_PI;

                node->setReflected(new Node{new Ray{newReflectedOrigin, reflection}, childImportance, intersection.first, node->getRefractionIndex()});

                // const glm::vec3 origin = ray->getOrigin();
                // const glm::vec3 trueReflection = glm::reflect(direction, normal);
                // std::cout << std::endl;
                // std::cout << "------------------------" << std::endl;
                // std::cout << "Root: " << (node == root) << std::endl;
                // std::cout << "Importance: "  << importance << std::endl;
                // std::cout << "Origin: "  << origin.x << " " << origin.y << " " << origin.z << std::endl;
                // std::cout << "Direction: "  << direction.x << " " << direction.y << " " << direction.z << std::endl;
                // std::cout << "Intersection: "  << intersection.second.x << " " << intersection.second.y << " " << intersection.second.z << std::endl;
                // std::cout << "Normal: "  << normal.x << " " << normal.y << " " << normal.z << std::endl;
                // std::cout << "TrueReflection: "  << trueReflection.x << " " << trueReflection.y << " " << trueReflection.z << std::endl;
                // std::cout << "Reflection: "  << reflection.x << " " << reflection.y << " " << reflection.z << std::endl;
                // std::string line;
                // std::getline(std::cin, line);

                traverse(node->getReflected());

                const glm::vec3 color = intersection.first->getIntensity();

                const glm::vec3 intensity =  0.5f*(childImportance / (probabilityNotToTerminateRay * importance)) * node->getReflected()->getIntensity()
                                            + 
                                          10.0f * scene.castShadowRays(newReflectedOrigin, 
                                                               incomingAngles, 
                                                               intersection.first,
                                                               numberOfShadowRays,
                                                               normal,
                                                               normalAngles);

                node->setIntensity(intensity * color * intersection.first->getColor(intersection.second));

                delete node->getReflected();
              }

            }

          };

          traverse(root);
        }

        glm::vec3 color{0.0f, 0.0f, 0.0f};
        for(unsigned int s=0; s<numberOfSamples; s++) {
          color += roots[s]->getIntensity();
          delete roots[s];
        }

        color /= (float)numberOfSamples;

        const int red = std::min( (int)(std::sqrt(color.r) * 100), 255);
        const int green = std::min( (int)(std::sqrt(color.g) * 100), 255);
        const int blue = std::min( (int)(std::sqrt(color.b) * 100), 255);
        const int alpha = 255;

        localMaxIntensity.r = std::max(localMaxIntensity.r, color.r);
        localMaxIntensity.g = std::max(localMaxIntensity.g, color.g);
        localMaxIntensity.b = std::max(localMaxIntensity.b, color.b);

        localMinIntensity.r = std::min(localMinIntensity.r, color.r);
        localMinIntensity.g = std::min(localMinIntensity.g, color.g);
        localMinIntensity.b = std::min(localMinIntensity.b, color.b);

        image[4 * width * y + 4 * x + 0] = red;
        image[4 * width * y + 4 * x + 1] = green;
        image[4 * width * y + 4 * x + 2] = blue;
        image[4 * width * y + 4 * x + 3] = alpha;
      }

      update.lock();
      std::cout << "\r" << (int)((++columnCounter / (float) width) * 100) << "%";
      std::flush(std::cout);
      globalMaxIntensity.r = std::max(globalMaxIntensity.r, localMaxIntensity.r);
      globalMaxIntensity.g = std::max(globalMaxIntensity.g, localMaxIntensity.g);
      globalMaxIntensity.b = std::max(globalMaxIntensity.b, localMaxIntensity.b);

      globalMinIntensity.r = std::min(globalMinIntensity.r, localMinIntensity.r);
      globalMinIntensity.g = std::min(globalMinIntensity.g, localMinIntensity.g);
      globalMinIntensity.b = std::min(globalMinIntensity.b, localMinIntensity.b);
      update.unlock();

    });
    threadPool.add(workItem);
  }
  threadPool.wait();

  std::cout << std::endl;
  // std::cout << "globalMinIntensity: " << globalMinIntensity.r << " " << globalMinIntensity.g << " " << globalMinIntensity.b << std::endl;
  // std::cout << "globalMaxIntensity: " << globalMaxIntensity.r << " " << globalMaxIntensity.g << " " << globalMaxIntensity.b << std::endl;

  std::string file = config.getValue<std::string>("name");
  if( argc == 2 ) {
    file = argv[1];
  }
  file += ".png";
  outputImage(file, image, width, height);

  const auto endTime = std::chrono::high_resolution_clock::now();
  const unsigned int duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  std::cout << " | " << file << " | " << duration << " ms" << std::endl;

  return 0;
}
