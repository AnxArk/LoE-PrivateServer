#ifndef QUEST_H
#define QUEST_H

#include <QString>
#include <QList>
#include "character.h"

// Quests are bound to one NPC
// They define the NPC, what he say, and the reactions.
struct Quest
{
public:
    Quest(QString path);

public:
    QList<QList<QString> > commands; // List of commands and their arguments, parsed from the quest file.
    quint16 eip; // Instruction pointer inside the commands.
    quint16 state; // State (progress) of the quest.
    quint16 id; // Unique id of the quest.
    QString name; // Name of the quest.
    QString descr; // Description of the quest.
    Pony* npc; // NPC of this quest's script
};

#endif // QUEST_H
