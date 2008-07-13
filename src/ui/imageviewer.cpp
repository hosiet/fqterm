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

#include "imageviewer.h"
#include "fqterm_exif_extractor.h"
#include "fqterm_canvas.h"

#include <QAction>
#include <QBuffer>
#include <QCursor>
#include <QFile>
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QDir>
#include <QDataStream>
#include <QFileInfo>
#include <QSizeGrip>
#include <QString>
#include <QIcon>
#include <QTextCodec>
#include <QToolTip>
#include <QTransform>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>


#include <QTreeView>
#include <QMenuBar>
#include <QComboBox>
#include <QTextEdit>

#include "fqterm_trace.h"
#include "fqterm_path.h"
#include "fqterm_config.h"
#include "fqterm_filedialog.h"
#include "pictureflow.h"

namespace FQTerm {


FQTermImageFlow::~FQTermImageFlow() {
  delete desktop;
  delete ivPicFlow;
  delete ivPicView;
  delete ivSortOpts;
  delete ivToolBar;
  delete ivSlider;
  delete ivMainLayout;
}

FQTermImageFlow::FQTermImageFlow(FQTermConfig * config, QWidget *parent,
                         Qt::WindowFlags wflag) :
	FQTermImage(parent, wflag),
	config_(config) {

  // first we get user's desktop geometry (the whole visible area)
  // to determine display parameters.
  desktop = new QDesktopWidget;
  dispRect = desktop->screenGeometry(desktop->primaryScreen());
  double slideWidth = dispRect.width() / 3.7;
  double slideHeight = dispRect.height() / 3.7;
  frameWidth = dispRect.width() / 1.6;
  frameHeight = dispRect.height() / 1.6;
  pixmapWidth = dispRect.width() / 2.0;
  pixmapHeight = dispRect.height() / 2.0;

  setWindowTitle("FQterm Image Viewer");
  setFixedSize(QSize(frameWidth, frameHeight));
  setFocusPolicy(Qt::StrongFocus);
  setFont(QFont("Sans Serif", 12));

  iconSource = getPath(RESOURCE) + ICON_SOURCE;
  poolPath = getPath(USER_CONFIG) + POOL_SOURCE;
  shadowPath = poolPath + SHADOW_SOURCE;
  // decent image formats
  imageSuffixes << "*.jpg" << "*.jpeg" << "*.gif"
			<< "*.png" << "*.tiff" << "*.bmp"
			<< "*.mng";

  ivPicFlow = new PictureFlow(this);
  ivPicFlow->setBackgroundColor(Qt::black);
  // it seems the slides look better with a swap of slideWidth and slideHeight...
  ivPicFlow->setSlideSize(QSize(slideHeight, slideWidth));

  ivPicView = new PictureView(this, Qt::Window);
  ivPicView->setMinimumSize(QSize(frameWidth, frameHeight));

  QAction *quitThisAct = new QAction(newIcon(tr("window-close.png")), tr("&Quit"), this);
  quitThisAct->setShortcut(QKeySequence(Qt::Key_Q));
  quitThisAct->setToolTip("Quit");
  FQ_VERIFY(connect(quitThisAct, SIGNAL(triggered()), this, SLOT(close())));

  QAction *saveImageAct = new QAction(newIcon(tr("document-save-as.png")), tr("&Save"), this);
  saveImageAct->setShortcut(QKeySequence(Qt::Key_S));
  saveImageAct->setToolTip("Save as");
  FQ_VERIFY(connect(saveImageAct, SIGNAL(triggered()), this, SLOT(saveImage())));

  QAction *deleteImageAct = new QAction(newIcon(tr("edit-delete.png")), tr("&Delte"), this);
  deleteImageAct->setShortcut(QKeySequence(Qt::Key_D));
  deleteImageAct->setToolTip("Delete");
  FQ_VERIFY(connect(deleteImageAct, SIGNAL(triggered()), this, SLOT(deleteImage())));

  QLabel *textLabel = new QLabel(tr("Sort by "), 0);
  textLabel->setFont(QFont("Sans Serif", 10, QFont::Bold));

  sortFlag = QDir::Time;
  ivSortOpts = new QComboBox(this);
  ivSortOpts->setFocusPolicy(Qt::NoFocus);
  ivSortOpts->setFont(QFont("Sans Serif", 10));
  ivSortOpts->setFrame(false);
  ivSortOpts->insertItem(1, tr("Time "));
  ivSortOpts->insertItem(0, tr("Name "));
  ivSortOpts->insertItem(2, tr("Size "));
  ivSortOpts->insertItem(3, tr("Type "));
  FQ_VERIFY(connect(ivSortOpts, SIGNAL(currentIndexChanged(int)), this, SLOT(sortPoolDir(int))));

  ivToolBar = new QToolBar(this);
  ivToolBar->setIconSize(QSize(32, 24));
  ivToolBar->addAction(quitThisAct);
  ivToolBar->addSeparator();
  ivToolBar->addAction(saveImageAct);
  ivToolBar->addAction(deleteImageAct);
  ivToolBar->addSeparator();
  ivToolBar->addWidget(textLabel);
  ivToolBar->addWidget(ivSortOpts);

  ivSlider = new QSlider(Qt::Horizontal, this);
  ivSlider->setFocusPolicy(Qt::ClickFocus);
  ivSlider->setSingleStep(1);
  ivSlider->setPageStep(1);

  FQ_VERIFY(connect(this, SIGNAL(totalCountChanged(int)), this, SLOT(setSliderMax(int))));
  FQ_VERIFY(connect(ivPicFlow, SIGNAL(totalCountChanged(int)), this, SLOT(setSliderMax(int))));
  FQ_VERIFY(connect(ivPicFlow, SIGNAL(centerIndexChanged(int)), this, SLOT(setSliderPos(int))));
  FQ_VERIFY(connect(this, SIGNAL(dirListChanged(QFileInfoList &, QString &)), ivPicFlow, SLOT(updateDirList(QFileInfoList &, QString &))));
  FQ_VERIFY(connect(this, SIGNAL(purgeImageCache(QString &)), ivPicFlow, SLOT(purgeImageHash(QString &))));
  FQ_VERIFY(connect(ivPicView, SIGNAL(purgeImageCache(QString &)), ivPicFlow, SLOT(purgeImageHash(QString &))));
  FQ_VERIFY(connect(ivSlider, SIGNAL(valueChanged(int)), this, SLOT(showSlideImage(int))));
  FQ_VERIFY(connect(ivPicFlow, SIGNAL(viewCurrentIndex()), ivPicView, SLOT(viewCurrentImage())));

  ivMainLayout = new QVBoxLayout(this);
  ivMainLayout->setMargin(0);
  ivMainLayout->addWidget(ivPicFlow);
  ivMainLayout->addWidget(ivSlider);
  ivMainLayout->addWidget(ivToolBar);
  ivMainLayout->setEnabled(true);
  setLayout(ivMainLayout);

  // for pixmap cache
  if (QPixmapCache::cacheLimit() < PIXMAP_CACHESIZE) {
	QPixmapCache::setCacheLimit(PIXMAP_CACHESIZE);
  }

  // at startup we need to hide ivPicView
  ivPicView->hide();
  sortPoolDir(-1);
  // move to the center of the screen.
  move((dispRect.width() - width()) / 2.0,
		(dispRect.height() - height()) / 2.0);
}

QIcon FQTermImageFlow::newIcon(const QString &iconPath) {

  if (iconPath.isEmpty()) {
	return QIcon();
  } else {
	return QIcon(iconSource + iconPath);
  }
}

/***********************************************/
/* these three functions below are kept for    */
/* API compatibility.                          */
/***********************************************/
void FQTermImageFlow::adjustItemSize() {

}

void FQTermImageFlow::scrollTo(const QString &filePath) {

  ivPicView->viewImage(filePath, true);
}

void FQTermImageFlow::updateImage(const QString &filePath) {

  ivPicView->viewImage(filePath, false);
}
/****************            *******************/
/***********************************************/

void FQTermImageFlow::setSliderMax(const int max) {

  ivSlider->setRange(0, max);
}

void FQTermImageFlow::setSliderPos(const int pos) {

  ivSlider->setSliderPosition(pos);
}

void FQTermImageFlow::showSlideImage(const int index) {

  ivPicFlow->showSlide(index);
}

void FQTermImageFlow::saveImage() {

  int index;

  if ( (index = currentIndex()) < 0) {
	return;
  }

  FQTermFileDialog *saveDlg = new FQTermFileDialog(config_);

  QFileInfo imgFile = imageDirList.at(index);
  QString saveName = saveDlg->getSaveName(imgFile.fileName(),
									"Image Files (*." + imgFile.suffix().toLower() + " *." + imgFile.suffix().toUpper() + ")", this);

  if (!saveName.isEmpty()) {
	QFile newFile(imgFile.absoluteFilePath());
	newFile.copy(saveName);
  } 

  delete saveDlg;
}

void FQTermImageFlow::deleteImage() {

  int index;

  if ( (index = currentIndex()) < 0) {
	return;
  }

  QMessageBox *ivDelete = new QMessageBox(this);
  QString strWarning = tr("<div>") +
						tr("<h4>Are you sure to delete this image?</h4>") +
						tr("</div>");

  ivDelete->setWindowTitle(tr("Wanring!"));
  ivDelete->setIcon(QMessageBox::Warning);
  ivDelete->setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ivDelete->setDefaultButton(QMessageBox::Cancel);
  ivDelete->setTextFormat(Qt::RichText);
  ivDelete->setText(strWarning);

  if (ivDelete->exec() == QMessageBox::Ok) {

	QFileInfo fi = imageDirList.at(index);
	QString s = fi.fileName();
	QFile imgFile(fi.absoluteFilePath());

	if (imgFile.remove()) {
	  // if there is a copy in disk cache, remove it.
	  if (QFile::exists(shadowPath + s)) {
		imgFile.setFileName(shadowPath + s);
		imgFile.remove();
		// finally, remove the copies in memory cache.
		QPixmapCache::remove(s);
		QPixmapCache::remove("sc-" + s);
	  }

	  ivPicFlow->deleteSlide(index);
	  emit purgeImageCache(s);
	  sortPoolDir(-1);
	}
  }

  if (!hasFocus()) {
	setFocus();
  }
  delete ivDelete;
}

void FQTermImageFlow::sortPoolDir(const int index) {

  QString originalImage;
  QDir imageDir(poolPath);

  if (!imageDir.exists()) {
	imageDir.mkdir(poolPath);
  }

  if (index >= 0) {
	sortFlag = QDir::SortFlag(index);
  }

  if (!imageDirList.isEmpty()) {
	originalImage = imageDirList.at(currentIndex()).absoluteFilePath();
  }

  QDir::Filters fileFilter = QDir::Files | QDir::Readable | QDir::NoSymLinks;
  imageDir.setNameFilters(imageSuffixes);
  imageDirList = imageDir.entryInfoList(fileFilter, sortFlag);

  if (!imageDirList.isEmpty()) {
	emit totalCountChanged(imageDirList.count() - 1);
	emit dirListChanged(imageDirList, originalImage);
  }
}

int FQTermImageFlow::currentIndex() {

  int index = ivPicFlow->centerIndex();

  if (imageDirList.isEmpty()
	  || index < 0
	  || index >= imageDirList.count()) {
	return -1;
  } else {
	return index;
  }
}

QPixmap *FQTermImageFlow::shadowPixmap(const QFileInfo &finfo) {

  if (finfo.suffix().toLatin1().toLower() == "gif") {
	return origPixmap(finfo);
  }

  QString shadowFilePath = shadowPath + finfo.fileName();
  QPixmap *p = new QPixmap;

  if (QPixmapCache::find("sc-" + finfo.fileName(), *p)) {
	return p;
  }

  if (!QFile::exists(shadowFilePath)) {
	p->load(finfo.absoluteFilePath(), finfo.suffix().toLatin1());
	// if current image is no bigger than the framge size, don't resize it.
	if (p->width() > pixmapWidth ||
		(p->height() > pixmapHeight)) {
	  p = new QPixmap(p->scaled(pixmapWidth, pixmapHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	  p->save(shadowFilePath, finfo.suffix().toLatin1(), 75);
	}
  } else {
	p->load(shadowFilePath, finfo.suffix().toLatin1());
  }

  QPixmapCache::insert("sc-" + finfo.fileName(), *p);

  return p;
}

QPixmap *FQTermImageFlow::origPixmap(const QFileInfo &finfo) {

  QString origFilePath = poolPath + finfo.fileName();
  QPixmap *p = new QPixmap;

  if (QPixmapCache::find(finfo.fileName(), *p)) {
	return p;
  }

  if(!QFile::exists(origFilePath)) {
	p->fill(Qt::transparent);
  } else {
	p->load(origFilePath, finfo.suffix().toLatin1());
	QPixmapCache::insert(finfo.fileName(), *p);
  }

  return p;
}

void FQTermImageFlow::keyPressEvent(QKeyEvent *event) {

  ivPicView->close();

  switch (event->key()) {
	case Qt::Key_Home:
	  ivPicFlow->showSlide(0);
	  break;
	case Qt::Key_End:
	  ivPicFlow->showSlide(imageDirList.count() - 1);
	  break;
	case Qt::Key_Up:
	  ivPicFlow->showSlide(ivPicFlow->centerIndex() + 5);
	  break;
	case Qt::Key_Down:
	  ivPicFlow->showSlide(ivPicFlow->centerIndex() - 5);
	  break;
	case Qt::Key_PageUp:
	case Qt::Key_Delete:
	case Qt::Key_Left:
	  ivPicFlow->showPrevious();
	  break;
	case Qt::Key_PageDown:
	case Qt::Key_Space:
	case Qt::Key_Right:
	  ivPicFlow->showNext();
	  break;
	case Qt::Key_Return:
	case Qt::Key_Enter:
	case Qt::Key_V:
	  ivPicView->viewCurrentImage();
	  break;
	case Qt::Key_Q:
		close();
	  break;
	default:
	  break;
  }
}

void FQTermImageFlow::wheelEvent(QWheelEvent *event) {
  // enable mouse wheel events in the browser.
  int numDegrees = event->delta() / 8;
  int numSteps = numDegrees / 15;

  if (numSteps < 0) {
	ivPicFlow->showPrevious();
  } else {
	ivPicFlow->showNext();
  }

  event->accept();
}

void FQTermImageFlow::mousePressEvent(QMouseEvent *event) {
  // any mouse press event will raise the browser.
  ivPicView->close();
  activateWindow();
  raise();
}

void FQTermImageFlow::closeEvent(QKeyEvent *event) {

  ivPicView->hide();
  hide();
}

/*******************************/
/****  Picture viewer **********/
/*******************************/

PictureView::PictureView(FQTermImageFlow *parent, Qt::WindowFlags wflag)
	:QWidget(parent, wflag) {

  iv = parent;
  picFlow = parent->ivPicFlow;

  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Midlight);
  setFocusPolicy(Qt::StrongFocus);

  imageDisplay = new ImageDisplay(this);
  imageDisplay->setMinimumSize(QSize(iv->frameWidth, iv->frameHeight));

  scrollArea = new QScrollArea(this);
  scrollArea->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
  scrollArea->setAutoFillBackground(true);
  scrollArea->setBackgroundRole(QPalette::Dark);
  scrollArea->setAlignment(Qt::AlignCenter);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(imageDisplay);

  QAction *quitThisAct = new QAction(iv->newIcon(tr("window-close.png")), tr("&Quit"), this);
  quitThisAct->setShortcut(QKeySequence(Qt::Key_Q));
  quitThisAct->setToolTip("Quit this window");
  FQ_VERIFY(connect(quitThisAct, SIGNAL(triggered()), this, SLOT(closeThisWindow())));

  QAction *showOrigAct = new QAction(iv->newIcon(tr("zoom-original.png")), tr("Ori&ginal"), this);
  showOrigAct->setShortcut(QKeySequence(Qt::Key_G));
  showOrigAct->setToolTip("Original");
  FQ_VERIFY(connect(showOrigAct, SIGNAL(triggered()), this, SLOT(showOrig())));

  QAction *showBestfitAct = new QAction(iv->newIcon(tr("zoom-fit-best.png")), tr("&Best fit"), this);
  showBestfitAct->setShortcut(QKeySequence(Qt::Key_B));
  showBestfitAct->setToolTip("Fit best");
  FQ_VERIFY(connect(showBestfitAct, SIGNAL(triggered()), this, SLOT(showBestfit())));

  QAction *zoomInAct = new QAction(iv->newIcon(tr("zoom-in.png")), tr("Zoom &in"), this);
  zoomInAct->setShortcut(QKeySequence(Qt::Key_I));
  zoomInAct->setToolTip("Zoom in");
  FQ_VERIFY(connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn())));

