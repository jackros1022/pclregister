//include own headers first
#include "registrator.hpp"
#include "filters.hpp"
#include "features.hpp"
#include "loader.hpp"

#include <pcl/io/boost.h>
#include <boost/make_shared.hpp>
#include <pcl/common/transforms.h>

/*for additional parsing options*/
#include <pcl/console/parse.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <boost/program_options.hpp>

int main(int argc, char **argv){

    //error code
    int err = 0;
    
    if(argc < 3){
        PCL_ERROR ("Syntax is: %s <source> <target> [*] (without file ending .pcd!)", argv[0]);
        PCL_ERROR ("[*] - multiple files can be added. The registration results of (i, i+1) will be registered against (i+2), etc");
        return (-1);
    }


    //initialize class object
    boost::shared_ptr<Registrator> registrator = boost::make_shared<Registrator>();
    boost::shared_ptr<Filters> filter = boost::make_shared<Filters>();
    boost::shared_ptr<Loader> loader = boost::make_shared<Loader>();
    boost::shared_ptr<Saver> saver = boost::make_shared<Saver>();
    boost::shared_ptr<Features> feature = boost::make_shared<Features>();
    
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr src_points = loader->loadPoints (argv[1]);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr tgt_points = loader->loadPoints (argv[2]);

    Eigen::Matrix4f transform = Eigen::Matrix4f::Identity ();

    boost::shared_ptr<Features::ObjectFeatures>
    srcFeatures = boost::make_shared<Features::ObjectFeatures>();
    
    boost::shared_ptr<Features::ObjectFeatures>
    tgtFeatures = boost::make_shared<Features::ObjectFeatures>();
    

    srcFeatures = feature->computeFeatures(src_points);
    tgtFeatures = feature->computeFeatures(tgt_points);
    
    
    saver->savePoints(argv[1], srcFeatures->points);
    saver->saveKeypoints(argv[1], srcFeatures->keypoints);
    saver->saveSurfaceNormals(argv[1], srcFeatures->normals);
    saver->saveLocalDescriptors(argv[1], srcFeatures->local_descriptors);
    
    saver->savePoints(argv[2], tgtFeatures->points);
    saver->saveKeypoints(argv[2], tgtFeatures->keypoints);
    saver->saveSurfaceNormals(argv[2], tgtFeatures->normals);
    saver->saveLocalDescriptors(argv[2], tgtFeatures->local_descriptors);


    //initial alignment parameters
    double min_sample_dist = 1e-6;
    double max_correspondence_dist = 0.03f;
    double nr_iters = 10000;

    // load the keypoints and local descriptors
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr srcKeypoints = loader->loadKeypoints(argv[1]);
    pcl::PointCloud<pcl::FPFHSignature33>::Ptr srcDescriptor = loader->loadLocalDescriptors(argv[1]);

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr tgtKeypoints = loader->loadKeypoints(argv[2]);
    pcl::PointCloud<pcl::FPFHSignature33>::Ptr tgtDescriptor = loader->loadLocalDescriptors(argv[2]);
    

    // find the transform that roughly aligns the points
    transform = registrator->computeInitialAlignment(srcKeypoints,
                                                     srcDescriptor,
                                                     tgtKeypoints,
                                                     tgtDescriptor,
                                                     min_sample_dist,
                                                     max_correspondence_dist,
                                                     nr_iters);

    pcl::console::print_info ("computed initial alignment!\n");

    //refined alignment parameters
    float max_correspondence_distance = 0.20f;
    float outlier_rejection_threshold = 0.40f;
    float transformation_epsilon = 1e-8;
    int max_iterations = 300;


    //filter NAN out of clouds
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr
    fsrcCloud = boost::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr
    ftgtCloud = boost::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();


    fsrcCloud = filter->removeNaNPoints(src_points, "source cloud");
    ftgtCloud = filter->removeNaNPoints(tgt_points, "target cloud");


    transform = registrator->refineAlignment (fsrcCloud,
                                              ftgtCloud,
                                              transform,
                                              max_correspondence_distance,
                                              outlier_rejection_threshold,
                                              transformation_epsilon,
                                              max_iterations);

    pcl::console::print_info ("refined alignment!\n");

    // transform the source point to align them with the target points
    pcl::transformPointCloud (*src_points, *src_points, transform);

    // save output
    std::string filename("output.pcd");

    // merge the two clouds
    (*src_points) += (*tgt_points);

    // save the result
    pcl::io::savePCDFile (filename, *src_points);
    pcl::console::print_info ("saved registered clouds as %s\n", filename.c_str ());

    return (0);
}
