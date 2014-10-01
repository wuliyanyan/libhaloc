#include <ros/ros.h>
#include <opencv2/opencv.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <boost/filesystem.hpp>

namespace fs=boost::filesystem;

using namespace std;
using namespace cv;

class EvaluationNode
{

  public:

    // Parameters
    string training_dir_;

    // Class constructor
    EvaluationNode() : nhp_("~") {}

    // Read node parameters
    void readParameters() {
      // Directories
      nhp_.param("training_dir", training_dir_, string(""));

      // Feature detector parameters
      nhp_.param<int>("sift_nfeatures", sift_nfeatures_, 0);
      nhp_.param<int>("sift_num_octave_layers", sift_num_octave_layers_, 3);
      nhp_.param<double>("sift_threshold", sift_threshold_, 0.04);
      nhp_.param<double>("sift_edge_threshold", sift_edge_threshold_, 10);
      nhp_.param<double>("sift_sigma", sift_sigma_, 1.6);

      // Trainer parameters
      nhp_.param<int>("max_images", max_images_, 50);
      nhp_.param<double>("cluster_size", cluster_size_, 0.6);
      nhp_.param<double>("lower_information_bound", lower_information_bound_, 0);
      nhp_.param<int>("min_descriptor_count", min_descriptor_count_, 50);
    }

    // Initialize the node
    void init() {
      // Feature tools
      detector_ = new SIFT(sift_nfeatures_,
                           sift_num_octave_layers_,
                           sift_threshold_,
                           sift_edge_threshold_,
                           sift_sigma_);
      extractor_ = new SIFT();
      matcher_ = new FlannBasedMatcher();
      bide_ = new BOWImgDescriptorExtractor(extractor_, matcher_);

      // BoW trainer
      trainer_ = of2::BOWMSCTrainer(cluster_size_);
      train_count_ = 0;
    }

    // Openfabmap learning stage
    void of2Learn() {
      // Sort directory of images
      typedef std::vector<fs::path> vec;
      vec v;
      copy(
        fs::directory_iterator(training_dir_),
        fs::directory_iterator(),
        back_inserter(v)
      );
      sort(v.begin(), v.end());
      vec::const_iterator it(v.begin());

      // Iterate over all images
      while (it!=v.end())
      {
        if (fs::is_directory(*it)) {
          // Next directory entry and continue
          it++;
          continue;
        }

        // Read image
        string filename = it->filename().string();
        string path = training_dir_ + "/" + filename;
        Mat img = imread(path, CV_LOAD_IMAGE_COLOR);

        ROS_INFO("[Haloc:] Detect");
        detector_->detect(img, kpts_);
        ROS_INFO("[Haloc:] Extract");
        extractor_->compute(img, kpts_, descriptors_);

        // Check if frame was useful
        if (!descriptors_.empty() && kpts_.size() > min_descriptor_count_)
        {
          trainer_.add(descriptors_);
          train_count_++;
          ROS_INFO_STREAM("[Haloc:] Added to trainer" << " (" << train_count_ << " / " << max_images_ << ")");

          // Add the frame to the sample pile
          frames_sampled_.push_back(path);
        }
        else
        {
          ROS_WARN("[Haloc:] Image not descriptive enough, ignoring.");
        }

        if ((!(train_count_ < max_images_) && max_images_ > 0))
        {

          // TODO: SHUTDOWN HERE!!!!
          break;
        }

        // Next directory entry
        it++;
      }
    }

  private:

    // Parameters
    ros::NodeHandle nhp_;
    int sift_nfeatures_;
    int sift_num_octave_layers_;
    int max_images_;
    int min_descriptor_count_;
    int train_count_;
    double sift_threshold_;
    double sift_edge_threshold_;
    double sift_sigma_;
    double cluster_size_;
    double lower_information_bound_;

    // OpenFABMap2
    of2::FabMap *fabMap_;
    of2::BOWMSCTrainer trainer_;
    Ptr<FeatureDetector> detector_;
    Ptr<DescriptorExtractor>  extractor_;
    Ptr<DescriptorMatcher> matcher_;
    Ptr<BOWImgDescriptorExtractor> bide_;
    vector<KeyPoint> kpts_;
    Mat descriptors_;
    vector<string> frames_sampled_;
};

int main(int argc, char** argv)
{
  // Init ros node
  ros::init(argc, argv, "evaluation");
  EvaluationNode node;

  // Read the parameters
  node.readParameters();
  node.init();

  // BOW training
  if (!node.training_dir_.empty()) {

    // Sanity check
    if (!fs::exists(node.training_dir_) || !fs::is_directory(node.training_dir_))
    {
      ROS_ERROR_STREAM("[HashMatching:] The image directory does not exists: " <<
      node.training_dir_);
      return 0;
    }

    // Training
    node.of2Learn();
  }

  ros::spin();
  return 0;
}