// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "UiDefines.h"

#include "InworldSceneController.h"
#include "ControlPanelManager.h"

#include "Common/AnchorLayoutManager.h"

#include "Menus/MenuManager.h"

#include "View/UiProxyWidget.h"
#include "View/UiWidgetProperties.h"
#include "View/CommunicationWidget.h"

#include "Inworld/ControlPanel/SettingsWidget.h"
#include "Inworld/ControlPanel/PersonalWidget.h"

#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsLinearLayout>
#include <QGraphicsWidget>
#include <QGraphicsView>
#include <QGraphicsScene>

#include <QObject>

#include "MemoryLeakCheck.h"

#define DOCK_WIDTH			(300)
#define DIST_FROM_BOTTOM	(200)
#define DIST_FROM_TOP		(50)

namespace UiServices
{
    InworldSceneController::InworldSceneController(Foundation::Framework *framework, QGraphicsView *ui_view) 
        : QObject(),
          framework_(framework),
          ui_view_(ui_view),
          communication_widget_(0)
    {
        if (!ui_view_)
            return;

        // Store scene pointer
        inworld_scene_ = ui_view_->scene();

        // Init layout manager with scene
        layout_manager_ = new CoreUi::AnchorLayoutManager(this, inworld_scene_);

        // Init UI managers with layout manager
        control_panel_manager_ = new CoreUi::ControlPanelManager(this, layout_manager_);
        menu_manager_ = new CoreUi::MenuManager(this, layout_manager_);
        
        // Communication core UI
        communication_widget_ = new CoreUi::CommunicationWidget(framework);
        layout_manager_->AddCornerAnchor(communication_widget_, Qt::BottomLeftCorner, Qt::BottomLeftCorner);
		
        // Connect settings widget
        connect(control_panel_manager_->GetSettingsWidget(), SIGNAL(NewUserInterfaceSettingsApplied(int, int)), SLOT(ApplyNewProxySettings(int, int)));
		
	    // Apply new positions to active widgets when the inworld_scene_ is resized
	    connect(inworld_scene_, SIGNAL(sceneRectChanged(const QRectF)), this, SLOT(ApplyNewProxyPosition(const QRectF)));

		// Docking widget
		docking_widget_ = new QWidget();
        docking_widget_->setStyleSheet("QWidget { background-color: rgba(0, 0, 0, 75); border: 1px solid rgba(255,255,255,100); border-radius: 0px; }");
		docking_widget_proxy_ = inworld_scene_->addWidget(docking_widget_);
		docking_widget_proxy_->hide();
		docking_widget_proxy_->setVisible(false);

    }

    InworldSceneController::~InworldSceneController()
    {
        SAFE_DELETE(communication_widget_);
        SAFE_DELETE(docking_widget_);
    }

    /*************** UI Scene Manager Public Services ***************/

    bool InworldSceneController::AddSettingsWidget(QWidget *settings_widget, const QString &tab_name) const
    {
        control_panel_manager_->GetSettingsWidget()->AddWidget(settings_widget, tab_name);
        return true;
    }

    UiProxyWidget* InworldSceneController::AddWidgetToScene(QWidget *widget)
    {
        return AddWidgetToScene(widget, UiWidgetProperties("Unnamed Widget", ModuleWidget));
    }

    UiProxyWidget* InworldSceneController::AddWidgetToScene(QWidget *widget, const UiServices::UiWidgetProperties &widget_properties)
    {
		UiProxyWidget *proxy_widget = new UiProxyWidget(widget, widget_properties);

		if (AddProxyWidget(proxy_widget))
            return proxy_widget;
        else
            return 0;
    }

    bool InworldSceneController::AddProxyWidget(UiServices::UiProxyWidget *proxy_widget)
    {
        if (!inworld_scene_)
        {
            SAFE_DELETE(proxy_widget);
            return false;
        }

        // Add to scene
        if (proxy_widget->isVisible())
            proxy_widget->hide();
        inworld_scene_->addItem(proxy_widget);
		
        // Add to internal control list
        if (!all_proxy_widgets_in_scene_.contains(proxy_widget))
            all_proxy_widgets_in_scene_.append(proxy_widget);

        // Add to menu structure if its needed
        UiWidgetProperties properties = proxy_widget->GetWidgetProperties();
        if (properties.IsShownInToolbar())
        {
            QString widget_name = properties.GetWidgetName();
            if (widget_name == "Inventory")
                control_panel_manager_->GetPersonalWidget()->SetInventoryWidget(proxy_widget);
            else if (widget_name == "Avatar Editor")
                control_panel_manager_->GetPersonalWidget()->SetAvatarWidget(proxy_widget);
            else
                menu_manager_->AddMenuItem(properties.GetMenuGroup(), proxy_widget, properties);

            connect(proxy_widget, SIGNAL( BringProxyToFrontRequest(UiProxyWidget*) ), this, SLOT( BringProxyToFront(UiProxyWidget*) ));
			connect(proxy_widget, SIGNAL(ProxyMoved(UiProxyWidget*, QPointF)), this, SLOT(ProxyWidgetMoved(UiProxyWidget*, QPointF)));
			connect(proxy_widget, SIGNAL(ProxyUngrabed(UiProxyWidget*, QPointF)), this, SLOT(ProxyWidgetUngrabed(UiProxyWidget*, QPointF)));
			connect(proxy_widget, SIGNAL(Closed()), this, SLOT(ProxyClosed()));
			connect(proxy_widget, SIGNAL(Visible(bool)), this, SLOT(ProxyClosed()));
        }
        return true;
    }

