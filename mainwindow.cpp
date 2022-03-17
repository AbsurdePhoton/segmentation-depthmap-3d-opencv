/*#-------------------------------------------------
#
#   Segmentation to Depthmap to 3D with openCV
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1.5 - 2019/07/08
#
#-------------------------------------------------*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QCursor>
#include <QColorDialog>
#include <QSizeGrip>
#include <QGridLayout>
#include <QDesktopWidget>
#include <QSettings>

#include "mat-image-tools.h"
#include "dispersion3D.h"

using namespace cv;
using namespace cv::ximgproc;
using namespace std;

/////////////////// Window init //////////////////////

void MainWindow::AddCurveItem(const QString &title, const QColor &color, const QString &tip) // used to add gray gradient curves to the list
{
    QListWidgetItem *item = new QListWidgetItem (); // create new label
    item->setText(title); // set name to current label
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // item enabled and selectable
    item->setSelected(false); // but don't select it !
    item->setForeground(color); // text color
    item->setToolTip(tip); // help bubble
    ui->listWidget_gradient_curve->addItem(item); // add new item to the list
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // window
    setWindowFlags((((windowFlags() | Qt::CustomizeWindowHint)
                            & ~Qt::WindowCloseButtonHint) | Qt::WindowMinMaxButtonsHint)); // don't show buttons in title bar
    //this->setWindowState(Qt::WindowMaximized); // maximize window
    setFocusPolicy(Qt::StrongFocus); // catch keyboard and mouse in priority

    // add size grip to openGL widget
    /*
    ui->openGLWidget_3d->setWindowFlags(Qt::SubWindow);
    QSizeGrip * sizeGrip = new QSizeGrip(ui->openGLWidget_3d);
    QGridLayout * layout = new QGridLayout(ui->openGLWidget_3d);
    layout->addWidget(sizeGrip, 0,0,1,1,Qt::AlignBottom | Qt::AlignRight);
    */

    // populate gray gradient curve combobox
    ui->listWidget_gradient_curve->blockSignals(true); // don't trigger automatic actions for these widgets
    AddCurveItem("LINEAR",      QColor(0,128,0),    "Gray levels go straight from beginning to end\n    f(x) = x");
    AddCurveItem("COSINUS²",    QColor(0,0,128),    "Gray levels are spread a bit more at beginning and end\n    f(x)=cos(pi/2-x*pi/2)²");
    AddCurveItem("SIGMOID",     QColor(0,0,128),    "S-shaped gray levels\n1/3 beginning 1/3 gradient 1/3 end\n    f(x)=1/(1 + e(-5*(2x -1))");
    AddCurveItem("COSINUS",     QColor(128,128,0),  "Fast beginning and strong end grays\n    f(x)=cos(pi/2-x*pi/2)");
    AddCurveItem("COS²SQRT",    QColor(128,128,0),  "Center grays are predominant\n    f(x)=cos(pi/2−sqrt(x)*pi/2)²");
    AddCurveItem("POWER²",      QColor(128,64,0),   "1/3 of beginning level then gradient for the rest, fast ending\n    f(x)=x²");
    AddCurveItem("COS²POWER2",  QColor(128,64,0),   "Very strong beginning then gradient\n    f(x)=cos(pi/2−x²∙pi/2)²");
    AddCurveItem("POWER³",      QColor(128,64,0),   "Very strong beginning and fast ending\n    f(x)=x³");
    AddCurveItem("UNDULATE",    QColor(128,0,0),    "Waves modulated by gray levels range\n    f(x)=cos(x/4*pi)");
    AddCurveItem("UNDULATE²",   QColor(128,0,0),    "Increasing waves modulated by gray levels range\n    f(x)=cos((x*2*pi)²)/2+0.5");
    AddCurveItem("UNDULATE+",   QColor(128,0,0),    "Increasing levels of gray stripes\n    f(x)=f(x) = cos(pi²∙(x+2.085)²) / ((x+2.085)³+10) + (x+2.085) − 2.11");
    ui->listWidget_gradient_curve->blockSignals(false);

    // populate tints comboBox
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/color.png"), "Color");
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/gray.png"), "Gray");
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/anaglyph.png"), "True Red-Cyan");
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/image-off.png"), "Half-tint");
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/image.png"), "Optimized");
    ui->comboBox_3d_tint->addItem(QIcon(":/icons/compute.png"), "Dubois");

    // other UI elements
    ui->frame_gradient->setEnabled(false); // only enabled when a segmentation or depthmap XML file is loaded
    ui->spinBox_3d_resolution->setValue(ui->openGLWidget_3d->width()); // default size openGL widget

    // initial variable values
    InitializeValues();
}

MainWindow::~MainWindow()
{
    delete ui;
}

///////////////////      GUI       //////////////////////

void MainWindow::InitializeValues() // Global variables init
{

    loaded = false; // main image NOT loaded
    zoom = 1; // init zoom
    oldZoom = 1; // to detect a zoom change
    zoom_type = ""; // not set now, can be "button" or (mouse) "wheel"

    basedirinifile = QDir::currentPath().toUtf8().constData();
    basedirinifile += "/dir.ini";
    cv::FileStorage fs(basedirinifile, FileStorage::READ); // open dir ini file
    if (fs.isOpened()) {
        fs["BaseDir"] >> basedir; // load labels
    }
        else basedir = "/home/"; // base path and file
    basefile = "example";
    nbLabels = 0; // no labels yet
    updateVertices3D = false; // not an update
    computeVertices3D = true; // update openGL widget vertices now
    ui->openGLWidget_3d->computeIndexes3D = true; // update openGL widget indexes now
    computeColors3D = true; // update openGL widget colors now
    moveBegin = false; // not moving any arrow now
    moveEnd = false;

    // load 3D example
    image = QImage2Mat(QImage(":/example/absurdephoton-image.png"));
    depthmap = QImage2Mat(QImage(":/example/absurdephoton-depthmap.png"));
    cvtColor(depthmap, depthmap, COLOR_BGR2GRAY);
    ui->openGLWidget_3d->image3D = image; // transfer data to widget
    ui->openGLWidget_3d->depthmap3D = depthmap;
    ui->openGLWidget_3d->area3D = Rect(0, 0, image.cols,image.rows);
    ui->openGLWidget_3d->mask3D = Mat::zeros(image.rows, image.cols, CV_8UC1);
    ui->openGLWidget_3d->depth3D = 1; // initial view
    ui->openGLWidget_3d->anaglyphShift = -1.5;
    ui->openGLWidget_3d->computeVertices3D = true; // recompute 3D vertices
    ui->openGLWidget_3d->computeIndexes3D = true; // recompute 3D indexes
    ui->openGLWidget_3d->computeColors3D = true; // recompute 3D colors
    ui->openGLWidget_3d->update(); // apply !
}

void MainWindow::on_button_quit_clicked()
{
    int quit = QMessageBox::question(this, "Quit this wonderful program", "Are you sure you want to quit?", QMessageBox::Yes|QMessageBox::No); // quit, are you sure ?
    if (quit == QMessageBox::No) // don't quit !
        return;

    writePositionSettings();

    QCoreApplication::quit();
}

///////////////////    Labels     //////////////////////

int MainWindow::GetCurrentLabelNumber() // label number for use with "label" mask
{
    return ui->listWidget_labels->currentItem()->data(Qt::UserRole).toInt(); // label number stored in this special field
}

void MainWindow::DeleteAllLabels() // delete all labels in the list
{
    ui->listWidget_labels->blockSignals(true); // the labels list must not trigger any action
    ui->listWidget_labels->clear(); // delete all labels
    ui->listWidget_labels->blockSignals(false); // return to normal
}

void MainWindow::BlockGradientsSignals(const bool &active) // block or not all gradient elements signals
{
    ui->horizontalSlider_begin->blockSignals(active);
    ui->horizontalSlider_end->blockSignals(active);
    ui->spinBox_color_begin->blockSignals(active);
    ui->spinBox_color_end->blockSignals(active);
    ui->radioButton_flat->blockSignals(active);
    ui->radioButton_linear->blockSignals(active);
    ui->radioButton_double_linear->blockSignals(active);
    ui->radioButton_radial->blockSignals(active);
    ui->listWidget_gradient_curve->blockSignals(active);
}

