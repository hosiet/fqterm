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

#include <stdio.h>
#include <stdlib.h>

#include <QApplication>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCharFormat>
#include <QTimer>
#include <QWheelEvent>
#include <QKeySequence>

#include "fqterm_buffer.h"
#include "fqterm_config.h"
#include "fqterm_convert.h"
#include "fqterm_frame.h"
#include "fqterm_param.h"
#include "fqterm_screen.h"
#include "fqterm_session.h"
#include "fqterm_text_line.h"
#include "fqterm_window.h"
#include "fqterm_wndmgr.h"

namespace FQTerm {
/* ------------------------------------------------------------------------ */
/*                                                                          */
/*                            Constructor/Destructor                        */
/*                                                                          */
/* ------------------------------------------------------------------------ */

FQTermScreen::FQTermScreen(QWidget *parent, FQTermParam *param, FQTermSession *session)
    : QWidget(parent),
      scrollBarWidth_(15),
      termBuffer_(session->getBuffer()){
  termWindow_ = (FQTermWindow*)parent;
  session_ = session;
  param_ = param;
  paintState_ = System;
  isCursorShown_ = true;
  is_light_background_mode_ = false;

  setFocusPolicy(Qt::ClickFocus);
  setAttribute(Qt::WA_InputMethodEnabled, true);
  setMouseTracking(true);

  setSchema();

  initFontMetrics();

  FQ_VERIFY(connect(termBuffer_, SIGNAL(bufferSizeChanged()),
                    this, SLOT(bufferSizeChanged())));

  FQ_VERIFY(connect(termBuffer_, SIGNAL(termSizeChanged(int, int)),
                    this, SLOT(termSizeChanged(int, int))));

  blinkTimer_ = new QTimer(this);
  FQ_VERIFY(connect(blinkTimer_, SIGNAL(timeout()), this, SLOT(blinkEvent())));

  cursorTimer_ = new QTimer(this);
  FQ_VERIFY(connect(cursorTimer_, SIGNAL(timeout()),
                    this, SLOT(cursorEvent())));

  // the scrollbar
  scrollBar_ = new QScrollBar(this);
  scrollBar_->setCursor(Qt::ArrowCursor);
  bufferStart_ = 0;
  bufferEnd_ = termBuffer_->getNumRows() - 1;
  areLinesBlink_ = new bool[bufferEnd_ - bufferStart_ + 1];
  scrollBar_->setRange(0, 0);
  scrollBar_->setSingleStep(1);
  scrollBar_->setPageStep(termBuffer_->getNumRows());
  scrollBar_->setValue(0);
  updateScrollBar();
  FQ_VERIFY(connect(scrollBar_, SIGNAL(valueChanged(int)),
                    this, SLOT(scrollChanged(int))));

  gotoPrevPage_ = new QShortcut(QKeySequence(tr("Shift+PageUp")),
                                this, SLOT(prevPage()));
  gotoNextPage_ = new QShortcut(QKeySequence(tr("Shift+PageDown")),
                                this, SLOT(nextPage()));
  gotoPrevLine_ = new QShortcut(QKeySequence(tr("Shift+Up")),
                                this, SLOT(prevLine()));
  gotoNextLine_ = new QShortcut(QKeySequence(tr("Shift+Down")),
                                this, SLOT(nextLine()));

  setAttribute(Qt::WA_OpaquePaintEvent, true);

  // init variable
  isBlinkScreen_ = false;
  isBlinkCursor_ = true;

  if (param_->dispEncodingID_ == 0) {
    encoding_ = QTextCodec::codecForName("GBK");
  } else {
    encoding_ = QTextCodec::codecForName("Big5");
  }

  preedit_line_ = new PreeditLine(this, colors_);

  tmp_im_query_ = new QString();
}

FQTermScreen::~FQTermScreen() {
  delete [] areLinesBlink_;
  delete blinkTimer_;
  delete cursorTimer_;
  //delete m_pCanvas;
  //delete m_inputContent;

  delete preedit_line_;

  delete tmp_im_query_;
}

bool FQTermScreen::event(QEvent *e) {
  if (e->type() == QEvent::KeyPress) {
    // forward all key press events to parant (FQTermWindow).
    return false;
  }

  return this->QWidget::event(e);
}

// focus event received
void FQTermScreen::focusInEvent(QFocusEvent*) {

  if (termWindow_->isMaximized() &&
      termWindow_->frame_->windowManager_->afterRemove()) {
    termWindow_->showNormal();
    termWindow_->showMaximized();
  }

  termWindow_->frame_->windowManager_->activateTheTab(termWindow_);

  gotoPrevPage_->setEnabled(true);
  gotoNextPage_->setEnabled(true);
  gotoPrevLine_->setEnabled(true);
  gotoNextLine_->setEnabled(true);

}

// focus out
void FQTermScreen::focusOutEvent(QFocusEvent*) {
  gotoPrevPage_->setEnabled(false);
  gotoNextPage_->setEnabled(false);
  gotoPrevLine_->setEnabled(false);
  gotoNextLine_->setEnabled(false);
}

void FQTermScreen::resizeEvent(QResizeEvent*) {
  syncBufferAndScreen();
}

void FQTermScreen::syncBufferAndScreen() {
  updateScrollBar();
  setBackgroundPixmap(backgroundPixmap_, backgroundPixmapType_);

  if (param_->isFontAutoFit_) {
    updateFont();
  } else {
    int cx = clientRectangle_.width() / charWidth_;
    int cy = clientRectangle_.height() / charHeight_;
    if (cx > 10 && cy > 10) {
      session_->setTermSize(cx, cy);
    }
  }
}

/* ------------------------------------------------------------------------ */
/*	                                                                        */
/* 	                         Mouse                                          */
/*                                                                          */
/* ------------------------------------------------------------------------ */
void FQTermScreen::enterEvent(QEvent *e) {
  QApplication::sendEvent(termWindow_, e);
}

void FQTermScreen::leaveEvent(QEvent *e) {
  QApplication::sendEvent(termWindow_, e);
}

void FQTermScreen::mousePressEvent(QMouseEvent *me) {
  
  termWindow_->mousePressEvent(me);
  setFocus();
}

void FQTermScreen::mouseMoveEvent(QMouseEvent *me) {
#ifdef Q_OS_MACX
  termWindow_->mouseMoveEvent(me);
#else
  termWindow_->mouseMoveEvent(me);
  //QApplication::sendEvent(m_pWindow, me);
#endif
}

void FQTermScreen::mouseReleaseEvent(QMouseEvent *me) {
  termWindow_->mouseReleaseEvent(me);
  //QApplication::sendEvent(m_pWindow, me);
}

void FQTermScreen::wheelEvent(QWheelEvent *we) {
  if (termWindow_->frame_->preference_.isWheelSupported_) {
    QApplication::sendEvent(termWindow_, we);
  } else {
    QApplication::sendEvent(scrollBar_, we);
  }
}


/* ------------------------------------------------------------------------ */
/*	                                                                        */
/* 	                                Font                                    */
/*                                                                          */
/* ------------------------------------------------------------------------ */

void FQTermScreen::initFontMetrics() {
  //issue 98
  if (param_->isFontAutoFit_) {
    englishFont_ = new QFont(param_->englishFontName_);
    nonEnglishFont_ = new QFont(param_->nonEnglishFontName_);

    updateFont();
  } else {
    englishFont_ = new QFont(param_->englishFontName_, qMax(8, param_->englishFontSize_));
    nonEnglishFont_ = new QFont(param_->nonEnglishFontName_, qMax(8, param_->nonEnglishFontSize_));

    setFontMetrics();
  }

  englishFont_->setWeight(QFont::Normal);;
  nonEnglishFont_->setWeight(QFont::Normal);

  // m_pFont->setStyleHint(QFont::System,
  //  m_pWindow->m_pFrame->m_pref.bAA ?
  //    QFont::PreferAntialias : QFont::NoAntialias);
}

void FQTermScreen::updateFont() {
  int nPixelSize;
  int nIniSize =
      qMax(8, qMin(clientRectangle_.height() / termBuffer_->getNumRows(),
                   clientRectangle_.width() *2 / termBuffer_->getNumColumns()));

  for (nPixelSize = nIniSize - 3; nPixelSize <= nIniSize + 3; nPixelSize++) {
    englishFont_->setPixelSize(nPixelSize);
    nonEnglishFont_->setPixelSize(nPixelSize);

    setFontMetrics();
    if ((termBuffer_->getNumRows() *charHeight_) > clientRectangle_.height() ||
        (termBuffer_->getNumColumns() *charWidth_) > clientRectangle_.width()) {
      while (nPixelSize > 5) {
        nPixelSize--;
        englishFont_->setPixelSize(nPixelSize);
        nonEnglishFont_->setPixelSize(nPixelSize);

        setFontMetrics();
        //changed by dp to get larger font...
        if ((termBuffer_->getNumRows()*charHeight_) <= clientRectangle_.height()
            && (termBuffer_->getNumColumns() *charWidth_) <=
            clientRectangle_.width()) {
          break;
        }
      }
      break;
    }
  }
  
  englishFont_->setWeight(QFont::Normal);
  nonEnglishFont_->setWeight(QFont::Normal);
  // m_pFont->setStyleHint(QFont::System,
  // m_pWindow->m_pFrame->m_pref.bAA ?
  //  QFont::PreferAntialias : QFont::NoAntialias);
}

void FQTermScreen::setFontMetrics() {
  QFontMetrics nonEnglishFM(*nonEnglishFont_);
  QFontMetrics englishFM(*englishFont_);

  // FIXME: find a typical character for the current language.
  float cn = nonEnglishFM.width(QChar(0x4e2D));
  float en = englishFM.width('W');

  int pix_size = englishFont_->pixelSize();
  while (2 * en > cn && pix_size > 5) {
	  --pix_size;
	  englishFont_->setPixelSize(pix_size);
	  englishFM = QFontMetrics(*englishFont_);
	  en = englishFM.width('W');	 
  }
  
  if (en / cn <= 0.7) {
	// almost half
    charWidth_ = qMax(int((cn + 1) / 2), int(en));
  } else {
    charWidth_ = (qMax(int(en), int(cn)) + 1) / 2;
  }

  charHeight_ = qMax(englishFM.height(), nonEnglishFM.height());
  fontAscent_ = qMax(englishFM.ascent(), nonEnglishFM.ascent());
  fontDescent_ = qMax(englishFM.descent(), nonEnglishFM.descent());

  FQ_TRACE("font", 4) << "\nNon-English font:"
                      << "\nheight: " << nonEnglishFM.height()
                      << "\nascent: " << nonEnglishFM.ascent()
                      << "\ndescent: " << nonEnglishFM.descent();
                      

  FQ_TRACE("font", 4) << "\nEnglish font:"
                      << "\nheight: " << englishFM.height()
                      << "\nascent: " << englishFM.ascent()
                      << "\ndescent: " << englishFM.descent();
}

/* ------------------------------------------------------------------------ */
/*	                                                                        */
/* 	                               Colors                                   */
/*	                                                                        */
/* ------------------------------------------------------------------------ */
void FQTermScreen::setSchema() {
  // the default color table
  colors_[0] = Qt::black;
  colors_[1] = Qt::darkRed;
  colors_[2] = Qt::darkGreen;
  colors_[3] = Qt::darkYellow;
  colors_[4] = Qt::darkBlue;
  colors_[5] = Qt::darkMagenta;
  colors_[6] = Qt::darkCyan;
  colors_[7] = Qt::darkGray;
  colors_[8] = Qt::gray;
  colors_[9] = Qt::red;
  colors_[10] = Qt::green;
  colors_[11] = Qt::yellow;
  colors_[12] = Qt::blue;
  colors_[13] = Qt::magenta;
  colors_[14] = Qt::cyan;
  colors_[15] = Qt::white;

  backgroundPixmapType_ = 0;

  // if we have schema defined
  if (QFile::exists(param_->schemaFileName_)) {
    FQTermConfig *pConf = new FQTermConfig(param_->schemaFileName_);
    for (int i = 0; i < 16; ++i) {
      colors_[i].setNamedColor(pConf->getItemValue("color",
                                                   QString("color%1").arg(i)));
    }

    // bg type
    QString strTmp = pConf->getItemValue("image", "type");
    backgroundPixmapType_ = strTmp.toInt();

    // fade effect
    QColor fadecolor;
    fadecolor.setNamedColor(pConf->getItemValue("image", "fade"));
    strTmp = pConf->getItemValue("image", "alpha");
    float alpha = strTmp.toFloat();

    // get the image name
    if (QFile::exists(pConf->getItemValue("image", "name"))
        && backgroundPixmapType_ > 1) { // valid image name and type
      backgroundPixmap_ = QPixmap(pConf->getItemValue("image", "name"));
      QImage ima(backgroundPixmap_.toImage());
      ima = fade(ima, alpha, fadecolor);
      backgroundPixmap_ = QPixmap::fromImage(ima);

    }
    delete pConf;
  }

  // override schema using user defined Fg/Bg color
  colors_[0] = param_->backgroundColor_;
  colors_[7] = param_->foregroundColor_;
}

/* ------------------------------------------------------------------------ */
/*	                                                                        */
/*                  	Scrollbar                                           */
/*                                                                          */
/* ------------------------------------------------------------------------*/
void FQTermScreen::prevPage() {
  scrollLine(-termBuffer_->getNumRows());
  setPaintState(NewData);
  update();
}

void FQTermScreen::nextPage() {
  scrollLine(termBuffer_->getNumRows());
  setPaintState(NewData);
  update();
}

void FQTermScreen::prevLine() {
  scrollLine(-1);
  setPaintState(NewData);
  update();
}

void FQTermScreen::nextLine() {
  scrollLine(1);
  setPaintState(NewData);
  update();
}

void FQTermScreen::scrollLine(int delta) {
  bufferStart_ += delta;

  if (bufferStart_ < 0) {
    bufferStart_ = 0;
    return ;
  }
  if (bufferStart_ > termBuffer_->getNumLines() - termBuffer_->getNumRows()) {
    bufferStart_ = termBuffer_->getNumLines() - termBuffer_->getNumRows();
    return ;
  }

  scrollBar_->setValue(bufferStart_);
  bufferEnd_ = bufferStart_ + termBuffer_->getNumRows() - 1;

  // notify session
  session_->setScreenStart(bufferStart_);

  for (int i = bufferStart_; i <= bufferEnd_; i++) {
    session_->setLineAllChanged(i);
  }
}

void FQTermScreen::scrollChanged(int value) {
  if (bufferStart_ == value) {
    return ;
  }

  if (value < 0) {
    value = 0;
  }
  if (value > termBuffer_->getNumLines() - termBuffer_->getNumRows()) {
    value = termBuffer_->getNumLines() - termBuffer_->getNumRows();
  }

  bufferStart_ = value;
  bufferEnd_ = value + termBuffer_->getNumRows() - 1;

  // notify session
  session_->setScreenStart(bufferStart_);

  for (int i = bufferStart_; i <= bufferEnd_; i++) {
    //termBuffer_->at(i)->setChanged(-1, -1);
    session_->setLineAllChanged(i);
  }

  setPaintState(NewData);
  update();
}

void FQTermScreen::updateScrollBar() {
  int numLeftPixels = 0; // ×ó±ß¾à
  int numUpPixels = 0; // ÉÏ±ß¾à

  switch (termWindow_->frame_->termScrollBarPosition_) {
    case 0:
      scrollBar_->hide();
      clientRectangle_ = QRect(numLeftPixels, numUpPixels,
                               rect().width() - numLeftPixels,
                               rect().height() - numUpPixels);
      break;
    case 1:
      // LEFT
      scrollBar_->setGeometry(0, 0, scrollBarWidth_, rect().height());
      scrollBar_->show();
      clientRectangle_ = QRect(scrollBarWidth_ + numLeftPixels, numUpPixels,
                               rect().width() - scrollBarWidth_ - numLeftPixels,
                               rect().height() - numUpPixels);
      break;
    case 2:
      // RIGHT
      scrollBar_->setGeometry(rect().width() - scrollBarWidth_, 0,
                              scrollBarWidth_, rect().height());
      scrollBar_->show();
      clientRectangle_ = QRect(numLeftPixels, numUpPixels,
                               rect().width() - scrollBarWidth_ - numLeftPixels,
                               rect().height() - numUpPixels);
      break;
  }
  setPaintState(Repaint);
  update();
}

void FQTermScreen::bufferSizeChanged() {
  FQ_VERIFY(disconnect(scrollBar_, SIGNAL(valueChanged(int)),
                       this, SLOT(scrollChanged(int))));
  scrollBar_->setRange(0, termBuffer_->getNumLines() - termBuffer_->getNumRows());
  scrollBar_->setSingleStep(1);
  scrollBar_->setPageStep(termBuffer_->getNumRows());

  // FIXME: should not always move scroll bar to the bottom when
  // buffer size is changed.
  scrollBar_->setValue(termBuffer_->getNumLines() - termBuffer_->getNumRows());

  bufferStart_ = scrollBar_->value();
  bufferEnd_ = scrollBar_->value() + termBuffer_->getNumRows() - 1;

  // notify session
  session_->setScreenStart(bufferStart_);

  FQ_VERIFY(connect(scrollBar_, SIGNAL(valueChanged(int)),
                    this, SLOT(scrollChanged(int))));

  delete [] areLinesBlink_;
  areLinesBlink_ = new bool[bufferEnd_ - bufferStart_ + 1];

  scrollLine(0);
}

void FQTermScreen::termSizeChanged(int column, int row) {
  FQ_TRACE("term", 0) << "The term size is changed to " << column << "x" << "row";
  syncBufferAndScreen();
  bufferSizeChanged();
  this->setPaintState(Repaint);
  this->repaint();
}

/* ------------------------------------------------------------------------ */
/*                                                                          */
/* 	                          Display	                                    */
/*                                                                          */
/* ------------------------------------------------------------------------ */

//set pixmap background

void FQTermScreen::setBackgroundPixmap(const QPixmap &pixmap, int nType) {
  hasBackground_ = false;
  backgroundPixmap_ = pixmap;
  backgroundPixmapType_ = nType;
  QPalette palette;

  switch (nType) {
    case 0:
      // none
      palette.setColor(backgroundRole(), colors_[0]);
      setPalette(palette);
      break;
    case 1:
      //{ // transparent{}
      break;
    case 2:
      // tile
      if (!pixmap.isNull())
        palette.setBrush(backgroundRole(), QBrush(pixmap));
      setPalette(palette);
      // 				setBackgroundPixmap( pixmap );
      hasBackground_ = true;
      break;
      //}
    case 3:
      // center
      if (!pixmap.isNull()) {
        QPixmap pxmBg = QPixmap(size());
        QPainter painter(&pxmBg);
        pxmBg.fill(colors_[0]);
        painter.drawPixmap((size().width() - pixmap.width()) / 2,
                           (size().height() - pixmap.height()) / 2,
                           pixmap.width(), pixmap.height(), pixmap);
        palette.setBrush(backgroundRole(), QBrush(pxmBg));
        setPalette(palette);
        // 				setBackgroundPixmap(pxmBg);
        hasBackground_ = true;
        break;
      }
    case 4:
      // stretch
      if (!pixmap.isNull()) {
        float sx = (float)size().width() / pixmap.width();
        float sy = (float)size().height() / pixmap.height();
        QMatrix matrix;
        matrix.scale(sx, sy);
        palette.setBrush(backgroundRole(), QBrush(pixmap.transformed(matrix)));
        setPalette(palette);
        // 				setBackgroundPixmap(pixmap.transformed( matrix ));
        hasBackground_ = true;
        break;
      }
    default:
      palette.setColor(backgroundRole(), colors_[0]);
      setPalette(palette);
      // 			setBackgroundColor( m_color[0] );

  }
}

void FQTermScreen::cursorEvent() {
  if (isBlinkCursor_) {
    setPaintState(Cursor);
    isCursorShown_ = !isCursorShown_;
    update();
  }
}

void FQTermScreen::blinkEvent() {
  if (hasBlinkText_) {
    isBlinkScreen_ = !isBlinkScreen_;
    setPaintState(Blink);
    update();
  }
}

void FQTermScreen::paintEvent(QPaintEvent *pe) {
  QPainter painter(this);

  if (paintState_ == System) {
    repaintScreen(pe, painter);
    return;
  }

  if (testPaintState(Repaint)) {
      repaintScreen(pe, painter);
      clearPaintState(Repaint);
  }

  if (testPaintState(NewData)) {
      if (termBuffer_->isLightBackgroundMode() != is_light_background_mode_) {
        is_light_background_mode_ = termBuffer_->isLightBackgroundMode();
        if (is_light_background_mode_) {
          colors_[0] = param_->foregroundColor_;
          colors_[7] = param_->backgroundColor_;
        } else {
          colors_[0] = param_->backgroundColor_;
          colors_[7] = param_->foregroundColor_;
        }
        repaintScreen(pe, painter);
      } else {      
        refreshScreen(painter);
      }

      clearPaintState(NewData);
  }

  if (testPaintState(Blink)) {
      blinkScreen(painter);
      clearPaintState(Blink);
  }

  if (testPaintState(Cursor)) {
      updateCursor(painter);
      clearPaintState(Cursor);
  }

}

void FQTermScreen::blinkScreen(QPainter &painter) {
  painter.setBackground(QBrush(colors_[0]));

  for (int index = bufferStart_; index <= bufferEnd_; index++) {
    if (areLinesBlink_[index - bufferStart_]) {
      const FQTermTextLine *pTextLine = termBuffer_->getTextLineInBuffer(index);
      uint linelength = pTextLine->getWidth();
      const unsigned char *attr = pTextLine->getAttributes();
      for (uint i = 0; i < linelength; ++i) {
        if (GETBLINK(attr[i])) {
          int startx = i;
          while (i < linelength && GETBLINK(attr[i])) {
            ++i;
          }
          drawLine(painter, index, startx, i, false);
        }
      }
    }
  }
}

void FQTermScreen::updateCursor(QPainter &painter) {
  if (termBuffer_->getCaretColumn() >= termBuffer_->getNumColumns()) {
    // we only allow the cursor located beyond the terminal screen temporarily.
    // just ignore this.
    return;
  }

  bool isCursorShown = isCursorShown_;
  if (termWindow_->frame_->isBossColor_) {
    isCursorShown = true;
  }

  if (termBuffer_->getCaretLine() <= bufferEnd_
      && termBuffer_->getCaretLine() >= bufferStart_) {

    if (!isCursorShown) {
      const FQTermTextLine *pTextLine
          = termBuffer_->getTextLineInBuffer(termBuffer_->getCaretLine());
      if ((int)pTextLine->getWidth() > termBuffer_->getCaretColumn()) {
        int startx = pTextLine->getCellBegin(termBuffer_->getCaretColumn());
        int endx = pTextLine->getCellEnd(termBuffer_->getCaretColumn() + 1);
        if ((int)pTextLine->getWidth() > endx) {
          endx = pTextLine->getCellEnd(endx + 1);
        }
        drawLine(painter, termBuffer_->getCaretLine(), startx, endx,
                 termBuffer_->getCaretLine());
      } else {
        painter.fillRect(mapToRect(termBuffer_->getCaretColumn(),
                                   termBuffer_->getCaretLine(), 1, 1),
                         colors_[0]);
      }
      return;
    }

    QPoint pt = mapToPixel(QPoint(termBuffer_->getCaretColumn(),
                                  termBuffer_->getCaretLine()));
    switch (param_->cursorType_) {
      case 0:
        // block
        {
          int x = termBuffer_->getCaretColumn();
          int y = termBuffer_->getCaretLine();
          const FQTermTextLine *pTextLine = termBuffer_->getTextLineInBuffer(y);
          if ((int)pTextLine->getWidth() > x) {

            int startx = pTextLine->getCellBegin(x);
            int endx = pTextLine->getCellEnd(x + 1);

            drawLine(painter, termBuffer_->getCaretLine(), startx, endx,
                     termBuffer_->getCaretLine());

            QRect rect = mapToRect(startx, termBuffer_->getCaretLine(), 1, 1);
            rect.translate(0, 0);
            painter.fillRect(rect, colors_[7]);
          } else {
            QRect rect = mapToRect(termBuffer_->getCaretColumn(),
                                   termBuffer_->getCaretLine(), 1, 1);
            rect.translate(0, 0);
            painter.fillRect(rect,
                             colors_[7]);
          }
        }
        break;
      case 1:
        // underline
        painter.fillRect(pt.x(), pt.y() + 9 * charHeight_ / 10, charWidth_,
                         charHeight_ / 10, colors_[7]);
        break;
      case 2:
        // I type
        painter.fillRect(pt.x(), pt.y() + 1, charWidth_ / 5, charHeight_ - 1,
                         colors_[7]);
        break;
      default:
        painter.fillRect(pt.x(), pt.y()+1,
                         charWidth_, charHeight_ - 1, colors_[7]);
    }
  }
}


// refresh the screen when
//	1. received new contents form server
//	2. scrolled by user
void FQTermScreen::refreshScreen(QPainter &painter) {
  FQ_FUNC_TIMER("screen_paint", 5);

  if (cursorTimer_->isActive()) {
    cursorTimer_->stop();
  }

  if (blinkTimer_->isActive()) {
    blinkTimer_->stop();
  }

  hasBlinkText_ = false;
  isCursorShown_ = true;

  for (int index = bufferStart_; index <= bufferEnd_; index++) {

    FQ_VERIFY(index < termBuffer_->getNumLines());

    const FQTermTextLine *pTextLine = termBuffer_->getTextLineInBuffer(index);

    if (pTextLine->hasBlink()) {
      hasBlinkText_ = true;
      areLinesBlink_[index - bufferStart_] = true;
    } else {
      areLinesBlink_[index - bufferStart_] = false;
    }

    unsigned int startx, endx;
    if (!pTextLine->getDirtyCells(startx, endx)) {
      continue;
    }

    /*
      Finally get around this for pku & ytht, don't know why some weird things
      happened when only erase and draw the changed part.
    */
    startx = pTextLine->getCellBegin(startx);

    int len = -1;
    if ((int)endx != -1) {
      len = endx - startx;
    }

    QRect rect = mapToRect(startx, index, len, 1);
    rect.setBottom(rect.bottom() + 1);
    painter.fillRect(rect ,QBrush(colors_[0]));
    
    drawLine(painter, index, startx, endx);

    session_->clearLineChanged(index);
  }

  updateMicroFocus();
  if (termWindow_->isConnected()) {
    updateCursor(painter);
  }
 

  if (termWindow_->isConnected()) {
    cursorTimer_->start(1000);
  }

  if (hasBlinkText_) {
    blinkTimer_->start(1000);
  }
}

void FQTermScreen::repaintScreen(QPaintEvent *pe, QPainter &painter) {
  FQ_FUNC_TIMER("screen_paint", 5);

  painter.setBackground(QBrush(colors_[0]));

  FQ_TRACE("screen", 5) << "Client area: " << pe->rect().width()
                        << "x" << pe->rect().height();

  QRect rect = pe->rect().intersect(clientRectangle_);
  painter.eraseRect(rect);

  QPoint tlPoint = mapToChar(QPoint(rect.left(), rect.top()));

  QPoint brPoint = mapToChar(QPoint(rect.right(), rect.bottom()));

  for (int y = tlPoint.y(); y <= brPoint.y(); y++) {
    drawLine(painter, y);
  }
  if (termWindow_->urlStartPoint_ != termWindow_->urlEndPoint_) {
    drawUnderLine(painter, termWindow_->urlStartPoint_, termWindow_->urlEndPoint_);
  }


}

/////////////////////////////////////////////////
//TODO: change to a more powerful function
static bool isEnglishChar(const QString& str) {
  if (str.length() > 1) {
    return false;
  }
  else if (str.length() == 1 && str[0].unicode() > 128) {
    return false;
  }
  return true;
}

/////////////////////////////////////////////////
// draw a line with the specialAtter if given.
// modified by hooey to draw part of the line.
void FQTermScreen::drawLine(QPainter &painter, int index, int startx, int endx,
                            bool complete) {
  FQ_ASSERT(index < termBuffer_->getNumLines());

  const FQTermTextLine *pTextLine = termBuffer_->getTextLineInBuffer(index);
  const unsigned char *color = pTextLine->getColors();
  const unsigned char *attr = pTextLine->getAttributes();

  uint linelength = pTextLine->getWidth();
  bool *isEnglishLanguage = new bool[linelength];
  bool isSessionSelected = session_->isSelected(index);
  bool isTransparent = isSessionSelected && param_->menuType_ == 2;

  menuRect_ = QRect();
  QString cstrText;

  if (startx < 0) {
    startx = 0;
  }

  if (endx > (long)linelength || endx < 0) {
    endx = linelength;
  }

  if (complete == true && isSessionSelected) {
    menuRect_ = drawMenuSelect(painter, index);
    startx = 0;
    endx = linelength;
  }

  startx = pTextLine->getCellBegin(startx);
  endx = pTextLine->getCellEnd(endx);

  bool isMonoSpace = true;
  if (!QFontInfo(*englishFont_).fixedPitch() || !QFontInfo(*nonEnglishFont_).fixedPitch()) {
    isMonoSpace = false;
  }
  
  for (unsigned i = startx; (long)i < endx; i++) {
    startx = i;
    unsigned char tempcp = color[i];
    unsigned char tempea = attr[i];
    bool bSelected = termBuffer_->isSelected(
        QPoint(i, index), termWindow_->session_->param_.isRectSelect_);

    // get str of the same attribute
    while ((long)i < endx && tempcp == color[i] && tempea == attr[i] &&
           bSelected == termBuffer_->isSelected(
               QPoint(i, index), termWindow_->session_->param_.isRectSelect_)) {
      ++i;
    }

    unsigned cell_begin = pTextLine->getCellBegin(startx);
    unsigned cell_end = pTextLine->getCellEnd(i);
    
    cstrText.clear();
    pTextLine->getPlainText(cell_begin, cell_end, cstrText);
    for (unsigned k = i; k < cell_end; ++k) {
      cstrText.append((QChar)URC);
    }

    int text_width;

    //    painter.setRenderHint(QPainter::TextAntialiasing);
    unsigned char_begin = pTextLine->getCharBegin(cell_begin);
    for (unsigned k = cell_begin; k < cell_end; ++k) {
      unsigned begin = pTextLine->getCellBegin(k);
      unsigned end = pTextLine->getCellEnd(k + 1);
      unsigned charBegin = pTextLine->getCharBegin(begin);
      unsigned charEnd = pTextLine->getCharEnd(end);
      isEnglishLanguage[k] = isEnglishChar(cstrText.mid(charBegin - char_begin,
                                            charEnd - charBegin));
    }

    for (unsigned k = cell_begin; k < cell_end; ++k){
      cstrText.clear();
      bool languageType = isEnglishLanguage[k];
      unsigned sameLanguageStart = pTextLine->getCellBegin(k);
      while (k < cell_end && isEnglishLanguage[k] == languageType) {
        k++;
      }
      unsigned sameLanguageEnd = pTextLine->getCellEnd(k);

      pTextLine->getPlainText(sameLanguageStart, sameLanguageEnd, cstrText);
      for (unsigned j = k; j < sameLanguageEnd; ++j) {
        cstrText.append((QChar)URC);
      }

      k--;
      text_width = sameLanguageEnd - sameLanguageStart;
      painter.setFont(languageType?*englishFont_:*nonEnglishFont_);

      if (isMonoSpace) {
        drawStr(painter, cstrText, sameLanguageStart, index, text_width,
                tempcp, tempea, isTransparent, bSelected);
      } else {
        // Draw Characters one by one to fix the variable-width font
        // display problem.
        int offset = 0;
        for (uint j = 0; (long)j < cstrText.length(); ++j) {
          // TODO_UTF16: wrong character width here.
          if (cstrText.at(j) < 0x7f) {
            drawStr(painter, (QString)cstrText.at(j),
                    sameLanguageStart + offset, index, 1,
                    tempcp, tempea,
                    isTransparent, bSelected);
            offset++;
          } else {
            drawStr(painter, (QString)cstrText.at(j),
                    sameLanguageStart + offset, index, 2,
                    tempcp, tempea,
                    isTransparent, bSelected);
            offset += 2;
          }
        }
      }
      --i;
    }

  }
  delete []isEnglishLanguage;
}
//
// draw functions
void FQTermScreen::drawStr(QPainter &painter, const QString &str,
                           int x, int y, int length,
                           unsigned char color, unsigned char attr,
                           bool transparent, bool selected) {
  unsigned char cp = color;
  unsigned char ea = attr;

  // test bold mask or always highlighted
  if (GETBRIGHT(ea) || param_->isAlwaysHighlight_) {
    cp |= SET_FG_HIGHLIGHT(1);
  }
  
  // use 8-15 color
  // test dim mask
  if (GETDIM(ea)) {
  }
  
  // test underline mask
  if (GETUNDERLINE(ea)) {
    QFont font = painter.font();
    font.setUnderline(true);
    painter.setFont(font);
  } else {
    QFont font = painter.font();
    font.setUnderline(false);
    painter.setFont(font);
  }
  // test blink mask
  if (GETBLINK(ea)){}
  // test rapidblink mask
  if (GETRAPIDBLINK(ea)){}
  // test reverse mask
  if (GETREVERSE(ea)) {
    cp = GET_INVERSE_COLOR(cp);
  }

  if (selected) {
    cp = GET_INVERSE_COLOR(cp);
  }

  // test invisible mask
  if (GETINVISIBLE(ea)) {
  }

  QPoint pt = mapToPixel(QPoint(x, y));
  QRect rcErase = mapToRect(x, y, length, 1);

  int pen_color_index = GETFG(cp);
  if (!param_->isAnsiColor_) {
    if (GETBG(cp) != 0) {
      pen_color_index = 0;
    } else {
      pen_color_index = 7;
    }
  }
  painter.setPen(colors_[pen_color_index]);

  int brush_color_index = GETBG(cp);
  if (!param_->isAnsiColor_) {
    if (GETBG(cp) != 0) {
      brush_color_index = 7;
    } else {
      brush_color_index = 0;
    }
  }

  if (GETBG(cp) != 0 && !transparent) {
    painter.setBackgroundMode(Qt::OpaqueMode);
    painter.setBackground(colors_[brush_color_index]);

  } else {
    painter.setBackgroundMode(Qt::TransparentMode);
  }

  QBrush brush(colors_[brush_color_index]);

  // black on white without attr
  if (termWindow_->frame_->isBossColor_) {
    painter.setPen(Qt::black);
    if (GETBG(cp) != 0 && !transparent) {
      painter.setBackgroundMode(Qt::OpaqueMode);
    } else {
      painter.setBackgroundMode(Qt::TransparentMode);
    }
    painter.setBackground(Qt::white);
    brush = QBrush(Qt::white);

    
  }

  rcErase.setBottom(rcErase.bottom() + 1);
  rcErase.setRight(rcErase.right() + 1);    
  
  if (!menuRect_.intersects(rcErase)){
    rcErase.setRight(rcErase.right() - 1);
    painter.fillRect(rcErase, brush);
  } else {
    QRect rcKeep = menuRect_.intersected(rcErase);
    rcKeep.setY(rcErase.y());
    rcKeep.setHeight(rcErase.height());
    if (rcErase.left() < rcKeep.left()) {
      QPoint point = rcKeep.bottomLeft();
      point.setX(point.x() + 1);
      point.setY(point.y() + 1);
      painter.fillRect(QRect(rcErase.topLeft(), point),
                       brush);
    }
    if (rcKeep.right() < rcErase.right()) {
      QPoint point = rcErase.bottomRight();
      point.setX(point.x() + 1);
      point.setY(point.y() + 1);

      painter.fillRect(QRect(rcKeep.topRight(), point),
                       brush);
    }
  }

  if (!(isBlinkScreen_ && GETBLINK(attr))) {
    FQ_TRACE("draw_text", 10) << "draw text: " << str;
    
    QFontMetrics qm = painter.fontMetrics();
    int font_height = qm.height();
    int expected_font_height = rcErase.height();
    int height_gap = expected_font_height - font_height;
    int offset = (height_gap + 1)/2; 
    int ascent = qm.ascent() + offset; 

    painter.drawText(pt.x(), pt.y() + ascent, str);
  }
}


void FQTermScreen::eraseRect(QPainter &, int, int, int, int, short){
  FQ_VERIFY(false);
}

void FQTermScreen::bossColor() {
  setBackgroundPixmap(backgroundPixmap_, backgroundPixmapType_);
  if (termWindow_->frame_->isBossColor_) {
    colors_[0] = Qt::white;
    colors_[7] = Qt::black;
    QPalette palette;
    palette.setColor(backgroundRole(), Qt::white);
    setPalette(palette);

  } else {
    colors_[0] = param_->backgroundColor_;
    colors_[7] = param_->foregroundColor_;
  }


  setPaintState(Repaint);
  update();
}

QRect FQTermScreen::drawMenuSelect(QPainter &painter, int index) {
  QRect rcSelect, rcMenu, rcInter;
  // 	FIXME: what is this for
  if (termBuffer_->isSelected(index)) {
    bool is_rect_sel = termWindow_->session_->param_.isRectSelect_;
    rcSelect = mapToRect(termBuffer_->getSelectRect(index, is_rect_sel));
    if (termWindow_->frame_->isBossColor_) {
      painter.fillRect(rcSelect, QBrush(colors_[0]));
    } else {
      painter.fillRect(rcSelect, QBrush(colors_[7]));
    }
  }

  if (session_->isSelected(index)) {
    rcMenu = mapToRect(session_->getSelectRect());
    // 		m_pBand->setGeometry(rcMenu);
    // 		m_pBand->show();
    switch (param_->menuType_) {
      case 0:
        // underline
        painter.fillRect(rcMenu.x(), rcMenu.y() + 10 * charHeight_ / 11,
                         rcMenu.width(), charHeight_ / 11, colors_[7]);
        break;
      case 2:
        painter.fillRect(rcMenu, QBrush(termWindow_->frame_->isBossColor_?colors_[0]:param_->menuColor_));
        break;
    }
  }
  return rcMenu;
}

/* ------------------------------------------------------------------------ */
/*	                                                                        */
/* 	                            Auxilluary	                                */
/*                                                                          */
/* ------------------------------------------------------------------------ */
QPoint FQTermScreen::mapToPixel(const QPoint &point) {
  QPoint pt = clientRectangle_.topLeft();

  QPoint pxlPoint;

  pxlPoint.setX(point.x() *charWidth_ + pt.x());
  pxlPoint.setY((point.y() - bufferStart_) *charHeight_ + pt.y());

  return pxlPoint;
}
// mapToChar: get a position in the window and return the
// corresponding char position
QPoint FQTermScreen::mapToChar(const QPoint &point) {
  QPoint pt = clientRectangle_.topLeft();

  QPoint chPoint;
  chPoint.setX(qMin(qMax(0, (point.x() - pt.x()) / charWidth_),
                    termBuffer_->getNumColumns() - 1));
  chPoint.setY(qMin(qMax(0, (point.y() - pt.y()) / charHeight_ + bufferStart_),
                    bufferEnd_));

  //FIXME add bound check
  return chPoint;
}

QRect FQTermScreen::mapToRect(int x, int y, int width, int height) {
  QPoint pt = mapToPixel(QPoint(x, y));

  if (width == -1) {
	// to the end
    return QRect(pt.x(), pt.y(), size().width(), charHeight_ *height);
  } else {
    return QRect(pt.x(), pt.y(), width *charWidth_, charHeight_ *height);
  }
}

QRect FQTermScreen::mapToRect(const QRect &rect) {
  return mapToRect(rect.x(), rect.y(), rect.width(), rect.height());
}

// from KImageEffect::fade
QImage &FQTermScreen::fade(QImage &img, float val, const QColor &color) {
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
        *data++ = qRgba(r, g, b, qAlpha(col));
      }
    }
  }

  return img;
}

