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
#include <QDate>
#include <QTimer>
#include <QToolBar>
#include <QTranslator>
#include <QWindowStateChangeEvent>

#ifdef HAVE_PYTHON
#include "fqterm_python.h"
#endif //HAVE_PYTHON

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
#include "fqterm_shortcuthelper.h"
#include "fqterm_mini_server.h"
#include "shortcutdialog.h"
#include "schemadialog.h"
#include "fqterm_ip_location.h"
#include "iplookup.h"
namespace FQTerm {


const QString FQTermFrame::qmPrefix[FQTermFrame::translatedModule] =
{"fqterm_", "ui_", "protocol_", "common_"};
const QString FQTermFrame::qmPostfix = ".qm";

//constructor
FQTermFrame::FQTermFrame()
    : QMainWindow(0), 
      tray(0),
      trayMenu_(0), 
#ifdef HAVE_PYTHON
      pythonHelper_(0),
#endif
      shortcutHelper_(0),
      ipLookupDialog_(0){
  setAttribute(Qt::WA_DeleteOnClose);

/*#ifndef __APPLE__
  setWindowFlags(Qt::CustomizeWindowHint);
#endif*/
  config_ = new FQTermConfig(getPath(USER_CONFIG) + "fqterm.cfg");

  initTranslator();
  shortcutHelper_ = new FQTermShortcutHelper(config_, this);

#ifdef IMAGE_USE_PICFLOW
  imageViewer_ = new FQTermImageFlow(config_, NULL, Qt::Window);
#else
  imageViewer_ = new FQTermImageOrigin(config_, NULL, Qt::Window);
#endif


  //create the window manager to deal with the window-tab-icon pairs
  windowManager_ = new FQTermWndMgr(this);
  setCentralWidget(windowManager_);

  //set menubar
  addMainMenu();
  FQ_VERIFY(connect(this, SIGNAL(changeLanguage()),
                    this, SLOT(recreateMenu())));

  //initialize all settings
  //This should be done before add main tool, since some status of tool will depend on setting
  iniSetting();
  FQTermConfig* conf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");
  checkHelpExists(conf);
  delete conf;
  //setup toolbar
  addMainTool();

  // add the custom defined key
  // and load toolbar position
  updateKeyToolBar();
  loadToolBarPosition();
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
    windowManager_, SLOT(activateTheWindow(int))));
  initAdditionalActions();

  //create a progress bar to notify the download process
  statusBar_ = new FQTerm::StatusBar(statusBar(), "mainStatusBar");
  statusBar()->addWidget(statusBar_, 0);

  installEventFilter(this);

  QDate lastCheckDate = QDate::fromString(config_->getItemValue("global", "lastcheckupdate"));
  QDate currentDate = QDate::currentDate();
  if (!lastCheckDate.isValid() || lastCheckDate.daysTo(currentDate) >= 31) {
    FQTermAutoUpdater* autoUpdater =
      new FQTermAutoUpdater(this, config_);
    autoUpdater->checkUpdate();  
    config_->setItemValue("global", "lastcheckupdate", currentDate.toString());
  }

  serverThread_ = new FQTermMiniServerThread();
  if (FQTermPref::getInstance()->runServer_)
    serverThread_->start();
#ifdef HAVE_PYTHON
  pythonHelper_ = new FQTermPythonHelper;
#endif
}

//destructor
FQTermFrame::~FQTermFrame() {
#ifdef HAVE_PYTHON
  delete pythonHelper_;
#endif //HAVE_PYTHON
  clearTranslator();
  delete imageViewer_;
  delete shortcutHelper_;
  delete config_;
  delete windowManager_;
  FQTermIPLocation::Destroy();
  serverThread_->quit();
  serverThread_->wait(1000);
  delete serverThread_;

}

