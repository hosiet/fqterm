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

#include "fqterm_scriptengine.h"
#include "fqterm_window.h"
#include "fqterm_screen.h"
#include "fqterm_session.h"
#include "fqterm_buffer.h"
#include "common.h"
#include "fqterm_path.h"
#include <QString>
#include <QDir>
#include <QMessageBox>
#include <QTime>
#include <QTimer>
#include <QFile>
#include <QPoint>
#include <QCursor>
#include <QFileInfo>
#include <QScriptContext>
#include <QScriptContextInfo>
namespace FQTerm {


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


FQTermScriptEngine::FQTermScriptEngine(FQTermWindow* parent) 
: QObject(parent),
  window_(parent),
  engine_(NULL),
  articleCopyThread_(NULL),
  timerIDCount_(0) {
  FQ_VERIFY(window_);
  session_ = window_->getSession();
  buffer_ = window_->getSession()->getBuffer();
  screen_ = window_->getScreen();
  articleCopyThread_ = new ArticleCopyThread(*session_, session_->getWaitCondition(), session_->getBufferLock());
  FQ_VERIFY(connect(articleCopyThread_, SIGNAL(articleCopied(int, const QString)),
    this, SLOT(articleCopied(int, const QString))));
  engine_ = new QScriptEngine();
  engine_->globalObject().setProperty(
    "fq_session", engine_->newQObject(session_));
  engine_->globalObject().setProperty(
    "fq_window", engine_->newQObject(window_));
  engine_->globalObject().setProperty(
    "fq_buffer", engine_->newQObject(buffer_));
  engine_->globalObject().setProperty(
    "fq_screen", engine_->newQObject(screen_));
  engine_->globalObject().setProperty(
    "fqterm", engine_->newQObject(this));
}

FQTermScriptEngine::~FQTermScriptEngine() {
  engine_->abortEvaluation();
  delete engine_;
}

void FQTermScriptEngine::runScript( const QString& filename ) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    FQ_TRACE("script", 0) << "Unable to open the script file: " << filename;
    QMessageBox::warning(window_, tr("FQTerm"),
      tr("Unable to open the script file\n") + filename,
      QMessageBox::Close);
    return;
  }

  QString script = QString::fromUtf8(file.readAll());
  QFileInfo fileInfo(filename);
  QScriptValue result = engine_->evaluate(script, fileInfo.absoluteFilePath());
  if (engine_->hasUncaughtException()) {
    int line = engine_->uncaughtExceptionLineNumber();
    FQ_TRACE("script", 0) << "uncaught exception at line " << line
      << ":" << result.toString();
    QMessageBox::warning(window_, tr("FQTerm"),
      tr("uncaught exception at line ") + QString::number(line)
      + ":\n" + result.toString(),
      QMessageBox::Close);
  }
}

void FQTermScriptEngine::stopScript() {
  //all loops should be stopped here.
  articleCopied_ = true;
  engine_->abortEvaluation();
}

void FQTermScriptEngine::msgBox( const QString& msg ) {
  QMessageBox::warning(window_, tr("FQTerm"),
    msg,
    QMessageBox::Close);
}

int FQTermScriptEngine::caretX() {
  return buffer_->getCaretColumn();
}

int FQTermScriptEngine::caretY() {
  return buffer_->getCaretRow();
}

int FQTermScriptEngine::columns() {
  return buffer_->getNumColumns();
}

int FQTermScriptEngine::rows() {
  return buffer_->getNumRows();
}

void FQTermScriptEngine::sendString(const QString& str) {
  window_->writeRawString(str);
}

void FQTermScriptEngine::sendParsedString( const QString& str ){
  window_->externInput(str);
}

QString FQTermScriptEngine::getText(int row) {
  QString str;
  if (row < rows())
    buffer_->getTextLineInTerm(row)->getAllPlainText(str);
  return str;
}

QString FQTermScriptEngine::getAttrText(int row) {
  QString str;
  if (row < rows())
    buffer_->getTextLineInTerm(row)->getAllAnsiText(str);
  return str;
}

bool FQTermScriptEngine::isConnected() {
  return window_->isConnected();
}

void FQTermScriptEngine::disconnect() {
  window_->disconnect();
}

void FQTermScriptEngine::reconnect() {
  session_->reconnect();
}

QString FQTermScriptEngine::getBBSCodec() {
  return session_->param().serverEncodingID_ == 0 ? "GBK" : "Big5";
}

