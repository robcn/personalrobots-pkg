/*
 * Copyright (c) 2008 Radu Bogdan Rusu <rusu -=- cs.tum.edu>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

/** \author Radu Bogdan Rusu */

#include <point_cloud_mapping/geometry/angles.h>
#include <point_cloud_mapping/geometry/nearest.h>
#include <point_cloud_mapping/geometry/point.h>
#include <point_cloud_mapping/geometry/statistics.h>

#include <point_cloud_mapping/kdtree/kdtree.h>
#include <point_cloud_mapping/kdtree/kdtree_ann.h>

namespace cloud_geometry
{

  namespace nearest
  {

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the centroid of a set of points and return it as a PointCloud message with 1 value.
      * \param points the input point cloud
      * \param centroid the output centroid
      */
    void
      computeCentroid (const robot_msgs::PointCloud &points, robot_msgs::PointCloud &centroid)
    {
      // Prepare the data output
      centroid.pts.resize (1);
      centroid.pts[0].x = centroid.pts[0].y = centroid.pts[0].z = 0;
      centroid.chan.resize (points.get_chan_size ());
      for (unsigned int d = 0; d < points.get_chan_size (); d++)
      {
        centroid.chan[d].name = points.chan[d].name;
        centroid.chan[d].vals.resize (1);
      }

      // For each point in the cloud
      for (unsigned int i = 0; i < points.get_pts_size (); i++)
      {
        centroid.pts[0].x += points.pts[i].x;
        centroid.pts[0].y += points.pts[i].y;
        centroid.pts[0].z += points.pts[i].z;

        for (unsigned int d = 0; d < points.get_chan_size (); d++)
          centroid.chan[d].vals[0] += points.chan[d].vals[i];
      }

      centroid.pts[0].x /= points.get_pts_size ();
      centroid.pts[0].y /= points.get_pts_size ();
      centroid.pts[0].z /= points.get_pts_size ();
      for (unsigned int d = 0; d < points.get_chan_size (); d++)
        centroid.chan[d].vals[0] /= points.get_pts_size ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the centroid of a set of points using their indices and return it as a PointCloud message with 1 value.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param centroid the output centroid
      */
    void
      computeCentroid (const robot_msgs::PointCloud &points, const std::vector<int> &indices, robot_msgs::PointCloud &centroid)
    {
      // Prepare the data output
      centroid.pts.resize (1);
      centroid.pts[0].x = centroid.pts[0].y = centroid.pts[0].z = 0;
      centroid.chan.resize (points.get_chan_size ());
      for (unsigned int d = 0; d < points.get_chan_size (); d++)
      {
        centroid.chan[d].name = points.chan[d].name;
        centroid.chan[d].vals.resize (1);
      }

      // For each point in the cloud
      for (unsigned int i = 0; i < indices.size (); i++)
      {
        centroid.pts[0].x += points.pts.at (indices.at (i)).x;
        centroid.pts[0].y += points.pts.at (indices.at (i)).y;
        centroid.pts[0].z += points.pts.at (indices.at (i)).z;

        for (unsigned int d = 0; d < points.get_chan_size (); d++)
          centroid.chan[d].vals[0] += points.chan[d].vals.at (indices.at (i));
      }

      centroid.pts[0].x /= indices.size ();
      centroid.pts[0].y /= indices.size ();
      centroid.pts[0].z /= indices.size ();
      for (unsigned int d = 0; d < points.get_chan_size (); d++)
        centroid.chan[d].vals[0] /= indices.size ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the eigenvalues and eigenvectors of a given surface patch
      * \param points the input point cloud
      * \param eigen_vectors the resultant eigenvectors
      * \param eigen_values the resultant eigenvalues
      */
    void
      computePatchEigen (const robot_msgs::PointCloud &points, Eigen::Matrix3d &eigen_vectors, Eigen::Vector3d &eigen_values)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      eigen_values = ei_symm.eigenvalues ();
      eigen_vectors = ei_symm.eigenvectors ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the eigenvalues and eigenvectors of a given surface patch
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param eigen_vectors the resultant eigenvectors
      * \param eigen_values the resultant eigenvalues
      */
    void
      computePatchEigen (const robot_msgs::PointCloud &points, const std::vector<int> &indices, Eigen::Matrix3d &eigen_vectors, Eigen::Vector3d &eigen_values)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, indices, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      eigen_values = ei_symm.eigenvalues ();
      eigen_vectors = ei_symm.eigenvectors ();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the Least-Squares plane fit for a given set of points, and return the estimated plane parameters
      * together with the surface curvature.
      * \param points the input point cloud
      * \param plane_parameters the plane parameters as: a, b, c, d (ax + by + cz + d = 0)
      * \param curvature the estimated surface curvature as a measure of
      * \f[
      * \lambda_0 / (\lambda_0 + \lambda_1 + \lambda_2)
      * \f]
      */
    void
      computePointNormal (const robot_msgs::PointCloud &points, Eigen::Vector4d &plane_parameters, double &curvature)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      Eigen::Vector3d eigen_values  = ei_symm.eigenvalues ();
      Eigen::Matrix3d eigen_vectors = ei_symm.eigenvectors ();

      // Normalize the surface normal (eigenvector corresponding to the smallest eigenvalue)
      double norm = sqrt ( eigen_vectors (0, 0) * eigen_vectors (0, 0) +
                           eigen_vectors (0, 1) * eigen_vectors (0, 1) +
                           eigen_vectors (0, 2) * eigen_vectors (0, 2));
      plane_parameters (0) = eigen_vectors (0, 0) / norm;
      plane_parameters (1) = eigen_vectors (0, 1) / norm;
      plane_parameters (2) = eigen_vectors (0, 2) / norm;

      // Hessian form (D = nc . p_plane (centroid here) + p)
      plane_parameters (3) = -1 * (plane_parameters (0) * centroid.x + plane_parameters (1) * centroid.y + plane_parameters (2) * centroid.z);

      // Compute the curvature surface change
      curvature = fabs ( eigen_values (0) / (eigen_values (0) + eigen_values (1) + eigen_values (2)) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the Least-Squares plane fit for a given set of points, using their indices,
      * and return the estimated plane parameters together with the surface curvature.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param plane_parameters the plane parameters as: a, b, c, d (ax + by + cz + d = 0)
      * \param curvature the estimated surface curvature as a measure of
      * \f[
      * \lambda_0 / (\lambda_0 + \lambda_1 + \lambda_2)
      * \f]
      */
    void
      computePointNormal (const robot_msgs::PointCloud &points, const std::vector<int> &indices, Eigen::Vector4d &plane_parameters, double &curvature)
    {
      robot_msgs::Point32 centroid;
      // Compute the 3x3 covariance matrix
      Eigen::Matrix3d covariance_matrix;
      computeCovarianceMatrix (points, indices, covariance_matrix, centroid);

      // Extract the eigenvalues and eigenvectors
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> ei_symm (covariance_matrix);
      Eigen::Vector3d eigen_values  = ei_symm.eigenvalues ();
      Eigen::Matrix3d eigen_vectors = ei_symm.eigenvectors ();

      // Normalize the surface normal (eigenvector corresponding to the smallest eigenvalue)
      // Note: Remember to take care of the eigen_vectors ordering
      double norm = sqrt ( eigen_vectors (0, 0) * eigen_vectors (0, 0) +
                           eigen_vectors (1, 0) * eigen_vectors (1, 0) +
                           eigen_vectors (2, 0) * eigen_vectors (2, 0));
      plane_parameters (0) = eigen_vectors (0, 0) / norm;
      plane_parameters (1) = eigen_vectors (1, 0) / norm;
      plane_parameters (2) = eigen_vectors (2, 0) / norm;

      // Hessian form (D = nc . p_plane (centroid here) + p)
      plane_parameters (3) = -1 * (plane_parameters (0) * centroid.x + plane_parameters (1) * centroid.y + plane_parameters (2) * centroid.z);

      // Compute the curvature surface change
      curvature = fabs ( eigen_values (0) / (eigen_values (0) + eigen_values (1) + eigen_values (2)) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the 3 moment invariants (j1, j2, j3) for a given set of points.
      * \param points the input point cloud
      * \param j1 the first moment invariant
      * \param j2 the second moment invariant
      * \param j3 the third moment invariant
      */
    void
      computeMomentInvariants (const robot_msgs::PointCloud &points, double &j1, double &j2, double &j3)
    {
      // Compute the centroid
      robot_msgs::Point32 centroid;
      computeCentroid (points, centroid);

      // Demean the pointset
      robot_msgs::PointCloud points_c;
      points_c.pts.resize (points.pts.size ());
      for (unsigned int i = 0; i < points.pts.size (); i++)
      {
        points_c.pts[i].x = points.pts[i].x - centroid.x;
        points_c.pts[i].y = points.pts[i].y - centroid.y;
        points_c.pts[i].z = points.pts[i].z - centroid.z;
      }

      double mu200 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 2.0, 0.0, 0.0);
      double mu020 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 2.0, 0.0);
      double mu002 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 0.0, 2.0);
      double mu110 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 1.0, 1.0, 0.0);
      double mu101 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 1.0, 0.0, 1.0);
      double mu011 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 1.0, 1.0);

