/* This file is part of the KDE project
   Copyright (C) 2007 Bart Cerneels <bart.cerneels@kde.org>
   Copyright (C) 2006 Mattias Fliesberg <mattias.fliesberg@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#ifndef XSPFPLAYLIST_H
#define XSPFPLAYLIST_H

#include <Playlist.h>
#include <EditablePlaylistCapability.h>

#include <QtXml>

class QTextStream;

/* convenience struct for internal use */
typedef struct {
    KUrl location;
    QString identifier;
    QString title;
    QString creator;
    QString annotation;
    KUrl info;
    KUrl image;
    QString album;
    uint trackNum;
    uint duration;
    KUrl link;
} XSPFtrack;

typedef QList<XSPFtrack> XSPFtrackList;

namespace Meta
{
/**
	@author Bart Cerneels <bart.cerneels@kde.org>
*/
class XSPFPlaylist : public Playlist, public QDomDocument, public EditablePlaylistCapability
{
public:
    XSPFPlaylist();
    XSPFPlaylist( QTextStream &stream );

    ~XSPFPlaylist();

    virtual QString name() const { return title(); }
    virtual QString prettyName() const { return title(); }

    /** returns all tracks in this playlist */
    TrackList tracks();

    /* convenience functions */
    QString title() const;
    QString creator();
    QString annotation();
    KUrl info();
    KUrl location();
    QString identifier();
    KUrl image();
    QDateTime date();
    KUrl license();
    KUrl::List attribution();
    KUrl link();

    /* EditablePlaylistCapability virtual functions */
    void setTitle( QString title );
    void setCreator( QString creator );
    void setAnnotation( QString annotation );
    void setInfo( KUrl info );
    void setLocation( KUrl location );
    void setIdentifier( QString identifier );
    void setImage( KUrl image );
    void setDate( QDateTime date );
    void setLicense( KUrl license );
    void setAttribution( KUrl attribution, bool append = true );
    void setLink( KUrl link );
    void setTrackList( TrackList trackList, bool append = false );

    //TODO: implement these
    void beginMetaDataUpdate() {};
    void endMetaDataUpdate() {};
    void abortMetaDataUpdate() {};

    bool isEditable() const { return true; };

    /* Meta::Playlist virtual functions */
    bool hasCapabilityInterface( Capability::Type type ) const;

    Capability* asCapabilityInterface( Capability::Type type );

private:
    XSPFtrackList trackList();
    bool loadXSPF( QTextStream& );
};

}
#endif
