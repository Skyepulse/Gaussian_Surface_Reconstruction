#include "gtest/gtest.h"
#include "Eigen/Core"

#include "mesh.hpp"
#include "igl/read_triangle_mesh.h"

#include "igl/gaussian_curvature.h"
#include "igl/cotmatrix.h"
#include "igl/massmatrix.h"
#include "igl/edges.h"
#include "igl/internal_angles.h"
#include "igl/barycenter.h"
#include "igl/doublearea.h"
#include "igl/circumradius.h"
#include "igl/per_face_normals.h"

std::string mesh_path = "..\\Meshes\\bunny.obj";

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    if (argc > 1)
        mesh_path = argv[1];

    return RUN_ALL_TESTS();
}

class MeshFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<mesh> test_mesh;
    static Eigen::MatrixXd V0;
    static Eigen::MatrixXi F0;

    static constexpr double tol = 1e-5;
    static constexpr double laptol = 1e-8;

    static void SetUpTestSuite()
    {
        test_mesh = std::make_unique<mesh>(mesh_path);
        V0 = test_mesh->V;
        F0 = test_mesh->F;
    }
};

std::unique_ptr<mesh> MeshFixture::test_mesh;
Eigen::MatrixXd MeshFixture::V0;
Eigen::MatrixXi MeshFixture::F0;

TEST_F(MeshFixture, CheckBasicSanityMesh)
{
    EXPECT_EQ(test_mesh->primal_vertices.size(), V0.rows());
    EXPECT_EQ(test_mesh->primal_faces.size(), F0.rows());
}

TEST_F(MeshFixture, CheckGoodInitializationVertex)
{
    int n_vertices = test_mesh->primal_vertices.size();

    for (int idx_vtx = 0; idx_vtx < n_vertices; idx_vtx++)
    {
        VertexPtr vtx = test_mesh->primal_vertices[idx_vtx];
        auto adjacent_faces = vtx->getOneRingFaces();
        auto outgoingHalfedges = vtx->getOutgoingHalfEdges();

        EXPECT_GT(adjacent_faces.size(), 0);
        EXPECT_GT(outgoingHalfedges.size(), 0);

        for (PrimalFacePtr ptr : adjacent_faces)
            ASSERT_NE(ptr, nullptr);

        for (HalfEdgePtr ptr : outgoingHalfedges)
        {
            ASSERT_NE(ptr, nullptr);
            ASSERT_NE(ptr->getStartVertex(), nullptr);
            ASSERT_NE(ptr->getEndVertex(), nullptr);
            ASSERT_NE(ptr->getFlipHalfEdge(), nullptr);
            ASSERT_NE(ptr->getNextHalfEdge(), nullptr);
            ASSERT_NE(ptr->getPrimalFace(), nullptr);
        }

        EXPECT_LT(vtx->index, n_vertices);
    }
}

TEST_F(MeshFixture, CheckVertexGeometry)
{
    int n_vertices = test_mesh->primal_vertices.size();

    Eigen::VectorXd gcurv;
    igl::gaussian_curvature(V0, F0, gcurv);

    Eigen::SparseMatrix<double> mass;
    igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, mass);

    for (int idx_vtx = 0; idx_vtx < n_vertices; idx_vtx++)
    {
        VertexPtr vtx = test_mesh->primal_vertices[idx_vtx];
        EXPECT_NEAR(vtx->voronoi_area, mass.coeff(vtx->index, vtx->index), tol);
    }
}

TEST_F(MeshFixture, CheckBasicSanityHalfEdge)
{
    ASSERT_EQ(test_mesh->hedges.size(), 3 * F0.rows());
}

