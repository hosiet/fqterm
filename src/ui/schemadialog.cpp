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

#include <QFileDialog>
#include <QColorDialog>
#include <QComboBox>
#include <QMessageBox>

#include "fqterm_config.h"
#include "fqterm_path.h"
#include "fqterm_trace.h"

#include "schemadialog.h"

namespace FQTerm {

schemaDialog::schemaDialog(QWidget *parent, Qt::WFlags fl)
    : QDialog(parent, fl),
      buttonGroup_(this) {
  ui_.setupUi(this);
  buttonGroup_.addButton(ui_.noneRadioButton, 0);
  buttonGroup_.addButton(ui_.imageRadioButton, 1);
  buttonGroup_.addButton(ui_.transpRadioButton, 2);

  lastItemID_ = -1;
  isModified_ = false;

  ui_.alphaSlider->setMinimum(0);
  ui_.alphaSlider->setMaximum(100);
  ui_.alphaSlider->setSingleStep(1);
  ui_.alphaSlider->setPageStep(10);

  ui_.imagePixmapLabel->setScaledContents(false);
  // 	ui.bgButtonGroup->setRadioButtonExclusive(true);
  colorButtons[0] = ui_.clr0Button;
  colorButtons[1] = ui_.clr1Button;
  colorButtons[2] = ui_.clr2Button;
  colorButtons[3] = ui_.clr3Button;
  colorButtons[4] = ui_.clr4Button;
  colorButtons[5] = ui_.clr5Button;
  colorButtons[6] = ui_.clr6Button;
  colorButtons[7] = ui_.clr7Button;
  colorButtons[8] = ui_.clr8Button;
  colorButtons[9] = ui_.clr9Button;
  colorButtons[10] = ui_.clr10Button;
  colorButtons[11] = ui_.clr11Button;
  colorButtons[12] = ui_.clr12Button;
  colorButtons[13] = ui_.clr13Button;
  colorButtons[14] = ui_.clr14Button;
  colorButtons[15] = ui_.clr15Button;

  connectSlots();
  loadList();
}


schemaDialog::~schemaDialog(){}

void schemaDialog::setSchema(const QString &strSchemaFile) {
  if (!QFile::exists(strSchemaFile)) {
    return ;
  }

  int n = fileList_.indexOf(strSchemaFile);
  ui_.nameListWidget->setCurrentRow(n);
}

QString schemaDialog::getSchema() {
  return currentSchemaFileName_;
}

void schemaDialog::connectSlots() {
  FQ_VERIFY(connect(ui_.saveButton, SIGNAL(clicked()), this, SLOT(saveSchema())));
  FQ_VERIFY(connect(ui_.removeButton, SIGNAL(clicked()), this, SLOT(removeSchema())));
  FQ_VERIFY(connect(ui_.okButton, SIGNAL(clicked()), this, SLOT(onOK())));
  FQ_VERIFY(connect(ui_.cancelButton, SIGNAL(clicked()), this, SLOT(onCancel())));

  for (int i = 0; i < 16; ++i) {
    FQ_VERIFY(connect(colorButtons[i], SIGNAL(clicked()), this, SLOT(buttonClicked())));
  }

  FQ_VERIFY(connect(ui_.nameListWidget, SIGNAL(currentRowChanged(int)),
                  this, SLOT(nameChanged(int))));

//  FQ_VERIFY(connect(&buttonGroup_, SIGNAL(clicked(int)), this, SLOT(bgType(int))));
  FQ_VERIFY(connect(ui_.typeComboBox, SIGNAL(activated(int)), this, SLOT(imageType(int))));
  FQ_VERIFY(connect(ui_.chooseButton, SIGNAL(clicked()), this, SLOT(chooseImage())));
  FQ_VERIFY(connect(ui_.fadeButton, SIGNAL(clicked()), this, SLOT(fadeClicked())));
  FQ_VERIFY(connect(ui_.alphaSlider, SIGNAL(valueChanged(int)), this, SLOT(alphaChanged(int))));

  FQ_VERIFY(connect(ui_.titleLineEdit, SIGNAL(textChanged(const QString &)),
                  this, SLOT(textChanged(const QString &))));
  FQ_VERIFY(connect(ui_.imageLineEdit, SIGNAL(textChanged(const QString &)),
                  this, SLOT(textChanged(const QString &))));
}

void schemaDialog::loadList() {
  QDir dir;
  QFileInfoList lstFile;
  //QFileInfo *fi;

  dir.setNameFilters(QStringList("*.schema"));
  dir.setPath(getPath(RESOURCE) + "schema");
  lstFile = dir.entryInfoList();

  //if(lstFile != NULL)
  {
    foreach(QFileInfo fi, lstFile) {
      FQTermConfig *pConf = new FQTermConfig(fi.absoluteFilePath());
      ui_.nameListWidget->addItem(pConf->getItemValue("schema", "title"));
      delete pConf;
      fileList_.append(fi.absoluteFilePath());
    }
  }
  if (ui_.nameListWidget->count() != 0) {
    ui_.nameListWidget->setCurrentRow(0);
  }
}

void schemaDialog::loadSchema(const QString &strSchemaFile) {
  FQTermConfig *pConf = new FQTermConfig(strSchemaFile);

  currentSchemaFileName_ = strSchemaFile;

  title_ = pConf->getItemValue("schema", "title");
  backgroundFileName_ = pConf->getItemValue("image", "name");
  QString strTmp = pConf->getItemValue("image", "type");
  backgroundType_ = strTmp.toInt();
  fade_.setNamedColor(pConf->getItemValue("image", "fade"));
  strTmp = pConf->getItemValue("image", "alpha");
  alpha_ = strTmp.toFloat();

  for (int i = 0; i < 16; ++i) {
    colors[i].setNamedColor(pConf->getItemValue("color", QString("color%1").arg(i)));
  }

  delete pConf;

  updateView();

}

void schemaDialog::saveNumSchema(int n) {
  // FIXME: ?, and there is more below
  QStringList::Iterator it = fileList_.begin();
  while (n--) {
    it++;
  }

  title_ = ui_.titleLineEdit->text();
  backgroundFileName_ = ui_.imageLineEdit->text();

  QString schemaFileName = getPath(RESOURCE) + "schema/" + title_ + ".schema";

  // create a new schema if title changed
  if (schemaFileName != currentSchemaFileName_) {
    ui_.nameListWidget->addItem(title_);
    fileList_.append(schemaFileName);
  }

  FQTermConfig *pConf = new FQTermConfig(currentSchemaFileName_);

  currentSchemaFileName_ = schemaFileName;


  pConf->setItemValue("schema", "title", title_);

  pConf->setItemValue("image", "name", backgroundFileName_);

  QString strTmp;
  strTmp.setNum(backgroundType_);
  pConf->setItemValue("image", "type", strTmp);

  pConf->setItemValue("image", "fade", fade_.name());

  strTmp.setNum(alpha_);
  pConf->setItemValue("image", "alpha", strTmp);
  for (int i = 0; i < 16; ++i) {
    pConf->setItemValue("color", QString("color%1").arg(i), colors[i].name());
  }

  pConf->save(schemaFileName);

  delete pConf;

  isModified_ = false;

}

void schemaDialog::updateView() {
  // title
  ui_.titleLineEdit->setText(title_);
  for (int i = 0; i < 16; ++i) {
    QPalette palette;
    palette.setColor(QPalette::Button, colors[i]);
    colorButtons[i]->setPalette(palette);
  }

  // bg type
  switch (backgroundType_) {
    case 0:
      // none
      buttonGroup_.button(0)->setChecked(true);
      bgType(2);
      break;
    case 1:
      // transparent
      buttonGroup_.button(2)->setChecked(true);
      bgType(3);
      break;
    case 2:
      // tile
      buttonGroup_.button(1)->setChecked(true);
      bgType(1);
      break;
    case 3:
      // center
      buttonGroup_.button(1)->setChecked(true);
      bgType(1);
      break;
    case 4:
      // stretch
      buttonGroup_.button(1)->setChecked(true);
      bgType(1);
      break;
    default:
      buttonGroup_.button(0)->setChecked(true);
      break;
  }
  // image type

  // image file
  ui_.imageLineEdit->setText(backgroundFileName_);
  // fade color
  QPalette palette;
  palette.setColor(ui_.fadeButton->backgroundRole(), fade_);
  ui_.fadeButton->setPalette(palette);

  // alpha
  ui_.alphaSlider->setValue(int(alpha_ * 100));

  // load from file, nothing changed
  isModified_ = false;
}

void schemaDialog::updateBgPreview() {
  QPalette palette;
  palette.setColor(ui_.imagePixmapLabel->backgroundRole(), colors[0]);
  ui_.imagePixmapLabel->setPalette(palette);

  // 	ui.imagePixmapLabel->setPaletteBackgroundColor(clr0);
  ui_.imagePixmapLabel->clear();
  if (!QFile::exists(backgroundFileName_) || backgroundType_ == 0) {
    return ;
  }

  QPixmap pixmap;
  QImage img(backgroundFileName_);
  img = fadeColor(img, alpha_, fade_);
  pixmap = QPixmap::fromImage(img.scaled(ui_.imagePixmapLabel->width(),
                                         ui_.imagePixmapLabel->height()));
  /*
	switch(type)
	{
	case 2:	// tile
	if( !pixmap.isNull() )
	{
	imagePixmapLabel->setPixmap( pixmap );
	break;
	}
	case 3:	// center
	if( !pixmap.isNull() )
	{
	QPixmap pxm;
	pxm.resize(size());
	pxm.fill( clr0 );
	bitBlt( &pxm,
	( size().width() - pixmap.width() ) / 2,
	( size().height() - pixmap.height() ) / 2,
	&pixmap, 0, 0,
	pixmap.width(), pixmap.height() );
	imagePixmapLabel->setPixmap(pxm);
	break;
	}
	case 4:	// stretch
	if( !pixmap.isNull() )
	{
	float sx = (float)size().width() / pixmap.width();
	float sy = (float)size().height() /pixmap.height();
	QWMatrix matrix;
	matrix.scale( sx, sy );
	imagePixmapLabel->setPixmap(pixmap.xForm( matrix ));
	break;
	}
	}
  */
  ui_.imagePixmapLabel->setPixmap(pixmap);

}

void schemaDialog::buttonClicked() {
  QPushButton *button = (QPushButton*)sender();
  QColor color =
      QColorDialog::getColor(button->palette().color(button->backgroundRole()));
  if (color.isValid() == true) {
    QPalette palette;
    palette.setColor(QPalette::Button, color);
    button->setPalette(palette);
    isModified_ = true;
  }
  for (int i = 0; i < 16; ++i) {
    if (colorButtons[i] == button) {
      colors[i] = color;
      break;
    }
  }
}

void schemaDialog::nameChanged(int item) {
  if (isModified_) {
    QMessageBox mb("FQTerm", "Setting changed, do you want to save?",
                   QMessageBox::Warning, QMessageBox::Yes | QMessageBox::Default,
                   QMessageBox::No | QMessageBox::Escape, 0, this);
    if (mb.exec() == QMessageBox::Yes) {
      if (lastItemID_ != -1) {
        saveNumSchema(lastItemID_);
      }
    }
  }

  QStringList::Iterator it = fileList_.begin();
  int n = item; //nameListBox->index(item);
  if (n != -1) {
    lastItemID_ = n;
    while (n--) {
      it++;
    }
  }


  loadSchema(*it);
  updateView();
}

void schemaDialog::bgType(int n) {
  switch (n) {
    case 1:
      // image
      // 			typeComboBox->setEnabled(true);
      // 			imageLineEdit->setEnabled(true);
      // 			chooseButton->setEnabled(true);
      if (backgroundType_ == 0) {
        backgroundType_ = 2;
      }
      ui_.typeComboBox->setCurrentIndex(backgroundType_ - 2);
      break;

    case 2:
      // none
      // 			typeComboBox->setEnabled(false);
      // 			imageLineEdit->setEnabled(false);
      // 			chooseButton->setEnabled(false);
      backgroundType_ = 0;
      break;
    case 3:
      // transparent
      QMessageBox::information(this, "sorry",
                               "We are trying to bring you this function soon :)");
      //			typeComboBox->setEnabled(false);
      //			imageLineEdit->setEnabled(false);
      //			chooseButton->setEnabled(false);
      //			backgroundType_ = 1;
      // 			bgButtonGroup->setButton(2);
      buttonGroup_.button(2)->setChecked(true);
      break;
  }
  isModified_ = true;
  updateBgPreview();
}

void schemaDialog::imageType(int n) {
  backgroundType_ = n + 2;
  isModified_ = true;
  updateBgPreview();
}

void schemaDialog::chooseImage() {
  QString img = QFileDialog::getOpenFileName(
      this, "Choose an image", QDir::currentPath());
  if (!img.isNull()) {
    ui_.imageLineEdit->setText(img);
    backgroundFileName_ = img;
    backgroundType_ = 2+ui_.typeComboBox->currentIndex();
    isModified_ = true;
    updateBgPreview();
  }
}

void schemaDialog::fadeClicked() {
  QColor color = QColorDialog::getColor(fade_);
  if (color.isValid() == TRUE) {
    fade_ = color;
#if (QT_VERSION>300)
    QPalette palette;
    palette.setColor(ui_.fadeButton->backgroundRole(), color);
    ui_.fadeButton->setPalette(palette);

    // 		ui.fadeButton->setPaletteBackgroundColor(color);
#else
    ui_.fadeButton->setPalette(color);
#endif

    isModified_ = true;
    updateBgPreview();
  }
}

void schemaDialog::alphaChanged(int value) {
  alpha_ = float(value) / 100;
  isModified_ = true;
  updateBgPreview();
}

void schemaDialog::saveSchema() {
  // get current schema file name
  int n = ui_.nameListWidget->currentRow();
  saveNumSchema(n);
}

void schemaDialog::removeSchema() {
  QFileInfo fi(currentSchemaFileName_);
  if (fi.isWritable()) {
    QFile::remove(currentSchemaFileName_);
    QStringList::Iterator it = fileList_.begin();
    int n = ui_.nameListWidget->currentRow();
    ui_.nameListWidget->takeItem(n);
    while (n--) {
      it++;
    }

    fileList_.erase(it);
  } else {
    QMessageBox::warning(this, "Error",
                         "This is a system schema. Permission Denied");
  }
}

void schemaDialog::onOK() {
  if (isModified_) {
    QMessageBox mb("FQTerm", "Setting changed, do you want to save?",
                   QMessageBox::Warning, QMessageBox::Yes | QMessageBox::Default,
                   QMessageBox::No | QMessageBox::Escape, 0, this);
    if (mb.exec() == QMessageBox::Yes) {
      int n = ui_.nameListWidget->currentRow();
      saveNumSchema(n);
    }
  }

  done(1);
}

void schemaDialog::onCancel() {
  if (isModified_) {
    QMessageBox mb("FQTerm", "Setting changed, do you want to save?",
                   QMessageBox::Warning, QMessageBox::Yes | QMessageBox::Default,
                   QMessageBox::No | QMessageBox::Escape, 0, this);
    if (mb.exec() == QMessageBox::Yes) {
      int n = ui_.nameListWidget->currentRow();
      saveNumSchema(n);
    }
  }

  done(0);
}

void schemaDialog::textChanged(const QString &) {
  isModified_ = true;
}

// from KImageEffect::fade
QImage &schemaDialog::fadeColor(QImage &img, float val, const QColor &color) {
  if (img.width() == 0 || img.height() == 0) {
    return img;
  }

  // We don't handle bitmaps
  if (img.depth() == 1) {
    return img;
  }

  unsigned char tbl[256];
  for (int i = 0; i < 256; i++) {
    tbl[i] = (int)(val *i + 0.5);
  }

  int red = color.red();
  int green = color.green();
  int blue = color.blue();

  QRgb col;
  int r, g, b, cr, cg, cb;

  if (img.depth() <= 8) {
    // pseudo color
    for (int i = 0; i < img.numColors(); i++) {
      col = img.color(i);
      cr = qRed(col);
      cg = qGreen(col);
      cb = qBlue(col);
      if (cr > red) {
        r = cr - tbl[cr - red];
      } else {
        r = cr + tbl[red - cr];
      }

      if (cg > green) {
        g = cg - tbl[cg - green];
      } else {
        g = cg + tbl[green - cg];
      }

      if (cb > blue) {
        b = cb - tbl[cb - blue];
      } else {
        b = cb + tbl[blue - cb];
      }

      img.setColor(i, qRgba(r, g, b, qAlpha(col)));
    }

  } else {
    // truecolor
    for (int y = 0; y < img.height(); y++) {
      QRgb *data = (QRgb*)img.scanLine(y);
      for (int x = 0; x < img.width(); x++) {
        col =  *data;
        cr = qRed(col);
        cg = qGreen(col);
        cb = qBlue(col);
        if (cr > Qt::red) {
          r = cr - tbl[cr - Qt::red];
        } else {
          r = cr + tbl[Qt::red - cr];
        }

        if (cg > Qt::green) {
          g = cg - tbl[cg - Qt::green];
        } else {
          g = cg + tbl[Qt::green - cg];
        }

        if (cb > Qt::blue) {
          b = cb - tbl[cb - Qt::blue];
        } else {
          b = cb + tbl[Qt::blue - cb];
        }

        *data++ = qRgba(r, g, b, qAlpha(col));
      }
    }
  }

  return img;
}

}  // namespace FQTerm

#include "schemadialog.moc"
