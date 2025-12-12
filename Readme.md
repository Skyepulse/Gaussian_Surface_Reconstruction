# Splatting Methods for Surface Reconstruction
## The four methods
https://trianglesplatting.github.io/ triangle splatting
https://trianglesplatting2.github.io/ triangle-splatting+ , improvement on triangle splatting
https://github.com/Anttwo/SuGaR SuGaR
https://github.com/Anttwo/MILo# MILo
https://arxiv.org/pdf/2506.18575? another approach to triangle splatting 2D

### Triangle Splatting
idea : be more efficient than NeRF or Gaussian Splatting by using a GPU primitive
Problem : triangles are not easy to differentiate
Cannot use a mesh when topology is unknown
#### Rendering triangles params
Params : vertices $v_i$ , color $c$, smoothness $\sigma$ and opacity $o$ .
Project the triangles using camera, with matrix $K$
Render triangles according to function $I$ a window function --> the idea is that inside the triangle I(p) = 1 ; whereas it is null outside
#### Prune and densify triangles
during rasterization, prune all triangles under a threshold $\tau$ corresponding to $T*o$ ,
prune triangles that are less than a pixel

Densification :
Using probabilities MCMC by Kheradmand et al. Allocate proportionnally to opacity and sharpness $1/\sigma$ 
a high $\sigma$ corresponds to places in the mesh with a lot of overlap (already high density), and a low $\sigma$ to low density areas.

Adding triangles by a midpoint subdivision, making one triangle into 4 more, unless triangle is too small; then it is cloned and moved with some noise.

#### Optimize triangle Params to fit images
Dataset : images, camera parameters (calibrated via SfM) --> produces a sparse point cloud, and xcreate triangle for each 3D point in that cloud.
Optimize the parameters of all triangles by minimizing rendering error from POVs.


### SuGaR
Mesh extraction from 3D gaussian splatting
Modifying gaussian splatting with a regularization term so that gaussians align with the surface. Use poisson reconstruction for the surface.
Optimize mesh and gaussians through rendering as a final step

- Loss term
Density function $d$ 
$d(p) = \sum_g \alpha_g exp(\frac{-1}{2}(p-\mu_g)^T*\Sigma_g^{-1}(p-\mu_g))

$\alpha_g$ alpha blending of the gaussian
decreases exponentially  $p$ as the position $\mu$ the position of the gaussian and $\Sigma$ its covariance (the bigger the covariance, the bigger the gaussian, --> the point counts more)

The gaussian closest to a pixel is the one who contributes more, so adapt the formula of the sum to contain only one gaussian, where the gaussian is the one with the largest contribution to the pixel.


To get the gaussians to lay flat : get the contribution to be equal to $\frac{1}{s_g^2} < p-\mu_g, n_g >^2$ where $n_g$ is the direction of the axis of the smallest scaling factor of the gaussian. (supposing they are flat on x, y or z), so $n_g$ is $x , y , z$.

Density is then approximated by : $d(p) = exp(-\frac{1}{2 s_g^2}) < p-\mu_g, n_g >^2$  where $g$ is a gaussian. Furthermore, switching loss to SDF rather than density increases the alignment of gaussians.

$f(p) = +- s_{g}\sqrt{+2log(d(p))}$
as the distance function associated with the density.

The regularization term is an average of the distance between current $f'(p)$  and ideal $f(p)$ which corresponds to the ideal surface (hypothesis we made about alignment and little overlap).

Computing $f'$ (non ideal SDF) is complex, using the depth maps of the gaussian splat during training. Then $f'$ is difference between depth of $p$ and projected depth map. Also add regularisation term along the normal to align the gaussians with the normal.

- Method to extract mesh
Sample points along the gaussian distribution and levelset of the density. Run a poisson recontruction on these points.
Or use the the normals of the SDF as points (better).

Problem is using points on the corect level set : use the depth maps. For every pixel m , sample along line of sight. Compute density values of all selected points . Select the two points st $d(p_i) < \lambda < d(p_j)$ where $\lambda$ is a parameter to pick the level set. Interpolate between the points st $d(p+t*v) = \lambda$

- optimization

Bind gaussians to mesh triangle and optimize mesh and gaussians together : mesh editable, gaussians provide good rendering. One gaussian per triangle. Reduces the learnable parameters on the gaussians, lose one axis of rotation to keep them flat on the surgace.
### MILo


## Metrics
