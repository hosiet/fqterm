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

#ifndef FQTERM_SCREEN_H
#define FQTERM_SCREEN_H

#include <QWidget>
#include "fqterm_param.h"
#include "fqterm_convert.h"

class QTextCodec;
class QColor;
class QPainter;
class QInputMethodEvent;
class QPixmap;
class QScrollBar;
class QShortcut;

namespace FQTerm {

class FQTermWindow;
class FQTermBuffer;
class FQTermSession;
class FQTermParam;

class PreeditLine: public QWidget {
public:
  PreeditLine(QWidget *parent,const QColor *colors);
  ~PreeditLine();
  
  void setPreeditText(QInputMethodEvent *e, const QFont *font);

  void paintEvent(QPaintEvent * event);

  int getCaretOffset() const {return caret_offset_;}

private:
  QPixmap *pixmap_;
  int caret_offset_;

  const QColor *colors_;
};

class FQTermScreen: public QWidget {
  Q_OBJECT;
 public:
  enum PaintState {
     System = 0, Repaint = 1, NewData = 2, Blink = 4, Cursor = 8
  };
  enum MouseState {
    Enter, Press, Move, Release, Leave
  };

  FQTermScreen(QWidget *parent, FQTermParam *param, FQTermSession *session);
  ~FQTermScreen();

  void setSchema();
  void setTermFont(bool isEnglish, const QFont& font);

  QFont termFont(bool isEnglish);

  void setBackgroundPixmap(const QPixmap &pixmap, int nType = 0);

  void setPaintState(PaintState ps) {
    paintState_ |= ps;
  }

  void clearPaintState(PaintState ps) {
    paintState_ &= ~ps;
  }

  bool testPaintState(PaintState ps) {
    return paintState_ & ps;
  }


  /*void clearPaintState() {
      paintState_ = None;
  }*/

  QTextCodec * encoding() const{ return encoding_;}

 private:
  int paintState_;
  void refreshScreen(QPainter &painter);
  void blinkScreen(QPainter &painter);
  void updateCursor(QPainter &painter);
  void repaintScreen(QPaintEvent *pe, QPainter &painter);
  void syncBufferAndScreen();

 signals:
  // 0 - enter  1 - press  2 - move  3 - release 4 - leave
  void mouseAction(int, QMouseEvent*);
  void inputEvent(const QString&);

 public slots:
  void bufferSizeChanged();
  void termSizeChanged(int column, int row);
  void bossColor();
  void updateScrollBar();
  void setFontAntiAliasing(bool on = true);

 protected:
  void initFontMetrics();

  bool event(QEvent *e);

  void resizeEvent(QResizeEvent*);
  void focusInEvent(QFocusEvent*);
  void focusOutEvent(QFocusEvent*);

  void paintEvent(QPaintEvent*);

  // mouse
  void enterEvent(QEvent*);
  void leaveEvent(QEvent*);
  void mousePressEvent(QMouseEvent*);
  void mouseMoveEvent(QMouseEvent*);
  void mouseReleaseEvent(QMouseEvent*);
  void wheelEvent(QWheelEvent*);

  // display
  void eraseRect(QPainter &, int, int, int, int, short);
  void drawStr(QPainter &painter, const QString &str, 
               int x, int y, int length,
               unsigned char color, unsigned char attr,
               bool transparent, bool selected = false);
  void drawLine(QPainter &, int index, int startx = -1, int endx = -1, 
                bool complete = true);
  void drawCaret(QPainter &, bool);
  QRect drawMenuSelect(QPainter &, int);
  void drawUnderLine(QPainter &, const QPoint& startPoint, const QPoint& endPoint);
  
  // auxiluary
  int getPosX(int xChar) {
    return xChar *charWidth_;
  }
  int getPosY(int yLine) {
    return yLine *charHeight_;
  }

  // Buffer cell coordinate to screen pixel.
  QPoint mapToPixel(const QPoint &);

  // Screen pixel coordinate to buffer cell coordinate.
  QPoint mapToChar(const QPoint &);

  // Buffer cell coordinate to screen pixel.
  QRect mapToRect(int, int, int, int);
  QRect mapToRect(const QRect &);

  void updateFont();
  void setFontMetrics();

  QImage &fade(QImage &, float, const QColor &);

  void inputMethodEvent(QInputMethodEvent *e);
  QVariant inputMethodQuery(Qt::InputMethodQuery property)const;


 protected slots:
  void blinkEvent();
  void cursorEvent();
  void scrollChanged(int);
  void prevPage();
  void nextPage();
  void prevLine();
  void nextLine();
  void scrollLine(int);
 protected:
   void drawSingleUnderLine(QPainter &, const QPoint& startPoint, const QPoint& endPoint);

  QRect clientRectangle_; // the display area
  QRect menuRect_;

  int scrollBarWidth_;

  QScrollBar *scrollBar_;

  QTimer *blinkTimer_;
  QTimer *cursorTimer_;

  bool isBlinkScreen_;
  bool isBlinkCursor_;
  bool hasBlinkText_;

  FQTermWindow *termWindow_;
  FQTermSession *session_;
  const FQTermBuffer *termBuffer_;
  FQTermParam *param_;

  bool is_light_background_mode_;

  QColor colors_[16];
  QFont *englishFont_;
  QFont *nonEnglishFont_;

  int fontAscent_, fontDescent_, charWidth_, charHeight_;
  int lineSpacing_; // for future

  bool *areLinesBlink_;
  bool isCursorShown_;

  // background
  bool hasBackground_;
  QPixmap backgroundPixmap_;
  int backgroundPixmapType_;

  // the range of the buffer's lines to be displayed
  int bufferStart_;
  int bufferEnd_;

  QShortcut *gotoPrevPage_;
  QShortcut *gotoNextPage_;
  QShortcut *gotoPrevLine_;
  QShortcut *gotoNextLine_;

  FQTermConvert encodingConverter_;

  QTextCodec *encoding_;
  PreeditLine *preedit_line_;
  
  QString *tmp_im_query_;

  friend class FQTermWindow;
  // for test only
};

}  // namespace FQTerm

#endif  // FQTERM_SCREEN_H
