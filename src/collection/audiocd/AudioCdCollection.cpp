/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Seb Ruiz <ruiz@kde.org>                                           *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "AudioCdCollection.h"

#include "amarokconfig.h"
#include "AudioCdCollectionCapability.h"
#include "AudioCdCollectionLocation.h"
#include "AudioCdMeta.h"
#include "collection/CollectionManager.h"
#include "collection/support/MemoryQueryMaker.h"
#include "covermanager/CoverFetcher.h"
#include "core/support/Debug.h"
#include "MediaDeviceMonitor.h"
#include "MemoryQueryMaker.h"
#include "SvgHandler.h"
#include "handler/AudioCdHandler.h"
#include "support/AudioCdConnectionAssistant.h"
#include "support/AudioCdDeviceInfo.h"

#include <kio/job.h>
#include <kio/netaccess.h>

#include <solid/device.h>
#include <solid/opticaldrive.h>

#include <KConfigGroup>
#include <KEncodingProber>

#include <KSharedConfig>

#include <QDir>
#include <QTextCodec>

using namespace Collections;

AMAROK_EXPORT_COLLECTION( AudioCdCollectionFactory, audiocdcollection )

AudioCdCollectionFactory::AudioCdCollectionFactory( QObject *parent, const QVariantList &args )
    : MediaDeviceCollectionFactory<AudioCdCollection>( new AudioCdConnectionAssistant() )
{
    setParent( parent );
    Q_UNUSED( args );
}


AudioCdCollection::AudioCdCollection( MediaDeviceInfo* info )
   : MediaDeviceCollection()
   , m_encodingFormat( OGG )
   , m_ready( false )
{
    DEBUG_BLOCK

    debug() << "Getting Audio CD info";
    AudioCdDeviceInfo *cdInfo = qobject_cast<AudioCdDeviceInfo *>( info );
    m_udi = cdInfo->udi();

    readAudioCdSettings();

    m_ejectAction = new QAction( KIcon( "media-eject" ), i18n( "&Eject" ), 0 );
    m_ejectAction->setProperty( "popupdropper_svg_id", "eject" );

    connect( m_ejectAction, SIGNAL( triggered() ), this, SLOT( eject() ) );

    m_handler = new Meta::AudioCdHandler( this );
}


AudioCdCollection::~AudioCdCollection()
{
}

void
AudioCdCollection::readCd()
{
    DEBUG_BLOCK

    //get the CDDB info file if possible.
    m_cdInfoJob =  KIO::storedGet( KUrl( "audiocd:/Information/CDDB Information.txt" ), KIO::NoReload, KIO::HideProgressInfo );
    connect( m_cdInfoJob, SIGNAL( result( KJob * ) )
            , this, SLOT( infoFetchComplete( KJob *) ) );

}

