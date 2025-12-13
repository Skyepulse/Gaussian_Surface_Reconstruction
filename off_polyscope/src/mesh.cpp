#include "mesh.hpp"
#include "mesh_load/mesh_load.h"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <igl/boundary_loop.h>
#include <igl/boundary_facets.h>
#include <igl/is_edge_manifold.h>
#include <igl/facet_components.h>
#include <cfloat>

#include <filesystem>

#define PIV 3.141592653589793

using namespace std;
typedef Eigen::Triplet<double> Trip;

//================================//
mesh::mesh(const std::string& path, const std::string& output_metrics_file) : file_path(path), output_metrics_file(output_metrics_file)
{
    cout << "[MESH] Loading mesh from file: " << file_path << endl;
    loadFromFile();
    cout << "[MESH] Mesh loaded. Vertices: " << V.rows() << ", Faces: " << F.rows() << endl;
    initializeMeshParts();
    buildHalfEdgeStructure();
    cout << "[MESH] Mesh parts initialized." << endl;
    //this->computeAreaMatrix();
    //this->computeCotangentMatrix();
    buildMetrics();
    cout << "[MESH] Mesh metrics computed." << endl;
}

//================================//
void mesh::loadFromFile()
{
    // Check File Exists and Load
    ifstream f(file_path);
    if (!f.is_open()) 
    {
        std::cerr << "[Mesh constructor] Path does not exist: " << file_path << std::endl;
        throw runtime_error("[Mesh constructor] Path does not exist: " + file_path);
    }

    // Get file extension
    const size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == string::npos)
    {
        std::cerr << "[Mesh constructor] File has no extension: " << file_path << std::endl;
        throw runtime_error("[Mesh constructor] File has no extension: " + file_path);
    }

    string extension = file_path.substr(dot_pos + 1);
    if (extension == "off" || extension == "COFF") 
    {
        std::cout << "Detected COFF file format." << std::endl;
        meshLoadCOFF(*this);
    } 
    else if (extension == "obj") 
    {
        meshLoadOBJ(*this);
    }
    else if (extension == "ply") 
    {
        meshLoadPLY(*this);
    }
    else 
    {
        std::cerr << "[Mesh constructor] Unsupported file extension: " << extension << std::endl;
        throw runtime_error("[Mesh constructor] Unsupported file extension: " + extension);
    }
    f.close();
}

//================================//
mesh::~mesh()
{
    
}

//================================//
void mesh::getVertices(Eigen::MatrixXd& out_V) const
{
    out_V = V;
}

//================================//
void mesh::getFaces(Eigen::MatrixXi& out_F) const
{
    out_F = F;
}

//================================//
void mesh::getFaceColors(Eigen::MatrixXd& out_faceColor) const
{
    out_faceColor = faceColor;
}

//================================//
static double computeTriangleArea(const Eigen::Vector3d& v0, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2)
{
    Eigen::Vector3d edge1 = v1 - v0;
    Eigen::Vector3d edge2 = v2 - v0;
    Eigen::Vector3d cross_product = edge1.cross(edge2);
    double area = 0.5 * cross_product.norm();
    return area;
}

//================================//
static double computeAngle(const Eigen::Vector3d& v0, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2)
{
    // Angle at v0 between v1 and v2
    Eigen::Vector3d vec1 = v1 - v0;
    Eigen::Vector3d vec2 = v2 - v0;
    double dot_product = vec1.dot(vec2);
    double norms_product = vec1.norm() * vec2.norm();
    double cos_angle = dot_product / norms_product;
    cos_angle = std::clamp(cos_angle, -1.0, 1.0);
    double angle = acos(cos_angle) * (180.0 / PIV);
    return angle;
}

