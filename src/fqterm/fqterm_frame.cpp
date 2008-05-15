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

#if !defined(WIN32)
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <QDesktopServices>
#include <QDir>
#include <QFontDialog>
#include <QInputDialog>
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

#include "aboutdialog.h"
#include "addrdialog.h"
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

namespace FQTerm {

const QString FQTermFrame::qmPrefix[FQTermFrame::translatedModule] =
{"fqterm_", "ui_"};
const QString FQTermFrame::qmPostfix = ".qm";

//constructor
FQTermFrame::FQTermFrame()
    : QMainWindow(0), 
      tray(0),
      menuTray_(0){
  setAttribute(Qt::WA_DeleteOnClose);

  config_ = new FQTermConfig(getPath(USER_CONFIG) + "fqterm.cfg");

  image_ = new FQTermImage(config_, NULL, Qt::Window);

  mdiArea_ = new QMdiArea(this);

  setCentralWidget(mdiArea_);

  windowMapper_ = new QSignalMapper(this);
  FQ_VERIFY(connect(windowMapper_, SIGNAL(mapped(int)),
                    this, SLOT(windowsMenuActivated(int))));

  initTranslator();

  //set menubar
  addMainMenu();
  FQ_VERIFY(connect(this, SIGNAL(changeLanguage()),
                    this, SLOT(recreateMenu())));

  //setup toolbar
  addMainTool();

  // add the custom defined key
  // and load toolbar position
  updateToolBar();

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
  tabBar_ = new QTabBar(hb);
  hbLayout->addWidget(tabBar_);
  FQ_VERIFY(connect(tabBar_, SIGNAL(currentChanged(int)),
                    this, SLOT(selectionChanged(int))));
  tabBar_->setShape(QTabBar::RoundedSouth);

  //create a progress bar to notify the download process
  statusBar_ = new FQTerm::StatusBar(statusBar(), "mainStatusBar");

  statusBar()->addWidget(statusBar_, 0);

  //create the window manager to deal with the window-tab-icon pairs
  windowManager_ = new FQTermWndMgr(this);

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
  actionNextWindow_->setShortcut(opt + tr("+Right"));
  FQ_VERIFY(connect(actionNextWindow_, SIGNAL(triggered()),
                    mdiArea_, SLOT(activateNextSubWindow())));
  addAction(actionNextWindow_);

  actionPrevWindow_ = new QAction(this);
  actionPrevWindow_->setShortcut(opt + tr("+Left"));
  FQ_VERIFY(connect(actionPrevWindow_, SIGNAL(triggered()),
                    mdiArea_, SLOT(activatePreviousSubWindow())));
  addAction(actionPrevWindow_);

  //initialize all settings
  iniSetting();

  installEventFilter(this);

  FQTermAutoUpdater* autoUpdater =
      new FQTermAutoUpdater(this, config_, preference_.zmodemPoolDir_);
  autoUpdater->checkUpdate();  

  if (preference_.useStyleSheet_) {
    loadStyleSheetFromFile(preference_.styleSheetFile_);
  }
}

//destructor
FQTermFrame::~FQTermFrame() {
  clearTranslator();
  delete image_;
  delete config_;
  delete windowManager_;
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

  actionNoEscape_->setChecked(true);
  escapeString_ = "";

  strTmp = config_->getItemValue("global", "clipcodec");
  if (strTmp == "0") {
    clipboardEncodingID_ = 0;
    actionGBK_->setChecked(true);
  } else {
    clipboardEncodingID_ = 1;
    actionBIG5_->setChecked(true);
  }

  strTmp = config_->getItemValue("global", "vscrollpos");
  if (strTmp == "0") {
    termScrollBarPosition_ = 0;
  } else if (strTmp == "1") {
    termScrollBarPosition_ = 1;
  } else {
    termScrollBarPosition_ = 2;
  }

  strTmp = config_->getItemValue("global", "statusbar");
  isStatusBarShown_ = (strTmp != "0");
  actionStatus_->setChecked(isStatusBarShown_);

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
  if (strTmp == "" || strTmp == "1") {
    subWindowMax_ = true;
  } else {
    subWindowMax_ = false;
  }

  strTmp = config_->getItemValue("global", "subwindowsize");
  if (strTmp != "") {
    int w, h;
    sscanf(strTmp.toLatin1(), "%d %d", &w, &h);
    subWindowSize_ = QSize(w, h);
  } else {
    //Magic Number. Initialize Window Size to Avoid Errors.
    subWindowSize_ = QSize(640, 480);
  }

  isBossColor_ = false;

  loadPref(config_);

  setUseDock(preference_.openMinimizeToTray_);
}

void FQTermFrame::loadPref(FQTermConfig *conf) {
  QString strTmp;
  strTmp = conf->getItemValue("preference", "xim");
  preference_.serverEncodingID_ = strTmp.toInt();
  strTmp = conf->getItemValue("preference", "wordwrap");
  preference_.widthToWrapWord_ = strTmp.toInt();
  //  strTmp = conf->getItemValue("preference", "smartww");
  //  m_pref.bSmartWW = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "wheel");
  preference_.isWheelSupported_ = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "url");
  preference_.openUrlCheck_ = (strTmp != "0");
  //  strTmp = conf->getItemValue("preference", "logmsg");
  //  m_pref.bLogMsg = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "blinktab");
  preference_.openTabBlinking_ = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "warn");
  preference_.openWarnOnClose_ = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "beep");
  preference_.openBeep_ = strTmp.toInt();
  preference_.beepSoundFileName_ = conf->getItemValue("preference", "wavefile");
  //  strTmp = conf->getItemValue("preference", "http");
  //  m_pref.strHttp = strTmp;
  strTmp = conf->getItemValue("preference", "antialias");
  preference_.openAntiAlias_ = (strTmp != "0");
  strTmp = conf->getItemValue("preference", "tray");
  if (strTmp.isEmpty()) {
#if defined(__APPLE__)
    preference_.openMinimizeToTray_ = false;
#else
    preference_.openMinimizeToTray_ = true;    
#endif
  } else {
    preference_.openMinimizeToTray_ = (strTmp != "0");
  }
  strTmp = conf->getItemValue("preference", "playmethod");
  preference_.beepMethodID_ = strTmp.toInt();
  strTmp = conf->getItemValue("preference", "externalplayer");
  preference_.beepPlayerName_ = strTmp;

  strTmp = conf->getItemValue("preference", "clearpool");
  preference_.needClearZmodemPoolOnClose_ = (strTmp == "1");
  strTmp = conf->getItemValue("preference", "pool");
  preference_.zmodemPoolDir_ = strTmp.isEmpty() ?
                               getPath(USER_CONFIG) + "pool/": strTmp;
  if (preference_.zmodemPoolDir_.right(1) != "/") {
    preference_.zmodemPoolDir_.append('/');
  }
  strTmp = conf->getItemValue("preference", "zmodem");
  preference_.zmodemDir_ = strTmp.isEmpty() ?
                           getPath(USER_CONFIG) + "zmodem/": strTmp;
  if (preference_.zmodemDir_.right(1) != "/") {
    preference_.zmodemDir_.append('/');
  }
  strTmp = conf->getItemValue("preference", "image");
  preference_.imageViewerName_ = strTmp;
  strTmp = conf->getItemValue("preference", "qssfile");
  preference_.styleSheetFile_ = strTmp;
  preference_.useStyleSheet_ = !strTmp.isEmpty();
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

  strTmp.setNum(clipboardEncodingID_);
  config_->setItemValue("global", "clipcodec", strTmp);

  strTmp.setNum(termScrollBarPosition_);
  config_->setItemValue("global", "vscrollpos", strTmp);

  config_->setItemValue("global", "statusbar", isStatusBarShown_ ? "1" : "0");
  config_->setItemValue("global", "switchbar", isTabBarShown_ ? "1" : "0");

  //save subwindow setting
  config_->setItemValue("global", "subwindowmax", subWindowMax_ ? "1" : "0");
  int w = subWindowSize_.width();
  int h = subWindowSize_.height();

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
  loadAddress(config_, -1, quick.param_);

  if (quick.exec() == 1) {
    newWindow(quick.param_);
  }
}