//initialize setting from fqterm.cfg
void FQTermFrame::iniSetting() {
  QString strTmp;

  strTmp = config_->getItemValue("global", "fullscreen");
  if (strTmp == "1") {
    isFullScreen_ = true;
    getAction(FQTermShortcutHelper::FULLSCREEN)->setChecked(true);
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
  getAction(FQTermShortcutHelper::COLORCTL_NO)->setChecked(true);
  FQTermPref::getInstance()->escapeString_ = config_->getItemValue("global", "escstr");
  if (FQTermPref::getInstance()->escapeString_ == "") {
    getAction(FQTermShortcutHelper::COLORCTL_NO)->setChecked(true);
  } else if (FQTermPref::getInstance()->escapeString_ == "^[^[[") {
    getAction(FQTermShortcutHelper::COLORCTL_SMTH)->setChecked(true);
  } else if (FQTermPref::getInstance()->escapeString_ == "^u[") {
    getAction(FQTermShortcutHelper::COLORCTL_PTT)->setChecked(true);
  } else {
    getAction(FQTermShortcutHelper::COLORCTL_CUSTOM)->setChecked(true);
  }

  strTmp = config_->getItemValue("global", "vscrollpos");
  if (strTmp == "0") {
    FQTermPref::getInstance()->termScrollBarPosition_ = 0;
  } else if (strTmp == "1") {
    FQTermPref::getInstance()->termScrollBarPosition_ = 1;
  } else {
    FQTermPref::getInstance()->termScrollBarPosition_ = 2;
  }
  strTmp = config_->getItemValue("global", "switchbar");
  isTabBarShown_ = (strTmp != "0");
  getAction(FQTermShortcutHelper::SWITCHBAR)->setChecked(isTabBarShown_);
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

  strTmp = config_->getItemValue("global", "runserver");
  FQTermPref::getInstance()->runServer_ = (strTmp != "0");
}

void FQTermFrame::loadPref() {
  QString strTmp;
  
  strTmp = config_->getItemValue("preference", "displayoffset");
  FQTermPref::getInstance()->displayOffset_ = strTmp.toInt();
  strTmp = config_->getItemValue("preference", "vsetting");
  FQTermPref::getInstance()->vsetting_ = strTmp.toInt();
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

  strTmp = config_->getItemValue("preference", "editor");
  FQTermPref::getInstance()->externalEditor_ = strTmp;

  strTmp = config_->getItemValue("preference", "editorarg");
  FQTermPref::getInstance()->externalEditorArg_ = strTmp;
 
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

  config_->setItemValue("global", "escstr", FQTermPref::getInstance()->escapeString_);

  strTmp.setNum(FQTermPref::getInstance()->termScrollBarPosition_);
  config_->setItemValue("global", "vscrollpos", strTmp);

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

  config_->setItemValue("global", "runserver", FQTermPref::getInstance()->runServer_ ? "1" : "0");

  config_->save(getPath(USER_CONFIG) + "fqterm.cfg");
}

void FQTermFrame::initAdditionalActions()
{
  
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

  FQTERM_ADDACTION(windowManager_, NEXTWINDOW, windowManager_, activateNextWindow);
  FQTERM_ADDACTION(windowManager_, PREVWINDOW, windowManager_, activatePrevWindow);

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
  deleteLater();
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
  FQTERM_ADDACTION(menuWindows_, CASCADEWINDOWS, windowManager_, cascade);
  FQTERM_ADDACTION(menuWindows_, TILEWINDOWS, windowManager_, tile);
  if (windowManager_->count() == 0) {
    getAction(FQTermShortcutHelper::CASCADEWINDOWS)->setEnabled(false);
    getAction(FQTermShortcutHelper::TILEWINDOWS)->setEnabled(false);
  }
  menuWindows_->addSeparator();

#ifdef Q_OS_MACX
  // used to dock the program
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

  int lastIndex = config_->getItemValue("global", "lastaddrindex").toInt();
  if (lastIndex < 0 || lastIndex >= listName.count())
    lastIndex = 0;
  if (listName.count() > 0)
  {    
    menuConnect_->addSeparator();
    QAction *idAction = menuConnect_->addAction(listName[lastIndex], this, SLOT
      (connectMenuActivated()));
    idAction->setData(LAST_ADDRESS_INDEX);
#ifdef __APPLE__
    QString shortCut(tr("Ctrl+Meta"));
#else
    QString shortCut(tr("Ctrl+Alt"));
#endif
    shortCut += "+" + QString("-");
    idAction->setShortcut(shortCut);

  }
}

void FQTermFrame::connectMenuActivated() {
  FQTermConfig *pConf = new FQTermConfig(getPath(USER_CONFIG) + "address.cfg");
  int id = static_cast < QAction * > (sender())->data().toInt();
  if (id == LAST_ADDRESS_INDEX)
  {
    id = config_->getItemValue("global", "lastaddrindex").toInt();
  }
  
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

  deleteLater();
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

void FQTermFrame::googleIt()
{
  windowManager_->activeWindow()->googleIt();
}

void FQTermFrame::externalEditor() {
  windowManager_->activeWindow()->externalEditor();
}

void FQTermFrame::fastPost() {
  windowManager_->activeWindow()->fastPost();
}

void FQTermFrame::copyRect() {
  windowManager_->activeWindow()->getSession()->param().isRectSelect_
      = !windowManager_->activeWindow()->getSession()->param().isRectSelect_;

  getAction(FQTermShortcutHelper::RECTANGLESELECTION)->setChecked(
      windowManager_->activeWindow()->getSession()->param().isRectSelect_);
}

void FQTermFrame::copyColor() {
  windowManager_->activeWindow()->getSession()->param().isColorCopy_
      = !windowManager_->activeWindow()->getSession()->param().isColorCopy_;

  getAction(FQTermShortcutHelper::COPYWITHCOLOR)->setChecked(
      windowManager_->activeWindow()->getSession()->param().isColorCopy_);
}

void FQTermFrame::copyArticle() {
  windowManager_->activeWindow()->copyArticle();
}

void FQTermFrame::autoCopy() {
  windowManager_->activeWindow()->getSession()->param().isAutoCopy_
      = !windowManager_->activeWindow()->getSession()->param().isAutoCopy_;

  getAction(FQTermShortcutHelper::AUTOCOPYSELECTION)->setChecked(
      windowManager_->activeWindow()->getSession()->param().isAutoCopy_);
}

void FQTermFrame::wordWrap() {
  windowManager_->activeWindow()->getSession()->param().isAutoWrap_
      = !windowManager_->activeWindow()->getSession()->param().isAutoWrap_;

  getAction(FQTermShortcutHelper::PASTEWORDWRAP)->setChecked(
      windowManager_->activeWindow()->getSession()->param().isAutoWrap_);
}

void FQTermFrame::noEsc() {
  FQTermPref::getInstance()->escapeString_ = "";
  getAction(FQTermShortcutHelper::COLORCTL_NO)->setChecked(true);
}

void FQTermFrame::escEsc() {
  FQTermPref::getInstance()->escapeString_ = "^[^[[";
  getAction(FQTermShortcutHelper::COLORCTL_SMTH)->setChecked(true);
}

void FQTermFrame::uEsc() {
  FQTermPref::getInstance()->escapeString_ = "^u[";
  getAction(FQTermShortcutHelper::COLORCTL_PTT)->setChecked(true);
}

void FQTermFrame::customEsc() {
  bool ok;
  QString strEsc = QInputDialog::getText(this, "define escape",
                                         "scape string *[", QLineEdit::Normal,
                                         FQTermPref::getInstance()->escapeString_, &ok);
  if (ok) {
    FQTermPref::getInstance()->escapeString_ = strEsc;
    getAction(FQTermShortcutHelper::COLORCTL_CUSTOM)->setChecked(true);
  }
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
   imageViewer_->adjustItemSize();
  }
}

void FQTermFrame::fullscreen() {
  isFullScreen_ = !isFullScreen_;

  if (isFullScreen_) {
    menuBar()->hide();
    toolBarMdiTools_->hide();
    toolBarMdiConnectTools_->hide();
    toolBarSetupKeys_->hide();
    hideScroll();
    showSwitchBar();
    showFullScreen();
  } else {
    menuBar()->show();
    toolBarMdiTools_->show();
    toolBarMdiConnectTools_->show();
    toolBarSetupKeys_->show();
    emit updateScroll();
    showSwitchBar();
    showNormal();
  }

}

void FQTermFrame::bosscolor() {
  FQTermPref::getInstance()->isBossColor_ = !FQTermPref::getInstance()->isBossColor_;
  emit bossColor();
  getAction(FQTermShortcutHelper::BOSSCOLOR)->setChecked(FQTermPref::getInstance()->isBossColor_);
}


void FQTermFrame::toggleServer(bool on) {
  FQTermPref::getInstance()->runServer_ = on;
  if (on) {
    serverThread_->start();
  } else {
    serverThread_->quit();
    serverThread_->wait(1000);
  }
}

void FQTermFrame::themesMenuAboutToShow() {
  QVector<QChar> vectorShortcutKeys;
  menuThemes_->clear();
  QStringList styles = QStyleFactory::keys();
  for (QStringList::ConstIterator it = styles.begin();
       it != styles.end(); it++) {
    QString strTheme = *it;
    for (int i = 0; i < strTheme.length(); ++i)
    {
      if (vectorShortcutKeys.indexOf(strTheme.at(i)) == -1)
      {
        vectorShortcutKeys.append(strTheme.at(i));
        strTheme.insert(i, QChar('&'));
        break;
      }
    }
    insertThemeItem(strTheme);
  }
}

void FQTermFrame::themesMenuActivated() {
  theme_ = ((QAction*)QObject::sender())->text().remove(QChar('&'));
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
  getAction(FQTermShortcutHelper::SWITCHBAR)->setChecked(isTabBarShown_);

  if (isTabBarShown_) {
    statusBar()->show();
  } else {
    statusBar()->hide();
  }
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

  addrDialog set(this, tmpParam, addrDialog::SAVE);

  if (set.exec() == 2) {
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

void FQTermFrame::shortcutSetting() {
  int act = windowManager_->activeWindowIndex();
  FQTermShortcutDialog fsd(shortcutHelper_, this);
  fsd.exec();
  windowManager_->activateTheWindow(act);
}

void FQTermFrame::keySetup() {
  keyDialog keyDlg(config_, this);
  if (keyDlg.exec() == 1) {
    updateKeyToolBar();
  }
}


void FQTermFrame::ipLookup() {
  if (!ipLookupDialog_)
    ipLookupDialog_ = new IPLookupDialog(this);
  ipLookupDialog_->show();
}

void FQTermFrame::antiIdle() {
  windowManager_->activeWindow()->toggleAntiIdle();
  getAction(FQTermShortcutHelper::ANTIIDLE)->setChecked(
      windowManager_->activeWindow()->getSession()->isAntiIdle());
}

void FQTermFrame::autoReply() {
  windowManager_->activeWindow()->toggleAutoReply();
  getAction(FQTermShortcutHelper::AUTOREPLY)->setChecked(
      windowManager_->activeWindow()->getSession()->isAutoReply());
}

void FQTermFrame::viewMessages() {
  windowManager_->activeWindow()->viewMessages();
}

void FQTermFrame::enableMouse() {
  windowManager_->activeWindow()->getSession()->param().isSupportMouse_
      = !windowManager_->activeWindow()->getSession()->param().isSupportMouse_;
  getAction(FQTermShortcutHelper::MOUSESUPPORT)->setChecked(
      windowManager_->activeWindow()->getSession()->param().isSupportMouse_);
}

void FQTermFrame::viewImages(QString filename, bool raiseViewer) {
  if (filename.isEmpty()) {
    filename = FQTermPref::getInstance()->poolDir_;
  }

  if (raiseViewer) {
    imageViewer_->scrollTo(filename);
    if (imageViewer_->isHidden()) {
      imageViewer_->resize(size() * 3 / 4 + QSize(1,1));
      imageViewer_->show();
    }
    clearFocus();
    imageViewer_->raise();
    imageViewer_->activateWindow();
  } else {
    imageViewer_->updateImage(filename);
  }
}

void FQTermFrame::viewImages() {
  viewImages(FQTermPref::getInstance()->poolDir_, true);
}

void FQTermFrame::beep() {
  windowManager_->activeWindow()->getSession()->param().isBeep_ =
      !windowManager_->activeWindow()->getSession()->param().isBeep_;
  getAction(FQTermShortcutHelper::BEEP)->setChecked(windowManager_->activeWindow()->getSession()->param().isBeep_);
}

void FQTermFrame::reconnect() {
  FQTermWindow * qtw = windowManager_->activeWindow();
  if (qtw){
    qtw->toggleAutoReconnect();
  }
}


void FQTermFrame::delayedRunScript() {
  windowManager_->activeWindow()->runScript();
}


void FQTermFrame::runScript() {
  QTimer::singleShot(1, this, SLOT(delayedRunScript()));
}

void FQTermFrame::stopScript() {
  windowManager_->activeWindow()->stopScript();
}


void FQTermFrame::runPyScript() {
#ifdef HAVE_PYTHON
  windowManager_->activeWindow()->runPythonScript();
#endif //HAVE_PYTHON
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
    QString scriptFile = strTmp.mid(1).trimmed();
#ifdef HAVE_PYTHON
    if (scriptFile.endsWith(".py", Qt::CaseInsensitive))
      windowManager_->activeWindow()->runPythonScriptFile(scriptFile);
    else
#endif
      windowManager_->activeWindow()->runScript(scriptFile.toAscii());
  } else if (strTmp[0] == '2') { // program
    runProgram(strTmp.mid(1));
  }
}

void FQTermFrame::addMainTool() {
  // the main toolbar
  toolBarMdiTools_ = addToolBar("Main ToolBar");
  toolBarMdiTools_->setObjectName("Main ToolBar");
  //toolBarMdiTools_->setIconSize(QSize(22, 22));

  connectButton_ = new QToolButton(toolBarMdiTools_);
  connectButton_->setIcon(QPixmap(getPath(RESOURCE) + "pic/connect.png"));

  toolBarMdiTools_->addWidget(connectButton_);
  menuConnect_ = new QMenu(this);
  //FIXME: autoupdate menu
  FQ_VERIFY(connect(menuConnect_, SIGNAL(aboutToShow()),
                    this, SLOT(popupConnectMenu())));
  connectButton_->setMenu(menuConnect_);
  connectButton_->setPopupMode(QToolButton::InstantPopup);

  toolBarMdiTools_->addAction(getAction(FQTermShortcutHelper::QUICKLOGIN));

  serverButton_ = new QToolButton(toolBarMdiTools_);
  serverButton_->setCheckable(true);
  serverButton_->setIcon(QPixmap(getPath(RESOURCE) + "pic/fqterm_32x32.png"));
  serverButton_->setChecked(FQTermPref::getInstance()->runServer_);
  FQ_VERIFY(connect(serverButton_, SIGNAL(toggled(bool)), this, SLOT(toggleServer(bool))));
  toolBarMdiTools_->addWidget(serverButton_);
  // custom define
  toolBarSetupKeys_ = addToolBar("Custom Key");
  toolBarSetupKeys_->setObjectName("Custom Key");

  // the toolbar
  toolBarMdiConnectTools_ = addToolBar("bbs operations");
  toolBarMdiConnectTools_->setObjectName("bbs operations");
  FQTERM_ADDACTION(toolBarMdiConnectTools_, DISCONNECT, this, disconnect);
  getAction(FQTermShortcutHelper::DISCONNECT)->setEnabled(false);
  toolBarMdiConnectTools_->addSeparator();

  // Edit (5)
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::COPY));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::PASTE));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::COPYWITHCOLOR));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::RECTANGLESELECTION));
  toolBarMdiConnectTools_->addSeparator();

  //View (3)
  fontButton_ = new QToolButton(toolBarMdiConnectTools_);
  QAction* dummyAction = new QAction(QPixmap(getPath(RESOURCE) + "pic/change_fonts.png"),
    tr("Set Terminal Fonts"), fontButton_);
  fontButton_->setDefaultAction(dummyAction);
  toolBarMdiConnectTools_->addWidget(fontButton_);
  fontButton_->setMenu(menuFont_);
  fontButton_->setPopupMode(QToolButton::InstantPopup);

  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::COLORSETTING));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::ANSICOLOR));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::REFRESHSCREEN));
  toolBarMdiConnectTools_->addSeparator();

  // Option
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::CURRENTSETTING));
  toolBarMdiConnectTools_->addSeparator();

  // Spec (5)
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::COPYARTICLE));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::ANTIIDLE));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::AUTOREPLY));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::VIEWMESSAGE));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::IMAGEVIEWER));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::MOUSESUPPORT));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::BEEP));
  toolBarMdiConnectTools_->addAction(getAction(FQTermShortcutHelper::AUTORECONNECT));

  //call popupConnectMenu() to enable the shortcuts
  popupConnectMenu(); 

}


