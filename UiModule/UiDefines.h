// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_UiDefines_h
#define incl_UiModule_UiDefines_h

#include <QString>
#include <QMap>

namespace UiDefines
{
    enum ConnectionState 
    { 
        Connected, 
        Disconnected, 
        Failed,
        Kicked
    };

    enum ControlButtonType
    {
        Unknown,
        Ether,
        Build,
        Quit,
        Settings,
        Notifications,
        Teleport
    };

    enum MenuNodeStyle
    {
        TextNormal,
        TextHover,
        TextPressed,
        IconNormal,
        IconHover,
        IconPressed
    };

    enum MenuGroup
    { 
        NoGroup = 0,
        RootGroup = 1, 
        ServerToolsGroup = 2,
        WorldToolsGroup = 3
    };

    typedef QMap<UiDefines::MenuNodeStyle, QString> MenuNodeStyleMap;
}

#endif
