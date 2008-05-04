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

#include <stdlib.h>

#if defined(WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/stat.h>
#include <errno.h>
#include <locale.h>
#endif

#if defined(FQTERM_USE_STATIC_QT)
  // static link Qt4 under other OS.
  void loadNecessaryQtPlugins() {}
  #include <QtPlugin>
  Q_IMPORT_PLUGIN(qkrcodecs)
  Q_IMPORT_PLUGIN(qcncodecs)
  Q_IMPORT_PLUGIN(qjpcodecs)
  Q_IMPORT_PLUGIN(qtwcodecs)
  Q_IMPORT_PLUGIN(qjpeg)
  Q_IMPORT_PLUGIN(qgif)
  Q_IMPORT_PLUGIN(qmng)
#else
  // dynamic link Qt4 under linux
  #include <QPluginLoader>
  void loadNecessaryQtPlugins() {
    static QPluginLoader qkrcodecsLoader( "qkrcodecs" );
    static QPluginLoader qcncodecsLoader( "qcncodecs" );
    static QPluginLoader qjpcodecsLoader( "qjpcodecs" );
    static QPluginLoader qtwcodecsLoader( "qtwcodecs" );
    static QPluginLoader qjpegLoader("qjpeg");
    static QPluginLoader qgifLoader("qgif");
    static QPluginLoader qmngLoader("qmng");
   
    qkrcodecsLoader.load();
    qcncodecsLoader.load();
    qjpcodecsLoader.load();
    qtwcodecsLoader.load();
    qjpegLoader.load();
    qgifLoader.load();
    qmngLoader.load();
  }
#endif

#include <QApplication>
#include <QTranslator>
#include <QFontDatabase>

#include "fqterm.h"
#include "fqterm_frame.h"
#include "fqterm_path.h"
#include "fqterm_trace.h"
#include "fqterm_config.h"
#include "fqterm_param.h"
#include "fqterm_text_line.h"

int main(int argc, char **argv) {
  QApplication a(argc, argv);

  // Set trace categories and level.
  FQTerm::setMaxTraceLevel(1);
  for (int i = 1; i < argc; ++i) {
    QString str(argv[i]);
    bool ok;
    int max_level = str.toInt(&ok, 0); 

    if (ok) {
      FQTerm::setMaxTraceLevel(max_level);
    } else {
      FQTerm::addAllowedCategory(argv[i]);
    }
  }

  using namespace FQTerm;

  loadNecessaryQtPlugins();

  if (!iniSettings()) {
    return -1;
  }

  FQTermFrame *mw = new FQTermFrame();
  mw->setWindowTitle("FQTerm " + QString(FQTERM_VERSION_STRING));
  mw->setWindowIcon(QPixmap(getPath(RESOURCE) + "pic/fqterm.png"));
  mw->show();
  FQ_VERIFY(a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit())));

  int res = a.exec();
  return res;
}
