// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "ECEditorModule.h"
#include "ECAttributeEditor.h"
#include "AttributeInterface.h"
#include "MultiEditPropertyManager.h"
#include "MultiEditPropertyFactory.h"
#include "LineEditPropertyFactory.h"

// QtPropertyBrowser headers.
#include <qtvariantproperty.h>
#include <qtpropertymanager.h>
#include <qtpropertybrowser.h>
#include <qteditorfactory.h>

#include "MemoryLeakCheck.h"

namespace ECEditor
{
    ECAttributeEditorBase::ECAttributeEditorBase(QtAbstractPropertyBrowser *owner,
                                                 Foundation::AttributeInterface *attribute,
                                                 QObject *parent):
        QObject(parent),
        owner_(owner),
        rootProperty_(0),
        factory_(0),
        propertyMgr_(0),
        listenEditorChangedSignal_(false),
        useMultiEditor_(false),
        editorState_(Uninitialized)
    {
        attributeName_ = attribute->GetName();
        attributes_.push_back(attribute);
    }

    ECAttributeEditorBase::~ECAttributeEditorBase()
    {
        UnInitialize();
    }

    bool ECAttributeEditorBase::ContainProperty(QtProperty *property) const
    {
        QSet<QtProperty *> properties = propertyMgr_->properties();
        QSet<QtProperty *>::const_iterator iter = properties.find(property);
        if(iter != properties.end())
            return true;
        return false;
    }

    void ECAttributeEditorBase::UpdateEditorUI()
    {
        if(attributes_.size() == 1)
        {
            if(!useMultiEditor_ && editorState_ != Uninitialized)
                Update(); 
            else
            {
                useMultiEditor_ = false;
                Initialize();
            }
            emit AttributeChanged(attributeName_.toStdString());
        }
        else if(attributes_.size() > 1)
        {
            if(!IsIdentical())
            {
                if(!useMultiEditor_)
                {
                    useMultiEditor_ = true;
                    UnInitialize();
                }
            }
            else
            {
                if(useMultiEditor_)
                {
                    useMultiEditor_ = false;
                    UnInitialize();
                }
            }

            if(editorState_ == Uninitialized)
                Initialize();
            else
                Update();
            emit AttributeChanged(attributeName_.toStdString()); 
        }
    }

    void ECAttributeEditorBase::AddNewAttribute(Foundation::AttributeInterface *attribute)
    {
        AttributeList::iterator iter = attributes_.begin();
        for(;iter != attributes_.end(); iter++)
        {
            if(*iter == attribute)
                return;
        }
        attributes_.push_back(attribute);
        UpdateEditorUI();
    }

    void ECAttributeEditorBase::RemoveAttribute(Foundation::AttributeInterface *attribute)
    {
        AttributeList::iterator iter = attributes_.begin();
        for(;iter != attributes_.end(); iter++)
        {
            if(*iter == attribute)
            {
                attributes_.erase(iter);
                UpdateEditorUI();
                return;
            }
        }
    }

    bool ECAttributeEditorBase::IsIdentical() const
    {
        if(attributes_.size() > 1)
        {
            AttributeList::const_iterator iter = attributes_.begin();
            std::string value = (*iter)->ToString();
            while(iter != attributes_.end())
            {
                if(value != (*iter)->ToString())
                    return false;
                iter++;
            }
        }
        return true;
    }

    void ECAttributeEditorBase::PreInitialize()
    {
        if(propertyMgr_ || factory_ || rootProperty_)
            UnInitialize();
        editorState_ = WaitingForResponse;
    }

