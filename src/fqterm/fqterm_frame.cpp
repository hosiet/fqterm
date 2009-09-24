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

#if !defined(WIN32)
#include <unistd.h>
#endif

#if defined(__linux__)
#include <QLocale>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <QDesktopServices>
#include <QDir>
#include <QFontDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QList>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalMapper>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTime>
#include <QToolBar>
#include <QTranslator>
#include <QWindowStateChangeEvent>

#include "aboutdialog.h"
#include "addrdialog.h"
#include "common.h"
#include "keydialog.h"
#include "prefdialog.h"
#include "fqterm.h"

#include "fqterm_path.h"
#include "fqterm_autoupdate.h"
#include "fqterm_config.h"
#include "fqterm_frame.h"
#include "fqterm_param.h"
#include "fqterm_session.h"
#include "fqterm_screen.h"
#include "fqterm_time_label.h"
#include "fqterm_tool_button.h"
#include "fqterm_window.h"
#include "fqterm_wndmgr.h"
#include "imageviewer.h"
#include "quickdialog.h"
#include "statusBar.h"
#include "sitemanager.h"

#include "fqterm_mini_server.h"

namespace FQTerm {

const QString FQTermFrame::qmPrefix[FQTermFrame::translatedModule] =
{"fqterm_", "ui_", "protocol_"};
const QString FQTermFrame::qmPostfix = ".qm";

//constructor
FQTermFrame::FQTermFrame()
    : QMainWindow(0), 
      tray(0),
      menuTray_(0){
  setAttribute(Qt::WA_DeleteOnClose);

  config_ = new FQTermConfig(getPath(USER_CONFIG) + "fqterm.cfg");

#ifdef IMAGE_USE_PICFLOW
  image_ = new FQTermImageFlow(config_, NULL, Qt::Window);
#else
  image_ = new FQTermImageOrigin(config_, NULL, Qt::Window);
#endif


  //create the window manager to deal with the window-tab-icon pairs
  windowManager_ = new FQTermWndMgr(this);
  //windowManager_->setViewMode(QMdiArea::TabbedView);
  //windowManager_->setTabPosition(QTabWidget::South);

  setCentralWidget(windowManager_);

  initTranslator();

  //set menubar
  addMainMenu();
  FQ_VERIFY(connect(this, SIGNAL(changeLanguage()),
                    this, SLOT(recreateMenu())));

  //setup toolbar
  addMainTool();

  // add the custom defined key
  // and load toolbar position
  updateKeyToolBar();
  loadToolBarPosition();

  // diaable some menu & toolbar
  enableMenuToolBar(false);

  // FIXME: !!!create a horizonal layout to hold the tabbar,the reason
  //of why not put the tabbar directly on the status bar is when no
  //tab in the tabbar,the statusbar display a horizonal line,ugly.
  //perhaps there is some function in statusbar to solve this.
  QWidget *hb = new QWidget(statusBar());
  hb->setObjectName("taskbar");
  QHBoxLayout *hbLayout = new QHBoxLayout(hb);
  hbLayout->setMargin(0);
  hbLayout->setObjectName("tasklayout");
  statusBar()->addWidget(hb);
  //create a tabbar in the hbox
  hbLayout->addWidget(windowManager_->tabBar());

  windowMapper_ = new QSignalMapper(this);
  FQ_VERIFY(connect(windowMapper_, SIGNAL(mapped(int)),
    windowManager_->tabBar(), SLOT(setCurrentIndex(int))));


  //create a progress bar to notify the download process
  statusBar_ = new FQTerm::StatusBar(statusBar(), "mainStatusBar");

  statusBar()->addWidget(statusBar_, 0);



#if defined(__APPLE__)
  QString opt(tr("Ctrl"));
#else
  QString opt(tr("Alt"));  
#endif
  
  for (int i = 1; i <= 10; ++i) {
    QAction *idAction = new QAction(this);
    QString shortcut(opt);
    shortcut += "+" + QString("").setNum(i % 10);
    idAction->setShortcut(shortcut);
    connect(idAction, SIGNAL(triggered()), windowMapper_, SLOT(map()));
    windowMapper_->setMapping(idAction, i - 1);
    addAction(idAction);
  }

  actionNextWindow_ = new QAction(this);
  QList<QKeySequence> keySequences;
  keySequences.append(opt + tr("+Right"));
  keySequences.append(QKeySequence::NextChild);
  actionNextWindow_->setShortcuts(keySequences);

  //TODO: wnd mgr should not expose mdiarea apis.
  FQ_VERIFY(connect(actionNextWindow_, SIGNAL(triggered()),
                    windowManager_, SLOT(activateNextWindow())));
  addAction(actionNextWindow_);
  keySequences.clear();


  actionPrevWindow_ = new QAction(this);
  keySequences.append(opt + tr("+Left"));
  keySequences.append(QKeySequence::PreviousChild);
  actionPrevWindow_->setShortcuts(keySequences);
  //TODO: wnd mgr should not expose mdiarea apis.
  FQ_VERIFY(connect(actionPrevWindow_, SIGNAL(triggered()),
                    windowManager_, SLOT(activatePrevWindow())));
  addAction(actionPrevWindow_);

  //initialize all settings
  iniSetting();

  installEventFilter(this);

  FQTermAutoUpdater* autoUpdater =
    new FQTermAutoUpdater(this, config_);
  autoUpdater->checkUpdate();  


//  serverThread_ = new FQTermMiniServerThread();
//  serverThread_->start();
}

//destructor
FQTermFrame::~FQTermFrame() {
  clearTranslator();
  delete image_;
  delete config_;
  delete windowManager_;

//  serverThread_->quit();
//  serverThread_->terminate();
//  serverThread_->wait();
//  delete serverThread_;

}

//initialize setting from fqterm.cfg
void FQTermFrame::iniSetting() {
  QString strTmp;

  strTmp = config_->getItemValue("global", "fullscreen");
  if (strTmp == "1") {
    isFullScreen_ = true;
    actionFullScreen_->setChecked(true);
    showFullScreen();
  } else {
    isFullScreen_ = false;
    //window size
    strTmp = config_->getItemValue("global", "max");
    if (strTmp == "1") {
      showMaximized();
    } else {
      QString size = config_->getItemValue("global", "size");
      if (size != "") {
        int x, y, cx, cy;
        sscanf(size.toLatin1(), "%d %d %d %d", &x, &y, &cx, &cy);
        resize(QSize(cx, cy));
        move(QPoint(x, y));
      }
    }
  }

  theme_ = config_->getItemValue("global", "theme");
  QStyle *style = QStyleFactory::create(theme_);
  if (style) {
    qApp->setStyle(style);
  }

  //language
  updateLanguageMenu();

  //TODO: 
  actionNoEscape_->setChecked(true);
  FQTermPref::getInstance()->escapeString_ = config_->getItemValue("global", "escstr");
  if (FQTermPref::getInstance()->escapeString_ == "") {
    actionNoEscape_->setChecked(true);
  } else if (FQTermPref::getInstance()->escapeString_ == "^[^[[") {
    actionEscEscape_->setChecked(true);
  } else if (FQTermPref::getInstance()->escapeString_ == "^u[") {
    actionUEscape_->setChecked(true);
  } else {
    actionCustomEscape_->setChecked(true);
  }

  strTmp = config_->getItemValue("global", "clipcodec");
  if (strTmp == "0") {
    FQTermPref::getInstance()->clipboardEncodingID_ = 0;
    actionGBK_->setChecked(true);
  } else {
    FQTermPref::getInstance()->clipboardEncodingID_ = 1;
    actionBIG5_->setChecked(true);
  }

  strTmp = config_->getItemValue("global", "vscrollpos");
  if (strTmp == "0") {
    FQTermPref::getInstance()->termScrollBarPosition_ = 0;
  } else if (strTmp == "1") {
    FQTermPref::getInstance()->termScrollBarPosition_ = 1;
  } else {
    FQTermPref::getInstance()->termScrollBarPosition_ = 2;
  }

  strTmp = config_->getItemValue("global", "statusbar");
  FQTermPref::getInstance()->isStatusBarShown_ = (strTmp != "0");
  actionStatus_->setChecked(FQTermPref::getInstance()->isStatusBarShown_);

  strTmp = config_->getItemValue("global", "switchbar");
  isTabBarShown_ = (strTmp != "0");
  actionSwitch_->setChecked(isTabBarShown_);
  if (isTabBarShown_) {
    statusBar()->show();
  } else {
    statusBar()->hide();
  }

  //read sub-window setting.
  strTmp = config_->getItemValue("global", "subwindowmax");
  windowManager_->setSubWindowMax((strTmp != "0"));


  strTmp = config_->getItemValue("global", "subwindowsize");
  if (strTmp != "") {
    //FIXME: In case of sub window size not saved properly.
    int w, h;
    sscanf(strTmp.toLatin1(), "%d %d", &w, &h);
    windowManager_->setSubWindowSize(QSize(w, h));
  } else {
    //Magic Number. Initialize Window Size to Avoid Errors.
    windowManager_->setSubWindowSize(QSize(640, 480));
  }

  FQTermPref::getInstance()->isBossColor_ = false;

  loadPref();

  setUseDock(FQTermPref::getInstance()->openMinimizeToTray_);

  if (FQTermPref::getInstance()->useStyleSheet_) {
    loadStyleSheetFromFile(FQTermPref::getInstance()->styleSheetFile_);
  }
}

void FQTermFrame::loadPref() {
  QString strTmp;
  
  strTmp = config_->getItemValue("preference", "displayoffset");
  FQTermPref::getInstance()->displayOffset_ = strTmp.toInt();
  strTmp = config_->getItemValue("preference", "xim");
  FQTermPref::getInstance()->imeEncodingID_ = strTmp.toInt();
  strTmp = config_->getItemValue("preference", "wordwrap");
  FQTermPref::getInstance()->widthToWrapWord_ = strTmp.toInt();
  //  strTmp = conf->getItemValue("preference", "smartww");
  //  m_pref.bSmartWW = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "wheel");
  FQTermPref::getInstance()->isWheelSupported_ = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "url");
  FQTermPref::getInstance()->openUrlCheck_ = (strTmp != "0");
  //  strTmp = conf->getItemValue("preference", "logmsg");
  //  m_pref.bLogMsg = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "blinktab");
  FQTermPref::getInstance()->openTabBlinking_ = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "warn");
  FQTermPref::getInstance()->openWarnOnClose_ = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "beep");
  FQTermPref::getInstance()->openBeep_ = strTmp.toInt();
  FQTermPref::getInstance()->beepSoundFileName_ = config_->getItemValue("preference", "wavefile");
  strTmp = config_->getItemValue("preference", "http");
  FQTermPref::getInstance()->httpBrowser_ = strTmp;
  //  m_pref.strHttp = strTmp;
  strTmp = config_->getItemValue("preference", "antialias");
  FQTermPref::getInstance()->openAntiAlias_ = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "enq");
  FQTermPref::getInstance()->replyENQ_ = (strTmp != "0");
  strTmp = config_->getItemValue("preference", "tray");
  if (strTmp.isEmpty()) {
#if defined(__APPLE__) || defined(__linux__)
    FQTermPref::getInstance()->openMinimizeToTray_ = false;
#else
    FQTermPref::getInstance()->openMinimizeToTray_ = true;    
#endif
  } else {
    FQTermPref::getInstance()->openMinimizeToTray_ = (strTmp != "0");
  }
  strTmp = config_->getItemValue("preference", "playmethod");
  FQTermPref::getInstance()->beepMethodID_ = strTmp.toInt();
  strTmp = config_->getItemValue("preference", "externalplayer");
  FQTermPref::getInstance()->beepPlayerName_ = strTmp;

  strTmp = config_->getItemValue("preference", "clearpool");
  FQTermPref::getInstance()->needClearZmodemPoolOnClose_ = (strTmp == "1");
  strTmp = config_->getItemValue("preference", "pool");
  FQTermPref::getInstance()->poolDir_ = strTmp.isEmpty() ?
                               getPath(USER_CONFIG) + "pool/": strTmp;
  if (FQTermPref::getInstance()->poolDir_.right(1) != "/") {
    FQTermPref::getInstance()->poolDir_.append('/');
  }
  strTmp = config_->getItemValue("preference", "zmodem");
  FQTermPref::getInstance()->zmodemDir_ = strTmp.isEmpty() ?
                           getPath(USER_CONFIG) + "zmodem/": strTmp;
  if (FQTermPref::getInstance()->zmodemDir_.right(1) != "/") {
    FQTermPref::getInstance()->zmodemDir_.append('/');
  }
  strTmp = config_->getItemValue("preference", "image");
  FQTermPref::getInstance()->imageViewerName_ = strTmp;
  strTmp = config_->getItemValue("preference", "qssfile");
  FQTermPref::getInstance()->styleSheetFile_ = strTmp;
  FQTermPref::getInstance()->useStyleSheet_ = !strTmp.isEmpty();
}