//================================//
void mesh::computeVoronoiAreas(PrimalFacePtr face)
{
    const std::vector<VertexPtr> vertices = face->getVertices();
    if (vertices.size() != 3)
    {
        std::cerr << "[MESH] Voronoi area computation only implemented for triangular faces." << std::endl;
        throw std::runtime_error("[MESH] Voronoi area computation only implemented for triangular faces.");
    }

    Eigen::Vector3d v0 = vertices[0]->position;
    Eigen::Vector3d v1 = vertices[1]->position;
    Eigen::Vector3d v2 = vertices[2]->position;

    const Eigen::Vector3d AB = v1 - v0;
    const Eigen::Vector3d BC = v2 - v1;
    const Eigen::Vector3d CA = v0 - v2;

    const double cot1 = AB.dot(-CA) / AB.cross(-CA).norm();
    const double cot2 = BC.dot(-AB) / BC.cross(-AB).norm();
    const double cot3 = CA.dot(-BC) / CA.cross(-BC).norm();

    const bool obtuse1 = AB.dot(-CA) < 0;
    const bool obtuse2 = BC.dot(-AB) < 0;
    const bool obtuse3 = CA.dot(-BC) < 0;
    const bool obtuse = obtuse1 || obtuse2 || obtuse3;
    face->is_obtuse = obtuse;

    const double area = 0.5 * (AB.cross(-CA)).norm();

    if (obtuse)
    {
        if (obtuse1)
            vertices[0]->voronoi_area += 0.5 * area;
        else
            vertices[0]->voronoi_area += 0.25 * area;

        if (obtuse2)
            vertices[1]->voronoi_area += 0.5 * area;
        else
            vertices[1]->voronoi_area += 0.25 * area;

        if (obtuse3)
            vertices[2]->voronoi_area += 0.5 * area;
        else
            vertices[2]->voronoi_area += 0.25 * area;
    }
    else
    {
        vertices[0]->voronoi_area += (1.0 / 8.0) * (CA.squaredNorm() * cot2 + AB.squaredNorm() * cot3);
        vertices[1]->voronoi_area += (1.0 / 8.0) * (AB.squaredNorm() * cot3 + BC.squaredNorm() * cot1);
        vertices[2]->voronoi_area += (1.0 / 8.0) * (BC.squaredNorm() * cot1 + CA.squaredNorm() * cot2);
    }
}

//================================//
void mesh::initializeMeshParts()
{
    // Metric specific initialization
    metrics.min_angle_p5 = 180.0;
    metrics.maximum_angle = 0.0;
    metrics.min_face_area = FLT_MAX - 1.0;
    metrics.max_face_area = 0.0;
    metrics.average_face_area = 0.0;

    this->primal_vertices.resize(V.rows());
    int n = V.rows();
    metrics.num_vertices = n;

    for (int i = 0; i < n; i++)
    {
        this->primal_vertices[i] = std::make_shared<Vertex>(i);
		this->primal_vertices[i]->position = V.row(i);
    }

    this->primal_faces.resize(F.rows());
	int k = F.rows();
    metrics.num_faces = k;

    for (int i = 0; i < k; i++)
    {
		const Eigen::RowVector3i& face = F.row(i);
        this->primal_faces[i] = std::make_shared<PrimalFace>();
        this->primal_faces[i]->index = i;
        for (int j = 0; j < 3; j++)
        {
            int vertex_index = face[j];
            this->primal_faces[i]->addVertex(this->primal_vertices[vertex_index]);
            this->primal_vertices[vertex_index]->addOneRingFace(this->primal_faces[i]);
		}

        this->computeVoronoiAreas(this->primal_faces[i]);

        const Eigen::Vector3d v0 = V.row(face[0]);
        const Eigen::Vector3d v1 = V.row(face[1]);
        const Eigen::Vector3d v2 = V.row(face[2]);

        this->primal_faces[i]->area = computeTriangleArea(v0, v1, v2);
        if (this->primal_faces[i]->area < metrics.min_face_area)
            metrics.min_face_area = this->primal_faces[i]->area;
        if (this->primal_faces[i]->area > metrics.max_face_area)
            metrics.max_face_area = this->primal_faces[i]->area;
        this->metrics.average_face_area += this->primal_faces[i]->area;

        this->primal_faces[i]->angles.resize(3);
        for (int j = 0; j < 3; j++)
        {
            const Eigen::Vector3d va = V.row(face[j]);
            const Eigen::Vector3d vb = V.row(face[(j + 1) % 3]);
            const Eigen::Vector3d vc = V.row(face[(j + 2) % 3]);
            double angle = computeAngle(va, vb, vc);
            this->primal_faces[i]->angles[j] = angle;

            if (angle > metrics.maximum_angle)
                metrics.maximum_angle = angle;

            this->face_angles.push_back(angle);
        }
    }

    std::sort(this->face_angles.begin(), this->face_angles.end());
    size_t index_5_percent = static_cast<size_t>(0.05 * this->face_angles.size());
    metrics.min_angle_p5 = this->face_angles[index_5_percent];

    this->metrics.average_face_area /= static_cast<double>(k);
    
    double threshold_area = 1e-6 * this->metrics.average_face_area;
    metrics.num_degenerate_faces = 0;
    for (const auto& face_ptr : this->primal_faces)
    {
        if (face_ptr->area < threshold_area)
        {
            metrics.num_degenerate_faces++;
        }

        const std::vector<VertexPtr> vertices = face_ptr->getVertices();
        for (const auto& vertex_ptr : vertices)
        {
            vertex_ptr->voronoi_area = max(vertex_ptr->voronoi_area, 0.0);
        }
    }
    metrics.area_degeneracy_ratio = static_cast<double>(metrics.num_degenerate_faces) / static_cast<double>(k);
}

