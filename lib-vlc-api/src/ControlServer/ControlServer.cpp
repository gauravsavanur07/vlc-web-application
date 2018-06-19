/*****************************************************************************
 * ControlServer.cpp: Acts as an interface between the controller and the client
 *****************************************************************************
 * Copyright (C) 2008-2018 VideoLAN
 *
 * Authors: Gaurav Savanur <gauravsavanur07@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "controlserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"

ControlServer::ControlServer(quint16 port,
                             QString expectedId) :
    m_wsServer(new QWebSocketServer("",
                                    QWebSocketServer::SecureMode, this)),
    m_expectedId(expectedId),
    m_client(nullptr),
    m_authDone(false)
{
    if (m_wsServer->listen(QHostAddress::Any, port)) {
        connect(m_wsServer, &QWebSocketServer::newConnection,
                this, &ControlServer::onNewConnection);
    }
}

ControlServer::~ControlServer() { }

void ControlServer::onNewConnection()
{
  if (m_client) {
    return;
  }

  m_client = m_pWebSocketServer->nextPendingConnection();

  connect(m_client, &QWebSocket::textMessageReceived,
          this, &ControlServer::onTextMsgReceived);
  connect(m_client, &QWebSocket::disconnected,
          this, &ControlServer::onSocketDisconnected);

  m_wsServer->close();
  delete m_wsServer;
}

void ControlServer::onTextMsgReceived(QString msg)
{
  if (m_authDone) {
    processMsg(msg);
  } else {
    tryAuth(msg);
  }
}

void ControlServer::onSocketDisconnected()
{
  delete m_client;
  emit closed();
}