//save current setting to fqterm.cfg
void FQTermFrame::saveSetting() {
  QString strTmp;
  //save font
  config_->setItemValue("global", "font", qApp->font().family());
  strTmp.setNum(QFontInfo(qApp->font()).pointSize());
  config_->setItemValue("global", "pointsize", strTmp);
  strTmp.setNum(QFontInfo(qApp->font()).pixelSize());
  config_->setItemValue("global", "pixelsize", strTmp);

  //save window position and size
  if (isMaximized()) {
    config_->setItemValue("global", "max", "1");
  } else {
    strTmp = QString("%1 %2 %3 %4").arg(x()).arg(y()).arg(width()).arg(height());
    // 		cstrTmp.sprintf("%d %d %d %d",x(),y(),width(),height());
    config_->setItemValue("global", "size", strTmp);
    config_->setItemValue("global", "max", "0");
  }

  if (isFullScreen_) {
    config_->setItemValue("global", "fullscreen", "1");
  } else {
    config_->setItemValue("global", "fullscreen", "0");
  }

  // cstrTmp.setNum(theme);
  config_->setItemValue("global", "theme", theme_);

  strTmp.setNum(FQTermPref::getInstance()->clipboardEncodingID_);
  config_->setItemValue("global", "clipcodec", strTmp);

  config_->setItemValue("global", "escstr", FQTermPref::getInstance()->escapeString_);

  strTmp.setNum(FQTermPref::getInstance()->termScrollBarPosition_);
  config_->setItemValue("global", "vscrollpos", strTmp);

  config_->setItemValue("global", "statusbar", FQTermPref::getInstance()->isStatusBarShown_ ? "1" : "0");
  config_->setItemValue("global", "switchbar", isTabBarShown_ ? "1" : "0");

  //save subwindow setting
  config_->setItemValue("global", "subwindowmax", windowManager_->getSubWindowMax() ? "1" : "0");
  int w = windowManager_->getSubWindowSize().width();
  int h = windowManager_->getSubWindowSize().height();

  strTmp = QString("%1 %2").arg(w).arg(h);
  config_->setItemValue("global", "subwindowsize", strTmp);

  //Save toolbarstate.
  QByteArray state = saveState().toHex();
  strTmp = QString(state);
  config_->setItemValue("global", "toolbarstate", strTmp);

  config_->save(getPath(USER_CONFIG) + "fqterm.cfg");
}

//addressbook
void FQTermFrame::addressBook() {
  siteDialog siteManager(this, false);
  if (siteManager.exec() == 1) {
    newWindow(siteManager.currentParameter(), siteManager.currentSiteIndex());
  }
}

//quicklogin
void FQTermFrame::quickLogin() {
  quickDialog quick(config_, this);
  FQTermConfig *pConf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");

  loadAddress(pConf, -1, quick.param_);
  delete pConf;

  if (quick.exec() == 1) {
    newWindow(quick.param_);
  }
}

