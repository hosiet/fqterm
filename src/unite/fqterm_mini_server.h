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

#ifndef FQTERM_MINI_SERVER_H
#define FQTERM_MINI_SERVER_H

#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <map>
#include <vector>
#include <algorithm>
#include <QString>
namespace FQTerm {


class FQTermMiniServer : public QTcpServer {
  Q_OBJECT;
public:
  FQTermMiniServer( QObject * parent = 0 );
  ~FQTermMiniServer();
  void finalizeMiniServer();
protected:
  void incomingConnection(int socketDescriptor);
signals:
  void quit();
};


class FQTermMiniServerThread : public QThread{
protected:
  virtual void run();

};
//----------------------------------------------------------------------------------
// this for FSM
class FQTermUniteDecode;
typedef bool (FQTermUniteDecode:: *StateFunc)();

struct StateOption {
  int byte; // char value to look for; -1==end/default
  StateFunc action;
  StateOption *nextState;
};



class FQTermUniteDecode {
public:
  FQTermUniteDecode();
  virtual ~FQTermUniteDecode() {}
  //should return number of chars consumed
  virtual int processInput(const QByteArray& input);
protected:
  virtual bool keyReceived(int key) = 0;
  //tab = 7, backspace = 8, delete is *[3~, page up is 5~, page down is 6~, home is 1~, end is 4~, double esc is double esc, 
  //up is A, down is B, left is D, right is C.
  //F1 - F12 11~  12~  13~  14~  15~  17~  18~  19~  20~  21~  23~  24~  
  //static char* specialKeys[];
  enum SPECIALKEY{SPECIALKEYSTART = 0x10000, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    UP, DOWN, LEFT, RIGHT, DOUBLEESC, INSERT, HOME, END, PGUP, PGDOWN, DELETE};

  // Virtual Keys: move cursor
  bool cursorLeft();
  bool cursorDown();
  bool cursorRight();
  bool cursorUp();

  // other escape sequence actions
  bool normalInput();

  //double esc pressed
  bool doubleEsc();

  // action parameters
  bool clearParam();
  bool paramDigit();
  //function key map
  bool functionKey();
private:
  // ********** decoder states ****************
  StateOption *current_state_;
  int param_; //valid when > 0.
  const char* data_;
  int inputLength_;
  int currentByte_;
  QByteArray leftToDecode_;
  struct EscStateMachine {
    static StateOption normal_state_[], esc_state_[], bracket_state_[];
  };
};
//----------------------------------------------------------------------------------


class FQTermUniteSession;
class FQTermUniteState;
class FQTermUniteSessionContext : public QObject {
  Q_OBJECT;
public:
  FQTermUniteSessionContext(FQTermUniteSession* session);
  void write(const QByteArray& str);
  void processInput(const QByteArray& input);
  void quit();
  enum STATE{WELCOME = 0, EXITING = 1, READING = 2, HELP = 3};
  FQTermUniteState* setCurrentState(int state, bool init = true);

  int row() {return 24;}
  int column() {return 80;}
private:
  FQTermUniteSession* session_;
  FQTermUniteState* currentState_;
  QByteArray inputBuffer_;
  typedef std::map<int, FQTermUniteState*> StateTable;
  typedef std::map<int, FQTermUniteState*>::iterator StateTableIterator;
  StateTable stateTable_;
};

class FQTermUniteSession : public QThread {
  Q_OBJECT;
public:
  FQTermUniteSession(int socketDescriptor);
  ~FQTermUniteSession();
  void write(const QByteArray& output);

protected:
  virtual void run();
protected slots:
  void readyRead();
  void disconnected();
  void stateChanged(QAbstractSocket::SocketState socketState);
  void error(QAbstractSocket::SocketError socketError);
  void welcome();
private:
  QTcpSocket* socket_;
  FQTermUniteSessionContext* context_;

};

class FQTermUniteMenu {
public:
  FQTermUniteMenu() {current_ = 0;}
  virtual ~FQTermUniteMenu() {}
protected:
  int addEntry(char key);
  int findEntry(char key);
  void clearEntry();
  int currentEntry();
  int setCurrentEntry(char key);
  int setCurrentIndex(int index);

