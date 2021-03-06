/*
  Q Light Controller Plus
  e131node.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "e131controller.h"

#include <QDebug>

E131Controller::E131Controller(QString ipaddr, QList<QNetworkAddressEntry> interfaces,
                                   QList<QString> macAddrList, Type type, QObject *parent)
    : QObject(parent)
{
    m_ipAddr = QHostAddress(ipaddr);

    int i = 0;
    foreach(QNetworkAddressEntry iface, interfaces)
    {
        if (iface.ip() == m_ipAddr)
        {
            m_MACAddress = macAddrList.at(i);
            break;
        }
        i++;
    }

    qDebug() << "[E131Controller] type: " << type;
    m_packetizer = new E131Packetizer();
    m_packetSent = 0;
    m_packetReceived = 0;

    m_UdpSocket = new QUdpSocket(this);

    if (m_UdpSocket->bind(m_ipAddr, E131_DEFAULT_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint) == false)
        return;

    connect(m_UdpSocket, SIGNAL(readyRead()),
            this, SLOT(processPendingPackets()));

    // reset initial DMX values if we're an input
    if (type == Input)
        m_dmxValues.fill(0, 2048);

    m_type = type;
}

E131Controller::~E131Controller()
{
    qDebug() << Q_FUNC_INFO;
    disconnect(m_UdpSocket, SIGNAL(readyRead()),
            this, SLOT(processPendingPackets()));
    m_UdpSocket->close();
}

void E131Controller::addUniverse(quint32 line, int uni)
{
    if (m_universes.contains(uni) == false)
    {
        m_universes[uni] = line;
        m_multicastAddr[uni] = QHostAddress(QString("239.255.0.%1").arg(uni + 1));
        qDebug() << "[E131Controller] Universe:" << uni <<
                 ", multicast address:" << m_multicastAddr[uni].toString() <<
                 "(MAC:" << m_MACAddress << ")";
    }
}

int E131Controller::getUniversesNumber()
{
    return m_universes.size();
}

bool E131Controller::removeUniverse(int uni)
{
    if (m_universes.contains(uni))
    {

        qDebug() << Q_FUNC_INFO << "Removing universe " << uni;
        return m_universes.remove(uni);
    }
    return false;
}

int E131Controller::getType()
{
    return m_type;
}

quint64 E131Controller::getPacketSentNumber()
{
    return m_packetSent;
}

quint64 E131Controller::getPacketReceivedNumber()
{
    return m_packetReceived;
}

QString E131Controller::getNetworkIP()
{
    return m_ipAddr.toString();
}

void E131Controller::sendDmx(const int &universe, const QByteArray &data)
{
    QByteArray dmxPacket;
    m_packetizer->setupE131Dmx(dmxPacket, universe, data);
    qint64 sent = m_UdpSocket->writeDatagram(dmxPacket.data(), dmxPacket.size(),
                                             m_multicastAddr[universe], E131_DEFAULT_PORT);
    if (sent < 0)
    {
        qDebug() << "sendDmx failed";
        qDebug() << "Errno: " << m_UdpSocket->error();
        qDebug() << "Errmgs: " << m_UdpSocket->errorString();
    }
    else
        m_packetSent++;
}

void E131Controller::processPendingPackets()
{
    while (m_UdpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        QHostAddress senderAddress;
        datagram.resize(m_UdpSocket->pendingDatagramSize());
        m_UdpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress);
        if (senderAddress != m_ipAddr)
        {
            qDebug() << "Received packet with size: " << datagram.size() << ", host: " << senderAddress.toString();
            if (m_packetizer->checkPacket(datagram) == true)
            {
                QByteArray dmxData;
                int universe;
                if (this->getType() == Input)
                {
                    m_packetReceived++;
                    if (m_packetizer->fillDMXdata(datagram, dmxData, universe) == true)
                    {
                        if ((universe * 512) > m_dmxValues.length() || m_universes.contains(universe) == false)
                        {
                            qDebug() << "Universe " << universe << "not supported !";
                            break;
                        }
                        for (int i = 0; i < dmxData.length(); i++)
                        {
                            if (m_dmxValues.at(i + (universe * 512)) != dmxData.at(i))
                            {
                                m_dmxValues[i + (universe * 512)] =  dmxData[i];
                                emit valueChanged(m_universes[universe], i, (uchar)dmxData.at(i));
                            }
                        }
                    }
                }
            }
        }
    }
}