void FQTermFrame::exitFQTerm() {
  
  if (!clearUp()) return;
  
  emit frameClosed();
}

void FQTermFrame::newWindow(const FQTermParam &param, int index) {
  QIcon *icon = new QIcon(QPixmap(getPath(RESOURCE) + "pic/tabpad.png"));


  FQTermWindow* window = windowManager_->newWindow(param, config_, icon, index);




  window->connectHost();
}


void FQTermFrame::aboutFQTerm() {
  aboutDialog about(this);
  about.exec();
}

//slot Help->Homepage
void FQTermFrame::homepage() {

  const QString &httpBrowser = FQTermPref::getInstance()->httpBrowser_;
  const QString homeUrl = "http://code.google.com/p/fqterm";

  if (httpBrowser.isNull() || httpBrowser.isEmpty()) {
	QDesktopServices::openUrl(homeUrl);
  } else {
	  runProgram(httpBrowser, homeUrl);
  }
}

//slot Windows menu aboutToShow
void FQTermFrame::windowsMenuAboutToShow() {
  menuWindows_->clear();
  QAction *cascadeAction = menuWindows_->addAction(tr("Cascade"), windowManager_, SLOT
                                                   (cascade()));
  QAction *tileAction = menuWindows_->addAction(tr("Tile"), windowManager_, SLOT(tile()));
  if (windowManager_->count() == 0) {
    cascadeAction->setEnabled(false);
    tileAction->setEnabled(false);
  }
  menuWindows_->addSeparator();

#ifdef Q_OS_MACX
  // used to dock the programe
  if (isHidden()) {
    menuWindows_->addAction(tr("Main Window"), this, SLOT(trayShow()));
  }
#endif

  for (int i = 0; i < int(windowManager_->count()); ++i) {
    QAction *idAction = menuWindows_->addAction(windowManager_->subWindowList().at(i)->windowTitle());
    idAction->setCheckable(true);
    idAction->setChecked(windowManager_->activeWindow() == windowManager_->nthWindow(i));
    connect(idAction, SIGNAL(triggered()), windowMapper_, SLOT(map()));
    windowMapper_->setMapping(idAction, i);
  }
}

void FQTermFrame::reloadConfig() {
  config_->load(getPath(USER_CONFIG) + "fqterm.cfg");
}

void FQTermFrame::popupConnectMenu() {
  menuConnect_->clear();

  menuConnect_->addAction(tr("Quick Login"), this, SLOT(quickLogin()));
  menuConnect_->addSeparator();

  FQTermConfig conf(getPath(USER_CONFIG) + "address.cfg");

  QStringList listName;
  loadNameList(&conf, listName);

  for (int i = 0; i < listName.count(); i++) {
    QAction *idAction = menuConnect_->addAction(listName[i], this, SLOT
                                                (connectMenuActivated()));
    idAction->setData(i);
    if (i < 10) {
#ifdef __APPLE__
      QString shortCut(tr("Ctrl+Meta"));
#else
      QString shortCut(tr("Ctrl+Alt"));
#endif
      shortCut += "+" + QString("").setNum((i + 1)% 10);
      idAction->setShortcut(shortCut);
    }
  }

}

void FQTermFrame::connectMenuAboutToHide() {
  // 	QMouseEvent me( QEvent::MouseButtonRelease, QPoint(0,0), QPoint(0,0),
  // 			Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  // 	QApplication::sendEvent( connectButton, &me );

}

void FQTermFrame::connectMenuActivated() {
  FQTermConfig *pConf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");
  int id = static_cast < QAction * > (sender())->data().toInt();
  FQTermParam param;

  // FIXME: don't know the relation with id and param setted by setItemParameter
  if (loadAddress(pConf, id, param)) {
    newWindow(param, id);
  }
  delete pConf;
}


bool FQTermFrame::eventFilter(QObject *o, QEvent *e) {
  return false;
}

//slot draw something e.g. logo in the background
//TODO : draw a pixmap in the background
void FQTermFrame::paintEvent(QPaintEvent*){
}

void FQTermFrame::closeEvent(QCloseEvent *clse) {
  if (FQTermPref::getInstance()->openMinimizeToTray_ &&
	windowManager_->count() > 0) {
    trayHide();
    clse->ignore();
    return ;
  }
  if (!clearUp())
  {
    clse->ignore();
    return;
  }

  clse->accept();

  emit frameClosed();
}

void FQTermFrame::langEnglish() {
  installTranslator("en_US");
}

void FQTermFrame::connectIt() {
  if (windowManager_->activeWindow() == NULL) {
    FQTermParam param;
    FQTermConfig *pConf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");

    loadAddress(pConf, -1, param);
    delete pConf;
    newWindow(param);
  } else if (!windowManager_->activeWindow()->isConnected()) {
    windowManager_->activeWindow()->getSession()->reconnect();
  }
}

void FQTermFrame::disconnect() {
  FQTermWindow* aw = windowManager_->activeWindow();
  if (aw) {
    aw->disconnect();
  }
}

void FQTermFrame::copy() {
  windowManager_->activeWindow()->copy();
}

void FQTermFrame::paste() {
  windowManager_->activeWindow()->paste();
}

void FQTermFrame::copyRect() {
  windowManager_->activeWindow()->getSession()->param_.isRectSelect_
      = !windowManager_->activeWindow()->getSession()->param_.isRectSelect_;

  actionRectangleSelect_->setChecked(
      windowManager_->activeWindow()->getSession()->param_.isRectSelect_);
}

void FQTermFrame::copyColor() {
  windowManager_->activeWindow()->getSession()->param_.isColorCopy_
      = !windowManager_->activeWindow()->getSession()->param_.isColorCopy_;

  actionColorCopy_->setChecked(
      windowManager_->activeWindow()->getSession()->param_.isColorCopy_);
}

void FQTermFrame::copyArticle() {
  windowManager_->activeWindow()->copyArticle();
}

void FQTermFrame::autoCopy() {
  windowManager_->activeWindow()->getSession()->param_.isAutoCopy_
      = !windowManager_->activeWindow()->getSession()->param_.isAutoCopy_;

  actionAutoCopy_->setChecked(
      windowManager_->activeWindow()->getSession()->param_.isAutoCopy_);
}

void FQTermFrame::wordWrap() {
  windowManager_->activeWindow()->getSession()->param_.isAutoWrap_
      = !windowManager_->activeWindow()->getSession()->param_.isAutoWrap_;

  actionWordWrap_->setChecked(
      windowManager_->activeWindow()->getSession()->param_.isAutoWrap_);
}

void FQTermFrame::noEsc() {
  FQTermPref::getInstance()->escapeString_ = "";
  actionNoEscape_->setChecked(true);
}

void FQTermFrame::escEsc() {
  FQTermPref::getInstance()->escapeString_ = "^[^[[";
  actionEscEscape_->setChecked(true);
}

void FQTermFrame::uEsc() {
  FQTermPref::getInstance()->escapeString_ = "^u[";
  actionUEscape_->setChecked(true);
}

void FQTermFrame::customEsc() {
  bool ok;
  QString strEsc = QInputDialog::getText(this, "define escape",
                                         "scape string *[", QLineEdit::Normal,
                                         FQTermPref::getInstance()->escapeString_, &ok);
  if (ok) {
    FQTermPref::getInstance()->escapeString_ = strEsc;
    actionCustomEscape_->setChecked(true);
  }
}

void FQTermFrame::gbkCodec() {
  FQTermPref::getInstance()->clipboardEncodingID_ = 0;
  actionGBK_->setChecked(true);
}

void FQTermFrame::big5Codec() {
  FQTermPref::getInstance()->clipboardEncodingID_ = 1;
  actionBIG5_->setChecked(true);
}


void FQTermFrame::setColor() {
  windowManager_->activeWindow()->setColor();

}

void FQTermFrame::refreshScreen() {
  windowManager_->activeWindow()->forcedRepaintScreen();
}