void
AudioCdCollection::infoFetchComplete( KJob *job )
{
    DEBUG_BLOCK
    if( job->error() )
    {
        error() << job->error();
        m_cdInfoJob->deleteLater();
        noInfoAvailable();
    }
    else
    {
        QString cddbInfo = m_cdInfoJob->data();

        KEncodingProber prober;
        KEncodingProber::ProberState result = prober.feed( m_cdInfoJob->data() );
        if( result != KEncodingProber::NotMe )
           cddbInfo = QTextCodec::codecForName( prober.encoding() )->toUnicode( m_cdInfoJob->data() );

        debug() << "Encoding: " << prober.encoding();
        debug() << "got cddb info: " << cddbInfo;

        int startIndex;
        int endIndex;

        QString artist;
        QString album;
        QString year;
        QString genre;

        startIndex = cddbInfo.indexOf( "DTITLE=", 0 );
        if ( startIndex != -1 )
        {
            startIndex += 7;
            endIndex = cddbInfo.indexOf( "\n", startIndex );
            QString compoundTitle = cddbInfo.mid( startIndex, endIndex - startIndex );

            debug() << "compoundTitle: " << compoundTitle;

            QStringList compoundTitleList = compoundTitle.split( " / " );

            artist = compoundTitleList.at( 0 );
            album = compoundTitleList.at( 1 );
        }

        Meta::AudioCdArtistPtr artistPtr = Meta::AudioCdArtistPtr( new  Meta::AudioCdArtist( artist ) );
        memoryCollection()->addArtist( Meta::ArtistPtr::staticCast( artistPtr ) );
        Meta::AudioCdAlbumPtr albumPtr = Meta::AudioCdAlbumPtr( new Meta::AudioCdAlbum( album ) );
        albumPtr->setAlbumArtist( artistPtr );
        memoryCollection()->addAlbum( Meta::AlbumPtr::staticCast( albumPtr ) );


        startIndex = cddbInfo.indexOf( "DYEAR=", 0 );
        if ( startIndex != -1 )
        {
            startIndex += 6;
            endIndex = cddbInfo.indexOf( "\n", startIndex );
            year = cddbInfo.mid( startIndex, endIndex - startIndex );
        }

        Meta::AudioCdYearPtr yearPtr = Meta::AudioCdYearPtr( new Meta::AudioCdYear( year ) );
        memoryCollection()->addYear( Meta::YearPtr::staticCast( yearPtr ) );


        startIndex = cddbInfo.indexOf( "DGENRE=", 0 );
        if ( startIndex != -1 )
        {
            startIndex += 7;
            endIndex = cddbInfo.indexOf( "\n", startIndex );
            genre = cddbInfo.mid( startIndex, endIndex - startIndex );
        }

        Meta::AudioCdGenrePtr genrePtr = Meta::AudioCdGenrePtr( new Meta::AudioCdGenre( genre ) );
        memoryCollection()->addGenre( Meta::GenrePtr::staticCast( genrePtr ) );

        m_discCddbId = "unknown";

        startIndex = cddbInfo.indexOf( "DISCID=", 0 );
        if ( startIndex != -1 )
        {
            startIndex += 7;
            endIndex = cddbInfo.indexOf( "\n", startIndex );
            m_discCddbId = cddbInfo.mid( startIndex, endIndex - startIndex );
        }

        //MediaDeviceMonitor::instance()->setCurrentCdId( m_discCddbId );

        //get the list of tracknames
        startIndex = cddbInfo.indexOf( "TTITLE0=", 0 );
        if ( startIndex != -1 )
        {
            endIndex = cddbInfo.indexOf( "\nEXTD=", startIndex );
            QString tracksBlock = cddbInfo.mid( startIndex, endIndex - startIndex );
            debug() << "Tracks block: " << tracksBlock;
            QStringList tracksBlockList = tracksBlock.split( '\n' );

            int numberOfTracks = tracksBlockList.count();

            for ( int i = 0; i < numberOfTracks; i++ )
            {
                QString prefix = "TTITLE" + QString::number( i ) + '=';
                debug() << "prefix: " << prefix;
                QString trackName = tracksBlockList.at( i );
                trackName = trackName.remove( prefix );

                QString trackArtist;
                //check if a track artist is included in the track name:

                if ( trackName.contains( " / " ) )
                {
                    QStringList trackArtistList = trackName.split( " / " );
                    trackName = trackArtistList.at( 1 );
                    trackArtist = trackArtistList.at( 0 );

                }

                debug() << "Track name: " << trackName;

                QString padding = i < 10 ? "0" : QString();

                QString baseFileName = m_fileNamePattern;
                debug() << "Track Base File Name (before): " << baseFileName;

                baseFileName.replace( "%{title}", trackName, Qt::CaseInsensitive );
                baseFileName.replace( "%{number}", padding  + QString::number( i + 1 ), Qt::CaseInsensitive );
                baseFileName.replace( "%{albumtitle}", album, Qt::CaseInsensitive );
                baseFileName.replace( "%{trackartist}", trackArtist, Qt::CaseInsensitive );
                baseFileName.replace( "%{albumartist}", artist, Qt::CaseInsensitive );
                baseFileName.replace( "%{year}", year, Qt::CaseInsensitive );
                baseFileName.replace( "%{genre}", genre, Qt::CaseInsensitive );

                //we hack the url so the engine controller knows what track on the CD to play..
                QString baseUrl = "audiocd:/" + m_discCddbId + '/' + QString::number( i + 1 );

                debug() << "Track Base File Name (after): " << baseFileName;
                debug() << "Track url: " << baseUrl;

                Meta::AudioCdTrackPtr trackPtr = Meta::AudioCdTrackPtr( new Meta::AudioCdTrack( this, trackName, baseUrl ) );

                trackPtr->setTrackNumber( i + 1 );
                trackPtr->setFileNameBase( baseFileName );

                memoryCollection()->addTrack( Meta::TrackPtr::staticCast( trackPtr ) );

                artistPtr->addTrack( trackPtr );

                if ( trackArtist.isEmpty() )
                    trackPtr->setArtist( artistPtr );
                else
                {
                    albumPtr->setIsCompilation( true );

                    Meta::AudioCdArtistPtr trackArtistPtr = Meta::AudioCdArtistPtr( new Meta::AudioCdArtist( trackArtist ) );
                    trackArtistPtr->addTrack( trackPtr );
                    trackPtr->setArtist( trackArtistPtr );
                }

                albumPtr->addTrack( trackPtr );
                trackPtr->setAlbum( albumPtr );

                genrePtr->addTrack( trackPtr );
                trackPtr->setGenre( genrePtr );

                yearPtr->addTrack( trackPtr );
                trackPtr->setYear( yearPtr );

            }
        }

        //lets see if we can find a cover for the album:
        if( AmarokConfig::autoGetCoverArt() )
            The::coverFetcher()->queueAlbum( Meta::AlbumPtr::staticCast( albumPtr ) );

    }

    emit ( updated() );
    updateProxyTracks();

    m_ready = true;

    //be nice and check if MainWindow is just aching for an audio cd to start playing
    if( The::mainWindow()->isWaitingForCd() )
    {
        debug() << "Tell MainWindow to start playing us immediately.";
        The::mainWindow()->playAudioCd();
    }
}

