#undef cross
#define GLM_FORCE_PURE
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include "off_cleaning/off_cleaning.h"
#include "mesh.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <filesystem>

std::unique_ptr<mesh> md;
std::unique_ptr<mesh> md_cleaned;

using Smesh = polyscope::SurfaceMesh;

Smesh* mesh_original_ps; // handle
Smesh* mesh_cleaned_ps; // handle

//=============================== UI callback =============================//
void myUICallback() 
{
  ImGui::Separator();
    ImGui::Text("Mesh Cleaning");

  if (ImGui::Button("Compute Mesh Metrics")) 
  {
    if (!md) {
      std::cerr << "[myUICallback] No mesh loaded!" << std::endl;
      return;
    }
    md->buildMetrics();
  }
}

//=============================== main =============================//
int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage:\n  " << argv[0] << " <mesh.off> <output_metrics_file>\n";
    return 1;
  }

  std::string path = argv[1];
  std::string output_metrics_file = argv[2];
  
  // Load mesh
  md = std::make_unique<mesh>(path, output_metrics_file);

  //polyscope::init();

  //mesh_original_ps = polyscope::registerSurfaceMesh("Loaded Mesh", md->V, md->F);
  //mesh_original_ps->setSmoothShade(false);
  //mesh_original_ps->setEdgeWidth(0.5);

  // Per-face color
  if (md->faceColor.rows() != 0) {
    //mesh_original_ps->addFaceColorQuantity("Face Color", md->faceColor)->setEnabled(true);
  }

  // Per-vertex color
  if(md->vertexColor.rows() != 0) {
    //mesh_original_ps->addVertexColorQuantity("Vertex Color", md->vertexColor)->setEnabled(false);
  }

  // Install UI callback for buttons
  //polyscope::state::userCallback = myUICallback;

  //polyscope::view::resetCameraToHomeView();
  //polyscope::show();

  // MATPLOT++ wellness plot example

  // CREATE OUTPUT DIR matplot_output IF NOT EXISTS

  return 0;
}