void FQTermFrame::uiFont() {
  bool ok;
  QFont font = QFontDialog::getFont(&ok, qApp->font());

  if (FQTermPref::getInstance()->openAntiAlias_) {
    font.setStyleStrategy(QFont::PreferAntialias);
  }

  if (ok == true) {
   qApp->setFont(font);
   //refresh style sheet
   if (FQTermPref::getInstance()->useStyleSheet_) {
     refreshStyleSheet();
   }
   image_->adjustItemSize();
  }
}

void FQTermFrame::fullscreen() {
  isFullScreen_ = !isFullScreen_;

  if (isFullScreen_) {
    //    menuBar()->hide();
    toolBarMdiTools_->hide();
    toolBarMdiConnectTools_->hide();
    toolBarSetupKeys_->hide();
    hideScroll();
    showStatusBar();
    showSwitchBar();
    showFullScreen();
  } else {
    //    menuBar()->show();
    toolBarMdiTools_->show();
    toolBarMdiConnectTools_->show();
    toolBarSetupKeys_->show();
    emit updateScroll();
    showStatusBar();
    showSwitchBar();
    showNormal();
  }

}

void FQTermFrame::bosscolor() {
  FQTermPref::getInstance()->isBossColor_ = !FQTermPref::getInstance()->isBossColor_;

  emit bossColor();

  actionBossColor_->setChecked(FQTermPref::getInstance()->isBossColor_);

}

void FQTermFrame::themesMenuAboutToShow() {
  menuThemes_->clear();
  QStringList styles = QStyleFactory::keys();
  for (QStringList::ConstIterator it = styles.begin();
       it != styles.end(); it++) {
    insertThemeItem(*it);
  }
}

void FQTermFrame::themesMenuActivated() {
  theme_ = ((QAction*)QObject::sender())->text();
  QStyle *style = QStyleFactory::create(theme_);
  if (style) {
    qApp->setStyle(style);
  }
}

void FQTermFrame::hideScroll() {
  FQTermPref::getInstance()->termScrollBarPosition_ = 0;

  emit updateScroll();
}

void FQTermFrame::leftScroll() {
  FQTermPref::getInstance()->termScrollBarPosition_ = 1;

  emit updateScroll();
}

void FQTermFrame::rightScroll() {
  FQTermPref::getInstance()->termScrollBarPosition_ = 2;

  emit updateScroll();
}

void FQTermFrame::showSwitchBar() {
  isTabBarShown_ = !isTabBarShown_;
  actionSwitch_->setChecked(isTabBarShown_);

  if (isTabBarShown_) {
    statusBar()->show();
  } else {
    statusBar()->hide();
  }
}

void FQTermFrame::showStatusBar() {
  FQTermPref::getInstance()->isStatusBarShown_ = !FQTermPref::getInstance()->isStatusBarShown_;
  actionStatus_->setChecked(FQTermPref::getInstance()->isStatusBarShown_);
  emit updateStatusBar(FQTermPref::getInstance()->isStatusBarShown_);
}

void FQTermFrame::setting() {
  windowManager_->activeWindow()->setting();
  updateMenuToolBar();
}

void FQTermFrame::defaultSetting() {
  FQTermConfig *pConf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");
  FQTermParam tmpParam;
 
  if (pConf->hasSection("default")) {
    loadAddress(pConf, -1, tmpParam);
  }

  addrDialog set(this, tmpParam);

  if (set.exec() == 1) {
    saveAddress(pConf, -1, set.param());
    pConf->save(getPath(USER_CONFIG) + "address.cfg");
  }

  delete pConf;
}

void FQTermFrame::preference() {
  prefDialog pref(config_, this);
  bool styleSheetUsed = FQTermPref::getInstance()->useStyleSheet_;

  if (pref.exec() == 1) {
    //TODO: refactor
    loadPref();
    setUseDock(FQTermPref::getInstance()->openMinimizeToTray_);
    if (FQTermPref::getInstance()->useStyleSheet_) {
      loadStyleSheetFromFile(FQTermPref::getInstance()->styleSheetFile_);
    }
    else if (styleSheetUsed) {
      clearStyleSheet();
    }
  }
  emit fontAntiAliasing(FQTermPref::getInstance()->openAntiAlias_);
}

void FQTermFrame::keySetup() {
  keyDialog keyDlg(config_, this);
  if (keyDlg.exec() == 1) {
    updateKeyToolBar();
  }
}


void FQTermFrame::antiIdle() {
  windowManager_->activeWindow()->toggleAntiIdle();
  actionAntiIdle_->setChecked(
      windowManager_->activeWindow()->getSession()->isAntiIdle());
}

void FQTermFrame::autoReply() {
  windowManager_->activeWindow()->toggleAutoReply();
  actionAutoReply_->setChecked(
      windowManager_->activeWindow()->getSession()->isAutoReply());
}

void FQTermFrame::viewMessages() {
  windowManager_->activeWindow()->viewMessages();
}

void FQTermFrame::enableMouse() {
  windowManager_->activeWindow()->getSession()->param_.isSupportMouse_
      = !windowManager_->activeWindow()->getSession()->param_.isSupportMouse_;
  actionMouse_->setChecked(
      windowManager_->activeWindow()->getSession()->param_.isSupportMouse_);
}

void FQTermFrame::viewImages(QString filename, bool raiseViewer) {

  if (filename.isEmpty()) {
    filename = FQTermPref::getInstance()->poolDir_;
  }

  

  if (raiseViewer) {
    image_->scrollTo(filename);
    if (image_->isHidden()) {
      image_->resize(size() * 3 / 4 + QSize(1,1));
      image_->show();
    }
    clearFocus();
    image_->raise();
    image_->activateWindow();
  } else {
    image_->updateImage(filename);
  }
}

void FQTermFrame::viewImages() {
  viewImages(FQTermPref::getInstance()->poolDir_, true);
}

void FQTermFrame::beep() {
  windowManager_->activeWindow()->getSession()->param_.isBeep_ =
      !windowManager_->activeWindow()->getSession()->param_.isBeep_;
  actionBeep_->setChecked(windowManager_->activeWindow()->getSession()->param_.isBeep_);
}

void FQTermFrame::reconnect() {
  FQTermWindow * qtw = windowManager_->activeWindow();
  if (qtw){
    qtw->getSession()->param_.isAutoReconnect_ =
        !windowManager_->activeWindow()->getSession()->param_.isAutoReconnect_;
  }
}

void FQTermFrame::runScript() {
  windowManager_->activeWindow()->runScript();
}

void FQTermFrame::stopScript() {
  windowManager_->activeWindow()->stopScript();
}

bool FQTermFrame::event(QEvent *e) {

  static bool shown = false;
  if (e->type() == QEvent::WindowStateChange 
    && (((QWindowStateChangeEvent*)(e))->oldState() & Qt::WindowMinimized)
    && !(windowState() & Qt::WindowMinimized)) {
    shown = true;
  }
  if (e->type() == QEvent::Paint && shown) {
    shown = false;
    qApp->setActiveWindow(NULL);
    qApp->setActiveWindow(this);
  }

  bool res = this->QMainWindow::event(e);

  if (e->type() == QEvent::HoverMove
      || e->type() == QEvent::MouseMove
      || e->type() == QEvent::Move) {
    if (res) {
      FQ_TRACE("frameEvent", 10) << "Accept event: " << e->type()
                                 << " " << getEventName(e->type()) << ".";
    } else {
      FQ_TRACE("frameEvent", 10) << "Ignore event: " << e->type()
                                 << " " << getEventName(e->type()) << ".";
    }
  } else {
    if (res) {
      FQ_TRACE("frameEvent", 9) << "Accept event: " << e->type()
                                << " " << getEventName(e->type()) << ".";
    } else {
      FQ_TRACE("frameEvent", 9) << "Ignore event: " << e->type()
                                << " " << getEventName(e->type()) << ".";
    }
  }

  return res;
}