QString
AudioCdCollection::collectionId() const
{
    return "AudioCd";
}

QString
AudioCdCollection::prettyName() const
{
    return "Audio CD";
}

KIcon
AudioCdCollection::icon() const
{
    return KIcon( "media-optical-audio");
}

void
AudioCdCollection::cdRemoved()
{
    emit remove();
}

QString
AudioCdCollection::encodingFormat() const
{
    switch( m_encodingFormat )
    {
        case WAV:
            return "wav";
        case FLAC:
            return "flac";
        case OGG:
            return "ogg";
        case MP3:
            return "mp3";
    }
    return QString();
}

QString
AudioCdCollection::copyableBasePath() const
{
    switch( m_encodingFormat )
    {
        case WAV:
            return "audiocd:/";
        case FLAC:
            return "audiocd:/FLAC/";
        case OGG:
            return "audiocd:/Ogg Vorbis/";
        case MP3:
            return "audiocd:/MP3/";
    }
    return QString();
}

void
AudioCdCollection::setEncodingFormat( int format ) const
{
    m_encodingFormat = format;
}

CollectionLocation *
AudioCdCollection::location() const
{
    return new AudioCdCollectionLocation( this );
}

void
AudioCdCollection::eject()
{
    DEBUG_BLOCK

    //we need to do a quick check if we are currently playing from this cd, if so, stop playback and then eject
    Meta::TrackPtr track = The::engineController()->currentTrack();

    if ( track )
    {
        if( track->playableUrl().url().startsWith( "audiocd:/" ) )
            The::engineController()->stop();
    }

    Solid::Device device = Solid::Device( m_udi );
    
    Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();
    if( drive )
        drive->eject();
    else
        debug() << "disc has no drive";
}

QAction *
AudioCdCollection::ejectAction() const
{
    return m_ejectAction;
}

bool
AudioCdCollection::hasCapabilityInterface( Capabilities::Capability::Type type ) const
{
    switch( type )
    {
        case Capabilities::Capability::Collection:
        case Capabilities::Capability::Decorator:
            return true;

        default:
            return false;
    }
}

Capabilities::Capability *
AudioCdCollection::asCapabilityInterface( Capabilities::Capability::Type type )
{
    if ( type == Capabilities::Capability::Collection )
        return new Capabilities::AudioCdCollectionCapability( this );
    return 0;
}

