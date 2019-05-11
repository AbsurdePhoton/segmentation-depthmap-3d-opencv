/*
 * OpenCV 3D dispersion tools library
 * Author: AbsurdePhoton
 *
 * v0 - 2018/09/23
 *
 * 3D projection RGB+Depth
 *
 */

#ifndef DISPERSION3D_H
#define DISPERSION3D_H

#include "opencv2/opencv.hpp"
#include <opencv2/ximgproc.hpp>
//#include <ImageMagick-7/Magick++.h>

using namespace cv;
//using namespace Magick;

Mat Displace(const Mat &image, const Mat &depthmap, const int &coefficient, const int &reference);
Mat AnaglyphRedCyan(Mat image, const Mat &depthmap, const bool &gray, const int &coefficient, const int &reference);
Mat Project3D(const Mat &image, const Mat &depthmap, const int &d0, const int &angleX, const int &angleY, const int &radius);
Mat AnaglyphProject3D(Mat image, const Mat &depthmap, const bool &gray, const int &d0, const int &angle, const int &radius);

#endif // DISPERSION3D_H

