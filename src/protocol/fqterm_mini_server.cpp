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

#include "fqterm_mini_server.h"
#include "fqterm_trace.h"
#include <QByteArray>

namespace FQTerm {


void FQTermMiniServerThread::run()
{
  setTerminationEnabled();
  FQTermMiniServer miniServer;
  miniServer.setMaxPendingConnections(1);
  miniServer.listen(QHostAddress::LocalHost, 10093);
  exec();
  miniServer.close();
  miniServer.finalizeMiniServer();
}

FQTermMiniServer::FQTermMiniServer(QObject * parent)
  : QTcpServer(parent) {
  socket_ = new QTcpSocket(this);
  FQ_VERIFY(connect(socket_, SIGNAL(readyRead()),
    this, SLOT(readyRead()),Qt::DirectConnection));
  FQ_VERIFY(connect(socket_, SIGNAL(connected()),
		    this, SLOT(welcome()),Qt::DirectConnection));
}

void FQTermMiniServer::incomingConnection( int socketDescriptor ) {
  if (socket_->state() != QAbstractSocket::UnconnectedState) {
    return;
  }
  socket_->setSocketDescriptor(socketDescriptor);
  socket_->write("Welcome to FQBBS\r\n");
}

FQTermMiniServer::~FQTermMiniServer() {
}

void FQTermMiniServer::welcome()
{
    socket_->write("Welcome to FQBBS\r\n");
}

void FQTermMiniServer::readyRead() {
  QByteArray str = socket_->readAll();
  socket_->write(str);
}

void FQTermMiniServer::finalizeMiniServer() {
  socket_->close();
  delete socket_;
  socket_ = NULL;
}
} //namespace FQTerm

#include "fqterm_mini_server.moc"
