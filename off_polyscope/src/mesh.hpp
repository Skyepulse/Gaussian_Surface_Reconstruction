#ifndef MESH_H
#define MESH_H

#include <Eigen/Core>
#include <vector>
#include <string>
#include "MeshParts/MeshParts.h"

using namespace MeshParts;

//================================//
struct MeshMetrics
{
    size_t num_vertices;
    size_t num_faces;
    size_t num_unique_edges;

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
    
    mesh(const std::string& path);
    mesh() = default;
    ~mesh();

    void getVertices(Eigen::MatrixXd& out_V) const;
    void getFaces(Eigen::MatrixXi& out_F) const;
    void getFaceColors(Eigen::MatrixXd& out_faceColor) const;

    void loadFromFile();
    void initializeMeshParts();
    void buildHalfEdgeStructure();
    void buildMetrics();

    void getMetrics(MeshMetrics& out_metrics) const { out_metrics = metrics; }
    std::string file_path;
};

#endif // MESH_H