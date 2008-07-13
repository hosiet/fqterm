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

#include <QCustomEvent>
#include <QTextStream>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QByteArray>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_PYTHON
#include <Python.h>

#include "fqterm_window.h"
#include "fqterm_buffer.h"
#include "fqterm_text_line.h"
#include "fqterm_param.h"

namespace FQTerm {

/* **************************************************************************
 *
 *				Pythons Embedding
 *
 * ***************************************************************************/
extern QString pathCfg;

QByteArray stripWhitespace(const QByteArray &cstr) {
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

QString getException() {
  PyObject *pType = NULL,  *pValue = NULL,  *pTb = NULL,  *pName,  *pTraceback;

  PyErr_Fetch(&pType, &pValue, &pTb);

  pName = PyString_FromString("traceback");
  pTraceback = PyImport_Import(pName);
  Py_DECREF(pName);

  if (pTraceback == NULL) {
    return "General Error in Python Callback";
  }

  pName = PyString_FromString("format_exception");
  PyObject *pRes = PyObject_CallMethodObjArgs(pTraceback, pName, pType, pValue,
                                              pTb, NULL);
  Py_DECREF(pName);

  Py_DECREF(pTraceback);

  Py_XDECREF(pType);
  Py_XDECREF(pValue);
  Py_XDECREF(pTb);

  if (pRes == NULL) {
    return "General Error in Python Callback";
  }

  pName = PyString_FromString("string");
  PyObject *pString = PyImport_Import(pName);
  Py_DECREF(pName);

  if (pString == NULL) {
    return "General Error in Python Callback";
  }

  pName = PyString_FromString("join");
  PyObject *pErr = PyObject_CallMethodObjArgs(pString, pName, pRes, NULL);
  Py_DECREF(pName);

  Py_DECREF(pString);
  Py_DECREF(pRes);

  if (pErr == NULL) {
    return "General Error in Python Callback";
  }

  QString str(PyString_AsString(pErr));
  Py_DECREF(pErr);

  return str;
}

QString getErrOutputFile(FQTermWindow *lp) {
  // file name
  QString str2;
  str2.setNum(long(lp));
  str2 += ".err";
  // path
  return pathCfg + str2;
}

// copy current artcle for back compatible use only
// for new coder please use getArticle
static PyObject *fqterm_copyArticle(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  FQTermWindow *termWindow_ = (FQTermWindow*)lp;

  QStringList strList;
  QString strArticle;
  while (1) {
    // check it there is duplicated string
    // it starts from the end in the range of one screen height
    // so this is a non-greedy match
    QString strTemp = stripWhitespace(termWindow_->session_->termBuffer_->screen(0)->getText())
                      ;
    int i = 0;
    int start = 0;
    for (QStringList::Iterator it = strList.end();
         it != strList.begin() &&  i < termWindow_->session_->termBuffer_->numRows() - 1;  // not exceeeding the last screen
		 --it, i++) {
      if (*it != strTemp) {
        continue;
      }
      QStringList::Iterator it2 = it;
      bool dup = true;
      // match more to see if its duplicated
      for (int j = 0; j <= i; j++, it2++) {
        QString str1 = termWindow_->stripWhitespace(termWindow_->session_->termBuffer_->screen(j)->getText());
        if (*it2 != str1) {
          dup = false;
          break;
        }
      }
      if (dup) {
        // set the start point
        start = i + 1;
        break;
      }
    }
    // add new lines
    for (i = start; i < termWindow_->session_->termBuffer_->numRows() - 1; i++) {
      strList += stripWhitespace(termWindow_->session_->termBuffer_->screen(i)->getText());
    }

    // the end of article
    if (termWindow_->session_->termBuffer_->screen(termWindow_->session_->termBuffer_->numRows() - 1)->getText().indexOf("%")
        == -1) {
      break;
    }
    // continue
    termWindow_->session_->telnet_->write(" ", 1);

    // TODO: fixme
    //if (!termWindow_->bbs_->waitCondition_.wait(10000)) {
    //  // timeout
    //  break;
    //}
  }
#if defined(_OS_WIN32_) || defined(Q_OS_WIN32)
  strArticle = strList.join("\r\n");
#else
  strArticle = strList.join("\n");
#endif

  PyObject *py_text = PyString_FromString(strArticle.toUtf8());

  Py_INCREF(py_text);
  return py_text;
}

static PyObject *fqterm_getArticle(PyObject *, PyObject *args) {
  long lp;
  int timeout;
  int succeed = 1;

  if (!PyArg_ParseTuple(args, "li", &lp, &timeout)) {
    return NULL;
  }

  FQTermWindow *termWindow_ = (FQTermWindow*)lp;

  QStringList strList;
  QString strArticle;
  while (1) {
    // check it there is duplicated string
    // it starts from the end in the range of one screen height
    // so this is a non-greedy match
    QString strTemp = stripWhitespace(termWindow_->session_->termBuffer_->screen(0)->getText())
                      ;
    int i = 0;
    int start = 0;
    for (QStringList::Iterator it = strList.end();
         it != strList.begin() && i < termWindow_->session_->termBuffer_->numRows() - 1;  // not exceeeding the last screen
         --it, i++) {
      if (*it != strTemp) {
        continue;
      }
      QStringList::Iterator it2 = it;
      bool dup = true;
      // match more to see if its duplicated
      for (int j = 0; j <= i; j++, it2++) {
        QString str1 = stripWhitespace(termWindow_->session_->termBuffer_->screen(j)->getText());
        if (*it2 != str1) {
          dup = false;
          break;
        }
      }
      if (dup) {
        // set the start point
        start = i + 1;
        break;
      }
    }
    // add new lines
    for (i = start; i < termWindow_->session_->termBuffer_->numRows() - 1; i++) {
      strList += stripWhitespace(termWindow_->session_->termBuffer_->screen(i)->getText());
    }

    // the end of article
    if (termWindow_->session_->termBuffer_->screen(termWindow_->session_->termBuffer_->numRows() - 1)->getText().indexOf("%")
        == -1) {
      break;
    }
    // continue
    termWindow_->session_->telnet_->write(" ", 1);

    // TODO: fixme
    //if (!termWindow_->bbs_->waitCondition_.wait(timeout *1000))
    // { // timeout
    //	succeed = 0;
    //	break;
    // }
  }
#if defined(_OS_WIN32_) || defined(Q_OS_WIN32)
  strArticle = strList.join("\r\n");
#else
  strArticle = strList.join("\n");
#endif

  PyObject *py_res = Py_BuildValue("si", (const char*)strArticle.toLocal8Bit(),
                                   succeed);

  Py_INCREF(py_res);

  return py_res;

}

static PyObject *fqterm_formatError(PyObject *, PyObject *args) {
  long lp;

  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  QString strErr;
  QString filename = getErrOutputFile((FQTermWindow*)lp);

  QDir d;
  if (d.exists(filename)) {
    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    QTextStream is(&file);
    while (!is.atEnd()) {
      strErr += is.readLine(); // line of text excluding '\n'
      strErr += '\n';
    }
    file.close();
    d.remove(filename);
  }

  if (!strErr.isEmpty()) {
    ((FQTermWindow*)lp)->pythonErrorMessage_ = strErr;
    // TODO: fixme
    //qApp->postEvent((FQTermWindow*)lp, new QCustomEvent(PYE_ERROR));
  } else {
    // TODO: fixme
    //qApp->postEvent((FQTermWindow*)lp, new QCustomEvent(PYE_FINISH));
  }


  Py_INCREF(Py_None);
  return Py_None;
}

// caret x
static PyObject *fqterm_caretX(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }
  int x = ((FQTermWindow*)lp)->session_->termBuffer_->getCaretColumn();
  PyObject *py_x = Py_BuildValue("i", x);
  Py_INCREF(py_x);
  return py_x;
}

// caret y
static PyObject *fqterm_caretY(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }
  int y = ((FQTermWindow*)lp)->session_->termBuffer_->getCaretRow();
  PyObject *py_y = Py_BuildValue("i", y);
  Py_INCREF(py_y);
  return py_y;

}

