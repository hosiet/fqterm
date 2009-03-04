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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QFontMetrics>
#include <QInputDialog>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QScriptEngine>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>

#include <QMdiArea>
#include <QMdiSubWindow>

#include "addrdialog.h"
#include "articledialog.h"
#include "msgdialog.h"
#include "osdmessage.h"
#include "popwidget.h"
#include "progressBar.h"
#include "common.h"
#include "fqterm_buffer.h"
#include "fqterm_canvas.h"
#include "fqterm_config.h"
#include "fqterm_convert.h"
#include "fqterm_filedialog.h"
#include "fqterm_frame.h"
#include "fqterm_http.h"
#include "fqterm_ip_location.h"
#include "fqterm_param.h"
#include "fqterm_path.h"
#include "fqterm_screen.h"
#include "fqterm_session.h"
#include "fqterm_sound.h"
#include "fqterm_window.h"
#include "fqterm_wndmgr.h"
#include "imageviewer.h"
#include "sshlogindialog.h"
#include "statusBar.h"
#include "zmodemdialog.h"

namespace FQTerm {

char FQTermWindow::directions_[][5] =  {
  // 4
  "\x1b[1~",  // 0 HOME
  "\x1b[4~",  // 1 END
  "\x1b[5~",  // 2 PAGE UP
  "\x1b[6~",  // 3 PAGE DOWN
  // 3
  "\x1b[A",  // 4 UP
  "\x1b[B",  // 5 DOWN
  "\x1b[D",  // 6 LEFT
  "\x1b[C" // 7 RIGHT
};

class FAQ: public QObject {
  Q_OBJECT;
 public slots:
  FAQ() {a = 100;}

  int get() {return a;}
  