  QAction *zoomOutAct = new QAction(iv->newIcon(tr("zoom-out.png")), tr("Zoom &out"), this);
  zoomOutAct->setShortcut(QKeySequence(Qt::Key_O));
  zoomOutAct->setToolTip("Zoom out");
  FQ_VERIFY(connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut())));

  QAction *rotLeftAct = new QAction(iv->newIcon(tr("object-rotate-left.png")), tr("Rotate &left"), this);
  rotLeftAct->setShortcut(QKeySequence(Qt::Key_L));
  rotLeftAct->setToolTip("Rotate left");
  FQ_VERIFY(connect(rotLeftAct, SIGNAL(triggered()), this, SLOT(rotateLeft())));

  QAction *rotRightAct = new QAction(iv->newIcon(tr("object-rotate-right.png")), tr("Rotate &right"), this);
  rotRightAct->setShortcut(QKeySequence(Qt::Key_R));
  rotRightAct->setToolTip("Rotate right");
  FQ_VERIFY(connect(rotRightAct, SIGNAL(triggered()), this, SLOT(rotateRight())));

  QAction *showFullscreenAct = new QAction(iv->newIcon(tr("view-fullscreen.png")), tr("&Fullscreen"), this);
  showFullscreenAct->setShortcut(QKeySequence(Qt::Key_F));
  showFullscreenAct->setToolTip("Fullscreen");
  FQ_VERIFY(connect(showFullscreenAct, SIGNAL(triggered()), this, SLOT(showFullscreen())));

  statInfo = new QStatusBar(this);
  statInfo->setSizeGripEnabled(true);

  toolBar = new QToolBar(this);
  toolBar->setIconSize(QSize(32, 24));

  toolBar->addAction(quitThisAct);
  toolBar->addSeparator();
  toolBar->addAction(showOrigAct);
  toolBar->addAction(showBestfitAct);
  toolBar->addAction(zoomInAct);
  toolBar->addAction(zoomOutAct);
  toolBar->addSeparator();
  toolBar->addAction(rotLeftAct);
  toolBar->addAction(rotRightAct);
  toolBar->addSeparator();
  toolBar->addAction(showFullscreenAct);

  mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->addWidget(scrollArea);
  mainLayout->addWidget(toolBar);
  mainLayout->addWidget(statInfo);
  mainLayout->setEnabled(true);
  setLayout(mainLayout);

  FQ_VERIFY(connect(this, SIGNAL(viewModeChanged(v_modes)),
				imageDisplay, SLOT(displayImage(v_modes))));
  FQ_VERIFY(connect(imageDisplay, SIGNAL(imageInfoChanged(int)), this, SLOT(updateTitle(int))));
  FQ_VERIFY(connect(picFlow, SIGNAL(centerIndexChanged(int)), imageDisplay, SLOT(renderImage(int))));

  setViewMode(V_SHADOW);
}

