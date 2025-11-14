// off_cleaning/off_cleaning.h
#if !defined(OFF_CLEANING_H)
#define OFF_CLEANING_H

#include <Eigen/Core>
#include <vector>

//=============================== structs =============================//
struct MeshData 
{
  std::vector<Eigen::Vector3d> V;         // vertices
  std::vector<Eigen::Vector3i> F;         // faces
  std::vector<Eigen::Vector3d> faceColor; // per-face RGB (0–1)
};

//=============================== Methods =============================//
MeshData loadCOFF_TriangleSplatting(const std::string& path);
double triangle_area(const Eigen::Vector3d& a,
                            const Eigen::Vector3d& b,
                            const Eigen::Vector3d& c);
MeshData clean_mesh(const MeshData& md);
MeshData remove_near_duplicate_triangles(
    const MeshData& md,
    double eps_centroid,
    double eps_normal_deg,
    double eps_area);
MeshData remove_high_aspect(const MeshData& md, double threshold);

#endif // OFF_CLEANING_H
