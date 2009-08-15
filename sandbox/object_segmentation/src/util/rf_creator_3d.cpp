/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <object_segmentation/util/rf_creator_3d.h>

using namespace std;

// --------------------------------------------------------------
/*
 * \brief Create nodes in the random field
 */
// --------------------------------------------------------------
void RFCreator3D::createNodes(RandomField& rf,
                              const sensor_msgs::PointCloud& pt_cloud,
                              cloud_kdtree::KdTree& pt_cloud_kdtree,
                              const vector<unsigned int>& labels,
                              const bool use_only_labeled,
                              set<unsigned int>& node_indices)

{
  unsigned int nbr_cloud_pts = pt_cloud.points.size();
  if (labels.size() != nbr_cloud_pts)
  {
    ROS_FATAL("Number of labels does not match number of points");
    abort();
  }

  // ----------------------------------------------
  // Create interests points over the whole point cloud
  cv::Vector<const geometry_msgs::Point32*> interest_pts;
  interest_pts.reserve(nbr_cloud_pts);
  map<unsigned int, unsigned int> interest2cloud_idx;
  unsigned int interest_idx = 0;
  for (unsigned int i = 0 ; i < nbr_cloud_pts ; i++)
  {
    if (use_only_labeled)
    {
      if (labels[i] != RandomField::UNKNOWN_LABEL)
      {
        interest_pts.push_back(&(pt_cloud.points[i]));
        interest2cloud_idx[interest_idx] = i;
        interest_idx++;
      }
    }
    else
    {
      interest_pts.push_back(&(pt_cloud.points[i]));
      interest2cloud_idx[interest_idx] = i;
      interest_idx++;
    }
  }

  // ----------------------------------------------
  // Compute features over all point cloud
  vector<boost::shared_array<const float> > concatenated_features;
  unsigned int nbr_concatenated_vals = Descriptor3D::computeAndConcatFeatures(pt_cloud,
      pt_cloud_kdtree, interest_pts, node_feature_descriptors_, concatenated_features);
  if (nbr_concatenated_vals == 0)
  {
    ROS_FATAL("Could not compute node features at all. This should never happen");
    abort();
  }

  // ----------------------------------------------
  // Create nodes for features that were okay
  node_indices.clear();
  unsigned int nbr_created_nodes = 0;
  unsigned int nbr_interest_pts = interest_pts.size();
  for (unsigned int i = 0 ; i < nbr_interest_pts ; i++)
  {
    // NULL indicates couldnt compute features for interest point
    if (concatenated_features[i].get() != NULL)
    {
      unsigned int curr_pt_cloud_idx = interest2cloud_idx[i];
      if (rf.createNode(curr_pt_cloud_idx, concatenated_features[i], nbr_concatenated_vals,
          labels[curr_pt_cloud_idx], pt_cloud.points[curr_pt_cloud_idx].x,
          pt_cloud.points[curr_pt_cloud_idx].y, pt_cloud.points[curr_pt_cloud_idx].z) < 0)
      {
        ROS_FATAL("Could not create node for point %u in point cloud", curr_pt_cloud_idx);
        abort();
      }
      else
      {
        node_indices.insert(curr_pt_cloud_idx);
        nbr_created_nodes++;
      }
    }
  }

  ROS_INFO("    @@@@@@@@@@ Created %u nodes from %u total points @@@@@@@@@@", nbr_created_nodes, nbr_cloud_pts);
}

// --------------------------------------------------------------
/*!
 * \brief Create clique set in the RandomField using kmeans clustering
 */
