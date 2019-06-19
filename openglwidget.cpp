/*#-------------------------------------------------
#
#                 openGL Widget
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#               v1.4 - 2019/05/10
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

#include <QtWidgets>

#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QtOpenGL>

#include "opencv2/opencv.hpp"
#include "openglwidget.h"
#include "mat-image-tools.h"

using namespace cv;

///////////////////////////////////////////////
//// Widget
///////////////////////////////////////////////

openGLWidget::openGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      vertexbuffer(QOpenGLBuffer::VertexBuffer),
      indexbuffer(QOpenGLBuffer::IndexBuffer),
      colorbuffer(QOpenGLBuffer::VertexBuffer)
{

}

openGLWidget::~openGLWidget()
{

}

///////////////////////////////////////////////
//// Redefine widget main functions :
////    init, repaint, resize
///////////////////////////////////////////////

void openGLWidget::initializeGL() // launched when the widget is initialized
{
    glClear(Qt::black); // clear the screen with black color

    glEnable(GL_DEPTH_TEST); // z-sorting
    //glEnable(GL_DEPTH_CLAMP); // no clipping - it's a bit slower
    glEnable(GL_COLOR_MATERIAL); // to see the two faces of a triangle, needed by lights when not shading
    glShadeModel(GL_SMOOTH); // blend colors - GL_FLAT chooses one color in the polygon

    glDisable(GL_CULL_FACE); // facet culling
    glEnable(GL_BLEND); // prefer using GLBlendFunc, used by the anaglyphic view

    xRot = 0; // initial values of rotation
    yRot = 0;
    zRot = 0;
    xShift = 0; // initial (x,y) position
    yShift = 0;
    angleLight = 5000;
    computeVertices3D = true; // compute vertices
    computeIndexes3D = true; // compute indexes
    computeColors3D = true; // compute colors
    updateVertices3D = false; // not a partial update
    updateAllVertices3D = false; // used by updateVertices3D
    zoom3D = 8; // zoom coefficient
    axesEnabled = true; // draw 3D origin axes enabled
    anaglyphEnabled = false; // anaglypgh red / cyan rendering disabled
    anaglyphShift = -1.5; // shift of cyan vs red image (angle in degrees on y axis)
    lightEnabled = false; // light disabled
    qualityEnabled = true; // antialiasing enabled

    //// Lights
    GLfloat light_position[] = { 5000, 0, 5000, 1.0 };
    GLfloat light_ambient[]  = { 0.1, 0.1, 0.1, 1.0};
    GLfloat light_diffuse[]  = { 3, 3, 3, 0.5};
    glEnable(GL_LIGHTING); // enable lighting
    glEnable(GL_LIGHT0); // define the first light
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient); // ambient
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse); // diffuse
}

int openGLWidget::NumberOfIndexes(const int &rows, const int &cols) // number of expected indexes for an image
{
    if (rows <= 0) // special case
            return 0;
        else // a bit complicated, see ComputeIndexes()
            return double(double(cols) * double(rows)) + double((double(cols) * 2 - 1) * (double(rows) / 2 - 1));
}

int openGLWidget::NumberOfVertices(const int &rows, const int &cols) // number of expected vertices for an image
{
    return rows * cols;
}

GLuint openGLWidget::VertexIndex(const int &y, const int &x) // pixel's index in a image
{
    return y * image3D.cols + x; // just like an array
}

void openGLWidget::ComputeVertices()  // (re)create vertices array and buffer
{
    vertexarray.clear(); // destroy buffers and arrays
    vertexbuffer.destroy();
    vertexarray.reserve(NumberOfVertices(depthmap3D.rows, depthmap3D.cols)); // reserve memory space in advance

    int halfX = -depthmap3D.cols / 2; // to center the axes in middle of image
    int halfY = depthmap3D.rows / 2;

    for (float row = 0; row < depthmap3D.rows; row++) { // for each row of the image
        for (float col = 0; col < depthmap3D.cols; col++) { // for each pixel in the row from left to right
            vertexarray.push_back(QVector3D(col+halfX, -row+halfY,
                                            (depthmap3D.at<uchar>(row, col) - 127) * depth3D)); // vertex in buffer
        }
    }

    vertexbuffer.create(); // create VBO vertices buffer
    vertexbuffer.bind(); // bind it
    vertexbuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw); // vertex buffer will be often modified
    vertexbuffer.allocate(vertexarray.constData(), vertexarray.size()*sizeof(QVector3D)); // allocate and populate in GPU RAM
    vertexbuffer.release(); // done

    computeVertices3D = false; // done recomputing
}

void openGLWidget::UpdateVertices() // update vertices z
{
    Rect area_save = area3D; // save values for later
    if (updateAllVertices3D) { // ... precisely in case of this
        area3D = Rect(0, 0, image3D.cols, image3D.rows); // area is the whole image
    }

    vertexbuffer.bind(); // use current VBO
    GLfloat* posBuffer = (GLfloat*) (vertexbuffer.map(QOpenGLBuffer::WriteOnly)); // map a pointer on it

    if (posBuffer != (GLfloat*) NULL) { // pointer not valid ?

        int index; // index of current vertex

        for (int row = area3D.y; row < area3D.y + area3D.height; row++) { // for each row of area
            for (int col = area3D.x; col < area3D.x + area3D.width; col++) { // for each pixel in the row from left to right
                if ((updateAllVertices3D) | (mask3D.at<uchar>(row, col) != 0)) { // current pixel set in mask ?
                    index = VertexIndex(row, col); // use index of this pixel
                    posBuffer[3 * index + 2] = (depthmap3D.at<uchar>(row, col) - 127) * depth3D; // rewrite directly into VBO
                }
            }
        }

        vertexbuffer.unmap(); // update done
    }

    vertexbuffer.release(); // release VBO

    updateVertices3D = false; // done recomputing

    if (updateAllVertices3D) { // restore old values
        area3D = area_save;
        updateAllVertices3D = false;
    }
}

void openGLWidget::ComputeIndexes() // (re)create index array and buffer
{
    indexarray.clear(); // destroy buffers and arrays
    indexbuffer.destroy();
    int arraySize = NumberOfIndexes(image3D.rows, image3D.cols); // how many indexes ?
    //indexarray.reserve(arraySize); // reserve memory space in advance

    for (int row = 0; row < depthmap3D.rows -1; row++) { // for each row of the images
        if (int(row)%2 == 0) { // row number is even
            for (int col = 0; col < depthmap3D.cols; col++) { // for each pixel in the row from left to right
                indexarray.push_back(VertexIndex(row, col)); // index in buffer
                indexarray.push_back(VertexIndex(row+1, col));
            }
        }
        else { // row number is odd
            for (int col = depthmap3D.cols - 1; col > 0 ; col--) { // for each pixel in the row from right to left except the first one
                indexarray.push_back(VertexIndex(row+1, col)); // the first time it creates a "degenerate triangle"
                indexarray.push_back(VertexIndex(row, col-1));
            }
            int col = 1;
            indexarray.push_back(VertexIndex(row+1, col-1)); // add one more vertex to finish the column
        }
    }

    indexbuffer.create(); // create VBO vertices buffer
    indexbuffer.bind(); // bind it
    indexbuffer.setUsagePattern(QOpenGLBuffer::StaticDraw); // vertex buffer will be often modified
    indexbuffer.allocate(indexarray.constData(), indexarray.size()*sizeof(GLuint)); // allocate and populate in GPU RAM
    indexbuffer.release(); // done

    computeIndexes3D = false; // done recomputing

    emit verticesChanged(arraySize); // emit signal for number of vertices
}

void openGLWidget::ComputeColors() // (re)create colors array and buffer
{
    colorarray.clear(); // destroy buffers and arrays
    colorbuffer.destroy();
    colorarray.reserve(NumberOfVertices(image3D.rows, image3D.cols)); // reserve memory space in advance
    Vec3b CO; // pixel color of reference image

    for (float row = 0; row < image3D.rows; row++) { // for each row of the images
        for (float col = 0; col < image3D.cols; col++) { // for each pixel in the row from left to right
            CO = Vec3b(image3D.at<Vec3b>(row, col)); // // pixel color of reference image
            colorarray.push_back(QVector3D(CO[2] / 255.0, CO[1] / 255.0, CO[0] / 255.0)); // color in buffer, RGB values between 0 and 1
        }
    }

    computeColors3D = false; // done recomputing

    colorbuffer.create(); // create colors buffer
    colorbuffer.bind(); // bind it
    colorbuffer.allocate(colorarray.constData(), colorarray.size()*sizeof(QVector3D)); // copy data to VBO
    colorbuffer.release(); // release VBO
}

void openGLWidget::SaveToObj(const QString &filename) // Save current 3D arrays to WaveFront .obj file
{
    //open ascii text file for writing
    QFile file(filename);
    if (file.open(QIODevice::ReadWrite))
    {
        QTextStream stream(&file);
        //stream << "example " << 49 << "\n";
        //file.close();

        // save vertices
        QVector3D vertex, color;
        int max = NumberOfVertices(image3D.rows, image3D.cols);

        for (int index = 0; index < max; index++) {
            vertex = vertexarray[index];
            color = colorarray[index];
            stream << "v " << vertex.x() << " " << vertex.y() << " " << vertex.z()
                   << " " << color[0] << " " << color[1] << " " << color[2]
                   << "\n";
        }

        // save indexes
        max = NumberOfIndexes(image3D.rows, image3D.cols) - 2;

        for (int index = 0; index < max; index++) {
            stream << "f " << indexarray[index]+1 << " " << indexarray[index+1]+1 << " " << indexarray[index+2]+1 << "\n";
        }

        file.close();
    }
}

void openGLWidget::SaveToPly(const QString &filename) // Save current 3D arrays to Polygon File Format .ply file
{
    //open ascii text file for writing
    QFile file(filename);
    if (file.open(QIODevice::ReadWrite))
    {
        QTextStream stream(&file);
        //stream << "example " << 49 << "\n";
        //file.close();

        //// header
        stream << "ply" << "\n";
        stream << "format ascii 1.0" << "\n";
        stream << "comment Produced by a tool from AbsurdePhoton" << "\n";
        stream << "comment GitHub: https://github.com/AbsurdePhoton" << "\n";
        stream << "comment My photography site: absurdephoton.fr" << "\n";

        // quantities
          // vertices
        int maxVertices = NumberOfVertices(image3D.rows, image3D.cols);
        stream << "element vertex " << maxVertices << "\n";
        stream << "property float x" << "\n"; // define vertex x y and z
        stream << "property float y" << "\n";
        stream << "property float z" << "\n";
        stream << "property uchar red" << "\n"; // and vertex colors
        stream << "property uchar green" << "\n";
        stream << "property uchar blue" << "\n";

          // triangles
        int maxTriangles = NumberOfIndexes(image3D.rows, image3D.cols) - 2;
        stream << "element face " << maxTriangles << "\n";
        stream << "property list uchar int vertex_index" << "\n";

          // end of header
        stream << "end_header" << "\n";

        //// save vertices
        QVector3D vertex, color;

        int index; // index of current vertex
        std::string st; // used to get a comma separator for floats, streams use locales !

        for (int row = 0; row < image3D.rows; row++) { // for each row of area
            for (int col = 0; col < image3D.cols; col++) { // for each pixel in the row from left to right
                    index = VertexIndex(row, col); // use index of this pixel
                    color = colorarray[index];
                    /*stream << col << " " << -row << " " << qSetRealNumberPrecision(5) << (depthmap3D.at<uchar>(row, col) - 127) * depth3D
                           << " " << int(round(color[0]*255)) << " " << int(round(color[1]*255)) << " " << int(round(color[2]*255))
                           << "\n";*/
                    st = std::to_string(col) + " " + std::to_string(-row) + " " + std::to_string(float((depthmap3D.at<uchar>(row, col) - 127) * depth3D))
                            + " " + std::to_string(int(round(color[0]*255))) + " " + std::to_string(int(round(color[1]*255))) + " " + std::to_string(int(round(color[2]*255)))
                            + "\n";
                    stream << QString::fromStdString(st);
            }
        }

        // save faces + indexes
        int maxIndexes = NumberOfIndexes(image3D.rows, image3D.cols) - 2;

        for (int index = 0; index < maxIndexes; index++) {
            stream << "3 " << indexarray[index] << " " << indexarray[index+1] << " " << indexarray[index+2] << "\n";
        }

        // Close the file
        stream << "\n";
        file.close();
    }
}

