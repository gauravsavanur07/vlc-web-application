/*****************************************************************************
 * ControlServer.h: Acts as an interface between the controller and the client
 *****************************************************************************
 * Copyright (C) 2008-2018 VideoLAN
 *
 * Authors: Alper Çakan <alpercakan98@gmail.com>
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

#ifndef CONTROLSERVER_H
#define CONTROLSERVER_H

#include <QObject>
#include <QQueue>
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"

class ControlServer : public QObject
{
    Q_OBJECT
public:
    explicit ControlServer(quint16 port,
                           QString expectedId);
    ~ControlServer();

signals:
  void closed();

private slots:
    void onNewConnection();
    void onTextMsgReceived(QString message);
    void onSocketDisconnected();

private:
    void tryAuth(QString);
    void processMsg(QString)

    const QString m_expectedId;

    QWebSocketServer *m_wsServer;
    QWebSocket *m_client;

    bool m_authDone;
};

#endif // CONTROLSERVER_H
