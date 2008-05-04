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

#ifndef FQTERM_DECODE_H
#define FQTERM_DECODE_H

#include <QObject>

namespace FQTerm {

class FQTermDecode;
class FQTermBuffer;
class FQTermTelnet;

// this for FSM
typedef void(FQTermDecode:: *StateFunc)();

struct StateOption {
  int byte; // char value to look for; -1==end/default
  StateFunc action;
  StateOption *nextState;
};

class FQTermDecode: public QObject {
  Q_OBJECT;
 public:
  FQTermDecode(FQTermBuffer *, FQTermTelnet *, int server_encoding);
  ~FQTermDecode();

  // translate data from telnet socket to our own buffer
  // return how many bytes are processed.
  int decode(const char *cstr, int length);

  bool bellReceive() {
    return isBell_;
  }

  void setServerEncoding(int encoding) {
    server_encoding_ = encoding;
  }

  //signals:
  //	void decodeFinished();
 signals:
  void mouseMode(bool);

 private:
  // escape sequence actions
  // you'd better see FSM structure array in FQTermDecode.cpp

  void setAttr();
  void setMargins();

  // char screen functions
  void deleteStr();
  void deleteLine();
  void insertStr();
  void insertLine();
  void eraseStr();
  void eraseLine();
  void eraseScreen();

  // cursor functions
  void saveCursor();
  void restoreCursor();
  void cursorPosition();

  // Move cursor, stop at boundary.
  void cursorLeft();
  void cursorDown();
  void cursorRight();
  void cursorUp();

  // Move cursor, scroll at boundary.
  void moveCursorUp();
  void moveCursorDown();
  void nextLine();

  void selectG0A();
  void selectG0B();
  void selectG00();
  void selectG01();
  void selectG02();

  void selectG1A();
  void selectG1B();
  void selectG10();
  void selectG11();
  void selectG12();

  // other escape sequence actions
  void normalInput();

  // action parameters
  void clearParam();
  void paramDigit();
  void nextParam();

  // non-printing characters
  void cr(), lf(), ff(), bell(), tab(), bs(), del(), g0(), g1(), enq();

  void addTabStop();
  void clearTabStop();
  
  void setMode();
  void resetMode();
  void setDecPrivateMode();
  void resetDecPrivateMode();
  void setNumericKeypad();
  void setAppKeypad();

  void saveMode();
  void restoreMode();

  // video alignment test - fill screen with E's.
  void fillScreen();

  void test();

 private:

  bool isBell_;
  // short currentAttr_, defaultAttr_;
  unsigned char current_color_, default_color_;
  unsigned char current_attr_, default_attr_;

  // ********** ansi decoder states ****************
  StateOption *current_state_;

  struct VT100StateMachine {
    static StateOption normal_state_[], esc_state_[], bracket_state_[];
    static StateOption private_state_[], title_state_[], sharp_state_[];
    static StateOption select_g0_charset_state_[], select_g1_charset_state_[];
  };

  static const char *getStateName(const StateOption *state);

  void logParam() const;
  

  bool interrupt_decode_;

  // ********** decoder		*****************
  const char *inputData_;
  int inputLength_, dataIndex_;

  int paramIndex_, param_[30];
  bool isParamAvailable_;

  bool savedMode_[30];
  bool currentMode_[30];

  FQTermTelnet *telnet_;
  FQTermBuffer *termBuffer_;
  int server_encoding_;

  QString bbs2unicode(const QByteArray &text);
};

}  // namespace FQTerm

#endif  // FQTERM_DECODE_H