void openGLWidget::paintGL() // 3D rendering
{
    if (lightEnabled) {
        glEnable(GL_LIGHTING); // turn on the lights...
    } else
        glDisable(GL_LIGHTING); // ... or not


    if (qualityEnabled) { // antialiasing
        glShadeModel(GL_SMOOTH); // blend colors
        glEnable(GL_POINT_SMOOTH); // draw points with anti-aliasing
        glEnable(GL_LINE_SMOOTH); // draw lines with anti-aliasing
        //glEnable(GL_POLYGON_SMOOTH); // draw polygons with anti-aliasing // can result in noise
        glEnable(GL_DITHER); //dither color components
        glEnable(GL_MULTISAMPLE); // use multiple fragment samples in computing the final color of a pixel
    }
    else { // no antialiasing
        glShadeModel(GL_FLAT); // don't blend colors
        glDisable(GL_POINT_SMOOTH); // draw aliased points
        glDisable(GL_LINE_SMOOTH); // draw aliased lines
        //glDisable(GL_POLYGON_SMOOTH); // draw aliased polygons
        glDisable(GL_DITHER); //dither color components
        glDisable(GL_MULTISAMPLE); // use multiple fragment samples in computing the final color of a pixel
    }

    //// init 3D view

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear color and depth buffers

    if (anaglyphEnabled) glRotatef(-anaglyphShift, 0.0, 1.0, 0.0); // rotate a bit the first image on y axis

    glLoadIdentity(); // replace current matrix with identity matrix (reset)

    glTranslatef(xShift, yShift, 0); // translation matrix for all objects - no z used

    glScaled(zoom3D, zoom3D, zoom3D); // scale objects with zoom factor

    glRotatef(xRot, 1.0, 0.0, 0.0); // rotate all objects
    glRotatef(yRot, 0.0, 1.0, 0.0);
    glRotatef(zRot, 0.0, 0.0, 1.0);

    //// draw 3D origin axes

    if (axesEnabled) { // yes draw origin axes
        glLineWidth(2); // bigger width of the lines to really see them

        if (anaglyphEnabled) glColor3d(1,1,1);
            else glColor3d(1,0,0); // x axis color : red
        glBegin(GL_LINES); // draw several lines
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(1000.0f, 0.0f, 0.0f);
        glEnd();
        glBegin(GL_TRIANGLES); // triangle at the end of the line = arrow
            glVertex3f(  925.0f, -50.0f,   0.0f );
            glVertex3f(  925.0f,  50.0f,   0.0f );
            glVertex3f( 1000.0f,   0.0f,   0.0f );
        glEnd();

        if (anaglyphEnabled) glColor3d(0.85,0.85,0.85);
            else glColor3d(0,0,1); // y axis color : blue
        glBegin(GL_LINES);
            glVertex3f( 0.0f,     0.0f, 0.0f );
            glVertex3f( 0.0f, -1000.0f, 0.0f );
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex3f( -50.0f, -925.0f,   0.0f );
            glVertex3f(  50.0f, -925.0f,   0.0f );
            glVertex3f(   0.0f, -1000.0f,  0.0f );
        glEnd();

        if (anaglyphEnabled) glColor3d(0.75,0.75,0.75);
            else glColor3d(0,1,0); // z axis color : green
        glBegin(GL_LINES);
            glVertex3f( 0.0f, 0.0f,    0.0f);
            glVertex3f( 0.0f, 0.0f, 1000.0f);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex3f(0.0f, -50.0f, 925.0f);
            glVertex3f(0.0f,  50.0f, 925.0f);
            glVertex3f(0.0f,   0.0f, 1000.0f);
        glEnd();
    }

    if ((depthmap3D.empty()) | (image3D.empty())) // nothing more to render => exit
        return;

    if (computeVertices3D) { // totally recompute vertices
        ComputeVertices();
        updateVertices3D = false;
    }

    if (updateVertices3D) // partially recompute vertices
        UpdateVertices();

    if (computeIndexes3D) { // totally recompute vertices
        ComputeIndexes();
    }

    if (computeColors3D) // totally recompute vertices colors
        ComputeColors();

    if (anaglyphEnabled) {
        glRotatef(-anaglyphShift, 0.0, 1.0, 0.0);
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE); // if anaglyph this is the left (red) image
    }

    glEnableClientState(GL_VERTEX_ARRAY); // vertices drawing mode
    glEnableClientState(GL_COLOR_ARRAY); // with colors
        colorbuffer.bind(); // VBO color buffer used
            glColorPointer(3, GL_FLOAT, 0, NULL); // use color buffer
        colorbuffer.release(); // done for colors
        vertexbuffer.bind(); // the same for vertices
            glVertexPointer(3, GL_FLOAT, 0, NULL);
        vertexbuffer.release();
        indexbuffer.bind(); // the same for indexes
            glDrawElements(GL_TRIANGLE_STRIP, indexarray.size(), GL_UNSIGNED_INT, NULL); // draw triangles
        indexbuffer.release();
    glDisableClientState(GL_VERTEX_ARRAY); // finished defining vertices and colors
    glDisableClientState(GL_COLOR_ARRAY);

    if (anaglyphEnabled) { // if anaglyph draw the second (cyan) image

        glClear(GL_DEPTH_BUFFER_BIT); // only reset the depth not the colors this time
        //glTranslatef(anaglyphShift * 2048, 0, 0); // bad method !
        glRotatef(anaglyphShift, 0.0, 1.0, 0.0); // rotate a bit the second image on y axis
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); // blend the right image with the already drawn left image
        glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE); // draw only in the cyan channels (blue + green)

        glEnableClientState(GL_VERTEX_ARRAY); // exactly as the left image, using the same buffers
        glEnableClientState(GL_COLOR_ARRAY);
        colorbuffer.bind();
            glColorPointer(3, GL_FLOAT, 0, NULL);
        colorbuffer.release();
        vertexbuffer.bind();
            glVertexPointer(3, GL_FLOAT, 0, NULL);
        vertexbuffer.release();
        indexbuffer.bind();
            glDrawElements(GL_TRIANGLE_STRIP, indexarray.size(), GL_UNSIGNED_INT, NULL);
        indexbuffer.release();
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);

        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); // red, green and blue channels enabled again
    }
}

