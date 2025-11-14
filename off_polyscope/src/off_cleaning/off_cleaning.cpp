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

//=============================== Mesh Load =============================//
MeshData loadCOFF_TriangleSplatting(const string& path) 
{
  ifstream f(path);
  if (!f.is_open()) throw runtime_error("Could not open file: " + path);

  string header;
  f >> header;
  if (header != "COFF" && header != "OFF") {
    throw runtime_error("Expected header COFF or OFF, got: " + header);
  }

  size_t nV = 0, nF = 0, nE = 0;
  f >> nV >> nF >> nE;

  MeshData md;
  md.V.reserve(nV);
  md.F.reserve(nF);
  md.faceColor.reserve(nF);

  // Read vertices
  for (size_t i = 0; i < nV; ++i) {
    double x, y, z;
    f >> x >> y >> z;
    md.V.emplace_back(x, y, z);
  }

  // Read faces + colors
  for (size_t i = 0; i < nF; ++i) {
    int n, v0, v1, v2;
    int r, g, b, a = 255;

    if (!(f >> n >> v0 >> v1 >> v2 >> r >> g >> b >> a)) {
      throw runtime_error("Malformed COFF line at face " + to_string(i));
    }

    if (n != 3)
      throw runtime_error("Non-triangle face encountered, expected '3' at face " + to_string(i));

    md.F.emplace_back(v0, v1, v2);
    md.faceColor.emplace_back(r / 255.0, g / 255.0, b / 255.0);
  }

  return md;
}

//=============================== Clean Mesh =============================//
MeshData clean_mesh(const MeshData& md)
{
    MeshData md_cleaned = md;

    // Remove near-duplicate triangles
    md_cleaned = remove_near_duplicate_triangles(md_cleaned, 0.1, 15.0, 1e-1);
    md_cleaned = remove_high_aspect(md_cleaned, 20.0);
    return md_cleaned;
}

MeshData remove_near_duplicate_triangles(
    const MeshData& md,
    double eps_centroid = 1e-3,
    double eps_normal_deg = 10.0,
    double eps_area = 1e-6)
{
    MeshData out;
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
        const auto& f = md.F[i];
        Eigen::Vector3d v0 = md.V[f[0]];
        Eigen::Vector3d v1 = md.V[f[1]];
        Eigen::Vector3d v2 = md.V[f[2]];

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
            }
        }
    }

    // Build output
    for (int i = 0; i < nF; i++) {
        if (!removed[i]) {
            out.F.push_back(md.F[i]);
            out.faceColor.push_back(md.faceColor[i]);
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

MeshData remove_high_aspect(const MeshData& md, double threshold = 20.0)
{
    MeshData out;
    out.V = md.V;

    for (size_t i = 0; i < md.F.size(); ++i) {
        auto f = md.F[i];
        double ar = triangle_aspect_ratio(
            md.V[f[0]], md.V[f[1]], md.V[f[2]]
        );

        if (ar < threshold) {
            out.F.push_back(f);
            out.faceColor.push_back(md.faceColor[i]);
        }
    }
    return out;
}