    void InworldSceneController::RemoveProxyWidgetFromScene(UiServices::UiProxyWidget *proxy_widget)
    {
        if (!inworld_scene_)
            return;
        
        QString widget_name = proxy_widget->GetWidgetProperties().GetWidgetName();
        if (widget_name != "Inventory" && widget_name != "Avatar Editor")
            menu_manager_->RemoveMenuItem(proxy_widget->GetWidgetProperties().GetMenuGroup(), proxy_widget);
        inworld_scene_->removeItem(proxy_widget);
        all_proxy_widgets_in_scene_.removeOne(proxy_widget);
    }

    void InworldSceneController::RemoveProxyWidgetFromScene(QWidget *widget)
    {
        UiProxyWidget *proxy_widget = dynamic_cast<UiProxyWidget*>(widget->graphicsProxyWidget());
        if (proxy_widget)
            RemoveProxyWidgetFromScene(proxy_widget);
    }

    void InworldSceneController::BringProxyToFront(UiProxyWidget *widget) const
    {
        if (!inworld_scene_ || !inworld_scene_->isActive())
            return;
        inworld_scene_->setActiveWindow(widget);
        inworld_scene_->setFocusItem(widget, Qt::ActiveWindowFocusReason);
    }

    void InworldSceneController::BringProxyToFront(QWidget *widget) const
    {
        if (!inworld_scene_)
            return;
        ShowProxyForWidget(widget);
        inworld_scene_->setActiveWindow(widget->graphicsProxyWidget());
        inworld_scene_->setFocusItem(widget->graphicsProxyWidget(), Qt::ActiveWindowFocusReason);
    }

    void InworldSceneController::ShowProxyForWidget(QWidget *widget) const
    {
        if (inworld_scene_)
            widget->graphicsProxyWidget()->show();
    }

    void InworldSceneController::HideProxyForWidget(QWidget *widget) const
    {
        if (inworld_scene_)
            widget->graphicsProxyWidget()->hide();
    }

    QObject *InworldSceneController::GetSettingsObject() const
    {
        return dynamic_cast<QObject *>(control_panel_manager_->GetSettingsWidget());
    }

    void InworldSceneController::SetFocusToChat() const
    {
        if (communication_widget_)
            communication_widget_->SetFocusToChat();
    }

    // Don't touch, please

    void InworldSceneController::SetImWidget(UiProxyWidget *im_proxy) const
    {
        if (communication_widget_)
            communication_widget_->UpdateImWidget(im_proxy);
    }

    // Private

    void InworldSceneController::ApplyNewProxySettings(int new_opacity, int new_animation_speed) const
    {
        foreach (UiProxyWidget *widget, all_proxy_widgets_in_scene_)
        {
            widget->SetUnfocusedOpacity(new_opacity);
            widget->SetShowAnimationSpeed(new_animation_speed);
        }
    }

    //Apply new proxy position
    void InworldSceneController::ApplyNewProxyPosition(const QRectF &new_rect)
	{
		QPointF left_distance;
		QPointF right_distance;
        qreal new_x;
        qreal new_y;

		foreach (UiProxyWidget *widget, all_proxy_widgets_in_scene_)
        {
			if (widget->isVisible())
			{	
                // Make sure widget is not docked
				if (!all_docked_proxy_widgets_.contains(widget))
				{
					left_distance.setX(widget->x() / last_scene_rect.width() * new_rect.width());
					left_distance.setY(widget->y() / last_scene_rect.height() * new_rect.height());

					right_distance.setX((widget->x() + widget->size().width()) / last_scene_rect.width() * new_rect.width());
					right_distance.setY((widget->y() + widget->size().height()) / last_scene_rect.height() * new_rect.height());						

					if (widget->size().width() < new_rect.width())
					{
						if (left_distance.x() > widget->size().width())
						{
							if (new_rect.width() > right_distance.x())
								new_x = right_distance.x() - widget->size().width();
							else
								new_x = left_distance.x();
						}
						else
							new_x = left_distance.x();
					}
					else
						new_x = left_distance.x();

					if (widget->size().height() < new_rect.height())
					{
						if (left_distance.y() > widget->size().height())
						{
							if (new_rect.height() > right_distance.y())
								new_y = right_distance.y() - widget->size().height();
							else
								new_y = left_distance.y();
						}
						else
							new_y = left_distance.y();
					}
					else
						new_y = left_distance.y();

					widget->setPos(new_x, new_y);
				}
				else
					widget->setPos(inworld_scene_->width() - DOCK_WIDTH, widget->y());
            }
        }

		last_scene_rect = new_rect;
        DockLineup();
	}