  int nextEntry();
  int prevEntry();
  virtual void clearPointer(int index) = 0;
  virtual void drawPointer(int index) = 0;
  virtual void enterEntry() = 0;
private:
  std::vector<char> entries_;
  int current_;
};

class FQTermUniteState : public QObject,
                         public FQTermUniteDecode {
  Q_OBJECT;
public:
  FQTermUniteState(FQTermUniteSessionContext* context);
  virtual ~FQTermUniteState() {}
  virtual void initialize() = 0;

  
protected:
  //util functions
  //cursor manipulations
  void moveCursorRelative(int dx, int dy);
  void moveCursorAbsolute(int x, int y);
  void moveCursorHome();
  //To scroll up, dy < 0, scroll down, dy > 0. 
  void scrollWindow(int dy);
  void moveCursorNextLine();
  void saveCursor();
  void restoreCursor();
  
  //attr control
  enum CHARATTR{DEFAULTATTR = -1, CLEARATTR = 0, BOLD = 1, LOWINTENSITY = 2, UNDERLINE = 4, BLINK = 5, REVERSECOLOR = 7, INVISIBLE = 8};
  void setAttr(int attr);
  enum CHARCOLOR{BLACK = 0, RED = 1, GREEN = 2, YELLOW = 3, BLUE = 4, PURPLE = 5, INDIGO = 6, WHITE = 7};
  void setForegroundColor(int color, bool highlight = true);
  void setBackgroundColor(int color);

  //clear lines/screen
  enum CLEARLINE{CURSORRIGHT = 0, CURSORLEFT = 1, WHOLELINE = 2};
  void clearLine(int part);
  enum CLEARSCREEN{CURSORDOWN = 0, CURSORUP = 1, WHOLESCREEN = 2};
  void clearScreen(int part);

  //window setting
  void setWindowRange(int startLine, int endLine);

  QByteArray escape();
  QByteArray cursorStr();
  void write(const QByteArray& output);
  void quit() {context_->quit();}
  FQTermUniteState* setNextState(int state, bool init = true) {return context_->setCurrentState(state, init);}
  int row() {return context_->row();}
  int column() {return context_->column();}


 friend struct cursorGuard;
 struct cursorGuard {
   cursorGuard(FQTermUniteState& state) : state_(state) {state_.saveCursor();}
   ~cursorGuard() {state_.restoreCursor();}
 private:
   FQTermUniteState& state_;
 };
private:
  FQTermUniteSessionContext* context_;
  int lastState_;
};


class FQTermWelcomeState : public FQTermUniteState, public FQTermUniteMenu {
  Q_OBJECT;
public:
  FQTermWelcomeState(FQTermUniteSessionContext* context);
  virtual void initialize();
protected:
 virtual bool keyReceived(int key);
private:
  virtual void clearPointer(int index);
  virtual void drawPointer(int index);
  virtual void enterEntry();
};

class FQTermExitState : public FQTermUniteState, public FQTermUniteMenu {
  Q_OBJECT;
public:
  FQTermExitState(FQTermUniteSessionContext* context);
  virtual void initialize();
protected:
  virtual bool keyReceived(int key);
private:
  virtual void clearPointer(int index);
  virtual void drawPointer(int index);
  void enterEntry();
};

class SplittedArticleBuffer {
public:
  SplittedArticleBuffer(const QByteArray& utf8Content, int column, int row = 24);
  ~SplittedArticleBuffer() {}
  int count() {return lines_.size();}
  QByteArray& retriveLine(size_t index) {return lines_[index];}
private:
  static int findNextLine(const QString& str, int start, int column);
  std::vector<QByteArray> lines_;
};

//TODO: content should be shared.
//TODO: buffer should be opt-ed.
class FQTermReadingState : public FQTermUniteState {
  Q_OBJECT;
public:
  FQTermReadingState(FQTermUniteSessionContext* context);
  ~FQTermReadingState();
  virtual void initialize();
  void setFile(const QString& filename);
  void setReturnState(int state) {returnState_ = state;}
protected:
  virtual bool keyReceived(int key);
private:
  void drawLines(int count, int yPos);
  void drawFooter();
  void adjustLineByDiff(int delta);
  SplittedArticleBuffer* saBuffer_;
  QByteArray content_;
  int currentStartLine_;
  int returnState_;
};


class FQTermHelpState : public FQTermUniteState, public FQTermUniteMenu {
  Q_OBJECT;
public:
  FQTermHelpState(FQTermUniteSessionContext* context);
  virtual void initialize();
protected:
  virtual bool keyReceived(int key);
private:
  virtual void clearPointer(int index);
  virtual void drawPointer(int index);
  virtual void enterEntry();
};


} //namespace FQTerm
#endif