QAction* FQTermFrame::getAction(int shortcut) {
  if (!shortcutHelper_)
    return NULL;
  return shortcutHelper_->getAction(shortcut);
}



void FQTermFrame::addMainMenu() {
  menuMain_ = menuBar(); 

  //File Menu
  menuFile_ = menuMain_->addMenu(tr("&File"));

  FQTERM_ADDACTION(menuFile_, CONNECT, this, connectIt);
  FQTERM_ADDACTION(menuFile_, DISCONNECT, this, disconnect);
  menuFile_->addSeparator();
  FQTERM_ADDACTION(menuFile_, ADDRESSBOOK, this, addressBook);
  FQTERM_ADDACTION(menuFile_, QUICKLOGIN, this, quickLogin);
  menuFile_->addSeparator();
  FQTERM_ADDACTION(menuFile_, EXIT, this, exitFQTerm);
  getAction(FQTermShortcutHelper::EXIT)->setMenuRole(QAction::QuitRole);

  //Edit Menu
  QMenu *edit = menuMain_->addMenu(tr("&Edit"));

  FQTERM_ADDACTION(edit, COPY, this, copy);
  FQTERM_ADDACTION(edit, PASTE, this, paste);
  edit->addSeparator();
  FQTERM_ADDACTION(edit, COPYWITHCOLOR, this, copyColor);
  FQTERM_ADDACTION(edit, RECTANGLESELECTION, this, copyRect);
  FQTERM_ADDACTION(edit, AUTOCOPYSELECTION, this, autoCopy);
  FQTERM_ADDACTION(edit, PASTEWORDWRAP, this, wordWrap);
  edit->addSeparator();
  FQTERM_ADDACTION(edit, GOOGLEIT, this, googleIt);
  edit->addSeparator();
  FQTERM_ADDACTION(edit, EXTERNALEDITOR, this, externalEditor);
  edit->addSeparator();
  FQTERM_ADDACTION(edit, FASTPOST, this, fastPost);
  edit->addSeparator();

  QMenu *escapeMenu = edit->addMenu(tr("Control sequence in clipboar&d"));
  escapeGroup = new QActionGroup(this);
  FQTERM_ADDACTION(escapeMenu, COLORCTL_NO,this, noEsc);
  escapeGroup->addAction(getAction(FQTermShortcutHelper::COLORCTL_NO));
  FQTERM_ADDACTION(escapeMenu, COLORCTL_SMTH,this, escEsc);
  escapeGroup->addAction(getAction(FQTermShortcutHelper::COLORCTL_SMTH));
  FQTERM_ADDACTION(escapeMenu, COLORCTL_PTT,this, uEsc);
  escapeGroup->addAction(getAction(FQTermShortcutHelper::COLORCTL_PTT));
  FQTERM_ADDACTION(escapeMenu, COLORCTL_CUSTOM,this, customEsc);
  escapeGroup->addAction(getAction(FQTermShortcutHelper::COLORCTL_CUSTOM));

  //View menu
  QMenu *view = menuMain_->addMenu(tr("&View"));

  menuFont_ = view->addMenu(tr("&Font"));
  menuFont_->setIcon(QPixmap(getPath(RESOURCE) + "pic/change_fonts.png"));
  view->addMenu(menuFont_);
  FQTERM_ADDACTION(menuFont_, NONENGLISHFONT, this, setFont);
  getAction(FQTermShortcutHelper::NONENGLISHFONT)->setData(0);
  FQTERM_ADDACTION(menuFont_, ENGLISHFONT, this, setFont);
  getAction(FQTermShortcutHelper::ENGLISHFONT)->setData(1);
  FQTERM_ADDACTION(view, COLORSETTING, this, setColor);
  FQTERM_ADDACTION(view, ANSICOLOR, this, toggleAnsiColor);
  FQTERM_ADDACTION(view, REFRESHSCREEN, this, refreshScreen);
  view->addSeparator();

  //language menu
  languageGroup = new QActionGroup(view);
  languageGroup->setExclusive(true);
  if (installerList_.isEmpty()) {
    FQ_TRACE("frame", 0)
        << "No language menu because of lack of translation files";
  } else {
    menuLanguage_ = view->addMenu(tr("&Language"));
    FQTERM_ADDACTION(menuLanguage_, LANGUAGE_ENGLISH, this, langEnglish);
    languageGroup->addAction(getAction(FQTermShortcutHelper::LANGUAGE_ENGLISH));

    foreach(TranslatorInstaller* installer, installerList_) {
      QAction* action = menuLanguage_->addAction(
          installer->languageName(), installer, SLOT(installTranslator()));
      action->setCheckable(true);
      languageGroup->addAction(action);
    }

  } 

  FQTERM_ADDACTION(view, UIFONT, this, uiFont);

  menuThemes_ = view->addMenu(tr("&Themes"));
  FQ_VERIFY(connect(menuThemes_, SIGNAL(aboutToShow()), this, SLOT(themesMenuAboutToShow())));

  FQTERM_ADDACTION(view, FULLSCREEN, this, fullscreen);
  FQTERM_ADDACTION(view, BOSSCOLOR, this, bosscolor);

  view->addSeparator();

  scrollMenu_ = view->addMenu(tr("&ScrollBar"));
  FQ_VERIFY(connect(scrollMenu_, SIGNAL(aboutToShow()), this, SLOT(scrollMenuAboutToShow())));
  FQTERM_ADDACTION(view, SWITCHBAR, this, showSwitchBar);

  // Option Menu
  QMenu *option = menuMain_->addMenu(tr("&Option"));

  FQTERM_ADDACTION(option, CURRENTSETTING, this, setting);
  option->addSeparator();
  FQTERM_ADDACTION(option, DEFAULTSETTING, this, defaultSetting);
#ifdef Q_OS_MAC
  FQTERM_ADDACTION(menuFile_, PREFERENCE, this, preference);
#else
  FQTERM_ADDACTION(option, PREFERENCE, this, preference);
#endif
  getAction(FQTermShortcutHelper::PREFERENCE)->setMenuRole(QAction::PreferencesRole);
  FQTERM_ADDACTION(option, SHORTCUTSETTING, this, shortcutSetting);
  option->addSeparator();
  FQTERM_ADDACTION(option, EDITSCHEMA, this, editSchema);

  // Special
  QMenu *spec = menuMain_->addMenu(tr("&Special"));
  FQTERM_ADDACTION(spec, COPYARTICLE, this, copyArticle);
  FQTERM_ADDACTION(spec, ANTIIDLE, this, antiIdle);
  FQTERM_ADDACTION(spec, AUTOREPLY, this, autoReply);
  FQTERM_ADDACTION(spec, VIEWMESSAGE, this, viewMessages);
  FQTERM_ADDACTION(spec, BEEP, this, beep);
  FQTERM_ADDACTION(spec, MOUSESUPPORT, this, enableMouse);
  FQTERM_ADDACTION(spec, IMAGEVIEWER, this, viewImages);
  FQTERM_ADDACTION(spec, IPLOOKUP, this, ipLookup);

  //Script
  QMenu *script = menuMain_->addMenu(tr("Scrip&t"));
  FQTERM_ADDACTION(script, RUNSCRIPT, this, runScript);
  FQTERM_ADDACTION(script, STOPSCRIPT, this, stopScript);
#ifdef HAVE_PYTHON
  script->addSeparator();
  FQTERM_ADDACTION(script, RUNPYTHONSCRIPT, this, runPyScript);
#endif //HAVE_PYTHON

  //Window menu
  menuWindows_ = menuMain_->addMenu(tr("&Windows"));
  FQ_VERIFY(connect(menuWindows_, SIGNAL(aboutToShow()),this, SLOT(windowsMenuAboutToShow())));
  menuMain_->addSeparator();

  //Help menu
  QMenu *help = menuMain_->addMenu(tr("&Help"));
  FQTERM_ADDACTION(help, ABOUT, this, aboutFQTerm);
  getAction(FQTermShortcutHelper::ABOUT)->setMenuRole(QAction::AboutRole);
  FQTERM_ADDACTION(help, HOMEPAGE, this, homepage);
}

