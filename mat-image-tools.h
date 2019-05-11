/*#-------------------------------------------------
 *
 * OpenCV image tools library
 * Author: AbsurdePhoton
 *
 * v1.8 - 2018/10/21
 *
 * Convert mat images to QPixmap or QImage and vice-versa
 * Brightness, Contrast, Gamma, Equalize, Color Balance
 * Erosion / dilation of pixels
 * Copy part of image
 * Resize image keeping aspect ration
 * Contours using Canny algorithm with auto min and max threshold
 * Noise reduction quality
 * Gray gradients
 * Red-cyan anaglyph tints
 *
#-------------------------------------------------*/

#ifndef MAT2IMAGE_H
#define MAT2IMAGE_H

#include "opencv2/opencv.hpp"
#include <opencv2/ximgproc.hpp>

using namespace cv;

const double Pi = atan(1)*4;
enum shift_direction{shift_up=1, shift_right, shift_down, shift_left}; // directions for shift function
enum gradientType {gradient_flat, gradient_linear, gradient_doubleLinear, gradient_radial}; // gradient types
enum curveType {curve_linear, curve_cosinus2, curve_sigmoid, curve_cosinus, curve_cos2sqrt,
                curve_power2, curve_cos2power2, curve_power3, curve_undulate, curve_undulate2, curve_undulate3}; // gray curve types
enum anaglyphTint {tint_color, tint_gray, tint_true, tint_half, tint_optimized, tint_dubois}; // red/cyan anaglyph tints

bool IsRGBColorDark(int red, int green, int blue); // is the RGB value given dark or not ?

Mat QImage2Mat(const QImage &source); // convert QImage to Mat
Mat QPixmap2Mat(const QPixmap &source); // convert QPixmap to Mat

QImage Mat2QImage(const Mat &source); // convert Mat to QImage
QPixmap Mat2QPixmap(const Mat &source); // convert Mat to QPixmap
QPixmap Mat2QPixmapResized(const Mat &source, const int &width, const int &height, const bool &smooth); // convert Mat to resized QPixmap
QImage cvMatToQImage(const Mat &source); // another implementation Mat type wise

Mat BrightnessContrast(const Mat &source, const double &alpha, const int &beta); // brightness and contrast
Mat GammaCorrection(const Mat &source, const double gamma); // gamma correction
Mat EqualizeHistogram(const Mat &source); // histogram equalization
Mat SimplestColorBalance(const Mat &source, const float &percent); // color balance with histograms

Mat DilatePixels(const Mat &source, const int &dilation_size); // dilate pixels
Mat ErodePixels(const Mat &source, const int &erosion_size); // erode pixels

Mat ShiftFrame(const Mat &source, const int &nb_pixels, const shift_direction &direction); // shift frame in one direction

Mat CopyFromImage (Mat source, const Rect &frame); // copy part of an image
Mat ResizeImageAspectRatio(const Mat &source, const Size &frame); // Resize image keeping aspect ratio

Mat DrawColoredContours(const Mat &source, const double &sigma, const int &apertureSize, const int &thickness); // draw colored contours of an image

double PSNR(const Mat &source1, const Mat &source2); // noise difference between 2 images

void GradientFillGray(const int &gradient_type, Mat &img, const Mat &msk,
                      const Point &beginPoint, const Point &endPoint,
                      const int &beginColor, const int &endColor,
                      const int &curve, Rect area = Rect(0, 0, 0, 0)); // fill a 1-channel image with the mask converted to gray gradients

Mat AnaglyphTint(const Mat & source, const int &tint); // change tint of image to avoid disturbing colors in red-cyan anaglyph mode

#endif // MAT2IMAGE_H