void FQTermScreen::inputMethodEvent(QInputMethodEvent *e) {
  FQ_TRACE("ime", 5) << "Preedit: " << e->preeditString();
  FQ_TRACE("ime", 5) << "Commit: " << e->commitString();

  FQ_TRACE("ime", 8) << "Replace: " << e->replacementStart()
                     << ", " << e->replacementLength();

  if (e->preeditString().size() > 0) {
    preedit_line_->setPreeditText(e, nonEnglishFont_);

    QPoint pt = mapToPixel(QPoint(termBuffer_->getCaretColumn(),
                                  termBuffer_->getCaretLine()));
    preedit_line_->move(pt);
    preedit_line_->show();
    preedit_line_->update();
  } else {
    preedit_line_->hide();
  }
  
  if (e->commitString().size() > 0) {
    emit inputEvent(e->commitString());
  }
}

QVariant FQTermScreen::inputMethodQuery(Qt::InputMethodQuery property) const {
  FQ_TRACE("ime", 5) << "Query: " << property;

  int row = termBuffer_->getCaretRow();
  int col = termBuffer_->getCaretColumn();

  switch (property) {
    case Qt::ImMicroFocus:
      // Here we must use (column + 1, row + 1) as the result,
      // otherwise MS Pinyin or GPY won't work well.
      return QVariant(
          QRect((termBuffer_->getCaretColumn()) * charWidth_ 
                + (preedit_line_->isVisible() ?
                   preedit_line_->getCaretOffset() : 0),
                (termBuffer_->getCaretRow()) * charHeight_,
                charWidth_, charHeight_));
    case Qt::ImFont:
      return QVariant(*nonEnglishFont_);
    case Qt::ImCurrentSelection:
      return QVariant(QString());
    case Qt::ImCursorPosition:
    case Qt::ImSurroundingText:
      {
        tmp_im_query_->clear();
        const FQTermTextLine *line = termBuffer_->getTextLineInTerm(row);
        line->getPlainText(0, line->getCellEnd(col), *tmp_im_query_);

        // TODO_UTF16: here only BMP is considered.
        if (Qt::ImCursorPosition) {
          return QVariant(tmp_im_query_->size());
        } else {
          tmp_im_query_->clear();
          line->getPlainText(0, line->getWidth(), *tmp_im_query_);
          return QVariant(QString(*tmp_im_query_));
        }
      }
      break;
    default:
      return QVariant();
  }
}