TEST_F(MeshFixture, CheckGoodInitializationHalfEdge)
{
    for (int hedge_itr = 0; hedge_itr < test_mesh->hedges.size(); hedge_itr++)
    {
        HalfEdgePtr hedge = test_mesh->hedges[hedge_itr];

        ASSERT_NE(hedge->getStartVertex(), nullptr);
        ASSERT_NE(hedge->getEndVertex(), nullptr);
        ASSERT_NE(hedge->getFlipHalfEdge(), nullptr);
        ASSERT_NE(hedge->getNextHalfEdge(), nullptr);
        ASSERT_NE(hedge->getPrimalFace(), nullptr);

        ASSERT_EQ(hedge->getFlipHalfEdge()->getStartVertex(), hedge->getEndVertex());
        ASSERT_EQ(hedge->getFlipHalfEdge()->getEndVertex(), hedge->getStartVertex());
        ASSERT_EQ(hedge->getFlipHalfEdge()->getFlipHalfEdge(), hedge);
        ASSERT_NE(hedge->getFlipHalfEdge()->getNextHalfEdge(), nullptr);
        ASSERT_NE(hedge->getFlipHalfEdge()->getPrimalFace(), nullptr);
    }
}

TEST_F(MeshFixture, SanityCheckFlipFlip)
{
    for (int hedge_itr = 0; hedge_itr < test_mesh->hedges.size(); hedge_itr++)
    {
        HalfEdgePtr hedge = test_mesh->hedges[hedge_itr];
        EXPECT_EQ(hedge, hedge->getFlipHalfEdge()->getFlipHalfEdge());
    }
}

TEST_F(MeshFixture, SanityCheckNext)
{
    for (int hedge_itr = 0; hedge_itr < test_mesh->hedges.size(); hedge_itr++)
    {
        HalfEdgePtr hedge = test_mesh->hedges[hedge_itr];
        PrimalFacePtr face = hedge->getPrimalFace();

        EXPECT_EQ(hedge, hedge->getNextHalfEdge()->getNextHalfEdge()->getNextHalfEdge());
        EXPECT_EQ(face, hedge->getNextHalfEdge()->getPrimalFace());
        EXPECT_EQ(face, hedge->getNextHalfEdge()->getNextHalfEdge()->getPrimalFace());
        EXPECT_EQ(face, hedge->getNextHalfEdge()->getNextHalfEdge()->getNextHalfEdge()->getPrimalFace());
    }
}

TEST_F(MeshFixture, SanityCheckPrimalFace)
{
    for (int hedge_itr = 0; hedge_itr < test_mesh->hedges.size(); hedge_itr++)
    {
        HalfEdgePtr hedge = test_mesh->hedges[hedge_itr];

        ASSERT_NE(hedge->getPrimalFace(), nullptr);
        ASSERT_NE(hedge->getFlipHalfEdge()->getPrimalFace(), nullptr);
        ASSERT_NE(hedge->getNextHalfEdge()->getPrimalFace(), nullptr);
        ASSERT_EQ(
            hedge->getPrimalFace(),
            hedge->getFlipHalfEdge()->getFlipHalfEdge()->getPrimalFace()
        );
    }
}

TEST_F(MeshFixture, CheckLaplacianWithLibigl)
{
    SpMat L_check;
    igl::cotmatrix(V0, F0, L_check);

    for (int k = 0; k < L_check.outerSize(); ++k)
    {
        for (Eigen::SparseMatrix<double>::InnerIterator it(L_check, k); it; ++it)
        {
            int i = it.row();
            int j = it.col();
            EXPECT_NEAR(it.value(), test_mesh->L.coeffRef(i, j), laptol);
        }
    }
}

TEST_F(MeshFixture, CheckAreaMatrixWithLibigl)
{
    SpMat Area_check;
    igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, Area_check);

    for (int k = 0; k < Area_check.outerSize(); ++k)
    {
        for (Eigen::SparseMatrix<double>::InnerIterator it(Area_check, k); it; ++it)
        {
            int i = it.row();
            int j = it.col();
            EXPECT_NEAR(it.value(), test_mesh->AreaMatrix.coeffRef(i, j), laptol);
        }
    }
}