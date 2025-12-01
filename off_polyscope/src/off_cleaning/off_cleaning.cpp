#undef cross
#define GLM_FORCE_PURE
#include <Eigen/Core>
#include <Eigen/Geometry>
#include "off_cleaning.h"
#include <fstream>
#include <sstream>
#include <string>
#include <queue>
#include <unordered_map>
#include <iostream>

using namespace std;

// M_PI may not be defined in some compilers
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//=============================== triangle area =============================//
double triangle_area(const Eigen::Vector3d& a,
                            const Eigen::Vector3d& b,
                            const Eigen::Vector3d& c)
{
    Eigen::Vector3d ba = b - a;
    Eigen::Vector3d ca = c - a;

    Eigen::Vector3d cross_prod = ba.cross(ca);
    return 0.5 * cross_prod.norm();
}

//=============================== Clean Mesh =============================//
mesh clean_mesh(const mesh& md)
{
    mesh md_cleaned = md;

    // Remove near-duplicate triangles
    md_cleaned = remove_near_duplicate_triangles(md_cleaned, 0.1, 15.0, 1e-1);
    md_cleaned = remove_high_aspect(md_cleaned, 20.0);
    return md_cleaned;
}

//================================//
mesh remove_near_duplicate_triangles(
    const mesh& md,
    double eps_centroid = 1e-3,
    double eps_normal_deg = 10.0,
    double eps_area = 1e-6)
{
    mesh out;
    out.V = md.V;

    int nF = md.F.size();

    // Precompute tri properties
    struct TriInfo {
        Eigen::Vector3i f;
        Eigen::Vector3d centroid;
        Eigen::Vector3d normal;
        double area;
    };

    std::vector<TriInfo> info(nF);

    for (int i = 0; i < nF; i++) {
        const Eigen::Vector3i& f = md.F.row(i);
        Eigen::Vector3d v0 = md.V.row(f[0]);
        Eigen::Vector3d v1 = md.V.row(f[1]);
        Eigen::Vector3d v2 = md.V.row(f[2]);

        Eigen::Vector3d c = (v0 + v1 + v2) / 3.0;
        Eigen::Vector3d n = (v1 - v0).cross(v2 - v0);
        double area = 0.5 * n.norm();

        if (n.norm() > 1e-12)
            n.normalize();
        else
            n = Eigen::Vector3d(0, 0, 0);

        info[i] = { f, c, n, area };
    }

    // Voxel grid hash
    struct Key {
        int x, y, z;
        bool operator==(const Key& o) const {
            return x == o.x && y == o.y && z == o.z;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return (std::hash<int>{}(k.x) ^ (std::hash<int>{}(k.y) << 1)) ^ (std::hash<int>{}(k.z) << 2);
        }
    };

    std::unordered_map<Key, std::vector<int>, KeyHash> grid;
    grid.reserve(nF * 2);

    double cell = eps_centroid * 2.0;

    // Fill voxel grid
    for (int i = 0; i < nF; i++) {
        Eigen::Vector3d c = info[i].centroid;
        Key key{
            int(std::floor(c.x() / cell)),
            int(std::floor(c.y() / cell)),
            int(std::floor(c.z() / cell))
        };
        grid[key].push_back(i);
    }

    std::vector<bool> removed(nF, false);

    auto rad = eps_normal_deg * M_PI / 180.0;
    double cos_th = std::cos(rad);

    // Compare triangles inside each voxel
    size_t removed_count = 0;
    for (auto& bucket : grid) {
        auto& faces = bucket.second;

        for (int i = 0; i < faces.size(); i++) {
            int fi = faces[i];
            if (removed[fi]) continue;

            for (int j = i + 1; j < faces.size(); j++) {
                int fj = faces[j];
                if (removed[fj]) continue;

                const auto& A = info[fi];
                const auto& B = info[fj];

                // Centroids very close?
                if ((A.centroid - B.centroid).norm() > eps_centroid) continue;

                // Normals similar?
                if (A.normal.dot(B.normal) < cos_th) continue;

                // Area similar?
                if (std::fabs(A.area - B.area) > eps_area) continue;

                // These triangles are near-duplicates
                removed[fj] = true;
                removed_count++;
            }
        }
    }

    out.F = Eigen::MatrixXi(nF - removed_count, 3);
    out.faceColor = Eigen::MatrixXd(nF - removed_count, 3);

    // Build output
    for (int i = 0; i < nF; i++) {
        if (!removed[i]) {
            out.F.row(out.F.rows() - removed_count) = md.F.row(i);
            out.faceColor.row(out.faceColor.rows() - removed_count) = md.faceColor.row(i);
            removed_count--;
        }
    }

    std::cout << "Removed near-duplicate triangles: "
              << (nF - out.F.size()) << std::endl;

    return out;
}

//=============================== Remove Skews =============================//
double triangle_aspect_ratio(const Eigen::Vector3d& a,
                             const Eigen::Vector3d& b,
                             const Eigen::Vector3d& c)
{
    double l0 = (b - a).norm();
    double l1 = (c - b).norm();
    double l2 = (a - c).norm();

    double longest = std::max({l0, l1, l2});
    double shortest = std::min({l0, l1, l2});

    return longest / shortest;
}

//================================//
mesh remove_high_aspect(const mesh& md, double threshold = 20.0)
{
    mesh out;
    out.V = md.V;

    for (size_t i = 0; i < md.F.rows(); ++i) {
        Eigen::Vector3i f = md.F.row(i);
        double ar = triangle_aspect_ratio(
            md.V.row(f[0]), md.V.row(f[1]), md.V.row(f[2])
        );

        out.F = Eigen::MatrixXi(0, 3);
        out.faceColor = Eigen::MatrixXd(0, 3);

        if (ar < threshold) {
            out.F.conservativeResize(out.F.rows() + 1, 3);
            out.F.row(out.F.rows() - 1) = f;

            out.faceColor.conservativeResize(out.faceColor.rows() + 1, 3);
            out.faceColor.row(out.faceColor.rows() - 1) = md.faceColor.row(i);
        }
    }
    return out;
}