// columns
static PyObject *fqterm_columns(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  int numColumns = ((FQTermWindow*)lp)->session_->termBuffer_->numColumns();
  PyObject *py_columns = Py_BuildValue("i", numColumns);

  Py_INCREF(py_columns);
  return py_columns;

}

// rows
static PyObject *fqterm_rows(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  int rows = ((FQTermWindow*)lp)->session_->termBuffer_->numRows();
  PyObject *py_rows = Py_BuildValue("i", rows);

  Py_INCREF(py_rows);
  return py_rows;
}

// sned string to server
static PyObject *fqterm_sendString(PyObject *, PyObject *args) {
  char *pstr;
  long lp;
  int len;

  if (!PyArg_ParseTuple(args, "ls", &lp, &pstr)) {
    return NULL;
  }

  len = strlen(pstr);

  ((FQTermWindow*)lp)->session_->telnet_->write(pstr, len);

  Py_INCREF(Py_None);
  return Py_None;
}

// same as above except parsing string first "\n" "^p" etc
static PyObject *fqterm_sendParsedString(PyObject *, PyObject *args) {
  char *pstr;
  long lp;
  int len;

  if (!PyArg_ParseTuple(args, "ls", &lp, &pstr)) {
    return NULL;
  }
  len = strlen(pstr);

  ((FQTermWindow*)lp)->sendParsedString(pstr);

  Py_INCREF(Py_None);
  return Py_None;
}