void MainWindow::on_listWidget_labels_currentItemChanged(QListWidgetItem *currentItem) // several actions when label changes
{
    ui->label_name->setText(currentItem->text()); // display label name as current

    int id = currentItem->data(Qt::UserRole).toInt(); // get label id
    currentLabelMask = labels == id; // extract label using its id
    selection = 0; // erase selection mask
    //selection.setTo(Vec3b(0, 32, 32), mask_temp); // fill with dark yellow

    vector<vector<cv::Point>> contours;
    vector<Vec4i> hierarchy;
    findContours(currentLabelMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // find new cell contours

    selection_rect = boundingRect(contours[0]); // find biggest rectangle
    for (uint n = 1; n < contours.size(); n++) { // ... by cycling through all found contours
        Rect rect_temp = boundingRect(contours[n]); // current Rect
        if (rect_temp.x < selection_rect.x) { // adjust size
            selection_rect.width += selection_rect.x - rect_temp.x;
            selection_rect.x = rect_temp.x;
        }
        if (rect_temp.y < selection_rect.y) {
            selection_rect.height += selection_rect.y - rect_temp.y;
            selection_rect.y = rect_temp.y;
        }
        if (rect_temp.x + rect_temp.width > selection_rect.x + selection_rect.width)
            selection_rect.width = rect_temp.x + rect_temp.width - selection_rect.x;
        if (rect_temp.y + rect_temp.height > selection_rect.y + selection_rect.height)
            selection_rect.height = rect_temp.y + rect_temp.height - selection_rect.y;
    }
    ui->openGLWidget_3d->area3D = selection_rect; // update opengl widget elements
    ui->openGLWidget_3d->mask3D = currentLabelMask;

    drawContours(selection, contours, -1, Vec3b(0, 255, 255), 1, 8, hierarchy ); // draw contour of new cell in selection mask
    /*cv::rectangle(selection, Rect(selection_rect.x, selection_rect.y, selection_rect.width, selection_rect.height),
                          Vec3b(255, 255, 255), 2); // draw entire selection rectangle*/

    BlockGradientsSignals(true); // don't trigger automatic actions for these widgets

    int row = ui->listWidget_labels->currentRow(); // get current row of the list to access array indexed on it

    switch (gradients[row].gradient) { // gradient type ?
        case gradient_flat: {
            ui->radioButton_flat->setChecked(true); // set correspondng radio button
            break;
        }
        case gradient_linear: {
            ui->radioButton_linear->setChecked(true);
            break;
        }
        case gradient_doubleLinear: {
            ui->radioButton_double_linear->setChecked(true);
            break;
        }
        case gradient_radial: {
            ui->radioButton_radial->setChecked(true);
            break;
        }
    }

    ui->horizontalSlider_begin->setValue(gradients[row].beginColor); // show begin and end colors
    ui->horizontalSlider_end->setValue(gradients[row].endColor);
    ui->spinBox_color_begin->setValue(gradients[row].beginColor);
    ui->spinBox_color_end->setValue(gradients[row].endColor);
    ui->listWidget_gradient_curve->setCurrentRow(gradients[row].curve); // and the gray gradient curve type

    BlockGradientsSignals(false); // signals : return to normal

    ShowGradient(); // show the nice gradient example
    Render(); // update global view
}

/////////////////// Save and load //////////////////////

void MainWindow::SaveDirBaseFile()
{
    cv::FileStorage fs(basedirinifile, cv::FileStorage::WRITE); // open dir ini file for writing
    fs << "BaseDir" << basedir; // write folder name
    fs.release(); // close file
}

void MainWindow::ChangeBaseDir(QString filename) // Set base dir and file
{
    basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files

    // Remove extension if present
    size_t period_idx = basefile.rfind('.');
    if (std::string::npos != period_idx)
        basefile.erase(period_idx);

    basedir = basefile;
    size_t found = basefile.find_last_of("\\/"); // find last directory
    std::string separator = basefile.substr(found,1); // copy path separator (Linux <> Windows)
    basedir = basedir.substr(0,found) + separator; // define base path
    basefile = basefile.substr(found+1); // delete path in base file

    SaveDirBaseFile(); // Save current path to ini file
}

void MainWindow::DisableGUI() // reset entire GUI - used when something goes wrong when loading files
{
    loaded = false;
    ui->label_viewport->setPixmap(QPixmap());
    ui->label_thumbnail->setPixmap(QPixmap());
    ui->frame_gradient->setEnabled(false);
    image.release();
    depthmap.release();
    labels.release();
    ui->openGLWidget_3d->image3D.release();
    ui->openGLWidget_3d->depthmap3D.release();
    ui->openGLWidget_3d->computeVertices3D = true;
    ui->openGLWidget_3d->computeColors3D = true;
    ui->openGLWidget_3d->update();
    DeleteAllLabels(); // delete all labels but do not create a new one
    ui->label_filename->setText("Please load a file"); // delete file name in ui
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::on_button_load_segmentation_clicked() // load segmentation XML file and corresponding image files
{
    QString filename = QFileDialog::getOpenFileName(this, "Load segmentation from XML file...", QString::fromStdString(basedir + "*-segmentation-data.xml"), "*.xml *.XML"); // get file name

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();

    /*basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    size_t pos = basefile.find(".xml");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    basefile = basefile.substr(found+1); // delete ending slash
    SaveDirBaseFile(); // Save current path to ini file*/
    ChangeBaseDir(filename);

    size_t pos = basefile.find("-segmentation-data"); // the XML file must end with this
    if (pos != std::string::npos) // yes !
        basefile.erase(pos, basefile.length());
    else { // doesn't end with "-segmentation-data"
        QMessageBox::critical(this, "File name error",
                              "There was a problem reading the segmentation mask file:\nit must end with ''-segmentation-data.xml''");
        DisableGUI(); // problem : reset GUI elements and exit
        return;
    }

    ui->label_filename->setText(filename); // display file name in GUI

    std::string filesession = filename.toUtf8().constData(); // base file name
    pos = filesession.find("-segmentation-data.xml"); // ends with "-segmentation-data.xml"
    if (pos != std::string::npos) filesession.erase(pos, filesession.length()); // this is what will be used to name files hereafter

    depthmap = cv::imread(filesession + "-segmentation-mask.png", IMREAD_COLOR); // load segmentation mask
    if (depthmap.empty()) { // mask empty, not good !
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation mask file:\nit must end with ''-segmentation-mask.png''");
        DisableGUI(); // problem : reset GUI elements and exit
        return;
    }

    image = cv::imread(filesession + "-segmentation-image.png"); // load reference image

    if (image.empty()) {
        QMessageBox::critical(this, "File error",
                                      "There was a problem reading the segmentation image file:\nit must end with ''-segmentation-image.png''");
        DisableGUI();
        return;
    }

    if ((image.cols != depthmap.cols) | (image.rows != depthmap.rows)) { // image and mask sizes not the same -> not good !
        QMessageBox::critical(this, "Image size error",
                                    "The image and mask image size (width and height) differ");
        DisableGUI();
        return;
    }

    depthmap = Mat::zeros(image.rows, image.cols, CV_8UC1); // initialize depthmap mask to image size
    selection = Mat::zeros(image.rows, image.cols, CV_8UC3); // initialize selection mask to image size

    DeleteAllLabels(); // delete all labels in the list

    cv::FileStorage fs(filesession + "-segmentation-data.xml", FileStorage::READ); // open labels file

    if (!fs.isOpened()) { // file not found ? this error is not handled by the above instructions
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the segmentation data file:\nit must end with ''-segmentation-data.xml''");
        DisableGUI();
        return;
    }

    labels.release(); // reset labels data
    try { // try to load labels data
        fs["LabelsMask"] >> labels; // load labels mask
    }
    catch( cv::Exception& e ) // problem ?
    {
        const char* err_msg = e.what();
        QMessageBox::critical(this, "XML Segmentation file error",
                              "There was a problem reading the segmentation XML file\nThe \"LabelsMask\" data is wrong\nError:\n"
                              + QString(err_msg));
        DisableGUI();
        return;
    }

    nbLabels = 0; // time to read each label specific data - initialize labels count
    fs["LabelsCount"] >> nbLabels; // read how many labels to load

    if (nbLabels <= 0) { // no labels ?
        QMessageBox::critical(this, "XML Segmentation file error",
                              "There was a problem reading the segmentation XML file\nThe data is wrong");
        DisableGUI();
        return;
    }

    ui->listWidget_labels->blockSignals(true); // the labels list must not trigger any action

    for (int i = 0; i < nbLabels; i++) { // for each label to load
        int num = -1;
        std::string name = "###Error###"; // errors are not handled here, better set rubbish values, the user will see it anyway

        QListWidgetItem *item = new QListWidgetItem (); // create new label item
        std::string field;

        field = "LabelId" + std::to_string(i); // read label id
        fs [field] >> num;
        item->setData(Qt::UserRole, num); // set it to current label

        field = "LabelName" + std::to_string(i); // read label name
        fs [field] >> name;
        item->setText(QString::fromStdString(name)); // set name to current label

        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // item enabled, editable and selectable

        ui->listWidget_labels->addItem(item); // add the label item to the list
        item->setSelected(false); // but don't select it !

        Mat1b mask_temp = labels == num; // extract label to temp depthmap
        Moments m = moments(mask_temp, false); // compute the barycenter of the label representation
        cv::Point p(m.m10/m.m00, m.m01/m.m00);

        uchar col = int(round(double(p.y) / image.rows * 255)); // set an arbitray gray level based on the barycenter height in image

        depthmap.setTo(col, mask_temp); // color this label mask in global depthmap with barycenter color

        // set values in the gradients array corresponding to to label id
        gradients[i].beginColor = col; // gradient begin and end color are barycenter color
        gradients[i].endColor = col;
        gradients[i].beginPoint = p; // begin point of gradient arrow is the barycenter
        if (p.y - 50 > 0) // the arrow is 50 pixels long (vertical), the head should stay in the image rectangle
            gradients[i].endPoint = cv::Point(p.x, p.y - 50);
        else
            gradients[i].endPoint = cv::Point(p.x, p.y + 50);
        gradients[i].gradient = gradient_flat; // gradient flat at first
        gradients[i].curve = curve_linear; // and gradient curve set to linear
    }

    fs.release(); // close file

    loaded = true; // we've done it !

    ui->label_thumbnail->setPixmap(QPixmap()); // flush current thumbnail
    ui->label_viewport->setPixmap(QPixmap()); // delete viewport

    ui->horizontalScrollBar_viewport->setMaximum(0); // update scrollbars
    ui->verticalScrollBar_viewport->setMaximum(0);
    ui->horizontalScrollBar_viewport->setValue(0);
    ui->verticalScrollBar_viewport->setValue(0);

    ui->label_image_width->setText(QString::number(image.cols)); // display image dimensions
    ui->label_image_height->setText(QString::number(image.rows));

    ui->checkBox_image->setChecked(false);
    ui->checkBox_depthmap->setChecked(true);
    ui->checkBox_3d_blur->setChecked(false); // several options that should be reset in GUI
    ui->comboBox_3d_tint->setCurrentIndex(0);
    ui->doubleSpinBox_gamma->setValue(1);
    ui->frame_gradient->setEnabled(true);

    double zoomX = double(ui->label_viewport->width()) / image.cols; // find the best fit for the viewport : try vertical and horizontal ratios
    double zoomY = double(ui->label_viewport->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fits the view
        else zoom = zoomY;
    oldZoom = zoom; // no zoom change
    ShowZoomValue(); // display current zoom value
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport size

    thumbnail = ResizeImageAspectRatio(image, cv::Size(ui->label_thumbnail->width(),ui->label_thumbnail->height())); // create thumbnail
    ShowThumbnail();

    ui->openGLWidget_3d->depthmap3D = depthmap; // initialize 3D view data
    ui->openGLWidget_3d->image3D = image;

    computeVertices3D = true; // update openGL widget vertices
    ui->openGLWidget_3d->computeIndexes3D = true; // update openGL widget indexes
    computeColors3D = true; // update openGL widget colors

    ui->listWidget_labels->blockSignals(false); // return to normal for labels
    ui->listWidget_labels->setCurrentRow(0); // and select the first item

    QApplication::restoreOverrideCursor(); // Restore cursor

    //QMessageBox::information(this, "Load segmentation session", "Session loaded with base name:\n" + QString::fromStdString(filesession));
}

void MainWindow::on_button_save_depthmap_clicked() // save XML and image depthmap files
{
    if (!loaded) { // nothing loaded yet = get out
        QMessageBox::warning(this, "Nothing to save",
                                 "Not now!\n\nBefore anything else, load a Segmentation or Depthmap project");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, "Save depthmap to XML file...", "./" + QString::fromStdString(basedir + basefile + "-depthmap-data.xml"), "*.xml *.XML"); // filename

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();

    /*// base file name and dir can change so reset them
    basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    size_t pos = basefile.find(".xml");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    basefile = basefile.substr(found+1); // delete ending slash*/
    ChangeBaseDir(filename);
    size_t pos = basefile.find("-depthmap-data");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());

    std::string filesession = filename.toUtf8().constData();
    pos = filesession.find("-depthmap-data.xml"); // use base file name
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());
    pos = filesession.find(".xml"); // use base file name
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());

    bool write;
    Mat depthmap_temp;
    cvtColor(depthmap, depthmap_temp, COLOR_GRAY2RGB);
    write = cv::imwrite(filesession + "-depthmap-mask.png", depthmap_temp); // save depthmap mask
    if (!write) { // problem ?
        QApplication::restoreOverrideCursor(); // Restore cursor
        QMessageBox::critical(this, "File error",
                              "There was a problem saving the depthmap mask image file");
        return;
    }

    write = cv::imwrite(filesession + "-depthmap-image.png", image); // save reference image
    if (!write) { // problem ?
        QApplication::restoreOverrideCursor(); // Restore cursor
        QMessageBox::critical(this, "File error",
                              "There was a problem saving the depthmap image file");
        return;
    }

    cv::FileStorage fs(filesession + "-depthmap-data.xml", cv::FileStorage::WRITE); // open depthmap XML file for writing
    if (!fs.isOpened()) { // problem ?
        QApplication::restoreOverrideCursor(); // Restore cursor
        QMessageBox::critical(this, "File error",
                              "There was a problem writing the depthmap data file");
        return;
    }

    fs << "LabelsCount" << nbLabels; // write labels count

    for (int i = 0; i < nbLabels; i++) { // for each label
        std::string field;

        QListWidgetItem *item = ui->listWidget_labels->item(i); // get label id
        field = "LabelId" + std::to_string(i);
        fs << field << item->data(Qt::UserRole).toInt(); // write label id

        std::string name = item->text().toUtf8().constData(); // write label name
        field = "LabelName" + std::to_string(i);
        fs << field << name;

        field = "GradientType" + std::to_string(i);
        fs << field << gradients[i].gradient; // write gradient type

        field = "GradientCurve" + std::to_string(i);
        fs << field << gradients[i].curve; // write gradient curve

        field = "GradientBeginColor" + std::to_string(i);
        fs << field << gradients[i].beginColor; // write gradient begin color

        field = "GradientEndColor" + std::to_string(i);
        fs << field << gradients[i].endColor; // write gradient end color

        field = "GradientBeginPoint" + std::to_string(i);
        fs << field << gradients[i].beginPoint; // write gradient begin point

        field = "GradientEndPoint" + std::to_string(i);
        fs << field << gradients[i].endPoint; // write gradient end point
    }

    fs << "Labels" << labels; // write labels data

    fs.release(); // close file

    ui->label_filename->setText(QString::fromStdString(filesession + "-depthmap-data.xml")); // display new file name in ui

    QApplication::restoreOverrideCursor(); // Restore cursor

    QMessageBox::information(this, "Save depthmap session", "Session successfuly saved with base name:\n" + QString::fromStdString(basefile));
}

