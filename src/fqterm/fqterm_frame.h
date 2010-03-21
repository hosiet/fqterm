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

#ifndef FQTERM_FRAME_H
#define FQTERM_FRAME_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenuBar>
#include <QApplication>
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

class IPLookupDialog;
class FQTermImage;
class FQTermParam;
class FQTermConfig;
class FQTermWndMgr;
class FQTermWindow;
class StatusBar;
class FQTermTimeLabel;
class TranslatorInstaller;
class FQTermMiniServerThread;
class FQTermShortcutHelper;

#ifdef HAVE_PYTHON
class FQTermPythonHelper;
#endif //HAVE_PYTHON

class FQTermApplication : public QApplication {
  Q_OBJECT;
public:
  FQTermApplication(int & argc, char ** argv) : QApplication(argc, argv) {}
protected:
  virtual void commitData(QSessionManager & manager) {emit saveData();}
signals:
  void saveData();
protected slots:
  void mainWindowDestroyed(QObject* obj) {quit();}
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
  void buzz(FQTermWindow* window = NULL);
  void installTranslator(const QString& lang);
  FQTermConfig * config() const { return config_; }

  static const int translatedModule = 4;
  static const QString qmPrefix[translatedModule];
  static const QString qmPostfix;

 signals:
  void bossColor();
  void updateScroll();
  void changeLanguage();
  void fontAntiAliasing(bool);

 protected slots:
  
  bool event(QEvent *event);

  void viewImages();
  bool clearUp();

  // Menu
  void recreateMenu();
  void addressBook();
  void quickLogin();
  void exitFQTerm();

  void aboutFQTerm();
  void langEnglish();
  void defaultSetting();
  void preference();
  void shortcutSetting();
  void runScript();
  void delayedRunScript(); // to avoid activate recursion guard
  void stopScript();
  void runPyScript();
  void homepage();

  void toggleAnsiColor();

  // Toolbar
  void keyClicked(int);

  void connectIt();
  void disconnect();
  void copy();
  void paste();
  void googleIt();
  void externalEditor();
  void fastPost();
  void copyRect();
  void copyColor();
  void copyArticle();
  void autoCopy();
  void wordWrap();
  void noEsc();
  void escEsc();
  void uEsc();
  void customEsc();
  void hideScroll();
  void leftScroll();
  void rightScroll();
  void showSwitchBar();
  void setFont();
  void setColor();
  void refreshScreen();
  void fullscreen();
  void bosscolor();
  void toggleServer(bool on);
  void uiFont();
  void antiIdle();
  void autoReply();
  void setting();
  void viewMessages();
  void enableMouse();
  void beep();
  void reconnect();
  void keySetup();
  void ipLookup();

  void scrollMenuAboutToShow();
  void themesMenuAboutToShow();
  void themesMenuActivated();
  void windowsMenuAboutToShow();
  void connectMenuActivated();
  void popupConnectMenu();
  void trayActived(QSystemTrayIcon::ActivationReason);
 
  //void trayClicked(const QPoint &, int);
  //void trayDoubleClicked();
  void trayHide();
  void trayShow();
  void buildTrayMenu();

  void reloadConfig();

  void saveSetting();
  void schemaUpdated();
  void editSchema();
 private:

  FQTermWndMgr *windowManager_;
  // image viewer
  FQTermImage *imageViewer_;

  FQTermTimeLabel *labelTime_;

  QString theme_;

  QActionGroup *escapeGroup;
  QActionGroup *languageGroup;
  QMenu *menuWindows_;
  QMenu *menuThemes_;
  QMenu *scrollMenu_;
  QMenu *menuFont_;
  QMenu *menuFile_;
  QMenu *menuLanguage_;
  QMenu *menuConnect_;
  QSignalMapper* windowMapper_;

  FQTerm::StatusBar *statusBar_;

  QToolButton *serverButton_;
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

  QMenu *trayMenu_;

  QTranslator * translator[translatedModule];
  QList<TranslatorInstaller*> installerList_;

  FQTermConfig * config_;
  FQTermShortcutHelper * shortcutHelper_;
  QAction* getAction(int shortcut);
  FQTermMiniServerThread* serverThread_;
  IPLookupDialog* ipLookupDialog_;

private:
  void newWindow(const FQTermParam &param, int index = -1);

  void closeEvent(QCloseEvent*);
  void selectStyleMenu(int, int);
  void iniSetting();
  void loadPref();

  void addMainMenu();
  void addMainTool();

  void updateKeyToolBar();

  void loadToolBarPosition();

  bool eventFilter(QObject *, QEvent*);

  void insertThemeItem(const QString&);
  void setUseDock(bool);

  void initAdditionalActions();

  void initTranslator();
  void clearTranslator();
  void connector();
  void updateLanguageMenu();

  void loadStyleSheetFromFile(const QString qssFile);
  void refreshStyleSheet();
  void clearStyleSheet();

#ifdef HAVE_PYTHON
public:
  FQTermPythonHelper* getPythonHelper() {
    return pythonHelper_;
  }

  //protected slots:
private:
  FQTermPythonHelper* pythonHelper_;
#endif //HAVE_PYTHON
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