// get text at line
static PyObject *fqterm_getText(PyObject *, PyObject *args) {
  long lp;
  int numRows;
  if (!PyArg_ParseTuple(args, "li", &lp, &numRows)) {
    return NULL;
  }
  QByteArray cstr = ((FQTermWindow*)lp)->session_->termBuffer_->screen(numRows)->getText();

  PyObject *py_text = PyString_FromString(cstr);

  Py_INCREF(py_text);
  return py_text;
}

// get text with attributes
static PyObject *fqterm_getAttrText(PyObject *, PyObject *args) {
  long lp;
  int numRows;
  if (!PyArg_ParseTuple(args, "li", &lp, &numRows)) {
    return NULL;
  }

  QByteArray cstr = ((FQTermWindow*)lp)->session_->termBuffer_->screen(numRows)->getAttrText();

  PyObject *py_text = PyString_FromString(cstr);

  Py_INCREF(py_text);
  return py_text;
}

// is host connected
static PyObject *fqterm_isConnected(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  bool connected = ((FQTermWindow*)lp)->isConnected();
  PyObject *py_connected = Py_BuildValue("i", connected ? 1 : 0);

  Py_INCREF(py_connected);
  return py_connected;
}

// disconnect from host
static PyObject *fqterm_disconnect(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  ((FQTermWindow*)lp)->disconnect();

  Py_INCREF(Py_None);
  return Py_None;
}

// reconnect to host
static PyObject *fqterm_reconnect(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  ((FQTermWindow*)lp)->session_->reconnect();

  Py_INCREF(Py_None);
  return Py_None;
}

// bbs encoding 0-GBK 1-BIG5
static PyObject *fqterm_getBBSCodec(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_codec = PyString_FromString(((FQTermWindow*)lp)
                                           ->session_->param_.serverEncodingID_ == 0 ? "GBK" : "Big5");
  Py_INCREF(py_codec);

  return py_codec;
}

// host address
static PyObject *fqterm_getAddress(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_addr = PyString_FromString(((FQTermWindow*)lp)
                                          ->session_->param_.hostAddress_.toLocal8Bit());
  Py_INCREF(py_addr);
  return py_addr;
}

// host port number
static PyObject *fqterm_getPort(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_port = Py_BuildValue("i", ((FQTermWindow*)lp)->session_->param_.port_);
  Py_INCREF(py_port);
  return py_port;
}

// connection protocol 0-telnet 1-SSH1 2-SSH2
static PyObject *fqterm_getProtocol(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_port = Py_BuildValue("i", ((FQTermWindow*)lp)
                                    ->session_->param_.protocolType_);
  Py_INCREF(py_port);
  return py_port;
}

// key to reply msg
static PyObject *fqterm_getReplyKey(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_key = PyString_FromString(((FQTermWindow*)lp)
                                         ->session_->param_.replyKeyCombination_.toLocal8Bit());
  Py_INCREF(py_key);
  return py_key;
}

// url under mouse
static PyObject *fqterm_getURL(PyObject *, PyObject *args) {
  long lp;
  if (!PyArg_ParseTuple(args, "l", &lp)) {
    return NULL;
  }

  PyObject *py_url = PyString_FromString(((FQTermWindow*)lp)->session_->getUrl().toUtf8().constData());
  Py_INCREF(py_url);
  return py_url;
}

// preview image link
static PyObject *fqterm_previewImage(PyObject *, PyObject *args) {
  long lp;
  char *url;
  if (!PyArg_ParseTuple(args, "ls", &lp, &url)) {
    return NULL;
  }

  ((FQTermWindow*)lp)->getHttpHelper(url, true);

  Py_INCREF(Py_None);
  return Py_None;

}

