#ifndef MESHPARTS_H
#define MESHPARTS_H

#include <Eigen/Core>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace MeshParts
{

  // Forward declarations
  struct HalfEdge;
  struct PrimalFace;
  struct Vertex;

  typedef std::shared_ptr<PrimalFace> PrimalFacePtr;
  typedef std::shared_ptr<Vertex> VertexPtr;
  typedef std::shared_ptr<HalfEdge> HalfEdgePtr;

  struct Vertex
  {
    Vertex() = default;
    ~Vertex() = default;

    Vertex(size_t index) : index(index) {};

    size_t index;
    Eigen::Vector3d position;
    Eigen::Vector3d normal;

    std::vector<std::weak_ptr<PrimalFace>> one_ring_faces;
    std::vector<std::weak_ptr<HalfEdge>> outgoing_half_edges;

    bool is_boundary = false;

    std::vector<PrimalFacePtr> getOneRingFaces() const
    {
      std::vector<PrimalFacePtr> faces;

      for (auto face : one_ring_faces)
      {
        if (!face.expired())
        {
          faces.push_back(face.lock());
        }
        else
        {
          throw std::runtime_error("Face is deleted");
        }
      }
      return faces;
    }

    std::vector<HalfEdgePtr> getOutgoingHalfEdges() const
    {
      std::vector<HalfEdgePtr> edges;
      for (auto edge : outgoing_half_edges)
      {
        if (!edge.expired())
        {
          edges.push_back(edge.lock());
        }
        else
        {
          throw std::runtime_error("Edge is deleted");
        }
      }
      return edges;
    }

    void addOneRingFace(PrimalFacePtr one_ring_face)
    {
      this->one_ring_faces.push_back(one_ring_face);
    }

    void addOutgoingHalfEdge(HalfEdgePtr outgoing_half_edge)
    {
      this->outgoing_half_edges.push_back(outgoing_half_edge);
    }

    bool operator==(const Vertex &other) const;
    void print();
  };

  struct HalfEdge
  {
    HalfEdge() = default;
    ~HalfEdge() = default;

    HalfEdge(int index) : index(index) {};

    bool boundary = false;

    int index;
    int sign_edge = -1;
    int index_edge = -1;
    std::weak_ptr<Vertex> start;
    std::weak_ptr<Vertex> end;
    std::weak_ptr<HalfEdge> flip;
    std::weak_ptr<HalfEdge> next;
    std::weak_ptr<HalfEdge> previous;
    std::weak_ptr<PrimalFace> primal_face;

    VertexPtr getStartVertex() const
    {
      if (!start.expired())
      {
        return start.lock();
      }
      else
      {
        throw std::runtime_error("Start Vertex is deleted");
      }
    }

    VertexPtr getEndVertex() const
    {
      if (!end.expired())
      {
        return end.lock();
      }
      else
      {
        throw std::runtime_error("End Vertex is deleted");
      }
    }

    HalfEdgePtr getFlipHalfEdge() const
    {
      if (this->boundary)
      {
        throw std::runtime_error(" You try to call flip on a boundary edge");
      }
      if (flip.expired())
      {
        throw std::runtime_error("Flip Half Edge is deleted");
      }
      else
      {
        return flip.lock();
      }
    }

    HalfEdgePtr getNextHalfEdge() const
    {
      if (next.expired())
      {
        std::cout << " The half edge is not hanging on the boundary, something "
                     "went realy wrong"
                  << std::endl;
        throw std::runtime_error("Next Half Edge is deleted");
      }
      else
      {
        return next.lock();
      }
    }

    HalfEdgePtr getPreviousHalfEdge() const
    {
      if (previous.expired())
      {
        throw std::runtime_error("Previous Half Edge is deleted");
      }
      else
      {
        return previous.lock();
      }
    }

    PrimalFacePtr getPrimalFace() const
    {
      if (primal_face.expired())
      {
        throw std::runtime_error("Primal Face is deleted");
      }
      else
      {
        return primal_face.lock();
      }
    }

    int getIndexOfStartVertex() const
    {
      if (!start.expired())
      {
        return start.lock()->index;
      }
      else
      {
        throw std::runtime_error("Start Vertex is deleted");
      }
    }

    int getIndexOfEndVertex() const
    {
      if (!end.expired())
      {
        return end.lock()->index;
      }
      else
      {
        throw std::runtime_error("End Vertex is deleted");
      }
    }

    int getIndexOfFlipHalfEdge() const
    {
      if (!flip.expired())
      {
        return flip.lock()->index;
      }
      else
      {
        throw std::runtime_error("Flip Half Edge is deleted");
      }
    }

    int getIndexOfNextHalfEdge() const
    {
      if (!next.expired())
      {
        return next.lock()->index;
      }
      else
      {
        throw std::runtime_error("Next Half Edge is deleted");
      }
    }

    void setStartVertex(VertexPtr start) { this->start = start; }

    void setEndVertex(VertexPtr end) { this->end = end; }

    void setFlipHalfEdge(HalfEdgePtr flip) { this->flip = flip; }

    void setNextHalfEdge(HalfEdgePtr next) { this->next = next; }

    void setPreviousHalfEdge(HalfEdgePtr previous) { this->previous = previous; }

    void setPrimalFace(PrimalFacePtr primal_face)
    {
      this->primal_face = primal_face;
    }

    void print();

    bool operator==(const HalfEdge &) const;
  };

  struct PrimalFace
  {
    PrimalFace() = default;
    ~PrimalFace() = default;
    int index = -1;
    std::vector<std::weak_ptr<Vertex>> vertices_face;
    std::vector<std::weak_ptr<HalfEdge>> hedges_face;

    bool is_boundary = false;

    std::vector<VertexPtr> getVertices() const
    {
      std::vector<VertexPtr> vertices;
      for (auto vertex : vertices_face)
      {
        if (!vertex.expired())
        {
          vertices.push_back(vertex.lock());
        }
        else
        {
          throw std::runtime_error("Vertex is deleted");
        }
      }
      return vertices;
    }

    std::vector<HalfEdgePtr> getHalfEdges() const
    {
      std::vector<HalfEdgePtr> hedges;
      for (auto hedge : hedges_face)
      {
        if (!hedge.expired())
        {
          hedges.push_back(hedge.lock());
        }
        else
        {
          throw std::runtime_error("Half Edge is deleted");
        }
      }
      return hedges;
    }

    void addVertex(VertexPtr vertex) { this->vertices_face.push_back(vertex); }

    void addHalfEdge(HalfEdgePtr hedge) { this->hedges_face.push_back(hedge); }

    bool operator==(const PrimalFace &other) const;

    void print();
  };

} // namespace MeshParts

#endif // MESHPARTS_H