void FQTermFrame::exitQTerm() {
  
  while (windowManager_->count() > 0) {
    bool closed = windowManager_->activeWindow()->close();
    if (!closed) {
      return ;
    }
  }

  saveSetting();
  // clear zmodem and pool if needed
  if (preference_.needClearZmodemPoolOnClose_) {
    clearDir(preference_.zmodemDir_);
    clearDir(preference_.zmodemPoolDir_);
    clearDir(preference_.zmodemPoolDir_ + "shadow-cache/");
  }

  setUseDock(false);
  qApp->quit();
}

void FQTermFrame::newWindow(const FQTermParam &param, int index) {
  FQTermWindow *window = new FQTermWindow(this, param, index, mdiArea_, 0);

  
  QMdiSubWindow* subWindow = mdiArea_->addSubWindow(window);
  subWindow->setAttribute(Qt::WA_NoSystemBackground);

  QIcon *icon = new QIcon(QPixmap(getPath(RESOURCE) + "pic/tabpad.png"));
  //QTab *qtab=new QTab(*icon,window->caption());
  QString qtab = window->windowTitle();

  //add window-tab-icon to window manager
  windowManager_->addWindow(window, qtab, icon);

  //if no this call, the tab wont display untill you resize the window
  tabBar_->addTab(*icon, qtab);
  tabBar_->updateGeometry();
  tabBar_->update();

  //activte the window-tab
  windowManager_->activateTheTab(window);

  subWindow->resize(subWindowSize_);
  if (subWindowMax_) {
    subWindow->setWindowFlags(Qt::SubWindow | Qt::CustomizeWindowHint
                              | Qt::WindowMinMaxButtonsHint);
    subWindow->showMaximized();
  } else {
    subWindow->setWindowFlags(Qt::SubWindow | Qt::CustomizeWindowHint
                              | Qt::WindowMinMaxButtonsHint
                              | Qt::WindowSystemMenuHint);
    subWindow->show();
  }

  FQ_VERIFY(connect(window, SIGNAL(resizeSignal(FQTermWindow*)),
                    this, SLOT(subWindowResized(FQTermWindow*))));
}

