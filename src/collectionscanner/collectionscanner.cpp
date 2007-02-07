/***************************************************************************
 *   Copyright (C) 2003-2005 by The Amarok Developers                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/

#define DEBUG_PREFIX "CollectionScanner"

#include "amarok.h"
#include "collectionscanner.h"
#include "collectionscannerdbushandler.h"
#include "debug.h"

#include <cerrno>
#include <iostream>

#include <dirent.h>    //stat
#include <limits.h>    //PATH_MAX
#include <stdlib.h>    //realpath

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>

#include <qdom.h>
#include <QFile>
#include <QTimer>
//Added by qt3to4:
#include <Q3CString>
#include <Q3ValueList>

#include <kglobal.h>
#include <klocale.h>


CollectionScanner::CollectionScanner( const QStringList& folders,
                                      bool recursive,
                                      bool incremental,
                                      bool importPlaylists,
                                      bool restart )
        : KApplication( /*allowStyles*/ false, /*GUIenabled*/ false )
        , m_importPlaylists( importPlaylists )
        , m_folders( folders )
        , m_recursively( recursive )
        , m_incremental( incremental )
        , m_restart( restart )
        , m_logfile( Amarok::saveLocation( QString::null ) + "collection_scan.log"  )
        , m_pause( false )
{
    DbusCollectionScannerHandler* dcsh = new DbusCollectionScannerHandler();
    connect( dcsh, SIGNAL(pauseRequest()), this, SLOT(pause()) );
    connect( dcsh, SIGNAL(unpauseRequest()), this, SLOT(resume()) );
    kapp->setName( QString( "amarokcollectionscanner" ).ascii() );
    if( !restart )
        QFile::remove( m_logfile );

    QTimer::singleShot( 0, this, SLOT( doJob() ) );
}


CollectionScanner::~CollectionScanner()
{
    DEBUG_BLOCK
}

void
CollectionScanner::pause()
{
    DEBUG_BLOCK
    m_pause = true;
}

void
CollectionScanner::resume()
{
    DEBUG_BLOCK
    m_pause = false;
}



void
CollectionScanner::doJob() //SLOT
{
    std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>";
    std::cout << "<scanner>";


    QStringList entries;

    if( m_restart ) {
        QFile logFile( m_logfile );
        QString lastFile;
        if ( !logFile.open( QIODevice::ReadOnly ) )
            warning() << "Failed to open log file " << logFile.name() << " read-only"
            << endl;
        else {
            Q3TextStream logStream;
            logStream.setDevice(&logFile);
            logStream.setEncoding(Q3TextStream::UnicodeUTF8);
            lastFile = logStream.read();
            logFile.close();
        }

        QFile folderFile( Amarok::saveLocation( QString::null ) + "collection_scan.files"   );
        if ( !folderFile.open( QIODevice::ReadOnly ) )
            warning() << "Failed to open folder file " << folderFile.name()
            << " read-only" << endl;
        else {
            Q3TextStream folderStream;
            folderStream.setDevice(&folderFile);
            folderStream.setEncoding(Q3TextStream::UnicodeUTF8);
            entries = QStringList::split( "\n", folderStream.read() );
        }

        for( int count = entries.findIndex( lastFile ) + 1; count; --count )
            entries.pop_front();

    }
    else {
        oldForeachType( QStringList, m_folders ) {
            if( (*it).isEmpty() )
                //apparently somewhere empty strings get into the mix
                //which results in a full-system scan! Which we can't allow
                continue;

            QString dir = *it;
            if( !dir.endsWith( "/" ) )
                dir += '/';

            readDir( dir, entries );
        }

        QFile folderFile( Amarok::saveLocation( QString::null ) + "collection_scan.files"   );
        folderFile.open( QIODevice::WriteOnly );
        Q3TextStream stream( &folderFile );
        stream.setEncoding(Q3TextStream::UnicodeUTF8);
        stream << entries.join( "\n" );
        folderFile.close();
    }

    if( !entries.isEmpty() ) {
        if( !m_restart ) {
            AttributeMap attributes;
            attributes["count"] = QString::number( entries.count() );
            writeElement( "itemcount", attributes );
        }

        scanFiles( entries );
    }

    std::cout << "</scanner>" << std::endl;

    quit();
}


