#include <QFile>
#include <QDir>
#include "widget.h"
#include "character.h"
#include "message.h"

SceneEntity::SceneEntity()
{
    modelName = QString();
    id = 0;
    netviewId = 0;
    pos=UVector(0,0,0);
    rot=UQuaternion(0,0,0,0);
    sceneName = QString();
}

Pony::Pony() : SceneEntity()
{
    modelName = "PlayerBase";
    name = "";
}

Player::Player()
{
    connected=false;
    inGame=0;
    lastPingNumber=0;
    lastPingTime=timestampNow();
    port=0;
    IP=QString();
    receivedDatas = new QByteArray;
    pony = Pony();
    for (int i=0;i<33;i++)
        udpSequenceNumbers[i]=0;
    for (int i=0;i<33;i++)
        udpRecvSequenceNumbers[i]=0;
}

void Player::reset()
{
    name.clear();
    connected=false;
    inGame=0;
    lastPingNumber=0;
    lastPingTime=timestampNow();
    port=0;
    IP.clear();
    receivedDatas->clear();
    pony = Pony();
    for (int i=0;i<33;i++)
        udpSequenceNumbers[i]=0;
    for (int i=0;i<33;i++)
        udpRecvSequenceNumbers[i]=0;
}

bool Player::savePlayers(QList<Player> playersData)
{
    QFile playersFile("data/players/players.dat");
    if (!playersFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
    {
        win.logStatusMessage("Error saving players database");
        win.stopServer();
        return false;
    }

    for (int i=0;i<playersData.size();i++)
    {
        playersFile.write(playersData[i].name.toLatin1());
        playersFile.write("\31");
        playersFile.write(playersData[i].passhash.toLatin1());
        if (i+1!=playersData.size())
            playersFile.write("\n");
    }
    return true;
}

QList<Player> Player::loadPlayers()
{
    QList<Player> players;
    QFile playersFile("data/players/players.dat");
    if (!playersFile.open(QIODevice::ReadOnly))
    {
        win.logStatusMessage("Error reading players database");
        win.stopServer();
        return players;
    }
    QList<QByteArray> data = playersFile.readAll().split('\n');
    if (data.size()==1 && data[0].isEmpty())
    {
        win.logMessage("Player database is empty. Continuing happily");
        return players;
    }
    for (int i=0;i<data.size();i++)
    {
        QList<QByteArray> line = data[i].split('\31');
        if (line.size()!=2)
        {
            win.logStatusMessage("Error reading players database");
            win.stopServer();
            return players;
        }
        Player newPlayer;
        newPlayer.name = line[0];
        newPlayer.passhash = line[1];
        players << newPlayer;
    }
    win.logMessage(QString("Got ")+QString().setNum(players.size())+" players in database");
    return players;
}

Player& Player::findPlayer(QList<Player>& players, QString uname)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i].name == uname)
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return *emptyPlayer;
}

Player& Player::findPlayer(QList<Player>& players, QString uIP, quint16 uport)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i].IP == uIP && players[i].port == uport)
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return *emptyPlayer;
}

Player& Player::findPlayer(QList<Player>& players, quint16 netviewId)
{
    for (int i=0; i<players.size(); i++)
    {
        if (players[i].pony.netviewId == netviewId)
            return players[i];
    }

    Player* emptyPlayer = new Player();
    return *emptyPlayer;
}

void Player::savePonies(Player& player, QList<Pony> ponies)
{
    win.logMessage("UDP: Saving ponies for "+QString().setNum(player.pony.netviewId)+" ("+player.name+")");

    QDir playerPath(QDir::currentPath());
    playerPath.cd("data");
    playerPath.cd("players");
    playerPath.mkdir(player.name.toLatin1());

    QFile file(QDir::currentPath()+"/data/players/"+player.name.toLatin1()+"/ponies.dat");
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);
    for (int i=0; i<ponies.size(); i++)
    {
        file.write(ponies[i].ponyData);
        file.write(vectorToData(ponies[i].pos));
        file.write(stringToData(ponies[i].sceneName));
    }
}

QList<Pony> Player::loadPonies(Player& player)
{
    QList<Pony> ponies;
    QFile file(QDir::currentPath()+"/data/players/"+player.name.toLatin1()+"/ponies.dat");
    if (!file.open(QIODevice::ReadOnly))
        return ponies;

    QByteArray data = file.readAll();

    int i=0;
    while (i<data.size())
    {
        Pony pony;
        // Read the ponyData
        unsigned strlen;
        unsigned lensize=0;
        {
            unsigned char num3; int num=0, num2=0;
            do {
                num3 = data[i+lensize]; lensize++;
                num |= (num3 & 0x7f) << num2;
                num2 += 7;
            } while ((num3 & 0x80) != 0);
            strlen = (uint) num;
        }
        int ponyDataSize = strlen+lensize+43;
        pony.ponyData = data.mid(i,ponyDataSize);
        pony.name = dataToString(pony.ponyData); // The name is the first elem
        //win.logMessage("Found pony : "+pony.name);
        i+=ponyDataSize;

        // Read pos
        UVector pos = dataToVector(data.mid(i,12));
        pony.pos = pos;
        i+=12;

        // Read sceneName
        unsigned strlen2;
        unsigned lensize2=0;
        {
            unsigned char num3; int num=0, num2=0;
            do {
                num3 = data[i+lensize2]; lensize2++;
                num |= (num3 & 0x7f) << num2;
                num2 += 7;
            } while ((num3 & 0x80) != 0);
            strlen2 = (uint) num;
        }
        pony.sceneName = data.mid(i+lensize2, strlen2);
        i+=strlen2+lensize2;

        ponies << pony;
    }

    return ponies;
}

void Player::removePlayer(QList<Player>* players, QString uIP, quint16 uport)
{
    for (int i=0; i<players->size(); i++)
    {
        if ((*players)[i].IP == uIP && (*players)[i].port == uport)
            players->removeAt(i);
    }
}

void Player::disconnectPlayerCleanup(Player& player)
{
    // Save the pony
    QList<Pony> ponies = loadPonies(player);
    for (int i=0; i<ponies.size(); i++)
        if (ponies[i].ponyData == player.pony.ponyData)
            ponies[i] = player.pony;
    savePonies(player, ponies);

    QString uIP = player.IP;
    quint16 uPort = player.port;

    Scene* scene = findScene(player.pony.sceneName);
    if (scene->name.isEmpty())
        win.logMessage("UDP: Can't find scene for player cleanup");

    removePlayer(&(scene->players), uIP, uPort);
    for (int i=0; i<scene->players.size(); i++)
        sendNetviewRemove(scene->players[i], player.pony.netviewId);
    removePlayer(&(win.udpPlayers), uIP, uPort);
}

WearableItem::WearableItem()
{
    id=0;
    index=0;
}

InventoryItem::InventoryItem() : WearableItem()
{
    amount=0;
}
