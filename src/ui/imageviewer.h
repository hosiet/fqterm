/***************************************************************************
 *   fqterm, a terminal emulator for both BBS and *nix.                    *
 *   Copyright (C) 2008 fqterm development group.                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.               *
 ***************************************************************************/

#ifndef IMAGE_VIERWER_H
#define IMAGE_VIERWER_H

#include <QDesktopWidget>
#include <QDirModel>
#include <QLabel>
#include <QPainter>
#include <QPixmapCache>
#include <QScrollArea>
#include <QStatusBar>
#include <QToolButton>
#include <QToolBar>
#include <QLayout>
#include <QItemDelegate>
#include <QItemSelection>

class QString;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QMenuBar;
class QTreeView;
class QComboBox;

namespace FQTerm {

class FQTermConfig;
class FQTermFileDialog;
class PictureFlow;
class PictureView;
class ImageDisplay;


#define DISP_MARGIN				(10.0) // show a margin
#define PIXMAP_CACHESIZE		(5120) // in KBytes
#define ZOOMIN_FACTOR			(1.15)
#define ZOOMOUT_FACTOR			(0.85)
#define ROTLEFT_DEG				(-90.0)
#define ROTRIGHT_DEG			(90.0)
#define IMG_CHUNK				(16384) // in bytes

#define ICON_SOURCE				"/pic/ViewerButtons/"
#define POOL_SOURCE				"/pool/"
#define SHADOW_SOURCE			"/shadow-cache/sc-"

typedef enum {
  V_UNKNOWN = -1,
  V_ORIGINAL,
  V_SHADOW,
  V_BESTFIT,
  V_ZOOMIN,
  V_ZOOMOUT,
  V_ROTLEFT,
  V_ROTRIGHT,
  V_FULLSCREEN
} v_modes;

class FQTermImage : public QWidget {
  Q_OBJECT;
public:
  FQTermImage(QWidget * parent, Qt::WindowFlags f);
  virtual void adjustItemSize() = 0;
  virtual void scrollTo(const QString &) = 0;
  virtual void updateImage(const QString &) = 0;
};

class FQTermImageFlow: public FQTermImage {
  Q_OBJECT;

public:
  FQTermImageFlow(FQTermConfig *, QWidget *, Qt::WindowFlags);
  ~FQTermImageFlow();

  // functions
  QIcon newIcon(const QString &);
  void adjustItemSize();
  void scrollTo(const QString &);
  void updateImage(const QString &);
  int currentIndex();
  QPixmap *shadowPixmap(const QFileInfo &);
  QPixmap *origPixmap(const QFileInfo &);

  // variables
  FQTermConfig *config_;
  PictureFlow *ivPicFlow;
  PictureView *ivPicView;
  QToolBar *ivToolBar;
  QSlider *ivSlider;
  QComboBox *ivSortOpts;
  QVBoxLayout *ivMainLayout;
  QDesktopWidget *desktop;
  QRect dispRect;
  QDir::SortFlags sortFlag;

  double frameWidth, frameHeight;
  double pixmapWidth, pixmapHeight;
  QString iconSource;
  QString poolPath, shadowPath;
  QStringList imageSuffixes;
  QFileInfoList imageDirList;


public slots:
  void setSliderMax(const int);
  void setSliderPos(const int);
  void showSlideImage(const int);
  void saveImage();
  void sortPoolDir(const int);
  void deleteImage();

signals:
  void totalCountChanged(const int);
  void dirListChanged(QFileInfoList &, QString &);
  void purgeImageCache(QString &);

protected:
  void keyPressEvent(QKeyEvent *);
  void wheelEvent(QWheelEvent *);
  void mousePressEvent(QMouseEvent *);
  void closeEvent(QKeyEvent *);
};

class PictureView: public QWidget {
  Q_OBJECT;

public:
  PictureView(FQTermImageFlow *, Qt::WindowFlags);
  ~PictureView();

  // variables
  FQTermImageFlow *iv;
  PictureFlow *picFlow;
  ImageDisplay *imageDisplay;
  QScrollArea *scrollArea;
  QToolBar *toolBar;
  QVBoxLayout *mainLayout;
  QStatusBar *statInfo;

public slots:
  void viewCurrentImage();
  void viewImage(const QString &, const bool);
  void closeThisWindow();

signals:
  void viewModeChanged(const v_modes);
  void purgeImageCache(QString &);

private:
  void setViewMode(const v_modes);

private slots:
  void showOrig();
  void showBestfit();
  void zoomIn();
  void zoomOut();
  void rotateLeft();
  void rotateRight();
  void showFullscreen();
  void updateTitle(const int);

protected:
  void keyPressEvent(QKeyEvent *);
  void closeEvent(QCloseEvent *);

};

class ImageDisplay: public QLabel {
  Q_OBJECT;

public:
  ImageDisplay(PictureView *parent);
  ~ImageDisplay();

  // functions
  void setCurrentPixmap(QPixmap *);
  QPixmap *currentPixmap() const;

  // variables
  FQTermImageFlow *iv;
  PictureView *picView;
  QPixmap *curPixmap;

public slots:
  // functions
  void renderImage(const int);
  void updateImageInfo(const int);
  void updateFrameSize(const int, const int);

signals:
  void imageInfoChanged(const int);
  void pixmapSizeChanged(const int, const int);

private:
  // functions
  QPixmap *renderPixmap(QPixmap *, const double, const double, const v_modes);
  void updateImageCache(const int);
  void drawImage(const QPixmap *, QPainter &);

private slots:
  void displayImage(const v_modes);

protected:
  void paintEvent(QPaintEvent *);

};


//the origin image viewer
class FQTermCanvas;
class ExifExtractor;

class ItemDelegate : public QItemDelegate{
public:
  ItemDelegate()
  {
    size_ = QSize(250,200);
  }
  QSize sizeHint (const QStyleOptionViewItem & option, const QModelIndex & index) const {
    return size_;
  }
  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const;
  static QSize size_;
};

class ExifTable : public QLabel
{
  Q_OBJECT
public:
  ExifTable(QWidget *parent);
signals:
  void showExifDetails();
protected:
  void mouseReleaseEvent(QMouseEvent *pEvent);


};

class ImageViewerDirModel : public QDirModel
{
public:
  ImageViewerDirModel(QObject *parent = 0);

  int columnCount(const QModelIndex & = QModelIndex()) const;
  QVariant headerData ( int section, Qt::Orientation orientation, int role) const;
  QVariant data(const QModelIndex &index, int role) const;
};


class FQTermImageOrigin: public FQTermImage {
  Q_OBJECT;
public:
  FQTermImageOrigin(FQTermConfig * config, QWidget *parent, Qt::WindowFlags wflag);
  ~FQTermImageOrigin();
  void scrollTo(const QString& filename);
  void updateImage(const QString& filename);

  public slots:
    void onChange(const QModelIndex & index);
    void next();
    void previous();
    void adjustItemSize();
    void selectionChanged(const QItemSelection & selected, const QItemSelection & deselected);
    void sortFileList(int index);
    void showFullExifInfo();
    void adjustLayout(bool withExifTable);
    void updateExifInfo();

protected:

  void closeEvent(QCloseEvent *clse);


private:
  FQTermCanvas* canvas_;
  QTreeView* tree_;
  ImageViewerDirModel* model_;
  QMenuBar* menuBar_;
  QComboBox* comboBox_;
  FQTermConfig* config_;
  ExifExtractor* exifExtractor_;
  ExifTable* exifTable_;
  QGridLayout* layout_;
  bool isExifTableShown_;
};


}  // namespace FQTerm

#endif  // IMAGE_VIERWER_H