void FQTermFrame::updateMenuToolBar() {
  FQTermWindow *window = windowManager_->activeWindow();

  if (window == NULL) {
    return ;
  }

  // update menu
  getAction(FQTermShortcutHelper::DISCONNECT)->setEnabled(window->isConnected());
  getAction(FQTermShortcutHelper::COPYWITHCOLOR)->setChecked(window->getSession()->param().isColorCopy_);
  getAction(FQTermShortcutHelper::RECTANGLESELECTION)->setChecked(window->getSession()->param().isRectSelect_);
  getAction(FQTermShortcutHelper::AUTOCOPYSELECTION)->setChecked(window->getSession()->param().isAutoCopy_);
  getAction(FQTermShortcutHelper::PASTEWORDWRAP)->setChecked(window->getSession()->param().isAutoWrap_);
  getAction(FQTermShortcutHelper::FULLSCREEN)->setChecked(isFullScreen_);
  getAction(FQTermShortcutHelper::ANSICOLOR)->setChecked(window->getSession()->param().isAnsiColor_);
  getAction(FQTermShortcutHelper::ANTIIDLE)->setChecked(window->getSession()->isAntiIdle());
  getAction(FQTermShortcutHelper::AUTOREPLY)->setChecked(window->getSession()->isAutoReply());
  getAction(FQTermShortcutHelper::BEEP)->setChecked(window->getSession()->param().isBeep_);
  getAction(FQTermShortcutHelper::MOUSESUPPORT)->setChecked(window->getSession()->param().isSupportMouse_);
  getAction(FQTermShortcutHelper::AUTORECONNECT)->setChecked(window->getSession()->param().isAutoReconnect_);

  
}

