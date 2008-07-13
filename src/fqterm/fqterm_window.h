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

#ifndef FQTERM_WINDOW_H
#define FQTERM_WINDOW_H

#include <QMainWindow>
#include <QCursor>
#include <QString>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fqterm_param.h"
#include "fqterm_config.h"
#include "fqterm_convert.h"

class QCloseEvent;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QMouseEvent; 
class QProgressBar;
class QScriptEngine;
class QWheelEvent;

namespace FQTerm {

class QProgressDialog;
class PageViewMessage;
class FQTermConfig;
class FQTermImage;
class FQTermFrame;
class FQTermHttp;
class FQTermIPLocation;
class FQTermScreen;
class FQTermSession;
class FQTermSound;
class FQTermWindow;
class popWidget;
class zmodemDialog;


class FQTermWindow: public QMainWindow {
  friend class FQTermScreen;

  Q_OBJECT;
 public:
  FQTermWindow(FQTermConfig *, FQTermFrame *frame, FQTermParam param, int addr = -1, QWidget
              *parent = 0, const char *name = 0, Qt::WFlags wflags = Qt::Window);
  ~FQTermWindow();

  void connectHost();
  bool isConnected();

  void disconnect();
  //redraw the dirty lines
  void refreshScreen();
  //repaint a dirty rectangle.
  void repaintScreen();
  //force a repaint by sending a resize event
  void forcedRepaintScreen();
  void viewMessages();
  void toggleAutoReply();
  void toggleAntiIdle();
  void setFont(bool isEnglish);

  void runScript(const QString & filename);
  void externInput(const QByteArray &);
  void externInput(const QString &);
  void getHttpHelper(const QString &, bool);

public:
  FQTermFrame *frame_;
  FQTermSession *session_;
  FQTermScreen *screen_;

  QString allMessages_;
  QString pythonErrorMessage_;
  // before calling repaintScreen(), pls
 // fill this rectangle.
  QRect clientRect_;
  QPoint urlStartPoint_;
  QPoint urlEndPoint_;


signals:
  void resizeSignal(FQTermWindow*);

 public slots:
  // ui
  void copy();
  void paste();
  void copyArticle();
  void setting();
  void setColor();
  void runScript();
  void stopScript();
  void showStatusBar(bool);
  //void reconnect();
  void sendParsedString(const char*);
  void showIP(bool show = true);

  void beep();
  void startBlink();
  void stopBlink();

  void connectionClosed();

  void messageAutoReplied();

  void pasteHelper(bool);
  QByteArray unicode2SessionEncoding(const QString &);

  QByteArray parseString(const QByteArray &, int *len = 0);

  //  void sendMouseState(int, Qt::KeyboardModifier, Qt::KeyboardModifier, const
  //                    QPoint &);
#ifdef HAVE_PYTHON
  bool pythonCallback(const QString &, PyObject*);
#endif
  void pythonMouseEvent(int, Qt::KeyboardModifier, Qt::KeyboardModifier, const
    QPoint &, int);

 protected slots:
  void setFont();
  void recreateMenu();
  //void readReady(int);
  //refresh screen & reset cursor position
  void sessionUpdated();

  void requestUserPwd(QString *userName, QString *password, bool *isOK);

  void TelnetState(int);
  void ZmodemState(int, int, const char *);
  void showSessionErrorMessage(const char *reason);

  void blinkTab();

  //http menu
  void previewLink();
  void saveLink();
  void openLink();
  void copyLink();
  void previewImage(const QString &filename, bool raiseViewer);
  void startHttpDownload(FQTermHttp *, const QString &filedesp);

  void httpDone(QObject*);
 
  // decode
  // void setMouseMode(bool);
  void articleCopied(int e, const QString content);
  //void jobDone(int);

 protected:
  bool event(QEvent*);

  void resizeEvent(QResizeEvent *);
  void mouseDoubleClickEvent(QMouseEvent*);
  void mouseMoveEvent(QMouseEvent*);
  void mousePressEvent(QMouseEvent*);
  void mouseReleaseEvent(QMouseEvent*);
  void wheelEvent(QWheelEvent*);
  void enterEvent(QEvent*);
  void leaveEvent(QEvent*);
  void changeEvent(QEvent*);
  void closeEvent(QCloseEvent*);
  void keyPressEvent(QKeyEvent*);
  void focusInEvent (QFocusEvent *);

 private:
  QMenu *menu_;
  QMenu *urlMenu_;

  static char directions_[][5];
  QCursor cursors_[9];

  FQTermConvert encodingConverter_;

  // mouse select
  QPoint lastMouseCell_;
  bool isSelecting_;

  // address setting
  bool isAddressChanged_;
  int addressIndex_;

  // url rect
  QRect urlRectangle_;

  //ip rect
  QRect ipRectangle_;
 
  // play sound
  FQTermSound *sound_;


  FQTermConfig *config_;
  zmodemDialog *zmodemDialog_;
  
  //IP location
  QString location_;
  FQTermIPLocation *ipDatabase_;


  //osd
  PageViewMessage *pageViewMessage_;

  popWidget *popWindow_;
  QTimer *tabBlinkTimer_;

  bool isMouseClicked_;
  bool blinkStatus_;
  bool isIpDataFileExisting_;

  bool isUrlUnderLined_;

#ifdef HAVE_PYTHON
  PyObject *pythonModule_,  *pythonDict_;
#endif

  QScriptEngine *script_engine_;  

 private:
  void addMenu();
  void saveSetting();

  void setCursorPosition(const QPoint& mousePosition);
  //set cursor type according to the content
  //show ip location info if openUrlCheck is set
  void setCursorType(const QPoint& mousePosition);

  void openUrl();
  void enterMenuItem();
  void processLClick(const QPoint& cellClicked);

  void startSelecting(const QPoint& mousePosition);
  void onSelecting(const QPoint& mousePosition);
  void finishSelecting(const QPoint& mousePosition);

  void updateSetting(const FQTermParam& param);

  void sendKey(const int key, const Qt::KeyboardModifiers modifier,
               const QString &text);

  QScriptEngine *getScriptEngine();
  void clearScriptEngine();
};

}  // namespace FQTerm

#endif  // FQTERM_WINDOW_H