//================================//
static double angle_between_vectors(const Eigen::Vector3d& v,
    const Eigen::Vector3d& w)
{
    double vn = v.norm();
    double wn = w.norm();
    if (vn < 1e-14 || wn < 1e-14) return 0.0;


    double dot = v.dot(w) / (vn * wn);
    dot = min(1.0, max(-1.0, dot));
    return atan2((v.cross(w)).norm(), dot * vn * wn);
}

//================================//
void mesh::buildHalfEdgeStructure()
{
    map<pair<int, int>, HalfEdgePtr> built_hedges;
    metrics.num_unique_edges = 0;

    int k = F.rows();
    for (int i = 0; i < k; i++)
    {
        // Half edges v1 -> v2, v2 -> v3, v3 -> v1
        array<HalfEdgePtr, 3> face_edges;
        const Eigen::RowVector3i& face = F.row(i);
        for (int j = 0; j < 3; j++)
        {
            const int vertex_start_index = face[j];
            const int vertex_end_index = face[(j + 1) % 3];

            HalfEdgePtr hedge = std::make_shared<HalfEdge>(i * 3 + j);
            hedge->setStartVertex(this->primal_vertices[vertex_start_index]);
            hedge->setEndVertex(this->primal_vertices[vertex_end_index]);
            hedge->setPrimalFace(this->primal_faces[i]);

            this->primal_faces[i]->addHalfEdge(hedge);
            this->primal_vertices[vertex_start_index]->addOutgoingHalfEdge(hedge);
            this->hedges.push_back(hedge);

            face_edges[j] = hedge;
            hedge->index_edge = j;

			pair<int, int> flip_key = make_pair(vertex_end_index, vertex_start_index);
            if (built_hedges.count(flip_key))
            {
				HalfEdgePtr flip_hedge = built_hedges[flip_key];
                hedge->setFlipHalfEdge(flip_hedge);
                flip_hedge->setFlipHalfEdge(hedge);

				hedge->sign_edge = -1;
				flip_hedge->sign_edge = 1;
            }
            else
            {
                metrics.num_unique_edges++;
                hedge->sign_edge = 1;
            }
            pair<int, int> hedge_key = make_pair(vertex_start_index, vertex_end_index);
            built_hedges[hedge_key] = hedge;
        }

		// Next, previous half-edges
        for (int j = 0; j < 3; j++)
        {
            face_edges[j]->setNextHalfEdge(face_edges[(j + 1) % 3]);
            face_edges[j]->setPreviousHalfEdge(face_edges[(j + 2) % 3]);
        }
    }

    // Set flag for boundary hedges, vertices and faces
    for (HalfEdgePtr hedge : this->hedges)
    {
        if (hedge->flip.expired())
        {
            hedge->boundary = true;
            hedge->getStartVertex()->is_boundary = true;
            hedge->getEndVertex()->is_boundary = true;
            hedge->getPrimalFace()->is_boundary = true;
            metrics.boundary_vertices++;
            metrics.boundary_faces++;
            metrics.boundary_edges++;
		}
    }

    for (HalfEdgePtr hedge : this->hedges)
    {
        const VertexPtr v_start = hedge->getStartVertex();
		const VertexPtr v_end = hedge->getEndVertex();

		const HalfEdgePtr hedge_next = hedge->getNextHalfEdge();
		const HalfEdgePtr hedge_prev = hedge->getPreviousHalfEdge();

		const VertexPtr v_next = hedge_next->getEndVertex();
		const VertexPtr v_prev = hedge_prev->getStartVertex();

        // So, tip is: angle between current and next half edge, we use end vertex and next vertex
		Eigen::Vector3d AB_tip = v_start->position - v_end->position;
		Eigen::Vector3d AC_tip = v_next->position - v_end->position;

		// Angle between AB and AC, at A === v_end
		double angle_tip = angle_between_vectors(AB_tip, AC_tip);

		// Tail angle: angle between previous half-edge and current half-edge, at v_start
		Eigen::Vector3d AB_tail = v_prev->position - v_start->position;
		Eigen::Vector3d AC_tail = v_end->position - v_start->position;

        double angle_tail = angle_between_vectors(AB_tail, AC_tail);

		// The last angle is easy to compute by angle sum in triangle
		double angle_opposite = PIV - angle_tip - angle_tail;
		hedge->cotangentOfOppAngle = cotangent(angle_opposite);
    }
}