 private:
  int a;
};


//constructor
FQTermWindow::FQTermWindow(FQTermConfig *config, FQTermFrame *frame, FQTermParam param,
                           int addressIndex, QWidget *parent,
                           const char *name, Qt::WFlags wflags)
    : QMainWindow(parent, wflags),
      frame_(frame),
      isSelecting_(false),
      isAddressChanged_(false),
      addressIndex_(addressIndex),
      sound_(NULL),
      location_(),
      isMouseClicked_(false),
      blinkStatus_(true),
      isUrlUnderLined_(false),
      script_engine_(NULL) {
  // isMouseX11_ = false;
  session_ = new FQTermSession(config, param, (frame_->preference_.openBeep_ != 0),
                               frame_->preference_.serverEncodingID_,
                               frame_->preference_.zmodemDir_);
  screen_ = new FQTermScreen(this, &session_->param_, session_);

  config_ = config;
  zmodemDialog_ = new zmodemDialog(this);



  ipDatabase_ = new FQTermIPLocation(getPath(USER_CONFIG));
  if (!ipDatabase_->haveFile()) {
    delete ipDatabase_;
    ipDatabase_ = new FQTermIPLocation(getPath(RESOURCE));
  }
  pageViewMessage_ = new PageViewMessage(this);
  tabBlinkTimer_ = new QTimer;
  isIpDataFileExisting_ = ipDatabase_->haveFile();

  setWindowIcon(QIcon("noicon"));
  setWindowTitle(param.name_);
  setMouseTracking(true);
  setFocusProxy(screen_);
  setCentralWidget(screen_);
  addMenu();
  pageViewMessage_->display(tr("Not connected"));
  statusBar()->setSizeGripEnabled(false);
  statusBar()->setVisible(frame_->isStatusBarShown_);

  FQ_VERIFY(connect(frame_, SIGNAL(changeLanguage()),
                    this, SLOT(recreateMenu())));
  FQ_VERIFY(connect(frame_, SIGNAL(bossColor()),
                    screen_, SLOT(bossColor())));
  FQ_VERIFY(connect(frame_, SIGNAL(updateScroll()),
                    screen_, SLOT(updateScrollBar())));
  FQ_VERIFY(connect(frame_, SIGNAL(updateStatusBar(bool)),
                    this, SLOT(showStatusBar(bool))));

  FQ_VERIFY(connect(screen_, SIGNAL(inputEvent(const QString&)),
                    session_, SLOT(handleInput(const QString&))));

  FQ_VERIFY(connect(session_, SIGNAL(zmodemStateChanged(int, int, const char*)),
                    this, SLOT(ZmodemState(int, int, const char *))));
  FQ_VERIFY(connect(zmodemDialog_, SIGNAL(canceled()),
                    session_, SLOT(cancelZmodem())));
  FQ_VERIFY(connect(session_, SIGNAL(connectionClosed()),
                    this, SLOT(connectionClosed())));
  FQ_VERIFY(connect(session_, SIGNAL(startAlert()), this, SLOT(startBlink())));
  FQ_VERIFY(connect(session_, SIGNAL(stopAlert()), this, SLOT(stopBlink())));
  // FQ_VERIFY(connect(session_->decoder_, SIGNAL(mouseMode(bool)),
  //                   this, SLOT(setMouseMode(bool))));
  FQ_VERIFY(connect(session_, SIGNAL(articleCopied(int, const QString)),
                    this, SLOT(articleCopied(int, const QString))));
  FQ_VERIFY(connect(session_, SIGNAL(requestUserPwd(QString*, QString*, bool*)),
                    this, SLOT(requestUserPwd(QString *, QString *, bool *))));
  //connect telnet signal to slots
  // QVERIFY(connect(session_->telnet_, SIGNAL(readyRead(int)),
  //                 this, SLOT(readReady(int))));
  FQ_VERIFY(connect(session_, SIGNAL(sessionUpdated()),
                    this, SLOT(sessionUpdated())));
  FQ_VERIFY(connect(session_, SIGNAL(bellReceived()), this, SLOT(beep())));
  FQ_VERIFY(connect(session_, SIGNAL(messageAutoReplied()),
                    this, SLOT(messageAutoReplied())));
  FQ_VERIFY(connect(session_, SIGNAL(telnetStateChanged(int)),
                    this, SLOT(TelnetState(int))));
  FQ_VERIFY(connect(session_, SIGNAL(errorMessage(const char *)),
                    this, SLOT(showSessionErrorMessage(const char *))));

  FQ_VERIFY(connect(tabBlinkTimer_, SIGNAL(timeout()), this, SLOT(blinkTab())));

#if defined(WIN32)
  popWindow_ = new popWidget(this, frame_);
#else
  popWindow_ = new popWidget(this);
#endif

  const QString &resource_dir = getPath(RESOURCE);

  cursors_[FQTermSession::kHome] = QCursor(
      QPixmap(resource_dir + "cursor/home.xpm"));
  cursors_[FQTermSession::kEnd] = QCursor(
      QPixmap(resource_dir + "cursor/end.xpm"));
  cursors_[FQTermSession::kPageUp] = QCursor(
      QPixmap(resource_dir + "cursor/pageup.xpm"));
  cursors_[FQTermSession::kPageDown] = QCursor(
      QPixmap(resource_dir + "cursor/pagedown.xpm"));
  cursors_[FQTermSession::kUp] = QCursor(
      QPixmap(resource_dir + "cursor/prev.xpm"));
  cursors_[FQTermSession::kDown] = QCursor(
      QPixmap(resource_dir + "cursor/next.xpm"));
  cursors_[FQTermSession::kLeft] = QCursor(
      QPixmap(resource_dir + "cursor/exit.xpm"), 0, 10);
  cursors_[FQTermSession::kRight] = QCursor(
      QPixmap(resource_dir + "cursor/hand.xpm"));
  cursors_[FQTermSession::kNormal] = Qt::ArrowCursor;

  connectHost();

  if  (session_->param_.isAutoLoadScript_
       && !session_->param_.autoLoadedScriptFileName_.isEmpty()) {
    const QString &filename = session_->param_.autoLoadedScriptFileName_;
    runScript(filename);    
  }
}

FQTermWindow::~FQTermWindow() {
  //  delete telnet_;
  delete session_;
  delete popWindow_;
  delete tabBlinkTimer_;
  delete menu_;
  delete urlMenu_;
  delete screen_;
  delete ipDatabase_;
  delete pageViewMessage_;
  delete script_engine_;
}

void FQTermWindow::addMenu() {
  urlMenu_ = new QMenu(screen_);
  urlMenu_->addAction(tr("Preview image"), this, SLOT(previewLink()));
  urlMenu_->addAction(tr("Open link"), this, SLOT(openLink()));
  urlMenu_->addAction(tr("Save As..."), this, SLOT(saveLink()));
  urlMenu_->addAction(tr("Copy link address"), this, SLOT(copyLink()));

  const QString &resource_dir = getPath(RESOURCE);

  menu_ = new QMenu(screen_);
#if defined(__APPLE__)
  // Please note that on MacOSX Qt::CTRL corresponds to Command key (apple key),
  // while Qt::Meta corresponds to Ctrl key.
  QKeySequence copy_shortcut(tr("Ctrl+C"));
  QKeySequence paste_shortcut(tr("Ctrl+V"));
#else
  QKeySequence copy_shortcut(tr("Ctrl+Insert"));
  QKeySequence paste_shortcut(tr("Shift+Insert"));
#endif
  menu_->addAction(QPixmap(resource_dir + "pic/copy.png"), tr("Copy"),
                   this, SLOT(copy()), copy_shortcut);
  menu_->addAction(QPixmap(resource_dir + "pic/paste.png"), tr("Paste"),
                   this, SLOT(paste()), paste_shortcut);

  menu_->addAction(QPixmap(resource_dir+"pic/get_article_fulltext.png"), tr("Copy Article"),
                   this, SLOT(copyArticle()), tr("F9"));
  menu_->addSeparator();

  QMenu *fontMenu = new QMenu(menu_);
  fontMenu->setTitle(tr("Font"));
  fontMenu->setIcon(QPixmap(resource_dir + "pic/change_fonts.png"));
  for (int i = 0; i < 2; ++i) {
    QAction *act = fontMenu->addAction(
        FQTermParam::getLanguageName(bool(i)) + tr(" Font"),
        this, SLOT(setFont()));
    act->setData(i);
  }
  menu_->addMenu(fontMenu);

  menu_->addAction(QPixmap(resource_dir + "pic/ansi_color.png"), tr("Color"),
                   this, SLOT(setColor()));
  menu_->addSeparator();
  menu_->addAction(QPixmap(resource_dir + "pic/preferences.png"), tr("Setting"),
                   this, SLOT(setting()));
}

void FQTermWindow::recreateMenu() {
  delete urlMenu_;
  delete menu_;
  addMenu();
}

//close event received
void FQTermWindow::closeEvent(QCloseEvent *clse) {
  if (isConnected() && frame_->preference_.openWarnOnClose_) {
    QMessageBox mb(tr("FQTerm"),
                   tr("Still connected, do you really want to exit?"),
                   QMessageBox::Warning, QMessageBox::Yes|QMessageBox::Default,
                   QMessageBox::No | QMessageBox::Escape, 0, this);
    if (mb.exec() == QMessageBox::Yes) {
      session_->close();
    } else {
      clse->ignore();
      return;
    }
  }
  saveSetting();
  frame_->windowManager_->removeWindow(this);
}

void FQTermWindow::blinkTab() {
  frame_->windowManager_->blinkTheTab(this, blinkStatus_);
  blinkStatus_ = !blinkStatus_;
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           Mouse & Key                                    */
/*                                                                          */
/* ------------------------------------------------------------------------ */
void FQTermWindow::enterEvent(QEvent*){}

void FQTermWindow::leaveEvent(QEvent*) {
  // TODO: code below doesn't work
  // QPoint temp(0, 0);
  // session_->setSelect(temp, temp, session_->isRectangleCopy_);
  // QApplication::postEvent(screen_, new QPaintEvent(QRect(0, 0, 0, 0));
  // &paintEvent);
}

void FQTermWindow::changeEvent(QEvent *e) {
  if (e->type() == QEvent::WindowStateChange) {
    QWindowStateChangeEvent *event = dynamic_cast<QWindowStateChangeEvent *>(e);
    Qt::WindowStates oldState = event->oldState();
    if (!isMaximized()) {
      frame_->windowManager_->refreshAllExcept(this);
    }
    if ( (oldState & Qt::WindowMinimized && !isMinimized()) ||
         (!(oldState & Qt::WindowActive) && isActiveWindow())) {
      forcedRepaintScreen();
    }
    if (oldState & Qt::WindowMaximized || isMaximized()) {
      emit resizeSignal(this);
    }
  }
}

void FQTermWindow::resizeEvent(QResizeEvent *qResizeEvent)
{
  emit resizeSignal(this);
}

bool FQTermWindow::event(QEvent *qevent) {
  bool res = false;
  QKeyEvent *keyEvent;
  switch(qevent->type()) {
#ifdef __linux__ 
    case QEvent::ShortcutOverride:    
       keyEvent = dynamic_cast<QKeyEvent *>(qevent);   
       if(keyEvent->key() == Qt::Key_W &&    
          keyEvent->modifiers() == Qt::ControlModifier) {    
         keyEvent->accept();   
         res = true;   
       }   
       break;
#endif
    case QEvent::KeyPress:
      keyEvent = dynamic_cast<QKeyEvent *>(qevent);
      if (keyEvent->key() == Qt::Key_Tab ||
          keyEvent->key() == Qt::Key_Backtab) {
        // Key_Tab and Key_Backtab are special, if we don't process them here,
        // the default behavoir is to move focus. see QWidget::event.
        keyPressEvent(keyEvent);
        keyEvent->accept();
        res = true;
      }
      break;
    default:
      break;
  }

  res = res || QMainWindow::event(qevent);

  if (qevent->type() == QEvent::HoverMove
      || qevent->type() == QEvent::MouseMove
      || qevent->type() == QEvent::Move) {
    if (res) {
      FQ_TRACE("wndEvent", 10) << "Accept event: " << qevent->type()
                               << " " << getEventName(qevent->type()) << ".";
    } else {
      FQ_TRACE("wndEvent", 10) << "Ignore event: " << qevent->type()
                               << " " << getEventName(qevent->type()) << ".";
    }
  } else {
    if (res) {
      FQ_TRACE("wndEvent", 9) << "Accept event: " << qevent->type()
                              << " " << getEventName(qevent->type()) << ".";
    } else {
      FQ_TRACE("wndEvent", 9) << "Ignore event: " << qevent->type()
                              << " " << getEventName(qevent->type()) << ".";
    }
  }
  return res;
}

void FQTermWindow::mouseDoubleClickEvent(QMouseEvent *mouseevent) {
  //pythonMouseEvent(3, me->button(), me->state(), me->pos(),0);
}

void FQTermWindow::mousePressEvent(QMouseEvent *mouseevent) {
  // stop  the tab blinking
  stopBlink();

  // Left Button for selecting
  if (mouseevent->button() & Qt::LeftButton && !(mouseevent->modifiers())) {
    startSelecting(mouseevent->pos());
  }

  // Right Button
  if ((mouseevent->button() & Qt::RightButton)) {
    if (mouseevent->modifiers() & Qt::ControlModifier) {
      // on Url
      if (!session_->getUrl().isEmpty()) {
        previewLink();
      }
      return ;
    }

#ifdef __APPLE__
    bool additional_modifier = (mouseevent->modifiers() & !Qt::MetaModifier);
#else
    bool additional_modifier = mouseevent->modifiers();
#endif

    if (!(additional_modifier)) {
      // on Url
      if (!session_->getUrl().isEmpty()) {
        urlMenu_->popup(mouseevent->globalPos());
      } else {
        menu_->popup(mouseevent->globalPos());
      }
      return ;
    }
  }
  // Middle Button for paste
  if (mouseevent->button() &Qt::MidButton && !(mouseevent->modifiers())) {
    if (isConnected())
      // on Url
      if (!session_->getUrl().isEmpty()) {
        previewLink();
      } else {
        pasteHelper(false);
      }
    return ;
  }

  // If it is a click, there should be a press event and a release event.
  isMouseClicked_ = true;

  // xterm mouse event
  //session_->sendMouseState(0, me->button(), me->state(), me->pos());

  // python mouse event
  //pythonMouseEvent(0, me->button(), me->state(), me->pos(),0);

}


void FQTermWindow::mouseMoveEvent(QMouseEvent *mouseevent) {
  QPoint position = mouseevent->pos();

  // selecting by leftbutton
  if ((mouseevent->buttons() &Qt::LeftButton) && isSelecting_) {
    onSelecting(position);
  }

  if (!(session_->isMouseSupported_ && isConnected())) {
    return;
  }

  setCursorPosition(position);

  setCursorType(position);



  if (!isUrlUnderLined_ && session_->urlStartPoint_ != session_->urlEndPoint_) {
    isUrlUnderLined_ = true;
    urlStartPoint_ = session_->urlStartPoint_;
    urlEndPoint_ = session_->urlEndPoint_;
	  clientRect_ = QRect(QPoint(0, urlStartPoint_.y()), QSize(session_->getBuffer()->getNumColumns(), urlEndPoint_.y() - urlStartPoint_.y() + 1));
	  repaintScreen();
	  
  } else if (isUrlUnderLined_ && (session_->urlStartPoint_ != urlStartPoint_ || session_->urlEndPoint_ != urlEndPoint_)) {
    clientRect_ = QRect(QPoint(0, urlStartPoint_.y()), QSize(session_->getBuffer()->getNumColumns(), urlEndPoint_.y() - urlStartPoint_.y() + 1));
    urlStartPoint_ = QPoint();
    urlEndPoint_ = QPoint();
	  repaintScreen();
	  isUrlUnderLined_ = false;

  }

  // python mouse event
  //pythonMouseEvent(2, me->button(), me->state(), position,0);
}

static bool isSupportedImage(const QString &name) {
  static QList<QByteArray> image_formats =
      QImageReader::supportedImageFormats();

  return image_formats.contains(name.section(".", -1).toLower().toUtf8());
}

void FQTermWindow::mouseReleaseEvent(QMouseEvent *mouseevent) {
  if (!isMouseClicked_) {
    return ;
  }
  isMouseClicked_ = false;
  // other than Left Button ignored
  if (!(mouseevent->button() & Qt::LeftButton) ||
      (mouseevent->modifiers() & Qt::KeyboardModifierMask)) {
    //pythonMouseEvent(1, me->button(), me->state(), me->pos(),0);
    // no local mouse event
    //session_->sendMouseState(3, me->button(), me->state(), me->pos());
    return ;
  }

  // Left Button for selecting
  QPoint currentMouseCell = screen_->mapToChar(mouseevent->pos());
  if (currentMouseCell != lastMouseCell_ && isSelecting_) {
    finishSelecting(mouseevent->pos());
    return ;
  }
  isSelecting_ = false;

  if (!session_->isMouseSupported_ || !isConnected()) {
    return ;
  }

  // url
  QString url = session_->getUrl();
  if (!url.isEmpty()) {
    if (isSupportedImage(url)) {
      previewLink();      
    } else {
      openUrl();
    }
    return ;
  }

  processLClick(currentMouseCell);
}

void FQTermWindow::wheelEvent(QWheelEvent *wheelevent) {
  int j = wheelevent->delta() > 0 ? 4 : 5;
  if (!(wheelevent->modifiers())) {
    if (frame_->preference_.isWheelSupported_ && isConnected()) {
      session_->write(directions_[j], sizeof(directions_[j]));
    }
    return ;
  }

  //pythonMouseEvent(4, Qt::NoButton, we->state(), we->pos(),we->delta());

  //session_->sendMouseState(j, Qt::NoButton, we->state(), we->pos());
}

//keyboard input event
void FQTermWindow::keyPressEvent(QKeyEvent *keyevent) {
  if (keyevent->isAutoRepeat() && !session_->getBuffer()->isAutoRepeatMode()) {
    FQ_TRACE("sendkey", 5)
        << "The terminal is set to not allow repeated key events.";
    keyevent->accept();
    return;
  }
  
#ifdef HAVE_PYTHON
  int state = 0;
  if (keyevent->modifiers() &Qt::AltModifier) {
    state |= 0x08;
  }
  if (keyevent->modifiers() &Qt::ControlModifier) {
    state |= 0x10;
  }
  if (keyevent->modifiers() &Qt::ShiftModifier) {
    state |= 0x20;
  }
  session_->pythonCallback(
      "keyEvent", Py_BuildValue("liii", this, 0, state, keyevent->key()));
#endif

  keyevent->accept();

  if (!isConnected()) {
    if (keyevent->key() == Qt::Key_Return) {
      session_->reconnect();
    }
    return ;
  }

  // stop  the tab blinking
  stopBlink();
  if (!session_->readyForInput()) {
    return;
  }
  // message replying
  session_->leaveIdle();

  Qt::KeyboardModifiers modifier = QApplication::keyboardModifiers();
  int key = keyevent->key();
  sendKey(key, modifier, keyevent->text());
}

void FQTermWindow::focusInEvent (QFocusEvent *event) {
  QMainWindow::focusInEvent(event);
  screen_->setFocus(Qt::OtherFocusReason);
}


//connect slot
void FQTermWindow::connectHost() {
  session_->setProxy(session_->param_.proxyType_,
                     session_->param_.isAuthentation_,
                     session_->param_.proxyHostName_,
                     session_->param_.proxyPort_,
                     session_->param_.proxyUserName_,
                     session_->param_.proxyPassword_);
  session_->connectHost(session_->param_.hostAddress_, session_->param_.port_);
}

bool FQTermWindow::isConnected() {
  return session_->isConnected_;
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           Telnet State                                   */
/*                                                                          */
/* ------------------------------------------------------------------------ */

void FQTermWindow::sessionUpdated() {
  refreshScreen();
  //send a mouse move event to make mouse-related change
  QMouseEvent* me = new QMouseEvent(
      QEvent::MouseMove, mapFromGlobal(QCursor::pos()),
      QCursor::pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QApplication::postEvent(this, me);
}

void FQTermWindow::requestUserPwd(QString *user, QString *pwd, bool *isOK) {
  SSHLoginDialog login(user, pwd, this);
  *isOK = (login.exec() == QDialog::Accepted);
}

void FQTermWindow::ZmodemState(int type, int value, const char *status) {
  QString strMsg;
  //to be completed
  switch (type) {
    case RcvByteCount:
    case SndByteCount:
      zmodemDialog_->setProgress(value);
      return;
    case RcvTimeout:
      /* receiver did not respond, aborting */
      strMsg = QString(tr("Time out!"));
      break;
    case SndTimeout:
      /* value is # of consecutive send timeouts */
      strMsg = QString(tr("Time out after trying %1 times")).arg(value);
      break;
    case RmtCancel:
      /* remote end has cancelled */
      strMsg = QString(tr("Canceled by remote peer %1")).arg(status);
      break;
    case ProtocolErr:
      /* protocol error has occurred, val=hdr */
      strMsg = QString(
          tr("Unhandled header %1 at state %2")).arg(value).arg(status);
      break;
    case RemoteMessage:
      /* message from remote end */
      strMsg = QString(tr("Msg from remote peer: %1")).arg(status);
      break;
    case DataErr:
      /* data error, val=error count */
      strMsg = QString(tr("Data errors %1")).arg(value);
      break;
    case FileErr:
      /* error writing file, val=errno */
      strMsg = QString(tr("Falied to write file"));
      break;
    case FileBegin:
      /* file transfer begins, str=name */
      FQ_TRACE("window", 1) << "Starting file " << status;
      if (session_->param_.serverEncodingID_ == 0) {
        zmodemDialog_->setFileInfo(G2U(status), value);
      } else {
        zmodemDialog_->setFileInfo(B2U(status), value);
      }
      
      zmodemDialog_->setProgress(0);
      zmodemDialog_->clearErrorLog();
      zmodemDialog_->show();
      zmodemDialog_->setModal(true);
      return;
    case FileEnd:
      /* file transfer ends, str=name */
      zmodemDialog_->hide();
      return;
    case FileSkip:
      /* file being skipped, str=name */
      strMsg = QString(tr("Skipping file %1")).arg(status);
      break;
  }
  zmodemDialog_->addErrorLog(strMsg);
}

// telnet state slot
void FQTermWindow::TelnetState(int state) {
  switch (state) {
    case TSRESOLVING:
      pageViewMessage_->display(tr("Resolving host name"));
      break;
    case TSHOSTFOUND:
      pageViewMessage_->display(tr("Host found"));
      break;
    case TSHOSTNOTFOUND:
      pageViewMessage_->display(tr("Host not found"));
      break;
    case TSCONNECTING:
      pageViewMessage_->display(tr("Connecting..."));
      break;
    case TSHOSTCONNECTED:
      pageViewMessage_->display(tr("Connected"));
      frame_->updateMenuToolBar();
      break;
    case TSPROXYCONNECTED:
      pageViewMessage_->display(tr("Connected to proxy"));
      break;
    case TSPROXYAUTH:
      pageViewMessage_->display(tr("Proxy authentation"));
      break;
    case TSPROXYFAIL:
      pageViewMessage_->display(tr("Proxy failed"));
      break;
    case TSREFUSED:
      pageViewMessage_->display(tr("Connection refused"));
      break;
    case TSREADERROR:
      pageViewMessage_->display(tr("Error when reading from server"),
                                PageViewMessage::Error);
      break;
    case TSCLOSED:
      pageViewMessage_->display(tr("Connection closed"));
      break;
    case TSCLOSEFINISH:
      pageViewMessage_->display(tr("Connection close finished"));
      break;
    case TSCONNECTVIAPROXY:
      pageViewMessage_->display(tr("Connect to host via proxy"));
      break;
    case TSEGETHOSTBYNAME:
      pageViewMessage_->display(
          tr("Error in gethostbyname"), PageViewMessage::Error);
      break;
    case TSEINIWINSOCK:
      pageViewMessage_->display(
          tr("Error in startup winsock"), PageViewMessage::Error);
      break;
    case TSERROR:
      pageViewMessage_->display(
          tr("Error in connection"), PageViewMessage::Error);
      break;
    case TSPROXYERROR:
      pageViewMessage_->display(tr("Error in proxy"), PageViewMessage::Error);
      break;
    case TSWRITED:
      break;
  }
}

void FQTermWindow::showSessionErrorMessage(const char *reason) {
  if (QString(reason) == tr("User Cancel")) {
    return;
  }
  QMessageBox::critical(this, tr("Session error"), tr(reason));
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           UI Slots                                       */
/*                                                                          */
/* ------------------------------------------------------------------------ */


void FQTermWindow::copy() {
  QClipboard *clipboard = QApplication::clipboard();

  QString selected_text = session_->getBuffer()->getTextSelected(
      session_->param_.isRectSelect_,
      session_->param_.isColorCopy_,
      parseString((const char*)session_->param_.escapeString_.toLatin1()));

  // TODO_UTF16: there are three modes: Clipboard, Selection, FindBuffer.
  clipboard->setText(selected_text, QClipboard::Clipboard);
  clipboard->setText(selected_text, QClipboard::Selection);
}

void FQTermWindow::paste() {
  pasteHelper(true);
}

void FQTermWindow::pasteHelper(bool clip) {
  if (!isConnected()) {
    return ;
  }

  // TODO_UTF16: there are three modes: Clipboard, Selection, FindBuffer.
  QClipboard *clipboard = QApplication::clipboard();
  QByteArray cstrText;

  // TODO_UTF16: what's the termFrame_->clipboardEncodingID_?
  /*
    if (termFrame_->clipboardEncodingID_ == 0) {
    if (clip) {
    cstrText = U2G(clipboard->text(QClipboard::Clipboard));
    }
    else {
    cstrText = U2G(clipboard->text(QClipboard::Selection));
    }

    if (session_->param_.serverEncodingID_ == 1) {
    char *str = encodingConverter_.G2B(cstrText, cstrText.length());
    cstrText = str;
    delete [] str;
    }
    } else {
    if (clip) {
    cstrText = U2B(clipboard->text(QClipboard::Clipboard));
    } else {
    cstrText = U2B(clipboard->text(QClipboard::Selection));
    }

    if (session_->param_.serverEncodingID_ == 0) {
    char *str = encodingConverter_.B2G(cstrText, cstrText.length());
    cstrText = str;
    delete [] str;
    }
    }
  */

  QString clipStr;

  if (clip) {
    clipStr = clipboard->text(QClipboard::Clipboard);
  } else {
    clipStr = clipboard->text(QClipboard::Selection);
  }

  if (session_->param_.serverEncodingID_ == 0) {
    cstrText = U2G(clipStr);
  } else  if (session_->param_.serverEncodingID_ == 1) {
    cstrText = U2B(clipStr);
  } else {
    FQ_VERIFY(false);
  }

  // TODO_UTF16: avoid this replacement.
  if (!frame_->escapeString_.isEmpty()) {
    cstrText.replace(
        parseString(frame_->escapeString_.toLatin1()),
        parseString((const char*)session_->param_.escapeString_.toLatin1()));
  }

  if (session_->isWordWrap_) {
    // convert to unicode for word wrap
    QString strText;
    if (session_->param_.serverEncodingID_ == 0) {
      strText = G2U(cstrText);
    } else {
      strText = B2U(cstrText);
    }

    // insert '\n' as needed
    for (uint i = 0; (long)i < strText.length(); i++) {
      uint j = i;
      uint k = 0, l = 0;
      while (strText.at(j) != QChar('\n') && (long)j < strText.length()) {
        if (frame_->preference_.widthToWrapWord_ - (l - k) >= 0
            && frame_->preference_.widthToWrapWord_ - (l - k) < 2) {
          strText.insert(j, QChar('\n'));
          k = l;
          j++;
          break;
        }
        // double byte or not
        if (strText.at(j).row() == '\0') {
          l++;
        } else {
          l += 2;
        }
        j++;
      }
      i = j;
    }

    if (session_->param_.serverEncodingID_ == 0) {
      cstrText = U2G(strText);
    } else {
      cstrText = U2B(strText);
    }
  }

  session_->write(cstrText, cstrText.length());
}

void FQTermWindow::copyArticle() {
  if (!isConnected()) {
    return ;
  }

  session_->copyArticle();
}

void FQTermWindow::setColor() {
  addrDialog set(this, session_->param_);

  set.setCurrentTabIndex(addrDialog::Display);

  if (set.exec() == 1) {
    updateSetting(set.param());
  }

}

void FQTermWindow::disconnect() {
  session_->close();

  connectionClosed();
}

void FQTermWindow::showIP(bool show) {
  pageViewMessage_->hide();
  if (!show) {
    return;
  }
  QString country, city;
  QString url = session_->getIP();
  if (ipDatabase_->getLocation(url, country, city)) {

    QRect screenRect = screen_->rect();
    QPoint messagePos;
    QRect globalIPRectangle = QRect(screen_->mapToRect(ipRectangle_));
    QFontMetrics fm(qApp->font());
    int charHeight = fm.height();

    int midLine = (screenRect.top() + screenRect.bottom()) / 2;
    int ipMidLine = (globalIPRectangle.top() + globalIPRectangle.bottom()) / 2;
    if (ipMidLine < midLine) {
      // "There is Plenty of Room at the Bottom." -- Feyman, 1959
      messagePos.setY(globalIPRectangle.bottom() + 0.618 * charHeight);
    } else {
      messagePos.setY(globalIPRectangle.top()
                      - 0.618 * charHeight
                      - pageViewMessage_->size().height());
    }

    QRect messageSize(
        pageViewMessage_->displayCheck(
        session_->param_.serverEncodingID_?B2U((country + city).toLatin1()):G2U((country + city).toLatin1()),
        PageViewMessage::Info));
    PageViewMessage::Alignment ali;
    if (messageSize.width() + globalIPRectangle.left() >= screenRect.right()) {
      //"But There is No Room at the Right" -- Curvelet, 2007
      messagePos.setX(globalIPRectangle.right());
      ali = PageViewMessage::TopRight;
    } else {
      messagePos.setX(globalIPRectangle.left());
      ali = PageViewMessage::TopLeft;
    }

    pageViewMessage_->display(
        session_->param_.serverEncodingID_?B2U((country + city).toLatin1()):G2U((country + city).toLatin1()),
        PageViewMessage::Info,
        0, messagePos, ali);
  }
}

void FQTermWindow::showStatusBar(bool bShow) {
  // TODO: remove status bar from sub window.
  if (bShow) {
    statusBar()->show();
  } else {
    statusBar()->hide();
  }
}

void FQTermWindow::runScript() {
  // get the previous dir
  QStringList fileList;
  FQTermFileDialog fileDialog(config_);

  fileList = fileDialog.getOpenNames("Choose a JAVA script file", "JavaScript File (*.js)");

  if (!fileList.isEmpty() && fileList.count() == 1) {
    runScript(fileList.at(0));
  }
}

void FQTermWindow::stopScript() {
}

void FQTermWindow::viewMessages() {
  msgDialog msg(this);

  QByteArray dlgSize =
//      frame_->config()->getItemValue("global", "msgdialog").toLatin1();
    config_->getItemValue("global", "msgdialog").toLatin1();
  const char *dsize = dlgSize.constData();
  if (!dlgSize.isEmpty()) {
    int x, y, cx, cy;
    sscanf(dsize, "%d %d %d %d", &x, &y, &cx, &cy);
    msg.resize(QSize(cx, cy));
    msg.move(QPoint(x, y));
  } else {
    msg.resize(QSize(300, 500));
    msg.move(QPoint(20,20));
  }

  msg.setMessageText(allMessages_);
  msg.exec();

  QString strSize = QString("%1 %2 %3 %4").arg(msg.x()).arg(msg.y()).arg
                    (msg.width()).arg(msg.height());
//  frame_->config()->setItemValue("global", "msgdialog", strSize);
    config_->setItemValue("global", "msgdialog", strSize);
}

void FQTermWindow::setting() {
  addrDialog set(this, session_->param_);
  if (set.exec() == 1) {
    updateSetting(set.param());
  }
}

void FQTermWindow::toggleAntiIdle() {
  session_->setAntiIdle(!session_->isAntiIdle());
}

void FQTermWindow::toggleAutoReply() {
  session_->setAutoReply(!session_->isAutoReply());
}

void FQTermWindow::connectionClosed() {
  stopBlink();

  pageViewMessage_->display(tr("Connection closed"));

  frame_->updateMenuToolBar();

  setCursor(cursors_[FQTermSession::kNormal]);

  refreshScreen();
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           Events                                         */
/*                                                                          */
/* ------------------------------------------------------------------------ */

void FQTermWindow::articleCopied(int e, const QString content) {
  if (e == DAE_FINISH) {
//    articleDialog article(frame_->config(), this);
    articleDialog article(config_, this);

    // Fix focus-losing bug.
    // dsize should be a pointer to a non-temprary object.
    QByteArray dlgSize =
//        frame_->config()->getItemValue("global", "articledialog").toLatin1();
      config_->getItemValue("global", "articledialog").toLatin1();

    const char *dsize = dlgSize.constData();

    if (!dlgSize.isEmpty()) {
      int x, y, cx, cy;
      sscanf(dsize, "%d %d %d %d", &x, &y, &cx, &cy);
      article.resize(QSize(cx, cy));
      article.move(QPoint(x, y));
    } else {
      article.resize(QSize(300, 500));
      article.move(20,20);
    }
    article.articleText_ = content;

    article.ui_.textBrowser->setPlainText(article.articleText_);
    article.exec();
    QString strSize = QString("%1 %2 %3 %4").arg(article.x()).arg(article.y())
                      .arg(article.width()).arg(article.height());
//    frame_->config()->setItemValue("global", "articledialog", strSize);
    config_->setItemValue("global", "articledialog", strSize);
  } else if (e == DAE_TIMEOUT) {
    QMessageBox::warning(this, "timeout", "download article timeout, aborted");
  } else if (e == PYE_ERROR) {
    QMessageBox::warning(this, "Python script error", pythonErrorMessage_);
  } else if (e == PYE_FINISH) {
    QMessageBox::information(this, "Python script finished",
                             "Python script file executed successfully");
  }
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           Aux Func                                       */
/*                                                                          */
/* ------------------------------------------------------------------------ */

QByteArray FQTermWindow::parseString(const QByteArray &cstr, int *len) {
  QByteArray parsed = "";

  if (len != 0) {
    *len = 0;
  }

  for (uint i = 0; (long)i < cstr.length(); i++) {
    if (cstr.at(i) == '^') {
      i++;
      if ((long)i < cstr.length()) {
        parsed += FQTERM_CTRL(cstr.at(i));
        if (len != 0) {
          *len =  *len + 1;
        }
      }
    } else if (cstr.at(i) == '\\') {
        i++;
        if ((long)i < cstr.length()) {
          if (cstr.at(i) == 'n') {
            parsed += CHAR_CR;
          } else if (cstr.at(i) == 'r') {
            parsed += CHAR_LF;
          } else if (cstr.at(i) == 't') {
            parsed += CHAR_TAB;
          }
          if (len != 0) {
            *len =  *len + 1;
          }
        }
      } else {
        parsed += cstr.at(i);
        if (len != 0) {
          *len =  *len + 1;
        }
      }
  }

  return parsed;
}

void FQTermWindow::messageAutoReplied() {
  pageViewMessage_->display("You have messages", PageViewMessage::Info, 0);
}

void FQTermWindow::saveSetting() {
  if (addressIndex_ == -1) {
    return ;
  }

  //save these options silently
  FQTermConfig pConf(getPath(USER_CONFIG) + "address.cfg");
  FQTermParam param;
  loadAddress(&pConf, addressIndex_, param);
  session_->param_.isAutoCopy_ = session_->param_.isAutoCopy_;
  session_->param_.isRectSelect_ = session_->param_.isRectSelect_;
  param.isAutoCopy_ = session_->param_.isAutoCopy_;
  param.isColorCopy_ = session_->param_.isColorCopy_;
  param.isRectSelect_ = session_->param_.isRectSelect_;
  saveAddress(&pConf, addressIndex_, param);
  pConf.save(getPath(USER_CONFIG) + "address.cfg");

  if (!isAddressChanged_) {
    return;
  }
  
  QMessageBox mb("FQTerm", "Setting changed do you want to save it?",
                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::Default,
                 QMessageBox::No | QMessageBox::Escape, 0, this);
  if (mb.exec() == QMessageBox::Yes) {
    saveAddress(&pConf, addressIndex_, session_->param_);
    pConf.save(getPath(USER_CONFIG) + "address.cfg");
  }
}

void FQTermWindow::externInput(const QByteArray &cstrText) {
  QByteArray cstrParsed = parseString(cstrText);
  session_->write(cstrParsed, cstrParsed.length());
}

void FQTermWindow::externInput(const QString &cstrText) {
  if (screen_->encoding()) {
    externInput(screen_->encoding()->fromUnicode(cstrText));
  } else {
    externInput(cstrText.toAscii());
  }
}
QByteArray FQTermWindow::unicode2SessionEncoding(const QString &text) {
  QByteArray strTmp;

  if (frame_->preference_.serverEncodingID_ == 0) {
    strTmp = U2G(text);
    if (session_->param_.serverEncodingID_ == 1) {
      char *str = encodingConverter_.G2B(strTmp, strTmp.length());
      strTmp = str;
      delete [] str;
    }
  } else {
    strTmp = U2B(text);
    if (session_->param_.serverEncodingID_ == 0) {
      char *str = encodingConverter_.B2G(strTmp, strTmp.length());
      strTmp = str;
      delete [] str;
    }
  }

  return strTmp;
}

void FQTermWindow::sendParsedString(const char *str) {
  int length;
  QByteArray cstr = parseString(str, &length);
  session_->write(cstr, length);
}

// void FQTermWindow::setMouseMode(bool on) {
//   isMouseX11_ = on;
// }

//  /* 0-left 1-middle 2-right 3-relsase 4/5-wheel
//   *
//   */
//  //void FQTermWindow::sendMouseState( int num,
//  //    Qt::ButtonState btnstate, Qt::ButtonState keystate, const QPoint& pt )
//  void FQTermWindow::sendMouseState(int num, Qt::KeyboardModifier btnstate,
//                           Qt::KeyboardModifier keystate, const QPoint &pt) {
//    /*
//      if(!m_bMouseX11)
//      return;
//
//      QPoint ptc = screen_->mapToChar(pt);
//
//      if(btnstate&Qt::LeftButton)
//      num = num==0?0:num;
//      else if(btnstate&Qt::MidButton)
//      num = num==0?1:num;
//      else if(btnstate&Qt::RightButton)
//      num = num==0?2:num;
//
//      int state = 0;
//      if(keystate&Qt::ShiftModifier)
//      state |= 0x04;
//      if(keystate&Qt::MetaModifier)
//      state |= 0x08;
//      if(keystate&Qt::ControlModifier)
//      state |= 0x10;
//
//      // normal buttons are passed as 0x20 + button,
//      // mouse wheel (buttons 4,5) as 0x5c + button
//      if(num>3) num += 0x3c;
//
//      char mouse[10];
//      sprintf(mouse, "\033[M%c%c%c",
//      num+state+0x20,
//      ptc.x()+1+0x20,
//      ptc.y()+1+0x20);
//      m_pTelnet->write( mouse, strlen(mouse) );
//    */
//  }

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           Python Func                                    */
/*                                                                          */
/* ------------------------------------------------------------------------ */

void FQTermWindow::pythonMouseEvent(
    int type, Qt::KeyboardModifier, Qt::KeyboardModifier,
    const QPoint &pt, int delta) {
  /*
    int state=0;

    if(btnstate&Qt::LeftButton)
    state |= 0x01;
    if(btnstate&Qt::RightButton)
    state |= 0x02;
    if(btnstate&Qt::MidButton)
    state |= 0x04;

    if(keystate&Qt::AltModifier)
    state |= 0x08;
    if(keystate&Qt::ControlModifier)
    state |= 0x10;
    if(keystate&Qt::ShiftModifier)
    state |= 0x20;

    QPoint ptc = screen_->mapToChar(pt);

    #ifdef HAVE_PYTHON
    pythonCallback("mouseEvent",
    Py_BuildValue("liiiii", this, type, state, ptc.x(), ptc.y(),delta));
    #endif
  */
}

/* ------------------------------------------------------------------------ */
/*                                                                         */
/*                           HTTP Func                                      */
/*                                                                          */
/* ------------------------------------------------------------------------ */

void FQTermWindow::openLink() {
  QString url = session_->getUrl();
  if (!url.isEmpty()) {
    const QString &httpBrowser = frame_->preference_.httpBrowser_;
	if (httpBrowser.isNull() || httpBrowser.isEmpty()) {
#if QT_VERSION >= 0x040400
    QDesktopServices::openUrl(QUrl::fromEncoded(url.toLocal8Bit()));
#else
    QDesktopServices::openUrl(url);
#endif
    } else {
	  runProgram(httpBrowser, url);
	}
  }
}

void FQTermWindow::saveLink() {
  QString url = session_->getUrl();
  if (!url.isEmpty())
    getHttpHelper(url, false);
}

void FQTermWindow::openUrl()
{
  QString caption = tr("URL Dialog");
  QString strUrl = session_->getUrl();
  QTextEdit *textEdit = new QTextEdit();
  textEdit->setPlainText(strUrl);
  //textEdit->setFocus();
  textEdit->setTabChangesFocus(true);
  textEdit->moveCursor(QTextCursor::End);
  

  QPushButton *okButton = new QPushButton(tr("&Open URL"));
  QPushButton *cancelButton = new QPushButton(tr("&Cancel"));
//  okButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
//  cancelButton->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  

  QGridLayout *layout = new QGridLayout;
  layout->addWidget(textEdit, 0, 0, 1, -1);
  layout->addWidget(okButton, 1, 3);
  layout->addWidget(cancelButton, 1, 4);

  QDialog dlg(this);

  QFontMetrics fm(qApp->font());
  

  connect(okButton, SIGNAL(pressed()), &dlg, SLOT(accept()));
  connect(cancelButton, SIGNAL(pressed()), &dlg, SLOT(reject()));

  
  dlg.setWindowTitle(caption);
  dlg.setLayout(layout);
  dlg.resize(fm.width(QString(30, 'W')), fm.height() * (fm.width(strUrl) / fm.width(QString(30, 'W')) + 5));
  textEdit->clearFocus();
  okButton->setFocus(Qt::TabFocusReason);
  if (dlg.exec()) {
    const QString &httpBrowser = frame_->preference_.httpBrowser_;
    if (httpBrowser.isNull() || httpBrowser.isEmpty()) {
#if QT_VERSION >= 0x040400
	    QDesktopServices::openUrl(QUrl::fromEncoded(textEdit->toPlainText().toLocal8Bit()));
#else
      QDesktopServices::openUrl(textEdit->toPlainText());
#endif
    } else {
	    runProgram(httpBrowser, textEdit->toPlainText());
    }
    
  }
}

void FQTermWindow::previewLink() {
  getHttpHelper(session_->getUrl(), true);
}

void FQTermWindow::copyLink() {
  QString strUrl;
  if (session_->param_.serverEncodingID_ == 0) {
    strUrl = G2U(session_->getUrl().toLatin1());
  } else {
    strUrl = B2U(session_->getUrl().toLatin1());
  }

  QClipboard *clipboard = QApplication::clipboard();

  clipboard->setText(strUrl, QClipboard::Selection);
  clipboard->setText(strUrl, QClipboard::Clipboard);
}

void FQTermWindow::getHttpHelper(const QString &url, bool preview) {

  const QString &strPool = frame_->preference_.zmodemPoolDir_;

  FQTermHttp *http = new FQTermHttp(config_, this, strPool, session_->serverEncodingID_);
  FQTerm::StatusBar::instance()->newProgressOperation(http).setDescription(tr("Waiting header...")).setAbortSlot(http, SLOT(cancel())).setMaximum(100);
  FQTerm::StatusBar::instance()->resetMainText();
  FQTerm::StatusBar::instance()->setProgress(http, 0);

  FQ_VERIFY(connect(http, SIGNAL(headerReceived(FQTermHttp *, const QString &)),
                    this, SLOT(startHttpDownload(FQTermHttp*,const QString&))));

  FQ_VERIFY(connect(http,SIGNAL(done(QObject*)),this,SLOT(httpDone(QObject*))));

  FQ_VERIFY(connect(http, SIGNAL(message(const QString &)),
                    pageViewMessage_, SLOT(showText(const QString &))));

  FQ_VERIFY(connect(http, SIGNAL(done(QObject*)),
                    FQTerm::StatusBar::instance(),
                    SLOT(endProgressOperation(QObject*))));

  FQ_VERIFY(connect(http, SIGNAL(percent(int)),
                    FQTerm::StatusBar::instance(), SLOT(setProgress(int))));

  FQ_VERIFY(connect(http, SIGNAL(previewImage(const QString &, bool, bool)),
                    this, SLOT(httpPreviewImage(const QString &, bool, bool))));


  http->getLink(url, preview);
}

void FQTermWindow::startHttpDownload(
    FQTermHttp *pHttp, const QString &filedesp) {
  FQTerm::StatusBar::instance()->newProgressOperation(pHttp).setDescription
      (filedesp).setAbortSlot(pHttp, SLOT(cancel())).setMaximum(100);

  FQTerm::StatusBar::instance()->resetMainText();
}

void FQTermWindow::httpDone(QObject *pHttp) {
  pHttp->deleteLater();
}

void FQTermWindow::httpPreviewImage(const QString &filename, bool raiseViewer, bool done) 
{
  if (config_->getItemValue("preference", "image").isEmpty() || done) {
    previewImage(filename, raiseViewer);
  }
}

void FQTermWindow::previewImage(const QString &filename, bool raiseViewer) {
//  QString strViewer = frame_->config()->getItemValue("preference", "image");
  QString strViewer = config_->getItemValue("preference", "image");

  if (strViewer.isEmpty()) {
    frame_->viewImages(filename, raiseViewer);
  } else if(raiseViewer) {
    runProgram(strViewer, filename);
  }

}

void FQTermWindow::beep() {
  if (session_->isBeep_) {
    if (frame_->preference_.beepSoundFileName_.isEmpty() ||
      frame_->preference_.openBeep_ == 3) {
      qApp->beep();
    }
    else {
      sound_ = NULL;

      switch (frame_->preference_.beepMethodID_) {
        case 0:
          sound_ =
              new FQTermSystemSound(frame_->preference_.beepSoundFileName_);
          break;
        case 1:
          sound_ =
              new FQTermExternalSound(frame_->preference_.beepPlayerName_,
                frame_->preference_.beepSoundFileName_);
          break;
      }

      if (sound_) {
        sound_->start();
      }
    }
  }

  QString strMsg = session_->getMessage();
  if (!strMsg.isEmpty()) {
    allMessages_ += strMsg + "\n\n";
  }

  session_->isSendingMessage_ = true;

  if (!isActiveWindow() || frame_->windowManager_->activeWindow() != this) {
    frame_->buzz();
  }
}

void FQTermWindow::startBlink() {
  if (frame_->preference_.openTabBlinking_) {
    if (!tabBlinkTimer_->isActive()) {
      tabBlinkTimer_->start(500);
    }
  }
}

void FQTermWindow::stopBlink() {
  if (tabBlinkTimer_->isActive()) {
    tabBlinkTimer_->stop();
    frame_->windowManager_->blinkTheTab(this, TRUE);
  }
}

void FQTermWindow::setCursorType(const QPoint& mousePosition) {
  QRect rcOld;
  bool isUrl = false;
  if (frame_->preference_.openUrlCheck_) {
    showIP(isIpDataFileExisting_ && session_->isIP(ipRectangle_, rcOld));
    if (session_->isUrl(urlRectangle_, rcOld)) {
      setCursor(Qt::PointingHandCursor);
      isUrl = true;
    }
  }
  if (!isUrl) {
    FQTermSession::CursorType cursorType =
        session_->getCursorType(screen_->mapToChar(mousePosition));
    setCursor(cursors_[cursorType]);
  }
}

void FQTermWindow::setCursorPosition(const QPoint& mousePosition) {
  // set cursor pos, repaint if state changed
  QRect rc;
  if (session_->setCursorPos(screen_->mapToChar(mousePosition), rc)) {
    screen_->repaint(screen_->mapToRect(rc));

    //re-check whether the cursor is still on the selected rectangle -- dp
    QPoint localPos = mapFromGlobal(QCursor::pos()); //local.
    if (!session_->getSelectRect().contains(screen_->mapToChar(localPos)))
    {
      QRect dummyRect;
      rc = session_->getSelectRect();
      session_->setCursorPos(screen_->mapToChar(localPos), dummyRect);
      screen_->repaint(screen_->mapToRect(rc));
      //restore the state.
      session_->setCursorPos(screen_->mapToChar(mousePosition), dummyRect); 
    }
  }
}

void FQTermWindow::refreshScreen() {
  screen_->setPaintState(FQTermScreen::NewData);
  screen_->update();
}

void FQTermWindow::repaintScreen() {

  screen_->setPaintState(FQTermScreen::Repaint);
  if (clientRect_ != QRect()) {
    screen_->repaint(screen_->mapToRect(clientRect_));
  } else {
    screen_->repaint();
  }
  clientRect_ = QRect();
//  screen_->update(clientRect_);
}

void FQTermWindow::enterMenuItem() {
  char cr = CHAR_CR;
  QRect rc = session_->getSelectRect();
  FQTermSession::PageState ps = session_->getPageState();
  switch (ps) {
    case FQTermSession::Menu:
    case FQTermSession::MailMenu:
    case FQTermSession::TOP10:
      if (!rc.isEmpty()) {
        char cMenu = session_->getMenuChar();
        session_->write(&cMenu, 1);
        session_->write(&cr, 1);
      }
      break;
    case FQTermSession::ArticleList:
    case FQTermSession::BoardList:
    case FQTermSession::FriendMailList:
    case FQTermSession::EliteArticleList:
      if (!rc.isEmpty()) {
        int n = rc.y() - session_->getBuffer()->getCaretLine();
        // scroll lines
        int cursorType = n>0?FQTermSession::kDown:FQTermSession::kUp;
        n = qAbs(n);
        while (n) {
          session_->write(directions_[cursorType], 4);
          n--;
        }
      }
      session_->write(&cr, 1);
      break;
    default:
      // TODO: process other PageState.
      break;
  }
}

void FQTermWindow::processLClick(const QPoint& cellClicked) {
  int cursorType = session_->getCursorType(cellClicked);
  switch(cursorType)
  {
    case FQTermSession::kHome:
    case FQTermSession::kEnd:
    case FQTermSession::kPageUp:
    case FQTermSession::kPageDown:
      session_->write(directions_[cursorType], 4);
      break;
    case FQTermSession::kUp:
    case FQTermSession::kDown:
    case FQTermSession::kLeft:
      session_->write(directions_[cursorType], 3);
      break;
    case FQTermSession::kRight:  //Hand
      enterMenuItem();
      break;
    case FQTermSession::kNormal:
      char cr = CHAR_CR;
      session_->write(&cr, 1);
      break;
  }
}

void FQTermWindow::startSelecting(const QPoint& mousePosition) {
  // clear the selected before
  session_->clearSelect();
  sessionUpdated();

  // set the selecting flag
  isSelecting_ = true;
  lastMouseCell_ = screen_->mapToChar(mousePosition);
}

void FQTermWindow::onSelecting(const QPoint& mousePosition) {
  if (mousePosition.y() < childrenRect().top()) {
    screen_->scrollLine(-1);
  }
  if (mousePosition.y() > childrenRect().bottom()) {
    screen_->scrollLine(1);
  }

  QPoint curMouseCell = screen_->mapToChar(mousePosition);
  session_->setSelect(lastMouseCell_, curMouseCell);
  sessionUpdated();
}

void FQTermWindow::finishSelecting(const QPoint& mousePosition) {
  QPoint currentMouseCell = screen_->mapToChar(mousePosition);
  session_->setSelect(currentMouseCell, lastMouseCell_);
  refreshScreen();
  if (session_->param_.isAutoCopy_) {
    copy();
  }
  isSelecting_ = false;
}

void FQTermWindow::sendKey(const int keyCode, const Qt::KeyboardModifiers modifier,
                           const QString &t) {
  if (keyCode == Qt::Key_Shift || keyCode == Qt::Key_Control
      || keyCode == Qt::Key_Meta || keyCode == Qt::Key_Alt) {
    return;
  } 
  
  int key = keyCode;
  QString text = t;
  
#ifdef __APPLE__
  // On Mac, pressing Command Key (Apple Key) will set the ControlModifier flag.
  bool alt_on = ((modifier & Qt::ControlModifier) || (modifier & Qt::AltModifier));
  bool ctrl_on = (modifier & Qt::MetaModifier);
#else
  bool alt_on = ((modifier & Qt::MetaModifier) || (modifier & Qt::AltModifier));
  bool ctrl_on = (modifier & Qt::ControlModifier);
#endif
  bool shift_on = (modifier & Qt::ShiftModifier);
  
#ifdef __APPLE__
  // Bullshit! Qt4.4.0 on Mac generates weird key evnets.
  // fix them here.

  if (ctrl_on) {
    static const char char_map[][2] = {
      {'@', '\x00'}, {'A', '\x01'}, {'B', '\x02'}, {'C', '\x03'}, {'D', '\x04'},
      {'E', '\x05'}, {'F', '\x06'}, {'G', '\x07'}, {'H', '\x08'}, {'J', '\x0a'},
      {'K', '\x0b'}, {'L', '\x0c'}, {'N', '\x0e'}, {'O', '\x0f'}, {'P', '\x10'},
      {'Q', '\x11'}, {'R', '\x12'}, {'S', '\x13'}, {'T', '\x14'}, {'U', '\x15'},
      {'V', '\x16'}, {'W', '\x17'}, {'X', '\x18'}, {'Y', '\x19'}, {'Z', '\x1a'},
      {'\\', '\x1c'}, {']', '\x1d'}, {'^', '\x1e'}, {'_', '\x1f'}
    };

    const int num = sizeof(char_map)/2;

    int expected_key = key;
        
    for (int i = 0; i < num; ++i) {
      const char *c = char_map[i];
      if (c[0] == key) {
        expected_key = c[1];
        break;
      }
    }
   
    if (expected_key != key) {
      FQ_TRACE("sendkey", 5) << "translate the key code from "
                             << (int)key << "(" << (char)key << ")"
                             << " to "
                             << (int)expected_key
                             << "(" << (char)expected_key << ")"
                             << ", and change the text appropriately." ;
      key = expected_key;
      text.clear();
      text.append((char) expected_key);
    }
  }

  if (!ctrl_on && alt_on && !shift_on) {
    if (text.size() == 0 && 0 <= key && key <= 128) {
      FQ_TRACE("sendkey", 5)
          << "add appropriate text according to the key code.";
      text.append((char)key);
    }
  }
  
  if (!ctrl_on && alt_on && shift_on) {
    // fix the key code when both shift key and meta/alt key are pressed.
    static const char char_map[][2] = {
      {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'},
      {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, 
      {'0', ')'}, {'-', '_'}, {'=', '+'}, {'[', '{'}, {']', '}'}, 
      {'\\', '|'}, {';', ':'}, {'\'', '"'}, {',', '<'},{'.', '>'},
      {'/', '?'}
    };
    const int num = sizeof(char_map)/2;

    int expected_key = key;
        
    for (int i = 0; i < num; ++i) {
      const char *c = char_map[i];
      if (c[0] == key) {
        expected_key = c[1];
        break;
      }
    }

    if (expected_key != key) {
      FQ_TRACE("sendkey", 5) << "translate the key code from "
                             << (int)key << "(" << (char)key << ")"
                             << " to "
                             << (int)expected_key
                             << "(" << (char)expected_key << ")";
      key = expected_key;
    }
        
    if (text.size() == 0) {
      text.append(key);
    } else {
      text[0] = key;
    }
    FQ_TRACE("sendkey", 5)
        << "set the text for this key event as the keycode.";
  }      
#endif

  FQ_TRACE("sendkey", 5) << "(alt " << (alt_on? " on": "off")
                         << ", ctrl " << (ctrl_on? " on": "off")
                         << ", shift " << (shift_on? " on": "off") << ")"
                         << " key: " << key
                         << " text: " << (text.size() == 0 ? "NULL" : text)
                         << " (in hex: "
                         << dumpHexString << text.toStdString()
                         << dumpNormalString << ")";
  
  if (session_->getBuffer()->isAnsiMode()) {
    if (session_->getBuffer()->isCursorMode()) {
      switch(key) {
        case Qt::Key_Up:
          session_->writeStr("\x1bOA");
          return;
        case Qt::Key_Down:
          session_->writeStr("\x1bOB");
          return;
        case Qt::Key_Left:
          session_->writeStr("\x1bOD");
          return;
        case Qt::Key_Right:
          session_->writeStr("\x1bOC");
          return;
      }
    } else {
      switch(key) {
        case Qt::Key_Up:
          session_->writeStr("\x1b[A");
          return;
        case Qt::Key_Down:
          session_->writeStr("\x1b[B");
          return;
        case Qt::Key_Left:
          session_->writeStr("\x1b[D");
          return;
        case Qt::Key_Right:
          session_->writeStr("\x1b[C");
          return;
      }
    }
  } else {
    // VT52 mode
    switch(key) {
      case Qt::Key_Up:
        // Do NOT change the parameter to "\x1bA".
        session_->writeStr("\x1b" "A");
        return;
      case Qt::Key_Down:
        // Do NOT change the parameter to "\x1bB".        
        session_->writeStr("\x1b" "B");
        return;
      case Qt::Key_Left:
        // Do NOT change the parameter to "\x1bD".        
        session_->writeStr("\x1b" "D");
        return;
      case Qt::Key_Right:
        // Do NOT change the parameter to "\x1bC".        
        session_->writeStr("\x1b" "C");
        return;
    }
  }

  if (!session_->getBuffer()->isNumericMode() &&
      (modifier & Qt::KeypadModifier)) {
    switch(key) {
      case Qt::Key_0:
      case Qt::Key_1:
      case Qt::Key_2:
      case Qt::Key_3:
      case Qt::Key_4:
      case Qt::Key_5:
      case Qt::Key_6:
      case Qt::Key_7:
      case Qt::Key_8:
      case Qt::Key_9:
        {
          char tmp[] = "\x1bOp";
          tmp[2] = 'p' + key - Qt::Key_0;
          session_->writeStr(tmp);
        }
        return;
      case Qt::Key_Plus:
        session_->writeStr("\x1bOl");
        return;
      case Qt::Key_Minus:
        session_->writeStr("\x1bOm");
        return;
      case Qt::Key_Period:
        session_->writeStr("\x1bOn");
        return;
      case Qt::Key_Asterisk:
        session_->writeStr("\x1bOM");
        return;
    }
  }

  switch (key) {
    case Qt::Key_Home:
      session_->writeStr(directions_[0]);
      return;
    case Qt::Key_End:
      session_->writeStr(directions_[1]);
      return;
    case Qt::Key_PageUp:
      session_->writeStr(directions_[2]);
      return;
    case Qt::Key_PageDown:
      session_->writeStr(directions_[3]);
      return;
    case Qt::Key_Up:
      session_->writeStr(directions_[4]);
      return;
    case Qt::Key_Down:
      session_->writeStr(directions_[5]);
      return;
    case Qt::Key_Left:
      session_->writeStr(directions_[6]);
      return;
    case Qt::Key_Right:
      session_->writeStr(directions_[7]);
      return;
  }
  
  if (text.length() > 0) {
    if (alt_on) {
      // Use ESC to emulate Alt-XX key.
      session_->writeStr("\x1b");
    }

    if (ctrl_on) {
      switch(key) {
        case Qt::Key_At:     // Ctrl-@
        case Qt::Key_Space:  // Ctrl-SPACE
          // send NULL
          session_->write("\x0", 1);
          return;
        case Qt::Key_AsciiCircum:  // Ctrl-^
        case Qt::Key_AsciiTilde:   // Ctrl-~
        case Qt::Key_QuoteLeft:    // Ctrl-`
          // send RS
          session_->write("\x1e", 1);
          return;
        case Qt::Key_Underscore: // Ctrl-_
        case Qt::Key_Question:   // Ctrl-?
          // send US
          session_->write("\x1f", 1);            
          return;
      }
    }
    
    if (key == Qt::Key_Backspace) {
      if (shift_on) {
        session_->writeStr("\x1b[3~");
      } else {
        session_->writeStr("\x08");
      }
      return;
    }
    
    if (key == Qt::Key_Delete) {
      if (shift_on) {
        session_->writeStr("\x08");
      } else {
        session_->writeStr("\x1b[3~");
      }
      return;
    }

    if (key == Qt::Key_Return) {
      if (session_->getBuffer()->isNewLineMode()) {
        session_->write("\r\n", 2);
      } else {
        session_->write("\r", 1);
      }
      return;
    }
    
    QByteArray cstrTmp = unicode2SessionEncoding(text);
    session_->write(cstrTmp, cstrTmp.length());
  }
  return;
}

void FQTermWindow::forcedRepaintScreen() {
  QResizeEvent *re = new QResizeEvent(screen_->size(), screen_->size());
  QApplication::postEvent(screen_, re);
}

void FQTermWindow::updateSetting(const FQTermParam& param) {
  session_->param_ = param;
  session_->setAntiIdle(session_->isAntiIdle());
  isAddressChanged_ = true;
  screen_->setTermFont(true,
                       QFont(param.englishFontName_,
                             param.englishFontSize_));
  screen_->setTermFont(false,
                       QFont(param.nonEnglishFontName_,
                             param.nonEnglishFontSize_));
  screen_->setSchema();
  isAddressChanged_ = true;
  forcedRepaintScreen();
}

void FQTermWindow::setFont()
{
  bool isEnglish =
       ((QAction*)(sender()))->data().toBool();
  setFont(isEnglish);
}

void FQTermWindow::setFont(bool isEnglish) {
  bool ok;
  QString& fontName = isEnglish?session_->param_.englishFontName_:session_->param_.nonEnglishFontName_;
  int& fontSize = isEnglish?session_->param_.englishFontSize_:session_->param_.nonEnglishFontSize_;
  QFont font(fontName, fontSize);
  font = QFontDialog::getFont(&ok, font);
  if (ok == true) {
    if (frame_->preference_.openAntiAlias_) {
      font.setStyleStrategy(QFont::PreferAntialias);
    }
    screen_->setTermFont(isEnglish, font);
    fontName = font.family();
    fontSize = font.pointSize();
    isAddressChanged_ = true;
    forcedRepaintScreen();
  }
}

void FQTermWindow::runScript(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
      FQ_TRACE("script", 0) << "Unable to open the script file: " << filename;
      QMessageBox::warning(this, tr("FQTerm"),
                           tr("Unable to open the script file\n") + filename,
                           QMessageBox::Close);
      return;
    }

    QString script = QString::fromUtf8(file.readAll());
    QScriptEngine *engine = getScriptEngine();

    QScriptValue result = engine->evaluate(script);
    if (engine->hasUncaughtException()) {
      int line = engine->uncaughtExceptionLineNumber();
      FQ_TRACE("script", 0) << "uncaught exception at line " << line
               << ":" << result.toString();
      QMessageBox::warning(this, tr("FQTerm"),
                           tr("uncaught exception at line ") + QString::number(line)
                           + ":\n" + result.toString(),
                           QMessageBox::Close);
    }
}

template <typename Tp>
QScriptValue qScriptValueFromQObject(QScriptEngine *engine, Tp &qobject) {
    return engine->newQObject(qobject);
}

template <typename Tp>
void qScriptValueToQObject(const QScriptValue &value, Tp &qobject) {   
    qobject = qobject_cast<Tp>(value.toQObject());
}

template <typename Tp>
int qScriptRegisterQObjectMetaType(
    QScriptEngine *engine, const QScriptValue &prototype = QScriptValue()
                                   ) {
    return qScriptRegisterMetaType<Tp>(engine, qScriptValueFromQObject,
                                       qScriptValueToQObject, prototype);
}

QScriptEngine *FQTermWindow::getScriptEngine() {
  if (script_engine_ == NULL) {
    script_engine_ = new QScriptEngine();
    script_engine_->globalObject().setProperty(
        "session", script_engine_->newQObject(session_));
    script_engine_->globalObject().setProperty(
        "sessionWindow", script_engine_->newQObject(this));

    // qScriptRegisterQObjectMetaType<QString *>(script_engine_);
  }

  return script_engine_;
}

void FQTermWindow::clearScriptEngine() {
  delete script_engine_;
  script_engine_ = NULL;
}

}  // namespace FQTerm

#include "fqterm_window.moc"