void FQTermFrame::keyClicked(int id) {
  if (windowManager_->activeWindow() == NULL) {
    return ;
  }

  QString strItem = QString("key%1").arg(id);
  QString strTmp = config_->getItemValue("key", strItem);

  if (strTmp[0] == '0') { // key
    windowManager_->activeWindow()->externInput(strTmp.mid(1));
  } else if (strTmp[0] == '1') { // script
    windowManager_->activeWindow()->runScript(strTmp.mid(1).toAscii());
  } else if (strTmp[0] == '2') { // program
    runProgram(strTmp.mid(1));
  }
}

void FQTermFrame::addMainTool() {
  // the main toolbar
  toolBarMdiTools_ = addToolBar("Main ToolBar");
  toolBarMdiTools_->setObjectName("Main ToolBar");
  toolBarMdiTools_->setIconSize(QSize(22, 22));

  connectButton_ = new QToolButton(toolBarMdiTools_);
  connectButton_->setIcon(QPixmap(getPath(RESOURCE) + "pic/connect.png"));

  toolBarMdiTools_->addWidget(connectButton_);
  menuConnect_ = new QMenu(this);
  //FIXME: autoupdate menu
  FQ_VERIFY(connect(menuConnect_, SIGNAL(aboutToShow()),
                    this, SLOT(popupConnectMenu())));
  connectButton_->setMenu(menuConnect_);
  connectButton_->setPopupMode(QToolButton::InstantPopup);

  toolBarMdiTools_->addAction(actionQuickConnect_);

  actionQuickConnect_->setIcon(QPixmap(getPath(RESOURCE) + "pic/quick_login.png"));
  // custom define
  toolBarSetupKeys_ = addToolBar("Custom Key");
  toolBarSetupKeys_->setObjectName("Custom Key");

  // the toolbar
  toolBarMdiConnectTools_ = addToolBar("bbs operations");
  toolBarMdiConnectTools_->setObjectName("bbs operations");
  actionDisconnect_ = toolBarMdiConnectTools_->addAction(
      QPixmap(getPath(RESOURCE) + "pic/disconnect.png"), tr("Disconnect"),
      this, SLOT(disconnect()));
  actionDisconnect_->setEnabled(false);
  toolBarMdiConnectTools_->addSeparator();

  // Edit (5)
  toolBarMdiConnectTools_->addAction(actionCopy_);
  actionCopy_->setIcon(QPixmap(getPath(RESOURCE) + "pic/copy.png"));
  toolBarMdiConnectTools_->addAction(actionPaste_);
  actionPaste_->setIcon(QPixmap(getPath(RESOURCE) + "pic/paste.png"));
  toolBarMdiConnectTools_->addAction(actionColorCopy_);
  actionColorCopy_->setIcon(QPixmap(getPath(RESOURCE) + "pic/copy_with_color.png"));
  actionColorCopy_->setCheckable(true);
  toolBarMdiConnectTools_->addAction(actionRectangleSelect_);
  actionRectangleSelect_->setIcon(QPixmap(getPath(RESOURCE) + "pic/rectangle_selection.png"));
  actionRectangleSelect_->setCheckable(true);
  toolBarMdiConnectTools_->addSeparator();

  //View (3)
  fontButton_ = new QToolButton(toolBarMdiConnectTools_);
  QAction* dummyAction = new QAction(QPixmap(getPath(RESOURCE) + "pic/change_fonts.png"),
    tr("Set Terminal Fonts"), fontButton_);
  fontButton_->setDefaultAction(dummyAction);
  toolBarMdiConnectTools_->addWidget(fontButton_);
  fontButton_->setMenu(menuFont_);
  fontButton_->setPopupMode(QToolButton::InstantPopup);

  toolBarMdiConnectTools_->addAction(actionColor_);
  actionColor_->setIcon(QPixmap(getPath(RESOURCE) + "pic/ansi_color.png"));
  toolBarMdiConnectTools_->addAction(actionToggleAnsiColor_);
  actionToggleAnsiColor_->setIcon(QPixmap(getPath(RESOURCE) + "pic/toggle_ansi_color.png"));
  actionToggleAnsiColor_->setCheckable(true);
  toolBarMdiConnectTools_->addAction(actionRefresh_);
  actionRefresh_->setIcon(QPixmap(getPath(RESOURCE) + "pic/refresh.png"));
  toolBarMdiConnectTools_->addSeparator();

  // Option
  toolBarMdiConnectTools_->addAction(actionCurrentSession_);
  actionCurrentSession_->setIcon(QPixmap(getPath(RESOURCE) + "pic/preferences.png"));
  toolBarMdiConnectTools_->addSeparator();

  // Spec (5)
  toolBarMdiConnectTools_->addAction(actionCopyArticle_);
  actionCopyArticle_->setIcon(QPixmap(getPath(RESOURCE)
                                      + "pic/get_article_fulltext.png"));
  toolBarMdiConnectTools_->addAction(actionAntiIdle_);
  actionAntiIdle_->setIcon(QPixmap(getPath(RESOURCE) + "pic/anti_idle.png"));
  actionAntiIdle_->setCheckable(true);
  toolBarMdiConnectTools_->addAction(actionAutoReply_);
  actionAutoReply_->setIcon(QPixmap(getPath(RESOURCE) + "pic/auto_reply.png"));
  actionAutoReply_->setCheckable(true);
  toolBarMdiConnectTools_->addAction(actionViewMessage_);
  actionViewMessage_->setIcon(QPixmap(getPath(RESOURCE) + "pic/view_messages.png"));
  toolBarMdiConnectTools_->addAction(actionViewImage_);
  actionViewImage_->setIcon(QPixmap(getPath(RESOURCE) + "pic/image_viewer.png"));
  toolBarMdiConnectTools_->addAction(actionMouse_);
  actionMouse_->setIcon(QPixmap(getPath(RESOURCE) + "pic/mouse.png"));
  actionMouse_->setCheckable(true);
  toolBarMdiConnectTools_->addAction(actionBeep_);
  actionBeep_->setIcon(QPixmap(getPath(RESOURCE) + "pic/beep.png"));
  actionBeep_->setCheckable(true);
  actionReconnect_ = toolBarMdiConnectTools_->addAction(
      QPixmap(getPath(RESOURCE) + "pic/auto_reconnect.png"),
      tr("Reconnect When Disconnected By Host"), 
      this, SLOT(reconnect()));
  actionReconnect_->setCheckable(true);
 

  //call popupConnectMenu() to enable the shortcuts
  popupConnectMenu(); 

}