// convert string from UTF8 to specified encoding
static PyObject *fqterm_fromUTF8(PyObject *, PyObject *args) {
  char *str,  *enc;
  if (!PyArg_ParseTuple(args, "ss", &str, &enc)) {
    return NULL;
  }
  QTextCodec *encodec = QTextCodec::codecForName(enc);
  QTextCodec *utf8 = QTextCodec::codecForName("utf8");

  PyObject *py_str = PyString_FromString(encodec->fromUnicode(utf8->toUnicode
                                                              (str)));
  Py_INCREF(py_str);
  return py_str;
}

// convert string from specified encoding to UTF8
static PyObject *fqterm_toUTF8(PyObject *, PyObject *args) {
  char *str,  *enc;
  if (!PyArg_ParseTuple(args, "ss", &str, &enc)) {
    return NULL;
  }
  QTextCodec *encodec = QTextCodec::codecForName(enc);
  QTextCodec *utf8 = QTextCodec::codecForName("utf8");

  PyObject *py_str = PyString_FromString(utf8->fromUnicode(encodec->toUnicode
                                                           (str)));
  Py_INCREF(py_str);
  return py_str;
}


PyMethodDef fqterm_methods[] =  {
  {
    "formatError", (PyCFunction)fqterm_formatError, METH_VARARGS,
	"get the traceback info"
  }

  ,

  {
    "getArticle", (PyCFunction)fqterm_getArticle, METH_VARARGS,
	"copy current article"
  }

  ,

  {
    "copyArticle", (PyCFunction)fqterm_copyArticle, METH_VARARGS,
	"copy current article (obsolete)"
  }

  ,

  {
    "getText", (PyCFunction)fqterm_getText, METH_VARARGS, "get text at line#"
  }

  ,

  {
    "getAttrText", (PyCFunction)fqterm_getAttrText, METH_VARARGS,
	"get attr text at line#"
  }

  ,

  {
    "sendString", (PyCFunction)fqterm_sendString, METH_VARARGS,
	"send string to server"
  }

  ,

  {
    "sendParsedString", (PyCFunction)fqterm_sendParsedString, METH_VARARGS,
	"send string with escape"
  }

  ,

  {
    "caretX", (PyCFunction)fqterm_caretX, METH_VARARGS, "caret x"
  }

  ,

  {
    "caretY", (PyCFunction)fqterm_caretY, METH_VARARGS, "caret y"
  }

  ,

  {
    "columns", (PyCFunction)fqterm_columns, METH_VARARGS, "screen width"
  }

  ,

  {
    "rows", (PyCFunction)fqterm_rows, METH_VARARGS, "screen height"
  }

  ,

  {
    "isConnected", (PyCFunction)fqterm_isConnected, METH_VARARGS,
	"connected to server or not"
  }

  ,

  {
    "disconnect", (PyCFunction)fqterm_disconnect, METH_VARARGS,
	"disconnect from server"
  }

  ,

  {
    "reconnect", (PyCFunction)fqterm_reconnect, METH_VARARGS, "reconnect"
  }

  ,

  {
    "getBBSCodec", (PyCFunction)fqterm_getBBSCodec, METH_VARARGS,
	"get the bbs encoding, GBK or Big5"
  }

  ,

  {
    "getAddress", (PyCFunction)fqterm_getAddress, METH_VARARGS,
	"get the bbs address"
  }

  ,

  {
    "getPort", (PyCFunction)fqterm_getPort, METH_VARARGS, "get the bbs port number"
  }

  ,

  {
    "getProtocol", (PyCFunction)fqterm_getPort, METH_VARARGS,
	"get the bbs protocol, 0/1/2 TELNET/SSH1/SSH2"
  }

  ,

  {
    "getReplyKey", (PyCFunction)fqterm_getReplyKey, METH_VARARGS,
	"get the key to reply messages"
  }

  ,

  {
    "getURL", (PyCFunction)fqterm_getURL, METH_VARARGS,
	"get the url string under mouse"
  }

  ,

  {
    "previewImage", (PyCFunction)fqterm_previewImage, METH_VARARGS,
	"preview the image link"
  }

  ,

  {
    "fromUTF8", (PyCFunction)fqterm_fromUTF8, METH_VARARGS,
	"decode from utf8 to string in specified codec"
  }

  ,

  {
    "toUTF8", (PyCFunction)fqterm_toUTF8, METH_VARARGS,
	"decode from string in specified codec to utf8"
  }

  ,

  {
    NULL, (PyCFunction)NULL, 0, NULL
  }
};

}  // namespace FQTerm

#endif //HAVE_PYTHON
