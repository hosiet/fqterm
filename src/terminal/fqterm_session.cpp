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

#include <stdio.h>
#include <ctype.h>

#include <algorithm>
#include <vector>
#include <list>

#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <QString>
#include <QTimer>
#include <QMutex>
#include <QRegExp>
#include <QtAlgorithms>
#include <QChar>
#include <QPair>

#include "fqterm.h"
#include "common.h"
#include "fqterm_trace.h"
#include "fqterm_session.h"
#include "fqterm_buffer.h"
#include "fqterm_text_line.h"
#include "fqterm_telnet.h"
#include "fqterm_decode.h"
#include "fqterm_zmodem.h"

namespace FQTerm {

const QString  FQTermSession::endOfUrl[] = {
  "ac","ad","ae","af","ag","ai","al","am","an","ao","aq","ar","as","at",
  "au","aw","az","ba","bb","bd","be","bf","bg","bh","bi","bj","bm","bn","bo","br",
  "bs","bt","bv","bw","by","bz","ca","cc","cd","cf","cg","ch","ci","ck","cl","cm",
  "cn","co","uk","com","cr","cs","cu","cv","cx","cy","cz","de","dj","dk","dm",
  "do","dz","ec","edu","ee","eg","eh","er","es","et","fi","fj","fk","fm","fo",
  "fr","ga","gd","ge","gf","gg","gh","gi","gl","gm","gn","gov","gp","gq","aero",
  "asia","biz","coop","eu","info","museum","name","pro","travel","gr","gs","gt",
  "gu","gw","gy","hk","hm","hn","hr","ht","hu","id","ie","il","im","in","int",
  "io","iq","ir","is","it","je","jm","jo","jp","ke","kg","kh","ki","km","kn","kp",
  "kr","kw","ky","kz","la","lb","lc","li","lk","lr","ls","lt","lu","lv","ly","ma",
  "mc","md","mg","mh","mil","mk","ml","mm","mn","mo","mp","mq","mr","ms","mt",
  "mu","mv","mw","mx","my","mz","na","nc","ne","net","nf","ng","ni","nl","no",
  "np","nr","nt","nu","nz","om","org","pa","pe","pf","pg","ph","pk","pl","pm",
  "pn","pr","ps","pt","pw","py","qa","re","ro","ru","rw","sa","sb","sc","sd","se",
  "sg","sh","si","sj","sk","sl","sm","sn","so","sr","sv","st","sy","sz","tc","td",
  "tf","tg","th","tj","tk","tm","tn","to","tp","tr","tt","tv","tw","tz","ua","ug",
  "uk","um","us","uy","uz","va","vc","ve","vg","vi","vn","vu","wf","ws","ye","yt",
  "yu","za","zm","zw"
};

FQTermSession::FQTermSession(FQTermConfig *config, FQTermParam param, bool isBeep,
                             int serverEncodingID, const QString &zmodemDir) {
  param_ = param;
  termBuffer_ = new FQTermBuffer(param_.numColumns_,
                                 param_.numRows_,
                                 param_.numScrollLines_,
                                 param_.hostType_ == 0);

  if (param.protocolType_ == 0) {
    telnet_ = new FQTermTelnet(param_.virtualTermType_.toLatin1(),
                               param_.numRows_, param_.numColumns_, false);
  } else {
#if defined(_NO_SSH_COMPILED)
    QMessageBox::warning(this, "sorry",
                         "SSH support is not compiled, "
                         "FQTerm can only use Telnet!");
    telnet_ = new FQTermTelnet(param_.virtualTermType_.toUtf8(),
                               param_.numRows_, param_.numColumns_, false);
#else
    telnet_ = new FQTermTelnet(param_.virtualTermType_.toUtf8(),
                               param_.numRows_, param_.numColumns_, true,
                               param_.sshUserName_.toUtf8(),
                               param_.sshPassword_.toUtf8());
#endif
  }

  zmodem_ = new FQTermZmodem(config, telnet_, param.protocolType_, zmodemDir);
  decoder_ = new FQTermDecode(termBuffer_, telnet_, param.serverEncodingID_);

  //  isColorCopy_ = param.isColorCopy_;
  //  isRectangleCopy_ = param.isRectSelect_;
  //  isAutoCopy_ = param.isAutoCopy_;
  isWordWrap_ = false;
  isAntiIdle_ = true;
  isAutoReply_ = param_.isAutoReply_;
  isBeep_ = isBeep;
  isMouseSupported_ = true;
  isAutoReconnect_ = param_.isAutoReconnect_;
  isConnected_ = false;
#ifndef _NO_SSH_COMPILED
  if (param.protocolType_ != 0) {
  	isLogining_ = true;
  } else {
    isLogining_ = false;
  }
#else
  isLogining_ = false;
#endif
  isIdling_ = false;
  isSendingMessage_ = false;
  isMouseX11_ = false;

  serverEncodingID_ = serverEncodingID;

  idleTimer_ = new QTimer;
  autoReplyTimer_ = new QTimer;
  reconnectTimer_ = new QTimer;

  acThread_ = new ArticleCopyThread(*this, waitCondition_);

  FQ_VERIFY(connect(reconnectTimer_, SIGNAL(timeout()),
                    this, SLOT(reconnect())));
  FQ_VERIFY(connect(decoder_, SIGNAL(mouseMode(bool)),
                    this, SLOT(setMouseMode(bool))));
  FQ_VERIFY(connect(telnet_, SIGNAL(readyRead(int, int)),
                    this, SLOT(readReady(int, int))));
  FQ_VERIFY(connect(telnet_, SIGNAL(TelnetState(int)),
                    this, SLOT(changeTelnetState(int))));
  FQ_VERIFY(connect(telnet_, SIGNAL(errorMessage(const char *)),
                    this, SIGNAL(errorMessage(const char *))));
  FQ_VERIFY(connect(telnet_, SIGNAL(requestUserPwd(QString*, QString*, bool*)),
                    this, SIGNAL(requestUserPwd(QString*, QString*, bool*))));

  FQ_VERIFY(connect(termBuffer_, SIGNAL(termSizeChanged(int, int)),
                    telnet_, SLOT(windowSizeChanged(int, int))));

  FQ_VERIFY(connect(zmodem_, SIGNAL(ZmodemState(int, int, const char *)),
                    this, SIGNAL(zmodemStateChanged(int, int, const char *))));

  FQ_VERIFY(connect(idleTimer_, SIGNAL(timeout()), this, SLOT(onIdle())));
  FQ_VERIFY(connect(autoReplyTimer_, SIGNAL(timeout()),
                    this, SLOT(onAutoReply())));

  FQ_VERIFY(connect(acThread_, SIGNAL(articleCopied(int, const QString)),
                    this, SIGNAL(articleCopied(int, const QString))));

  setAntiIdle(isAntiIdle_);
}

FQTermSession::~FQTermSession() {
  delete idleTimer_;
  delete autoReplyTimer_;
  delete reconnectTimer_;
  delete acThread_;
  delete termBuffer_;
  delete telnet_;
  delete zmodem_;
  delete decoder_;
}

const QString FQTermSession::protocolPrefix[] = {
  "http://", "https://", "mms://", "rstp://", "ftp://", "mailto:", "telnet://"
};

void FQTermSession::setScreenStart(int nStart) {
  screenStartLineNumber_ = nStart;
}

bool FQTermSession::setCursorPos(const QPoint &pt, QRect &rc) {
  QRect rectOld = getSelectRect();

  cursorPoint_ = pt;

  QRect rectNew = getSelectRect();

  rc = rectOld | rectNew;

  return rectOld != rectNew;
}

QString FQTermSession::getMessage() {
  const FQTermTextLine *line;
  LineColorInfo colorInfo;
  QString message;

  getLineColorInfo(termBuffer_->getTextLineInTerm(0), &colorInfo);
  if (!colorInfo.uniBackgroundColor) {
    return message;
  }

  int i = 1;
  termBuffer_->getTextLineInTerm(0)->getAllPlainText(message);

  line = termBuffer_->getTextLineInTerm(i);
  getLineColorInfo(line, &colorInfo);
  while (colorInfo.uniBackgroundColor && colorInfo.hasBackgroundColor) {
    message += "\n";
    line->getAllPlainText(message);
    i++;
    line = termBuffer_->getTextLineInTerm(i);
    getLineColorInfo(line, &colorInfo);
  }
  return message;
}


void FQTermSession::detectPageState() {
  //for smth type bbs.
  pageState_ = Undefined;

  const FQTermTextLine *line[4];
  LineColorInfo colorInfo[4];
  int lineIndex[4] = {0, 1, 2};
  lineIndex[3] = termBuffer_->getNumRows() - 1;
  for (int i = 0; i < 4; ++i) {
    line[i] = termBuffer_->getTextLineInTerm(lineIndex[i]);
    getLineColorInfo(line[i], colorInfo + i);
  }

  //TODO: Detect PageState in a clearer way.
  if (!colorInfo[0].hasBackgroundColor) {
    if (colorInfo[3].foregroundColorIndex.count() != 0 &&
        colorInfo[3].hasBackgroundColor){
        if (colorInfo[3].uniBackgroundColor) {
            QString text;
            line[3]->getAllPlainText(text);
            if (text[0] != L'\x3010') {
                pageState_ = Read;
              } else {
                  pageState_ = Edit;
                }
            } else if (colorInfo[3].backgroundColorIndex.count() == 2 &&
                  colorInfo[3].backgroundColorIndex.at(0) == 4 &&
                  colorInfo[3].backgroundColorIndex.at(1) == 0){
          pageState_ = Read;
      }
    }
    return;
  }

  if (colorInfo[0].backgroundColorIndex.at(0) != 4
      || !(colorInfo[3].hasBackgroundColor && colorInfo[3].backgroundColorIndex.at(0) == 4)) {
      if (!colorInfo[3].hasBackgroundColor &&
            colorInfo[0].foregroundColorIndex.count() == 4 &&
            colorInfo[0].foregroundColorIndex.at(1) == 4 &&
            colorInfo[0].foregroundColorIndex.at(2) == 7 &&
            colorInfo[0].foregroundColorIndex.at(3) == 4){
            pageState_ = TOP10;
          }
    return;
  }

  if (colorInfo[1].hasBackgroundColor && colorInfo[1].uniBackgroundColor) {
    pageState_ = Message;
    return;
  }

  if (colorInfo[0].uniBackgroundColor) {
    if (!colorInfo[2].hasBackgroundColor ||
        colorInfo[2].backgroundColorIndex.at(0) != 4 ||
        colorInfo[2].backgroundColorIndex.count() > 3) {
      pageState_ = Menu;
      return;
    }
  }

  if (!colorInfo[0].uniBackgroundColor &&
      colorInfo[0].backgroundColorIndex.at(0) == 4 &&
      !colorInfo[3].uniBackgroundColor) {
    //announce, elite root
    pageState_ = EliteArticleList;
    return;
  }

  if (colorInfo[2].hasBackgroundColor &&
      !colorInfo[2].uniBackgroundColor &&
      colorInfo[2].backgroundColorIndex.at(0) == 4 &&
      !colorInfo[3].uniBackgroundColor) {
    pageState_ = EliteArticleList;
    return;
  }

  if (colorInfo[0].foregroundColorIndex.indexOf(14) != -1) {
    pageState_ = ArticleList;
    return;
  }

  if (colorInfo[2].foregroundColorIndex.indexOf(7) != -1) {
    QString text;
    line[2]->getAllPlainText(text);
    if (text[0] == ' ') {
      pageState_ = BoardList;
    } else {
      pageState_ = MailMenu;
    }
  }

}

FQTermSession::CursorType FQTermSession::getCursorType(const QPoint &pt) {
  if (screenStartLineNumber_ !=
      (termBuffer_->getNumLines() - termBuffer_->getNumRows())) {
    return kNormal;
  }

  QRect rc = getSelectRect();
  CursorType nCursorType = kNormal;
  switch (pageState_) {
    case Undefined:  // not recognized
      nCursorType = kNormal;
      break;
    case Menu:
    case MailMenu:
      // menu
      if (pt.x() < 5) {
        // LEFT
        nCursorType = kLeft;
      } else if (rc.contains(pt)) {
        // HAND
        nCursorType = kRight;
      } else {
        nCursorType = kNormal;
      }
      break;
    case ArticleList:
    case BoardList:
    case EliteArticleList:
    case FriendMailList:
      // list
      if (pt.x() < 12) {
        // LEFT
        nCursorType = kLeft;
      } else if (pt.y() - screenStartLineNumber_ < 3) {
        // HOME
        nCursorType = kHome;
      } else if (pt.y() == termBuffer_->getNumLines() - 1) {
        // END
        nCursorType = kEnd;
      } else if (pt.x() > termBuffer_->getNumColumns() - 16 &&
                 pt.y() - screenStartLineNumber_ <= termBuffer_->getNumRows() / 2) {
        //PAGEUP
        nCursorType = kPageUp;
      } else if (pt.x() > termBuffer_->getNumColumns() - 16
                 && pt.y() - screenStartLineNumber_ >
                 termBuffer_->getNumRows() / 2) {
        // PAGEDOWN
        nCursorType = kPageDown;
      } else if (rc.contains(pt)) {
        // HAND
        nCursorType = kRight;
      } else {
        nCursorType = kNormal;
      }
      break;
    case Read:
      // read
      if (pt.x() < 12) {
        // LEFT
        nCursorType = kLeft;
      } else if (pt.x() > (termBuffer_->getNumColumns() - 16) &&
                 (pt.y() - screenStartLineNumber_)
                 <= termBuffer_->getNumRows() / 2) {
        // PAGEUP
        nCursorType = kPageUp;
      } else if (pt.x() > (termBuffer_->getNumColumns() - 16) &&
                 (pt.y() - screenStartLineNumber_)
                 > termBuffer_->getNumRows() / 2) {
        // PAGEDOWN
        nCursorType = kPageDown;
      } else {
        nCursorType = kNormal;
      }
      break;
    case Edit:
      // TODO: add action for kEdit state.
      break;
    case TOP10:
      if (pt.x() < 12) {
        nCursorType = kLeft;
        } else if (rc.contains(pt)){
          nCursorType = kRight;
          }
      break;
    default:
      FQ_TRACE("error", 2) << "Error, wrong PageState.";
      break;
  }

  return nCursorType;
}

bool FQTermSession::isSelected(int line) {
  // TODO: possible performance issue
  QRect rect = getSelectRect();

  // nothing selected
  if (rect.isNull()) {
    return false;
  }

  return line >= rect.bottom() && line <= rect.top();
}

bool FQTermSession::isSelected(const QPoint &pt) {
  // TODO: possible performance issue
  QRect rect = getSelectRect();

  // nothing selected
  if (rect.isNull()) {
    return false;
  }

  return rect.contains(pt);
}

FQTermSession::PageState FQTermSession::getPageState() {
  return pageState_;
}

char FQTermSession::getMenuChar() {
  return menuChar_;
}
//getSelectRect:
QRect FQTermSession::getSelectRect() {
  QRect rect(0, 0, 0, 0);

  // current screen scrolled
  if (screenStartLineNumber_ !=
      (termBuffer_->getNumLines() - termBuffer_->getNumRows())) {
    return rect;
  }

  const FQTermTextLine *line;
  int menuStartLine = 8;
  switch (pageState_) {
    case Undefined: break;
    case MailMenu:
      menuStartLine = 7;
    case Menu:
      if (cursorPoint_.y() - screenStartLineNumber_ >= menuStartLine &&
          cursorPoint_.x() > 5) {
        line = termBuffer_->getTextLineInBuffer(cursorPoint_.y());
        QString cstr;
        int cell_end = line->getCellEnd(qMin(cursorPoint_.x(),
                                             (int)line->getWidth()));
        line->getPlainText(0, cell_end, cstr);

        int base = cstr.lastIndexOf("   ");
        if (base == -1) {
          base = 0;
        }

        QRegExp reg("[a-zA-Z0-9][).\\]]");
        char indexChar = cstr.indexOf(reg, base);
        if (indexChar != -1) {
          menuChar_ = cstr.at(indexChar).toLatin1();

          QString strTmp = cstr.left(cstr.indexOf(reg, base));
          int nMenuStart = get_str_width((const UTF16 *)strTmp.data(),
                                         strTmp.size());
          if (cstr[indexChar - 1] == '(' || cstr[indexChar - 1] == '[') {
            nMenuStart--;
          }

          cstr.clear();
          line->getAllPlainText(cstr);


          reg = QRegExp("[^ ]");

          int nMenuBaseLength = 20;
          int cell_begin =
              line->getCellBegin(
                  qMin(nMenuStart + nMenuBaseLength, (int)line->getWidth() - 1));
          int char_begin = line->getCharBegin(cell_begin);
          //last [^ ] until char_begin
          strTmp = cstr.left(cstr.lastIndexOf(reg, char_begin) + 1);

          int nMenuLength =
              get_str_width((const UTF16 *)strTmp.data(), strTmp.size())
              - nMenuStart;
          if (nMenuLength == nMenuBaseLength + 1) {
            strTmp = cstr.left(cstr.indexOf(" ", char_begin) + 1);
            nMenuLength =
                get_str_width((const UTF16 *)strTmp.data(), strTmp.size())
                - nMenuStart;  //base length is not enough
          }

          if (cursorPoint_.x() >= nMenuStart && cursorPoint_.x() <=
              nMenuStart + nMenuLength) {
            rect.setX(nMenuStart);
            rect.setY(cursorPoint_.y());
            rect.setWidth(nMenuLength);
            rect.setHeight(1);
          }
        }
      }
      break;
    case ArticleList:
    case BoardList:
    case FriendMailList:
    case EliteArticleList:
      if ((cursorPoint_.y() - screenStartLineNumber_) >= 3
          && (cursorPoint_.y() - screenStartLineNumber_)
          < termBuffer_->getNumRows() -1
          && cursorPoint_.x() >= 12
          && cursorPoint_.x() <= termBuffer_->getNumColumns() - 16) {
        line = termBuffer_->getTextLineInBuffer(cursorPoint_.y());
        //QString str = line->getText();
        QString str;
        line->getAllPlainText(str);

        if (str.count(" ") != (int)str.length()) {
          rect.setX(0);
          rect.setY(cursorPoint_.y());
          rect.setWidth(line->getWidth());
          rect.setHeight(1);
        }
      }
      break;
    case Read:

      break;
    case Edit:
      break;
    case TOP10:
      {
        int ln = cursorPoint_.y() - screenStartLineNumber_;
        if (ln >= 3 && (ln & 1) && ln <= 21 &&
              cursorPoint_.x() >= 12 &&
              cursorPoint_.x() <= termBuffer_->getNumColumns() - 8){
            line = termBuffer_->getTextLineInBuffer(cursorPoint_.y());
  
            QString str;
            line->getAllPlainText(str);
    
            if (str.count(" ") != (int)str.length()) {
              if (ln == 21){
                  menuChar_ = '0';
                } else {
                    menuChar_ = ((ln - 1) >> 1) + '0';
                  }
    
                  rect.setX(12);
                rect.setY(cursorPoint_.y());
                rect.setWidth(line->getWidth() - 20);
                rect.setHeight(1);
              }
        }
      }
      break;
    default:
      break;
  }

  return rect;
}

bool FQTermSession::isIllChar(char ch) {
  static char illChars[] = ",;'\"()[]<>^";
  return ch > '~' || ch < '!' || strchr(illChars, ch) != NULL;
}

bool FQTermSession::isUrl(QRect &rcUrl, QRect &rcOld) {
  return checkUrl(rcUrl, rcOld, false);
}

bool FQTermSession::isIP(QRect &rcUrl, QRect &rcOld) {
  return checkUrl(rcUrl, rcOld, true);
}

QString FQTermSession::expandUrl(const QPoint& pt, QPair<int, int>& range)
{
  //  int at = pt.x();
  range.first = -1;
  range.second = -2;
  const FQTermTextLine *textLine = termBuffer_->getTextLineInBuffer(pt.y());
  if (textLine == NULL || pt.x() > (int)textLine->getWidth()) {
    return "";
  }

  QString text;
  textLine->getAllPlainText(text);

  int cell_begin = textLine->getCellBegin(pt.x());
  int at = textLine->getCharBegin(cell_begin);
  if (at >= text.length()) {
    return "";
  }

  int start;
  int end;
  for (start = at; start >= 0 && !isIllChar(text.at(start).toLatin1());
       start--) {
  }
  start++;
  for (end = at; end < text.length() && !isIllChar(text.at(end).toLatin1());
       end++) {
  }
  range.first = get_str_width((const UTF16 *)text.data(), start);
  range.second = get_str_width((const UTF16 *)text.data(), end);
  return text.mid(start, end - start);
}


bool FQTermSession::checkUrl(QRect &rcUrl, QRect &rcOld, bool checkIP) {



  QPoint pt = cursorPoint_;
  int at = pt.x();
  rcOld = urlRect_;

  if (at > urlRect_.left() && at < urlRect_.right() && urlRect_.y() == pt.y()) {
    if ( (checkIP && !ip_.isEmpty()) ||
         (!checkIP && !url_.isEmpty()) ) {
      rcUrl = urlRect_;
      return true;
    }

  }

  QPoint urlStartPoint;
  QPoint urlEndPoint;
  urlStartPoint_ = QPoint();
  urlEndPoint_ = QPoint();
  urlRect_ = QRect(0, 0, -1, -1);
  if (!checkIP) {
    url_.clear();
  } else {
    ip_.clear();
  }

  QString urlText;
  //phase 1: find all consecutive legal chars
  QPair<int, int> range;
  urlText = expandUrl(pt, range);
  if (range.first < 0 || range.first >= range.second) {
    return false;
  }
  

  QRect urlRect = QRect(range.first, pt.y(), range.second - range.first, 1);
  urlStartPoint = QPoint(range.first, pt.y());
  urlEndPoint = QPoint(range.second, pt.y());

  if (range.second == termBuffer_->getNumColumns()) {
    for (int i = pt.y() + 1; i < termBuffer_->getNumLines(); ++i) {
      QString lineText = expandUrl(QPoint(0, i), range);
      if (range.first == 0) {
        urlText += lineText;
        urlEndPoint.setY(urlEndPoint.y() + 1);
        urlEndPoint.setX(range.second);
        if (range.second == termBuffer_->getNumColumns()) {
          continue;
        }
      }
      break;
    }
  }

  //phase 2: find protocol prefix for url
  //if none, prefix to add is set to "mailto:" if '@' is found
  //otherwise, the prefix is "http://" by default.
  QString prefixToAdd;
  int protocolIndex;
  for (protocolIndex = 0; protocolIndex < ProtocolSupported; ++protocolIndex) {
    int begin = urlText.indexOf(protocolPrefix[protocolIndex],
                                Qt::CaseInsensitive);
    if (begin != -1 && begin <= at) {
      prefixToAdd = protocolPrefix[protocolIndex];
      urlText = urlText.mid(begin + protocolPrefix[protocolIndex].length());
      urlStartPoint.setX(urlStartPoint.x() + begin);
      break;
    }
  }
  if (protocolIndex == ProtocolSupported) {
    int begin = urlText.indexOf("://");
    if (begin != -1) {
      return false;
    }
    if (urlText.indexOf('@') != -1) {
      prefixToAdd = protocolPrefix[Mailto];
    } else {
      prefixToAdd = protocolPrefix[Http];
    }
  }

  //no point or duplicated points
  if (urlText.indexOf("..") != -1 || urlText.indexOf('.') == -1) {
    return false;
  }

  //phase 3: (for check ip) if there is a ':' before ip, there must be a '@'
  int host_begin = qMax(urlText.lastIndexOf('@') + 1, 0);
  int host_end = urlText.indexOf(':', host_begin);
  if (host_end == -1) {
    host_end = urlText.indexOf('/', host_begin);
  }
  if (host_end == -1) {
    host_end = urlText.length();
  }
  if (host_begin >= host_end) {
    return false;
  }
  if (checkIP && urlText[host_end - 1] == '*') {
    urlText[host_end - 1] = '1';
  }

  for (int index = 0; index < urlText.length() && urlText.at(index) != '/';
       index++) {
    QChar cur(urlText.at(index));
    if (!cur.isLetterOrNumber() && !QString("-_~:@.").contains(cur)) {
      return false;
    }
  }

  QString newIP;
  newIP = urlText.mid(host_begin, host_end - host_begin);
  QStringList ipv4 = newIP.split(QLatin1String("."));
  bool isIPv4 = true;
  if (ipv4.count() != 4){
    isIPv4 = false;
  }
  foreach(QString domain, ipv4) {
    bool ok;
    int num = domain.toInt(&ok);
    //won't tolerant spaces.
    if (!ok || num < 0 || num > 255 || domain.length() > 3) {
      isIPv4 = false;
      break;
    }
  }

  if (!checkIP){
    QString lastName = ipv4.back();

    int lastCharIndex = lastName.lastIndexOf(QRegExp("[a-zA-Z]"));
    if (lastCharIndex == -1) {
      if (!isIPv4) {
        return false;
      } else if (urlText == newIP) {
        return false;
      }
    } else {
      const QString *begin = endOfUrl;
      const QString *end = endOfUrl + sizeof(endOfUrl) / sizeof(QString *);
      if (qFind(begin, end, lastName) == end) {
        return false;
      }
    }
    url_ = prefixToAdd + urlText;
  } else {
    if (!isIPv4) {
      return false;
    }
    ip_ = newIP;
  }

  urlRect_ = urlRect;
  rcUrl = urlRect;
  urlStartPoint_ = urlStartPoint;
  urlEndPoint_ = urlEndPoint;
  return true;
}

QString FQTermSession::getUrl() {
  return url_;
}

QString FQTermSession::getIP() {
  return ip_;
}

bool FQTermSession::isPageComplete() {
  return termBuffer_->getCaretRow() == (termBuffer_->getNumRows() - 1)
	  && termBuffer_->getCaretColumn() == (termBuffer_->getNumColumns() - 1);
}

bool FQTermSession::isAntiIdle() {
  return isAntiIdle_;
}

void FQTermSession::setAntiIdle(bool antiIdle) {
  isAntiIdle_ = antiIdle;

  // disabled
  if (!isAntiIdle_ && idleTimer_->isActive()) {
    idleTimer_->stop();
  }

  // enabled
  if (isAntiIdle_) {
    if (idleTimer_->isActive()) {
      idleTimer_->stop();
    }
    idleTimer_->start(param_.maxIdleSeconds_ *1000);
  }
}

void FQTermSession::autoReplyMessage() {
  if (autoReplyTimer_->isActive()) {
    autoReplyTimer_->stop();
  }
  if (pageState_ != Message) {
    return;
  }

  QByteArray cstrTmp = param_.replyKeyCombination_.toLocal8Bit();
  QByteArray cstr = parseString(cstrTmp.isEmpty() ? QByteArray("^Z"): cstrTmp);
  //cstr += m_param.m_strAutoReply.toLocal8Bit();
  if (param_.serverEncodingID_ == 0) {
    cstr += U2G(param_.autoReplyMessage_);
  } else {
    cstr += U2B(param_.autoReplyMessage_);
  }

  cstr += '\n';
  telnet_->write(cstr, cstr.length());

  emit messageAutoReplied();
}

QByteArray FQTermSession::parseString(const QByteArray &cstr, int *len) {
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

void FQTermSession::reconnect() {
  if (!isConnected_) {
    telnet_->connectHost(param_.hostAddress_, param_.port_);
  }
}

void FQTermSession::reconnectProcess() {
  static int retry = 0;
  if (retry < param_.retryTimes_ || param_.retryTimes_ == -1) {
    if (param_.reconnectInterval_ <= 0) {
      reconnect();
    } else {
      reconnectTimer_->start(param_.reconnectInterval_ *1000);
    }
    retry++;
  }
}

void FQTermSession::setMouseMode(bool on) {
  isMouseX11_ = on;
}

void FQTermSession::doAutoLogin() {
  if (!param_.preLoginCommand_.isEmpty()) {
    QByteArray temp = parseString(param_.preLoginCommand_.toLatin1());
    telnet_->write((const char*)(temp), temp.length());
  }
  if (!param_.userName_.isEmpty()) {
    QByteArray temp = param_.userName_.toLocal8Bit();
    telnet_->write((const char*)(temp), temp.length());
    char ch = CHAR_CR;
    telnet_->write(&ch, 1);
  }
  if (!param_.password_.isEmpty()) {
    QByteArray temp = param_.password_.toLocal8Bit();
    telnet_->write((const char*)(temp), temp.length());
    char ch = CHAR_CR;
    telnet_->write(&ch, 1);
  }

  // smth ignore continous input, so sleep 1 sec :)
#if defined(_OS_WIN32_) || defined(Q_OS_WIN32)
  Sleep(1);
#else
  sleep(1);
#endif

  if (!param_.postLoginCommand_.isEmpty()) {
    QByteArray temp = parseString(param_.postLoginCommand_.toLatin1());
    telnet_->write((const char*)(temp), temp.length());
  }
  isLogining_ = false;
}

//read slot
void FQTermSession::readReady(int size, int raw_size) {
  FQ_ASSERT(size > 0 || raw_size > 0);  // maybe raw_size is greater than 0.

  raw_data_.resize(raw_size);
  telnet_->read_raw(&raw_data_[0], raw_size);

  // read raw buffer
  int zmodem_consumed;
  zmodem_->ZmodemRcv((uchar*)&raw_data_[0], raw_data_.size(), &(zmodem_->info),
                     zmodem_consumed);

  if (zmodem_->transferstate == notransfer) {
    //decode
    if (size > 0) {
      int last_size = telnet_data_.size();
      telnet_data_.resize(last_size + size);
      telnet_->read(&telnet_data_[0] + last_size, size);

      int processed = decoder_->decode(&telnet_data_[0], telnet_data_.size());
      telnet_data_.erase(telnet_data_.begin(), telnet_data_.begin() + processed);
    }

    if (isLogining_) {
      //FIXME: why these codes check those non-sense?
      int n = termBuffer_->getCaretRow();
      for (int y = n - 5; y < n + 5; y++) {
        y = qMax(0, y);
        const FQTermTextLine *pTextLine = termBuffer_->getTextLineInTerm(y);
        if (pTextLine == NULL) {
          continue;
        }

        // QString str = pTextLine->getText();
        QString str;
        pTextLine->getAllPlainText(str);

        if (str.indexOf("guest") != -1 /*&& str.indexOf("new") != -1*/) {
          doAutoLogin();
          break;
        }
      }
    }
    // page complete when caret at the right corner
    // this works for most but not for all
    const FQTermTextLine *pTextLine =
        termBuffer_->getTextLineInTerm(termBuffer_->getNumRows() - 1);

    //QString strText = stripWhitespace(pTextLine->getText());
    // TODO_UTF16: fixme! performance issue!
    QString strText;
    pTextLine->getAllPlainText(strText);

    // TODO_UTF16: shall we disable trim?
    strText = strText.trimmed();

    if (termBuffer_->getCaretRow() == termBuffer_->getNumRows() - 1
        && termBuffer_->getCaretColumn() >= strText.length() - 1) {
      waitCondition_.wakeAll();
    }

    //QToolTip::remove(this, screen_->mapToRect(m_rcUrl));

    // message received
    // 03/19/2003. the caret posion removed as a message judgement
    // because smth.org changed
    if (decoder_->bellReceive()) {
      emit startAlert();
      emit bellReceived();

      if (isAutoReply()) {
#ifdef HAVE_PYTHON
        if (!pythonCallback("autoReply", Py_BuildValue("(l)", this))) {
#endif
          // TODO: save messages
          if (isIdling_) {
            autoReplyMessage();
          } else {
            autoReplyTimer_->start(param_.maxIdleSeconds_ *1000 / 2);
          }
#ifdef HAVE_PYTHON
        }
#endif
      }
    }

    // set page state
    detectPageState();

    emit sessionUpdated();

#ifdef HAVE_PYTHON
    // python
    pythonCallback("dataEvent", Py_BuildValue("(l)", this));
#endif
  }

  if (zmodem_->transferstate == transferstop) {
    zmodem_->transferstate = notransfer;
  }
}

QByteArray FQTermSession::stripWhitespace(const QByteArray &cstr) {
  QString cstrText = QString::fromLatin1(cstr);

#if (QT_VERSION>=300)
  int pos = cstrText.lastIndexOf(QRegExp("[\\S]"));
#else
  int pos = cstrText.lastIndexOf(QRegExp("[^\\s]"));
#endif

  if (pos == -1) {
    cstrText = "";
  } else {
    cstrText.truncate(pos + 1);
  }

  return cstrText.toLatin1();
}

/* 0-left 1-middle 2-right 3-relsase 4/5-wheel
 *
 */
//void FQTermWindow::sendMouseState( int num,
//    Qt::ButtonState btnstate, Qt::ButtonState keystate, const QPoint& pt )
void FQTermSession::sendMouseState(int num, Qt::KeyboardModifier btnstate,
                                   Qt::KeyboardModifier keystate, const QPoint &pt) {
  /*
    if(!m_bMouseX11)
    return;

    QPoint ptc = screen_->mapToChar(pt);

    if(btnstate&Qt::LeftButton)
    num = num==0?0:num;
    else if(btnstate&Qt::MidButton)
    num = num==0?1:num;
    else if(btnstate&Qt::RightButton)
    num = num==0?2:num;

    int state = 0;
    if(keystate&Qt::ShiftModifier)
    state |= 0x04;
    if(keystate&Qt::MetaModifier)
    state |= 0x08;
    if(keystate&Qt::ControlModifier)
    state |= 0x10;

    // normal buttons are passed as 0x20 + button,
    // mouse wheel (buttons 4,5) as 0x5c + button
    if(num>3) num += 0x3c;

    char mouse[10];
    sprintf(mouse, "\033[M%c%c%c",
    num+state+0x20,
    ptc.x()+1+0x20,
    ptc.y()+1+0x20);
    telnet_->write( mouse, strlen(mouse) );
  */
}

void FQTermSession::cancelZmodem() {
  zmodem_->zmodemCancel();
}

void FQTermSession::disconnect() {
  telnet_->close();
  finalizeConnection();
}

void FQTermSession::finalizeConnection() {
  if (isConnected_) {
    QString strMsg = "";
    strMsg += "\n\n\n\r\n\r";
    strMsg += "\x1b[17C\x1b[0m===========================================\n\r";
    strMsg +=
        "\x1b[17C Connection Closed, Press \x1b[1m\x1b[31;40mEnter\x1b[m\x1b[0m To Connect\n\r";
    strMsg += "\x1b[17C===========================================\n";
    decoder_->decode(strMsg.toLatin1(), strMsg.length());
  }
  isConnected_ = false;

  if (autoReplyTimer_->isActive()) {
    autoReplyTimer_->stop();
  }

  if (idleTimer_->isActive()) {
    idleTimer_->stop();
  }

  emit connectionClosed();
}

// telnet state slot
void FQTermSession::changeTelnetState(int state) {
  switch (state) {
    case TSRESOLVING:
      break;
    case TSHOSTFOUND:
      break;
    case TSHOSTNOTFOUND:
      finalizeConnection();
      break;
    case TSCONNECTING:
      break;
    case TSHOSTCONNECTED:
      isConnected_ = true;
      if (param_.isAutoLogin_) {
        isLogining_ = true;
      }
      break;
    case TSPROXYCONNECTED:
      break;
    case TSPROXYAUTH:
      break;
    case TSPROXYFAIL:
      disconnect();
      break;
    case TSREFUSED:
      finalizeConnection();
      break;
    case TSREADERROR:
      disconnect();
      break;
    case TSCLOSED:
      finalizeConnection();
      if (param_.isAutoReconnect_ && isAutoReconnect_) {
        reconnectProcess();
      }
      break;
    case TSCLOSEFINISH:
      finalizeConnection();
      break;
    case TSCONNECTVIAPROXY:
      break;
    case TSEGETHOSTBYNAME:
      finalizeConnection();
      break;
    case TSEINIWINSOCK:
      finalizeConnection();
      break;
    case TSERROR:
      disconnect();
      break;
    case TSPROXYERROR:
      disconnect();
      break;
    case TSWRITED:
      // restart the idle timer
      if (idleTimer_->isActive()) {
        idleTimer_->stop();
      }
      if (isAntiIdle_) {
        idleTimer_->start(param_.maxIdleSeconds_ *1000);
      }
      isIdling_ = false;
      break;
    default:
      break;
  }

  emit telnetStateChanged(state);
}

void FQTermSession::handleInput(const QString &text) {
  if (text.length() > 0) {
    QByteArray cstrTmp = unicode2bbs(text);
    telnet_->write(cstrTmp, cstrTmp.length());
  }
}

QByteArray FQTermSession::unicode2bbs(const QString &text) {
  QByteArray strTmp;

  if (serverEncodingID_ == 0) {
    strTmp = U2G(text);
    if (param_.serverEncodingID_ == 1) {
      char *str = encodingConverter_.G2B(strTmp, strTmp.length());
      strTmp = str;
      delete [] str;
    }
  } else {
    strTmp = U2B(text);
    if (param_.serverEncodingID_ == 0) {
      char *str = encodingConverter_.B2G(strTmp, strTmp.length());
      strTmp = str;
      delete [] str;
    }
  }

  return strTmp;
}

#ifdef HAVE_PYTHON
bool FQTermSession::pythonCallback(const QString &func, PyObject *pArgs) {
  if (!isPythonScriptLoaded_) {
    Py_DECREF(pArgs);
    return false;
  };

  bool done = false;
  // get the global lock
  PyEval_AcquireLock();
  // get a reference to the PyInterpreterState

  //Python thread references
  extern PyThreadState *mainThreadState;

  PyInterpreterState *mainInterpreterState = mainThreadState->interp;
  // create a thread state object for this thread
  PyThreadState *myThreadState = PyThreadState_New(mainInterpreterState);
  PyThreadState_Swap(myThreadState);

  PyObject *pF = PyString_FromString(func.toLatin1());
  PyObject *pFunc = PyDict_GetItem(pythonDict_, pF);
  Py_DECREF(pF);

  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pValue = PyObject_CallObject(pFunc, pArgs);
    Py_DECREF(pArgs);
    if (pValue != NULL) {
      done = true;
      Py_DECREF(pValue);
    } else {
      // TODO: fixme.
      // QMessageBox::warning(this, "Python script error", getException());
    }
  } else {
    PyErr_Print();
    FQ_TRACE("session", 0) << "Cannot find python callback function: "
                           << func;
  }

  // swap my thread state out of the interpreter
  PyThreadState_Swap(NULL);
  // clear out any cruft from thread state object
  PyThreadState_Clear(myThreadState);
  // delete my thread state object
  PyThreadState_Delete(myThreadState);
  // release the lock
  PyEval_ReleaseLock();

  if (func == "autoReply") {
    // TODO: fixme
    // paveViewMessage_->display("You have messages", PageViewMessage::Info, 0);
  }

  return done;
}
#endif //HAVE_PYTHON

void FQTermSession::onIdle() {
  // do as autoreply when it is enabled
  if (autoReplyTimer_->isActive() && isAutoReply_) {
    autoReplyMessage();
    stopAlert();
    return ;
  }

  isIdling_ = true;
  // system script can handle that
#ifdef HAVE_PYTHON
  if (pythonCallback("antiIdle", Py_BuildValue("(l)", this))) {
    return ;
  }
#endif
  // the default function
  int length;
  QByteArray cstr = parseString(param_.antiIdleMessage_.toLocal8Bit(), &length);
  telnet_->write(cstr, length);
}

void FQTermSession::onAutoReply() {
  // if AutoReply still enabled, then autoreply
  if (isAutoReply_) {
    autoReplyMessage();
  } else {
    // else just stop the timer
    autoReplyTimer_->stop();
  }

  stopAlert();
}
bool FQTermSession::isAutoReply() {
  return isAutoReply_;
}

void FQTermSession::setAutoReply(bool on) {
  isAutoReply_ = on;
  if (!isAutoReply_ && autoReplyTimer_->isActive()) {
    autoReplyTimer_->stop();
  }
}

int FQTermSession::write(const char *data, int len) {
  return telnet_->write(data, len);
}

int FQTermSession::writeStr(const char *str) {
  return write(str, strlen(str));
}

void FQTermSession::setProxy(int type, bool needAuth,
                             const QString &hostName, quint16 portNumber,
                             const QString &userName, const QString &password) {
  telnet_->setProxy(type, needAuth, hostName, portNumber, userName, password);
}

void FQTermSession::connectHost(const QString &hostName, quint16 portNumber) {
  telnet_->connectHost(hostName, portNumber);
}

void FQTermSession::close() {
  telnet_->close();
  if (reconnectTimer_->isActive()) {
    reconnectTimer_->stop();
  }
  finalizeConnection();
}

void FQTermSession::clearLineChanged(int index) {
  termBuffer_->clearLineChanged(index);
}

void FQTermSession::setLineAllChanged(int index) {
  termBuffer_->setLineAllChanged(index);
}

void FQTermSession::setTermSize(int col, int row) {
  termBuffer_->setTermSize(col, row);
}

const FQTermBuffer *FQTermSession::getBuffer() const {
  return termBuffer_;
}

void FQTermSession::setSelect(const QPoint &pt1, const QPoint &pt2) {
  termBuffer_->setSelect(pt1, pt2);
}

void FQTermSession::clearSelect() {
  termBuffer_->clearSelect();
}

void FQTermSession::leaveIdle() {
  if (autoReplyTimer_->isActive()) {
    autoReplyTimer_->stop();
  }

  if (idleTimer_->isActive()) {
    idleTimer_->stop();
  }

  if (isAntiIdle_) {
    idleTimer_->start(param_.maxIdleSeconds_ * 1000);
  }
}

void FQTermSession::copyArticle() {
  if (!acThread_->isRunning()) {
    acThread_->start();
  }
}

void FQTermSession::getLineColorInfo(const FQTermTextLine * line,
                                     LineColorInfo * colorInfo)
{
  colorInfo->backgroundColorIndex.clear();
  colorInfo->foregroundColorIndex.clear();
  colorInfo->hasBackgroundColor = false;
  colorInfo->uniBackgroundColor = true;
  colorInfo->uniForegroundColor = true;
  colorInfo->hasForegroundColor = false;

  if (line->getWidth() == 0) {
    return;
  }

  const unsigned char *color = line->getColors();
  unsigned char background = GETBG(color[0]);
  unsigned char foreground = GETFG(color[0]);
  colorInfo->backgroundColorIndex.append(background);
  colorInfo->foregroundColorIndex.append(foreground);
  //for (int i = 0; i < color.length() / 2; i++) {
  // TODO_UTF16: why use color.length()/2 formerly?
  for (int i = 0; i < (int)line->getWidth(); i++) {
    if (GETBG(color[i]) != background) {
      colorInfo->uniBackgroundColor = false;
      background = GETBG(color[i]);
      colorInfo->backgroundColorIndex.append(background);
    }
    if (GETBG(color[i]) != GETBG(NO_COLOR)) {
      colorInfo->hasBackgroundColor = true;
    }
    if (GETFG(color[i]) != foreground) {
      colorInfo->uniForegroundColor = false;
      foreground = GETFG(color[i]);
      colorInfo->foregroundColorIndex.append(foreground);
    }
    if (GETFG(color[i]) != GETFG(NO_COLOR)) {
      colorInfo->hasForegroundColor = true;
    }
  }
}

ArticleCopyThread::ArticleCopyThread(
    FQTermSession &bbs, QWaitCondition &waitCondition)
    : session_(bbs),
      waitCondition_(waitCondition),
      mutex_(new QMutex()) {
}

ArticleCopyThread::~ArticleCopyThread() {
  delete mutex_;
}

static void removeTrailSpace(QString &line) {
  for (int last_non_space = line.size() - 1;
       last_non_space >= 0; --last_non_space) {
    QChar a = line.at(last_non_space);
    if (!a.isSpace()) {
      line.resize(last_non_space + 1);
      break;
    }

    if (last_non_space == 0) {
      line.resize(0);
    }
  }
}

///////////////////////////////////////
//analyze last line when copy article
static std::pair<int, int> ParseLastLine(QString str) {
  int startLine = -1;
  int endLine = -1;
  int infoStart = str.indexOf('(', str.indexOf('(') + 1);
  int infoEnd = str.indexOf(')', infoStart);
  int infoSplit = str.indexOf('-', infoStart);
  if (infoStart == -1 || infoEnd == -1 || infoSplit == -1) {
    return std::make_pair(-1, -1);
  }
  bool ok = true;
  startLine = str.mid(infoStart + 1, infoSplit - infoStart - 1).toInt(&ok);
  if (!ok) {
    return std::make_pair(-1, -1);
  }
  endLine = str.mid(infoSplit + 1, infoEnd - infoSplit - 1).toInt(&ok);
  if (!ok) {
    return std::make_pair(-1, -1);
  }
  return std::make_pair(startLine, endLine);
}
///////////////////////////////////////


void ArticleCopyThread::run() {
  mutex_->lock();

  typedef std::vector<QString> Page;
  const FQTermBuffer *buffer = session_.getBuffer();;
  Page page;
  bool MultiPage = false;

  QString firstPageLastLine;
  buffer->getTextLineInTerm(buffer->getNumRows() - 1)->getAllPlainText(firstPageLastLine);
  if (firstPageLastLine.indexOf("%") != -1) {
    MultiPage = true;
    session_.write("e", 1);
    if (!waitCondition_.wait(mutex_, 10000)) {
      emit articleCopied(DAE_TIMEOUT, "");
      return;
    }
    QString LastPageLastLine;
    buffer->getTextLineInTerm(buffer->getNumRows() - 1)->getAllPlainText(LastPageLastLine);
    std::pair<int, int> range = ParseLastLine(LastPageLastLine);
    if (range.first == -1 || range.second == -1) {
      return;
    }
    page.resize(range.second);
    for (int i = range.first; i <= range.second; ++i) {
      QString strTemp;
      buffer->getTextLineInTerm(i - range.first)->getAllPlainText(strTemp);
      page[i - 1] = strTemp;
    }
    session_.write("s", 1);
    if (!waitCondition_.wait(mutex_, 10000)) {
      emit articleCopied(DAE_TIMEOUT, "");
      return;
    }
  }
  else {
    for (int i = 0; i < buffer->getNumRows() - 1; ++i) {
      QString strTemp;
      buffer->getTextLineInTerm(i)->getAllPlainText(strTemp);
      page.push_back(strTemp);
    }
  }

  while (MultiPage) {
    // the end of article
    int lineIndex = buffer->getNumRows() - 1;
    //QByteArray lastLine = buffer->getLineInScreen(lineIndex)->getText();
    QString lastLine;
    buffer->getTextLineInTerm(lineIndex)->getAllPlainText(lastLine);

    std::pair<int, int> range = ParseLastLine(lastLine);
    if (range.first == -1 || range.second == -1) {
      page.push_back(lastLine);
      break;
    }

    // copy a page of lines exclude the last line.
    for (int i = range.first; i <= range.second; ++i) {
      QString strTemp;
      buffer->getTextLineInTerm(i - range.first)->getAllPlainText(strTemp);
      page[i - 1] = strTemp;
    }


    // wait for next page.
    session_.write(" ", 1);
    if (!waitCondition_.wait(mutex_, 10000)) {
      emit articleCopied(DAE_TIMEOUT, "");
      break;
    }
  }



  QString articleText;

  for (Page::iterator line = page.begin(); line != page.end(); ++line) {
    removeTrailSpace(*line);
    articleText += (*line);
    articleText += (OS_NEW_LINE);
  }

  //qApp->postEvent(pWin, new QCustomEvent(DAE_FINISH));
  emit articleCopied(DAE_FINISH, articleText);
  mutex_->unlock();
}

}  // namespace FQTerm

#include "fqterm_session.moc"