void MainWindow::on_button_load_depthmap_clicked() // load depthmap XML file
{
    QString filename = QFileDialog::getOpenFileName(this, "Load depthmap from XML file...", QString::fromStdString(basedir + "*-depthmap-data.xml"), "*.xml *.XML"); // file name

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();

    /*basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    size_t pos = basefile.find(".xml");
    if (pos != std::string::npos) basefile.erase(pos, basefile.length());
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    SaveDirBaseFile(); // Save current path to ini file

    basefile = basefile.substr(found+1); // delete ending slash*/
    ChangeBaseDir(filename);
    size_t pos = basefile.find("-depthmap-data");
    if (pos != std::string::npos)
        basefile.erase(pos, basefile.length());
    else { // doesn't end with "-depthmap-data"
        QMessageBox::critical(this, "File name error",
                              "There was a problem reading the depthmap file:\nit must end with ''-depthmap-data.xml''");
        DisableGUI(); // problem : reset GUI elements and exit
        return;
    }

    std::string filesession = filename.toUtf8().constData(); // base file name
    pos = filesession.find("-depthmap-data.xml"); // ends with "depthmap-data.xml"
    if (pos != std::string::npos) filesession.erase(pos, filesession.length());

    depthmap = cv::imread(filesession + "-depthmap-mask.png", IMREAD_COLOR); // load depthmap mask
    if (depthmap.channels() > 1)
        cvtColor(depthmap, depthmap, COLOR_BGR2GRAY);
    if (depthmap.empty()) { // problem ?
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the depthmap mask file:\nit must end with ''-depthmap-mask.png''");
        DisableGUI();
        return;
    }

    image = cv::imread(filesession + "-depthmap-image.png"); // load reference image

    if (image.empty()) {
        QMessageBox::critical(this, "File error",
                                    "There was a problem reading the depthmap image file:\nit must end with ''-depthmap-image.png''");
        DisableGUI();
        return;
    }

    if ((image.cols != depthmap.cols) | (image.rows != depthmap.rows)) { // image and mask sizes not the same -> not good !
        QMessageBox::critical(this, "Image size error",
                                    "The image and mask image size (width and height) differ");
        DisableGUI();
        return;
    }

    selection = Mat::zeros(image.rows, image.cols, CV_8UC3); // initialize selection mask to image size

    DeleteAllLabels(); // delete all labels in the list

    cv::FileStorage fs(filesession + "-depthmap-data.xml", FileStorage::READ); // open labels file

    if (!fs.isOpened()) { // file not opened, not handled by above instructions
        QMessageBox::critical(this, "File error",
                              "There was a problem reading the depthmap data file:\nit must end with ''-depthmap-data.xml''");
        DisableGUI();
        return;
    }

    Mat labels_temp = Mat::zeros(image.rows, image.cols, CV_32SC1); // labels on 1 channel
    try { // try to read labels data
        fs["Labels"] >> labels_temp; // load labels
    }
    catch( cv::Exception& e ) // problem ?
    {
        const char* err_msg = e.what(); // get error from openCV
        QMessageBox::critical(this, "XML Depthmap file error",
                              "There was a problem reading the depthmap XML file\nThe \"Labels\" data is wrong\nError:\n"
                              + QString(err_msg));
        DisableGUI();
        return;
    }

    labels_temp.copyTo(labels); // valid data copied to labels

    nbLabels = 0; // labels count
    fs["LabelsCount"] >> nbLabels; // read how many labels to load ?

    if (nbLabels <= 0) { // no labels ?
        QMessageBox::critical(this, "XML Depthmap file error",
                              "There was a problem reading the depthmap XML file\nThe data is wrong");
        DisableGUI();
        return;
    }

    ui->listWidget_labels->blockSignals(true); // don't trigger events when populating labels list

    for (int i = 0; i < nbLabels; i++) { // for each label to load
        std::string name ="###Error###"; // init default values, no error handling this time, the user should see if there was a problem
        int num = -1;
        cv::Point point = cv::Point(0,0);
        int color = 255;
        int gtype = 0;
        int gcurve = 0;

        QListWidgetItem *item = new QListWidgetItem (); // create new label item
        std::string field;

        field = "LabelId" + std::to_string(i); // read label id
        fs [field] >> num;
        item->setData(Qt::UserRole, num); // set it to current label

        field = "LabelName" + std::to_string(i); // read label name
        fs [field] >> name;
        item->setText(QString::fromStdString(name)); // set name to current label

        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // item enabled and selectable

        ui->listWidget_labels->addItem(item); // add new item to labels list
        item->setSelected(false); // don't select it !

        field = "GradientType" + std::to_string(i);
        fs [field] >> gtype; // load gradient type
        gradients[i].gradient = gradientType(gtype);

        field = "GradientCurve" + std::to_string(i);
        fs [field] >> gcurve; // load curve type
        gradients[i].curve = curveType(gcurve);

        field = "GradientBeginColor" + std::to_string(i);
        fs [field] >> color; // load gradient begin color
        gradients[i].beginColor = color;

        field = "GradientEndColor" + std::to_string(i);
        fs [field] >> color; // load gradient end color
        gradients[i].endColor = color;

        field = "GradientBeginPoint" + std::to_string(i);
        fs [field] >> point; // load gradient begin point
        gradients[i].beginPoint = point;

        field = "GradientEndPoint" + std::to_string(i);
        fs [field] >> point; // load gradient end point
        gradients[i].endPoint = point;
    }

    fs.release(); // close file

    loaded = true; // all good !

    ui->label_filename->setText(filename); // display file name in ui

    ui->label_thumbnail->setPixmap(QPixmap());
    ui->label_viewport->setPixmap(QPixmap()); // delete depthmap image

    ui->horizontalScrollBar_viewport->setMaximum(0); // update scrollbars
    ui->verticalScrollBar_viewport->setMaximum(0);
    ui->horizontalScrollBar_viewport->setValue(0);
    ui->verticalScrollBar_viewport->setValue(0);

    ui->label_image_width->setText(QString::number(image.cols)); // display image dimensions
    ui->label_image_height->setText(QString::number(image.rows));

    ui->checkBox_image->setChecked(false);
    ui->checkBox_depthmap->setChecked(true);
    ui->checkBox_3d_blur->setChecked(false); // init several GUI items to default
    ui->comboBox_3d_tint->setCurrentIndex(0);
    ui->doubleSpinBox_gamma->setValue(1);
    ui->frame_gradient->setEnabled(true); // enable gradients selection

    double zoomX = double(ui->label_viewport->width()) / image.cols; // find best fit for the viewport, try vertical and horizontal ratios
    double zoomY = double(ui->label_viewport->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fits the view
        else zoom = zoomY;
    oldZoom = zoom; // zoom already good
    ShowZoomValue(); // display current zoom
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport size

    thumbnail = ResizeImageAspectRatio(image, cv::Size(ui->label_thumbnail->width(),ui->label_thumbnail->height())); // create thumbnail

    ShowThumbnail(); // update thumbnail view

    ui->openGLWidget_3d->depthmap3D = depthmap; // init new 3D scene
    ui->openGLWidget_3d->image3D = image;

    computeVertices3D = true; // update openGL widget vertices
    ui->openGLWidget_3d->computeIndexes3D = true; // update openGL widget indexes
    computeColors3D = true; // update openGL widget colors

    ui->listWidget_labels->blockSignals(false); // return to normal for labels
    ui->listWidget_labels->setCurrentRow(0); // select first row of the list

    QApplication::restoreOverrideCursor(); // Restore cursor

    //QMessageBox::information(this, "Load depthmap session", "Session loaded with base name:\n" + QString::fromStdString(filesession));
}

void MainWindow::on_button_load_rgbd_clicked() // load previous session
{
    QString filename = QFileDialog::getOpenFileName(this, "Load RGB+D : image...", QString::fromStdString(basedir),
                                                    "Images (*.jpg *.JPG *.jpeg *.JPEG *.jp2 *.JP2 *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.bmp *.BMP)"); // reference image file name

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();

    /*basefile = filename.toUtf8().constData(); // base file name and dir are used after to save other files
    basefile = basefile.substr(0, basefile.size()-4); // strip file extension
    basedir = basefile;
    size_t found = basefile.find_last_of("/"); // find last directory
    basedir = basedir.substr(0,found) + "/"; // extract file location
    SaveDirBaseFile(); // Save current path to ini file

    basefile = basefile.substr(found+1); // delete ending slash
    ui->label_filename->setText(filename); // display file name in ui*/
    ChangeBaseDir(filename);

    std::string filesession = filename.toUtf8().constData(); // base file name
    image = cv::imread(filesession); // load image
    if (image.empty()) {
        QMessageBox::critical(this, "File error", "There was a problem reading the image file");
        DisableGUI();
        return;
    }

    filename = QFileDialog::getOpenFileName(this, "Load RGB+D : depthmap...", QString::fromStdString(basedir),
                                            "Images (*.jpg *.JPG *.jpeg *.JPEG *.jp2 *.JP2 *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.bmp *.BMP)"); // depthmap file name
    ChangeBaseDir(filename);
    filesession = filename.toUtf8().constData(); // base file name

    depthmap = cv::imread(filesession, IMREAD_COLOR); // load depthmap
    if (depthmap.channels() > 1)
        cvtColor(depthmap, depthmap, COLOR_BGR2GRAY);

    if ((depthmap.empty()) | (image.cols != depthmap.cols) | (image.rows != depthmap.rows)) { // if image and depthmap sizes differ or depthmap empty
        if ((image.cols != depthmap.cols) | (image.rows != depthmap.rows)) // sizes differ
            QMessageBox::critical(this, "Image size error", "The image and depthmap size (width and height) must be the same.");
        else
            QMessageBox::critical(this, "File error", "There was a problem reading the depthmap file");
        DisableGUI();
        return;
    }

    selection = Mat::zeros(image.rows, image.cols, CV_8UC3); // initialize selection mask to image size

    labels = Mat::zeros(image.rows, image.cols, CV_32SC1); // labels on 1 channel

    DeleteAllLabels(); // delete all labels but do not create a new one

    loaded = true; // loaded successfully !

    ui->label_filename->setText(filename); // display file name in ui

    ui->label_thumbnail->setPixmap(QPixmap()); // reinit several GUI elements
    ui->label_viewport->setPixmap(QPixmap());

    ui->checkBox_image->setChecked(true); // no need for that
    ui->checkBox_depthmap->setChecked(false);
    ui->frame_gradient->setEnabled(false);
    ui->checkBox_3d_blur->setChecked(false); // reinit 3D view options
    ui->comboBox_3d_tint->setCurrentIndex(0);
    ui->doubleSpinBox_gamma->setValue(1);

    ui->horizontalScrollBar_viewport->setMaximum(0); // update scrollbars
    ui->verticalScrollBar_viewport->setMaximum(0);
    ui->horizontalScrollBar_viewport->setValue(0);
    ui->verticalScrollBar_viewport->setValue(0);

    ui->label_image_width->setText(QString::number(image.cols)); // display image dimensions
    ui->label_image_height->setText(QString::number(image.rows));

    double zoomX = double(ui->label_viewport->width()) / image.cols; // find best fit for viewport, try vertical and horizontal ratios
    double zoomY = double(ui->label_viewport->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // the lowest fits the view
        else zoom = zoomY;
    oldZoom = zoom; // zoom already set
    ShowZoomValue(); // display current zoom
    viewport = Rect(0, 0, image.cols, image.rows); // update viewport size

    thumbnail = ResizeImageAspectRatio(image, cv::Size(ui->label_thumbnail->width(),ui->label_thumbnail->height())); // create thumbnail

    CopyFromImage(image, viewport).copyTo(disp_color); // copy only the viewport part of image
    QPixmap D;
    D = Mat2QPixmapResized(disp_color, int(viewport.width*zoom), int(viewport.height*zoom), true); // zoomed image
    ui->label_viewport->setPixmap(D); // Set new image content viewport
    ShowThumbnail(); // update thumbnail view

    ui->openGLWidget_3d->depthmap3D = depthmap; // init 3D scene
    ui->openGLWidget_3d->image3D = image;
    ui->openGLWidget_3d->area3D = Rect(0, 0, image.rows, image.cols);
    ui->openGLWidget_3d->mask3D = Mat(image.rows, image.cols, CV_8UC1);
    ui->openGLWidget_3d->computeVertices3D = true; // recompute the 3d scene
    ui->openGLWidget_3d->computeIndexes3D = true; // update openGL widget indexes
    ui->openGLWidget_3d->computeColors3D = true;
    ui->openGLWidget_3d->update(); // view 3D scene

    QApplication::restoreOverrideCursor(); // Restore cursor

    //QMessageBox::information(this, "Load depthmap session", "Session loaded with base name:\n" + QString::fromStdString(filesession));
}

void MainWindow::on_button_save_ply_clicked() // save XML and image depthmap files
{
    if (!loaded) { // nothing loaded yet = get out
        QMessageBox::warning(this, "Nothing to save",
                                 "Not now!\n\nBefore anything else, load a Segmentation or Depthmap project");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, "Save mesh to PLY file...", "./" + QString::fromStdString(basedir + basefile + ".ply"), "*.ply *.PLY"); // filename

    if (filename.isNull() || filename.isEmpty()) // cancel ?
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();

    ui->openGLWidget_3d->SaveToPly(filename);

    QApplication::restoreOverrideCursor(); // Restore cursor

    QMessageBox::information(this, "Export 3D mesh", "Mesh successfully exported with file name:\n" + filename);
}

/////////////////// Display controls //////////////////////

void MainWindow::on_checkBox_depthmap_clicked() // Refresh image : depthmap on/off
{
    Render(); // update display
}

void MainWindow::on_checkBox_image_clicked() // Refresh image : image on/off
{
    Render(); // update display
}

void MainWindow::on_horizontalSlider_blend_depthmap_valueChanged() // refresh image : blend depthmap transparency changed
{
    Render(); // update display
}

void MainWindow::on_horizontalSlider_blend_image_valueChanged() // refresh image : image mask transparency changed
{
    Render(); // update display
}

void MainWindow::on_horizontalScrollBar_viewport_valueChanged() // update viewport
{
    viewport.x = ui->horizontalScrollBar_viewport->value(); // new value
    Render(); // update display
}

void MainWindow::on_verticalScrollBar_viewport_valueChanged()  // update viewport
{
    viewport.y = ui->verticalScrollBar_viewport->value(); // new value
    Render(); // update display
}

void MainWindow::ZoomPlus() // zoom in
{
    int z = 0;
    while (zoom >= zooms[z]) z++; // from lowest to highest value find the next one
    if (z == num_zooms) z--; // maximum

    if (zoom != zooms[z]) { // zoom changed ?
        QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
        qApp->processEvents();
        oldZoom = zoom;
        zoom = zooms[z]; // new zoom value
        ShowThumbnail();
        Render(); // update display
        QApplication::restoreOverrideCursor(); // Restore cursor
    }
}

void MainWindow::ZoomMinus() // zoom out
{
    int z = num_zooms;
    while (zoom <= zooms[z]) z--; // from highest to lowest value find the next one
    if (z == 0) z++; // minimum

    if (zoom != zooms[z]) { // zoom changed ?
        QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
        qApp->processEvents();
        oldZoom = zoom;
        zoom = zooms[z]; // new zoom value
        ShowThumbnail();
        Render(); // update display
        QApplication::restoreOverrideCursor(); // Restore cursor
    }
}

void MainWindow::on_pushButton_zoom_minus_clicked() // zoom out from button
{
    zoom_type = "button";
    ZoomMinus();
}

void MainWindow::on_pushButton_zoom_plus_clicked() // zoom in from button
{
    zoom_type = "button";
    ZoomPlus();
}

void MainWindow::on_pushButton_zoom_fit_clicked() // zoom fit from button
{
    zoom_type = "button";
    oldZoom = zoom;
    double zoomX = double(ui->label_viewport->width()) / image.cols; // find the best value of zoom
    double zoomY = double(ui->label_viewport->height()) / image.rows;
    if (zoomX < zoomY) zoom = zoomX; // less = fit view borders
        else zoom = zoomY;

    QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
    qApp->processEvents();
    Render(); // update display
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::on_pushButton_zoom_100_clicked() // zoom 100% from button
{
    zoom_type = "button";
    oldZoom = zoom;
    zoom = 1; // new zoom value
    QApplication::setOverrideCursor(Qt::SizeVerCursor); // zoom cursor
    qApp->processEvents();
    Render(); // update display
    QApplication::restoreOverrideCursor(); // Restore cursor
}

void MainWindow::ShowZoomValue() // display zoom percentage in ui
{
    ui->label_zoom->setText(QString::number(int(zoom * 100)) +"%"); // show new zoom value in percent
}

//// Gradients signals

void MainWindow::ShowGradient() // show gradient example from current label
{
    if (!loaded) // no need for that if nothing is loaded !
        return;

    Mat gradient= cv::Mat::zeros(ui->label_gradient->height(), ui->label_gradient->width(), CV_8UC1); // final image to display
    Mat1b msk = cv::Mat::zeros(ui->label_gradient->height(), ui->label_gradient->width(), CV_8UC1); // the gradient needs a mask
    msk = 255; // fill the mask with non-zero values

    int row = ui->listWidget_labels->currentRow(); // get current label

    cv::Point beginPoint, endPoint; // begin and end points will change with gradient example type

    switch (gradients[row].gradient) {
        case gradient_flat: {
            break;
        }
        case gradient_linear: {
            beginPoint = cv::Point(0, 0);
            endPoint = cv::Point(ui->label_gradient->width(), ui->label_gradient->height());
            break;
        }
        case gradient_doubleLinear: {
            beginPoint = cv::Point(ui->label_gradient->width() / 2, ui->label_gradient->height() / 2);
            endPoint = cv::Point(0, 0);
            break;
        }
        case gradient_radial: {
            beginPoint = cv::Point(ui->label_gradient->width() / 2, ui->label_gradient->height() / 2);
            endPoint = cv::Point(ui->label_gradient->width() / 2, -20);
            break;
        }
    }
    GradientFillGray(gradients[row].gradient, gradient, msk,
                     beginPoint, endPoint,
                     gradients[row].beginColor, gradients[row].endColor,
                     ui->listWidget_gradient_curve->currentRow()); // fill shape with gray gradient

    cvtColor(gradient, gradient, COLOR_GRAY2BGR);
    QPixmap D;
    D = Mat2QPixmap(gradient);
    ui->label_gradient->setPixmap(D); // show gradient example

    QString file;
    switch (ui->listWidget_gradient_curve->currentRow()) { // file in .qrc resource needed to display curve shape
        case curve_linear: {
            file = ":/icons/curve-linear.png";
            break;
        }
        case curve_cosinus: {
            file = ":/icons/curve-cosinus.png";
            break;
        }
        case curve_cosinus2: {
            file = ":/icons/curve-cosinus2.png";
            break;
        }
        case curve_power2: {
            file = ":/icons/curve-power2.png";
            break;
        }
        case curve_power3: {
            file = ":/icons/curve-power3.png";
            break;
        }
        case curve_sigmoid: {
            file = ":/icons/curve-sigmoid.png";
            break;
        }
        case curve_cos2power2: {
            file = ":/icons/curve-cos2power2.png";
            break;
        }
        case curve_cos2sqrt: {
            file = ":/icons/curve-cos2sqrt.png";
            break;
        }
        case curve_undulate: {
            file = ":/icons/curve-undulate.png";
            break;
        }
        case curve_undulate2: {
            file = ":/icons/curve-undulate2.png";
            break;
        }
        case curve_undulate3: {
            file = ":/icons/curve-undulate3.png";
            break;
        }
    }

    ui->label_curve_image->setPixmap(QPixmap::fromImage(QImage(file), Qt::AutoColor)); // show curve shape
}

void MainWindow::on_radioButton_flat_clicked() // flat gradient selected
{
    if (!loaded) // no need for that if file not loaded !
        return;

    if (ui->radioButton_flat->isChecked()) {
        BlockGradientsSignals(true);
        gradients[ui->listWidget_labels->currentRow()].gradient = gradient_flat; // change value in gradients array
        BlockGradientsSignals(false);
        ShowGradient(); // show gradient example
        ChangeLabelGradient(); // apply effect
    }  
}

void MainWindow::on_radioButton_linear_clicked() // linear gradient selected
{
    if (!loaded)
        return;

    if (ui->radioButton_linear->isChecked()) {
        BlockGradientsSignals(true);
        gradients[ui->listWidget_labels->currentRow()].gradient = gradient_linear;
        BlockGradientsSignals(false);
        ShowGradient();
        ChangeLabelGradient();
    }
}

void MainWindow::on_radioButton_double_linear_clicked() // bi-linear gradient selected
{
    if (!loaded)
        return;

    if (ui->radioButton_double_linear->isChecked()) {
        BlockGradientsSignals(true);
        gradients[ui->listWidget_labels->currentRow()].gradient = gradient_doubleLinear;
        BlockGradientsSignals(false);
        ShowGradient();
        ChangeLabelGradient();
    }
}

void MainWindow::on_radioButton_radial_clicked() // radial gradient selected
{
    if (!loaded)
        return;

    if (ui->radioButton_radial->isChecked()) {
        BlockGradientsSignals(true);
        gradients[ui->listWidget_labels->currentRow()].gradient = gradient_radial;
        BlockGradientsSignals(false);
        ShowGradient();
        ChangeLabelGradient();
    }
}

void MainWindow::on_horizontalSlider_begin_valueChanged(int value) // begin color for gradient changed
{
    if (!loaded) // not needed if file not loaded !
        return;

    BlockGradientsSignals(true);
    gradients[ui->listWidget_labels->currentRow()].beginColor = value; // change value in gradients array
    BlockGradientsSignals(false);

    ShowGradient(); // show gradient example

    ChangeLabelGradient(); // apply effect
}

void MainWindow::on_horizontalSlider_end_valueChanged(int value) // end color for gradient changed
{
    if (!loaded)
        return;

    BlockGradientsSignals(true);
    gradients[ui->listWidget_labels->currentRow()].endColor = value;
    BlockGradientsSignals(false);

    ShowGradient();

    ChangeLabelGradient();
}

void MainWindow::on_listWidget_gradient_curve_currentItemChanged(QListWidgetItem *currentItem) // gradient curve shape changed
{
    if (!loaded) // not needed if file not loaded !
        return;

    BlockGradientsSignals(true);
    gradients[ui->listWidget_labels->currentRow()].curve = curveType(ui->listWidget_gradient_curve->currentRow()); // change value in gradients array
    BlockGradientsSignals(false);

    ShowGradient(); // show gradient example

    ChangeLabelGradient(); // apply effect
}

//// 3D view

void MainWindow::on_checkBox_3d_realtime_clicked() // activate 3D view in real-time
{
    if (ui->checkBox_3d_realtime->isChecked()) {
        ui->button_3d_update->setEnabled(false); // deactivate 3D update button
        //ui->openGLWidget_3d->depthmap3D = depthmap; // update depthmap for 3D scene
        ui->openGLWidget_3d->computeVertices3D = true; // recompute 3D scene
        ui->openGLWidget_3d->update(); // view 3D scene
    }
    else {
        ui->button_3d_update->setEnabled(true); // 3D real-time deactivated = 3D update button available
    }
}

void MainWindow::on_button_3d_update_clicked() // manually update 3D scene
{
    //ui->openGLWidget_3d->depthmap3D = depthmap; // update depthmap for 3D scene
    ui->openGLWidget_3d->computeVertices3D = true;
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_horizontalSlider_depth3D_valueChanged(int value) // change depth value for 3D scene
{
    ui->openGLWidget_3d->depth3D = double(value) / 10; // change value
    ui->openGLWidget_3d->updateVertices3D = true; // recompute 3D scene
    ui->openGLWidget_3d->updateAllVertices3D = true; // do it for entire image
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_checkBox_3d_anaglyph_clicked() // activate or not anaglyph view
{
    ui->openGLWidget_3d->anaglyphEnabled = ui->checkBox_3d_anaglyph->isChecked(); // set value
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_comboBox_3d_tint_currentIndexChanged(int index) // change tint of image in 3D scene
{
    Mat image_temp = AnaglyphTint(image, ui->comboBox_3d_tint->currentIndex());
    ui->openGLWidget_3d->image3D = GammaCorrection(image_temp, ui->doubleSpinBox_gamma->value());

    ui->openGLWidget_3d->computeColors3D = true; // recompute 3D colors
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_doubleSpinBox_gamma_valueChanged(double value)
{
    on_comboBox_3d_tint_currentIndexChanged(0);
}

void MainWindow::on_checkBox_3d_axes_clicked() // show or not the 3D axes
{
    ui->openGLWidget_3d->axesEnabled = ui->checkBox_3d_axes->isChecked(); // set value
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_checkBox_3d_blur_clicked() // blur depthmap for 3D view
{
    if (ui->checkBox_3d_blur->isChecked()) { // blur activated
        Mat depthmap3D_temp;
        cv::GaussianBlur(depthmap, depthmap3D_temp,
                         Size(ui->horizontalSlider_blur_amount->value() * 2 + 1,
                              ui->horizontalSlider_blur_amount->value() * 2 + 1), 0, 0); // gaussian blur image
        ui->openGLWidget_3d->depthmap3D = depthmap3D_temp; // copy blurred depthmap
    }
    else // no blur
        ui->openGLWidget_3d->depthmap3D = depthmap; // copy original depthmap

    ui->openGLWidget_3d->updateVertices3D = true; // recompute 3D scene
    ui->openGLWidget_3d->updateAllVertices3D = true; // for all image
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_horizontalSlider_blur_amount_valueChanged(int value) // change blur amount for 3D scene
{
    on_checkBox_3d_blur_clicked(); // view 3D scene with blur
}

void MainWindow::on_checkBox_3d_light_clicked() // light on/off in 3D scene
{
    ui->openGLWidget_3d->lightEnabled = ui->checkBox_3d_light->isChecked(); // set value
    ui->openGLWidget_3d->update(); // view 3D scene
}

void MainWindow::on_horizontalSlider_anaglyph_shift_valueChanged(int value) // change distance between virtual eyes in 3D scene
{
    ui->openGLWidget_3d->anaglyphShift = double(-value) / 2; // set value
    if (ui->checkBox_3d_anaglyph->isChecked()) { // if anaglyph view activated
        ui->openGLWidget_3d->update(); // view 3D scene
    }
}

void MainWindow::on_checkBox_3d_fullscreen_clicked() // view 3D scene fullscreen, <ESC> to return from it
{
    saveXOpenGL = ui->openGLWidget_3d->x(); // save openGL widget position and size
    saveYOpenGL = ui->openGLWidget_3d->y();
    saveWidthOpenGL = ui->openGLWidget_3d->width();
    saveHeightOpenGL = ui->openGLWidget_3d->height();

    /*
    QRect screenSize = qApp->desktop()->availableGeometry(qApp->desktop()->primaryScreen()); // get screen size in which app is run
    int newW = screenSize.width();
    int newH = round(newW * (double(ui->openGLWidget_3d->width()) / ui->openGLWidget_3d->height())); // keep aspect ratio of widget using new width

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    show();
    ui->openGLWidget_3d->move(QPoint(0, 0)); // move widget to upper-left position in window
    ui->openGLWidget_3d->resize(QSize(newW, newH)); // resize openGL widget
    ui->openGLWidget_3d->raise(); // bring the 3d widget to front, above all other objects
    */

    ui->openGLWidget_3d->setWindowFlags(Qt::Window);
    ui->openGLWidget_3d->setContentsMargins(0, 0, 0, 0);
    ui->openGLWidget_3d->showFullScreen();
}

void MainWindow::on_button_3d_reset_clicked() // recenter position and reset zoom for 3D scene
{
    ui->openGLWidget_3d->zoom3D = 8; // zoom coefficient

    ui->openGLWidget_3d->SetXShift(0); // initial (x,y) position
    ui->openGLWidget_3d->SetYShift(0);
}

//// Capture 3D

void MainWindow::on_spinBox_3d_frames_valueChanged(int value) // change number of frames for 3D animation
{
    if (value == 1) { // unique view ?
        ui->checkBox_3d_capture_unique->setChecked(true); // check unique button
        ui->checkBox_3d_infinite_cycle->setChecked(false); // uncheck infinite button
        ui->checkBox_3d_double_cycle->setChecked(false); // uncheck double cycle button
    }
    else // not unique view
        ui->checkBox_3d_capture_unique->setChecked(false); // uncheck unique button
}

void MainWindow::on_checkBox_3d_capture_unique_clicked() // only one frame for 3D animation
{
    if (ui->checkBox_3d_capture_unique->isChecked()) { // unique ?
        ui->checkBox_3d_infinite_cycle->setChecked(false); // uncheck infinite button
        ui->spinBox_3d_frames->setValue(1); // only 1 frame
        ui->checkBox_3d_double_cycle->setChecked(false); // uncheck double cycle button
    }
    else { // not unique view
        ui->spinBox_3d_frames->setValue(12); // default to 12 frames
    }
}

void MainWindow::on_checkBox_3d_infinite_cycle_clicked() // activate infinite cycle for 3D animation
{
    if (ui->checkBox_3d_infinite_cycle->isChecked()) { // infinite cycle activated ?
        if (ui->checkBox_3d_capture_unique->isChecked()) { // incompatible with unique view
            ui->checkBox_3d_capture_unique->setChecked(false); // uncheck it
            ui->spinBox_3d_frames->setValue(12); // default to 12 frames
        }
    }
}

void MainWindow::on_checkBox_3d_capture_fullscreen_clicked() // activate fullscreen view for 3D animation
{
    if (ui->checkBox_3d_capture_fullscreen->isChecked()) { // fullscreen ?
        QRect screenSize = qApp->desktop()->availableGeometry(qApp->desktop()->primaryScreen()); // get screen size in which app is run
        ui->spinBox_3d_resolution->setValue(screenSize.width()); // set screen size width to 3D resolution
    }
    else // not fullscreen
        ui->spinBox_3d_resolution->setValue(ui->openGLWidget_3d->width()); // width = widget width
}

void MainWindow::on_checkBox_3d_double_cycle_clicked() // activate to-and-from cycle for 3D animation
{
    if (ui->checkBox_3d_double_cycle->isChecked()) {
        if (ui->checkBox_3d_capture_unique->isChecked()) { // incompatible with unique view
            ui->checkBox_3d_capture_unique->setChecked(false); // uncheck it
            ui->spinBox_3d_frames->setValue(12); // default to 12 frames
        }
    }
}

void MainWindow::on_checkBox_3d_capture_clicked() // launch 3D animation and save images if option is set
{
    if (!ui->checkBox_3d_capture->isChecked()) { // animation can be interrupted by the same button which launched it
        abort_3d = true; // set stop flag and exit
        return;
    }

    std::string filesession;
    if (ui->checkBox_3d_save_files->isChecked()) { // save to image option activated ?
        QString filename = QFileDialog::getSaveFileName(this, "Save 3D capture to images...", "./" + QString::fromStdString(basedir + basefile + "-capture.png"), "*.png *.PNG"); // get filename

        if (filename.isNull() || filename.isEmpty()) // cancel ?
            return;

        filesession = filename.toUtf8().constData(); // base file name
        size_t pos = filesession.find(".png");
        if (pos != std::string::npos) filesession.erase(pos, filesession.length()); // delete extension
    }

    QApplication::setOverrideCursor(Qt::WaitCursor); // wait cursor
    qApp->processEvents();
    ui->frame_3D_capture->setEnabled(false); // disable capture panel while animating

    int w = ui->openGLWidget_3d->width(); // save openGL widget current values
    int h = ui->openGLWidget_3d->height();
    int rX = ui->openGLWidget_3d->xRot;
    int rY = ui->openGLWidget_3d->yRot;
    int rZ = ui->openGLWidget_3d->zRot;
    saveXOpenGL = ui->openGLWidget_3d->x(); // save openGL widget current position and size
    saveYOpenGL = ui->openGLWidget_3d->y();
    saveWidthOpenGL = ui->openGLWidget_3d->width();
    saveHeightOpenGL = ui->openGLWidget_3d->height();

    if (ui->checkBox_3d_capture_fullscreen->isChecked()) { // fullscreen ?
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        show();
        ui->openGLWidget_3d->move(QPoint(0, 0)); // move widget to position (0,0)
        ui->openGLWidget_3d->raise(); // bring the 3d widget to front, above all other objects
    }
    int newW = ui->spinBox_3d_resolution->value(); // width chosen by user
    int newH = round(double(newW) / (double(w) / h)); // ratio of widget rectangle
    ui->openGLWidget_3d->resize(newW, newH); // set new size to widget

    // init values;
    int nb_frames = ui->spinBox_3d_frames->value() - 1; // number of frames -1 to start from 0
    //bool circular = ui->checkBox_3d_circular->isChecked(); // circular ?
    double middleX = double(ui->spinBox_3d_x_angle->value()) / 2; // halves of angles
    double middleY = double(ui->spinBox_3d_y_angle->value()) / 2;
    double xSector, ySector;
        xSector = double(ui->spinBox_3d_x_angle->value()) / (nb_frames); // angle value of one sector
        ySector = double(ui->spinBox_3d_y_angle->value()) / (nb_frames);
    int begin; // 1st frame value
    if (ui->checkBox_3d_double_cycle->isChecked()) // 1st frame value depends on mode (circular, double cycle)
        begin = -nb_frames;
            else begin = 0;
    abort_3d = false; // used to interrupt capture
    int counter = 1; // used to save images only once if infinite cycle set
    int progress = 1; // for progress bar
    // progress bar init
    if (ui->checkBox_3d_double_cycle->isChecked()) // double cycle ?
        ui->progressBar_3d_capture->setMaximum(nb_frames * 2); // max of progress bar is twice the amount
    else // not double cycle
        if (!ui->checkBox_3d_capture_unique->isChecked()) // more than 1 frame ?
            ui->progressBar_3d_capture->setMaximum(1); // max of progress bar is the number of frames
                else ui->progressBar_3d_capture->setMaximum(1); // else max of progress bar is 1 (setting it to 0 makes the progress bar shuffle)
    ui->progressBar_3d_capture->setValue(0); // 0% done in progress bar

    // compute frames
    if (ui->checkBox_3d_capture_unique->isChecked()) { // unique = capture "as is"
        ui->openGLWidget_3d->update(); // view 3D scene
        ui->openGLWidget_3d->Capture(); // capture it

        // display capture in GUI
        Mat capture = QImage2Mat(ui->openGLWidget_3d->capture3D); // convert capture from QImage

        // write image file if needed
        if (ui->checkBox_3d_save_files->isChecked()) { // save to file enabled
            imwrite(filesession + ".png", capture); // save capture to PNG image
        }

        ui->progressBar_3d_capture->setValue(1); // update progress bar

        qApp->processEvents(); // force refresh of GUI
    }
    else
        for (int frame = begin; frame <= nb_frames; frame++) { // for each frame
            if (abort_3d) { // abort ?
                //QMessageBox::critical(this, "3D capture", "Capture was interrupted");
                ui->progressBar_3d_capture->setValue(0); // reset progress bar and exit
                break;
            }
            double xAngle, yAngle;
            if (ui->checkBox_3d_circular_vertical->isChecked()) {
                int frm;
                if (frame < 0)
                    frm = -frame;
                else
                    frm = frame;
                if (frm == 0) // compute current angles of view on x and y axes for each interval
                    xAngle = -middleX;
                else if (frm < double(nb_frames) / 4) // 1st quarter
                    xAngle = xSector * double(frm) * 2 - middleX;
                else if (frm < double(nb_frames) / 2) // 2nd quarter
                    xAngle = xSector * double(frm) * 2 - middleX;
                else if (frm < double(nb_frames) * 3 / 4) // 3rd quarter
                    xAngle = middleX - xSector * (double(frm) - double(nb_frames) / 2) * 2;
                else // 4th quarter
                    xAngle = middleX - xSector * (double(frm) - double(nb_frames) / 2) * 2;
            }
            if (ui->checkBox_3d_circular_horizontal->isChecked()) {
                int frm;
                if (frame < 0)
                    frm = nb_frames + frame;
                else
                    frm = frame;
                if (frm == 0) // compute current angles of view on x and y axes for each interval
                    yAngle = 0;
                else if (frm < double(nb_frames) / 4) // 1st quarter
                    yAngle = ySector * double(frm) * 2;
                else if (frm < double(nb_frames) / 2) // 2nd quarter
                    yAngle = middleY - ySector * (double(frm) - double(nb_frames) / 4) * 2;
                else if (frm < double(nb_frames) * 3 / 4) // 3rd quarter
                    yAngle = middleY - ySector * (double(frm) - double(nb_frames) / 4) * 2;
                else // 4th quarter
                    yAngle = ySector * (double(frm) - double(nb_frames) * 3 / 4) * 2 - middleY;
            }
            if (!ui->checkBox_3d_circular_vertical->isChecked()) { // vertical wobble
                xAngle = xSector * double(frame) - middleX; // compute angles
                if (frame < 0) // in double cycle mode angle is not the same for "negative" frames
                    xAngle = double(-ui->spinBox_3d_x_angle->value()) - xAngle;
                if (ui->checkBox_3d_double_cycle->isChecked()) // in double cycle mode invert angles
                    xAngle = -xAngle;
            }
            if (!ui->checkBox_3d_circular_horizontal->isChecked()) { // horizontal wobble
                yAngle = ySector * double(frame) - middleY;
                if (frame < 0) // in double cycle mode angle is not the same for "negative" frames
                    yAngle = double(-ui->spinBox_3d_y_angle->value()) - yAngle;
                if (ui->checkBox_3d_double_cycle->isChecked()) // in double cycle mode invert angles
                    yAngle = -yAngle;
            }

            ui->openGLWidget_3d->xRot = xAngle; // set angles in 3D scene
            ui->openGLWidget_3d->yRot = yAngle;

            ui->openGLWidget_3d->update(); // view 3D scene
            ui->openGLWidget_3d->Capture(); // capture it

            // display capture in GUI
            Mat capture = QImage2Mat(ui->openGLWidget_3d->capture3D); // convert capture from QImage

            // write image file if needed
            if (ui->checkBox_3d_save_files->isChecked() & (counter == 1)) { // save to file enabled and still in 1st animation ?
                std::stringstream str_number;
                str_number << std::setw(3) << std::setfill('0') << progress; // add 0s to file number
                imwrite(filesession + "-" + str_number.str() + ".png", capture); // save capture to PNG image
            }

            ui->progressBar_3d_capture->setValue(progress); // update progress bar

            qApp->processEvents(); // force refresh of GUI

            if ((ui->checkBox_3d_infinite_cycle->isChecked()) & (frame == nb_frames)) { // infinite cycle and end of animation reached ?
                if (ui->checkBox_3d_double_cycle->isChecked())
                    frame = -frame; // return to 1st frame
                else
                    frame = 0;
                counter++; // end of animation reached = no more writing image to file
            }

            progress++; // increase value for progress bar - the 1st time progress is already equal to 1 that's why it is increased after updating the progress bar
            if (progress > 10000) // if infinite loop don't go too far
                progress = nb_frames + 1; // reset progress value to just over 100%
        }

    ui->openGLWidget_3d->xRot = rX; // restore initial 3D scene values
    ui->openGLWidget_3d->yRot = rY;
    ui->openGLWidget_3d->zRot = rZ;

    if (ui->checkBox_3d_capture_fullscreen->isChecked()) // full screen ?
        this->setWindowFlags((((windowFlags() | Qt::CustomizeWindowHint)
                                & ~Qt::WindowCloseButtonHint) | Qt::WindowMinMaxButtonsHint));
        show();
        ui->openGLWidget_3d->move(QPoint(saveXOpenGL, saveYOpenGL)); // move back openGL widget
        ui->spinBox_3D_rotate_x->raise(); // bring back controls over 3D view
        ui->spinBox_3D_rotate_y->raise();
        ui->horizontalSlider_3D_rotate_y->raise();
        ui->verticalSlider_3D_rotate_x->raise();
        ui->button_3d_reset->raise();
    ui->openGLWidget_3d->resize(saveWidthOpenGL, saveHeightOpenGL); // and resize it
    if ((ui->spinBox_3d_resolution->value() == ui->openGLWidget_3d->width()) & (!ui->checkBox_3d_capture_fullscreen->isChecked()))
            ui->openGLWidget_3d->update(); // force update of 3D scene if width of widget hasn't changed

    ui->frame_3D_capture->setEnabled(true); // activate capture panel
    ui->checkBox_3d_capture->setChecked(false); // set capture button to initial state
    QApplication::restoreOverrideCursor(); // Restore cursor
}

///////////////////  Keyboard events  //////////////////////

void MainWindow::keyReleaseEvent(QKeyEvent *keyEvent) // spacebar = move view in viewport
{
    if ((keyEvent->key() == Qt::Key_Space) & (loaded)) { // spacebar ?
        QPoint mouse_pos = ui->label_viewport->mapFromGlobal(QCursor::pos()); // current mouse position

        int decX = mouse_pos.x() - mouse_origin.x(); // distance from last mouse position
        int decY = mouse_pos.y() - mouse_origin.y();

        SetViewportXY(viewport.x - double(decX) / zoom, viewport.y - double(decY) / zoom); // update viewport

        ShowThumbnail(); // show thumbnail
        Render(); // update view

        QApplication::restoreOverrideCursor(); // Restore cursor
    }
    else {
        if (keyEvent->key() == Qt::Key_Left)
            ui->openGLWidget_3d->SetShiftLeft();
        if (keyEvent->key() == Qt::Key_Right)
            ui->openGLWidget_3d->SetShiftRight();
        if (keyEvent->key() == Qt::Key_Up)
            ui->openGLWidget_3d->SetShiftUp();
        if (keyEvent->key() == Qt::Key_Down)
            ui->openGLWidget_3d->SetShiftDown();
        if (keyEvent->key() == Qt::Key_PageUp)
            ui->openGLWidget_3d->SetAngleUp();
        if (keyEvent->key() == Qt::Key_PageDown)
            ui->openGLWidget_3d->SetAngleDown();
        if (keyEvent->key() == Qt::Key_Home)
            ui->openGLWidget_3d->SetAngleLeft();
        if (keyEvent->key() == Qt::Key_End)
            ui->openGLWidget_3d->SetAngleRight();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *keyEvent) // special keys
{
    if ((keyEvent->key() == Qt::Key_Space) & (ui->label_viewport->underMouse()) & (loaded)) { // spacebar = move view
        mouse_origin = ui->label_viewport->mapFromGlobal(QCursor::pos()); // mouse position reference for keyReleaseEvent

        QApplication::setOverrideCursor(Qt::SizeAllCursor); // show "move" cursor
        qApp->processEvents();
    }
    else if ((keyEvent->key() == Qt::Key_Escape) & (ui->checkBox_3d_fullscreen->isChecked())) { // get out of fullscreen view
        //this->setWindowFlags((((windowFlags() | Qt::CustomizeWindowHint)
        //                        & ~Qt::WindowCloseButtonHint) | Qt::WindowMinMaxButtonsHint));
        //show();

        ui->openGLWidget_3d->setWindowFlags(Qt::SubWindow);
        ui->openGLWidget_3d->showNormal();

        ui->openGLWidget_3d->move(QPoint(saveXOpenGL, saveYOpenGL)); // restore openGL widget to its previous position
        ui->openGLWidget_3d->resize(QSize(saveWidthOpenGL, saveHeightOpenGL)); // ... and size
        ui->checkBox_3d_fullscreen->setChecked(false); // uncheck fullscreen button
        ui->spinBox_3D_rotate_x->raise(); // bring back controls over 3D view
        ui->spinBox_3D_rotate_y->raise();
        ui->horizontalSlider_3D_rotate_y->raise();
        ui->verticalSlider_3D_rotate_x->raise();
        ui->button_3d_reset->raise();
    }
    else if ((keyEvent->key() == Qt::Key_Escape) & (ui->checkBox_3d_capture->isChecked()) & (!abort_3d)) { // abort 3D capture
        abort_3d = true;
    }
}

/////////////////// Mouse events //////////////////////

void MainWindow::mouseReleaseEvent(QMouseEvent *eventRelease) // event triggered by releasing mouse button
{
    QApplication::restoreOverrideCursor(); // Restore cursor

    if (moveBegin) { // move origin of label vector
        moveBegin = false; // stop moving
        updateVertices3D = true; // recompute 3D
        Render(); // show result
    }
    else if (moveEnd) { // same as above but for head of label vector
        moveEnd = false;
        updateVertices3D = true;
        Render();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *eventPress) // event triggered by a mouse click
{
    if (!loaded) return; // no image = get out

    if (ui->label_viewport->underMouse()) { // mouse over viewport ?
        bool key_control = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier); // modifier keys pressed ? (shift control etc)
        bool key_alt = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);

        mouse_origin = ui->label_viewport->mapFromGlobal(QCursor::pos()); // current mouse position, save it
        cv::Point pos = Viewport2Image(cv::Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouseButton = eventPress->button(); // mouse button value

        if (mouseButton == Qt::RightButton) { // move view = right button
            QApplication::setOverrideCursor(Qt::SizeAllCursor); // show "move" cursor
            qApp->processEvents();
            return;
        }
        if ((mouseButton == Qt::MiddleButton) & (pos.x >= 0) & (pos.x < image.cols)
                                            & (pos.y >= 0) & (pos.y < image.rows)) { // right button = get gray value
            uchar color = depthmap.at<uchar>(pos.y, pos.x); // get gray "color" under mouse cursor
            ui->label_gray_level->setText(QString::number(color)); // show it
            return;
        }
        else if (loaded) {
            int row = ui->listWidget_labels->currentRow(); // get current label number in list
            if (mouseButton == Qt::LeftButton) { // left mouse button ?
                int size;
                if (zoom < 1)
                    size = 1.0 / zoom * 6;
                else
                    size = zoom * 6;
                if ((pos.x >= gradients[row].endPoint.x - size - 1) & (pos.y >= gradients[row].endPoint.y - size - 1)
                         & (pos.x <= gradients[row].endPoint.x + size + 1) & (pos.y <= gradients[row].endPoint.y + size + 1)) { // mouse cursor over end point
                     moveEnd = true; // begin moving it
                }
                else if ((pos.x >= gradients[row].beginPoint.x - size - 1) & (pos.y >= gradients[row].beginPoint.y - size - 1)
                      & (pos.x <= gradients[row].beginPoint.x + size + 1) & (pos.y <= gradients[row].beginPoint.y + size + 1)) { // mouse cursor over begin point
                    moveBegin = true; // begin moving it

                }
                else if ((pos.x >= 0) & (pos.x < image.cols)
                         & (pos.y >= 0) & (pos.y < image.rows)) { // select label in viewport
                    int value = labels.at<int>(pos.y, pos.x); // get label mask value under mouse cursor
                    for (int i = 0; i < ui->listWidget_labels->count(); i++) { // find clicked label id
                        QListWidgetItem *item = ui->listWidget_labels->item(i); // pointer on item
                        if (item->data(Qt::UserRole).toInt() == value) { // is it the right one ?
                            ui->listWidget_labels->setCurrentRow(i); // yes, select label in list, it will trigger actions
                            break; // fount it, exit
                        }
                    }
                }
        }
        }
    }
    else if (ui->label_thumbnail->underMouse()) { // if mouse over thumbnail
        QPoint mouse_pos = ui->label_thumbnail->mapFromGlobal(QCursor::pos()); // current mouse position
        mouseButton = eventPress->button(); // mouse button value

        if (mouseButton == Qt::LeftButton) { // move view ?
            // convert middle of viewport from thumbnail to image coordinates
            int middleX = double(mouse_pos.x() - (ui->label_thumbnail->width() - ui->label_thumbnail->pixmap()->width()) / 2) / ui->label_thumbnail->pixmap()->width() * image.cols;
            int middleY = double(mouse_pos.y() - (ui->label_thumbnail->height() - ui->label_thumbnail->pixmap()->height()) / 2) / ui->label_thumbnail->pixmap()->height() * image.rows;
            SetViewportXY(middleX - viewport.width /2, middleY - viewport.height /2); // set viewport to new middle position

            ShowThumbnail(); // show thumbnail
            Render(); // show result
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *eventMove) // first mouse click has already been treated and is holded down
{
    if (!loaded) return;// no image = get out

    if (ui->label_thumbnail->underMouse()) { // mouse over thumbnail ?
        QPoint mouse_pos = ui->label_thumbnail->mapFromGlobal(QCursor::pos()); // current mouse position

        if (mouseButton == Qt::LeftButton) { // move view
            // convert middle of viewport from thumbnail to image coordinates
            int middleX = double(mouse_pos.x() - (ui->label_thumbnail->width() - ui->label_thumbnail->pixmap()->width()) / 2) / ui->label_thumbnail->pixmap()->width() * image.cols;
            int middleY = double(mouse_pos.y() - (ui->label_thumbnail->height() - ui->label_thumbnail->pixmap()->height()) / 2) / ui->label_thumbnail->pixmap()->height() * image.rows;
            SetViewportXY(middleX - viewport.width /2, middleY - viewport.height /2); // set viewport to new middle position

            ShowThumbnail(); // show thumbnail
            Render(); // display result
            return;
        }
    }
    else if (ui->label_viewport->underMouse()) { // if mouse over viewport
        QPoint mouse_pos = ui->label_viewport->mapFromGlobal(QCursor::pos()); // current mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_pos.x(), mouse_pos.y())); // convert from viewport to image coordinates

        if (pos.x < 1) pos.x = 1; // mouse coordinates can't be outside image
        if (pos.y < 1) pos.y = 1;
        if (pos.x > image.cols-2) pos.x = image.cols - 2;
        if (pos.y > image.rows-2) pos.y = image.rows - 2;

        if (moveBegin) { // move base of label vector
            int row = ui->listWidget_labels->currentRow(); // get current label
            gradients[row].beginPoint = pos; // set new pos in gradients array
            ChangeLabelGradient(); // change gradient in depthmap
        }
        else if (moveEnd) { // same comment as before, except this is for head of label vector
            int row = ui->listWidget_labels->currentRow();
            gradients[row].endPoint = pos;
            ChangeLabelGradient();
        }
        else if ((mouseButton == Qt::MiddleButton) & (pos.x >= 0) & (pos.x < image.cols)
                                                  & (pos.y >= 0) & (pos.y < image.rows)) { // show gray "color" under mouse cursor
            uchar color = depthmap.at<uchar>(pos.y, pos.x); // get color in labels mask
            ui->label_gray_level->setText(QString::number(color)); // show it
            return;
        }
        else if (mouseButton == Qt::RightButton) { // move view
            int decX = mouse_pos.x() - mouse_origin.x(); // distance from last mouse position
            int decY = mouse_pos.y() - mouse_origin.y();

            SetViewportXY(viewport.x - double(decX) / zoom, viewport.y - double(decY) / zoom); // update viewport

            ShowThumbnail(); // show thumbnail
            Render(); // display result

            mouse_origin = mouse_pos; // save mouse position
        }
    }
}

void MainWindow::wheelEvent(QWheelEvent *wheelEvent) // mouse wheel turned
{
    if (!loaded)
        return;// no image = get out

    if (ui->label_viewport->underMouse()) { // if mouse over viewport
        mouse_origin = ui->label_viewport->mapFromGlobal(QCursor::pos()); // mouse position
        cv::Point pos = Viewport2Image(cv::Point(mouse_origin.x(), mouse_origin.y())); // convert from viewport to image coordinates
        mouse_origin = QPoint(pos.x, pos.y); // zoom from mouse position
        int n = wheelEvent->delta(); // amount of wheel turn
        if (n < 0) { // positive = zoom out
            zoom_type = "wheel"; // to zoom under mouse cursor
            ZoomMinus(); // do it
        }
        if (n > 0) { // negative = zoom in
            zoom_type = "wheel";
            ZoomPlus(); // do it
        }
    }
}

/////////////////// Core functions //////////////////////

cv::Point MainWindow::Viewport2Image(const cv::Point &p) // convert viewport to image coordinates
{
    int posX = double(p.x - (ui->label_viewport->width() - ui->label_viewport->pixmap()->width()) / 2) / zoom + viewport.x;
    int posY = double(p.y - (ui->label_viewport->height() - ui->label_viewport->pixmap()->height()) / 2) / zoom + viewport.y;
    return cv::Point(posX, posY);
}

void MainWindow::ShowThumbnail() // image thumnail with transparent viewport indicator
{
    if ((viewport.width == image.cols) & (viewport.height == image.rows)) { // entire view = no rectangle to draw
        ui->label_thumbnail->setPixmap(Mat2QPixmap(thumbnail)); // update view
        return;
    }

    double p1x, p1y, p2x, p2y; // rectangle position
    if (viewport.width == image.cols) { // entire horizontal view ?
        p1x = 0;
        p2x = ui->label_thumbnail->pixmap()->width();
    }
    else { // partial view
        p1x = double(viewport.x) / image.cols;
        p2x = p1x + double(viewport.width) / image.cols;
        p1x *= double(ui->label_thumbnail->pixmap()->width());
        p2x *= double(ui->label_thumbnail->pixmap()->width());
    }

    if (viewport.height == image.rows) { // entire vertical view ?
        p1y = 0;
        p2y= ui->label_thumbnail->pixmap()->height();
    }
    else { // partial view
        p1y = double(viewport.y) / image.rows;
        p2y = p1y + double(viewport.height) / image.rows;
        p1y *= double(ui->label_thumbnail->pixmap()->height());
        p2y *= double(ui->label_thumbnail->pixmap()->height());
    }

    Mat thumbnail_temp = Mat::zeros(thumbnail.rows, thumbnail.cols, CV_8UC3); // create thumbnail mask
    rectangle(thumbnail_temp, cv::Point(p1x, p1y), cv::Point(p2x-1, p2y-1), Scalar(92,92,92), -1); // draw filled rectangle representing the viewport position
    rectangle(thumbnail_temp, cv::Point(p1x, p1y), cv::Point(p2x-1, p2y-1), Scalar(255,255,255), 2); // draw rectangle with thick border
    cv::addWeighted(thumbnail, 1, thumbnail_temp, 0.25, 0, thumbnail_temp, -1); // add to thumbnail with transparency
    ui->label_thumbnail->setPixmap(Mat2QPixmap(thumbnail_temp)); // update thumbnail view
}

void MainWindow::UpdateViewportDimensions() // recompute viewport width and height
{
    viewport.width = double(ui->label_viewport->width()) / zoom; // new width
    if (viewport.width > image.cols) { // can't be out of image on the left
        viewport.width = image.cols;
        viewport.x = 0;
    }
    viewport.height = double(ui->label_viewport->height()) / zoom; // new height
    if (viewport.height > image.rows) { // can't be out of image on top
        viewport.height = image.rows;
        viewport.y = 0;
    }
    if (viewport.x > (image.cols - viewport.width)) viewport.x = image.cols - viewport.width; // can't be out of image at right-bottom
    if (viewport.y > (image.rows - viewport.height)) viewport.y = image.rows - viewport.height;
}

void MainWindow::SetViewportXY(const int &x, const int &y) // set viewport top-left to (x,y) new coordinates
{
    viewport.x = x;
    viewport.y = y;
    if (viewport.x < 0) viewport.x = 0; // can't be out of image
    if (viewport.x > image.cols - viewport.width) viewport.x = image.cols - viewport.width;
    if (viewport.y < 0) viewport.y = 0;
    if (viewport.y > image.rows - viewport.height) viewport.y = image.rows - viewport.height;

    ui->horizontalScrollBar_viewport->blockSignals(true); // horizontal scrollbars must not trigger any action
    ui->horizontalScrollBar_viewport->setValue(viewport.x); // new x value
    ui->horizontalScrollBar_viewport->blockSignals(false); // return to normal state
    ui->verticalScrollBar_viewport->blockSignals(true); // vertical scrollbar must not trigger any action
    ui->verticalScrollBar_viewport->setValue(viewport.y); // new y value
    ui->verticalScrollBar_viewport->blockSignals(false); // return to normal state
}

void MainWindow::Render() // show masks for image + depthmap + selection
{
    if (!loaded) // no image to display
        return;

    int oldMiddleX, oldMiddleY;
    if (oldZoom != zoom) { // zoom changed ?
        oldMiddleX = viewport.x + viewport.width / 2; // current middle of viewport for zooming to center of image
        oldMiddleY = viewport.y + viewport.height / 2;
        if (zoom_type == "wheel") { // if zoomed with mouse wheel = set origin of zoom under mouse cursor
            oldMiddleX = mouse_origin.x() - oldZoom * (mouse_origin.x() - viewport.x) / zoom + double(ui->label_viewport->width()) / zoom / 2;
            oldMiddleY = mouse_origin.y() - oldZoom * (mouse_origin.y() - viewport.y) / zoom + double(ui->label_viewport->height()) / zoom / 2;
            zoom_type = "";
        }
        UpdateViewportDimensions(); // recompute viewport width and height
        double newPosX = oldMiddleX - viewport.width / 2; // compute new middle of viewport
        double newPosY = oldMiddleY - viewport.height / 2;
        ui->horizontalScrollBar_viewport->setMaximum(image.cols-viewport.width); // update scrollbars range
        ui->verticalScrollBar_viewport->setMaximum(image.rows-viewport.height);
        SetViewportXY(newPosX, newPosY); // set top-left coordinates of viewport to new value
        ShowZoomValue(); // display new zoom value
        oldZoom = zoom; // backup zoom value
    }

    Mat image_temp = CopyFromImage(image, viewport); // copy only zoomed part of image
    disp_color = Mat::zeros(image_temp.rows, image_temp.cols, CV_8UC3); // empty new view
    if (ui->checkBox_image->isChecked()) // image mask with transparency
        cv::addWeighted(disp_color, 1-double(ui->horizontalSlider_blend_image->value()) / 100,
                        image_temp, double(ui->horizontalSlider_blend_image->value()) / 100,
                        0, disp_color, -1);
    if (ui->checkBox_depthmap->isChecked() & (!depthmap.empty())) // depthmap with transparency
        cv::addWeighted(disp_color, 1,
                        CopyFromImage(depthmap, viewport), double(ui->horizontalSlider_blend_depthmap->value()) / 100,
                        0, disp_color, -1);
    if ((!selection.empty()) & (loaded)) { // something selected ?
        Mat selection_temp;
        selection.copyTo(selection_temp); // make a copy of selection mask

        int row = ui->listWidget_labels->currentRow(); // get current label row in list

        // draw end point : a blue tiny rectangle crossed by diagonals
        int size;
        if (zoom < 1)
            size = 1.0 / zoom * 6;
        else
            size = zoom * 6;
        cv::rectangle(selection_temp, Rect(gradients[row].endPoint.x - size, gradients[row].endPoint.y - size, size * 2 + 1, size * 2 + 1),
                      Vec3b(255, 0, 0), 2);
        cv::line(selection_temp, cv::Point(gradients[row].endPoint.x - size, gradients[row].endPoint.y - size),
                              cv::Point(gradients[row].endPoint.x + size, gradients[row].endPoint.y + size),
                              Vec3b(255, 0, 0), 2);
        cv::line(selection_temp, cv::Point(gradients[row].endPoint.x - size, gradients[row].endPoint.y + size),
                              cv::Point(gradients[row].endPoint.x + size, gradients[row].endPoint.y - size),
                              Vec3b(255, 0, 0), 2);
        // draw begin point : a red tiny rectangle crossed by diagonals
        cv::rectangle(selection_temp, Rect(gradients[row].beginPoint.x - size, gradients[row].beginPoint.y - size, size * 2 + 1, size * 2 + 1),
                      Vec3b(0, 0, 255), 2);
        cv::line(selection_temp, cv::Point(gradients[row].beginPoint.x - size, gradients[row].beginPoint.y - size),
                              cv::Point(gradients[row].beginPoint.x + size, gradients[row].beginPoint.y + size),
                              Vec3b(0, 0, 255), 2);
        cv::line(selection_temp, cv::Point(gradients[row].beginPoint.x - size, gradients[row].beginPoint.y + size),
                              cv::Point(gradients[row].beginPoint.x + size, gradients[row].beginPoint.y - size),
                              Vec3b(0, 0, 255), 2);
        Vec3b CO; // color of vector
        if ((moveBegin) | (moveEnd)) // is it being moved ?
            CO = Vec3b(0, 255, 0); // yes = green
        else
            CO = Vec3b(255, 255, 255); // no = white
        cv::arrowedLine(selection_temp, gradients[row].beginPoint, gradients[row].endPoint, CO, 1, 8, 0, 0.2); // draw vector

        selection_temp = CopyFromImage(selection_temp, viewport); // only keep the view part of selection mask
        if (zoom <= 1) selection_temp = DilatePixels(selection_temp, int(1/zoom)); // dilation depends of zoom
            else selection_temp = DilatePixels(selection_temp, 3-zoom);
        /*cv::addWeighted(disp_color, 1,
                        selection_temp, 0.25,
                        0, disp_color, -1);*/
        selection_temp.copyTo(disp_color, selection_temp); // copy selection mask over the view
    }

    // display viewport
    QPixmap D;
    D = Mat2QPixmapResized(disp_color, int(viewport.width*zoom), int(viewport.height*zoom), (zoom < 1)); // zoomed image
    ui->label_viewport->setPixmap(D); // set new image content to viewport

    if ((computeVertices3D | updateVertices3D | computeColors3D) & (ui->checkBox_3d_realtime->isChecked())) { // need to update 3D view ?
        if (updateVertices3D) {
            updateVertices3D = false;
            //ui->openGLWidget_3d->area3D = selection_rect;
            //ui->openGLWidget_3d->mask3D = currentLabelMask;
            ui->openGLWidget_3d->updateVertices3D = true; // 3D scene vertices must change
        }
        if (computeVertices3D) {
            computeVertices3D = false;
            ui->openGLWidget_3d->computeVertices3D = true; // recompute vertices in 3D scene
        }
        if (computeColors3D) {
            computeColors3D = false;
            ui->openGLWidget_3d->computeColors3D = true; // recompute colors in 3D scene
        }
        ui->openGLWidget_3d->update(); // view 3D scene
        qApp->processEvents(); // force refresh of GUI
    }
}

void MainWindow::ChangeLabelGradient() // update depthmap mask with gradient
{
    int row = ui->listWidget_labels->currentRow(); // get current label row in list

    GradientFillGray(gradients[row].gradient, depthmap, currentLabelMask,
                     gradients[row].beginPoint, gradients[row].endPoint,
                     gradients[row].beginColor, gradients[row].endColor,
                     gradients[row].curve, selection_rect); // fill depthmap mask with gradient

    ui->openGLWidget_3d->depthmap3D = depthmap;
    updateVertices3D = true;
    Render(); // update view
}

// https://stackoverflow.com/questions/74690/how-do-i-store-the-window-size-between-sessions-in-qt
void MainWindow::writePositionSettings()
{
    QSettings qsettings;

    qsettings.beginGroup("mainwindow");

    qsettings.setValue("geometry", saveGeometry());
    qsettings.setValue("savestate", saveState());
    qsettings.setValue("maximized", isMaximized());
    if (!isMaximized()) {
        qsettings.setValue("pos", pos());
        qsettings.setValue("size", size());
    }

    qsettings.endGroup();
}

void MainWindow::readPositionSettings()
{
    QSettings qsettings;

    qsettings.beginGroup("mainwindow");

    restoreGeometry(qsettings.value("geometry", saveGeometry()).toByteArray());
    restoreState(qsettings.value("savestate", saveState()).toByteArray());
    move(qsettings.value("pos", pos()).toPoint());
    resize(qsettings.value("size", size()).toSize());
    if (qsettings.value("maximized", true).toBool())
        showMaximized();

    qsettings.endGroup();
}

void MainWindow::moveEvent(QMoveEvent*)
{
    writePositionSettings();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (!ui->checkBox_3d_fullscreen->isChecked()) {
        const int w = width() - ui->openGLWidget_3d->pos().x();
        const int h = height() - ui->openGLWidget_3d->pos().y();
        if (w > 0 && h > 0)
        {
            ui->openGLWidget_3d->setFixedSize(w, h);
        }
    }
    writePositionSettings();
}