//the tabbar selection changed
void FQTermFrame::selectionChanged(int n) {
  QString qtab = tabBar_->tabText(n);
  windowManager_->activateTheWindow(qtab, n);
}

void FQTermFrame::aboutQTerm() {
  aboutDialog about(this);
  about.exec();
}

//slot Help->Homepage
void FQTermFrame::homepage() {
  QDesktopServices::openUrl(QString("http://code.google.com/p/fqterm"));
}

//slot Windows menu aboutToShow
void FQTermFrame::windowsMenuAboutToShow() {
  menuWindows_->clear();
  QAction *cascadeAction = menuWindows_->addAction(tr("Cascade"), this, SLOT
                                                   (cascade()));
  QAction *tileAction = menuWindows_->addAction(tr("Tile"), this, SLOT(tile()));
  if (mdiArea_->subWindowList().isEmpty()) {
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

  QList<QMdiSubWindow *> subWins = mdiArea_->subWindowList();

  for (int i = 0; i < int(subWins.count()); ++i) {
    QAction *idAction = menuWindows_->addAction(subWins.at(i)->windowTitle());
    idAction->setCheckable(true);
    idAction->setChecked(mdiArea_->activeSubWindow() == subWins.at(i));
    connect(idAction, SIGNAL(triggered()), windowMapper_, SLOT(map()));
    windowMapper_->setMapping(idAction, i);
  }
}

//slot activate the window correspond with the menu id
void FQTermFrame::windowsMenuActivated(int id) {
  if (id < 0 || id >= mdiArea_->subWindowList().size()) {
    return;
  }
  QMdiSubWindow *w = mdiArea_->subWindowList().at(id);
  if (w) {
    mdiArea_->setActiveSubWindow(w);
    if (!w->isMaximized()) {
      w->showNormal();
    }
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

void FQTermFrame::switchWin(int id) {
  QList<QMdiSubWindow *> subWins = mdiArea_->subWindowList();

  if (subWins.count() == 0) {
    return ;
  }

  if (id == 200) {
    windowManager_->activeNextPrev(false);
    return ;
  }
  if (id == 201 || id == 202) {
    windowManager_->activeNextPrev(true);
    return ;
  }

  QMdiSubWindow *w = subWins.at(id -1);
  if (w == mdiArea_->activeSubWindow()) {
    return ;
  }

  if (w != NULL) {
    w->showNormal();
  }
}

bool FQTermFrame::eventFilter(QObject *o, QEvent *e) {
  return false;
}

//slot draw something e.g. logo in the background
//TODO : draw a pixmap in the background
void FQTermFrame::paintEvent(QPaintEvent*){
}

void FQTermFrame::closeEvent(QCloseEvent *clse) {
  if (preference_.openMinimizeToTray_) {
    trayHide();
    clse->ignore();
    return ;
  }

  while (windowManager_->count() > 0) {
    bool closed = mdiArea_->activeSubWindow()->close();
    if (!closed) {
      clse->ignore();
      return;
    }
  }

  saveSetting();
  // clear zmodem and pool if needed
  if (preference_.needClearZmodemPoolOnClose_) {
    clearDir(preference_.zmodemDir_);
    clearDir(preference_.zmodemPoolDir_);
    clearDir(preference_.zmodemPoolDir_ + "shadow-cache/");

  }

  setUseDock(false);

  clse->accept();

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
    windowManager_->activeWindow()->session_->reconnect();
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
  windowManager_->activeWindow()->session_->param_.isRectSelect_
      = !windowManager_->activeWindow()->session_->param_.isRectSelect_;

  actionRectangleSelect_->setChecked(
      windowManager_->activeWindow()->session_->param_.isRectSelect_);
}

void FQTermFrame::copyColor() {
  windowManager_->activeWindow()->session_->param_.isColorCopy_
      = !windowManager_->activeWindow()->session_->param_.isColorCopy_;

  actionColorCopy_->setChecked(
      windowManager_->activeWindow()->session_->param_.isColorCopy_);
}

void FQTermFrame::copyArticle() {
  windowManager_->activeWindow()->copyArticle();
}

void FQTermFrame::autoCopy() {
  windowManager_->activeWindow()->session_->param_.isAutoCopy_
      = !windowManager_->activeWindow()->session_->param_.isAutoCopy_;

  actionAutoCopy_->setChecked(
      windowManager_->activeWindow()->session_->param_.isAutoCopy_);
}

void FQTermFrame::wordWrap() {
  windowManager_->activeWindow()->session_->isWordWrap_
      = !windowManager_->activeWindow()->session_->isWordWrap_;

  actionWordWrap_->setChecked(
      windowManager_->activeWindow()->session_->isWordWrap_);
}

void FQTermFrame::noEsc() {
  escapeString_ = "";
  actionNoEscape_->setChecked(true);
}

void FQTermFrame::escEsc() {
  escapeString_ = "^[^[[";
  actionEscEscape_->setChecked(true);
}

void FQTermFrame::uEsc() {
  escapeString_ = "^u[";
  actionUEscape_->setChecked(true);
}

void FQTermFrame::customEsc() {
  bool ok;
  QString strEsc = QInputDialog::getText(this, "define escape",
                                         "scape string *[", QLineEdit::Normal,
                                         escapeString_, &ok);
  if (ok) {
    escapeString_ = strEsc;
    actionCustomEscape_->setChecked(true);
  }
}

void FQTermFrame::gbkCodec() {
  clipboardEncodingID_ = 0;
  actionGBK_->setChecked(true);
}

void FQTermFrame::big5Codec() {
  clipboardEncodingID_ = 1;
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

  if (preference_.openAntiAlias_) {
    font.setStyleStrategy(QFont::PreferAntialias);
  }

  if (ok == true) {
   qApp->setFont(font);
   //refresh style sheet
   if (preference_.useStyleSheet_) {
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
  isBossColor_ = !isBossColor_;

  emit bossColor();

  actionBossColor_->setChecked(isBossColor_);
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
  termScrollBarPosition_ = 0;

  emit updateScroll();
}

void FQTermFrame::leftScroll() {
  termScrollBarPosition_ = 1;

  emit updateScroll();
}

void FQTermFrame::rightScroll() {
  termScrollBarPosition_ = 2;

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
  isStatusBarShown_ = !isStatusBarShown_;
  actionStatus_->setChecked(isStatusBarShown_);
  emit updateStatusBar(isStatusBarShown_);
}

void FQTermFrame::setting() {
  windowManager_->activeWindow()->setting();
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
  bool styleSheetUsed = preference_.useStyleSheet_;

  if (pref.exec() == 1) {
    //TODO: refactor
    loadPref(config_);
    setUseDock(preference_.openMinimizeToTray_);
    if (preference_.useStyleSheet_) {
      loadStyleSheetFromFile(preference_.styleSheetFile_);
    }
    else if (styleSheetUsed) {
      clearStyleSheet();
    }
  }
}

void FQTermFrame::keySetup() {
  keyDialog keyDlg(config_, this);
  if (keyDlg.exec() == 1) {
    updateToolBar();
  }
}


void FQTermFrame::antiIdle() {
  windowManager_->activeWindow()->toggleAntiIdle();
  actionAntiIdle_->setChecked(
      windowManager_->activeWindow()->session_->isAntiIdle_);
}

void FQTermFrame::autoReply() {
  windowManager_->activeWindow()->toggleAutoReply();
  actionAutoReply_->setChecked(
      windowManager_->activeWindow()->session_->isAutoReply_);
}

void FQTermFrame::viewMessages() {
  windowManager_->activeWindow()->viewMessages();
}

void FQTermFrame::enableMouse() {
  windowManager_->activeWindow()->session_->isMouseSupported_
      = !windowManager_->activeWindow()->session_->isMouseSupported_;
  actionMouse_->setChecked(
      windowManager_->activeWindow()->session_->isMouseSupported_);
}

void FQTermFrame::viewImages(QString filename, bool raiseViewer) {
  if (filename.isEmpty()) {
    filename = preference_.zmodemPoolDir_;
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
  viewImages(preference_.zmodemPoolDir_, true);
}

void FQTermFrame::beep() {
  windowManager_->activeWindow()->session_->isBeep_ =
      !windowManager_->activeWindow()->session_->isBeep_;
  actionBeep_->setChecked(windowManager_->activeWindow()->session_->isBeep_);
}

void FQTermFrame::reconnect() {
  FQTermWindow * qtw = windowManager_->activeWindow();
  if (qtw){
    qtw->session_->isAutoReconnect_ =
        !windowManager_->activeWindow()->session_->isAutoReconnect_;
  }
}

void FQTermFrame::runScript() {
  windowManager_->activeWindow()->runScript();
}

void FQTermFrame::stopScript() {
  windowManager_->activeWindow()->stopScript();
}

bool FQTermFrame::event(QEvent *e) {
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
    windowManager_->activeWindow()->externInput(strTmp.mid(1).toLatin1());
  } else if (strTmp[0] == '1') { // script
    windowManager_->activeWindow()->runScript(strTmp.mid(1).toLatin1());
  } else if (strTmp[0] == '2') { // program
    system((strTmp.mid(1) + " &").toLocal8Bit());
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
  QAction* dummyAction = new QAction(QPixmap(getPath(RESOURCE) + "pic/change_fonts.png"), tr("Set Terminal Fonts"), fontButton_);
  fontButton_->setDefaultAction(dummyAction);
  toolBarMdiConnectTools_->addWidget(fontButton_);
  fontButton_->setMenu(menuFont_);
  fontButton_->setPopupMode(QToolButton::InstantPopup);

  toolBarMdiConnectTools_->addAction(actionColor_);
  actionColor_->setIcon(QPixmap(getPath(RESOURCE) + "pic/ansi_color.png"));
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
  menuFile_->addAction(tr("&Exit"), this, SLOT(exitQTerm()));

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
                                 tr("&Color"), this, SLOT(setColor()));
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
      tr("&Setting for currrent session"),
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
  help->addAction(tr("About &FQTerm"), this, SLOT(aboutQTerm()), tr("F1"));

  help->addAction(tr("FQTerm's &Homepage"), this, SLOT(homepage()));
}

void FQTermFrame::updateMenuToolBar() {
  FQTermWindow *window = windowManager_->activeWindow();

  if (window == NULL) {
    return ;
  }

  // update menu
  actionDisconnect_->setEnabled(window->isConnected());

  actionColorCopy_->setChecked(window->session_->param_.isColorCopy_);
  actionRectangleSelect_->setChecked(window->session_->param_.isRectSelect_);
  actionAutoCopy_->setChecked(window->session_->param_.isAutoCopy_);
  actionWordWrap_->setChecked(window->session_->isWordWrap_);

  actionFullScreen_->setChecked(isFullScreen_);

  actionAntiIdle_->setChecked(window->session_->isAntiIdle_);
  actionAutoReply_->setChecked(window->session_->isAutoReply_);
  actionBeep_->setChecked(window->session_->isBeep_);
  actionMouse_->setChecked(window->session_->isMouseSupported_);
}

void FQTermFrame::enableMenuToolBar(bool enable) {
  actionDisconnect_->setEnabled(enable);

  actionCopy_->setEnabled(enable);
  actionPaste_->setEnabled(enable);
  actionColorCopy_->setEnabled(enable);
  actionRectangleSelect_->setEnabled(enable);
  actionAutoCopy_->setEnabled(enable);
  actionWordWrap_->setEnabled(enable);

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

void FQTermFrame::updateToolBar() {
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

  //load toolbar setting
  strTmp = config_->getItemValue("global", "toolbarstate");
  if (!strTmp.isEmpty())
  {
    restoreState(QByteArray::fromHex(strTmp.toAscii()));
  } else {
    addToolBar(Qt::TopToolBarArea, toolBarMdiConnectTools_);
    insertToolBar(toolBarMdiConnectTools_,toolBarSetupKeys_);
    insertToolBar(toolBarSetupKeys_, toolBarMdiTools_);
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
      delete menuTray_;
      menuTray_ = 0;
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
  menuTray_->addAction(tr("About"), this, SLOT(aboutQTerm()));
  menuTray_->addAction(tr("Exit"), this, SLOT(exitQTerm()));
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

void FQTermFrame::buzz() {
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

//---------------------------
//record subwindows' size 
//---------------------------
void
FQTermFrame::subWindowResized(FQTermWindow *termWindow) {
  int n = windowManager_->termWindowList().indexOf(termWindow);
  if (n  == -1) {
    return;
  }
  QMdiSubWindow *subWindow = mdiArea_->subWindowList().at(n);
  if (!subWindow) {
    return;
  }
  Qt::WindowFlags wfs = subWindow->windowFlags();
  if (!(subWindowMax_ = subWindow->isMaximized())){
    subWindowSize_ = subWindow->frameSize();
    if (!(wfs & Qt::WindowSystemMenuHint)) {
      subWindow->setWindowFlags(wfs | Qt::WindowSystemMenuHint);
    }
  }
  else {
    if (wfs & Qt::WindowSystemMenuHint) {
      subWindow->setWindowFlags(wfs & ~Qt::WindowSystemMenuHint);
    }
  }

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
  updateToolBar();
  if (!mdiArea_->activeSubWindow()) {
    enableMenuToolBar(false);
  }
  if (preference_.useStyleSheet_) {
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
  installTranslator(lang);
}

void FQTermFrame::cascade() {
  QSize oldSize = subWindowSize_;
  mdiArea_->cascadeSubWindows();
  foreach(QMdiSubWindow* subWindow, mdiArea_->subWindowList()) {
    if (subWindow) {
      subWindow->resize(oldSize);
    }
  }
}

void FQTermFrame::tile() {
  mdiArea_->tileSubWindows();
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
  switch(termScrollBarPosition_) {
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
