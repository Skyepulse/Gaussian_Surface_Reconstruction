#undef cross
#define GLM_FORCE_PURE
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include "off_cleaning/off_cleaning.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>

MeshData md;
MeshData md_cleaned;
polyscope::SurfaceMesh* mesh_original_ps = nullptr; // handle
polyscope::SurfaceMesh* mesh_cleaned_ps = nullptr; // handle

//=============================== UI callback =============================//
void myUICallback() 
{
  ImGui::Separator();
    ImGui::Text("Mesh Cleaning");

  if (ImGui::Button("Clean Mesh")) 
  {
    if (mesh_cleaned_ps != nullptr) 
    {
      polyscope::removeStructure(mesh_cleaned_ps);
      mesh_cleaned_ps = nullptr;
    }
    md_cleaned = clean_mesh(md);

    mesh_cleaned_ps = polyscope::registerSurfaceMesh("Cleaned Mesh", md_cleaned.V, md_cleaned.F);
    mesh_cleaned_ps->setEnabled(true);
    mesh_cleaned_ps->setSmoothShade(false);
    mesh_cleaned_ps->setEdgeWidth(0.5);
    mesh_cleaned_ps->addFaceColorQuantity("Face Color", md_cleaned.faceColor)->setEnabled(true);
  }
}

//=============================== main =============================//
int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage:\n  " << argv[0] << " <mesh.off>\n";
    return 1;
  }

  std::string path = argv[1];

  try {
    md = loadCOFF_TriangleSplatting(path);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  polyscope::init();

  mesh_original_ps = polyscope::registerSurfaceMesh("TriangleSplatting mesh", md.V, md.F);
  mesh_original_ps->setSmoothShade(false);
  mesh_original_ps->setEdgeWidth(0.5);

  // Per-face color
  if (!md.faceColor.empty()) {
    mesh_original_ps->addFaceColorQuantity("Face Color", md.faceColor)->setEnabled(true);
  }

  // Install UI callback for buttons
  polyscope::state::userCallback = myUICallback;

  polyscope::view::resetCameraToHomeView();
  polyscope::show();

  return 0;
}
