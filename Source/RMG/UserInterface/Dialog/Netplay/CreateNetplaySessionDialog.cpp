/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "CreateNetplaySessionDialog.hpp"

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


CreateNetplaySessionDialog::CreateNetplaySessionDialog(QWidget *parent, QWebSocket* webSocket, QMap<QString, CoreRomSettings> modelData) : QDialog(parent)
{
    qRegisterMetaType<NetplayRomData_t>();

    this->setupUi(this);

    // prepare web socket
    this->webSocket = webSocket;
    connect(this->webSocket, &QWebSocket::textMessageReceived, this, &CreateNetplaySessionDialog::on_webSocket_textMessageReceived);

    // prepare broadcast
    broadcastSocket.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    connect(&this->broadcastSocket, &QUdpSocket::readyRead, this, &CreateNetplaySessionDialog::on_broadcastSocket_readyRead);
    QByteArray multirequest;
    multirequest.append(1);
    broadcastSocket.writeDatagram(multirequest, QHostAddress::Broadcast, 45000);

    // change ok button name
    QPushButton* button = this->buttonBox->button(QDialogButtonBox::Ok);
    button->setText("Create");

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

    this->validateCreateButton();
}

CreateNetplaySessionDialog::~CreateNetplaySessionDialog(void)
{
}

QJsonObject CreateNetplaySessionDialog::GetSessionJson(void)
{
    return this->sessionJson;
}

QString CreateNetplaySessionDialog::GetSessionFile(void)
{
    return this->sessionFile;
}

void CreateNetplaySessionDialog::showErrorMessage(QString error, QString details)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Icon::Critical);
    msgBox.setWindowTitle("Error");
    msgBox.setText(error);
    msgBox.setDetailedText(details);
    msgBox.addButton(QMessageBox::Ok);
    msgBox.exec();
}

bool CreateNetplaySessionDialog::validate(void)
{
    if (this->nickNameLineEdit->text().isEmpty() ||
        this->nickNameLineEdit->text().contains(' ') ||
        this->nickNameLineEdit->text().size() > 256)
    {
        return false;
    }

    if (this->sessionNameLineEdit->text().isEmpty() ||
        this->sessionNameLineEdit->text().size() > 256)
    {
        return false;
    }

    if (this->listWidget->count() == 0 ||
        this->serverComboBox->count() == 0)
    {
        return false;
    }

    if (this->listWidget->currentItem() == nullptr)
    {
        return false;
    }

    return true;
}

void CreateNetplaySessionDialog::validateCreateButton(void)
{
    QPushButton* createButton = this->buttonBox->button(QDialogButtonBox::Ok);
    createButton->setEnabled(this->validate());
}

void CreateNetplaySessionDialog::on_webSocket_textMessageReceived(QString message)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = jsonDocument.object();

    if (json.value("type").toString() == "reply_create_room")
    {
        if (json.value("accept").toInt() == 0)
        {
            this->sessionJson = json;
            QDialog::accept();
        }
        else
        {
            this->showErrorMessage("Server Error", json.value("message").toString());
        }
    }
}

void CreateNetplaySessionDialog::on_broadcastSocket_readyRead()
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

void CreateNetplaySessionDialog::on_serverComboBox_currentIndexChanged(int index)
{
    if (index == -1)
    {
        return;
    }

    QString address = this->serverComboBox->itemData(index).toString();
    this->webSocket->open(QUrl(address));
}

void CreateNetplaySessionDialog::on_nickNameLineEdit_textChanged(void)
{
    this->validateCreateButton();
}

void CreateNetplaySessionDialog::on_sessionNameLineEdit_textChanged(void)
{
    this->validateCreateButton();
}

void CreateNetplaySessionDialog::on_passwordLineEdit_textChanged(void)
{
    this->validateCreateButton();
}

void CreateNetplaySessionDialog::on_listWidget_currentRowChanged(int index)
{
    this->validateCreateButton();
}

void CreateNetplaySessionDialog::accept()
{
    if (!this->webSocket->isValid())
    {
        this->showErrorMessage("Server Error", "Connection Failed");
        return;
    }

    // TODO?
    QPushButton* button = this->buttonBox->button(QDialogButtonBox::Ok);
    button->setEnabled(false);

    QListWidgetItem* item    = this->listWidget->currentItem();
    NetplayRomData_t romData = item->data(Qt::UserRole).value<NetplayRomData_t>();

    this->sessionFile = romData.File;

    QJsonObject json;
    json.insert("type", "request_create_room");
    json.insert("room_name", this->sessionNameLineEdit->text());
    json.insert("player_name", this->nickNameLineEdit->text());
    json.insert("password", this->passwordLineEdit->text());
    
    json.insert("MD5", romData.MD5);
    json.insert("game_name", romData.GoodName);
    // TODO
    json.insert("client_sha", "c96e1682c94027eeed662d07083d25ffab8309b8"); //QString::fromStdString(CoreGetVersion()));
    json.insert("netplay_version", NETPLAY_VER);

    // TODO: add common function...
    json.insert("emulator", "simple64");

    this->webSocket->sendTextMessage(QJsonDocument(json).toJson());
}