void openGLWidget::resizeGL(int width, int height) // called when the widget is resized
{
    double ratio = double(width) / height; // ratio of the widget : width / height
    glViewport(0, 0, width, height); // resize openGL viewport

    glMatrixMode(GL_PROJECTION); // openGL projection mode
    glLoadIdentity();
#ifdef QT_OPENGL_ES_1 // older versions of openGL
    glOrthof(-4 * 2048, +4 * 2048, -4 * 2048 / ratio, +4 * 2048  / ratio, -5000*2048, 5000*2048); // define view rectangle and clipping
#else
    glOrtho(-4 * 2048, +4 * 2048, -4 * 2048 / ratio, +4 * 2048 / ratio, -5000*2048, 5000*2048);
#endif
    glMatrixMode(GL_MODELVIEW); // now openGL model mode
}

///////////////////////////////////////////////
//// Drag with mouse + zoom with mouse wheel
////    + emit signals to get new values
///////////////////////////////////////////////

static void NormalizeAngle(int &angle) // angle must be between 0 and 359°
{
    while (angle < 0) angle = 360 + angle; // no negative values
    angle = angle % 360; // 0 -> 359°
}

void openGLWidget::SetXRotation(int angle) // set X rotation and emit a signal
{
    NormalizeAngle(angle); // angle must be between 0 and 359°
    if (angle != xRot) { // really changed ?
        xRot = angle;
        emit xRotationChanged(angle); // emit signal
        update(); // update 3D rendering
    }
}