void FQTermFrame::addMainMenu() {
  menuMain_ = menuBar(); //new QMenuBar(this);

  menuFile_ = menuMain_->addMenu(tr("&File"));

  menuFile_->addAction(QPixmap(getPath(RESOURCE) + "pic/connect.png"),
                       tr("&Connect"), this,
                  SLOT(connectIt()));

  menuFile_->addAction(QPixmap(getPath(RESOURCE) + "pic/disconnect.png"),
                       tr("&Disconnect"),
                  this, SLOT(disconnect())); //, 0, ID_FILE_DISCONNECT );

  menuFile_->addSeparator();
  menuFile_->addAction(QPixmap(getPath(RESOURCE) + "pic/address_book.png"),
                       tr("&Address book"),
                       this, SLOT(addressBook()), tr("F2"));
  actionQuickConnect_ =
      menuFile_->addAction(QPixmap(getPath(RESOURCE) + "pic/quick_login.png"),
                           tr("&Quick login"),
                           this, SLOT(quickLogin()), tr("F3"));
  menuFile_->addSeparator();
  menuFile_->addAction(tr("&Exit"), this, SLOT(exitFQTerm()));

  //Edit Menu
  QMenu *edit = menuMain_->addMenu(tr("&Edit"));

  // 	edit->setCheckable( true );
#if defined(__APPLE__)
  actionCopy_ = edit->addAction(QPixmap(getPath(RESOURCE) + "pic/copy.png"),
                                tr("&Copy"), this, SLOT(copy()), tr("Ctrl+C"));
  actionPaste_ = edit->addAction(QPixmap(getPath(RESOURCE) + "pic/paste.png"),
                                 tr("&Paste"),
                                 this, SLOT(paste()), tr("Ctrl+V"));
#else
  actionCopy_ = edit->addAction(QPixmap(getPath(RESOURCE) + "pic/copy.png"),
                                tr("&Copy"),
                                this, SLOT(copy()), tr("Ctrl+Insert"));
  actionPaste_ = edit->addAction(QPixmap(getPath(RESOURCE) + "pic/paste.png"),
                                 tr("&Paste"), this, SLOT(paste()),
                                 tr("Shift+Insert"));
#endif
  edit->addSeparator();
  actionColorCopy_ = edit->addAction(
      QPixmap(getPath(RESOURCE) + "pic/copy_with_color.png"),
      tr("C&opy with color"), this, SLOT(copyColor()));
  actionColorCopy_->setCheckable(true);
  actionRectangleSelect_ = edit->addAction(
      QPixmap(getPath(RESOURCE) + "pic/rectangle_selection.png"),
      tr("&Rectangle select"), this, SLOT(copyRect()));
  actionRectangleSelect_->setCheckable(true);
  actionAutoCopy_ = edit->addAction(tr("Auto copy &select"), this, SLOT
                                    (autoCopy()));
  actionAutoCopy_->setCheckable(true);
  actionWordWrap_ = edit->addAction(tr("P&aste with wordwrap"), this, SLOT
                                    (wordWrap()));
  actionWordWrap_->setCheckable(true);

  QMenu *escapeMenu = edit->addMenu(tr("Paste &with color"));
  escapeGroup = new QActionGroup(this);
  actionNoEscape_ = escapeMenu->addAction(tr("&None"), this, SLOT(noEsc()));
  actionNoEscape_->setCheckable(true);
  actionEscEscape_ = escapeMenu->addAction(tr("&ESC ESC ["),
                                           this, SLOT(escEsc()));
  actionEscEscape_->setCheckable(true);
  actionUEscape_ = escapeMenu->addAction(tr("Ctrl+&U ["), this, SLOT(uEsc()));
  actionUEscape_->setCheckable(true);
  actionCustomEscape_ = escapeMenu->addAction(tr("&Custom..."), this, SLOT
                                              (customEsc()));
  actionCustomEscape_->setCheckable(true);
  escapeGroup->addAction(actionNoEscape_);
  escapeGroup->addAction(actionEscEscape_);
  escapeGroup->addAction(actionUEscape_);
  escapeGroup->addAction(actionCustomEscape_);

  QMenu *codecMenu = edit->addMenu(tr("Clipboard &encoding"));
  codecGroup = new QActionGroup(this);
  actionGBK_ = codecMenu->addAction(tr("&GBK"), this, SLOT(gbkCodec()));
  actionGBK_->setCheckable(true);
  actionBIG5_ = codecMenu->addAction(tr("&Big5"), this, SLOT(big5Codec()));
  actionBIG5_->setCheckable(true);
  codecGroup->addAction(actionGBK_);
  codecGroup->addAction(actionBIG5_);

  //View menu
  QMenu *view = menuMain_->addMenu(tr("&View"));

  menuFont_ = view->addMenu(tr("&Font"));
  menuFont_->setIcon(QPixmap(getPath(RESOURCE) + "pic/change_fonts.png"));
  view->addMenu(menuFont_);
  for (int i = 0; i < 2; ++i) {
    QAction *act = menuFont_->addAction(
        FQTermParam::getLanguageName(bool(i)) + tr(" Font"),
        this, SLOT(setFont()));
    act->setData(i);
  }

  actionColor_ = view->addAction(QPixmap(getPath(RESOURCE) + "pic/ansi_color.png"),
                                 tr("&Color Setting"), this, SLOT(setColor()));
  actionToggleAnsiColor_ = view->addAction(QPixmap(getPath(RESOURCE) + "pic/toggle_ansi_color.png"),
    tr("&Use ANSI Color"), this, SLOT(toggleAnsiColor()));
  actionRefresh_ = view->addAction(
      QPixmap(getPath(RESOURCE) + "pic/refresh.png"),
      tr("&Refresh"), this, SLOT(refreshScreen()), tr("F5"));
  view->addSeparator();

  //language menu
  languageGroup = new QActionGroup(this);
  languageGroup->setExclusive(true);
  if (installerList_.isEmpty()) {
    FQ_TRACE("frame", 0)
        << "No language menu because of lack of translation files";
  } else {
    menuLanguage_ = view->addMenu(tr("&Language"));
    actionEnglish_ = menuLanguage_->addAction(tr("&English"),
                                              this, SLOT(langEnglish()));
    actionEnglish_->setCheckable(true);
    languageGroup->addAction(actionEnglish_);

    foreach(TranslatorInstaller* installer, installerList_) {
      QAction* action = menuLanguage_->addAction(
          installer->languageName(), installer, SLOT(installTranslator()));
      action->setCheckable(true);
      languageGroup->addAction(action);
    }

  } 

  view->addAction(tr("&UI font"), this, SLOT(uiFont()));

  menuThemes_ = view->addMenu(tr("&Themes"));
  FQ_VERIFY(connect(menuThemes_, SIGNAL(aboutToShow()),
                    this, SLOT(themesMenuAboutToShow())));
  actionFullScreen_ = view->addAction(
      tr("&Fullscreen"), this, SLOT(fullscreen()), tr("F6"));
  actionFullScreen_->setCheckable(true);

  actionBossColor_ = view->addAction(
      tr("Boss &Color"), this, SLOT(bosscolor()), tr("F12"));
  actionBossColor_->setCheckable(true);

  view->addSeparator();

  scrollMenu_ = view->addMenu(tr("&ScrollBar"));
  FQ_VERIFY(connect(scrollMenu_, SIGNAL(aboutToShow()),
                    this, SLOT(scrollMenuAboutToShow())));

  actionStatus_ = view->addAction(
      tr("Status &Bar"), this, SLOT(showStatusBar()));
  actionStatus_->setCheckable(true);
  actionSwitch_ = view->addAction(
      tr("S&witch Bar"), this, SLOT(showSwitchBar()));
  actionSwitch_->setCheckable(true);

  // Option Menu
  QMenu *option = menuMain_->addMenu(tr("&Option"));

  actionCurrentSession_ = option->addAction(
      QPixmap(getPath(RESOURCE) + "pic/preferences.png"),
      tr("&Setting for current session"),
      this, SLOT(setting()));
  option->addSeparator();
  option->addAction(tr("&Default setting"), this, SLOT(defaultSetting()));
  option->addAction(tr("&Preference"), this, SLOT(preference()));

  // Special
  QMenu *spec = menuMain_->addMenu(tr("&Special"));
  actionCopyArticle_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/get_article_fulltext.png"), tr("&Copy article"),
      this, SLOT(copyArticle()), tr("F9"));
  actionAntiIdle_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/anti_idle.png"), tr("Anti &idle"),
      this, SLOT(antiIdle()));
  actionAutoReply_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/auto_reply.png"), tr("Auto &reply"),
      this, SLOT(autoReply()));
  actionViewMessage_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/view_messages.png"), tr("&View messages"),
      this, SLOT(viewMessages()), tr("F10"));
  actionBeep_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/beep.png"), tr("&Beep "),
      this, SLOT(beep()));
  actionMouse_ = spec->addAction(
      QPixmap(getPath(RESOURCE) + "pic/mouse.png"), tr("&Mouse support"),
      this, SLOT(enableMouse()));
  actionViewImage_ = spec->addAction(
    QPixmap(getPath(RESOURCE) + "pic/image_viewer.png"), tr("&Image viewer"),
    this, SLOT(viewImages()));

  //Script
  QMenu *script = menuMain_->addMenu(tr("Scrip&t"));
  actionRunScript_ = script->addAction(tr("&Run..."), this, SLOT(runScript()),
                                       tr("F7"));
  actionStopScript_ = script->addAction(tr("&Stop"), this, SLOT(stopScript()),
                                        tr("F8"));

  //Window menu
  menuWindows_ = menuMain_->addMenu(tr("&Windows"));
  FQ_VERIFY(connect(menuWindows_, SIGNAL(aboutToShow()),
                    this, SLOT(windowsMenuAboutToShow())));

  menuMain_->addSeparator();

  //Help menu
  QMenu *help = menuMain_->addMenu(tr("&Help"));
  help->addAction(tr("About &FQTerm"), this, SLOT(aboutFQTerm()), tr("F1"));

  help->addAction(tr("FQTerm's &Homepage"), this, SLOT(homepage()));
}

