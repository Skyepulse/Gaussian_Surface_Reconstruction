#undef cross
#define GLM_FORCE_PURE
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <fstream>
#include <stdexcept>
#include "mesh_load.h"
#include <igl/stb/read_image.h>
#include <igl/readOBJ.h>
#include <igl/readPLY.h>
#include <algorithm>
#include <iostream>
#include <climits>


using namespace std;

//=============================== Mesh Load =============================//
void meshLoadCOFF(mesh& m)
{
    string path = m.file_path;
    cout << "Loading COFF mesh from file: " << path << endl;

    // Check File Exists
    ifstream f(path);
    if (!f.is_open()) 
    {
        std::cerr << "[meshLoadCOFF] Path does not exist: " << path << std::endl;
        throw runtime_error("[meshLoadCOFF] Path does not exist: " + path);
    }

    string header;
    f >> header;
    if (header != "COFF" && header != "OFF")
    {
        throw runtime_error("[meshLoadCOFF] Invalid COFF header: " + header);
        std::cerr << "[meshLoadCOFF] Invalid COFF header: " << header << std::endl;
    }

    size_t nV = 0, nF = 0, nE = 0;
    f >> nV >> nF >> nE;
    
    m.V = Eigen::MatrixXd(nV, 3);
    m.F = Eigen::MatrixXi(nF, 3);
    m.faceColor = Eigen::MatrixXd(nF, 3);

    // Read vertices
    for (size_t i = 0; i < nV; ++i) {
        double x, y, z;
        f >> x >> y >> z;
        m.V.row(i) << x, y, z;
    }

    // Read faces + colors
    for (size_t i = 0; i < nF; ++i) {
        int n, v0, v1, v2;
        int r, g, b, a = 255;

        if (!(f >> n >> v0 >> v1 >> v2 >> r >> g >> b >> a))
            throw runtime_error("[meshLoadCOFF] Malformed COFF line at face " + to_string(i));

        if (n != 3)
        throw runtime_error("[meshLoadCOFF] Non-triangle face encountered, expected '3' at face " + to_string(i));

        m.F.row(i) << v0, v1, v2;
        m.faceColor.row(i) << r / 255.0, g / 255.0, b / 255.0;
    }
    f.close();
}

//================================//
void meshLoadOBJ(mesh& m)
{
    ifstream f(m.file_path);
    if (!f.is_open()) 
        throw runtime_error("[meshLoadCOFF] Path does not exist: " + m.file_path);

    igl::readOBJ(m.file_path, m.V, m.UV, m.N, m.F, m.FUV, m.FN);
}

//================================//
void meshLoadPLY(mesh& m)
{
    string path = m.file_path;

    // Check File Exists
    ifstream f(path);
    if (!f.is_open()) 
        throw runtime_error("[meshLoadPLY] Path does not exist: " + path);

    igl::readPLY(path, m.V, m.F);    
}