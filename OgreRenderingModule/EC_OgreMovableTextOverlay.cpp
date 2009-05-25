// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "EC_OgreMovableTextOverlay.h"
#include "Renderer.h"
#include "OgreRenderingModule.h"

#include <Ogre.h>
#include <OgreTextAreaOverlayElement.h>
#include <OgreFontManager.h>

// Code partly adapted from http://www.ogre3d.org/wiki/index.php/ObjectTextDisplay and
// http://www.ogre3d.org/wiki/index.php/ObjectTitle

namespace OgreRenderer
{
    EC_OgreMovableTextOverlay::EC_OgreMovableTextOverlay(Foundation::ModuleInterface* module) : 
        Foundation::ComponentInterface(module->GetFramework()),
        text_element_(NULL),
        container_(NULL),
        overlay_(NULL),
        camera_(NULL),
        font_(NULL),
        renderer_(checked_static_cast<OgreRenderingModule*>(module)->GetRenderer()),
//        char_height_(2*0.0175f),
        visible_(false),
        overlayName_(""),
        containerName_(""),
        text_("")
        
    {
        camera_ = renderer_.lock()->GetCurrentCamera();
        windowWidth_ = camera_->getViewport()->getActualWidth();
        windowHeight_ = camera_->getViewport()->getActualHeight();
        
        CreateOverlay();
    }
    
    // virtual
    EC_OgreMovableTextOverlay::~EC_OgreMovableTextOverlay()
    {
        if (!renderer_.expired())
        {    
	        overlay_->hide();
	        container_->removeChild(overlayName_);
	        overlay_->remove2D(container_);

            Ogre::OverlayManager *overlayManager = Ogre::OverlayManager::getSingletonPtr();    	    
	        overlayManager->destroyOverlayElement(text_element_);
	        overlayManager->destroyOverlayElement(container_);
	        overlayManager->destroy(overlay_);
        }
    }

    void EC_OgreMovableTextOverlay::Update()
    {
        if (!visible_)
	        return;

	    if(!node_->isInSceneGraph())
	    {
		    overlay_->hide();
		    return;
	    }
	    
	    Ogre::Vector3 point = node_->_getDerivedPosition();
                
	    // Is the camera facing that point? If not, hide the overlay and return.
	    Ogre::Plane cameraPlane = Ogre::Plane(Ogre::Vector3(camera_->getDerivedOrientation().zAxis()), camera_->getDerivedPosition());
	    if(cameraPlane.getSide(point) != Ogre::Plane::NEGATIVE_SIDE)
	    {
		    overlay_->hide();
		    return;
	    }
    	
	    // Derive the 2D screen-space coordinates for that point
	    point = camera_->getProjectionMatrix() * (camera_->getViewMatrix() * point);

	    // Transform from coordinate space [-1, 1] to [0, 1]
	    float x = (point.x / 2) + 0.5f;
	    float y = 1 - ((point.y / 2) + 0.5f);

	    // Update the position (centering the text)
	    container_->setPosition(x - (textDim_.x / 2), y);
        
        // Update the dimensions also if the window is resized.
        if (windowWidth_ != camera_->getViewport()->getActualWidth() ||
            windowHeight_ != camera_->getViewport()->getActualHeight())
        {
            windowWidth_ = camera_->getViewport()->getActualWidth();
            windowHeight_ = camera_->getViewport()->getActualHeight();
            
	        textDim_ = GetTextDimensions(text_);
	        container_->setDimensions(textDim_.x, textDim_.y);
        }
        
        ///\todo Scale the text and width and height of the container?	    
//        text_element_->setMetricsMode(Ogre::GMM_RELATIVE);
//        text_element_->setPosition(textDim_.x, textDim_.y);
//        text_element_->setPosition(textDim_.x / 10, 0.01);
//        text_element_->setCharHeight(max_x - min_x/*2*0.0175f*///);
       
	    overlay_->show();
    }
    
    void EC_OgreMovableTextOverlay::SetVisible(bool visible)
    {
	    visible_ = visible;
	    if (visible)
		    overlay_->show();
	    else
		    overlay_->hide();
    }
    
    void EC_OgreMovableTextOverlay::SetParentNode(Ogre::SceneNode *parent_node)
    {
        parent_node->addChild(node_);
    }

    void EC_OgreMovableTextOverlay::SetText(const std::string& text)
    {
	    text_ = text;
	    text_element_->setCaption(text_);
        textDim_ = GetTextDimensions(text_);
	    container_->setDimensions(textDim_.x, textDim_.y);
    }

    void EC_OgreMovableTextOverlay::CreateOverlay()
    {
        if (renderer_.expired())
            return;
        
        // Create SceneNode
        Ogre::SceneManager *scene_mgr = renderer_.lock()->GetSceneManager();
        node_ = scene_mgr->createSceneNode();
        
        // Set the node position above the parent node.
        node_->setPosition(0, 0, 2);
        
	    // Overlay
	    overlayName_ = renderer_.lock()->GetUniqueObjectName();
	    overlay_ = Ogre::OverlayManager::getSingleton().create(overlayName_);
	    
	    // Container
	    containerName_ = renderer_.lock()->GetUniqueObjectName();
	    container_ = static_cast<Ogre::OverlayContainer*>
	        (Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", containerName_));
        container_->setMaterialName("RedTransparent");
	    overlay_->add2D(container_);
	    
	    // Font
	    ///\todo user-defined font
	    std::string fontName = "Console";
        Ogre::FontManager::getSingleton().load(fontName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        font_ = (Ogre::Font*)Ogre::FontManager::getSingleton().getByName(fontName).getPointer();
        font_->setParameter("size", "16");
        
        // Overlay text
        text_element_ = checked_static_cast<Ogre::TextAreaOverlayElement*>
            (Ogre::OverlayManager::getSingleton().createOverlayElement("TextArea", overlayName_));
//	    text_element_ = Ogre::OverlayManager::getSingleton().createOverlayElement("TextArea", "shapeNameText");
        text_element_->setDimensions(0.8, 0.8);
        text_element_->setMetricsMode(Ogre::GMM_PIXELS);
        text_element_->setPosition(1, 2);
        text_element_->setParameter("font_name", fontName);
        text_element_->setParameter("char_height", font_->getParameter("size"));
//        text_element_->setCharHeight(0.035f);
        text_element_->setParameter("horz_align", "left");
	    text_element_->setColour(Ogre::ColourValue::Black);
	    container_->addChild(text_element_);
	    
	    if(text_ != "")
	    {
            textDim_ = GetTextDimensions(text_);
            container_->setDimensions(textDim_.x, textDim_.y);
        }
        
        if (visible_)
            overlay_->show();
        else
            overlay_->hide();
        
        overlay_->setZOrder(500);
    }
    
    Ogre::Vector2 EC_OgreMovableTextOverlay::GetTextDimensions(const std::string &text)
    {
	    float charHeight = Ogre::StringConverter::parseReal(font_->getParameter("size"));
        
	    Ogre::Vector2 result(0, 0);
        
	    for(std::string::const_iterator it = text.begin(); it < text.end(); ++it)
	    {   
		    if (*it == 0x0020)
			    result.x += font_->getGlyphAspectRatio(0x0030);
		    else
			    result.x += font_->getGlyphAspectRatio(*it);
	    }
        
	    result.x = (result.x * charHeight) / (float)camera_->getViewport()->getActualWidth();
	    result.y = charHeight / (float)camera_->getViewport()->getActualHeight();

	    return result;
    }
}