void
CollectionScanner::readDir( const QString& dir, QStringList& entries )
{
    static DCOPRef dcopRef( "amarok", "collection" );

    // linux specific, but this fits the 90% rule
    if( dir.startsWith( "/dev" ) || dir.startsWith( "/sys" ) || dir.startsWith( "/proc" ) )
        return;

    const Q3CString dir8Bit = QFile::encodeName( dir );
    DIR *d = opendir( dir8Bit );
    if( d == NULL ) {
        warning() << "Skipping, " << strerror(errno) << ": " << dir << endl;
        return;
    }
    int dfd = dirfd(d);
    if (dfd == -1) {
	warning() << "Skipping, unable to obtain file descriptor: " << dir << endl;
	closedir(d);
	return;
    }

    struct stat statBuf;
    struct stat statBuf_symlink;
    fstat( dfd, &statBuf );

    struct direntry de;
    memset(&de, 0, sizeof(struct direntry));
    de.dev = statBuf.st_dev;
    de.ino = statBuf.st_ino;

    int f = -1;
#if __GNUC__ < 4
    for( unsigned int i = 0; i < m_processedDirs.size(); ++i )
        if( memcmp( &m_processedDirs[i], &de, sizeof( direntry ) ) == 0 ) {
            f = i; break;
        }
#else
    f = m_processedDirs.find( de );
#endif

    if ( ! S_ISDIR( statBuf.st_mode ) || f != -1 ) {
        debug() << "Skipping, already scanned: " << dir << endl;
        closedir(d);
        return;
    }

    AttributeMap attributes;
    attributes["path"] = dir;
    writeElement( "folder", attributes );

    m_processedDirs.resize( m_processedDirs.size() + 1 );
    m_processedDirs[m_processedDirs.size() - 1] = de;

    for( dirent *ent; ( ent = readdir( d ) ); ) {
        Q3CString entry (ent->d_name);
        Q3CString entryname (ent->d_name);

        if ( entry == "." || entry == ".." )
            continue;

        entry.prepend( dir8Bit );

        if ( stat( entry, &statBuf ) != 0 )
            continue;
        if ( lstat( entry, &statBuf_symlink ) != 0 )
            continue;

        // loop protection
        if ( ! ( S_ISDIR( statBuf.st_mode ) || S_ISREG( statBuf.st_mode ) ) )
            continue;

        if ( S_ISDIR( statBuf.st_mode ) && m_recursively && entry.length() && entryname[0] != '.' )
        {
            if ( S_ISLNK( statBuf_symlink.st_mode ) ) {
                char nosymlink[PATH_MAX];
                if ( realpath( entry, nosymlink ) ) {
                    debug() << entry << " is a symlink. Using: " << nosymlink << endl;
                    entry = nosymlink;
                }
            }
            const QString file = QFile::decodeName( entry );

            bool isInCollection = false;
            if( m_incremental )
                dcopRef.call( "isDirInCollection", file ).get( isInCollection );

            if( !m_incremental || !isInCollection )
                // we MUST add a '/' after the dirname
                readDir( file + '/', entries );
        }

        else if( S_ISREG( statBuf.st_mode ) )
            entries.append( QFile::decodeName( entry ) );
    }

    closedir( d );
}