void openGLWidget::SetYRotation(int angle) // set Y rotation and emit a signal
{
    NormalizeAngle(angle); // angle must be between 0 and 359°
    if (angle != yRot) { // really changed ?
        yRot = angle;
        emit yRotationChanged(angle); // emit signal
        update(); // update 3D rendering
    }
}

void openGLWidget::SetZRotation(int angle) // set Z rotation and emit a signal
{
    NormalizeAngle(angle); // angle must be between 0 and 359°
    if (angle != zRot) { // really changed ?
        zRot = angle;
        emit zRotationChanged(angle); // emit signal
        update(); // update 3D rendering
    }
}

void openGLWidget::SetAngleUp() // for keyboard control of x and y angles
{
    SetXRotation(xRot - 10);
}

void openGLWidget::SetAngleDown()
{
    SetXRotation(xRot + 10);
}

void openGLWidget::SetAngleLeft()
{
    SetYRotation(yRot - 10);
}

void openGLWidget::SetAngleRight()
{
    SetYRotation(yRot + 10);
}

void openGLWidget::SetXShift(int value) // move view (x)
{
    xShift = value;
    emit xShiftChanged(xShift); // emit signal
    update(); // update 3D rendering
}

void openGLWidget::SetYShift(int value) // move view (y)
{
    yShift = value;
    emit yShiftChanged(yShift); // emit signal
    update(); // update 3D rendering
}