	void InworldSceneController::ProxyWidgetMoved(UiProxyWidget* proxy_widget, QPointF proxy_pos)
	{
		if (proxy_pos.x() + proxy_widget->size().width() > inworld_scene_->width() - DOCK_WIDTH)
		{
            docking_widget_->setGeometry(0, 0, DOCK_WIDTH, inworld_scene_->height() - DIST_FROM_BOTTOM);
			docking_widget_proxy_->show();
			docking_widget_proxy_->setPos(inworld_scene_->width() - DOCK_WIDTH, DIST_FROM_TOP);
		}
		else
		{
			docking_widget_proxy_->hide();
			docking_widget_proxy_->setVisible(false);
		}
	}

	void InworldSceneController::ProxyWidgetUngrabed(UiProxyWidget* proxy_widget, QPointF proxy_pos)
	{
        bool changes = false;
		if (proxy_pos.x() + proxy_widget->size().width() > inworld_scene_->width() - DOCK_WIDTH)
		{
			if (!all_docked_proxy_widgets_.contains(proxy_widget))
			{
				old_proxy_size.insert(proxy_widget, proxy_widget->size());
				all_docked_proxy_widgets_.append(proxy_widget);
                changes = true;
			}
		}
		else
		{
            if (all_docked_proxy_widgets_.contains(proxy_widget))
            {
                UiServices::UiProxyWidget *proxy = all_docked_proxy_widgets_.at(all_docked_proxy_widgets_.indexOf(proxy_widget));
                if (proxy)
                {
                    QSizeF old_size = old_proxy_size.value(proxy);
					proxy->setMinimumSize(old_size);
					proxy->setGeometry(QRectF(proxy_pos.x(), proxy_pos.y(), old_size.width(), old_size.height()));
					all_docked_proxy_widgets_.removeAll(proxy);
                    changes = true;
                }
            }
		}

        if (changes)
            DockLineup();
	}

	void InworldSceneController::DockLineup()
	{
        if (all_docked_proxy_widgets_.count() == 0)
            return;

        int top_frame_addition = 22;
        int side_frame_width = 5;
        int order_in_dock = 0;

        qreal new_x = inworld_scene_->width() - DOCK_WIDTH + side_frame_width;
        qreal new_height = (docking_widget_->height() / all_docked_proxy_widgets_.length()) - top_frame_addition - side_frame_width;
        qreal new_width = DOCK_WIDTH - side_frame_width;

        qreal last_bottom_y = 0;
        foreach (UiServices::UiProxyWidget *proxy, all_docked_proxy_widgets_)
        {
            qreal new_y;
            if (order_in_dock == 0)
                new_y = DIST_FROM_TOP + top_frame_addition;
            else
                new_y = last_bottom_y;
            
            QRectF new_rect(new_x, new_y, new_width, new_height);
            proxy->setMinimumSize(new_width, new_height);
            proxy->setGeometry(new_rect);

            last_bottom_y = new_rect.bottom() + top_frame_addition + side_frame_width;
            order_in_dock++;
        }
	}

	void InworldSceneController::ProxyClosed()
	{
		for(int i = 0; i < all_docked_proxy_widgets_.length(); i++)
		{
			if (!all_docked_proxy_widgets_.at(i)->isVisible())
			{	
				QRectF dock_rect;
				dock_rect.setRect(inworld_scene_->width() - all_docked_proxy_widgets_.at(i)->size().width(), all_docked_proxy_widgets_.at(i)->pos().y(), old_proxy_size.value(all_docked_proxy_widgets_.at(i)).width(), old_proxy_size.value(all_docked_proxy_widgets_.at(i)).height());
				all_docked_proxy_widgets_.at(i)->setMinimumSize(old_proxy_size.value(all_docked_proxy_widgets_.at(i)).width(), old_proxy_size.value(all_docked_proxy_widgets_.at(i)).height());
				all_docked_proxy_widgets_.at(i)->setGeometry(dock_rect);
				all_docked_proxy_widgets_.removeAt(i);
			}
		}
		DockLineup();
		docking_widget_proxy_->hide();
		docking_widget_proxy_->setVisible(false);
	}
}
