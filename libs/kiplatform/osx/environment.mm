/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Ian McInerney <Ian.S.McInerney at ieee.org>
 * Copyright (C) 2020 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <kiplatform/environment.h>

#import <Cocoa/Cocoa.h>
#include <wx/osx/core/cfstring.h>


bool KIPLATFORM::ENV::MoveToTrash( const wxString& aPath, wxString& aError )
{
    wxString temp = "file:///" + aPath;

    NSURL*   url = [NSURL URLWithString:wxCFStringRef( temp ).AsNSString()];
    NSError* err = NULL;

    BOOL result = [[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&err];

    // Extract the error string if the operation failed
    if( result == NO )
    {
        NSString* errmsg;
        errmsg = [err.localizedFailureReason stringByAppendingFormat:@"\n\n%@", err.localizedRecoverySuggestion];
        aError = wxCFStringRef::AsString( (CFStringRef) errmsg );
        return false;
    }

    return true;
}