QString FQTermScriptEngine::getAddress() {
  return session_->param().hostAddress_;
}

int FQTermScriptEngine::getPort() {
  return session_->param().port_;
}
// connection protocol 0-telnet 1-SSH1 2-SSH2
int FQTermScriptEngine::getProtocol() {
  return session_->param().protocolType_;
}

QString FQTermScriptEngine::getReplyKey() {
  return session_->param().replyKeyCombination_;
}

QString FQTermScriptEngine::getURL() {
  return session_->getUrl();
}

QString FQTermScriptEngine::getIP() {
  return session_->getIP();
}

void FQTermScriptEngine::previewImage(const QString& url) {
  window_->getHttpHelper(url, true);
}

void FQTermScriptEngine::sleep(int ms) {
  int originInterval = engine_->processEventsInterval();
  engine_->setProcessEventsInterval(1);
  QString busy_wait = "var start = new Date().getTime();\n"
                      "while(true) {if(new Date().getTime() - start > " + QString("%1").arg(ms) + ") break;}\n";
  engine_->evaluate(busy_wait);
  engine_->setProcessEventsInterval(originInterval);
}

//TODO: flags
void FQTermScriptEngine::articleCopied(int state, const QString content) {
  if (state == DAE_FINISH) {
    articleCopied_ = true;
    articleText_ = content;
  } else /*if (state == DAE_TIMEOUT)*/ {
    articleCopied_ = true;
    articleText_ = "";
  }
}

QString FQTermScriptEngine::copyArticle() {
  articleCopied_ = false;
  articleText_ = "";
  articleCopyThread_->wait();
  articleCopyThread_->start();
  while(!articleCopied_) {
    sleep(200);
  }
  return articleText_;
}

void FQTermScriptEngine::finalizeScript() { 
  engine_->setProcessEventsInterval(-1);
  stopScript();
  setParent(NULL);
  //TODO: safe?
  deleteLater();
}

void FQTermScriptEngine::writeFile( const QString& filename, const QString& str ) {
  QFile file(filename);
  file.open(QIODevice::WriteOnly);
  file.write(U2U8(str));
  file.close();
}

QString FQTermScriptEngine::readFile( const QString& filename ) {
  QFile file(filename);
  file.open(QIODevice::ReadOnly);
  QString str = U82U(file.readAll());
  file.close();
  return str;
}

void FQTermScriptEngine::appendFile( const QString& filename, const QString& str ) {
  QFile file(filename);
  file.open(QIODevice::WriteOnly | QIODevice::Append);
  file.write(U2U8(str));
  file.close();
}

QStringList FQTermScriptEngine::readFolder( const QString& path ) {
  return QDir(path).entryList(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable);
}

QString FQTermScriptEngine::platform() {
#if defined(WIN32)
  return "Win";
#elif defined(__unix__) && !defined(__APPLE__)
  return "Linux";
#else
  return "Mac";
#endif
}

bool FQTermScriptEngine::makePath( const QString& path ){
  return QDir().mkpath(path);
}

QString FQTermScriptEngine::newLine() {
  return OS_NEW_LINE;
}

void FQTermScriptEngine::openUrl(const QString & url) {
  window_->openUrlImpl(url);
}

QString FQTermScriptEngine::getSelect(bool color_copy) {
  return buffer_->getTextSelected(
    session_->param().isRectSelect_,
    color_copy,
    window_->parseString((const char*)session_->param().escapeString_.toLatin1()));
}

QList<int> FQTermScriptEngine::mapToChar(int screenX, int screenY) {
  QPoint charP(screen_->mapToChar(QPoint(screenX, screenY)));
  QList<int> result;
  result << charP.x() << charP.y() - screen_->getBufferStart();
  return result;
}

int FQTermScriptEngine::charX(int screen_x) {
  return screen_->mapToChar(QPoint(screen_x, 0)).x();
}

int FQTermScriptEngine::charY(int screen_y) {
  return screen_->mapToChar(QPoint(0, screen_y)).y() - screen_->getBufferStart();
}

QString FQTermScriptEngine::getTextAt( int row, int column, int len ) {
  if (column < 0 || column >= columns() ||
      row < 0 || row >= rows()) 
      return "";
  if (column + len > columns() || len < 0) {
    len = columns() - column;
  }
  QString result;
  buffer_->getTextLineInTerm(row)->getPlainText(column, column + len, result);
  return result;
}