void FQTermScreen::setTermFont(bool isEnglish, const QFont& font)
{
  QFont *&pFont = isEnglish?englishFont_:nonEnglishFont_;
  delete pFont;
  if (param_->isFontAutoFit_) {
    pFont = new QFont(font.family());
    pFont->setKerning(false);
  } else {
    pFont = new QFont(font);
    pFont->setKerning(false);    
    setFontMetrics();
  }
}

QFont FQTermScreen::termFont(bool isEnglish) {
  return isEnglish?(*englishFont_):(*nonEnglishFont_);
}

void FQTermScreen::drawUnderLine(QPainter &painter, const QPoint& startPoint, const QPoint& endPoint) {
  if (startPoint.y() == endPoint.y()) {
    drawSingleUnderLine(painter, startPoint, endPoint);
  } else {
    drawSingleUnderLine(painter, startPoint, QPoint(termBuffer_->getNumColumns(), startPoint.y()));
    drawSingleUnderLine(painter, QPoint(0, endPoint.y()), endPoint);
    for (int i = startPoint.y() + 1; i < endPoint.y(); ++i) {
      drawSingleUnderLine(painter, QPoint(0, i), QPoint(termBuffer_->getNumColumns(), i));
    }
  }
}

void FQTermScreen::drawSingleUnderLine(QPainter &painter, const QPoint& startPoint, const QPoint& endPoint) {
  FQ_VERIFY(startPoint.y() == endPoint.y());
  QPoint realStart = mapToPixel(startPoint);
  QPoint realEnd = mapToPixel(endPoint);
  painter.fillRect(realStart.x(), realStart.y() + 10 * charHeight_ / 11,
    realEnd.x() - realStart.x(), charHeight_ / 11, termWindow_->frame_->isBossColor_?colors_[0]:param_->menuColor_);
}
PreeditLine::PreeditLine(QWidget *parent,const QColor *colors)
    : QWidget(parent),
      pixmap_(new QPixmap()),
      colors_(colors) {
  //setFocusPolicy(Qt::NoFocus);
  hide();
}

