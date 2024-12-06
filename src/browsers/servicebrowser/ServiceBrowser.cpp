/****************************************************************************************
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#include "ServiceBrowser.h"

#include "core/support/Debug.h"

#include <QStandardPaths>

ServiceBrowser * ServiceBrowser::s_instance = nullptr;

ServiceBrowser * ServiceBrowser::instance()
{
    if ( s_instance == nullptr )
        s_instance = new ServiceBrowser( QStringLiteral("internet") );

    return s_instance;
}


ServiceBrowser::ServiceBrowser( const QString& name, QWidget* parent )
    : BrowserCategoryList( name, parent, true )
    , m_scriptableServiceManager( nullptr )
{
    debug() << "ServiceBrowser starting...";

    setLongDescription( i18n( "The Internet browser lets you browse online sources of content that integrates directly into Amarok. Amarok ships with a number of these sources, but many more can be added using scripts." ) );

    setImagePath( QStandardPaths::locate( QStandardPaths::GenericDataLocation, QStringLiteral("amarok/images/hover_info_internet.png") ) );
}


ServiceBrowser::~ServiceBrowser()
{
    DEBUG_BLOCK
}

//TODO: This should be moved to the ScriptableServiceManager instead
void
ServiceBrowser::setScriptableServiceManager( ScriptableServiceManager * scriptableServiceManager )
{
    m_scriptableServiceManager = scriptableServiceManager;
    m_scriptableServiceManager->setParent( this );
    connect ( m_scriptableServiceManager, &ScriptableServiceManager::addService,
              this, &ServiceBrowser::addService );
}

void
ServiceBrowser::resetService( const QString &name )
{
    //What in the world is this for...

    //Currently unused, but needed, in the future, for resetting a service based on config changes
    //or the user choosing to reset the state of the service somehow.
    Q_UNUSED( name );
}

void ServiceBrowser::addService( ServiceBase * service )
{
    DEBUG_BLOCK
    addCategory( service );
}


