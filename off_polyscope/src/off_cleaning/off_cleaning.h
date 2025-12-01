// off_cleaning/off_cleaning.h
#if !defined(OFF_CLEANING_H)
#define OFF_CLEANING_H

#include <Eigen/Core>
#include "../mesh.hpp"
#include <vector>

//=============================== Methods =============================//
double triangle_area(const Eigen::Vector3d& a,
                            const Eigen::Vector3d& b,
                            const Eigen::Vector3d& c);
mesh clean_mesh(const mesh& md);
mesh remove_near_duplicate_triangles(
    const mesh& md,
    double eps_centroid,
    double eps_normal_deg,
    double eps_area);
mesh remove_high_aspect(const mesh& md, double threshold);
#endif // OFF_CLEANING_H