PictureView::~PictureView() {
  delete imageDisplay;
  delete scrollArea;
  delete statInfo;
  delete toolBar;
  delete mainLayout;
}

void PictureView::viewCurrentImage() {

  activateWindow();
  raise();
  show();
  emit viewModeChanged(V_SHADOW);
}

void PictureView::viewImage(const QString &filePath, const bool RealTime) {

  iv->sortPoolDir(-1);

  if (!iv->imageDirList.contains(filePath)) {
	return;
  }

  int index = iv->imageDirList.indexOf(filePath);

  if (RealTime) {

	QFileInfo fi = iv->imageDirList.at(index);
	QString sf = fi.fileName();
	QString sa = fi.absoluteFilePath();

	QFile imgFile(sa);
	qint64 fragSize;
	QByteArray imgBytes;

	fragSize = 0;
	imgFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);

	while (fragSize <= imgFile.size()) {
	  imgBytes = imgFile.read(IMG_CHUNK);
	  if (imgBytes.size() > 0) {
		imgFile.reset();
		fragSize += imgBytes.size();
	  } // if we can't read in bytes, we do nothing.
	}

	imgFile.close();
	imgFile.setFileName(iv->shadowPath + sf);
	if (imgFile.exists()) {
	  imgFile.remove();
	}

	QPixmapCache::remove("sc-" + sf);
	QPixmapCache::remove(sf);
	QPixmap *p = iv->shadowPixmap(fi);
	emit purgeImageCache(sf);
	picFlow->insertSlide(index, *p);
  }

  iv->sortPoolDir(-1);
  picFlow->setCenterIndex(index);
}