      j1 = mu200 + mu020 + mu002;
      j2 = mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110*mu110 - mu101*mu101 - mu011*mu011;
      j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110*mu110 - mu020*mu101*mu101 - mu200*mu011*mu011;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Compute the 3 moment invariants (j1, j2, j3) for a given set of points, using their indices.
      * \param points the input point cloud
      * \param indices the point cloud indices that need to be used
      * \param j1 the first moment invariant
      * \param j2 the second moment invariant
      * \param j3 the third moment invariant
      */
    void
      computeMomentInvariants (const robot_msgs::PointCloud &points, const std::vector<int> &indices, double &j1, double &j2, double &j3)
    {
      // Compute the centroid
      robot_msgs::Point32 centroid;
      computeCentroid (points, indices, centroid);

      // Demean the pointset
      robot_msgs::PointCloud points_c;
      points_c.pts.resize (indices.size ());
      for (unsigned int i = 0; i < indices.size (); i++)
      {
        points_c.pts[i].x = points.pts.at (indices.at (i)).x - centroid.x;
        points_c.pts[i].y = points.pts.at (indices.at (i)).y - centroid.y;
        points_c.pts[i].z = points.pts.at (indices.at (i)).z - centroid.z;
      }

      double mu200 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 2.0, 0.0, 0.0);
      double mu020 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 2.0, 0.0);
      double mu002 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 0.0, 2.0);
      double mu110 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 1.0, 1.0, 0.0);
      double mu101 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 1.0, 0.0, 1.0);
      double mu011 = cloud_geometry::statistics::computeCentralizedMoment (points_c, 0.0, 1.0, 1.0);

      j1 = mu200 + mu020 + mu002;
      j2 = mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110*mu110 - mu101*mu101 - mu011*mu011;
      j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110*mu110 - mu020*mu101*mu101 - mu200*mu011*mu011;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Check whether a point is a boundary point in a planar patch of projected points given by indices.
      * \note A coordinate system u-v-n must be computed a-priori using \a getCoordinateSystemOnPlane
      * \param points a pointer to the input point cloud
      * \param q_idx the index of the query point in \a points
      * \param neighbors the estimated point neighbors of the query point
      * \param u the u direction
      * \param v the v direction
      * \param angle_threshold the threshold angle (default $\pi / 2.0$)
      */
    bool
      isBoundaryPoint (const robot_msgs::PointCloud &points, int q_idx, const std::vector<int> &neighbors,
                       const Eigen::Vector3d& u, const Eigen::Vector3d& v, double angle_threshold)
    {
      if (neighbors.size () < 3)
        return (false);
      double uvn_nn[2];
      // Compute the angles between each neighbouring point and the query point itself
      std::vector<double> angles;
      angles.reserve (neighbors.size ());
      for (unsigned int i = 0; i < neighbors.size (); i++)
      {
        uvn_nn[0] = (points.pts.at (neighbors.at (i)).x - points.pts.at (q_idx).x) * u (0) +
                    (points.pts.at (neighbors.at (i)).y - points.pts.at (q_idx).y) * u (1) +
                    (points.pts.at (neighbors.at (i)).z - points.pts.at (q_idx).z) * u (2);
        uvn_nn[1] = (points.pts.at (neighbors.at (i)).x - points.pts.at (q_idx).x) * v (0) +
                    (points.pts.at (neighbors.at (i)).y - points.pts.at (q_idx).y) * v (1) +
                    (points.pts.at (neighbors.at (i)).z - points.pts.at (q_idx).z) * v (2);
        if (uvn_nn[0] == 0 && uvn_nn[1] == 0)
          continue;
        angles.push_back (cloud_geometry::angles::getAngle2D (uvn_nn));
      }
      sort (angles.begin (), angles.end ());

      // Compute the maximal angle difference between two consecutive angles
      double max_dif = DBL_MIN, dif;
      for (unsigned int i = 0; i < angles.size () - 1; i++)
      {
        dif = angles[i + 1] - angles[i];
        if (max_dif < dif)
          max_dif = dif;
      }
      // Get the angle difference between the last and the first
      dif = 2 * M_PI - angles[angles.size () - 1] + angles[0];
      if (max_dif < dif)
        max_dif = dif;

      // Check results
      if (max_dif > angle_threshold)
        return (true);
      else
        return (false);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Estimate (in place) the point normals and surface curvatures for a given point cloud dataset (points)
      * using the data from a different point cloud (surface) for least-squares planar estimation.
      * \param points the input point cloud (on output, 4 extra channels will be added: \a nx, \a ny, \a nz, and \a curvatures)
      * \param surface the point cloud data to use for least-squares planar estimation
      * \param k use a fixed number of k-nearest neighbors for the least-squares fit
      * \param viewpoint the viewpoint where the cloud was acquired from (used for normal flip)
      */
    void
      computePointCloudNormals (robot_msgs::PointCloud &points, const robot_msgs::PointCloud &surface, int k,
                                const robot_msgs::PointStamped &viewpoint)
    {
      int nr_points = points.pts.size ();
      int orig_dims = points.chan.size ();
      points.chan.resize (orig_dims + 4);                     // Reserve space for 4 channels: nx, ny, nz, curvature
      points.chan[orig_dims + 0].name = "nx";
      points.chan[orig_dims + 1].name = "ny";
      points.chan[orig_dims + 2].name = "nz";
      points.chan[orig_dims + 3].name = "curvatures";
      for (unsigned int d = orig_dims; d < points.chan.size (); d++)
        points.chan[d].vals.resize (nr_points);

      cloud_kdtree::KdTree *kdtree = new cloud_kdtree::KdTreeANN (surface);

//#pragma omp parallel for schedule(dynamic)
      for (int i = 0; i < nr_points; i++)                     // Get the nearest neighbors for all the point indices in the bounds
      {
        std::vector<int> nn_indices;
        std::vector<float> nn_distances;
        kdtree->nearestKSearch (points.pts[i], k, nn_indices, nn_distances);

        Eigen::Vector4d plane_parameters;                     // Compute the point normals (nx, ny, nz), surface curvature estimates (c)
        double curvature;
        computePointNormal (surface, nn_indices, plane_parameters, curvature);

        cloud_geometry::angles::flipNormalTowardsViewpoint (plane_parameters, points.pts[i], viewpoint);

        points.chan[orig_dims + 0].vals[i] = plane_parameters (0);
        points.chan[orig_dims + 1].vals[i] = plane_parameters (1);
        points.chan[orig_dims + 2].vals[i] = plane_parameters (2);
        points.chan[orig_dims + 3].vals[i] = curvature;
      }
      delete kdtree;                                          // Delete the kd-tree
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Estimate (in place) the point normals and surface curvatures for a given point cloud dataset (points)
      * using the data from a different point cloud (surface) for least-squares planar estimation.
      * \param points the input point cloud (on output, 4 extra channels will be added: \a nx, \a ny, \a nz, and \a curvatures)
      * \param surface the point cloud data to use for least-squares planar estimation
      * \param radius use a neighbor fixed radius search for the least-squares fit
      * \param viewpoint the viewpoint where the cloud was acquired from (used for normal flip)
      */
    void
      computePointCloudNormals (robot_msgs::PointCloud &points, const robot_msgs::PointCloud &surface, double radius,
                                const robot_msgs::PointStamped &viewpoint)
    {
      int nr_points = points.pts.size ();
      int orig_dims = points.chan.size ();
      points.chan.resize (orig_dims + 4);                     // Reserve space for 4 channels: nx, ny, nz, curvature
      points.chan[orig_dims + 0].name = "nx";
      points.chan[orig_dims + 1].name = "ny";
      points.chan[orig_dims + 2].name = "nz";
      points.chan[orig_dims + 3].name = "curvatures";
      for (unsigned int d = orig_dims; d < points.chan.size (); d++)
        points.chan[d].vals.resize (nr_points);

      cloud_kdtree::KdTree *kdtree = new cloud_kdtree::KdTreeANN (surface);

//#pragma omp parallel for schedule(dynamic)
      for (int i = 0; i < nr_points; i++)                     // Get the nearest neighbors for all the point indices in the bounds
      {
        std::vector<int> nn_indices;
        std::vector<float> nn_distances;
        kdtree->radiusSearch (points.pts[i], radius, nn_indices, nn_distances);

        Eigen::Vector4d plane_parameters;                     // Compute the point normals (nx, ny, nz), surface curvature estimates (c)
        double curvature;
        computePointNormal (surface, nn_indices, plane_parameters, curvature);

        cloud_geometry::angles::flipNormalTowardsViewpoint (plane_parameters, points.pts[i], viewpoint);

        points.chan[orig_dims + 0].vals[i] = plane_parameters (0);
        points.chan[orig_dims + 1].vals[i] = plane_parameters (1);
        points.chan[orig_dims + 2].vals[i] = plane_parameters (2);
        points.chan[orig_dims + 3].vals[i] = curvature;
      }
      delete kdtree;                                          // Delete the kd-tree
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Estimate (in place) the point normals and surface curvatures for a given point cloud dataset (points)
      * \param points the input point cloud (on output, 4 extra channels will be added: \a nx, \a ny, \a nz, and \a curvatures)
      * \param k use a fixed number of k-nearest neighbors for the least-squares fit
      * \param viewpoint the viewpoint where the cloud was acquired from (used for normal flip)
      */
    void
      computePointCloudNormals (robot_msgs::PointCloud &points, int k, const robot_msgs::PointStamped &viewpoint)
    {
      int nr_points = points.pts.size ();
      int orig_dims = points.chan.size ();
      points.chan.resize (orig_dims + 4);                     // Reserve space for 4 channels: nx, ny, nz, curvature
      points.chan[orig_dims + 0].name = "nx";
      points.chan[orig_dims + 1].name = "ny";
      points.chan[orig_dims + 2].name = "nz";
      points.chan[orig_dims + 3].name = "curvatures";
      for (unsigned int d = orig_dims; d < points.chan.size (); d++)
        points.chan[d].vals.resize (nr_points);

      // Peter: timing the different pieces of this
      ros::Time ts = ros::Time::now();
      cloud_kdtree::KdTree *kdtree = new cloud_kdtree::KdTreeANN (points);
      ROS_INFO("KdTree created in %f seconds", (ros::Time::now () - ts).toSec ());

      double total_search_time = 0.0, total_normal_time = 0.0;

//#pragma omp parallel for schedule(dynamic)
      for (int i = 0; i < nr_points; i++)                     // Get the nearest neighbors for all the point indices in the bounds
      {
        std::vector<int> nn_indices;
        std::vector<float> nn_distances;

	ros::Time ts_search = ros::Time::now();
        kdtree->nearestKSearch (points.pts[i], k, nn_indices, nn_distances);
	total_search_time += (ros::Time::now () - ts_search).toSec ();

        Eigen::Vector4d plane_parameters;                     // Compute the point normals (nx, ny, nz), surface curvature estimates (c)
        double curvature;

	ros::Time ts_normal = ros::Time::now();
        computePointNormal (points, nn_indices, plane_parameters, curvature);
	total_normal_time += (ros::Time::now () - ts_normal).toSec ();

        cloud_geometry::angles::flipNormalTowardsViewpoint (plane_parameters, points.pts[i], viewpoint);

        points.chan[orig_dims + 0].vals[i] = plane_parameters (0);
        points.chan[orig_dims + 1].vals[i] = plane_parameters (1);
        points.chan[orig_dims + 2].vals[i] = plane_parameters (2);
        points.chan[orig_dims + 3].vals[i] = curvature;
      }
      ROS_INFO("Breakdown: %f seconds to search, %f seconds to compute normals", total_search_time, total_normal_time);

      delete kdtree;                                          // Delete the kd-tree
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /** \brief Estimate (in place) the point normals and surface curvatures for a given point cloud dataset (points)
      * \param points the input point cloud (on output, 4 extra channels will be added: \a nx, \a ny, \a nz, and \a curvatures)
      * \param radius use a neighbor fixed radius search for the least-squares fit
      * \param viewpoint the viewpoint where the cloud was acquired from (used for normal flip)
      */
    void
      computePointCloudNormals (robot_msgs::PointCloud &points, double radius, const robot_msgs::PointStamped &viewpoint)
    {
      int nr_points = points.pts.size ();
      int orig_dims = points.chan.size ();
      points.chan.resize (orig_dims + 4);                     // Reserve space for 4 channels: nx, ny, nz, curvature
      points.chan[orig_dims + 0].name = "nx";
      points.chan[orig_dims + 1].name = "ny";
      points.chan[orig_dims + 2].name = "nz";
      points.chan[orig_dims + 3].name = "curvatures";
      for (unsigned int d = orig_dims; d < points.chan.size (); d++)
        points.chan[d].vals.resize (nr_points);

      cloud_kdtree::KdTree *kdtree = new cloud_kdtree::KdTreeANN (points);

//#pragma omp parallel for schedule(dynamic)
      for (int i = 0; i < nr_points; i++)                     // Get the nearest neighbors for all the point indices in the bounds
      {
        std::vector<int> nn_indices;
        std::vector<float> nn_distances;
        kdtree->radiusSearch (points.pts[i], radius, nn_indices, nn_distances);

        Eigen::Vector4d plane_parameters;                     // Compute the point normals (nx, ny, nz), surface curvature estimates (c)
        double curvature;
        computePointNormal (points, nn_indices, plane_parameters, curvature);

        cloud_geometry::angles::flipNormalTowardsViewpoint (plane_parameters, points.pts[i], viewpoint);

        points.chan[orig_dims + 0].vals[i] = plane_parameters (0);
        points.chan[orig_dims + 1].vals[i] = plane_parameters (1);
        points.chan[orig_dims + 2].vals[i] = plane_parameters (2);
        points.chan[orig_dims + 3].vals[i] = curvature;
      }
      delete kdtree;                                          // Delete the kd-tree
    }

  }
}