#include "opencv2/opencv.hpp"
#include <opencv2/ximgproc.hpp>
//#include <ImageMagick-7/Magick++.h>

using namespace cv;
using namespace cv::ximgproc;
using namespace std;
//using namespace Magick;

Mat Displace(const Mat &image, const Mat &depthmap, const int &coefficient, const int &reference)
{
    Mat result;
    image.copyTo(result);
    //result = 0;

    int displacement;

    for (int i = 0; i < image.cols; i++) {
        for (int j = 0; j < image.rows; j++) {
            displacement = round(((double(depthmap.at<Vec3b>(j,i)[0]) - (255 - reference)) / (255 - reference)) * (-coefficient));

            if ((i + displacement < image.cols) & (i + displacement >= 0)) {
                result.at<Vec3b>(j,i + displacement) = image.at<Vec3b>(j,i);
            }
        }
    }

    return result;
}

Mat AnaglyphRedCyan(Mat image, const Mat &depthmap, const bool &gray, const int &coefficient, const int &reference)
{
    if (gray) {
        Mat temp;
        cv::cvtColor(image, temp, CV_BGR2GRAY);
        cv::cvtColor(temp, image, CV_GRAY2BGR);
    }

    Mat result = Displace(image, depthmap, coefficient, reference);

    int from_to[] = { 2,2 }; // channel 2 = red, copy channel 2 from source to channel 2 in dest
    mixChannels( &image, 1, &result, 1, from_to, 1 );

    return result;
}

Mat Project3D(const Mat &image, const Mat &depthmap, const int &d0, const int &angleX, const int &angleY, const int &radius)
{
    // angle is in degrees from 0 to 360 - PI = 180°
    // d0 = reference depth (zero parallax)

    Mat result = Mat::zeros(image.rows, image.cols, CV_8UC4);

    double h = 400.0; // camera offset
    double D = 1000; // depth of image plane
    double L = 10000; // focal length of camera

    int W = image.cols;
    int H = image.rows;
        int x0 = round(W / 2);
        int y0 = round(H / 2);

        //loop over each frame
        //for ( var frame_id = 0 ; frame_id < nb_frames ; frame_id++ ) {
            double thetaX = 3.14159 * 2.0 * angleX / 360; // angle of rotation of camera
            double thetaY = 3.14159 * 2.0 * angleY / 360; // angle of rotation of camera

            double camera_x = cos(thetaX) * h; // camera position
            double camera_y = sin(thetaY) * h;

            Mat current_depth(image.rows, image.cols, CV_8UC1);
            current_depth = -d0 - 1;

            for ( int i= 0 ; i< H ; i++ ) {
                for ( int j= 0 ; j< W ; j++ ) {
                    //var d = depthp[4*(i*W+j)+0] - d0;
                    int d = depthmap.at<Vec3b>(i,j)[0] - d0; // depth of current pixel

                    //get x coordinate of corresponding pixel in target image
                    double tangent_alpha = camera_x / D;
                    double alpha = atan(tangent_alpha);
                    double tangent_beta = (tangent_alpha * L + camera_x + x0 - j) / (D - d + L);
                    double beta = atan(tangent_beta);
                    double gamma = beta - alpha;
                    double x_ref_dbl = L * tan(gamma) / cos(alpha);
                    x_ref_dbl = x0 - x_ref_dbl;
                    //get y coordinate of corresponding pixel in target image
                    tangent_alpha = camera_y / D;
                    alpha = atan(tangent_alpha);
                    tangent_beta = (tangent_alpha * L + camera_y + y0 - i) / (D - d + L);
                    beta = atan(tangent_beta);
                    gamma = beta - alpha;
                    double y_ref_dbl = L * tan(gamma) / cos(alpha);
                    y_ref_dbl = y0 - y_ref_dbl;

                    int x_ref = round(x_ref_dbl);
                    int y_ref = round(y_ref_dbl);

                    //var radius = 0;

                    for ( int x_offset= -radius ; x_offset<= +radius ; x_offset++ ) {
                        for ( int y_offset= -radius ; y_offset<= +radius ; y_offset++ ) {
                            int x = x_ref+x_offset;
                            int y = y_ref+y_offset;
                            //we are only interested in the four pixels at the corners of the square the pixel is in
                            //dist = Math.max(Math.abs(x-x_ref_dbl),Math.abs(y-y_ref_dbl));
                            //if ( dist > radius ) continue;
                            if ( x >= 0 && x < W && y >= 0 && y < H ) {
                                //check if frame pixel has already gotten a color
                                //only update if new depth is closer than current depth
                                if ( d > current_depth.at<char>(y,x) ) {
                                    //update color
                                    //for ( var rgb_ind = 0 ; rgb_ind < 3 ; rgb_ind++ ) {
                                    //    destp[4*(y*W+x)+rgb_ind] = imgp[4*(i*W+j)+rgb_ind];
                                    //}
                                    Vec3b color = image.at<Vec3b>(i,j);
                                    result.at<Vec4b>(y,x) = Vec4b(color[0], color[1], color[2], 255);
                                    //destp[4*(y*W+x)+3] = 255;
                                    //update the current depth for the frame pixel
                                    //current_depth[y*W+x] = d;
                                    current_depth.at<char>(y,x) = d;
                                }
                            }
                        }
                    }
                }
            }
            /*
            //check consistency
            for ( var i= 0 ; i< H ; i++ ) {
                for ( var j= 0 ; j< W ; j++ ) {
                    if ( destp[4*(i*W+j)+3] == 255 ) {
                        if ( current_depth[i*W+j] == -d0-1 ) {
                            //should not happen unless something is really wrong
                            console.log("i = ",i,"j = ", j,"current_depth = ",current_depth[i*W+j]);
                        }
                    }
                }
            }
            //do some inpainting for the occluded pixels that are now visible
            if ( inpainting_method == "method 1" ) {
                //that's what is in https://github.com/fulmicoton/wigglejs
                fill_with_average(destp, W, H);
            }
            else if ( inpainting_method == "method 2" ) {
                //that's my version
                fill_occlusions(destp,current_depth,W,H,d0);
            }
            else if ( inpainting_method == "none" ) {
                for ( var i= 0 ; i< H ; i++ ) {
                    for ( var j= 0 ; j< W ; j++ ) {
                        //see if occluded (still transparent)
                        if ( destp[4*(i*W+j)+3] == 255 ) {
                            //pixel is not transparent anymore (has been filled)
                            continue;
                        }
                        //make pixel not transparent (should be black)
                        destp[4*(i*W+j)+3] = 255;
                    }
                }
            }*/

    Mat convert;
    cv::cvtColor(result, convert, CV_BGRA2BGR);

    return convert;
}

Mat AnaglyphProject3D(Mat image, const Mat &depthmap, const bool &gray, const int &d0, const int &angle, const int &radius)
{
    if (gray) {
        Mat temp;
        cv::cvtColor(image, temp, CV_BGR2GRAY);
        cv::cvtColor(temp, image, CV_GRAY2BGR);
    }

    Mat gauche = Project3D(image, depthmap, d0, angle, 0, radius);
    Mat droite = Project3D(image, depthmap, d0, 180-angle, 0, radius);

    int from_to[] = { 2,2 }; // channel 2 = red, copy channel 2 from source to channel 2 in dest
    mixChannels( &gauche, 1, &droite, 1, from_to, 1 );

    return droite;
}
