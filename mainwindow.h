/*#-------------------------------------------------
#
#   Segmentation to Depthmap to 3D with openCV
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1.5 - 2019/07/08
#
#-------------------------------------------------*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <iostream>

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include <opencv2/ximgproc.hpp>
#include "opencv2/opencv.hpp"
#include "opencv2/core/utility.hpp"
#include <QMainWindow>
#include <QFileDialog>
#include <QButtonGroup>
#include <QListWidgetItem>

#include "mat-image-tools.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void writePositionSettings();
    void readPositionSettings();

public slots:

private slots:

    //// quit
    void on_button_quit_clicked();

    //// init
    void AddCurveItem(const QString &title, const QColor &color, const QString &tip);

    //// labels
    void on_listWidget_labels_currentItemChanged(QListWidgetItem *currentItem); // show current label color when item change

    //// load & save
    void ChangeBaseDir(const QString& filename); // Set base dir and file
    void SaveDirBaseFile(); // just to keep the last open dir

    void DisableGUI();
    void on_button_save_depthmap_clicked(); // save session files
    void on_button_load_depthmap_clicked(); // load session files
    void on_button_load_segmentation_clicked();
    void on_button_load_rgbd_clicked();
    void on_button_save_ply_clicked();

    //// Display

    // Masks
    void on_checkBox_depthmap_clicked(); // toggle depthmap view
    void on_checkBox_image_clicked(); // toggle image view

    void on_horizontalSlider_blend_image_valueChanged(); // image transparency
    void on_horizontalSlider_blend_depthmap_valueChanged(); // depthmap transparency

    // Gradients
    void on_radioButton_flat_clicked();
    void on_radioButton_linear_clicked();
    void on_radioButton_double_linear_clicked();
    void on_radioButton_radial_clicked();
    void on_horizontalSlider_begin_valueChanged(int value);
    void on_horizontalSlider_end_valueChanged(int value);
    void on_listWidget_gradient_curve_currentItemChanged(QListWidgetItem *currentItem);

    // Viewport
    void on_pushButton_zoom_minus_clicked(); // levels of zoom
    void on_pushButton_zoom_plus_clicked();
    void on_pushButton_zoom_fit_clicked();
    void on_pushButton_zoom_100_clicked();

    void on_horizontalScrollBar_viewport_valueChanged(); // scroll the viewport
    void on_verticalScrollBar_viewport_valueChanged();

    // 3D view
    void on_checkBox_3d_realtime_clicked();
    void on_button_3d_update_clicked();

    void on_checkBox_3d_anaglyph_clicked();
    void on_checkBox_3d_axes_clicked();
    void on_checkBox_3d_blur_clicked();
    void on_checkBox_3d_light_clicked();
    void on_comboBox_3d_tint_currentIndexChanged(int index);
    void on_doubleSpinBox_gamma_valueChanged(double value);
    void on_horizontalSlider_depth3D_valueChanged(int value);
    void on_horizontalSlider_anaglyph_shift_valueChanged(int value);
    void on_horizontalSlider_blur_amount_valueChanged(int value);
    void on_button_3d_reset_clicked();
    void on_checkBox_3d_fullscreen_clicked();

    // 3D capture
    void on_spinBox_3d_frames_valueChanged(int value);
    void on_checkBox_3d_capture_clicked();
    void on_checkBox_3d_capture_unique_clicked();
    void on_checkBox_3d_infinite_cycle_clicked();
    void on_checkBox_3d_double_cycle_clicked();
    //void on_checkBox_3d_circular_clicked();
    void on_checkBox_3d_capture_fullscreen_clicked();

private:
    //// UI
    void InitializeValues();

    //// labels
    int GetCurrentLabelNumber(); // label value for use with label_mask
    void DeleteAllLabels(); // delete ALL labels and create one new if wanted

    //// Keyboard & mouse events
    void keyPressEvent(QKeyEvent *keyEvent) override; // for the create cell mode
    void keyReleaseEvent(QKeyEvent *keyEvent) override;

    void mouseReleaseEvent(QMouseEvent *eventRelease) override; // when the mouse button is released
    void mousePressEvent(QMouseEvent *eventPress) override; // mouse events = zoom, set cell color etc
    void mouseMoveEvent(QMouseEvent *eventPress) override;
    void wheelEvent(QWheelEvent *wheelEvent) override;

    void moveEvent(QMoveEvent*) override;
    void resizeEvent(QResizeEvent*) override;

    //// Display
    void Render(); // display image in viewport with depthmap and selection
    void ShowThumbnail(); // display thumbnail view
    /*void ShowAnaglyph();
    void ShowAnaglyphProject3D();*/

    void ZoomPlus(); // zoom in
    void ZoomMinus(); // zoom out
    void ShowZoomValue(); // display current zoom level

    void BlockGradientsSignals(const bool &active);
    void ChangeLabelGradient();
    void ShowGradient();
    void SetViewportXY(const int &x, const int &y); // change the origin of the viewport
    void UpdateViewportDimensions(); // calculate width and height of the viewport
    cv::Point Viewport2Image(const cv::Point &p); // calculate coordinates in the image from the viewport

    // the UI object, to access the UI elements created with Qt Designer
    Ui::MainWindow *ui;

    cv::Mat labels; // Segmentation cells and labels
    int nbLabels; // max number of labels

    cv::Mat image, // main image
            thumbnail, // thumbnail of main image
            depthmap, // gray-painted cells
            selection, // selection mask
            currentLabelMask; // for 3D partial update

    cv::Rect selection_rect; // rectangle of current selection

    cv::Mat disp_color; // Processed image display with mask and depthmap
    cv::Rect viewport; // part of the segmentation image to display

    std::string basefile, basedir, basedirinifile; // main image filename: directory and filename without extension

    Qt::MouseButton mouseButton; // save mouse button value when holding down a mouse button
    QPoint mouse_origin; // save the mouse position when holding down a mouse button
    cv::Point pos_save; // save the mouse position in image coordinates

    int num_zooms = 22; // number of zoom values
    double zooms[23] = {0, 0.05, 0.0625, 0.0833, 0.125, 0.1667, 0.25, 0.3333, 0.5, 0.6667, // reduced view
                        1, 1.25, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10000}; // zoomed view
    double zoom, oldZoom; // zoom factor for image display
    QString zoom_type; // if zoom changes indicates where it came from: button clic or mouse wheel

    bool loaded, computeVertices3D, updateVertices3D, computeColors3D; // indicators: image loaded & segmentation computed
    bool moveBegin, moveEnd;
    bool abort_3d;
    int saveXOpenGL, saveYOpenGL, saveWidthOpenGL, saveHeightOpenGL;

    struct struct_gradient{
        cv::Point beginPoint;
        cv::Point endPoint;
        int beginColor;
        int endColor;
        gradientType gradient;
        curveType curve;
    };

    struct_gradient gradients[1000];

};

#endif // MAINWINDOW_H