void FQTermFrame::enableMenuToolBar(bool enable) {
  getAction(FQTermShortcutHelper::DISCONNECT)->setEnabled(enable);
  getAction(FQTermShortcutHelper::COPY)->setEnabled(enable);
  getAction(FQTermShortcutHelper::PASTE)->setEnabled(enable);
  getAction(FQTermShortcutHelper::COPYWITHCOLOR)->setEnabled(enable);
  getAction(FQTermShortcutHelper::RECTANGLESELECTION)->setEnabled(enable);
  getAction(FQTermShortcutHelper::AUTOCOPYSELECTION)->setEnabled(enable);
  getAction(FQTermShortcutHelper::PASTEWORDWRAP)->setEnabled(enable);
  getAction(FQTermShortcutHelper::ANSICOLOR)->setEnabled(enable);

  fontButton_->setEnabled(enable);
  menuFont_->setEnabled(enable);

  getAction(FQTermShortcutHelper::COLORSETTING)->setEnabled(enable);
  getAction(FQTermShortcutHelper::REFRESHSCREEN)->setEnabled(enable);
  getAction(FQTermShortcutHelper::CURRENTSETTING)->setEnabled(enable);
  getAction(FQTermShortcutHelper::COPYARTICLE)->setEnabled(enable);
  getAction(FQTermShortcutHelper::ANTIIDLE)->setEnabled(enable);
  getAction(FQTermShortcutHelper::AUTOREPLY)->setEnabled(enable);
  getAction(FQTermShortcutHelper::VIEWMESSAGE)->setEnabled(enable);
  getAction(FQTermShortcutHelper::BEEP)->setEnabled(enable);
  getAction(FQTermShortcutHelper::MOUSESUPPORT)->setEnabled(enable);
  getAction(FQTermShortcutHelper::RUNSCRIPT)->setEnabled(enable);
  getAction(FQTermShortcutHelper::STOPSCRIPT)->setEnabled(enable);
#ifdef HAVE_PYTHON
  getAction(FQTermShortcutHelper::RUNPYTHONSCRIPT)->setEnabled(enable);
#endif 
  getAction(FQTermShortcutHelper::GOOGLEIT)->setEnabled(enable);
  getAction(FQTermShortcutHelper::EXTERNALEDITOR)->setEnabled(enable);
  getAction(FQTermShortcutHelper::FASTPOST)->setEnabled(enable);
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
    button->setText(strTmp);
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

void FQTermFrame::insertThemeItem(const QString& themeitem) {
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

  trayMenu_ = new QMenu;
  FQ_VERIFY(connect(trayMenu_, SIGNAL(aboutToShow()), SLOT(buildTrayMenu())));

  tray = new QSystemTrayIcon(this);
  tray->setIcon(QPixmap(getPath(RESOURCE) + "pic/fqterm_tray.png"));
  tray->setContextMenu(trayMenu_);
  FQ_VERIFY(connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                    SLOT(trayActived(QSystemTrayIcon::ActivationReason))));
  tray->show();
}