PreeditLine::~PreeditLine() {
  delete pixmap_;
}

void PreeditLine::setPreeditText(QInputMethodEvent *e, const QFont *font) {
  QString text = e->preeditString();

  const QColor &white_color = colors_[7];
  const QColor &black_color = colors_[0];

  // MS Pinyin will add a white space '\u3000' in the end. Remove it.
  text = text.trimmed();

  QFontMetrics fm(*font);
  QRect textRect = fm.boundingRect(text);

  int width = textRect.width();
  int height = textRect.height();

  delete pixmap_;
  pixmap_ = new QPixmap(width, height + 1); // add one pixel to shadow
                                            // the original cursor on
                                            // screen.
  this->resize(width, height + 1);

  QPainter painter(pixmap_);
  painter.fillRect(textRect, QBrush(black_color));

  painter.setFont(*font);
  painter.setBackgroundMode(Qt::OpaqueMode);

  // Parse attributes of the input method event.
  for (int i = 0; i < e->attributes().size(); ++i) {
    const QInputMethodEvent::Attribute &a = e->attributes().at(i);
    switch(a.type) {
      case QInputMethodEvent::Cursor:
        FQ_TRACE("ime", 10) << "Cursor: " << a.start << ", " << a.length;
        {
          painter.setPen(white_color);
      
          int caret_begin = fm.boundingRect(text.left(a.start)).right();

          painter.drawLine(caret_begin, 0, caret_begin, height);
          painter.drawLine(caret_begin + 1, 0, caret_begin + 1, height);

          caret_offset_ = caret_begin;
        }
        break;
      case QInputMethodEvent::TextFormat:
        {
          if (a.start + a.length > text.size()) {
            // index over flow. ignore it.
            break;
          }

          const QString &sub_text = text.mid(a.start, a.length);

          int x = fm.boundingRect(text.left(a.start)).width();

          QTextCharFormat f =
              qvariant_cast<QTextFormat>(a.value).toCharFormat();

          // Hack here.
          // If the background() is of style Qt::SolidPattern,
          // we consider this segment of text is highlighted.
          // Otherwise it's normal preedit text.
          if (f.background().style() == Qt::SolidPattern) {
            QBrush bg(white_color);
            bg.setStyle(Qt::SolidPattern);

            painter.setBackground(bg);
            painter.setPen(black_color);
          } else {
            QBrush bg(black_color);
            bg.setStyle(Qt::SolidPattern);

            painter.setBackground(bg);
            painter.setPen(white_color);
          }

          // Draw the text;
          painter.drawText(x, fm.ascent(), sub_text);

          FQ_TRACE("ime", 10) << "Sub-text: " << sub_text;
          FQ_TRACE("ime", 10) << "Format: " << a.start << ", " << a.length;
          FQ_TRACE("ime", 10) << "Background: " << f.background();
          FQ_TRACE("ime", 10) << "Pen: " << f.textOutline(); 
        }
        break;
      case QInputMethodEvent::Language:
        FQ_TRACE("ime", 10) << "Language: " << a.start << ", " << a.length;
        break;
      case QInputMethodEvent::Ruby:
        FQ_TRACE("ime", 10) << "Ruby: " << a.start << ", " << a.length;
        break;
    }
  }

  // Draw a line under the preedit text.
  QPen pen(white_color);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.drawLine(0, height - 1, width, height - 1);
}

void PreeditLine::paintEvent(QPaintEvent *e) {
  QWidget::paintEvent(e);
  QPainter p(this);
  p.drawPixmap(QPointF(0, 0), *pixmap_, e->rect());
}

}  // namespace FQTerm

#include "fqterm_screen.moc"