//================================//
void mesh::computeAreaMatrix()
{
    vector<Trip> list;
	vector<Trip> AinvList;
	const int k = V.rows();

    for(int i = 0; i < k; i++)
    {
        VertexPtr v = this->primal_vertices[i];
        list.push_back(Trip(i, i, v->voronoi_area));
		AinvList.push_back(Trip(i, i, 1. / v->voronoi_area));
	}

	this->AreaMatrix.resize(k, k);
	this->AreaInvMatrix.resize(k, k);

    AreaMatrix.setFromTriplets(list.begin(), list.end());
	AreaMatrix.makeCompressed();

	AreaInvMatrix.setFromTriplets(AinvList.begin(), AinvList.end());
	AreaInvMatrix.makeCompressed();
}

//================================//
void mesh::computeCotangentMatrix()
{
    // So for this we have:
    // Lij =
	// IF i and j connected -> 1/2 (cot alpha + cot beta)
	// IF i == j -> -sum(Lij)
    // 0 else

    vector<Trip> list;
    vector<double> sumWeights;

	const int k = V.rows();
	sumWeights.resize(k, 0.0);

    for (int i = 0; i < k; i++)
    {
		const VertexPtr v = this->primal_vertices[i];
        for (HalfEdgePtr hedge : v->getOutgoingHalfEdges())
        {
			const VertexPtr vj = hedge->getEndVertex();
            
			double angleSum = hedge->cotangentOfOppAngle;
            if (!hedge->boundary)
            {
                HalfEdgePtr hedgeFlip = hedge->getFlipHalfEdge();
                if (!hedgeFlip)
					throw runtime_error("Flip half-edge is null when it is not marked as boundary!");

                angleSum += hedgeFlip->cotangentOfOppAngle;
			}

			list.push_back(Trip(i, vj->index, 0.5 * angleSum));
			sumWeights[i] += 0.5 * angleSum;
        }
    }

    for (int i = 0; i < k; i++)
    {
        list.push_back(Trip(i, i, -sumWeights[i]));
	}

    L.resize(k, k);
	L.setFromTriplets(list.begin(), list.end());
    L.makeCompressed();
}

