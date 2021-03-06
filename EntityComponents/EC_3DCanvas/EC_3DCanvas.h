// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_EC_3DCanvas_EC_3DCanvas_h
#define incl_EC_3DCanvas_EC_3DCanvas_h

#include "ComponentInterface.h"
#include "Declare_EC.h"

#include <QMap>

namespace Scene
{
    class Entity;
}

namespace Foundation
{
    class Framework;
}

namespace Ogre
{
    class TextureManager;
    class MaterialManager;
}

class QWidget;
class QTimer;

class EC_3DCanvas : public Foundation::ComponentInterface
{
    Q_OBJECT
    DECLARE_EC(EC_3DCanvas);

public:
    ~EC_3DCanvas();

public slots:
    void Start();
    void Setup(QWidget *widget, const QList<uint> &submeshes, int refresh_per_second);

    void SetWidget(QWidget *widget);
    void SetRefreshRate(int refresh_per_second);
    void SetSubmesh(uint submesh);
    void SetSubmeshes(const QList<uint> &submeshes);

    QWidget *GetWidget() { return widget_; }

private:
    explicit EC_3DCanvas(Foundation::ModuleInterface *module);
    void UpdateSubmeshes();

private slots:
    void WidgetDestroyed(QObject *obj);
    void Update();

private:
    QWidget *widget_;
    QList<uint> submeshes_;
    QTimer *refresh_timer_;

    QMap<uint, std::string> restore_materials_;
    std::string material_name_;
    std::string texture_name_;

    int update_interval_msec_;
    bool update_internals_;
};

#endif