// --------------------------------------------------------------
void RFCreator3D::createCliqueSet(RandomField& rf,
                                  const sensor_msgs::PointCloud& pt_cloud,
                                  cloud_kdtree::KdTree& pt_cloud_kdtree,
                                  const set<unsigned int>& node_indices,
                                  const unsigned int clique_set_idx)
{
  const map<unsigned int, RandomField::Node*>& rf_nodes = rf.getNodesRandomFieldIDs();
  vector<pair<bool, point_cloud_clustering::PointCloudClustering*> >& clique_set_info =
      clique_set_clusterings_[clique_set_idx];

  // total number of clusters and cliques created from this function call
  unsigned int nbr_created_cliques = 0;
  unsigned int nbr_created_clusters = 0;

  unsigned int nbr_constructions = clique_set_info.size();
  for (unsigned int i = 0 ; i < nbr_constructions ; i++)
  {
    // ----------------------------------------------
    // Clustering
    // ----------------------------------------------
    // create new clusters starting from previous count
    clique_set_info[i].second->setStartingClusterLabel(nbr_created_clusters);
    bool cluster_only_nodes = clique_set_info[i].first;
    map<unsigned int, vector<int> > created_clusters;
    int cluster_ret_val = -1;
    if (cluster_only_nodes)
    {
      cluster_ret_val = clique_set_info[i].second->cluster(pt_cloud, pt_cloud_kdtree, node_indices,
          created_clusters);
    }
    else
    {
      cluster_ret_val = clique_set_info[i].second->cluster(pt_cloud, pt_cloud_kdtree,
          created_clusters);
    }
    if (cluster_ret_val < 0)
    {
      ROS_FATAL("Could not perform clustering %u for clique set %u", i, clique_set_idx);
      abort();
    }
    unsigned int curr_nbr_clusters = created_clusters.size();
    nbr_created_clusters += curr_nbr_clusters;
    // -----------------------------
    // compute centroids for each cluster
    map<unsigned int, vector<float> > cluster_centroids;
    point_cloud_clustering::PointCloudClustering::computeClusterCentroids(pt_cloud,
        created_clusters, cluster_centroids);

    // !!!!!!!!!!!!!!! TEMPO Print: x y z cluster_label !!!!!!!!!!!!!!!!!!!!!!!!!
    /*
    for (map<unsigned int, vector<int> >::iterator iter_created_clusters = created_clusters.begin() ; iter_created_clusters
        != created_clusters.end() ; iter_created_clusters++)
    {
      unsigned int curr_cluster_label = iter_created_clusters->first;
      vector<int>& cluster_pt_indices = iter_created_clusters->second;
      for (unsigned int poop = 0 ; poop < cluster_pt_indices.size() ; poop++)
      {
        cout << pt_cloud.points[cluster_pt_indices[poop]].x << " "
            << pt_cloud.points[cluster_pt_indices[poop]].y << " "
            << pt_cloud.points[cluster_pt_indices[poop]].z << " " << curr_cluster_label << endl;
      }
    }
    */
    // !!!!!!!!!!!!!!! TEMPO !!!!!!!!!!!!!!!!!!!!!!!!!

    // ----------------------------------------------
    // Feature computation
    // -----------------------------
    // Create interests regions from the clustering
    cv::Vector<const vector<int>*> interest_region_indices(curr_nbr_clusters, NULL);
    size_t cluster_idx = 0;
    for (map<unsigned int, vector<int> >::iterator iter_created_clusters = created_clusters.begin() ; iter_created_clusters
        != created_clusters.end() ; iter_created_clusters++, cluster_idx++)
    {
      interest_region_indices[cluster_idx] = (&iter_created_clusters->second);
    }
    // -----------------------------
    // Compute features over clusters
    vector<boost::shared_array<const float> > concatenated_features;
    unsigned int nbr_concatenated_vals = Descriptor3D::computeAndConcatFeatures(pt_cloud,
        pt_cloud_kdtree, interest_region_indices, clique_set_feature_descriptors_[clique_set_idx],
        concatenated_features);
    if (nbr_concatenated_vals == 0)
    {
      ROS_FATAL("Could not compute cluster features at all. This should never happen");
      abort();
    }

    // ----------------------------------------------
    // Clique construction
    // Assumes that point index in point cloud is equivalent to random field node id
    // -----------------------------
    cluster_idx = 0;
    for (map<unsigned int, vector<int> >::iterator iter_created_clusters = created_clusters.begin() ; iter_created_clusters
        != created_clusters.end() ; iter_created_clusters++, cluster_idx++)
    {
      boost::shared_array<const float> curr_cluster_features = concatenated_features[cluster_idx];

      // Only create cliques where could compute cluster features
      if (curr_cluster_features.get() != NULL)
      {
        // Retrieve point cloud indices within the cluster.
        const vector<int>& curr_cluster_pt_indices = iter_created_clusters->second;
        unsigned int nbr_pts_in_cluster = curr_cluster_pt_indices.size();

        // For each point, find its corresponding node (it may not exist if clustering over all points),
        // and create a list of the nodes that represent the points in the cluster
        list<const RandomField::Node*> clique_nodes;
        unsigned int nbr_nodes_in_clique = 0;
        for (unsigned int j = 0 ; j < nbr_pts_in_cluster ; j++)
        {
          unsigned int curr_pt_idx = curr_cluster_pt_indices[j];
          if (rf_nodes.count(curr_pt_idx) != 0)
          {
            clique_nodes.push_back(rf_nodes.find(curr_pt_idx)->second);
            nbr_nodes_in_clique++;
          }
        }

        // Ensure the clique has at least two nodes
        if (nbr_nodes_in_clique < 2)
        {
          ROS_DEBUG("Skipping clique of size less than 2");
        }
        else
        {
          // Retrieve the cluster's label/id
          const unsigned int curr_cluster_label = iter_created_clusters->first;

          // Retrieve the cluster's centroid
          const vector<float>& centroid = cluster_centroids.find(curr_cluster_label)->second;

          // Create clique using the cluster label as the clique id
          if (rf.createClique(curr_cluster_label, clique_set_idx, clique_nodes,
              curr_cluster_features, nbr_concatenated_vals, centroid[0], centroid[1], centroid[2])
              == NULL)
          {
            ROS_FATAL("Could not create the %u 'th clique with id %u for clique set %u", nbr_created_cliques, curr_cluster_label, clique_set_idx);
            abort();
          }
          else
          {
            nbr_created_cliques++;
          }
        }
      } // end if (curr_cluster_features.get() != NULL)
    } // end iterate over clusters
  }

  ROS_INFO("    ########### Created clique set %u with %u cliques from %u clusters #############", clique_set_idx, nbr_created_cliques, nbr_created_clusters);
}

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
boost::shared_ptr<RandomField> RFCreator3D::createRandomField(const sensor_msgs::PointCloud& pt_cloud,
                                                              const vector<unsigned int>& labels,
                                                              const bool use_only_labeled)
{
  cloud_kdtree::KdTreeANN pt_cloud_kdtree(pt_cloud);

  boost::shared_ptr<RandomField> rf(new RandomField(nbr_clique_sets_));

  ROS_INFO("=============== CREATING RANDOM FIELD =================");

  // ----------------------------------------------------------
  // Create nodes
  set<unsigned int> node_indices;
  createNodes(*rf, pt_cloud, pt_cloud_kdtree, labels, use_only_labeled, node_indices);

  // ----------------------------------------------------------
  // Create clique sets
  for (unsigned int i = 0 ; i < nbr_clique_sets_ ; i++)
  {
    createCliqueSet(*rf, pt_cloud, pt_cloud_kdtree, node_indices, i);
  }

  ROS_INFO("=============== FINISHED RANDOM FIELD =================\n");

  return rf;
}

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
boost::shared_ptr<RandomField> RFCreator3D::createRandomField(const sensor_msgs::PointCloud& pt_cloud)
{
  std::vector<unsigned int> labels(pt_cloud.points.size(), RandomField::UNKNOWN_LABEL);
  return createRandomField(pt_cloud, labels, false);
}

// --------------------------------------------------------------
/*! See function definition */
// --------------------------------------------------------------
RFCreator3D::RFCreator3D(const std::vector<Descriptor3D*>& node_feature_descriptors,
                         const std::vector<std::vector<Descriptor3D*> >& clique_set_feature_descriptors,
                         const std::vector<std::vector<std::pair<bool,
                             point_cloud_clustering::PointCloudClustering*> > >& clique_set_clusterings)
{
  if (clique_set_feature_descriptors.size() != clique_set_clusterings.size())
  {
    ROS_FATAL("Inconsistent number of clique set featuers and clusterings");
    abort();
  }

  node_feature_descriptors_ = node_feature_descriptors;
  clique_set_feature_descriptors_ = clique_set_feature_descriptors;
  clique_set_clusterings_ = clique_set_clusterings;
  nbr_clique_sets_ = clique_set_feature_descriptors_.size();
}