void PictureView::updateTitle(const int index) {

  QString strTitle;

  if (iv->imageDirList.isEmpty()) {
	strTitle = tr("No image");
  } else {
	strTitle = iv->imageDirList.at(index).fileName();
  }

  setWindowTitle(strTitle);
}

void PictureView::setViewMode(const v_modes mode) {

  emit viewModeChanged(mode);
}

void PictureView::showOrig() {

  setViewMode(V_ORIGINAL);
}

void PictureView::showBestfit() {

  setViewMode(V_BESTFIT);
}

void PictureView::zoomIn() {

  setViewMode(V_ZOOMIN);
}

void PictureView::zoomOut() {

  setViewMode(V_ZOOMOUT);
}

void PictureView::rotateLeft() {

  setViewMode(V_ROTLEFT);
}

void PictureView::rotateRight() {

  setViewMode(V_ROTRIGHT);
}

void PictureView::showFullscreen() {

  const Qt::WindowStates winState = windowState() ^ Qt::WindowFullScreen;

  setWindowState(winState);

  switch (winState) {
	case Qt::WindowNoState:
	case Qt::WindowMaximized:
	  toolBar->show();
	  setBackgroundRole(QPalette::Midlight);
	  scrollArea->setBackgroundRole(QPalette::Dark);
	  imageDisplay->setBackgroundRole(QPalette::Dark);
	  setViewMode(V_SHADOW);
	  break;
	default:
	  toolBar->hide();
	  setBackgroundRole(QPalette::Shadow);
	  scrollArea->setBackgroundRole(QPalette::Shadow);
	  imageDisplay->setBackgroundRole(QPalette::Shadow);
	  setViewMode(V_FULLSCREEN);
	  break;
  }
}