void
AudioCdCollection::noInfoAvailable()
{
    DEBUG_BLOCK

    m_discCddbId = "unknown";

    //MediaDeviceMonitor::instance()->setCurrentCdId( m_discCddbId );

    QString artist = i18n( "Unknown" );
    QString album = i18n( "Unknown" );
    QString year = i18n( "Unknown" );
    QString genre = i18n( "Unknown" );

    Meta::AudioCdArtistPtr artistPtr = Meta::AudioCdArtistPtr( new Meta::AudioCdArtist( artist ) );
    memoryCollection()->addArtist( Meta::ArtistPtr::staticCast( artistPtr ) );
    Meta::AudioCdAlbumPtr albumPtr = Meta::AudioCdAlbumPtr( new Meta::AudioCdAlbum( album ) );
    albumPtr->setAlbumArtist( artistPtr );
    memoryCollection()->addAlbum( Meta::AlbumPtr::staticCast( albumPtr ) );
    Meta::AudioCdYearPtr yearPtr = Meta::AudioCdYearPtr( new Meta::AudioCdYear( year ) );
    memoryCollection()->addYear( Meta::YearPtr::staticCast( yearPtr ) );
    Meta::AudioCdGenrePtr genrePtr = Meta::AudioCdGenrePtr( new Meta::AudioCdGenre( genre ) );
    memoryCollection()->addGenre( Meta::GenrePtr::staticCast( genrePtr ) );


    int i = 1;
    QString prefix( "0" );
    QString trackName = "Track " + prefix + QString::number( i );

    while( KIO::NetAccess::exists( "audiocd:/" + trackName + ".wav", KIO::NetAccess::SourceSide,0 ) )
    {
        debug() << "got track: " << "audiocd:/" + trackName + ".wav";

        QString baseUrl = "audiocd:/" + m_discCddbId + '/' + QString::number( i );

        Meta::AudioCdTrackPtr trackPtr = Meta::AudioCdTrackPtr( new Meta::AudioCdTrack( this, trackName, baseUrl ) );

        trackPtr->setTrackNumber( i );
        trackPtr->setFileNameBase( trackName );

        memoryCollection()->addTrack( Meta::TrackPtr::staticCast( trackPtr ) );

        artistPtr->addTrack( trackPtr );
        trackPtr->setArtist( artistPtr );

        albumPtr->addTrack( trackPtr );
        trackPtr->setAlbum( albumPtr );

        genrePtr->addTrack( trackPtr );
        trackPtr->setGenre( genrePtr );

        yearPtr->addTrack( trackPtr );
        trackPtr->setYear( yearPtr );

        i++;
        prefix = i < 10 ? "0" : "";
        trackName = "Track " + prefix + QString::number( i );
    }

    emit ( updated() );
    updateProxyTracks();

    m_ready = true;

}

void
AudioCdCollection::readAudioCdSettings()
{
    KSharedConfigPtr conf = KSharedConfig::openConfig( "kcmaudiocdrc" );
    KConfigGroup filenameConf = conf->group( "FileName" );

    m_fileNamePattern = filenameConf.readEntry( "file_name_template", "%{trackartist} - %{number} - %{title}" );
    m_albumNamePattern = filenameConf.readEntry( "album_name_template", "%{albumartist} - %{albumtitle}" );
}

bool
AudioCdCollection::possiblyContainsTrack(const KUrl & url) const
{
    DEBUG_BLOCK;
    debug() << "match: " << url.url().startsWith( "audiocd:/" );

    return url.url().startsWith( "audiocd:/" );
}

Meta::TrackPtr
AudioCdCollection::trackForUrl( const KUrl & url )
{
    DEBUG_BLOCK;

    debug() << "Disk id: " << m_discCddbId;

    if ( !m_discCddbId.isEmpty() )
    {

        QString urlString = url.url().remove( "audiocd:/" );

        QStringList parts = urlString.split( '/' );

        if ( parts.count() != 2 )
            return Meta::TrackPtr();

        QString discId = parts.at( 0 );

        if ( discId != m_discCddbId )
            return Meta::TrackPtr();

        int trackNumber = parts.at( 1 ).toInt();

        foreach( Meta::TrackPtr track, memoryCollection()->trackMap().values() )
        {
            if ( track->trackNumber() == trackNumber )
                return track;
        }

        return Meta::TrackPtr();

    }
    else
    {
        if ( m_proxyMap.contains( url ) )
        {
            return Meta::TrackPtr( m_proxyMap.value( url ) );
        }
        else
        {
            MetaProxy::Track* ptrack = new MetaProxy::Track( url.url(), true );
            m_proxyMap.insert( url, ptrack );
            return Meta::TrackPtr( ptrack );
        }
    }

}

void
AudioCdCollection::updateProxyTracks()
{
    foreach( const KUrl &url, m_proxyMap.keys() )
    {

        const QString &urlString = url.url().remove( "audiocd:/" );
        const QStringList &parts = urlString.split( '/' );

        if( parts.count() != 2 )
            continue;

        const QString &discId = parts.at( 0 );

        if( discId != m_discCddbId )
            continue;

        const int trackNumber = parts.at( 1 ).toInt();

        foreach( const Meta::TrackPtr &track, memoryCollection()->trackMap().values() )
        {
            if( track->trackNumber() == trackNumber )
            {
                m_proxyMap.value( url )->updateTrack( track );
            }
        }

    }

    m_proxyMap.clear();
}

void AudioCdCollection::startFullScan()
{
    DEBUG_BLOCK
    readCd();
    emit collectionReady( this );
}

bool AudioCdCollection::isReady()
{
    return m_ready;
}



#include "AudioCdCollection.moc"

