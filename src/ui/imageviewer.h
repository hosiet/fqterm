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
#ifndef IMAGE_VIERWER_H
#define IMAGE_VIERWER_H

#include <QDirModel>
#include <QDialog>
#include <QApplication>
#include <QItemDelegate>
#include <QPainter>
#include <QLabel>


class QString;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QHeaderView;
class QModelIndex;
class QTreeView;
class QMenuBar;
class QStyleOptionViewItem;
class QItemSelection;
class QComboBox;
class QGridLayout;

namespace FQTerm {

class FQTermCanvas;
class FQTermConfig;
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


class FQTermImage: public QWidget {
  Q_OBJECT;
public:
   FQTermImage(FQTermConfig * config, QWidget *parent, Qt::WindowFlags wflag);
   ~FQTermImage();
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