void FQTermFrame::updateMenuToolBar() {
  FQTermWindow *window = windowManager_->activeWindow();

  if (window == NULL) {
    return ;
  }

  // update menu
  actionDisconnect_->setEnabled(window->isConnected());

  actionColorCopy_->setChecked(window->getSession()->param_.isColorCopy_);
  actionRectangleSelect_->setChecked(window->getSession()->param_.isRectSelect_);
  actionAutoCopy_->setChecked(window->getSession()->param_.isAutoCopy_);
  actionWordWrap_->setChecked(window->getSession()->param_.isAutoWrap_);

  actionFullScreen_->setChecked(isFullScreen_);

  actionToggleAnsiColor_->setChecked(window->getSession()->param_.isAnsiColor_);

  actionAntiIdle_->setChecked(window->getSession()->isAntiIdle());
  actionAutoReply_->setChecked(window->getSession()->isAutoReply());
  actionBeep_->setChecked(window->getSession()->param_.isBeep_);
  actionMouse_->setChecked(window->getSession()->param_.isSupportMouse_);
}

void FQTermFrame::enableMenuToolBar(bool enable) {
  actionDisconnect_->setEnabled(enable);

  actionCopy_->setEnabled(enable);
  actionPaste_->setEnabled(enable);
  actionColorCopy_->setEnabled(enable);
  actionRectangleSelect_->setEnabled(enable);
  actionAutoCopy_->setEnabled(enable);
  actionWordWrap_->setEnabled(enable);

  actionToggleAnsiColor_->setEnabled(enable);

  fontButton_->setEnabled(enable);
  menuFont_->setEnabled(enable);
  actionColor_->setEnabled(enable);
  actionRefresh_->setEnabled(enable);

  actionCurrentSession_->setEnabled(enable);

  actionCopyArticle_->setEnabled(enable);
  actionAntiIdle_->setEnabled(enable);
  actionAutoReply_->setEnabled(enable);
  actionViewMessage_->setEnabled(enable);
  actionBeep_->setEnabled(enable);
  actionMouse_->setEnabled(enable);

  actionRunScript_->setEnabled(enable);
  actionStopScript_->setEnabled(enable);
}

void FQTermFrame::updateKeyToolBar() {
  toolBarSetupKeys_->clear();
  toolBarSetupKeys_->addAction(QPixmap(getPath(RESOURCE) + "pic/setup_shortcuts.png"),
                               tr("Key Setup"), this, SLOT(keySetup()));

  QString strItem, strTmp;
  strTmp = config_->getItemValue("key", "num");
  int num = strTmp.toInt();

  for (int i = 0; i < num; i++) {
    strItem = QString("name%1").arg(i);
    strTmp = config_->getItemValue("key", strItem);
    FQTermToolButton *button =
        new FQTermToolButton(toolBarSetupKeys_, i, strTmp);

    // 		button->setUsesTextLabel(true);
    button->setText(strTmp);
    // 		button->setTextPosition(QToolButton::BesideIcon);
    strItem = QString("key%1").arg(i);
    strTmp = (config_->getItemValue("key", strItem));
    //TODO: add tool tip here
    // 		QToolTip::add( button, strTmp.mid(1) );
    // 		button->addToolTip(strTmp.mid(1));

    strTmp = config_->getItemValue("key", QString("shortcut%1").arg(i));
    if (!strTmp.isEmpty()) {
      QAction *act = new QAction(button);
      act->setShortcut(strTmp);
      button->addAction(act);
      FQ_VERIFY(connect(act, SIGNAL(triggered()), button, SLOT(slotClicked())));
    }

    FQ_VERIFY(connect(button, SIGNAL(buttonClicked(int)),
                      this, SLOT(keyClicked(int))));
    toolBarSetupKeys_->addWidget(button);
  }



}

QString FQTermFrame::valueToString(bool shown, int dock, int index,
                                   bool nl, int extra) {
  QString str = "";

  str = QString("%1 %2 %3 %4 %5")
        .arg(shown ? 1 : 0)
        .arg(dock)
        .arg(index)
        .arg(nl? 1: 0)
        .arg(extra);

  return str;
}

void FQTermFrame::popupFocusIn(FQTermWindow*) {
  // bring to font
  if (isHidden()) {
    show();
  }
  if (isMinimized()) {
    if (isMaximized()) {
      showMaximized();
    } else {
      showNormal();
    }
  }
  raise();
  activateWindow();
}

void FQTermFrame::insertThemeItem(QString themeitem) {
  QAction *idAction;

  idAction = menuThemes_->addAction(themeitem,
                                    this, SLOT(themesMenuActivated()));
  idAction->setCheckable(true);
  idAction->setChecked(themeitem == theme_);
}

void FQTermFrame::setUseDock(bool use) {
  if (use == false) {
    if (tray) {
      tray->hide();
      delete tray;
      tray = 0;
      //delete menuTray_;
      //menuTray_ = 0;
    }
    return ;
  }

  if (tray || !QSystemTrayIcon::isSystemTrayAvailable()) {
    return ;
  }

  menuTray_ = new QMenu;
  FQ_VERIFY(connect(menuTray_, SIGNAL(aboutToShow()), SLOT(buildTrayMenu())));

  tray = new QSystemTrayIcon(this);
  tray->setIcon(QPixmap(getPath(RESOURCE) + "pic/fqterm_tray.png"));
  tray->setContextMenu(menuTray_);
  FQ_VERIFY(connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                    SLOT(trayActived(QSystemTrayIcon::ActivationReason))));
  tray->show();
}

void FQTermFrame::buildTrayMenu() {
  if (!menuTray_) {
    return ;
  }
  menuTray_->clear();

  if (isHidden()) {
    menuTray_->addAction(tr("Show"), this, SLOT(trayShow()));
  } else {
    menuTray_->addAction(tr("Hide"), this, SLOT(trayHide()));
  }
  menuTray_->addSeparator();
  menuTray_->addAction(tr("About"), this, SLOT(aboutFQTerm()));
  menuTray_->addAction(tr("Exit"), this, SLOT(exitFQTerm()));
}

void FQTermFrame::trayActived(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Context) {
    return ;
  }
  if (!isVisible()) {
    trayShow();
  } else {
    trayHide();
  }
}

void FQTermFrame::trayShow() {
  show();
  // bring to font
  if (isHidden()) {
    show();
  }
  if (isMinimized()) {
    if (isMaximized()) {
      showMaximized();
    } else {
#ifdef Q_OS_MACX
      showMaximized();
#else
      showNormal();
#endif
    }
  }
  raise();
  activateWindow();
  FQTermWindow* window = windowManager_->activeWindow();
  if (window) {
    window->forcedRepaintScreen();
    if (!window->isMaximized()) {
      windowManager_->refreshAllExcept(window);
    }
  }
}

void FQTermFrame::trayHide() {
  static bool showTip = true;
  hide();
  if (showTip) {
    tray->showMessage(tr("FQTerm"),
                      tr("FQTerm will keep running in the system tray.\n"
                         "To terminate the program, "
                         "choose exit in the tray menu."));
    showTip = false;
  }
}

void FQTermFrame::buzz(FQTermWindow* window) {
  if (windowManager_->activeWindow() == window)
     return;
  int xp = x();
  int yp = y();
  QTime t;

  t.start();
  for (int i = 32; i > 0;) {
    if (t.elapsed() >= 1) {
      int delta = i >> 2;
      int dir = i &3;
      int dx = ((dir == 1) || (dir == 2)) ? delta : -delta;
      int dy = (dir < 2) ? delta : -delta;
      move(xp + dx, yp + dy);
      t.restart();
      i--;
    }
  }
  move(xp, yp);
}


