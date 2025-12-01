#include "mesh.hpp"
#include "mesh_load/mesh_load.h"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <igl/boundary_loop.h>
#include <igl/boundary_facets.h>
#include <igl/is_edge_manifold.h>

using namespace std;

//================================//
mesh::mesh(const std::string& path) : file_path(path)
{
    cout << "Loading mesh from file: " << file_path << endl;
    loadFromFile();
    initializeMeshParts();
    buildHalfEdgeStructure();
    cout << "Half edge structure built." << endl;
    buildMetrics();
    cout << "Mesh metrics computed." << endl;
}

//================================//
void mesh::loadFromFile()
{
    // Check File Exists and Load
    ifstream f(file_path);
    if (!f.is_open()) 
        throw runtime_error("[Mesh constructor] Path does not exist: " + file_path);

    // Get file extension
    const size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == string::npos)
        throw runtime_error("[Mesh constructor] File has no extension: " + file_path);

    string extension = file_path.substr(dot_pos + 1);
    if (extension == "off" || extension == "COFF") 
    {
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
void mesh::initializeMeshParts()
{
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
    }
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
}

//================================//
void mesh::buildMetrics()
{
    Eigen::MatrixXi E;
    Eigen::VectorXi EMAP;

    // Basic Info
    cout << "Mesh Metrics:" << endl;
    cout << "  Number of vertices: " << metrics.num_vertices << endl;
    cout << "  Number of faces: " << metrics.num_faces << endl;
    cout << "  Number of unique edges: " << metrics.num_unique_edges << endl;

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
}