//================================//
void mesh::buildMetrics()
{
    // Create folder given output_metrics_file path
    std::filesystem::path output_path(output_metrics_file);
    std::filesystem::create_directories(output_path.parent_path());

    Eigen::MatrixXi E;
    Eigen::VectorXi EMAP;

    // Basic Info
    cout << "Mesh Metrics:" << endl;
    cout << "  Number of vertices: " << metrics.num_vertices << endl;
    cout << "  Number of faces: " << metrics.num_faces << endl;
    cout << "  Number of unique edges: " << metrics.num_unique_edges << endl;
    metrics.euler_characteristic = static_cast<int>(metrics.num_vertices) - static_cast<int>(metrics.num_unique_edges) + static_cast<int>(metrics.num_faces);
    cout << "  Euler characteristic: " << metrics.euler_characteristic << endl;

    // Boundaries
    metrics.loops.clear();
    igl::boundary_loop(F, metrics.loops);
    metrics.boundary_loops = metrics.loops.size();
    
    metrics.boundary_edges = 0;
    if (metrics.boundary_loops > 0) 
    {
        metrics.max_loop_size = 0;
        metrics.min_loop_size = static_cast<size_t>(INT_MAX);

        for (const auto& loop : metrics.loops) 
        {
            metrics.boundary_edges += loop.size();

            if (loop.size() > metrics.max_loop_size)
                metrics.max_loop_size = loop.size();

            if (loop.size() < metrics.min_loop_size)
                metrics.min_loop_size = loop.size();
        }
        metrics.average_loop_size = static_cast<double>(metrics.boundary_edges) / metrics.boundary_loops;

    } 
    else 
    {
        metrics.average_loop_size = 0.f;
        metrics.max_loop_size = 0;
        metrics.min_loop_size = 0;
    }

    cout << "  Number of boundary loops: " << metrics.boundary_loops << endl;
    cout << "  Number of boundary edges: " << metrics.boundary_edges << endl;
    cout << "  Average loop size: " << metrics.average_loop_size << endl;
    cout << "  Max loop size: " << metrics.max_loop_size << endl;
    cout << "  Min loop size: " << metrics.min_loop_size << endl;

    // Ratios
    metrics.edge_boundary_ratio = static_cast<double>(metrics.boundary_edges) / metrics.num_unique_edges;
    cout << "  Edge boundary ratio: " << metrics.edge_boundary_ratio << endl;

    // Connected Components
    Eigen::VectorXi CC;
    int num_cc = igl::facet_components(F, CC);

    metrics.num_connected_components = num_cc;
    cout << "  Number of connected components: " << metrics.num_connected_components << endl;

    metrics.component_sizes.resize(num_cc, 0);

    int maxS = 0;
    int minS = INT_MAX;
    for (int i = 0; i < CC.size(); i++)
    {
        int comp_id = CC[i];
        metrics.component_sizes[comp_id]++;

        if (metrics.component_sizes[comp_id] > maxS)
            maxS = metrics.component_sizes[comp_id];

        if (metrics.component_sizes[comp_id] < minS)
            minS = metrics.component_sizes[comp_id];
    }
    metrics.largest_component_size = maxS;
    metrics.smallest_component_size = minS;

    metrics.average_component_size = static_cast<double>(metrics.num_faces) / metrics.num_connected_components;
    cout << "  Largest component size (faces): " << metrics.largest_component_size << endl;
    cout << "  Smallest component size (faces): " << metrics.smallest_component_size << endl;
    cout << "  Average component size (faces): " << metrics.average_component_size << endl;

    // Face area and angles degeneracy
    cout << "  Minimum face area: " << metrics.min_face_area << endl;
    cout << "  Maximum face area: " << metrics.max_face_area << endl;
    cout << "  Average face area: " << metrics.average_face_area << endl;
    cout << "  Number of degenerate faces: " << metrics.num_degenerate_faces << endl;
    cout << "  Area degeneracy ratio: " << metrics.area_degeneracy_ratio << endl;
    cout << "  5th percentile minimum angle: " << metrics.min_angle_p5 << endl;
    cout << "  Maximum angle: " << metrics.maximum_angle << endl;
}