QString FQTermScriptEngine::getAttrTextAt( int row, int column, int len ) {
  if (column < 0 || column >= columns() ||
    row < 0 || row >= rows()) 
    return "";
  if (column + len > columns() || len < 0) {
    len = columns() - column;
  }
  QString result;
  buffer_->getTextLineInTerm(row)->getAnsiText(column, column + len, result);
  return result;
}

QString FQTermScriptEngine::getFullTextAt(int row, int column, int len) {
  return  getTextAt(row, buffer_->getTextLineInTerm(row)->getCellBegin(column), len);
}

QString FQTermScriptEngine::getFullAttrText(int row, int column, int len) {
  return  getAttrTextAt(row, buffer_->getTextLineInTerm(row)->getCellBegin(column), len);
}

int FQTermScriptEngine::mouseSX() {
  return screen_->mapFromGlobal(QCursor::pos()).x();
}

int FQTermScriptEngine::mouseSY() {
  return screen_->mapFromGlobal(QCursor::pos()).y();
}

int FQTermScriptEngine::screenX( int char_x ){
  return screen_->mapToPixel(QPoint(char_x, 0)).x();
}

int FQTermScriptEngine::screenY( int char_y ){
  return screen_->mapToPixel(QPoint(0, char_y + screen_->getBufferStart())).y();
}

bool FQTermScriptEngine::scriptCallback( const QString& func, const QScriptValueList& args ) {
  QScriptValue f = engine_->globalObject().property("fqterm").property(func);
  if (f.isFunction()) {
    QScriptValue res = f.call(QScriptValue(), args);
    return res.toBool();
  }
  return false;
}

bool FQTermScriptEngine::import( const QString& filename ) {
  QFileInfo fileInfo(filename);
  if (!fileInfo.isAbsolute()) {
    //first search current running path.
    if (engine_->currentContext() && engine_->currentContext()->parentContext()) {
      QString runningFile = QScriptContextInfo(engine_->currentContext()->parentContext()).fileName();
      if (!runningFile.isEmpty()) {
        QFileInfo current(runningFile);
        fileInfo = QFileInfo(current.absolutePath() + '/' + filename);
      }
    }
    //then look in script folder.
    if (!fileInfo.exists()) {
      fileInfo = QFileInfo(getPath(RESOURCE) + "script/" + filename);
    }
  } 
  if (!fileInfo.exists()) {
    return false;
  }
  fileInfo.makeAbsolute();
  runScript(fileInfo.absoluteFilePath());
  return true;
}

int FQTermScriptEngine::setInterval(int ms, const QScriptValue& func) {
  return createTimer(ms, func, false);
}


void FQTermScriptEngine::clearInterval(int id) {
  destroyTimer(id);
}

void FQTermScriptEngine::serverRedraw() {
  sendParsedString("^L");
}

void FQTermScriptEngine::clientRedraw() {
  window_->refreshScreen();
}

void FQTermScriptEngine::setMenuRect(int row, int column, int len) {
  if (column < 0 || column >= columns() ||
    row < 0 || row >= rows()) 
    return ;
  if (column + len > columns() || len < 0) {
    len = columns() - column;
  }
  row += screen_->getBufferStart();
  session_->setMenuRect(row, column, len);
}

int FQTermScriptEngine::setTimeout(int ms, const QScriptValue& func) {
  return createTimer(ms, func, true);
}

void FQTermScriptEngine::clearTimeout(int id) {
  destroyTimer(id);
}

int FQTermScriptEngine::createTimer(int ms, const QScriptValue& func, bool singleShot) {
  QTimer* timer = new QTimer(this);
  timer->setInterval(ms);
  timer->setSingleShot(singleShot);
  int id = timerIDCount_++;
  if (timerTable_.find(id) != timerTable_.end()) {
    //actually impossible!
    destroyTimer(id);
  }
  timerTable_[id] = timer;
  bool res = qScriptConnect(timer, SIGNAL(timeout()), QScriptValue(), func);
  timer->start();
  return id;
}

void FQTermScriptEngine::destroyTimer(int id) {
  if (timerTable_.find(id) != timerTable_.end()) {
    timerTable_[id]->stop();
    delete timerTable_[id];
    timerTable_.erase(id);
  }
}

int FQTermScriptEngine::getUIEventInterval() {
  return engine_->processEventsInterval();
}

void FQTermScriptEngine::setUIEventInterval(int ms) {
  engine_->setProcessEventsInterval(ms);
}
} // namespace FQTerm

#include "fqterm_scriptengine.moc"