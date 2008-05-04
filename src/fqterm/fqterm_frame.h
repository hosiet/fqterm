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

#ifndef FQTERM_FRAME_H
#define FQTERM_FRAME_H

#include <QMainWindow>
#include <QSystemTrayIcon>

class QWidget;
class QLineEdit;
class QToolButton;
class QMdiArea;
class QSignalMapper;
class QTabBar;
class QFontDialog;
class QActionGroup;
class QString;

namespace FQTerm {

class FQTermImage;
class FQTermParam;
class FQTermConfig;
class FQTermWndMgr;
class FQTermWindow;
class StatusBar;
class FQTermTimeLabel;
class TranslatorInstaller;

struct FQTermPref {
  int serverEncodingID_; //	0--GBK	1--BIG5
  int widthToWrapWord_;
  // bool bSmartWW;
  bool isWheelSupported_;
  bool openWarnOnClose_;
  bool openTabBlinking_;
  // bool bLogMsg;
  // QString strHttp;
  int openBeep_;
  QString beepSoundFileName_;
  int beepMethodID_;
  QString beepPlayerName_;
  bool openUrlCheck_;
  // bool bAutoCopy;
  bool openAntiAlias_;
  bool openMinimizeToTray_;
  bool needClearZmodemPoolOnClose_;
  bool useStyleSheet_;
  QString styleSheetFile_;
  QString zmodemDir_;
  QString zmodemPoolDir_;
  QString imageViewerName_;
};

class FQTermFrame: public QMainWindow {
  Q_OBJECT;
 public:
  FQTermFrame();
  ~FQTermFrame();

  void updateMenuToolBar();
  void enableMenuToolBar(bool);

  void popupFocusIn(FQTermWindow*);

  void viewImages(QString filename, bool raiseViewer);
  void buzz();
  void installTranslator(const QString& lang);
  FQTermConfig * config() const { return config_; }

  static const int translatedModule = 2;
  static const QString qmPrefix[translatedModule];
  static const QString qmPostfix;

 signals:
  void bossColor();
  void updateScroll();
  void updateStatusBar(bool);
  void changeLanguage();

 protected slots:
  
  bool event(QEvent *event);

  void viewImages();

  // Menu
  void recreateMenu();
  void addressBook();
  void quickLogin();
  void exitQTerm();

  void selectionChanged(int);
  void aboutQTerm();
  void langEnglish();
  void defaultSetting();
  void preference();
  void runScript();
  void stopScript();
  void homepage();

  void cascade();
  void tile();

  // Toolbar
  void keyClicked(int);

  void connectIt();
  void disconnect();
  void copy();
  void paste();
  void copyRect();
  void copyColor();
  void copyArticle();
  void autoCopy();
  void wordWrap();
  void noEsc();
  void escEsc();
  void uEsc();
  void customEsc();
  void gbkCodec();
  void big5Codec();
  void hideScroll();
  void leftScroll();
  void rightScroll();
  void showSwitchBar();
  void showStatusBar();
  void setFont();
  void setColor();
  void refreshScreen();
  void fullscreen();
  void bosscolor();
  void uiFont();
  void antiIdle();
  void autoReply();
  void setting();
  void viewMessages();
  void enableMouse();
  void beep();
  void reconnect();
  void keySetup();

  void scrollMenuAboutToShow();
  void themesMenuAboutToShow();
  void themesMenuActivated();
  void windowsMenuAboutToShow();
  void windowsMenuActivated(int);
  void connectMenuActivated();
  void popupConnectMenu();
  void connectMenuAboutToHide();
  void trayActived(QSystemTrayIcon::ActivationReason);
 
  //void trayClicked(const QPoint &, int);
  //void trayDoubleClicked();
  void trayHide();
  void trayShow();
  void buildTrayMenu();

  void switchWin(int);
  void paintEvent(QPaintEvent*);

  //record subwindows' size changes
  void subWindowResized(FQTermWindow *);

  void reloadConfig();
 
 public:
  QTabBar *tabBar_;
  FQTermWndMgr *windowManager_;
  FQTermPref preference_;

  bool isBossColor_;

  QString escapeString_;

  int clipboardEncodingID_; // 0--GBK 1--BIG5

  bool isStatusBarShown_;

