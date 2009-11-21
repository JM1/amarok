
#include "PlayPauseButton.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QTimerEvent>
#include <QToolBar>

PlayPauseButton::PlayPauseButton( QWidget *parent ) : QWidget( parent )
, m_isPlaying( false )
, m_isClick( false )
, m_animStep( 0 )
, m_animTimer( 0 )
{
    m_iconPlay[0] = QImage("play.png");
    m_iconPlay[1] = QImage("play_h.png");
    m_iconPause[0] = QImage("pause.png");
    m_iconPause[1] = QImage("pause_h.png");
    updateIconBuffer();
}

void PlayPauseButton::enterEvent( QEvent * )
{
    startFade();
}

void PlayPauseButton::leaveEvent( QEvent * )
{
    startFade();
}

void PlayPauseButton::mousePressEvent( QMouseEvent *me )
{
    me->accept();
    m_isClick = true;
    update();
}

void PlayPauseButton::mouseReleaseEvent( QMouseEvent *me )
{
    me->accept();
    if ( !( m_isClick && underMouse() ) )
    {
        m_isClick = false;
        return;
    }
    emit toggled( !m_isPlaying );
}


void PlayPauseButton::paintEvent( QPaintEvent * )
{
    QPainter p(this);
    p.drawPixmap( 0,0, m_iconBuffer );
    p.end();
}

void PlayPauseButton::resizeEvent( QResizeEvent *re )
{
    if ( width() != height() )
        resize( height(), height() );
    else
        QWidget::resizeEvent( re );
    updateIconBuffer();
}

void PlayPauseButton::setPlaying( bool b )
{
    if ( m_isPlaying == b )
        return;
    m_isPlaying = b;
    updateIconBuffer();
    update();
}

QSize PlayPauseButton::sizeHint() const
{
    if ( QToolBar *toolBar = qobject_cast<QToolBar*>( parentWidget() ) )
        return toolBar->iconSize();

    return QSize( 32, 32 );
}

void PlayPauseButton::startFade()
{
    if ( m_animTimer )
        killTimer( m_animTimer );
    m_animTimer = startTimer( 40 );
}

void PlayPauseButton::stopFade()
{
    killTimer( m_animTimer );
    m_animTimer = 0;
    if ( m_animStep < 0 )
        m_animStep = 0;
    else if ( m_animStep > 6 )
        m_animStep = 6;
}

void PlayPauseButton::timerEvent( QTimerEvent *te )
{
    if ( te->timerId() != m_animTimer )
        return;
    if ( underMouse() ) // fade in
    {
        m_animStep += 2;
        updateIconBuffer();
        if ( m_animStep > 5 )
            stopFade();
    }
    else // fade out
    {
        --m_animStep;
        updateIconBuffer();
        if ( m_animStep < 1 )
            stopFade();
    }
    repaint();
}
#include <QtDebug>
static QImage
interpolated( const QImage &img1, const QImage &img2, int a1, int a2 )
{
//     QImage *wider, *higher;
//     QImage img( qMax( img1.width(), img2.width() ), qMax( img1.height(), img2.height() ), QImage::Format_RGB32 );
    QImage img( img1.size(), QImage::Format_ARGB32 );
    const int a = a1 + a2;
    if (!a)
        return img;

    const uchar *src[2] = { img1.bits(), img2.bits() };
    uchar *dst = img.bits();
    const int n = img.width()*img.height()*4;
    for ( int i = 0; i < n; ++i )
    {
        *dst = ((*src[0]*a1 + *src[1]*a2)/a) & 0xff;
        ++dst; ++src[0]; ++src[1];
    }
    return img;
}

void PlayPauseButton::updateIconBuffer()
{
    QImage img;
    QImage (*base)[2] = m_isPlaying ? &m_iconPlay : &m_iconPause;

    if (m_animStep < 1)
        img = (*base)[0];
    else if (m_animStep > 5)
        img = (*base)[1];
    else
        img = interpolated( (*base)[0], (*base)[1], 6 - m_animStep, m_animStep );

    m_iconBuffer = QPixmap::fromImage( img.scaled( size(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}