    void ECAttributeEditorBase::UnInitialize()
    {
        if(owner_)
        {
            owner_->unsetFactoryForManager(propertyMgr_);
            for(uint i = 0; i < optionalPropertyManagers_.size(); i++)
                owner_->unsetFactoryForManager(optionalPropertyManagers_[i]);
        }
        if(propertyMgr_)
        {
            propertyMgr_->deleteLater();
            propertyMgr_ = 0;
        }
        while(!optionalPropertyManagers_.empty())
        {
            optionalPropertyManagers_.back()->deleteLater();
            optionalPropertyManagers_.pop_back();
        }
        if(factory_)
        {
            factory_->deleteLater();
            factory_ = 0;
        }
        while(!optionalPropertyFactories_.empty())
        {
            optionalPropertyFactories_.back()->deleteLater();
            optionalPropertyFactories_.pop_back();
        }
        editorState_ = Uninitialized;
    }

    //-------------------------REAL ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<Real>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *realPropertyManager = new QtVariantPropertyManager(this);
            QtVariantEditorFactory *variantFactory = new QtVariantEditorFactory(this);
            propertyMgr_ = realPropertyManager;
            factory_ = variantFactory;

            rootProperty_ = realPropertyManager->addProperty(QVariant::Double, attributeName_);
            if(rootProperty_)
            {
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(realPropertyManager, variantFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }


    template<> void ECAttributeEditor<Real>::Update()
    {
        if(!useMultiEditor_)
        {
            AttributeList::iterator iter = attributes_.begin();
            QtVariantPropertyManager *realPropertyManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            assert(realPropertyManager);
            if(!realPropertyManager)
                return;

            if(iter != attributes_.end())
            {
                if(rootProperty_)
                {
                    Foundation::Attribute<Real> *attribute = dynamic_cast<Foundation::Attribute<Real>*>(*iter);
                    realPropertyManager->setValue(rootProperty_, attribute->Get());
                }
            }
        }
        else
            UpdateMultiEditorValue();
    }

    template<> void ECAttributeEditor<Real>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            Real newValue = ParseString<Real>(property->valueText().toStdString());
            SetValue(newValue);
        }
    }

    //-------------------------INT ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<int>::Update()
    {
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *intPropertyManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            AttributeList::iterator iter = attributes_.begin();
            assert(intPropertyManager);
            if(!intPropertyManager)
                return;

            if(iter != attributes_.end())
            {
                if(rootProperty_)
                {
                    Foundation::Attribute<int> *attribute = dynamic_cast<Foundation::Attribute<int>*>(*iter);
                    intPropertyManager->setValue(rootProperty_, attribute->Get());
                }
            }
        }
        else
            UpdateMultiEditorValue();
    }

    template<> void ECAttributeEditor<int>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *intPropertyManager = new QtVariantPropertyManager(this);
            QtVariantEditorFactory *variantFactory = new QtVariantEditorFactory(this);
            propertyMgr_ = intPropertyManager;
            factory_ = variantFactory;
            rootProperty_ = intPropertyManager->addProperty(QVariant::Int, attributeName_);
            if(rootProperty_)
            {
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(intPropertyManager, variantFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<int>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            Real newValue = ParseString<int>(property->valueText().toStdString());
            SetValue(newValue);
        }
    }

    //-------------------------BOOL ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<bool>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *boolPropertyManager = new QtVariantPropertyManager(this);
            QtVariantEditorFactory *variantFactory = new QtVariantEditorFactory(this);
            propertyMgr_ = boolPropertyManager;
            factory_ = variantFactory;
            rootProperty_ = boolPropertyManager->addProperty(QVariant::Bool, attributeName_);
            if(rootProperty_)
            {
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(boolPropertyManager, variantFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<bool>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            if(property->valueText().toStdString() == "True")
                SetValue(true);
            else
                SetValue(false);
        }
    }

    template<> void ECAttributeEditor<bool>::Update()
    {
        if(!useMultiEditor_)
        {
            AttributeList::iterator iter = attributes_.begin();
            QtVariantPropertyManager *boolPropertyManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            if(!boolPropertyManager)
                return;

            if(iter != attributes_.end())
            {
                if(rootProperty_)
                {
                    Foundation::Attribute<bool> *attribute = dynamic_cast<Foundation::Attribute<bool>*>(*iter);
                    boolPropertyManager->setValue(rootProperty_, attribute->Get());
                }
            }
        }
        else
            UpdateMultiEditorValue();
    }

    //-------------------------VECTOR3DF ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<Vector3df>::Update()
    {
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *variantManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            if(rootProperty_)
            {
                QList<QtProperty *> children = rootProperty_->subProperties();
                if(children.size() >= 3)
                {
                    Foundation::Attribute<Vector3df> *attribute = dynamic_cast<Foundation::Attribute<Vector3df> *>(*(attributes_.begin()));
                    if(!attribute)
                        return;

                    Vector3df vectorValue = attribute->Get();
                    variantManager->setValue(children[0], vectorValue.x);
                    variantManager->setValue(children[1], vectorValue.y);
                    variantManager->setValue(children[2], vectorValue.z);
                }
            }
        }
        else
            UpdateMultiEditorValue();
    }

    template<> void ECAttributeEditor<Vector3df>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *variantManager = new QtVariantPropertyManager(this);
            QtVariantEditorFactory *variantFactory = new QtVariantEditorFactory(this);
            propertyMgr_ = variantManager;
            factory_ = variantFactory;
            rootProperty_ = variantManager->addProperty(QtVariantPropertyManager::groupTypeId(), attributeName_);

            if(rootProperty_)
            {
                QtProperty *childProperty = 0;
                childProperty = variantManager->addProperty(QVariant::Double, "x");
                rootProperty_->addSubProperty(childProperty);

                childProperty = variantManager->addProperty(QVariant::Double, "y");
                rootProperty_->addSubProperty(childProperty);

                childProperty = variantManager->addProperty(QVariant::Double, "z");
                rootProperty_->addSubProperty(childProperty);
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(variantManager, variantFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<Vector3df>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            QtVariantPropertyManager *variantManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            QList<QtProperty *> children = rootProperty_->subProperties();
            if(children.size() >= 3)
            {
                Foundation::Attribute<Vector3df> *attribute = dynamic_cast<Foundation::Attribute<Vector3df> *>(*(attributes_.begin()));
                if(!attribute)
                    return;

                Vector3df newValue = attribute->Get();
                QString propertyName = property->propertyName();
                if(propertyName == "x")
                    newValue.x = ParseString<Real>(property->valueText().toStdString());
                else if(propertyName == "y")
                    newValue.y = ParseString<Real>(property->valueText().toStdString());
                else if(propertyName == "z")
                    newValue.z = ParseString<Real>(property->valueText().toStdString());
                SetValue(newValue);
            }
        }
    }

    //-------------------------COLOR ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<Color>::Update()
    {
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *variantManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            if(rootProperty_)
            {
                QList<QtProperty *> children = rootProperty_->subProperties();
                if(children.size() >= 4)
                {
                    Foundation::Attribute<Color> *attribute = dynamic_cast<Foundation::Attribute<Color> *>(*(attributes_.begin()));
                    if(!attribute)
                        return;

                    Color colorValue = attribute->Get();
                    variantManager->setValue(children[0], colorValue.r * 255);
                    variantManager->setValue(children[1], colorValue.g * 255);
                    variantManager->setValue(children[2], colorValue.b * 255);
                    variantManager->setValue(children[3], colorValue.a * 255);
                }
            }
        }
        else
            UpdateMultiEditorValue();
    }

    template<> void ECAttributeEditor<Color>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtVariantPropertyManager *variantManager = new QtVariantPropertyManager(this);
            QtVariantEditorFactory *variantFactory = new QtVariantEditorFactory(this);
            propertyMgr_ = variantManager;
            factory_ = variantFactory;

            rootProperty_ = variantManager->addProperty(QtVariantPropertyManager::groupTypeId(), attributeName_);
            if(rootProperty_)
            {
                QtVariantProperty *childProperty = 0;
                childProperty = variantManager->addProperty(QVariant::Int, "Red");
                rootProperty_->addSubProperty(childProperty);
                variantManager->setAttribute(childProperty, "minimum", QVariant(0));
                variantManager->setAttribute(childProperty, "maximum", QVariant(255));

                childProperty = variantManager->addProperty(QVariant::Int, "Green");
                rootProperty_->addSubProperty(childProperty);
                variantManager->setAttribute(childProperty, "minimum", QVariant(0));
                variantManager->setAttribute(childProperty, "maximum", QVariant(255));

                childProperty = variantManager->addProperty(QVariant::Int, "Blue");
                rootProperty_->addSubProperty(childProperty);
                variantManager->setAttribute(childProperty, "minimum", QVariant(0));
                variantManager->setAttribute(childProperty, "maximum", QVariant(255));

                childProperty = variantManager->addProperty(QVariant::Int, "Alpha");
                rootProperty_->addSubProperty(childProperty);
                variantManager->setAttribute(childProperty, "minimum", QVariant(0));
                variantManager->setAttribute(childProperty, "maximum", QVariant(255));

                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(variantManager, variantFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<Color>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            QtVariantPropertyManager *variantManager = dynamic_cast<QtVariantPropertyManager *>(propertyMgr_);
            QList<QtProperty *> children = rootProperty_->subProperties();
            if(children.size() >= 4)
            {
                Foundation::Attribute<Color> *attribute = dynamic_cast<Foundation::Attribute<Color> *>(*(attributes_.begin()));
                if(!attribute)
                    return;

                Color newValue = attribute->Get();
                QString propertyName = property->propertyName();
                if(propertyName == "Red")
                    newValue.r = ParseString<int>(property->valueText().toStdString()) / 255.0f;
                else if(propertyName == "Green")
                    newValue.g = ParseString<int>(property->valueText().toStdString()) / 255.0f;
                else if(propertyName == "Blue")
                    newValue.b = ParseString<int>(property->valueText().toStdString()) / 255.0f;
                else if(propertyName == "Alpha")
                    newValue.a = ParseString<int>(property->valueText().toStdString()) / 255.0f;
                SetValue(newValue);
            }
        }
    }

    //-------------------------STD::STRING ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<std::string>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtStringPropertyManager *qStringPropertyManager = new QtStringPropertyManager(this);
            ECEditor::LineEditPropertyFactory *lineEditFactory = new ECEditor::LineEditPropertyFactory(this);
            propertyMgr_ = qStringPropertyManager;
            factory_ = lineEditFactory;
            rootProperty_ = qStringPropertyManager->addProperty(attributeName_);
            if(rootProperty_)
            {
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(qStringPropertyManager, lineEditFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<std::string>::Set(QtProperty *property)
    {
        if (listenEditorChangedSignal_)
            SetValue(property->valueText().toStdString());
    }

    template<> void ECAttributeEditor<std::string>::Update()
    {
        if(!useMultiEditor_)
        {
            AttributeList::iterator iter = attributes_.begin();
            QtStringPropertyManager *qStringPropertyManager = dynamic_cast<QtStringPropertyManager *>(propertyMgr_);

            assert(qStringPropertyManager);
            if(!qStringPropertyManager)
                return;

            if(iter != attributes_.end())
            {
                if (rootProperty_)
                {
                    Foundation::Attribute<std::string> *attribute = dynamic_cast<Foundation::Attribute<std::string>*>(*iter);
                    qStringPropertyManager->setValue(rootProperty_, attribute->Get().c_str());
                }
                iter++;
            }
        }
        else
            UpdateMultiEditorValue();
    }

    //-------------------------QVARIANT ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<QVariant>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtStringPropertyManager *qStringPropertyManager = new QtStringPropertyManager(this);
            ECEditor::LineEditPropertyFactory *lineEditFactory = new ECEditor::LineEditPropertyFactory(this);
            propertyMgr_ = qStringPropertyManager;
            factory_ = lineEditFactory;
            rootProperty_ = qStringPropertyManager->addProperty(attributeName_);
            if(rootProperty_)
            {
                Update();
                QObject::connect(propertyMgr_, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(qStringPropertyManager, lineEditFactory);
        }
        else
        {
            InitializeMultiEditor();
        }
    }

    template<> void ECAttributeEditor<QVariant>::Set(QtProperty *property)
    {
        if (listenEditorChangedSignal_)
        {
            QVariant value(property->valueText());
            SetValue(value);
        }
    }

    template<> void ECAttributeEditor<QVariant>::Update()
    {
        if(!useMultiEditor_)
        {
            AttributeList::iterator iter = attributes_.begin();
            QtStringPropertyManager *qStringPropertyManager = dynamic_cast<QtStringPropertyManager *>(propertyMgr_);
            assert(qStringPropertyManager);
            if(!qStringPropertyManager)
                return;

            if(iter != attributes_.end())
            {
                if (rootProperty_)
                {
                    Foundation::Attribute<QVariant> *attribute = dynamic_cast<Foundation::Attribute<QVariant>*>(*iter);
                    qStringPropertyManager->setValue(rootProperty_, attribute->Get().toString());
                }
                iter++;
            }
        }
        else
            UpdateMultiEditorValue();
    }

    //-------------------------ASSETREFERENCE ATTRIBUTE TYPE-------------------------

    template<> void ECAttributeEditor<Foundation::AssetReference>::Update()
    {
        if(!useMultiEditor_)
        {
            QList<QtProperty*> children = rootProperty_->subProperties();
            if(children.size() == 2)
            {
                QtStringPropertyManager *stringManager = dynamic_cast<QtStringPropertyManager *>(children[0]->propertyManager());
                Foundation::Attribute<Foundation::AssetReference> *attribute = dynamic_cast<Foundation::Attribute<Foundation::AssetReference> *>(*(attributes_.begin()));
                if(!attribute || !stringManager)
                    return;

                Foundation::AssetReference value = attribute->Get();
                stringManager->setValue(children[0], QString::fromStdString(value.id_));
                stringManager->setValue(children[1], QString::fromStdString(value.type_));
            }
        }
        else
            UpdateMultiEditorValue();
    }

    template<> void ECAttributeEditor<Foundation::AssetReference>::Initialize()
    {
        ECAttributeEditorBase::PreInitialize();
        if(!useMultiEditor_)
        {
            QtGroupPropertyManager *groupManager = new QtGroupPropertyManager(this);
            QtStringPropertyManager *stringManager = new QtStringPropertyManager(this);
            LineEditPropertyFactory *lineEditFactory = new LineEditPropertyFactory(this);
            propertyMgr_ = groupManager;
            factory_ = lineEditFactory;
            optionalPropertyManagers_.push_back(stringManager);

            rootProperty_ = groupManager->addProperty();
            rootProperty_->setPropertyName(attributeName_);
            if(rootProperty_)
            {
                QtProperty *childProperty = 0;
                childProperty = stringManager->addProperty("Asset ID");
                rootProperty_->addSubProperty(childProperty);

                childProperty = stringManager->addProperty("Asset type");
                rootProperty_->addSubProperty(childProperty);

                Update();
                QObject::connect(stringManager, SIGNAL(propertyChanged(QtProperty*)), this, SLOT(SetAttribute(QtProperty*)));
            }
            owner_->setFactoryForManager(stringManager, lineEditFactory);
        }
        else
            InitializeMultiEditor();
    }

    template<> void ECAttributeEditor<Foundation::AssetReference>::Set(QtProperty *property)
    {
        if(listenEditorChangedSignal_)
        {
            QList<QtProperty*> children = rootProperty_->subProperties();
            if(children.size() == 2)
            {
                QtStringPropertyManager *stringManager = dynamic_cast<QtStringPropertyManager *>(children[0]->propertyManager());
                Foundation::Attribute<Foundation::AssetReference> *attribute = dynamic_cast<Foundation::Attribute<Foundation::AssetReference> *>(*(attributes_.begin()));
                if(!attribute || !stringManager)
                    return;

                Foundation::AssetReference value;
                value.id_ = stringManager->value(children[0]).toStdString();
                value.type_ = stringManager->value(children[1]).toStdString();
                SetValue(value);
            }
        }
    }
}