  int termScrollBarPosition_; // 0--hide 1--LEFT 2--RIGHT

  QMdiArea *mdiArea_;
 private:

  // image viewer
  FQTermImage *image_;

  FQTermTimeLabel *labelTime_;

  QString theme_;
  //sub-window position & size
  bool subWindowMax_;
  QSize subWindowSize_;

  QActionGroup *escapeGroup;
  QActionGroup *codecGroup;
  QActionGroup *languageGroup;
  QMenu *menuWindows_;
  QMenu *menuThemes_;
  QMenu *scrollMenu_;
  QMenu *menuFont_;

  QMenu *menuFile_;
  //  QMenu *menuEscape_;
  QMenu *menuLanguage_;
  QMenu *menuConnect_;
  // 	File
  QAction *actionDisconnect_;
  QAction *actionQuickConnect_;
  // 	Edit
  QAction *actionCopy_;
  QAction *actionPaste_;
  QAction *actionColorCopy_; //used
  QAction *actionRectangleSelect_; //used
  QAction *actionAutoCopy_; //used
  QAction *actionWordWrap_; //used
  QAction *actionNoEscape_; //used
  QAction *actionEscEscape_; //used
  QAction *actionUEscape_; //used
  QAction *actionCustomEscape_; //used
  QAction *actionGBK_; //used
  QAction *actionBIG5_; //used
  QAction *actionHideScrollBar_; //used
  QAction *actionLeftScrollBar_; //used
  QAction *actionRightScrollBar_; //used
  QAction *actionEnglish_;
  QAction *actionStatus_;
  QAction *actionSwitch_; //used
  // 	View
  QAction *actionFullScreen_;
  QAction *actionBossColor_;
  QAction *actionAntiIdle_;
  QAction *actionAutoReply_;
  QAction *actionMouse_;
  QAction *actionBeep_;
  QAction *actionReconnect_;
  QAction *actionColor_;
  QAction *actionRefresh_;

  QAction *actionCurrentSession_;
  QAction *actionCopyArticle_;
  QAction *actionRunScript_;
  QAction *actionStopScript_;
  QAction *actionViewMessage_;
  QAction *actionViewImage_;

  QAction *actionNextWindow_;
  QAction *actionPrevWindow_;

  QSignalMapper* windowMapper_;

  FQTerm::StatusBar *statusBar_;

  QToolButton *connectButton_; // *disconnectButton,
  QToolButton *fontButton_;
  // 				*editRect, *editColor,
  // 				*specAnti, *specAuto, *specMouse, *specBeep, *specReconnect;

  QMenuBar *menuMain_;
  QToolBar *toolBarMdiConnectTools_;
  QToolBar *toolBarMdiTools_;
  QToolBar *toolBarSetupKeys_;

  bool isFullScreen_;
  bool isTabBarShown_;

  QSystemTrayIcon *tray;

  QMenu *menuTray_;

  QTranslator * translator[translatedModule];
  QList<TranslatorInstaller*> installerList_;

  FQTermConfig * config_;



  //function
  //FQTermWindow * newWindow( const FQTermParam& param, int index=-1 );

  void newWindow(const FQTermParam &param, int index = -1);

  void closeEvent(QCloseEvent*);
  void selectStyleMenu(int, int);
  void iniSetting();
  void loadPref(FQTermConfig*);

  void saveSetting();

  void addMainMenu();
  void addMainTool();

  void updateKeyToolBar();

  bool eventFilter(QObject *, QEvent*);

  QString valueToString(bool, int, int, bool, int);
  void insertThemeItem(QString);
  void setUseDock(bool);

  void initTranslator();
  void clearTranslator();
  void connector();
  void updateLanguageMenu();

  void loadStyleSheetFromFile(const QString qssFile);
  void refreshStyleSheet();
  void clearStyleSheet();
};

class TranslatorInstaller : public QObject
{
  Q_OBJECT;

public:
  TranslatorInstaller(const QString& language, FQTermFrame* frame);

  QString languageName();
  QString languageFormalName();
public slots:
  void installTranslator();

protected:
  QString language_;
  FQTermFrame* frame_;

  QString languageName_;
};

}  // namespace FQTerm

#endif  // FQTERM_FRAME_H
