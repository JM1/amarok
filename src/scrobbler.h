// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// (c) 2004 Sami Nieminen <sami.nieminen@iki.fi>
// See COPYING file for licensing information


#ifndef AMAROK_SCROBBLER_H
#define AMAROK_SCROBBLER_H

#include "amarokconfig.h"
#include "engineobserver.h"

#include <kio/job.h>
#include <kio/jobclasses.h>

#include <qobject.h>         //baseclass
#include <qstringlist.h>

class ScrobblerSubmitter;
class SubmitItem;

class Scrobbler : public QObject, public EngineObserver
{
    Q_OBJECT

    public:
        static Scrobbler *instance();

        void similarArtists( QString /*artist*/ );
        void applySettings();

    signals:
        void relatedArtistsFetched( const QString& artist, const QStringList& suggestions );

    protected:
        void engineNewMetaData( const MetaBundle& /*bundle*/, bool /*state*/ );
        void engineTrackPositionChanged( long /*position*/ );

    private slots:
        void audioScrobblerSimilarArtistsResult( KIO::Job* /*job*/ );
        void audioScrobblerSimilarArtistsData(
            KIO::Job* /*job*/, const QByteArray& /*data*/ );

    private:
        Scrobbler();
        ~Scrobbler();
        QString m_similarArtistsBuffer;
        QString m_artist;
        long m_prevPos;
        bool m_validForSending;
        ScrobblerSubmitter* m_submitter;
        SubmitItem* m_item;
};

class ScrobblerSubmitter : public QObject
{
    Q_OBJECT

    public:
        static QString PROTOCOL_VERSION;
        static QString CLIENT_ID;
        static QString CLIENT_VERSION;
        static QString HANDSHAKE_URL;

        ScrobblerSubmitter();

        void handshake();
        void submitItem( SubmitItem* /* item */ );

        const QString username() const { return m_username; }
        const QString password() const { return m_password; }
        const bool enabled() const { return m_scrobblerEnabled; }

        void setUsername( const QString& /*username*/ );
        void setPassword( const QString& /* password*/ );
        void setEnabled( bool /*enabled*/ );

    private slots:
        void audioScrobblerHandshakeResult( KIO::Job* /*job*/ );
        void audioScrobblerSubmitResult( KIO::Job* /*job*/ );
        void audioScrobblerSubmitData(
            KIO::Job* /*job*/, const QByteArray& /*data*/ );

    private:
        bool canSubmit() const;
        QString m_submitResultBuffer;
        QString m_username;
        QString m_password;
        QString m_submitUrl;
        QString m_challenge;
        bool m_scrobblerEnabled;
        uint m_prevSubmitTime;
        uint m_interval;

        SubmitItem* m_item; // TODO: buffer these
};


class SubmitItem
{
    public:
        SubmitItem(
            const QString& /*artist*/,
            const QString& /*album*/,
            const QString& /*title*/,
            int /*length*/ );

        const QString artist() const { return m_artist; }
        const QString album() const { return m_album; }
        const QString title() const { return m_title; }
        const int length() const { return m_length; }
        const uint playStartTime() const { return m_playStartTime; }

    private:
        QString m_artist;
        QString m_album;
        QString m_title;
        int m_length;
        uint m_playStartTime;
};

#endif /* AMAROK_SCROBBLER_H */
