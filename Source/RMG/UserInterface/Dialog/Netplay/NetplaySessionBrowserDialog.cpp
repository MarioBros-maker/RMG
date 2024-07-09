/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "NetplaySessionBrowserDialog.hpp"
#include "UserInterface/NoFocusDelegate.hpp"

#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonObject>

#include <RMG-Core/Core.hpp>

#define NETPLAY_VER 16

using namespace UserInterface::Dialog;

//
// Local Structs
//

struct NetplayRomData_t
{
    QString GoodName;
    QString MD5;
    QString File;
};

Q_DECLARE_METATYPE(NetplayRomData_t);

//
// Exported Functions
//


NetplaySessionBrowserDialog::NetplaySessionBrowserDialog(QWidget *parent, QWebSocket* webSocket, QMap<QString, CoreRomSettings> modelData) : QDialog(parent)
{
    qRegisterMetaType<NetplayRomData_t>();

    this->setupUi(this);

    // prepare web socket
    this->webSocket = webSocket;

    connect(this->webSocket, &QWebSocket::connected, this, &NetplaySessionBrowserDialog::on_webSocket_connected);
    connect(this->webSocket, &QWebSocket::textMessageReceived, this, &NetplaySessionBrowserDialog::on_webSocket_textMessageReceived);

    // prepare broadcast
    broadcastSocket.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    connect(&this->broadcastSocket, &QUdpSocket::readyRead, this, &NetplaySessionBrowserDialog::on_broadcastSocket_readyRead);
    QByteArray multirequest;
    multirequest.append(1);
    broadcastSocket.writeDatagram(multirequest, QHostAddress::Broadcast, 45000);

    // change ok button name
    QPushButton* button = this->buttonBox->button(QDialogButtonBox::Ok);
    button->setText("Join");

    /*
    // transform model data to data we can use
    QList<NetplayRomData_t> romData;
    romData.reserve(modelData.size());
    for (auto it = modelData.begin(); it != modelData.end(); it++)
    {
        romData.append(
        {
            QString::fromStdString(it.value().GoodName), 
            QString::fromStdString(it.value().MD5), 
            it.key()
        });
    }
    // add data to list widget
    for (const NetplayRomData_t& data : romData)
    {
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole, QVariant::fromValue(data));
        item->setText(data.GoodName);
        this->listWidget->addItem(item);
    }
    this->listWidget->sortItems();

    this->validateCreateButton();*/

    // prepare session browser
    this->tableWidget->setFrameStyle(QFrame::NoFrame);
    this->tableWidget->setItemDelegate(new NoFocusDelegate(this));
    this->tableWidget->setWordWrap(false);
    this->tableWidget->setShowGrid(false);
    this->tableWidget->setSortingEnabled(true);
    this->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->tableWidget->setSelectionBehavior(QTableView::SelectRows);
    this->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    this->tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
    this->tableWidget->verticalHeader()->hide();
    this->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->tableWidget->horizontalHeader()->setSectionsMovable(true);
    this->tableWidget->horizontalHeader()->setFirstSectionMovable(true);
    this->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    this->tableWidget->horizontalHeader()->setSortIndicatorShown(false);
    this->tableWidget->horizontalHeader()->setHighlightSections(false);
}

NetplaySessionBrowserDialog::~NetplaySessionBrowserDialog(void)
{
}

void NetplaySessionBrowserDialog::addSessionData(QString name, QString game, QString md5, bool password)
{
    int row = this->tableWidget->rowCount();

    this->tableWidget->insertRow(row);

    // Session name
    QTableWidgetItem* tableWidgetItem1 = new QTableWidgetItem(name);
    this->tableWidget->setItem(row, 0, tableWidgetItem1);

    // game
    QTableWidgetItem* tableWidgetItem2 = new QTableWidgetItem(game);
    this->tableWidget->setItem(row, 1, tableWidgetItem2);

    // md5
    QTableWidgetItem* tableWidgetItem3 = new QTableWidgetItem(md5);
    this->tableWidget->setItem(row, 2, tableWidgetItem3);

    // password
    QTableWidgetItem* tableWidgetItem4 = new QTableWidgetItem(password ? "Yes" : "No");
    this->tableWidget->setItem(row, 3, tableWidgetItem4);
}

void NetplaySessionBrowserDialog::on_webSocket_connected(void)
{
    QJsonObject json;
    json.insert("type", "request_get_rooms");
    json.insert("netplay_version", NETPLAY_VER);
    
    // TODO
    json.insert("client_sha", "c96e1682c94027eeed662d07083d25ffab8309b8"); //QString::fromStdString(CoreGetVersion()));

    // TODO: add common function...
    json.insert("emulator", "simple64");

    this->webSocket->sendTextMessage(QJsonDocument(json).toJson());
}

#include <iostream>
void NetplaySessionBrowserDialog::on_webSocket_textMessageReceived(QString message)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = jsonDocument.object();

    QString type = json.value("type").toString();

    if (type == "reply_get_rooms")
    {
        if (json.value("accept").toInt() == 0)
        {
            this->addSessionData(json.value("room_name").toString(), 
                                    json.value("game_name").toString(), 
                                    json.value("MD5").toString(), 
                                    !json.value("features").toObject().value("cheats").toString().isEmpty());
        }
        else
        {
            std::cout << "aaaa: " << json.value("message").toString().toStdString() << std::endl;
        }
    }
}

void NetplaySessionBrowserDialog::on_broadcastSocket_readyRead(void)
{
    while (this->broadcastSocket.hasPendingDatagrams())
    {
        QNetworkDatagram datagram = this->broadcastSocket.receiveDatagram();
        QByteArray incomingData = datagram.data();
        QJsonDocument json_doc  = QJsonDocument::fromJson(incomingData);
        QJsonObject json        = json_doc.object();
        QStringList servers     = json.keys();

        for (int i = 0; i < servers.size(); i++)
        {
            this->serverComboBox->addItem(servers.at(i), json.value(servers.at(i)).toString());
        }
    }
}

void NetplaySessionBrowserDialog::on_serverComboBox_currentIndexChanged(int index)
{
    if (index == -1)
    {
        return;
    }

    std::cout << "on_serverComboBox_currentIndexChanged" << std::endl;

    QString address = this->serverComboBox->itemData(index).toString();
    std::cout << address.toStdString() << std::endl;
    this->webSocket->open(QUrl(address));
}