/***************************************************************************
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
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
 ***************************************************************************/

#ifndef FQTERM_CANVAS_H
#define FQTERM_CANVAS_H

//Added by qt3to4:
#include <QScrollArea>
#include <QImage>
// #include <QResizeEvent>
// #include <QCloseEvent>

class QLabel;
class QMenu;
class QCloseEvent;
class QMouseEvent;
class QKeyEvent;
class QResizeEvent;
class QToolBar;
class QImage;

namespace FQTerm {

class FQTermConfig;
class ExifExtractor;

class FQTermCanvas: public QScrollArea {
  Q_OBJECT;
 public:
  FQTermCanvas(FQTermConfig *, QWidget *parent_ = NULL, Qt::WFlags f = Qt::Window);
  ~FQTermCanvas();

  enum AdjustMode{Origin, Fit, Stretch, MaxFit};

  void updateImage(const QString& filename);
  void loadImage(QString, bool = true);
  QMenu* menu();
  QToolBar* ToolBar();
 public slots:
  void oriSize();
  void zoomIn();
  void zoomOut();
  void autoAdjust();
  void fullScreen();
  void saveImage();
  void copyImage();
  void silentCopy();
  void cwRotate();
  void ccwRotate();
  void deleteImage();
  void SetAdjustMode(AdjustMode am);
  void openDir();
 protected slots:
  void SetAdjustMode();
 protected:
  void resizeEvent (QResizeEvent * event);
  void moveImage(float, float);
  void resizeImage(float);
  void rotateImage(float);

  void closeEvent(QCloseEvent*);
  void mousePressEvent(QMouseEvent*);
  void keyPressEvent(QKeyEvent *ke);
  void viewportResizeEvent(QResizeEvent *re);
  void adjustSize(QSize);
  QPixmap scaleImage(const QSize &);
 protected:
  QLabel *label;
  bool useAdjustMode_;
  QSize imageSize_;
  QString fileName_;
  QImage image_;
  QMenu *menu_;
  QToolBar *toolBar_;
  FQTermConfig * config_;
  

  AdjustMode adjustMode_;
  Qt::AspectRatioMode aspectRatioMode_;
  // TODO: Very dirty trick, I hate it
  bool isEmbedded;
};

}  // namespace FQTerm

#endif  // FQTERM_CANVAS_H
