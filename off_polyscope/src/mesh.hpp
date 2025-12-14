#ifndef MESH_H
#define MESH_H

#include <Eigen/Core>
#include <Eigen/Sparse>
#include <vector>
#include <string>
#include "MeshParts/MeshParts.h"

using namespace MeshParts;
typedef Eigen::SparseMatrix<double> SpMat;

//================================//
struct MeshMetrics
{
    // Basics
    size_t num_vertices;
    size_t num_faces;
    size_t num_unique_edges;
    int euler_characteristic;

    // Loops and Boundaries
    std::vector<std::vector<int>> loops;
    Eigen::VectorXi manifold_edge_flags;

    size_t boundary_loops;
    double average_loop_size;
    size_t max_loop_size;
    size_t min_loop_size;

    size_t boundary_edges;
    size_t boundary_vertices;
    size_t boundary_faces;

    double edge_boundary_ratio;
    double edge_raw_boundary_ratio;

    // Connected Components
    int num_connected_components;
    std::vector<size_t> component_sizes;
    size_t largest_component_size;
    size_t smallest_component_size;
    double average_component_size;

    // Face area and angles degeneracy
    double min_face_area;
    double max_face_area;
    double average_face_area;

    int num_degenerate_faces;
    double area_degeneracy_ratio;

    double min_angle_p5; // Do not directly use min_angle to avoid special outliers
    double maximum_angle;
};

//================================//
class mesh
{
private:
    MeshMetrics metrics;

public:
    Eigen::MatrixXd V;              // Primal vertices
    Eigen::MatrixXd N;              // Normals
    Eigen::MatrixXd UV;             // Texture coordinates

    Eigen::MatrixXi F;              // Primal faces
    Eigen::MatrixXi FUV;            // Texture coordinate indices
    Eigen::MatrixXi FN;             // Normal indices

    Eigen::MatrixXd faceColor;      // per-face RGB (0–1)
    Eigen::MatrixXd vertexColor;    // per-vertex RGB (0–1)

    std::vector<std::shared_ptr<Vertex>> primal_vertices;
    std::vector<std::shared_ptr<PrimalFace>> primal_faces;
    std::vector<std::shared_ptr<HalfEdge>> hedges;

    SpMat AreaMatrix;
    SpMat AreaInvMatrix;
    SpMat L; // Cotangent matrix

    std::vector<float> face_angles;
    
    mesh(const std::string& path, const std::string& output_metrics_file);
    mesh() = default;
    ~mesh();

    void getVertices(Eigen::MatrixXd& out_V) const;
    void getFaces(Eigen::MatrixXi& out_F) const;
    void getFaceColors(Eigen::MatrixXd& out_faceColor) const;

    void loadFromFile();
    void initializeMeshParts();
    void buildHalfEdgeStructure();
    void buildMetrics();

    void computeVoronoiAreas(PrimalFacePtr face);
    void computeCotangentMatrix();
    void computeAreaMatrix();

    double cotangent(double x) { return 1 / (tan(x)); }

    void getMetrics(MeshMetrics& out_metrics) const { out_metrics = metrics; }
    std::string file_path;
    std::string output_metrics_file;
};

#endif // MESH_H