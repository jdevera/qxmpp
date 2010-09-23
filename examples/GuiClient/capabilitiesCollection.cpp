/*
 * Copyright (C) 2008-2010 The QXmpp developers
 *
 * Author:
 *	Manjeet Dahiya
 *
 * Source:
 *	http://code.google.com/p/qxmpp
 *
 * This file is a part of QXmpp library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */


#include "capabilitiesCollection.h"

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include <utils.h>

#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QDir>

capabilitiesCollection::capabilitiesCollection(QXmppClient* client) :
    QObject(client), m_client(client)
{
    QXmppDiscoveryManager* ext = m_client->findExtension<QXmppDiscoveryManager>();
    if(ext)
    {
        bool check = connect(ext, SIGNAL(infoReceived(const QXmppDiscoveryIq&)),
                             SLOT(infoReceived(const QXmppDiscoveryIq&)));
        Q_ASSERT(check);
    }
}

bool capabilitiesCollection::isCapabilityAvailable(const QString& nodeVer)
{
    return m_mapCapabilities.contains(nodeVer);
}

void capabilitiesCollection::requestInfo(const QString& jid, const QString& node)
{
    QXmppDiscoveryManager* ext = m_client->findExtension<QXmppDiscoveryManager>();
    if(ext)
    {
        bool alreadyRequested = false;
        foreach(QString key, m_mapIdNodeVer.keys())
        {
            if(m_mapIdNodeVer[key] == node)
            {
                alreadyRequested = true;
                break;
            }
        }

        if(!alreadyRequested)
        {
            QString id = ext->requestInfo(jid, node);
            m_mapIdNodeVer[id] = node;
        }
    }
}

void capabilitiesCollection::infoReceived(const QXmppDiscoveryIq& discoIqRcv)
{
    QXmppDiscoveryIq discoIq = discoIqRcv;
    if(discoIq.queryType() == QXmppDiscoveryIq::InfoQuery &&
       discoIq.type() == QXmppIq::Result)
    {
        if(discoIq.queryNode().isEmpty())
        {
            discoIq.setQueryNode(m_mapIdNodeVer[discoIq.id()]);
            m_mapIdNodeVer.remove(discoIq.id());
        }

        discoIq.setTo("");
        discoIq.setFrom("");
        discoIq.setId("");
        m_mapCapabilities[discoIq.queryNode()] = discoIq;
        saveToCache(discoIq.queryNode());
    }
}

void capabilitiesCollection::loadAllFromCache()
{
    m_mapCapabilities.clear();

    QDir dirCaps(getSettingsDir(m_client->configuration().jidBare())+ "capabilities/");
    if(dirCaps.exists())
    {
        QStringList list = dirCaps.entryList(QStringList("*.xml"));
        foreach(QString fileName, list)
        {
            QFile file(getSettingsDir(m_client->configuration().jidBare())+ "capabilities/" + fileName);
            if(file.open(QIODevice::ReadOnly))
            {
                QDomDocument doc;
                if(doc.setContent(&file, true))
                {
                    QXmppDiscoveryIq discoIq;
                    discoIq.parse(doc.documentElement());
                    m_mapCapabilities[discoIq.queryNode()] = discoIq;
                    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
            }
        }
    }
}

void capabilitiesCollection::saveToCache(const QString& nodeVer)
{
    if(!m_mapCapabilities.contains(nodeVer))
        return;

    QString fileName = getImageHash(nodeVer.toUtf8());
    QDir dir;
    if(!dir.exists(getSettingsDir(m_client->configuration().jidBare())))
        dir.mkpath(getSettingsDir(m_client->configuration().jidBare()));

    QDir dir2;
    if(!dir2.exists(getSettingsDir(m_client->configuration().jidBare())+ "capabilities/"))
        dir2.mkpath(getSettingsDir(m_client->configuration().jidBare())+ "capabilities/");

    QString fileCapability = getSettingsDir(m_client->configuration().jidBare()) + "capabilities/" + fileName + ".xml";
    QFile file(fileCapability);

    if(file.open(QIODevice::ReadWrite))
    {
        QXmlStreamWriter stream(&file);
        m_mapCapabilities[nodeVer].toXml(&stream);
        file.close();
    }
}

QStringList capabilitiesCollection::getFeatures(const QString& nodeVer)
{
    if(!m_mapCapabilities.contains(nodeVer))
        return QStringList();

    return m_mapCapabilities[nodeVer].features();
}

QStringList capabilitiesCollection::getIdentities(const QString& nodeVer)
{
    if(!m_mapCapabilities.contains(nodeVer))
        return QStringList();

    QStringList idList;
    QList<QXmppDiscoveryIq::Identity> list = m_mapCapabilities[nodeVer].identities();
    foreach(QXmppDiscoveryIq::Identity identity, list)
    {
        QStringList tmpList;
        if(!identity.name().isEmpty())
            tmpList << identity.name();
        if(!identity.category().isEmpty())
            tmpList << identity.category();
        if(!identity.type().isEmpty())
            tmpList << identity.type();
        if(!identity.language().isEmpty())
            tmpList << identity.language();
        idList << tmpList.join(" | ");
    }
    return idList;
}