void
CollectionScanner::scanFiles( const QStringList& entries )
{
    DEBUG_BLOCK

    typedef QPair<QString, QString> CoverBundle;

    QStringList validImages;    validImages    << "jpg" << "png" << "gif" << "jpeg";
    QStringList validPlaylists; validPlaylists << "m3u" << "pls";

    Q3ValueList<CoverBundle> covers;
    QStringList images;

    int itemCount = 0;

    DCOPRef dcopRef( "amarok", "collection" );

    oldForeachType( QStringList, entries ) {
        const QString path = *it;
        const QString ext  = extension( path );
        const QString dir  = directory( path );

        itemCount++;

        // Write path to logfile
        if( !m_logfile.isEmpty() ) {
            QFile log( m_logfile );
            if( log.open( QIODevice::WriteOnly ) ) {
                Q3CString cPath = path.utf8();
                log.writeBlock( cPath, cPath.length() );
                log.close();
            }
        }

        if( validImages.contains( ext ) )
            images += path;

        else if( m_importPlaylists && validPlaylists.contains( ext ) ) {
            AttributeMap attributes;
            attributes["path"] = path;
            writeElement( "playlist", attributes );
        }

        else {
            MetaBundle::EmbeddedImageList images;
            MetaBundle mb( KUrl::fromPathOrUrl( path ), true, TagLib::AudioProperties::Fast, &images );
            const AttributeMap attributes = readTags( mb );

            if( !attributes.empty() ) {
                writeElement( "tags", attributes );

                CoverBundle cover( attributes["artist"], attributes["album"] );

                if( !covers.contains( cover ) )
                    covers += cover;

                oldForeachType( MetaBundle::EmbeddedImageList, images ) {
                    AttributeMap attributes;
                    attributes["path"] = path;
                    attributes["hash"] = (*it).hash();
                    attributes["description"] = (*it).description();
                    writeElement( "embed", attributes );
                }
            }
        }

        // Update Compilation-flag, when this is the last loop-run
        // or we're going to switch to another dir in the next run
        QStringList::ConstIterator itTemp( it );
        ++itTemp;
        if( path == entries.last() || dir != directory( *itTemp ) )
        {
            // we entered the next directory
            oldForeachType( QStringList, images ) {
                // Serialize CoverBundle list with AMAROK_MAGIC as separator
                QString string;

                for( Q3ValueList<CoverBundle>::ConstIterator it2 = covers.begin(); it2 != covers.end(); ++it2 )
                    string += (*it2).first + "AMAROK_MAGIC" + (*it2).second + "AMAROK_MAGIC";

                AttributeMap attributes;
                attributes["path"] = *it;
                attributes["list"] = string;
                writeElement( "image", attributes );
            }

            AttributeMap attributes;
            attributes["path"] = dir;
            writeElement( "compilation", attributes );

            // clear now because we've processed them
            covers.clear();
            images.clear();
        }

        if( itemCount % 20 == 0 )
        {
            kapp->processEvents();  // let DCOP through!
            if( m_pause )
            {
                dcopRef.send( "scannerAcknowledged" );
                while( m_pause )
                {
                    sleep( 1 );
                    kapp->processEvents();
                }
                dcopRef.send( "scannerAcknowledged" );
            }
        }

    }
}


AttributeMap
CollectionScanner::readTags( const MetaBundle& mb )
{
    // Tests reveal the following:
    //
    // TagLib::AudioProperties   Relative Time Taken
    //
    //  No AudioProp Reading        1
    //  Fast                        1.18
    //  Average                     Untested
    //  Accurate                    Untested

    AttributeMap attributes;

    if ( !mb.isValidMedia() ) {
        std::cout << "<dud/>";
        return attributes;
    }

    attributes["path"]    = mb.url().path();
    attributes["title"]   = mb.title();
    attributes["artist"]  = mb.artist();
    attributes["composer"]= mb.composer();
    attributes["album"]   = mb.album();
    attributes["comment"] = mb.comment();
    attributes["genre"]   = mb.genre();
    attributes["year"]    = mb.year() ? QString::number( mb.year() ) : QString();
    attributes["track"]   = mb.track() ? QString::number( mb.track() ) : QString();
    attributes["discnumber"]   = mb.discNumber() ? QString::number( mb.discNumber() ) : QString();
    attributes["bpm"]   = mb.bpm() ? QString::number( mb.bpm() ) : QString();
    attributes["filetype"]  = QString::number( mb.fileType() );
    attributes["uniqueid"] = mb.uniqueId();
    attributes["compilation"] = QString::number( mb.compilation() );

    if ( mb.audioPropertiesUndetermined() )
        attributes["audioproperties"] = "false";
    else {
        attributes["audioproperties"] = "true";
        attributes["bitrate"]         = QString::number( mb.bitrate() );
        attributes["length"]          = QString::number( mb.length() );
        attributes["samplerate"]      = QString::number( mb.sampleRate() );
    }

    if ( mb.filesize() >= 0 )
        attributes["filesize"] = QString::number( mb.filesize() );

    return attributes;
}


void
CollectionScanner::writeElement( const QString& name, const AttributeMap& attributes )
{
    QDomDocument doc; // A dummy. We don't really use DOM, but SAX2
    QDomElement element = doc.createElement( name );

    oldForeachType( AttributeMap, attributes )
    {
        // There are at least some characters that Qt cannot categorize which make the resulting
        // xml document ill-formed and prevent the parser from processing the remaining document.
        // Because of this we skip attributes containing characters not belonging to any category.
        QString data = it.data();
        const unsigned len = data.length();
        bool nonPrint = false;
        for( unsigned i = 0; i < len; i++ )
        {
            if( data.ref( i ).category() == QChar::NoCategory )
            {
                nonPrint = true;
                break;
            }
        }

        if( nonPrint )
            continue;

        element.setAttribute( it.key(), it.data() );
    }

    QString text;
    Q3TextStream stream( &text, QIODevice::WriteOnly );
    element.save( stream, 0 );

    std::cout << text.utf8() << std::endl;
}


#include "collectionscanner.moc"