//--------------------------
//recreate the main menu
//--------------------------
void
FQTermFrame::recreateMenu() {
  menuBar()->clear();
  delete escapeGroup;
  delete codecGroup;
  delete languageGroup;
  addMainMenu();
  updateLanguageMenu();

  QString strTmp = QString(saveState().toHex());
  config_->setItemValue("global", "toolbarstate", strTmp);
  config_->save(getPath(USER_CONFIG) + "fqterm.cfg");

  delete toolBarMdiTools_;
  delete toolBarSetupKeys_;
  delete toolBarMdiConnectTools_;
  delete menuConnect_;
  addMainTool();
  updateKeyToolBar();
  loadToolBarPosition();
  if (!windowManager_->activeWindow()) {
    enableMenuToolBar(false);
  } else {
    updateMenuToolBar();
  }
  if (FQTermPref::getInstance()->useStyleSheet_) {
    refreshStyleSheet();
  }
}

void FQTermFrame::installTranslator(const QString& lang) {
  config_->setItemValue("global", "language", lang);
  config_->save(getPath(USER_CONFIG) + "fqterm.cfg");

  for (int i = 0; i < translatedModule; ++i) {
    if (translator[i] != 0) {
      qApp->removeTranslator(translator[i]);
      delete translator[i];
      translator[i] = 0;
    }
  }

  if (lang != "en_US" && !lang.isEmpty()) {
    for (int i = 0; i < translatedModule; ++i) {
      QString dict =
          getPath(RESOURCE) + "dict/" + qmPrefix[i] + lang + qmPostfix;
      translator[i] = new QTranslator(0);
      translator[i]->load(dict);
      qApp->installTranslator(translator[i]);
    }
  }

  emit changeLanguage();
}

void FQTermFrame::initTranslator() {
  QString dict_path = getPath(RESOURCE) + "dict/";
  QDir dictDir(dict_path);
  QStringList langList;

  for (int i = 0; i < translatedModule; ++i) {
    translator[i] = 0;
    QStringList filter;
    filter << qmPrefix[i] + "*" + qmPostfix;
    dictDir.setNameFilters(filter);
    QFileInfoList list = dictDir.entryInfoList();

    FQ_TRACE("translator", 3) << "Found " << list.size()
                              <<  "  " << qmPostfix
                              << " file(s) under path "
                              << dict_path;
    
    foreach(QFileInfo info, list){
      QString language = info.fileName();
      language = language.mid(
          qmPrefix[i].length(),
          language.length() - qmPrefix[i].length() - qmPostfix.length());

      FQ_TRACE("translator", 3) << "Check file " << info.fileName();

      if (!langList.contains(language)) {
        langList << language;
        installerList_.append(new TranslatorInstaller(language, this));
        FQ_TRACE("translator", 3) << "Found translations for " << language;
      } else {
        FQ_TRACE("translator", 3) << "Language " << language
                                  << " is already registered.";
      }
    }
  }

  if (installerList_.empty()) {
    FQ_TRACE("translator", 0) << "No translations found.";
  }
  
  QString lang = config_->getItemValue("global", "language");
  if (!langList.contains(lang)) {
    lang = QLocale::system().name();
  }
#if defined(__linux__)
  if (QLocale::system().language() == QLocale::English)
    lang = QLocale::system().name();
#endif
  installTranslator(lang);
}


void FQTermFrame::clearTranslator() {
  foreach(TranslatorInstaller* installer, installerList_){
    delete installer;
  }
}

void FQTermFrame::updateLanguageMenu() {
  QString strTmp = config_->getItemValue("global", "language");
  int i = 0;
  foreach(TranslatorInstaller* installer, installerList_){
    if (installer->languageFormalName() == strTmp) {
      languageGroup->actions().at(i + 1)->setChecked(true);
      break;
    }
    ++i;
  }
  if (!installerList_.isEmpty() &&
      (strTmp == "en_US" || i == installerList_.size())) {
    languageGroup->actions().at(0)->setChecked(true);
  }
}

void FQTermFrame::scrollMenuAboutToShow() {
  scrollMenu_->clear();
  actionHideScrollBar_ = scrollMenu_->addAction(
      tr("&Hide"), this, SLOT(hideScroll()));
  actionHideScrollBar_->setCheckable(true);
  actionLeftScrollBar_ = scrollMenu_->addAction(
      tr("&Left"), this, SLOT(leftScroll()));
  actionLeftScrollBar_->setCheckable(true);
  actionRightScrollBar_ = scrollMenu_->addAction(
      tr("&Right"), this, SLOT(rightScroll()));
  actionRightScrollBar_->setCheckable(true);
  switch(FQTermPref::getInstance()->termScrollBarPosition_) {
    case 0:
      actionHideScrollBar_->setChecked(true);
      break;
    case 1:
      actionLeftScrollBar_->setChecked(true);
      break;
    case 2:
      actionRightScrollBar_->setChecked(true);
      break;
  }
}

void FQTermFrame::setFont() {
  bool isEnglish =
      ((QAction*)(sender()))->data().toBool();

  windowManager_->activeWindow()->setFont(isEnglish);
}

void FQTermFrame::refreshStyleSheet()
{
  qApp->setStyleSheet(qApp->styleSheet());
}
void FQTermFrame::loadStyleSheetFromFile( const QString qssFile )
{
  QFile file(qssFile);
  file.open(QIODevice::ReadOnly);
  QString qss = file.readAll();
  qApp->setStyleSheet(qss);
  file.close();
}

void FQTermFrame::clearStyleSheet() {
  qApp->setStyleSheet("");
}

void FQTermFrame::loadToolBarPosition()
{
  //load toolbar setting
  QString strTmp = config_->getItemValue("global", "toolbarstate");
  if (!strTmp.isEmpty())
  {
    restoreState(QByteArray::fromHex(strTmp.toAscii()));
  } else {
    addToolBar(Qt::TopToolBarArea, toolBarMdiConnectTools_);
    insertToolBar(toolBarMdiConnectTools_,toolBarSetupKeys_);
    insertToolBar(toolBarSetupKeys_, toolBarMdiTools_);
  }
}

void FQTermFrame::toggleAnsiColor() {
  windowManager_->activeWindow()->getSession()->param_.isAnsiColor_
    = !windowManager_->activeWindow()->getSession()->param_.isAnsiColor_;
  actionToggleAnsiColor_->setChecked(
    windowManager_->activeWindow()->getSession()->param_.isAnsiColor_);
  refreshScreen();
}

bool FQTermFrame::clearUp() {
  if (!windowManager_->closeAllWindow())
      return false;

  saveSetting();
  // clear zmodem and pool if needed
  if (FQTermPref::getInstance()->needClearZmodemPoolOnClose_) {
    clearDir(FQTermPref::getInstance()->zmodemDir_);
    clearDir(FQTermPref::getInstance()->poolDir_);
    clearDir(FQTermPref::getInstance()->poolDir_ + "shadow-cache/");
  }

  setUseDock(false);

  return true;
}


TranslatorInstaller::TranslatorInstaller(const QString& language,
                                         FQTermFrame* frame)
    : language_(language),
      frame_(frame) {
  FQTermConfig conf(getPath(USER_CONFIG) + "language.cfg");

  languageName_ = conf.getItemValue("Name", language_);
  if (languageName_.isEmpty()){
    languageName_ = QLocale::languageToString(QLocale(language_).language());
    if (languageName_ == "C") {
      languageName_ = "Unknown Language";
    }
  }
}

QString TranslatorInstaller::languageName() {
  return languageName_;
}

void TranslatorInstaller::installTranslator() {
  frame_->installTranslator(language_);
}

QString TranslatorInstaller::languageFormalName() {
  return language_;
}

}  // namespace FQTerm

#include "fqterm_frame.moc"