void PictureView::closeThisWindow() {

  hide();
}

void PictureView::keyPressEvent(QKeyEvent *event) {

  switch (event->key()) {
	case Qt::Key_F:
	  showFullscreen();
	  break;
// may we don't need specific key shortcuts
// to do this.
//	case Qt::Key_Q:
//	  closeThisWindow();
	default:
	  lower();
	  hide();
	  iv->activateWindow();
	  iv->raise();
	  iv->show();
	  break;
  }
}

void PictureView::closeEvent(QCloseEvent *event) {

  closeThisWindow();
}


/********************************************/
/*****  ImageDisplay ************************/
/********************************************/

ImageDisplay::ImageDisplay(PictureView *parent) {

  picView = parent;
  iv = parent->iv;

  setMinimumSize(QSize(iv->frameWidth - DISP_MARGIN,
					iv->frameHeight - DISP_MARGIN));
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Dark);

  FQ_VERIFY(connect(this, SIGNAL(imageInfoChanged(int)), this, SLOT(updateImageInfo(int))));
  FQ_VERIFY(connect(this, SIGNAL(pixmapSizeChanged(int, int)), this, SLOT(updateFrameSize(int, int))));
}

ImageDisplay::~ImageDisplay() {
}

void ImageDisplay::renderImage(const int index) {

  // maybe we don't want an update while
  // iv is not visible.
  if (isVisible()) {
	updateImageCache(index);
  }

  setCurrentPixmap(iv->shadowPixmap(iv->imageDirList.at(index)));
}

