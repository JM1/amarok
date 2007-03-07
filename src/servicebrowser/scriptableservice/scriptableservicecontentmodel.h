 /*
  Copyright (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/


#ifndef AMAROKSCRIPTABLESERVICECONTENTMODEL_H
#define AMAROKSCRIPTABLESERVICECONTENTMODEL_H

#include "scriptableservicecontentitem.h"
#include "../servicemodelbase.h"
#include <QMap>
#include <QModelIndex>
#include <QVariant>


class ScriptableServiceContentModel : public ServiceModelBase
{
    Q_OBJECT

private:

    mutable QMap<int, ScriptableServiceContentItem *> m_contentItemMap;  // the script refers to each node using an id number
    mutable int m_contentIndex;

    ScriptableServiceContentItem *m_rootContentItem;
    QString m_header;


public:

   

    ScriptableServiceContentModel(QObject *parent, QString header);
    ~ScriptableServiceContentModel();
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;

    int insertItem( QString name, QString url, QString infoHtml, int parentId );

    void requestHtmlInfo ( const QModelIndex & item ) const;

signals:

    void infoChanged( QString infoHtml ) const;


 };

 #endif