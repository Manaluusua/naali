// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_UiModule_ActionNode_h
#define incl_UiModule_ActionNode_h

#include "MenuNode.h"
#include "UiDefines.h"

namespace CoreUi
{
    class ActionNode : public MenuNode
    {

    Q_OBJECT

    public:
        ActionNode(const QString& name, QIcon icon, UiDefines::MenuNodeStyleMap map);
        ~ActionNode();

    public slots:
        void NodeClicked();
        void AddChildNode(MenuNode *node) {};

    };
}

#endif