void ImageDisplay::updateImageInfo(const int index) {

  QString strInfo;

  if (index < 0) {
	strInfo = tr("No image");
  } else {
	QFileInfo fi = iv->imageDirList.at(index);
	QPixmap *current = currentPixmap();
	QPixmap *original = iv->origPixmap(fi);

	strInfo = QString("%1 x %2 - %3 KB - %4%")
					.arg(original->width()).arg(original->height())
					.arg(fi.size() / 1024.0, 0, 'f', 2)
					.arg(qRound((((double)current->width() * (double)current->height()) /
						   ((double)original->width() * (double)original->height())) * 100.0));
  }

  picView->statInfo->showMessage(strInfo); 
}

QPixmap *ImageDisplay::renderPixmap(QPixmap *pixmap, const double x, const double y,
							const v_modes mode) {
  QPixmap *p;
  QMatrix matrix;

  switch (mode) {
	case V_BESTFIT:
	case V_FULLSCREEN:
	  p = new QPixmap(pixmap->scaled(x, y, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	  break;
	case V_SHADOW:
	case V_ZOOMIN:
	case V_ZOOMOUT:
	  matrix.reset();
	  matrix.scale(x, y);
	  p = new QPixmap(pixmap->transformed(matrix, Qt::SmoothTransformation));
	  break;
	case V_ROTLEFT:
	case V_ROTRIGHT:
	  matrix.reset();
	  matrix.rotate(x);
	  p = new QPixmap(pixmap->transformed(matrix, Qt::SmoothTransformation));
	  break;
	case V_UNKNOWN:
	default:
	  p = currentPixmap();
	  break;
	}

  return p;
}

void ImageDisplay::updateImageCache(const int index) {

  QPixmap *p = new QPixmap;
  QFileInfo fi = iv->imageDirList.at(index);

  if (QFile::exists(fi.absoluteFilePath())) {
	p->load(fi.absoluteFilePath(), fi.suffix().toLatin1());
	QPixmapCache::remove(fi.fileName());
	QPixmapCache::insert(fi.fileName(), *p);
  }

  if (QFile::exists(iv->shadowPath + fi.fileName())) {
	p->load(iv->shadowPath + fi.fileName(), fi.suffix().toLatin1());
	QPixmapCache::remove("sc-" + fi.fileName());
	QPixmapCache::insert("sc-" + fi.fileName(), *p);
  }

  delete p;
}

void ImageDisplay::setCurrentPixmap(QPixmap *p) {

  curPixmap = p;
}



QPixmap *ImageDisplay::currentPixmap() const {

  return curPixmap;
}

void ImageDisplay::updateFrameSize(const int width, const int height) {

  if (width > 0 && width <= 4096
	  && height > 0 && height <= 4096) {
	setMinimumSize(QSize(width, height));
  }
}

void ImageDisplay::displayImage(const v_modes mode) {

  int index;

  if ( (index = iv->currentIndex()) < 0) {
	return;
  }

  double fullX = iv->dispRect.width();
  double fullY = iv->dispRect.height();
  double x = picView->scrollArea->width();
  double y = picView->scrollArea->height();
  QPixmap *p = new QPixmap;
  QFileInfo fi = iv->imageDirList.at(index);

  switch (mode) {
	case V_SHADOW:
	  p = iv->shadowPixmap(fi);
	  setCurrentPixmap(p);
	  break;
	case V_ORIGINAL:
	  p = iv->origPixmap(fi);
	  setCurrentPixmap(p);
	  break;
	case V_FULLSCREEN:
	  p = iv->origPixmap(fi);
	  if (p->width() >= fullX || p->height() >= fullY) {
		p = renderPixmap(p, fullX - DISP_MARGIN * 3.5, fullY - DISP_MARGIN * 3.5, V_FULLSCREEN);
	  }
	  setCurrentPixmap(p);
	  break;
	case V_BESTFIT:
	  p = iv->origPixmap(fi);
	  p = renderPixmap(p, x - DISP_MARGIN, y - DISP_MARGIN, V_BESTFIT);
	  break;
	case V_ZOOMOUT:
	  p = renderPixmap(currentPixmap(), ZOOMOUT_FACTOR, ZOOMOUT_FACTOR, V_ZOOMOUT);
	  break;
	case V_ZOOMIN:
	  p = renderPixmap(currentPixmap(), ZOOMIN_FACTOR, ZOOMIN_FACTOR, V_ZOOMIN);
	  break;
	case V_ROTLEFT:
	  p = renderPixmap(currentPixmap(), ROTLEFT_DEG, ROTLEFT_DEG, V_ROTLEFT);
	  break;
	case V_ROTRIGHT:
	  p = renderPixmap(currentPixmap(), ROTRIGHT_DEG, ROTRIGHT_DEG, V_ROTRIGHT);
	  break;
	case V_UNKNOWN:
	default:
	  p = currentPixmap();
	  break;
  }

  setCurrentPixmap(p);

  if (mode != V_SHADOW) {
	emit pixmapSizeChanged(p->width(), p->height());
  } else {
	emit pixmapSizeChanged(iv->pixmapWidth, iv->pixmapHeight);
  }

  this->repaint();
  emit imageInfoChanged(index);
}

void ImageDisplay::drawImage(const QPixmap *pixmap, QPainter &painter) {

  painter.drawPixmap((width() - pixmap->width()) / 2.0,
					(height() - pixmap->height()) / 2.0, *pixmap);

}

void ImageDisplay::paintEvent(QPaintEvent *event) {

  QPixmap *p = currentPixmap();

  if (!p->isNull()) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	drawImage(p, painter);
	painter.end();
  }
}


//original image viewer
void FQTermImageOrigin::onChange(const QModelIndex & index) {
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

FQTermImageOrigin::~FQTermImageOrigin() {
  delete menuBar_;
  delete canvas_;
  delete tree_;
  delete model_;
}

FQTermImageOrigin::FQTermImageOrigin(FQTermConfig * config, QWidget *parent,
                                     Qt::WindowFlags wflag) :
FQTermImage(parent, wflag),
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
  tree_->setColumnWidth(0, 150);
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

void FQTermImageOrigin::scrollTo(const QString& filename) {
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

void FQTermImageOrigin::updateImage(const QString& filename) {
  static int i = 0;
  if (++i == 10) {
    model_->refresh(model_->index(filename));
    i = 0;
  }
  canvas_->updateImage(filename);
}

void FQTermImageOrigin::previous() {
  const QModelIndex& index = tree_->indexAbove(tree_->currentIndex());
  if (index.isValid()) {
    tree_->setCurrentIndex(index);
    canvas_->loadImage(QDir::toNativeSeparators(model_->filePath(index)));
  }
}

void FQTermImageOrigin::next() {
  const QModelIndex& index = tree_->indexBelow(tree_->currentIndex());
  if (index.isValid()) {
    tree_->setCurrentIndex(index);
    canvas_->loadImage(QDir::toNativeSeparators(model_->filePath(index)));
  }
}

void FQTermImageOrigin::adjustItemSize() {
  QFontMetrics fm(font());
  ItemDelegate::size_.setWidth(qMax(128, fm.width("WWWWWWWW.WWW")));
  ItemDelegate::size_.setHeight(fm.height() + 150);
}

void FQTermImageOrigin::selectionChanged(const QItemSelection & selected,
                                         const QItemSelection & deselected) {
                                           onChange(tree_->selectionModel()->currentIndex());
}

void FQTermImageOrigin::sortFileList(int index) {
  model_->setSorting(QDir::SortFlag(comboBox_->itemData(index).toInt()));
  QString poolPath = config_->getItemValue("preference", "pool");
  if (poolPath.isEmpty()) {
    poolPath = getPath(USER_CONFIG) + "pool/";
  }
  tree_->setRootIndex(model_->index(poolPath));
}

void FQTermImageOrigin::showFullExifInfo()
{
  QString exifInfo = QString::fromStdString(exifExtractor_->extractExifInfo(model_->filePath(tree_->currentIndex()).toLocal8Bit().data()));
  QString comment;
  if ((*exifExtractor_)["UserComment"].length() > 8) {
    QString commentEncoding = QString::fromStdString((*exifExtractor_)["UserComment"].substr(0, 8));

    if (commentEncoding.startsWith("UNICODE")) {
      //UTF-16
      QTextCodec* c = QTextCodec::codecForName("UTF-16");
      comment = c->toUnicode((*exifExtractor_)["UserComment"].substr(8).c_str());
    } else if (commentEncoding.startsWith("JIS")) {
      //JIS X 0208
      QTextCodec* c = QTextCodec::codecForName("JIS X 0208");
      comment = c->toUnicode((*exifExtractor_)["UserComment"].substr(8).c_str());
    } else {
      comment = QString::fromStdString((*exifExtractor_)["UserComment"].substr(8));
    }
  }


  QTextEdit* info = new QTextEdit;
  info->setText(exifInfo + tr("Comment : ") + comment + "\n");
  info->setWindowFlags(Qt::Dialog);
  info->setAttribute(Qt::WA_DeleteOnClose);
  info->setAttribute(Qt::WA_ShowModal);
  //  info->setLineWrapMode(QTextEdit::NoWrap);
  info->setReadOnly(true);
  QFontMetrics fm(font());
  info->resize(fm.width("Orientation : 1st row - 1st col : top - left side    "), fm.height() * 20);
  info->show();
}

void FQTermImageOrigin::adjustLayout(bool withExifTable) {
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

void FQTermImageOrigin::updateExifInfo() {
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

void FQTermImageOrigin::closeEvent( QCloseEvent *clse )
{
  hide();
  clse->ignore();
  return ;
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




FQTermImage::FQTermImage( QWidget * parent, Qt::WindowFlags f ) : QWidget(parent, f) {

}
}  // namespace FQTerm

#include "imageviewer.moc"
