/*#-------------------------------------------------
#
#                 openGL Widget
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#               v1.5 - 2019/07/08
#
# * .ply mesh export
#
# * Render using openGL VBO (i.e. in GPU memory)
#
# * Options :
#     - Lights
#     - Axes drawing
#     - Anaglyph red/cyan with adjustable shift
#     - Mouse control :
#         . zoom with wheel
#         . move view with left mouse button
#         . rotate view with right mouse button
#     - Keyboard controls :
#         . move view with arrows
#         . rotate with pg_up, pg_down, home, end
#
# * QT signals sent when zoomed, moved or rotated
#
# * Public access to zoom, position and rotation
#
#-------------------------------------------------*/

#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include "opencv2/opencv.hpp"

class openGLWidget : public QOpenGLWidget
{
    Q_OBJECT


public:
    explicit openGLWidget(QWidget *parent = 0);
    ~openGLWidget();

    void Capture(); // take a snapshot of rendered 3D scene

    bool computeVertices3D, // recompute all vertices and create a new buffer
         computeIndexes3D, // recompute all indexes and create a new buffer
         computeColors3D, // recompute all colors and create a new buffer
         updateVertices3D, // recompute only vertices using the mask "mask3D", directly in GPU's RAM
         updateAllVertices3D; // when only updating vertices, indicate that the whole image is concerned

    QOpenGLBuffer vertexbuffer; // VBO for vertices
    QOpenGLBuffer indexbuffer; // VBO for indexes
    QOpenGLBuffer colorbuffer; // VBO for vertex colors

    QVector<QVector3D> vertexarray; // vertex coordinates
    QVector<GLuint> indexarray; // vertex indexes
    QVector<QVector3D> colorarray; // vertex colors

    double xRot, yRot, zRot; // rotation values
    int xShift, yShift; // position values
    double angleLight; // x position

    cv::Mat image3D; // reference image
    cv::Mat depthmap3D; // depthmap image
    cv::Mat mask3D; // mask for partial update
    cv::Rect area3D; // used for partial update

    double zoom3D; // zoom coefficient
    double depth3D; // used for depthmap rendering

    bool anaglyphEnabled; // anaglypgh red / cyan rendering (angle in degrees on y axis)
    double anaglyphShift; // shift of cyan vs red image
    bool axesEnabled; // draw 3D origin axes
    bool lightEnabled; // light
    bool qualityEnabled; // antialiasing

    QImage capture3D; // image of captured 3D scene


protected:

    void initializeGL(); // launched when the widget is initialized
    void paintGL(); // 3D rendering
    void resizeGL(int width, int height); // called when the widget is resized
    void mousePressEvent(QMouseEvent *event); // save initial mouse position for move and rotate
    void mouseMoveEvent(QMouseEvent *event); // move and rotate view with mouse buttons
    void wheelEvent(QWheelEvent *event); // zoom
    void keyPressEvent(QKeyEvent *keyEvent);
    void ComputeVertices(); // create vertices
    void ComputeIndexes(); // create indexes
    void UpdateVertices(); // update vertices z
    void ComputeColors(); // recompute colors
    int NumberOfVertices(const int &rows, const int &cols); // number of expected vertices for an image
    int NumberOfIndexes(const int &rows, const int &cols); // number of expected indexes for an image
    GLuint VertexIndex(const int &y, const int &x); // pixel's index in a image

public slots:

    void SetXRotation(int angle); // rotate view
    void SetYRotation(int angle);
    void SetZRotation(int angle);
    void SetAngleUp();
    void SetAngleDown();
    void SetAngleLeft();
    void SetAngleRight();

    void SetXShift(int value); // move view
    void SetYShift(int value);
    void SetShiftUp();
    void SetShiftDown();
    void SetShiftLeft();
    void SetShiftRight();

    void SaveToObj(const QString &filename);
    void SaveToPly(const QString &filename);


signals:

    void xRotationChanged(int angle); // rotation signals
    void yRotationChanged(int angle);
    void zRotationChanged(int angle);

    void xShiftChanged(int dec); // move signals
    void yShiftChanged(int dec);

    void zoomChanged(double zoom); // zoom signal

    void verticesChanged(int nb_Vertices); // number of vertices signal


private:

    QPoint lastPos; // save mouse position

};

#endif // OPENGLWIDGET_H

