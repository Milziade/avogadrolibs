/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "pluginmanager.h"

#include "sceneplugin.h"
#include "extensionplugin.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMutex>
#include <QtCore/QPluginLoader>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtCore/QDebug>

namespace Avogadro {
namespace QtGui {

PluginManager::PluginManager(QObject *p) : QObject(p)
{
  // http://doc.qt.digia.com/qt/deployment-plugins.html#debugging-plugins
  bool debugPlugins = !qgetenv("QT_DEBUG_PLUGINS").isEmpty();

  // The usual base directory is the parent directory of the executable's
  // location. (exe is in "bin" or "MacOS" and plugins are under the parent
  // directory at "lib/avogadro2/plugins"...)
  //
  QDir baseDir(QCoreApplication::applicationDirPath() + "/..");
  if (debugPlugins)
    qDebug() << "  baseDir:" << baseDir.absolutePath();

#ifdef __APPLE__
  // But if NOT running from the installed bundle on the Mac, the plugins are
  // relative to the build directory instead:
  //
  if (!QFileInfo(baseDir.absolutePath() + "/Resources/qt.conf").exists()) {
    QDir buildDir(QCoreApplication::applicationDirPath() + "/../../../..");
    baseDir = buildDir;
    if (debugPlugins)
      qDebug() << "  using buildDir:" << buildDir.absolutePath();
  }
#endif

  QDir pluginsDir(baseDir.absolutePath() + "/lib/avogadro2/plugins");
  qDebug() << "  pluginsDir:" << pluginsDir.absolutePath();
  m_pluginDirs.append(pluginsDir.absolutePath());

  if (debugPlugins) {
    int count = 0;
    foreach(const QString &pluginPath, pluginsDir.entryList(QDir::Files)) {
      ++count;
      qDebug() << " " << pluginsDir.absolutePath() + "/" + pluginPath;
    }

    if (count > 0)
      qDebug() << " " << count << "files found in" << pluginsDir.absolutePath();
    else
      qDebug() << "  no plugin files found in" << pluginsDir.absolutePath();
  }
}

PluginManager::~PluginManager()
{
}

PluginManager * PluginManager::instance()
{
  static QMutex mutex;
  // Compiler initializes this static pointer to 0.
  static PluginManager *pluginManagerInstance;
  if (!pluginManagerInstance) {
    mutex.lock();
    if (!pluginManagerInstance)
      pluginManagerInstance = new PluginManager(QCoreApplication::instance());
    mutex.unlock();
  }
  return pluginManagerInstance;
}

void PluginManager::load()
{
  foreach(const QString &dir, m_pluginDirs)
    load(dir);
}

void PluginManager::load(const QString &path)
{
  QDir dir(path);
  foreach(const QString &pluginPath, dir.entryList(QDir::Files)) {
    QPluginLoader pluginLoader(dir.absolutePath() + "/" + pluginPath);

    // We only want to count plugins once, the || should not be necessary but
    // I found that on the Mac at least isLoaded was not always reliable (and
    // if it is we skip the second in the short-circuit).
    if (pluginLoader.isLoaded() || m_plugins.contains(pluginLoader.instance()))
      continue;

    QObject *pluginInstance = pluginLoader.instance();

    // Check if the plugin loaded correctly. Keep debug output for now, should
    // go away once we have verified this (or added to a logger).
    if (!pluginInstance) {
      qDebug() << "Failed to load" << pluginPath << "error"
               << pluginLoader.errorString();
      continue;
    }

    m_plugins.append(pluginInstance);
  }
}

QList<ScenePluginFactory *> PluginManager::scenePluginFactories() const
{
  return pluginFactories<ScenePluginFactory>();
}

QList<ExtensionPluginFactory *> PluginManager::extensionPluginFactories() const
{
  return pluginFactories<ExtensionPluginFactory>();
}

} // End QtGui namespace
} // End Avogadro namespace