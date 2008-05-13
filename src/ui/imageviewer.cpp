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
#include "imageviewer.h"
#include "fqterm_canvas.h"
#include "fqterm_exif_extractor.h"

#include <QTreeView>
#include <QComboBox>
#include <QTextEdit>
#include <QFile>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QGridLayout>
#include <QModelIndex>
#include <QToolBar>
#include <QIcon>
#include <QItemSelectionModel>

#include "fqterm_trace.h"
#include "fqterm_path.h"
#include "fqterm_config.h"

namespace FQTerm {


void FQTermImage::onChange(const QModelIndex & index) {
  if (!model_->isDir(index)) {
    if (!isHidden())
      canvas_->hide();

    QString exifInfo = QString::fromStdString(exifExtractor_->extractExifInfo(model_->filePath(tree_->currentIndex()).toLocal8Bit().data()));
    bool resized = false;
    if (exifInfo != "") {
      if (!isExifTableShown_) {
        adjustLayout(true);
        isExifTableShown_ = true;
        resized = true;
      }
      updateExifInfo();
    } else {
      if (isExifTableShown_) {
        adjustLayout(false);
        isExifTableShown_ = false;
        resized = true;
      }
    }
    QString path  = QDir::toNativeSeparators(model_->filePath(index));
    if (path.endsWith(QDir::separator()))
      path.chop(1);
    canvas_->loadImage(path, !resized);
//    canvas_->autoAdjust();
    if (!isHidden())
      canvas_->show();
  }
}

FQTermImage::~FQTermImage() {
  delete menuBar_;
  delete canvas_;
  delete tree_;
  delete model_;
}

FQTermImage::FQTermImage(FQTermConfig * config, QWidget *parent,
                         Qt::WindowFlags wflag) :
    QWidget(parent, wflag),
    config_(config),
    isExifTableShown_(false) {
  setWindowTitle(tr("FQTerm Image Viewer"));
  ItemDelegate* itemDelegate = new ItemDelegate;
  exifExtractor_ = new ExifExtractor;
  exifTable_ = new ExifTable(this);
  exifTable_->setTextFormat(Qt::RichText);
  exifTable_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  canvas_ = new FQTermCanvas(config, this, 0);
  canvas_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  model_ = new ImageViewerDirModel;
  tree_ = new QTreeView;
  tree_->setModel(model_);
  tree_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  adjustItemSize();
  tree_->setItemDelegate(itemDelegate);
  tree_->setColumnWidth(1, 150);
//  tree_->hideColumn(0);

  tree_->setUniformRowHeights(true);
  tree_->setWordWrap(true);

  comboBox_ = new QComboBox(this);
  comboBox_->addItem(tr("Sort by name"), QDir::Name);
  comboBox_->addItem(tr("Sort by time"), QDir::Time);
  comboBox_->addItem(tr("Sort by size"), QDir::Size);
  comboBox_->addItem(tr("Sort by type"), QDir::Type);

  FQ_VERIFY(connect(comboBox_, SIGNAL(currentIndexChanged(int)), this, SLOT(sortFileList(int))));

  comboBox_->setCurrentIndex(1);

  layout_ = new QGridLayout;
  menuBar_ = new QMenuBar(this);
  menuBar_->addMenu(canvas_->menu());
  menuBar_->resize(1,1);

  canvas_->ToolBar()->addAction(
      QIcon(getPath(RESOURCE) + "/pic/ViewerButtons/prev.png"), tr("Previous"),
      this, SLOT(previous()));
  canvas_->ToolBar()->addAction(
      QIcon(getPath(RESOURCE) + "/pic/ViewerButtons/next.png"), tr("Next"),
      this, SLOT(next()));

  layout_->addWidget(tree_, 0, 0, 12, 1);
  layout_->addWidget(comboBox_, 12, 0, 1, 1);
  layout_->addWidget(canvas_, 0, 1, 12, 10);
//  layout_->addWidget(exifTable_, 10, 1, 2, 10);
  layout_->addWidget(canvas_->ToolBar(), 12, 1, 1, 10, Qt::AlignHCenter);
  layout_->setColumnMinimumWidth(0, tree_->columnWidth(0) + 150);
  setLayout(layout_);
  /*
  FQ_VERIFY(connect(tree_, SIGNAL(clicked(const QModelIndex &)),
                    this, SLOT(onChange(const QModelIndex &))));
                    */
  FQ_VERIFY(connect(tree_, SIGNAL(activated(const QModelIndex &)),
                    this, SLOT(onChange(const QModelIndex &))));
  FQ_VERIFY(connect(tree_->selectionModel(),
                    SIGNAL(selectionChanged(const QItemSelection&,
                                            const QItemSelection&)),
                    this, SLOT(selectionChanged(const QItemSelection&,
                                                const QItemSelection&))));
  FQ_VERIFY(connect(exifTable_, SIGNAL(showExifDetails()),
    this, SLOT(showFullExifInfo())));
}

void FQTermImage::scrollTo(const QString& filename) {
  QString path = QFileInfo(filename).absolutePath();
  model_->refresh();
  tree_->setRootIndex(model_->index(path));
  canvas_->loadImage(filename);
  if (canvas_->isHidden() && !isHidden()) {
    canvas_->show();
  }
  const QModelIndex& index = model_->index(filename);
  tree_->scrollTo(index);
  tree_->setCurrentIndex(index);
}

void FQTermImage::updateImage(const QString& filename) {
  static int i = 0;
  if (++i == 10) {
    model_->refresh(model_->index(filename));
    i = 0;
  }
  canvas_->updateImage(filename);
}

void FQTermImage::previous() {
  const QModelIndex& index = tree_->indexAbove(tree_->currentIndex());
  if (index.isValid()) {
    tree_->setCurrentIndex(index);
    canvas_->loadImage(QDir::toNativeSeparators(model_->filePath(index)));
  }
}

void FQTermImage::next() {
  const QModelIndex& index = tree_->indexBelow(tree_->currentIndex());
  if (index.isValid()) {
    tree_->setCurrentIndex(index);
    canvas_->loadImage(QDir::toNativeSeparators(model_->filePath(index)));
  }
}

void FQTermImage::adjustItemSize() {
  QFontMetrics fm(font());
  ItemDelegate::size_.setWidth(qMax(128, fm.width("WWWWWWWW.WWW")));
  ItemDelegate::size_.setHeight(fm.height() + 150);
}

void FQTermImage::selectionChanged(const QItemSelection & selected,
                                   const QItemSelection & deselected) {
  onChange(tree_->selectionModel()->currentIndex());
}

void FQTermImage::sortFileList(int index) {
  model_->setSorting(QDir::SortFlag(comboBox_->itemData(index).toInt()));
  QString poolPath = config_->getItemValue("preference", "pool");
  if (poolPath.isEmpty()) {
    poolPath = getPath(USER_CONFIG) + "pool/";
  }
  tree_->setRootIndex(model_->index(poolPath));
}

void FQTermImage::showFullExifInfo()
{
  QString exifInfo = QString::fromStdString(exifExtractor_->extractExifInfo(model_->filePath(tree_->currentIndex()).toLocal8Bit().data()));
  QTextEdit* info = new QTextEdit;
  info->setText(exifInfo);
  info->setWindowFlags(Qt::Dialog);
  info->setAttribute(Qt::WA_DeleteOnClose);
  info->setAttribute(Qt::WA_ShowModal);
//  info->setLineWrapMode(QTextEdit::NoWrap);
  info->setReadOnly(true);
  QFontMetrics fm(font());
  info->resize(fm.width("Orientation : 1st row - 1st col : top - left side"), fm.height() * 20);
  info->show();
}

void FQTermImage::adjustLayout(bool withExifTable) {
  if (withExifTable) {
    layout_->addWidget(canvas_, 0, 1, 11, 10);
    layout_->addWidget(exifTable_, 11, 1, 1, 10, Qt::AlignHCenter);
    if (!isHidden() && exifTable_->isHidden()) {
      exifTable_->show();
    }
    layout_->addWidget(canvas_->ToolBar(), 12, 1, 1, 10, Qt::AlignHCenter);
  } else {
    layout_->addWidget(canvas_, 0, 1, 12, 10);
    layout_->removeWidget(exifTable_);
    exifTable_->hide();
    layout_->addWidget(canvas_->ToolBar(), 12, 1, 1, 10, Qt::AlignHCenter);
  }
}

void FQTermImage::updateExifInfo() {
  exifTable_->clear();

  QString exifInfoToShow = "<table border=\"1\"><tr><td>"
    + tr("Model") + " : " + QString::fromStdString((*exifExtractor_)["Model"]) + "</td><td>"
    + QString::fromStdString((*exifExtractor_)["DateTime"]) + "</td><td>"
    + QString::fromStdString((*exifExtractor_)["Flash"]) + "</td>"
    + "</tr><tr><td>"
    + tr("ExposureTime") + " : " + QString::fromStdString((*exifExtractor_)["ExposureTime"]) + "</td><td>"
    + tr("FNumber") + " : " + QString::fromStdString((*exifExtractor_)["FNumber"]) + "</td><td>"
    + tr("ISO") + " : " + QString::fromStdString((*exifExtractor_)["ISOSpeedRatings"]) + "</td>"
    + "</tr><tr><td>"
    + tr("FocalLength") + " : " + QString::fromStdString((*exifExtractor_)["FocalLength"]) + "</td><td>"
    + tr("MeteringMode") + " : " + QString::fromStdString((*exifExtractor_)["MeteringMode"]) + "</td><td>" 
    + tr("ExposureBias") + " : " + QString::fromStdString((*exifExtractor_)["ExposureBiasValue"]) + "</td></tr></tabel>";

  exifTable_->setText(exifInfoToShow);
  if (!isHidden() && exifTable_->isHidden()) {
    exifTable_->show();
  }
}
ImageViewerDirModel::ImageViewerDirModel(QObject *parent /*= 0*/)
  : QDirModel(parent) {
  insertColumn(1);
  QStringList nameFilterList;
  nameFilterList << "*.jpg" <<  "*.jpeg" << "*.png"
                 << "*.mng" << "*.bmp" << "*.gif";
  setNameFilters(nameFilterList);
  setFilter(QDir::Files);
}

int ImageViewerDirModel::columnCount(const QModelIndex &/*parent*/) const {
  return 1;
}

QVariant ImageViewerDirModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole) {
    if (section == 0) {
      return QString(tr("Image Preview"));
    }
  }
  return QDirModel::headerData(section, orientation, role);
}