void openGLWidget::SetShiftUp() // for keyboard control of x and y positions
{
    SetYShift(yShift + 10 * zoom3D);
}

void openGLWidget::SetShiftDown()
{
    SetYShift(yShift - 10 * zoom3D);
}

void openGLWidget::SetShiftLeft()
{
    SetXShift(xShift - 10 * zoom3D);
}

void openGLWidget::SetShiftRight()
{
    SetXShift(xShift + 10 * zoom3D);
}

void openGLWidget::mousePressEvent(QMouseEvent *event) // save initial mouse position for move and rotate
{
    lastPos = event->pos(); // save initial position of the mouse
}

void openGLWidget::mouseMoveEvent(QMouseEvent *event) // move and rotate view with mouse buttons
{
    int dx = event->x() - lastPos.x(); // how much the mouse has moved in pixels
    int dy = event->y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) { // left button = rotate on x and y axes
        SetXRotation(xRot + dy); // x = vertical
        SetYRotation(yRot + dx); // y = horizontal
    } else if (event->buttons() & Qt::RightButton) { // right button = move the view on x and y axes
        //setXRotation(xRot + 8 * dy);
        //setZRotation(zRot + 8 * dx);
        SetXShift(xShift + dx * 48);
        SetYShift(yShift - dy * 48);

        update(); // redraw 3d scene
    }

    lastPos = event->pos(); // save mouse position again
}

void openGLWidget::wheelEvent(QWheelEvent *event) // zoom
{
    int n = event->delta(); // amount of wheel turn
    //zoom3D += n / 120 / 2; // should work with this (standard) value

    if (n < 0) // zoom out
        zoom3D = zoom3D / 1.25;
    else // zoom in
        zoom3D = zoom3D * 1.25;

    emit zoomChanged(zoom3D); // emit signal

    update(); // redraw 3d scene
}

///////////////////////////////////////////////
//// Capture 3D scene to QImage
///////////////////////////////////////////////

void openGLWidget::Capture() // take a snapshot of rendered 3D scene
{
    capture3D = grabFramebuffer(); // slow because it relies on glReadPixels() to read back the pixels
}
