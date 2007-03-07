/*
Copyright (c) 2006  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>

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

#include "servicebrowser.h"
#include "amarok.h"
#include "debug.h"


ScriptableService::ScriptableService( const char *name )
        : ServiceBase( name )
{
    setObjectName(name);
    DEBUG_BLOCK

}


/*void MagnatuneBrowser::treeItemSelected( const QModelIndex & index ) {

    m_model->requestHtmlInfo( index );

}

void MagnatuneBrowser::infoChanged ( QString infoHtml ) {

    m_infoBox->begin( );
    m_infoBox->write( infoHtml );
    m_infoBox->end();

}*/



#include "scriptableservice.moc"