QVariant ImageViewerDirModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::DecorationRole) {
    if (isDir(index)) {
      return QVariant();
    }
    QString path  = QDir::toNativeSeparators(filePath(index));
    if (path.endsWith(QDir::separator()))
      path.chop(1);

    QPixmap pixmap(path);
    if (pixmap.height() > 128 || pixmap.width() > 128) {
      return pixmap.scaled(128, 128,
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pixmap;
  } else if (role == Qt::DisplayRole) {
    return fileName(index);
  }/*
  else if (role == Qt::TextAlignmentRole) {
    return Qt::Qt::AlignBottom;
  }*/
  return QVariant();
}

QSize ItemDelegate::size_;

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem & option,
                         const QModelIndex & index ) const {
  QStyleOptionViewItemV3 opt = setOptions(index, option);

  // prepare
  painter->save();

  // get the data and the rectangles

  const QPixmap& pixmap
      = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));
  QRect decorationRect = QRect(opt.rect.topLeft(), pixmap.size());
  decorationRect.moveTo(decorationRect.left(), decorationRect.top() + 10);
  const QString& text = index.data(Qt::DisplayRole).toString();
  QFontMetrics fm(painter->font());
  QRect displayRect = QRect(decorationRect.bottomLeft(),
                            QSize(fm.width(text),fm.height()));

  QRect checkRect;
  Qt::CheckState checkState = Qt::Unchecked;
  QVariant value = index.data(Qt::CheckStateRole);
  if (value.isValid()) {
    checkState = static_cast<Qt::CheckState>(value.toInt());
    checkRect = check(opt, opt.rect, value);
  }

  // do the layout

//  doLayout(opt, &checkRect, &decorationRect, &displayRect, false);

  // draw the item

  drawBackground(painter, opt, index);
  painter->drawPixmap(decorationRect, pixmap);
  painter->drawText(displayRect, text);

  drawFocus(painter, opt, displayRect);

  // done
  painter->restore();
}

void ExifTable::mouseReleaseEvent(QMouseEvent *pEvent)
{
  if (pEvent->button() == Qt::LeftButton) {
    emit(showExifDetails());
  }
}

ExifTable::ExifTable(QWidget *parent) : QLabel(parent) {

}

}  // namespace FQTerm

#include "imageviewer.moc"
