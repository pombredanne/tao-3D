// ****************************************************************************
//  module_manager.cpp                                             Tao project
// ****************************************************************************
//
//   File Description:
//
//    Implementation of the ModuleManager class.
//
//
//
//
//
//
//
//
// ****************************************************************************
// This software is property of Taodyne SAS - Confidential
// Ce logiciel est la propriété de Taodyne SAS - Confidentiel
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************


#include "module_manager.h"
#include "tao/module_api.h"
#include "application.h"
#include "options.h"
#include "tao_utf8.h"
#include "parser.h"
#include "runtime.h"
#include "repository.h"
#include <QSettings>
#include <QHash>
#include <QMessageBox>

#include <unistd.h>    // For chdir()
#include <sys/param.h> // For MAXPATHLEN
#include <math.h>


namespace Tao {

ModuleManager * ModuleManager::instance = NULL;
ModuleManager::Cleanup ModuleManager::cleanup;


ModuleManager * ModuleManager::moduleManager()
// ----------------------------------------------------------------------------
//   Module manager factory
// ----------------------------------------------------------------------------
{
    if (!instance)
        instance = new ModuleManager;
    return instance;
}


XL::Tree_p ModuleManager::import(XL::Context_p context,
                                 XL::Tree_p self,
                                 XL::Tree_p what,
                                 bool execute)
// ----------------------------------------------------------------------------
//   The import primitive
// ----------------------------------------------------------------------------
{
    // import "filename"
    XL::Text *file = what->AsText();
    if (file)
        return XL::xl_import(context, self, file->value, execute);

    // Other import syntax: explicit module import
    ModuleManager *mmgr = moduleManager();
    if (mmgr)
        return mmgr->importModule(context, self, what, execute);

    return XL::xl_false;
}


XL::Tree_p ModuleManager::importModule(XL::Context_p context,
                                       XL::Tree_p self,
                                       XL::Tree_p what,
                                       bool execute)
// ----------------------------------------------------------------------------
//   The primitive to import a module, for example:   import ModuleName "1.10"
// ----------------------------------------------------------------------------
{
    XL::Tree *err = NULL;
    XL::Name *name = NULL;
    double m_v = 1.0;
    XL::Prefix *prefix = what->AsPrefix();

    if (prefix)
    {
        m_v = parseVersion(prefix->right);
        name = prefix->left->AsName();
    }
    else
    {
        name = what->AsName();
    }

    if (name && m_v > 0.0)
    {
        text m_n = name->value;
        bool found = false, name_found = false, version_found = false;
        bool enabled_found = false;
        double inst_v = 0.0;
        foreach (ModuleInfoPrivate m, modules)
        {
            if (m_n == m.importName)
            {
                name_found = true;
                inst_v = m.ver;
                if (versionMatches(m.ver, m_v))
                {
                    version_found = true;
                    if (!m.enabled)
                        continue;
                    enabled_found = true;
                    if (m.hasNative)
                    {
                        if (!m.native && !m.inError)
                        {
                            bool ok = loadNative(context, m);
                            m = *moduleById(m.id);
                            if (!ok)
                                break;
                        }
                        if (!m.native)
                            continue;
                    }
                    found = true;
                    QString xlPath = m.xlPath();

                    if (!execute)
                    {
                        IFTRACE(modules)
                            debug() << "  Importing module " << m_n
                                    << " version " << inst_v << " (requested "
                                    << m_v <<  "): " << +xlPath << "\n";
                        if (m.native)
                        {
                            // execute == false when file is [re]loaded
                            // => we won't call enter_symbols at each execution
                            enter_symbols_fn es =
                                (enter_symbols_fn)
                                m.native->resolve("enter_symbols");
                            if (es)
                            {
                                IFTRACE(modules)
                                    debug() << "    Calling enter_symbols\n";
                                es(context);
                            }
                        }
                    }

                    XL::xl_import(context, self, +xlPath, execute);
                    moduleById(m.id)->loaded = true;
                    break;
                }
            }
        }

        if (!found)
        {
            if (name_found)
            {
                if (version_found)
                {
                    if (enabled_found)
                    {
                        err = XL::Ooops("Module $1 load error", name);
                    }
                    else
                    {
                        err = XL::Ooops("Module $1 is disabled", name);
                    }
                }
                else
                {
                    err = XL::Ooops("Installed module $1 version $2 does not "
                                    "match requested version $3", name,
                                    new XL::Real(inst_v),
                                    prefix ? prefix->right
                                           : new XL::Real(m_v));
                }
            }
            else
            {
                err = XL::Ooops("Module $1 not found", name);
            }
        }
    }
    else
    {
        err = XL::Ooops("Invalid module import: $1", self);
    }

    if (err)
        return xl_evaluate(context, err);

    return XL::xl_true;
}


bool ModuleManager::init()
// ----------------------------------------------------------------------------
//   Initialize module manager, load/check/update user's module configuration
// ----------------------------------------------------------------------------
{
    if (!enabled())
        return false;

    IFTRACE(modules)
        debug() << "Initializing\n";

    if (!initPaths())
        return false;

    if (!loadConfig())
        return false;

    if (!checkNew())
        return false;

    if (!cleanConfig())
        return false;

    IFTRACE(modules)
    {
        debug() << "Updated module list before load\n";
        foreach (ModuleInfoPrivate m, modules)
            debugPrintShort(m);
    }

    return true;
}


bool ModuleManager::initPaths()
// ----------------------------------------------------------------------------
//   Setup per-user and system-wide module paths
// ----------------------------------------------------------------------------
{
#   define MODULES "/modules"
    u = Application::defaultTaoPreferencesFolderPath() + MODULES;
    u = QDir::toNativeSeparators(u);
    s = Application::defaultTaoApplicationFolderPath() + MODULES;
    s = QDir::toNativeSeparators(s);
#   undef MODULES

    IFTRACE(modules)
    {
        debug() << "User path:   " << +u << "\n";
        debug() << "System path: " << +s << "\n";
    }
    return true;
}


bool ModuleManager::loadConfig()
// ----------------------------------------------------------------------------
//   Read list of configured modules from user settings
// ----------------------------------------------------------------------------
{
    // The module settings are stored as child groups under the Modules main
    // group.
    //
    // Modules/<ID1>/Name = "Some module name"
    // Modules/<ID1>/Enabled = true
    // Modules/<ID2>/Name = "Other module name"
    // Modules/<ID2>/Enabled = false

    IFTRACE2(modules, settings)
        debug() << "Reading user's module configuration\n";

    QSettings settings;
    settings.beginGroup(USER_MODULES_SETTING_GROUP);
    QStringList ids = settings.childGroups();
    foreach(QString id, ids)
    {
        settings.beginGroup(id);

        QString name    = settings.value("Name").toString();
        bool    enabled = settings.value("Enabled").toBool();

        ModuleInfoPrivate m(+id, "", enabled);
        m.name = +name;
        modules[id] = m;

        IFTRACE(modules)
            debugPrintShort(m);

        settings.endGroup();
    }
    settings.endGroup();
    IFTRACE2(modules, settings)
        debug() << ids.size() << " module(s) configured\n";
    return true;
}


bool ModuleManager::saveConfig()
// ----------------------------------------------------------------------------
//   Save all modules into user's configuration
// ----------------------------------------------------------------------------
{
    IFTRACE(settings)
        debug() << "Saving user's module configuration\n";
    bool ok = true;
    foreach (ModuleInfoPrivate m, modules)
        ok &= addToConfig(m);
    IFTRACE(settings)
            debug() << modules.count() << " module(s) saved\n";
    return ok;
}


bool ModuleManager::removeFromConfig(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Remove m from the list of configured modules
// ----------------------------------------------------------------------------
{
    IFTRACE2(modules, settings)
        debug() << "Removing module " << m.toText() << " from configuration\n";

    modules.remove(+m.id);

    QSettings settings;
    settings.beginGroup(USER_MODULES_SETTING_GROUP);
    settings.remove(+m.id);
    settings.endGroup();

    IFTRACE2(modules, settings)
        debug() << "Module removed\n";

    return true;
}


bool ModuleManager::addToConfig(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Add m to the list of configured modules
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
        debug() << "Adding module " << m.toText() << " to configuration\n";

    modules[+m.id] = m;

    QSettings settings;
    settings.beginGroup(USER_MODULES_SETTING_GROUP);
    settings.beginGroup(+m.id);
    settings.setValue("Name", +m.name);
    settings.setValue("Enabled", m.enabled);
    settings.endGroup();
    settings.endGroup();

    return true;
}


bool ModuleManager::cleanConfig()
// ----------------------------------------------------------------------------
//   Check current module list, remove invalid modules, save config to disk
// ----------------------------------------------------------------------------
{
    foreach (ModuleInfoPrivate m, modules)
    {
        if (m.path == "" || m.id == "0")
        {
            removeFromConfig(m);
            continue;
        }
        ModuleInfoPrivate * m_p = moduleById(m.id);
        m_p->hasNative = QFile(m_p->libPath()).exists();
    }
    return true;
}


void ModuleManager::setEnabled(QString id, bool enabled)
// ----------------------------------------------------------------------------
//   Enable or disable a module
// ----------------------------------------------------------------------------
{
    ModuleInfoPrivate * m_p = moduleById(+id);
    if (!m_p)
        return;

    IFTRACE2(modules, settings)
        debug() << (enabled ? "En" : "Dis") << "abling module "
                << m_p->toText() << "\n";

    QSettings settings;
    settings.beginGroup(USER_MODULES_SETTING_GROUP);
    settings.beginGroup(id);
    settings.setValue("Enabled", enabled);
    settings.endGroup();
    settings.endGroup();

    bool prev = m_p->enabled;
    m_p->enabled = enabled;

    if (prev == enabled && !m_p->inError)
        return;
    if (!m_p->context)
        return;
    if (enabled && !m_p->loaded)
        load(m_p->context, *m_p);
}


bool ModuleManager::enabled(QString importName)
// ----------------------------------------------------------------------------
//   Return true if module can be used (exists and is enabled)
// ----------------------------------------------------------------------------
{
    foreach (ModuleInfoPrivate m, modules)
        if (m.enabled && m.importName == +importName)
            return true;
    return false;
}


bool ModuleManager::loadAll(Context *context)
// ----------------------------------------------------------------------------
//   Load all enabled modules for current user
// ----------------------------------------------------------------------------
{
    QList<ModuleInfoPrivate> toload;
    foreach (ModuleInfoPrivate m, modules)
    {
        modules[+m.id].context = context;
        if (m.enabled && !m.loaded && m.path != "")
            toload.append(m);
    }
    bool ok = load(context, toload);
    IFTRACE(modules)
    {
        debug() << "All modules\n";
        foreach (ModuleInfoPrivate m, modules)
            debugPrint(m);
    }
    return ok;
}


bool ModuleManager::loadAutoLoadModules(Context *context)
// ----------------------------------------------------------------------------
//   Load and init native code for all modules that are marked auto-load
// ----------------------------------------------------------------------------
{
    bool ok = true;
    foreach (ModuleInfoPrivate m, modules)
    {
        modules[+m.id].context = context;
        if (m.enabled && !m.loaded && m.path != "" && (m.importName == "" ||
                                                       m.autoLoad))
            ok &= loadNative(context, m);
    }
    return ok;
}


QStringList ModuleManager::anonymousXL()
// ----------------------------------------------------------------------------
//   Return list of all XL files for modules that have no import_name
// ----------------------------------------------------------------------------
{
    QStringList files;
    foreach (ModuleInfoPrivate m, modules)
    {
        if (m.enabled && m.path != "" && m.importName == "")
            files << m.xlPath();
    }
    return files;
}



QList<ModuleManager::ModuleInfoPrivate> ModuleManager::allModules()
// ----------------------------------------------------------------------------
//   Return a list of all configured modules
// ----------------------------------------------------------------------------
{
    QList<ModuleInfoPrivate> all;
    foreach (const ModuleInfoPrivate m, modules)
        all.append(m);
    return all;
}


bool ModuleManager::checkNew()
// ----------------------------------------------------------------------------
//   Check for new modules in user and system directories
// ----------------------------------------------------------------------------
{
    if (!checkNew(u))
        return false;
    if (!checkNew(s))
        return false;
    return true;
}


bool ModuleManager::checkNew(QString path)
// ----------------------------------------------------------------------------
//   Check for new modules in path
// ----------------------------------------------------------------------------
{
    bool ok = true;

    QList<ModuleInfoPrivate> mods = newModules(path);
    foreach (ModuleInfoPrivate m, mods)
    {
        if (askEnable(m, tr("New module")))
            m.enabled = true;
        addToConfig(m);
        IFTRACE(modules)
            debug() << "Module " << (m.enabled ? "en" : "dis") << "abled\n";
    }
    return ok;
}


QList<ModuleManager::ModuleInfoPrivate> ModuleManager::newModules(QString path)
// ----------------------------------------------------------------------------
//   Return the modules under path that are not in the user's configuration
// ----------------------------------------------------------------------------
//   For already configured modules, update path and all properties
{
    QList<ModuleManager::ModuleInfoPrivate> mods;

    IFTRACE(modules)
        debug() << "Checking for modules in " << +path << "\n";

    int known = 0, disabled = 0;
    QDir dir(path);
    if (dir.isReadable())
    {
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (QString d, subdirs)
        {
            QString moduleDir = QDir(dir.filePath(d)).absolutePath();
            IFTRACE(modules)
                debug() << "Checking " << +moduleDir << "\n";
            ModuleInfoPrivate m = readModule(moduleDir);
            if (m.id != "")
            {
                emit checking(+m.name);

                if (!modules.contains(+m.id))
                {
                    IFTRACE(modules)
                    {
                        debug() << "Found new module:\n";
                        debugPrint(m);
                    }
                    mods.append(m);
                }
                else
                {
                    ModuleInfoPrivate & existing = modules[+m.id];
                    if (existing.path != "")
                    {
                        IFTRACE(modules)
                        {
                            debug() << "WARNING: Duplicate module "
                                       "will be ignored:\n";
                            debugPrintShort(m);
                        }
                        warnDuplicateModule(m);
                    }
                    else
                    {
                        existing.path = m.path;
                        known ++;
                        existing.copyPublicProperties(m);
                        existing.qchFiles = m.qchFiles;
                        if (!m.enabled)
                            disabled ++;
                    }
                }
            }
        }
    }

    IFTRACE(modules)
        debug() << known << " known modules and "
                << mods.size() << " new module(s) found\n";

    return mods;
}


void ModuleManager::refreshModuleProperties(QString moduleDir)
// ----------------------------------------------------------------------------
//   Read module directory. If module is known, its refresh entry
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
        debug() << "Refreshing module properties for " << +moduleDir << "\n";

    ModuleInfoPrivate m = readModule(moduleDir);
    if (m.id != "" && modules.contains(+m.id))
    {
        ModuleInfoPrivate & existing = modules[+m.id];
        existing.copyPublicProperties(m);
    }
}


ModuleManager::ModuleInfoPrivate ModuleManager::readModule(QString moduleDir)
// ----------------------------------------------------------------------------
//   Read module directory and return module info. Set m.id == "" on error.
// ----------------------------------------------------------------------------
{
    ModuleInfoPrivate m("", +moduleDir);
    QString cause;
    QString xlPath = m.xlPath();
    if (QFileInfo(xlPath).isReadable())
    {
        if (XL::Tree * tree = parse(xlPath))
        {
            // If any mandatory attribute is missing, module is ignored
            QString id =  moduleAttr(tree, "id");
            QString name = moduleAttr(tree, "name");
            double ver = 1.0;
            FindAttribute findAttribute("module_description", "version");
            if (Tree *version = tree->Do(findAttribute))
                ver = parseVersion(version);
            if (id != "" && name != "" && ver > 0.0)
            {
                m = ModuleInfoPrivate(+id, +moduleDir);
                m.name = +name;
                m.ver = ver;
                m.desc = +moduleAttr(tree, "description");
                QString iconPath = QDir(moduleDir).filePath("icon.png");
                if (QFile(iconPath).exists())
                    m.icon = +iconPath;
                m.author = +moduleAttr(tree, "author");
                m.website = +moduleAttr(tree, "website");
                m.importName = +moduleAttr(tree, "import_name");
                m.autoLoad = (moduleAttr(tree, "auto_load") != "");
            }
            else
            {
                cause = tr("Missing ID, name or version");
            }
        }
        else
        {
            cause = tr("Could not parse %1").arg(xlPath);
        }
    }
    if (m.id != "")
    {
        // We have a valid module. Try to get its version from Git.
        double git_ver = parseVersion(+gitVersion(moduleDir));
        if (git_ver != -1)
            m.ver = git_ver;
        // Check if there is a pending update
        if (applyPendingUpdate(m))
        {
            git_ver = parseVersion(+gitVersion(moduleDir));
            if (git_ver != -1)
                m.ver = git_ver;
        }
        // Look for online documentation file
        QString qchPath = moduleDir + "/doc/" + TaoApp->lang + "/qch";
        QDir qchDir(qchPath);
        QStringList files = qchDir.entryList(QStringList("*.qch"),
                                             QDir::Files);
        if (files.isEmpty())
        {
            // Look for english by default
            qchPath = moduleDir + "/doc/en/qch";
            QDir qchDir(qchPath);
            files = qchDir.entryList(QStringList("*.qch"),
                                                 QDir::Files);
        }
        foreach(QString f, files)
            m.qchFiles << qchPath + "/" + f;
    }
    else
    {
        warnInvalidModule(moduleDir, cause);
    }
    return m;
}


bool ModuleManager::hasPendingUpdate(QString moduleDir)
// ----------------------------------------------------------------------------
//   Check if latest local tag is newer than current version
// ----------------------------------------------------------------------------
{
    bool hasUpdate = false;

    double current = parseVersion(+gitVersion(moduleDir));
    double latest_local = parseVersion(+latestTag(moduleDir));
    if (current != -1 && latest_local != -1)
        hasUpdate = (latest_local > current);

    IFTRACE(modules)
        debug() << (hasUpdate ? "Has" : "No") << " pending update\n";

    return hasUpdate;
}


QString ModuleManager::latestTag(QString moduleDir)
// ----------------------------------------------------------------------------
//   Return latest local tag or ""
// ----------------------------------------------------------------------------
{
    QString latest;
    RepositoryFactory::Mode mode = RepositoryFactory::OpenExistingHere;
    repository_ptr repo = RepositoryFactory::repository(moduleDir, mode);
    if (repo && repo->valid())
    {
        QStringList tags = repo->tags();
        if (!tags.isEmpty())
        {
            latest = tags[0];
            foreach (QString tag, tags)
                if (Repository::versionGreaterOrEqual(tag, latest))
                    latest = tag;
        }
    }
    return latest;
}


bool ModuleManager::applyPendingUpdate(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Checkout latest local tag if current version is not latest
// ----------------------------------------------------------------------------
{
    bool hasUpdate = hasPendingUpdate(+m.path);
    if (hasUpdate)
    {
        QString latest = latestTag(+m.path);
        double current = parseVersion(+gitVersion(+m.path));
        double latestVer = parseVersion(+latest);
        IFTRACE(modules)
            debug() << "Installing update: " << current
                    << " -> " << latestVer << "\n";
        emit updating(+m.name);
        RepositoryFactory::Mode mode = RepositoryFactory::OpenExistingHere;
        repository_ptr repo = RepositoryFactory::repository(+m.path, mode);
        repo->checkout(+latest);
        return true;
    }
    return false;
}


QString ModuleManager::gitVersion(QString moduleDir)
// ----------------------------------------------------------------------------
//   Try to find the version of the module using Git
// ----------------------------------------------------------------------------
{
    RepositoryFactory::Mode mode = RepositoryFactory::OpenExistingHere;
    repository_ptr repo = RepositoryFactory::repository(moduleDir, mode);
    if (repo && repo->valid())
        return +repo->version();
    return "";
}


XL::Tree * ModuleManager::parse(QString xlFile)
// ----------------------------------------------------------------------------
//   Parse the XL file of a module
// ----------------------------------------------------------------------------
{
    XL::Syntax          syntax (XL::MAIN->syntax);
    XL::Positions      &positions = XL::MAIN->positions;
    XL::Errors          errors;
    XL::Parser          parser((+xlFile).c_str(), syntax,positions,errors);

    return parser.Parse();
}


QString ModuleManager::moduleAttr(XL::Tree *tree, QString attribute)
// ----------------------------------------------------------------------------
//   Look for attribute value in module_description section of tree
// ----------------------------------------------------------------------------
{
    QString val;
    Tree * v = moduleAttrAsTree(tree, attribute);
    if (v)
    {
        Text_p t = toText(v);
        if (t)
            val = +(t->value);
    }
    return val;
}


Tree * ModuleManager::moduleAttrAsTree(XL::Tree *tree, QString attribute)
// ----------------------------------------------------------------------------
//   Look for attribute pointer in module_description section of tree
// ----------------------------------------------------------------------------
{
    // Look first in the localized description block, if present
    FindAttribute action("module_description", +attribute, +TaoApp->lang);
    Tree * t = tree->Do(action);
    if (!t)
    {
        FindAttribute action("module_description", +attribute);
        t = tree->Do(action);
    }
    return t;
}


Text * ModuleManager::toText(Tree *what)
// ----------------------------------------------------------------------------
//   Try to reduce 'what' to a single Text element (perform '&' concatenation)
// ----------------------------------------------------------------------------
{
    Text * txt = what->AsText();
    if (txt)
        return txt;
    Block * block = what->AsBlock();
    if (block)
        return toText(block->child);
    Infix * inf = what->AsInfix();
    if (inf && (inf->name == "&" || inf->name == "\n"))
    {
        Text * left = toText(inf->left);
        if (!left)
            return NULL;
        Text * right = toText(inf->right);
        if (!right)
            return NULL;
        return new Text(left->value + right->value);
    }
    return NULL;
}


bool ModuleManager::load(Context *ctx, const QList<ModuleInfoPrivate> &mods)
// ----------------------------------------------------------------------------
//   Load modules, in sequence
// ----------------------------------------------------------------------------
{
    bool ok = true;
    foreach(ModuleInfoPrivate m, mods)
        ok &= load(ctx, m);
    return ok;
}


bool ModuleManager::load(Context *context, const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Load one module
// ----------------------------------------------------------------------------
{
    bool ok = true;

    IFTRACE(modules)
        debug() << "Loading module " << m.toText() << "\n";

    ok = loadNative(context, m);
    if (ok)
        ok = loadXL(context, m);

    return ok;
}


bool ModuleManager::loadXL(Context */*context*/, const ModuleInfoPrivate &/*m*/)
// ----------------------------------------------------------------------------
//   Load the XL code of a module
// ----------------------------------------------------------------------------
{
    // Do not import module definitions at startup time but only on explicit
    // import (see importModule)
    // REVISIT: here we probably want to execute only a specific part of
    // the module's XL file
    return true;

#if 0
    IFTRACE(modules)
        debug() << "  Loading XL source\n";

    QString xlPath = m.xlPath();

    // module_description <indent block> evaluates as nil
    // REVISIT: bind a native function and use it to parse the block?
    Name *n = new Name("module_description");
    Block *b = new Block(new Name("x"), XL::Block::indent,
                         XL::Block::unindent);
    Prefix *from = new Prefix(n, b);
    Name *to = new Name("nil");
    context->Define(from, to);

    bool ok = XL::xl_import(context, +xlPath) != NULL;

    ModuleInfoPrivate *m_p = moduleById(m.id);
    if (ok && m_p)
        m_p->loaded = true;

    XL::source_files::iterator it = XL::MAIN->files.find(+xlPath);
    XL::source_files::iterator end = XL::MAIN->files.end();
    if (it != end)
    {
        XL::SourceFile &sf = (*it).second;
        Context *moduleContext = sf.context;

        if (context->Bound(new XL::Name("module_init")))
        {
            IFTRACE(modules)
                debug() << "    Calling XL module_init\n";
            XL::XLCall("module_init")(moduleContext);
        }
    }

    return ok;
#endif
}


struct SetCwd
// ----------------------------------------------------------------------------
//   Temporarily change current directory
// ----------------------------------------------------------------------------
{
    SetCwd(QString path)
    {
        IFTRACE(modules)
        {
            ModuleManager::debug() << "    Changing current directory to: "
                                   << +path << "\n";
        }
        if (getcwd(wd, sizeof(wd)))
            if (chdir(path.toStdString().c_str()) < 0)
                perror("Tao chdir");
    }
    ~SetCwd()
    {
        IFTRACE(modules)
            ModuleManager::debug() << "    Restoring current directory: "
                                   << wd << "\n";
        if (chdir(wd) < 0)
            perror("Tao chdir restore");
    }

    char wd[MAXPATHLEN];
};



bool ModuleManager::loadNative(Context * /*context*/,
                               const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Load the native code of a module (shared libraries under lib/)
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
        debug() << "  Looking for native library\n";

    ModuleInfoPrivate * m_p = moduleById(m.id);
    Q_ASSERT(m_p);
    bool ok;

    if (m_p->hasNative)
    {
        // Change current directory, just the time to load any module dependency
        QString path = m.libPath();
        QString libdir = QFileInfo(path).absolutePath();
        SetCwd cd(libdir);
        QLibrary * lib = new QLibrary(path, this);
        if (lib->load())
        {
            path = lib->fileName();
            IFTRACE(modules)
                    debug() << "    Loaded: " << +path << "\n";
            ok = isCompatible(lib);
            if (ok)
            {
                // enter_symbols is called later, when module is imported

                module_init_fn mi =
                        (module_init_fn) lib->resolve("module_init");
                if ((mi != NULL))
                {
                    IFTRACE(modules)
                            debug() << "    Calling module_init\n";
                    int st = mi(&api, &m);
                    if (st)
                    {
                        IFTRACE(modules)
                            debug() << "      Error (return code: "
                                    << st << ")\n";
                        return false;
                    }
                }

                m_p->show_preferences =
                    (module_preferences_fn) lib->resolve("show_preferences");
                if (m_p->show_preferences)
                {
                    IFTRACE(modules)
                        debug() << "    Resolved show_preferences function\n";
                }

                QTranslator * translator = new QTranslator;
                QString tr_name = m.dirname() + "_" + TaoApp->lang;
                if (translator->load(tr_name, +m.path))
                {
                    Application::installTranslator(translator);
                    m_p->translator = translator;
                    IFTRACE(modules)
                        debug() << "    Translations loaded: " << +tr_name
                                << "\n";
                }
                else
                {
                    delete translator;
                }

                m_p->native = lib;
                emit modulesChanged();
            }
        }
        else
        {
            IFTRACE(modules)
                debug() << "    Load error: " << +lib->errorString() << "\n";
            warnLibraryLoadError(+m.name, lib->errorString());
            delete lib;
            m_p->inError = true;
        }
    }
    else
    {
        IFTRACE(modules)
            debug() << "    Module has no native library\n";
    }
    return (m_p->hasNative && m_p->native);
}


bool ModuleManager::isCompatible(QLibrary * lib)
// ----------------------------------------------------------------------------
//   Return true if library was built with a compatible version of the Tao API
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
        debug() << "    Checking API compatibility\n";

    unsigned   current   = TAO_MODULE_API_CURRENT;
    unsigned   age       = TAO_MODULE_API_AGE;
    unsigned * m_current = (unsigned *) lib->resolve("module_api_current");
    if (!m_current)
    {
        IFTRACE(modules)
            debug() << "      Error: library has no version information\n";
        return false;
    }

    IFTRACE(modules)
        debug() << "      Module [current " << *m_current << "]  Tao [current "
                << current << " age " << age << "]\n";
    bool ok = ((current - age) <= *m_current && *m_current <= current);
    if (!ok)
    {
        IFTRACE(modules)
            debug() << "      Version mismatch!\n";
        warnBinaryModuleIncompatible(lib);
    }
    return ok;
}


std::ostream & ModuleManager::debug()
// ----------------------------------------------------------------------------
//   Convenience method to log with a common prefix
// ----------------------------------------------------------------------------
{
    std::cerr << "[Module Manager] ";
    return std::cerr;
}


void ModuleManager::debugPrint(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Convenience method to display a ModuleInfoPrivate object
// ----------------------------------------------------------------------------
{
    debug() << "  ID:         " <<  m.id << "\n";
    debug() << "  Path:       " <<  m.path << "\n";
    debug() << "  Name:       " <<  m.name << "\n";
    debug() << "  Import:     " <<  m.importName << "\n";
    debug() << "  Auto load:  " <<  m.autoLoad << "\n";
    debug() << "  Author:     " <<  m.author << "\n";
    debug() << "  Website:    " <<  m.website << "\n";
    debug() << "  Icon:       " <<  m.icon << "\n";
    debug() << "  Doc:        " << +m.qchFiles.join(" ");
    debug() << "  XL file:    " << +m.xlPath() << "\n";
    debug() << "  Version:    " <<  m.ver << "\n";
    debug() << "  Latest:     " <<  m.latest << "\n";
    debug() << "  Up to date: " << !m.updateAvailable << "\n";
    debug() << "  Enabled:    " <<  m.enabled << "\n";
    debug() << "  XL Loaded:  " <<  m.loaded << "\n";
    if (m.enabled)
    {
        debug() << "  Has native: " <<  m.hasNative << "\n";
        debug() << "  Lib loaded: " << (m.native != NULL) << "\n";
        if (m.native)
            debug() << "  Lib file:   " << +m.libPath() << "\n";
    }
    debug() << "  ------------------------------------------------\n";
}


void ModuleManager::debugPrintShort(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Display minimal information about a module
// ----------------------------------------------------------------------------
{
    debug() << "  ID:         " <<  m.id << "\n";
    debug() << "  Name:       " <<  m.name << "\n";
    debug() << "  Enabled:    " <<  m.enabled << "\n";
    debug() << "  ------------------------------------------------\n";
}


ModuleManager::ModuleInfoPrivate * ModuleManager::moduleById(text id)
// ----------------------------------------------------------------------------
//   Lookup a module info structure in the list of known modules
// ----------------------------------------------------------------------------
{
    if (modules.contains(+id))
        return &modules[+id];
    return NULL;
}



// ============================================================================
//
//   Virtual methods are the module manager's interface with the user
//
// ============================================================================

bool ModuleManager::askRemove(const ModuleInfoPrivate &m, QString reason)
// ----------------------------------------------------------------------------
//   Prompt user when we're about to remove a module from the configuration
// ----------------------------------------------------------------------------
{
#if 0
    QString msg = tr("Do you want to remove the following module from the "
                     "Tao configuration?\n\n"
                     "Name: %1\n"
                     "Location: %2").arg(m.name).arg(m.path);
    if (reason != "")
        msg.append(tr("\n\nReason: %1").arg(reason));

    int ret = QMessageBox::question(NULL, tr("Tao modules"), msg,
                                    QMessageBox::Yes | QMessageBox::No);
    return (ret == QMessageBox::Yes);
#endif
    (void)m; (void)reason;
    return true;
}


bool ModuleManager::askEnable(const ModuleInfoPrivate &m, QString reason)
// ----------------------------------------------------------------------------
//   Prompt user to know if a module should be enabled or disabled
// ----------------------------------------------------------------------------
{
#if 0
    QString msg = tr("Do you want to enable following module?\n\n"
                     "Name: %1\n"
                     "Location: %2").arg(m.name).arg(m.path);
    if (reason != "")
        msg.append(tr("\n\nReason: %1").arg(reason));

    int ret = QMessageBox::question(NULL, tr("Tao modules"), msg,
                                    QMessageBox::Yes | QMessageBox::No);
    return (ret == QMessageBox::Yes);
#endif
    (void)m; (void)reason;
    return true;
}


void ModuleManager::warnInvalidModule(QString moduleDir, QString cause)
// ----------------------------------------------------------------------------
//   Tell user of invalid module (will be ignored)
// ----------------------------------------------------------------------------
{
    std::cerr << +tr("WARNING: Skipping invalid module %1\n")
                 .arg(moduleDir);
    if (cause != "")
        std::cerr << +tr("WARNING:   %1\n").arg(cause);
}


void ModuleManager::warnDuplicateModule(const ModuleInfoPrivate &m)
// ----------------------------------------------------------------------------
//   Tell user of conflicting new module (will be ignored)
// ----------------------------------------------------------------------------
{
    (void)m;
}


void ModuleManager::warnLibraryLoadError(QString name, QString errorString)
// ----------------------------------------------------------------------------
//   Tell user that library failed to load (module will be ignored)
// ----------------------------------------------------------------------------
{
    QString msg = tr("Module %1 cannot be initialized.\n%2").arg(name)
                                                            .arg(errorString);
    QMessageBox::warning(NULL, tr("Tao modules"), msg);
}


void ModuleManager::warnBinaryModuleIncompatible(QLibrary *lib)
// ----------------------------------------------------------------------------
//   Tell user about incompatible binary module (will be ignored)
// ----------------------------------------------------------------------------
{
    std::cerr << +QObject::tr("WARNING: Skipping incompatible binary module ")
              << +lib->fileName() << "\n";
}


double ModuleManager::parseVersion(Tree *versionId)
// ----------------------------------------------------------------------------
//   Verify if we have a valid version number
// ----------------------------------------------------------------------------
//   Version numbers can have one of three forms:
//   - An integer value, e.g 1, which is the same as 1.0
//   - A real value, e.g.  1.0203,
//   - A text value, e.g. "1.0203"
{
    if (Integer *iver = versionId->AsInteger())
        return iver->value;
    if (Real *rver = versionId->AsReal())
        return rver->value;
    if (Text *tver = versionId->AsText())
        return parseVersion(tver->value);

    XL::Ooops("Malformed version number $1", versionId);
    return 1.0;
}


double ModuleManager::parseVersion(text versionId)
// ----------------------------------------------------------------------------
//    Parse the text form of version numbers
// ----------------------------------------------------------------------------
{
    double ver = -1.0;
    kstring sver = versionId.c_str();
    sscanf(sver, "%lf", &ver);
    return ver;
}


bool ModuleManager::versionGreaterOrEqual(text ver, text ref)
// ----------------------------------------------------------------------------
//    Return true if ver >= ref
// ----------------------------------------------------------------------------
{
    double v = parseVersion(ver), r = parseVersion(ref);
    return (v >= r);
}


bool ModuleManager::versionMatches(double ver, double ref)
// ----------------------------------------------------------------------------
//   Return true if ver.major == ref.major and ver.minor >= ref.minor
// ----------------------------------------------------------------------------
{
    double verMajor = floor(ver);
    double refMajor = floor(ref);
    double verMinor = ver - verMajor;
    double refMinor = ref - refMajor;
    return verMajor == refMajor && verMinor >= refMinor;
}


QStringList ModuleManager::qchFiles()
// ----------------------------------------------------------------------------
//   Return documentation files (*.qch) of all enabled modules
// ----------------------------------------------------------------------------
{
    QStringList files;
    foreach (ModuleInfoPrivate m, modules)
        if (m.enabled)
            files << m.qchFiles;
    return files;
}

// ============================================================================
//
//   Checking if a module has new updates
//
// ============================================================================

bool CheckForUpdate::start()
// ----------------------------------------------------------------------------
//   Initiate "check for update" process for a module
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
    {
        debug() << "Start checking for updates, module "
                << m.toText() << "\n";
        debug() << "Current version " << m.ver << "\n";
    }

    bool inProgress = false;
    repo = RepositoryFactory::repository(+m.path,
                                         RepositoryFactory::OpenExistingHere);
    if (repo && repo->valid())
    {
        QStringList tags = repo->tags();
        if (!tags.isEmpty())
        {
            foreach (QString t, tags)
            {
                double tval = ModuleManager::parseVersion(+t);
                if (m.ver == tval)
                {
                    proc = repo->asyncGetRemoteTags("origin");
                    connect(repo.data(),
                            SIGNAL(asyncGetRemoteTagsComplete(QStringList)),
                            this,
                            SLOT(processRemoteTags(QStringList)));
                    repo->dispatch(proc);
                    inProgress = true;
                    break;
                }
            }
            if (!inProgress)
            {
                IFTRACE(modules)
                    debug() << "N/A (current module version does not match "
                               "a tag)\n";
            }
        }
        else
        {
            IFTRACE(modules)
                debug() << "N/A (no local tags)\n";
        }
    }
    else
    {
        IFTRACE(modules)
            debug() << "N/A (not a Git repository)\n";
    }

    if (!inProgress)
        emit complete(m, false);

    return true;
}


void CheckForUpdate::processRemoteTags(QStringList tags)
// ----------------------------------------------------------------------------
//   Process the list of remote tags for module in currentModuleDir
// ----------------------------------------------------------------------------
{
    IFTRACE(modules)
        debug() << "Module " << m.toText() << "\n";

    bool hasUpdate = false;
    if (!tags.isEmpty())
    {
        double current = m.ver;
        QString latest = tags[0];
        foreach (QString tag, tags)
            if (ModuleManager::versionGreaterOrEqual(+tag, +latest))
                latest = tag;

        double latestVer = ModuleManager::parseVersion(+latest);
        mm.modules[+m.id].latest = latestVer;

        hasUpdate = (latestVer > current);

        IFTRACE(modules)
        {
            debug() << "  Remote tags: " << +tags.join(" ")
                    << " latest " << +latest << "\n";
            debug() << "  Current " << +current << "\n";
            debug() << "  Needs update: " << (hasUpdate ? "yes" : "no") << "\n";
        }

    }
    else
    {
        IFTRACE(modules)
            debug() << "  No remote tags\n";
    }

    mm.modules[+m.id].updateAvailable = hasUpdate;
    emit complete(m, hasUpdate);
    deleteLater();
}



// ============================================================================
//
//   Checking if any module has an update
//
// ============================================================================

bool CheckAllForUpdate::start()
// ----------------------------------------------------------------------------
//   Start "check for updates" for all modules
// ----------------------------------------------------------------------------
{
    // Signal checkForUpdateComplete(bool need) will be emitted
    bool ok = true;

    QList<ModuleManager::ModuleInfoPrivate> modules = mm.allModules();
    num = modules.count();
    if (!num)
        return false;

    emit minimum(0);
    emit maximum(num);

    foreach (ModuleManager::ModuleInfoPrivate m, modules)
        pending << +m.id;

    foreach (ModuleManager::ModuleInfoPrivate m, modules)
    {
        CheckForUpdate *cfu = new CheckForUpdate(mm, +m.id);
        connect(cfu, SIGNAL(complete(ModuleManager::ModuleInfoPrivate,bool)),
                this,
                SLOT(processResult(ModuleManager::ModuleInfoPrivate,bool)));
        if (!cfu->start())
            ok = false;
    }
    return ok;
}


void CheckAllForUpdate::processResult(ModuleManager::ModuleInfoPrivate m,
                                      bool updateAvailable)
// ----------------------------------------------------------------------------
//   Remove module from pending list and emit result on last module
// ----------------------------------------------------------------------------
{
    if (updateAvailable)
        this->updateAvailable = true;
    pending.remove(+m.id);
    emit progress(num - pending.count());
    if (pending.isEmpty())
    {
        emit complete(this->updateAvailable);
        deleteLater();
    }
}



// ============================================================================
//
//   Updating one module to the latest version
//
// ============================================================================

bool UpdateModule::start()
// ----------------------------------------------------------------------------
//   Start update (download and install) of one module
// ----------------------------------------------------------------------------
{
    bool ok = false;

    if (m.path != "")
    {
        repo = RepositoryFactory::repository(+m.path);
        if (repo && repo->valid())
        {
            proc = repo->asyncFetch("origin");
            connect(proc.data(), SIGNAL(finished(int,QProcess::ExitStatus)),
                    this,        SLOT(onFinished(int,QProcess::ExitStatus)));
            connect(proc.data(), SIGNAL(percentComplete(int)),
                    this,        SIGNAL(progress(int)));
            repo->dispatch(proc);
            ok = true;
        }
    }

    return ok;
}


void UpdateModule::onFinished(int exitCode, QProcess::ExitStatus status)
// ----------------------------------------------------------------------------
//   emit complete signal
// ----------------------------------------------------------------------------
{
    bool ok = (status ==  QProcess::NormalExit && exitCode == 0);
    if (ok)
    {
        // NB: Defer checkout on next restart, because module may be in use
        ModuleManager::ModuleInfoPrivate *p = mm.moduleById(m.id);
        p->updateAvailable = false;
    }
    emit complete(ok);
}

}