void FQTermFrame::buildTrayMenu() {
  if (!trayMenu_) {
    return ;
  }
  trayMenu_->clear();

  if (isHidden()) {
    trayMenu_->addAction(tr("Show"), this, SLOT(trayShow()));
  } else {
    trayMenu_->addAction(tr("Hide"), this, SLOT(trayHide()));
  }
  trayMenu_->addSeparator();
  trayMenu_->addAction(tr("About"), this, SLOT(aboutFQTerm()));
  trayMenu_->addAction(tr("Exit"), this, SLOT(exitFQTerm()));
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
  QString str = config_->getItemValue("global", "traytipshown");
  if (str == "1") {
    showTip = false;
  }
  hide();
  if (showTip) {
    tray->showMessage(tr("FQTerm"),
                      tr("FQTerm will keep running in the system tray.\n"
                         "To terminate the program, "
                         "choose exit in the tray menu."));
    showTip = false;
    config_->setItemValue("global", "traytipshown", "1");
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

  if (shortcutHelper_) {
    shortcutHelper_->retranslateActions();
  }
  menuBar()->clear();
  delete escapeGroup;
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
    enableMenuToolBar(true);
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
  FQTERM_ADDACTION(scrollMenu_, SCROLLBAR_HIDDEN, this, hideScroll);
  FQTERM_ADDACTION(scrollMenu_, SCROLLBAR_RIGHT, this, rightScroll);
  FQTERM_ADDACTION(scrollMenu_, SCROLLBAR_LEFT, this, leftScroll);
  switch(FQTermPref::getInstance()->termScrollBarPosition_) {
    case 0:
      getAction(FQTermShortcutHelper::SCROLLBAR_HIDDEN)->setChecked(true);
      break;
    case 1:
      getAction(FQTermShortcutHelper::SCROLLBAR_LEFT)->setChecked(true);
      break;
    case 2:
      getAction(FQTermShortcutHelper::SCROLLBAR_RIGHT)->setChecked(true);
      break;
  }
}

void FQTermFrame::setFont() {
  bool isEnglish = (QAction*)(sender()) == getAction(FQTermShortcutHelper::ENGLISHFONT);
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
  windowManager_->activeWindow()->getSession()->param().isAnsiColor_
    = !windowManager_->activeWindow()->getSession()->param().isAnsiColor_;
  getAction(FQTermShortcutHelper::ANSICOLOR)->setChecked(
    windowManager_->activeWindow()->getSession()->param().isAnsiColor_);
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

void FQTermFrame::editSchema() {

  schemaDialog schema(this);
  FQ_VERIFY(connect(&schema, SIGNAL(schemaEdited()),
                  this, SLOT(schemaUpdated())));
  schema.exec();
}

void FQTermFrame::schemaUpdated() {
  for (int i = 0; i < windowManager_->count(); ++i) {
    windowManager_->nthWindow(i)->getScreen()->setSchema();
    windowManager_->nthWindow(i)->forcedRepaintScreen();
  }
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
