import rexviewer as r
from circuits import Component
import PythonQt

instance = None

class LoadURLHandler(Component):
    def __init__(self):
        Component.__init__(self)
        self.proxywidget = None
        self.webview = None
        
        global instance
        instance = self

    def loadurl(self, url):
        uism = r.getUiSceneManager()
        uiprops = r.createUiWidgetProperty(2) #2 == scene(?) widget or so
        uiprops.widget_name_ = 'LoadURL'
        
        self.webview = PythonQt.QtWebKit.QWebView()
        self.proxywidget = r.createUiProxyWidget(self.webview, uiprops)
        
        if not uism.AddProxyWidget(self.proxywidget):
            r.logError('LoadURLHandler: Adding the ProxyWidget to the scene failed.')
            return
            
        self.proxywidget.show()
        self.webview.load(url)

    def on_exit(self):
        r.logInfo('LoadURLHandler exiting..')
        if self.proxywidget is not None:
            self.proxywidget.hide()
            uism = r.getUiSceneManager()
            uism.RemoveProxyWidgetFromScene(self.proxywidget)

def loadurl(urlstring):
    url = PythonQt.QtCore.QUrl(urlstring)
    if instance is not None:
        instance.loadurl(url)
    else:
        print "ERROR: loadurl handler wasn't there!"

