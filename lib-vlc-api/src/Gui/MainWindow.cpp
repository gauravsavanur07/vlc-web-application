/*****************************************************************************
 * MainWindow.cpp: VLMC MainWindow
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QSizePolicy>
#include <QDockWidget>
#include <QFileDialog>
#include <QSlider>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUndoView>
#include <QUrl>
#include <QNetworkProxy>
#include <QSysInfo>
#include <QScrollArea>
#include <QProgressBar>
#include "Main/Core.h"
#include "Project/Project.h"
#include "Library/Library.h"
#include "Tools/VlmcDebug.h"
#include "Tools/VlmcLogger.h"
#include "Backend/IBackend.h"
#include "Workflow/MainWorkflow.h"
#include "Renderer/ClipRenderer.h"
#include "Commands/AbstractUndoStack.h"

/* Gui */
#include "MainWindow.h"
#include "About.h"
#include "export/RendererSettings.h"
#include "export/ShareOnInternet.h"
#include "settings/SettingsDialog.h"

/* Widgets */
#include "effectsengine/EffectsListView.h"
#include "transition/TransitionsListView.h"
#include "library/MediaLibraryView.h"
#include "library/ClipLibraryView.h"
#include "preview/PreviewWidget.h"
#include "timeline/Timeline.h"

/* Settings / Preferences */
#include "Project/RecentProjects.h"
#include "wizard/ProjectWizard.h"
#include "Settings/Settings.h"
#include "LanguageHelper.h"
#include "Commands/KeyboardShortcutHelper.h"

MainWindow::MainWindow( Backend::IBackend* backend, QWidget *parent )
    : QMainWindow( parent )
    , m_backend( backend )
    , m_projectPreferences( nullptr )
    , m_wizard( nullptr )
{
    m_ui.setupUi( this );

    Core::instance()->logger()->setup();
    //Preferences
    initVlmcPreferences();

    // GUI
    createGlobalPreferences();
    initToolbar();
    createStatusBar();
    initializeDockWidgets();
    checkFolders();
    loadGlobalProxySettings();
    createProjectPreferences();
    updateRecentProjects();

#ifdef WITH_CRASHBUTTON
    setupCrashTester();
#endif

    connect( Core::instance()->project(), &Project::projectNameChanged,
             this, &MainWindow::projectNameChanged );
    connect( Core::instance()->project(), &Project::outdatedBackupFileFound,
             this, &MainWindow::onOudatedBackupFile );
    connect( Core::instance()->project(), &Project::backupProjectLoaded,
             this, &MainWindow::onBackupFileLoaded );
    connect( Core::instance()->project(), &Project::projectSaved,
             this, &MainWindow::onProjectSaved );
    connect( Core::instance()->project(), &Project::cleanStateChanged,
             this, &MainWindow::cleanStateChanged );
    connect( Core::instance()->recentProjects(), &RecentProjects::updated,
             this, &MainWindow::updateRecentProjects );

    // Connect the medialibrary before it starts discovering/reloading
    connect( Core::instance()->library(), &Library::progressUpdated, this,
             [this]( int percent ) {
        if ( percent < 100 )
        {
            m_progressBar->setValue( percent );
            m_ui.statusbar->show();
            m_ui.actionStatusbar->setEnabled( false );
            m_ui.actionStatusbar->setChecked( true );
        }
        else
        {
            m_progressBar->hide();
            m_ui.statusbar->clearMessage();
            m_ui.actionStatusbar->setEnabled( true );
            if ( VLMC_GET_BOOL( "private/ShowStatusbar" ) == false )
            {
                m_ui.actionStatusbar->setChecked( false );
                m_ui.statusbar->hide();
            }
        }
    });

    connect( Core::instance()->library(), &Library::discoveryProgress, this,
        [this](const QString& folder) {
            m_ui.statusbar->showMessage( "Discovering " + folder + "...", 2500 );
    }, Qt::QueuedConnection );
    connect( Core::instance()->library(), &Library::discoveryCompleted, this,
        [this]( const QString& ) {
            m_ui.statusbar->showMessage( "Analyzing media" );
    }, Qt::QueuedConnection );

#ifdef WITH_CRASHHANDLER
    if ( restoreSession() == true )
        return ;
#endif

    //All preferences have been created: restore them:
    Core::instance()->settings()->load();

    // Restore the geometry
    restoreGeometry( VLMC_GET_BYTEARRAY( "private/MainWindowGeometry" ) );
    // Restore the layout
    restoreState( VLMC_GET_BYTEARRAY( "private/MainWindowState" ) );
    retranslateUi();
    projectNameChanged( Project::unNamedProject );
}

MainWindow::~MainWindow()
{
    m_projectPreview->stop();
    m_clipPreview->stop();
    delete m_timeline;
}

void
MainWindow::showWizard()
{
    if ( m_wizard == nullptr )
    {
        m_wizard = new ProjectWizard( this );
        m_wizard->setModal( true );
    }
    m_wizard->show();
}

void
MainWindow::retranslateUi()
{
    m_dockedUndoView->setWindowTitle( tr( "History" ) );
    m_dockedEffectsList->setWindowTitle( tr( "Effects List" ) );
    m_dockedTransitionsList->setWindowTitle( tr( "Transitions List" ) );
    m_dockedMediaLibrary->setWindowTitle( tr( "Media Library" ) );
    m_dockedClipLibrary->setWindowTitle( tr( "Clip Library" ) );
    m_dockedClipPreview->setWindowTitle( tr( "Clip Preview" ) );
    m_dockedProjectPreview->setWindowTitle( tr( "Project Preview" ) );
}

void
MainWindow::changeEvent( QEvent *e )
{
    switch ( e->type() )
    {
    case QEvent::LanguageChange:
        // Translate m_ui as a separate step to avoid calling it twice upon first
        // creation. We call retranslateUi() from MainWindow's constructor, but we also initialize
        // m_ui, which calls m_ui.retranslateUi()
        m_ui.retranslateUi( this );
        retranslateUi();
        break;
    case QEvent::WindowStateChange:
        if ( isFullScreen() )
            m_ui.actionFullscreen->setChecked( true );
        else
            m_ui.actionFullscreen->setChecked( false );
        break;
    default:
        break;
    }
}

//use this helper when the shortcut is binded to a menu action
#define CREATE_MENU_SHORTCUT( key, defaultValue, name, desc, actionInstance  )      \
        VLMC_CREATE_PREFERENCE_KEYBOARD( key, defaultValue, name, desc );           \
        new KeyboardShortcutHelper( key, m_ui.actionInstance, this );

void
MainWindow::initVlmcPreferences()
{
    //Setup VLMC Keyboard Preference...
    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/mediapreview", "Ctrl+Return",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Media preview" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Preview the selected media, or pause the current preview" ) );

    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/renderpreview", "Space",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Render preview" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Preview the project, or pause the current preview" ) );

    //A bit nasty, but we better use what Qt's providing as default shortcut
    CREATE_MENU_SHORTCUT( "keyboard/undo",
                          QKeySequence( QKeySequence::Undo ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Undo" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Undo the last action" ), actionUndo );

    CREATE_MENU_SHORTCUT( "keyboard/redo",
                          QKeySequence( QKeySequence::Redo ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Redo" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Redo the last action" ), actionRedo );

    CREATE_MENU_SHORTCUT( "keyboard/help",
                          QKeySequence( QKeySequence::HelpContents ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Help" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Toggle the help page" ), actionHelp );

    CREATE_MENU_SHORTCUT( "keyboard/quit", "Ctrl+Q",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Quit" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Quit VLMC" ), actionQuit );

    CREATE_MENU_SHORTCUT( "keyboard/preferences", "Alt+P",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Preferences" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open VLMC preferences" ), actionPreferences );

    CREATE_MENU_SHORTCUT( "keyboard/projectpreferences", "Ctrl+P",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Project preferences" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open the project preferences"), actionProject_Preferences );

    CREATE_MENU_SHORTCUT( "keyboard/fullscreen", "F",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Fullscreen" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Switch to fullscreen mode" ), actionFullscreen );

    CREATE_MENU_SHORTCUT( "keyboard/newproject",
                          QKeySequence( QKeySequence::New ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "New project" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open the new project wizard" ), actionNew_Project );

    CREATE_MENU_SHORTCUT( "keyboard/openproject",
                          QKeySequence( QKeySequence::Open ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open a project" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open an existing project" ), actionLoad_Project );

    CREATE_MENU_SHORTCUT( "keyboard/save",
                          QKeySequence( QKeySequence::Save ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Save" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Save the current project" ), actionSave );

    CREATE_MENU_SHORTCUT( "keyboard/saveas", "Ctrl+Shift+S",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Save as" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Save the current project to a new file" ), actionSave_As );

    CREATE_MENU_SHORTCUT( "keyboard/closeproject",
                          QKeySequence( QKeySequence::Close ).toString().toLocal8Bit(),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Close the project" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Close the current project" ), actionClose_Project );

    CREATE_MENU_SHORTCUT( "keyboard/importmedia", "Ctrl+I",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Import media" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Open the import window" ), actionImport );

    CREATE_MENU_SHORTCUT( "keyboard/renderproject", "Ctrl+R",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Render the project" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Render the project to a file" ), actionRender );

    CREATE_MENU_SHORTCUT( "keyboard/defaultmode", "n",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Selection mode" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Select the selection tool in the timeline" ), actionSelection_mode );

    CREATE_MENU_SHORTCUT( "keyboard/cutmode", "x",
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Cut mode" ),
                          QT_TRANSLATE_NOOP( "PreferenceWidget", "Select the cut/razor tool in the timeline" ), actionCut_mode );

    //Setup VLMC Lang. Preferences...
    SettingValue* lang = VLMC_CREATE_PREFERENCE_LANGUAGE( "vlmc/VLMCLang", "default",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Language" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "The VLMC's UI language" ) );

    connect( lang, SIGNAL( changed( QVariant ) ),
             LanguageHelper::instance(), SLOT( languageChanged( const QVariant& ) ) );

    //Setup VLMC General Preferences...
    VLMC_CREATE_PREFERENCE_BOOL( "vlmc/ConfirmDeletion", true,
                                 QT_TRANSLATE_NOOP( "PreferenceWidget", "Confirm clip deletion"),
                                 QT_TRANSLATE_NOOP( "PreferenceWidget", "Ask for confirmation before deleting a clip from the timeline" ) );

    VLMC_CREATE_PREFERENCE_PATH( "vlmc/TempFolderLocation", QDir::tempPath() + "/VLMC/",
                                    QT_TRANSLATE_NOOP( "PreferenceWidget", "Temporary folder" ),
                                    QT_TRANSLATE_NOOP( "PreferenceWidget", "The temporary folder used by VLMC to process videos." ) );

    //Setup VLMC Youtube Preference...
    VLMC_CREATE_PREFERENCE_STRING( "youtube/DeveloperKey", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Youtube Developer Key" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "VLMC's Youtube Developer Key" ) );

    VLMC_CREATE_PREFERENCE_STRING( "youtube/Username", "username",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Youtube Username" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Valid YouTube username" ) );

    VLMC_CREATE_PREFERENCE_PASSWORD( "youtube/Password", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Youtube Password" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Leave this field empty, password will be stored in unencrypted form." ) );

    //Setup VLMC Proxy Settings
    VLMC_CREATE_PREFERENCE_BOOL( "network/UseProxy", false,
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Enable Proxy for VLMC"),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Enables Global Network Proxy for VLMC." ) );

    VLMC_CREATE_PREFERENCE_STRING( "network/ProxyHost", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Proxy Hostname" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Set Proxy Hostname." ) );

    VLMC_CREATE_PREFERENCE_STRING( "network/ProxyPort", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Proxy Port" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Set Proxy Port." ) );

    VLMC_CREATE_PREFERENCE_STRING( "network/ProxyUsername", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Proxy Username" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Set Proxy Username, if any." ) );

    VLMC_CREATE_PREFERENCE_PASSWORD( "network/ProxyPassword", "",
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Proxy Password" ),
                                     QT_TRANSLATE_NOOP( "PreferenceWidget", "Set Proxy Password, if any." ) );

    // Setup private variables
    VLMC_CREATE_PRIVATE_PREFERENCE_BOOL( "private/CleanQuit", true );
    VLMC_CREATE_PRIVATE_PREFERENCE_STRING( "private/EmergencyBackup", "" );
    VLMC_CREATE_PRIVATE_PREFERENCE_STRING( "private/ImportPreviouslySelectPath", QDir::homePath() );
    VLMC_CREATE_PRIVATE_PREFERENCE_BYTEARRAY( "private/MainWindowGeometry", "" );
    VLMC_CREATE_PRIVATE_PREFERENCE_BYTEARRAY( "private/MainWindowState", "" );

    // Toolbar
    auto toolbarSetting = VLMC_CREATE_PRIVATE_PREFERENCE_BOOL( "private/ShowToolbar", false );
    connect( toolbarSetting, &SettingValue::changed, this, [this]( const QVariant& var )
    {
        bool v = var.toBool();
        m_ui.toolBar->setVisible( v );
        m_ui.actionToolbar->setChecked( v );
    } );
    connect( m_ui.actionToolbar, &QAction::toggled, toolbarSetting, &SettingValue::set );

    // Statusbar
    auto statusbarSetting = VLMC_CREATE_PRIVATE_PREFERENCE_BOOL( "private/ShowStatusbar", false );
    connect( statusbarSetting, &SettingValue::changed, this, [this]( const QVariant& var )
    {
        bool v = var.toBool();
        m_ui.statusbar->setVisible( v );
        m_ui.actionStatusbar->setChecked( v );
    } );
    connect( m_ui.actionStatusbar, &QAction::triggered, statusbarSetting, &SettingValue::set );

    // Layout Lock
    auto lockSetting = VLMC_CREATE_PRIVATE_PREFERENCE_BOOL( "private/LayoutLock", true );
    connect( lockSetting, &SettingValue::changed, this, [this]( const QVariant& var )
    {
        bool v = var.toBool();
        for ( auto w : findChildren<QDockWidget *>() )
            if ( v == true )
                w->setTitleBarWidget( new QWidget );
            else
                w->setTitleBarWidget( 0 );
         m_ui.actionLock->setChecked( v );
    } );
    connect( m_ui.actionLock, &QAction::toggled, lockSetting, &SettingValue::set );
}

#undef CREATE_MENU_SHORTCUT

void
MainWindow::on_actionSave_triggered()
{
    if ( Core::instance()->project()->hasProjectFile() == false )
        on_actionSave_As_triggered();
    else
        Core::instance()->project()->save();
}

void
MainWindow::on_actionSave_As_triggered()
{
    auto path = Core::instance()->recentProjects()->mostRecentProjectFile();

    if ( path.isEmpty() == true )
        path = VLMC_GET_STRING( "vlmc/WorkspaceLocation" );

    QString dest = QFileDialog::getSaveFileName( nullptr, QObject::tr( "Enter the output file name" ),
                                  path, QObject::tr( "VLMC project file(*.vlmc)" ) );
    if ( dest.isEmpty() == true )
        return;
    if ( !dest.endsWith( ".vlmc" ) )
        dest += ".vlmc";
    Core::instance()->project()->saveAs( dest );
}

void
MainWindow::on_actionLoad_Project_triggered()
{
    QString folder = VLMC_GET_STRING( "vlmc/WorkspaceLocation" );
    QString fileName = QFileDialog::getOpenFileName( nullptr, tr( "Please choose a project file" ),
                                    folder, tr( "VLMC project file(*.vlmc)" ) );
    if ( fileName.isEmpty() == true )
        return ;
    Core::instance()->project()->load( fileName );
}

void
MainWindow::createStatusBar()
{
    m_progressBar = new QProgressBar( this );
    m_ui.statusbar->addPermanentWidget( m_progressBar );
    m_progressBar->show();

    // Spacer
    QWidget* spacer = new QWidget( this );
    spacer->setFixedWidth( 20 );
    m_ui.statusbar->addPermanentWidget( spacer );

    // Zoom Out
    QToolButton* zoomOutButton = new QToolButton( this );
    zoomOutButton->setIcon( QIcon( ":/images/zoomout" ) );
    zoomOutButton->setStatusTip( tr( "Zoom out" ) );
    m_ui.statusbar->addPermanentWidget( zoomOutButton );
    connect( zoomOutButton, SIGNAL( clicked() ),
             this, SLOT( zoomOut() ) );

    // Zoom slider
    m_zoomSlider = new QSlider( this );
    m_zoomSlider->setOrientation( Qt::Horizontal );
    m_zoomSlider->setTickInterval( 1 );
    m_zoomSlider->setSingleStep( 1 );
    m_zoomSlider->setPageStep( 1 );
    m_zoomSlider->setMinimum( 0 );
    m_zoomSlider->setMaximum( 9 );
    m_zoomSlider->setValue( 4 );
    m_zoomSlider->setFixedWidth( 80 );
    m_zoomSlider->setInvertedAppearance( true );
    m_ui.statusbar->addPermanentWidget( m_zoomSlider );

    // Zoom IN
    QToolButton* zoomInButton = new QToolButton( this );
    zoomInButton->setIcon( QIcon( ":/images/zoomin" ) );
    zoomInButton->setStatusTip( tr( "Zoom in" ) );
    m_ui.statusbar->addPermanentWidget( zoomInButton );
    connect( zoomInButton, SIGNAL( clicked() ),
             this, SLOT( zoomIn() ) );
    connect( m_zoomSlider, &QSlider::valueChanged, this, [this]( int val )
    {
        emit scaleChanged( val );
    } );
}

void
MainWindow::initializeDockWidgets()
{
    m_timeline = new Timeline( Core::instance()->project()->settings(), this );
    setCentralWidget( m_timeline->container() );

    setupLibrary();
    setupEffectsList();
    setupTransitionsList();
    setupClipPreview();
    setupProjectPreview();
    setupUndoRedoWidget();
}

void
MainWindow::setupUndoRedoWidget()
{
    m_undoView = new QUndoView;
    m_undoView->setObjectName( QStringLiteral( "History" ) );
    m_dockedUndoView = dockWidget( m_undoView, Qt::TopDockWidgetArea );
    auto stack = Core::instance()->workflow()->undoStack();
    connect( stack, SIGNAL( canUndoChanged( bool ) ), this, SLOT( canUndoChanged( bool ) ) );
    connect( stack, SIGNAL( canRedoChanged( bool ) ), this, SLOT( canRedoChanged( bool ) ) );
    canUndoChanged( stack->canUndo() );
    canRedoChanged( stack->canRedo() );
    m_undoView->setStack( stack );
}

void
MainWindow::setupEffectsList()
{
    m_effectsList = new EffectsListView( this );
    m_dockedEffectsList = dockWidget( m_effectsList->container(), Qt::TopDockWidgetArea );
}

void
MainWindow::setupTransitionsList()
{
    m_transitionsList = new TransitionsListView( this );
    m_dockedTransitionsList = dockWidget( m_transitionsList->container(), Qt::TopDockWidgetArea );
}

void
MainWindow::setupLibrary()
{
    m_mediaLibrary = new MediaLibraryView( this );
    m_clipLibrary = new ClipLibraryView( this );
    m_dockedMediaLibrary = dockWidget( m_mediaLibrary->container(), Qt::TopDockWidgetArea );
    m_dockedClipLibrary = dockWidget( m_clipLibrary->container(), Qt::TopDockWidgetArea );
}

void
MainWindow::setupClipPreview()
{
    m_clipPreview = new PreviewWidget;
    auto renderer = new ClipRenderer;
    renderer->setParent( m_clipPreview );
    m_clipPreview->setRenderer( renderer );
    connect( Core::instance()->library(), &Library::clipRemoved,
             renderer, &ClipRenderer::clipUnloaded );
    connect( m_clipLibrary, &ClipLibraryView::clipSelected, renderer, [renderer]( const QString& uuid )
    {
        renderer->setClip( Core::instance()->library()->clip( uuid ) );
    } );
    connect( m_mediaLibrary, &MediaLibraryView::baseClipSelected, renderer, [renderer]( const QString& uuid )
    {
        renderer->setClip( Core::instance()->library()->clip( uuid ) );
    } );


    KeyboardShortcutHelper* clipShortcut = new KeyboardShortcutHelper( "keyboard/mediapreview", this );
    connect( clipShortcut, SIGNAL( activated() ), m_clipPreview, SLOT( on_pushButtonPlay_clicked() ) );
    m_dockedClipPreview = dockWidget( m_clipPreview, Qt::TopDockWidgetArea );
}

void
MainWindow::setupProjectPreview()
{
    m_projectPreview = new PreviewWidget;
    m_projectPreview->setClipEdition( false );
    m_projectPreview->setRenderer( Core::instance()->workflow()->renderer() );
    KeyboardShortcutHelper* renderShortcut = new KeyboardShortcutHelper( "keyboard/renderpreview", this );
    connect( renderShortcut, SIGNAL( activated() ), m_projectPreview, SLOT( on_pushButtonPlay_clicked() ) );
    m_dockedProjectPreview = dockWidget( m_projectPreview, Qt::TopDockWidgetArea );
}

void
MainWindow::initToolbar()
{
    QActionGroup    *mouseActions = new QActionGroup( m_ui.toolBar );
    mouseActions->addAction( m_ui.actionSelection_mode );
    mouseActions->addAction( m_ui.actionCut_mode );
    m_ui.actionSelection_mode->setChecked( true );
    m_ui.toolBar->addActions( mouseActions->actions() );
    connect( mouseActions, SIGNAL( triggered( QAction* ) ),
             this, SLOT( toolButtonClicked( QAction* ) ) );
    m_ui.menuTools->addActions( mouseActions->actions() );
#if defined ( Q_OS_MAC )
    // Use native fullscreen on OS X >= LION
    if ( QSysInfo::macVersion() >= QSysInfo::MV_LION ) {
        m_ui.actionFullscreen->setEnabled( false );
        m_ui.actionFullscreen->setVisible( false );
    }
#endif
}

void
MainWindow::createGlobalPreferences()
{
    m_globalPreferences = new SettingsDialog( Core::instance()->settings(), tr( "VLMC Preferences" ), this );
    m_globalPreferences->addCategory( "vlmc", QT_TRANSLATE_NOOP( "Settings", "General" ), QIcon( ":/images/vlmc" ) );
    m_globalPreferences->addCategory( "keyboard", QT_TRANSLATE_NOOP( "Settings", "Keyboard" ), QIcon( ":/images/keyboard" ) );
    m_globalPreferences->addCategory( "youtube", QT_TRANSLATE_NOOP( "Settings", "YouTube" ), QIcon( ":/images/youtube" ) );
    m_globalPreferences->addCategory( "network", QT_TRANSLATE_NOOP( "Settings", "Network" ), QIcon( ":/images/network" ) );
}

void
MainWindow::loadGlobalProxySettings()
{
    if( VLMC_GET_BOOL( "network/UseProxy" ) )
    {
        /* Updates Global Proxy for VLMC */
        QNetworkProxy proxy;
        proxy.setType( QNetworkProxy::HttpProxy );
        proxy.setHostName( VLMC_GET_STRING( "network/ProxyHost" ) );
        proxy.setPort( VLMC_GET_STRING( "network/ProxyPort" ).toInt() );
        proxy.setUser( VLMC_GET_STRING( "network/ProxyUsername" ) );
        proxy.setPassword( VLMC_GET_STRING( "network/ProxyPassword" ) );
        QNetworkProxy::setApplicationProxy( proxy );
        return;
    }

    QNetworkProxy::setApplicationProxy( QNetworkProxy::NoProxy );
}

void
MainWindow::createProjectPreferences()
{
    m_projectPreferences = new SettingsDialog( Core::instance()->project()->settings(), tr( "Project preferences" ), this );
    m_projectPreferences->addCategory( "general", QT_TRANSLATE_NOOP( "Settings", "General" ), QIcon( ":/images/vlmc" ) );
    m_projectPreferences->addCategory( "video", QT_TRANSLATE_NOOP( "Settings", "Video" ), QIcon( ":/images/video" ) );
    m_projectPreferences->addCategory( "audio", QT_TRANSLATE_NOOP( "Settings", "Audio" ), QIcon( ":/images/audio" ) );
}

void
MainWindow::checkFolders()
{
    QDir dirUtil;
    if ( !dirUtil.exists( VLMC_GET_STRING( "vlmc/TempFolderLocation" ) ) )
        dirUtil.mkdir( VLMC_GET_STRING( "vlmc/TempFolderLocation" ) );
}

void
MainWindow::clearTemporaryFiles()
{
    QDir dirUtil;
    if( dirUtil.cd( VLMC_GET_STRING( "vlmc/TempFolderLocation" ) ) )
        for ( const QString& file: dirUtil.entryList( QDir::Files ) )
            dirUtil.remove( file );
}

//Private slots definition

void
MainWindow::on_actionQuit_triggered()
{
    saveSettings();
    close();
}

void
MainWindow::on_actionPreferences_triggered()
{
   m_globalPreferences->show();
}

void
MainWindow::on_actionAbout_triggered()
{
    About about( this );
    about.exec();
}

bool
MainWindow::checkVideoLength()
{
    if ( Core::instance()->workflow()->canRender() == false )
    {
        QMessageBox::warning( nullptr, tr ( "VLMC Renderer" ), tr( "There is nothing to render." ) );
        return false;
    }
    return true;
}

bool
MainWindow::renderVideoSettings( bool shareOnInternet )
{
    RendererSettings settings( shareOnInternet );

    if ( settings.exec() == QDialog::Rejected )
        return false;

    QString     outputFileName = settings.outputFileName();
    quint32     width          = settings.width();
    quint32     height         = settings.height();
    double      fps            = settings.fps();
    quint32     vbitrate       = settings.videoBitrate();
    quint32     abitrate       = settings.audioBitrate();
    auto        ar             = settings.aspectRatio();
    auto        nbChannels     = settings.nbChannels();
    auto        sampleRate     = settings.sampleRate();


    return  Core::instance()->workflow()->startRenderToFile( outputFileName, width, height,
                                                             fps, ar, vbitrate, abitrate,
                                                             nbChannels, sampleRate );
}

QDockWidget*
MainWindow::dockWidget( QWidget* widget, Qt::DockWidgetArea startArea )
{
    QDockWidget*    dock = new QDockWidget( this );

    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    widget->setParent( dock );
    dock->setWidget( widget );
    dock->setObjectName( widget->objectName() );
    if ( VLMC_GET_BOOL( "private/LayoutLock" ) == true )
        dock->setTitleBarWidget( new QWidget );
    else
        dock->setTitleBarWidget( 0 );
    addDockWidget( startArea, dock );
    registerWidgetInWindowMenu( dock );
    return dock;
}

void
MainWindow::on_actionRender_triggered()
{
    if ( checkVideoLength() )
    {
        //Setup dialog box for querying render parameters.
        renderVideoSettings( false );
    }
}

void
MainWindow::on_actionShare_On_Internet_triggered()
{
    if ( checkVideoLength() )
    {
        if( !renderVideoSettings( true ) )
            return;

        checkFolders();
        QString fileName = VLMC_GET_STRING( "vlmc/TempFolderLocation" ) + "/" +
                           Core::instance()->project()->name() +
                           "-vlmc.mp4";

        loadGlobalProxySettings();

        ShareOnInternet *shareVideo = new ShareOnInternet();
        shareVideo->setVideoFile( fileName );

        if ( shareVideo->exec() == QDialog::Rejected )
        {
            delete shareVideo;
            return;
        }

        delete shareVideo;
    }
}

void
MainWindow::on_actionNew_Project_triggered()
{
    m_wizard->restart();
    m_wizard->show();
}

void
MainWindow::on_actionHelp_triggered()
{
    QDesktopServices::openUrl( QUrl( "http://videolan.org/vlmc" ) );
}

void
MainWindow::zoomIn()
{
    m_zoomSlider->setValue( m_zoomSlider->value() - 1 );
}

void
MainWindow::zoomOut()
{
    m_zoomSlider->setValue( m_zoomSlider->value() + 1 );
}

void
MainWindow::setScale( quint32 scale )
{
    m_zoomSlider->setValue( scale );
}

void
MainWindow::on_actionFullscreen_triggered( bool checked )
{
    if ( checked && !isFullScreen() )
    {
        setUnifiedTitleAndToolBarOnMac( false );
        showFullScreen();
    }
    else
    {
        setUnifiedTitleAndToolBarOnMac( true );
        showNormal();
    }
}

void
MainWindow::registerWidgetInWindowMenu( QDockWidget* widget )
{
    m_ui.menuView->insertAction( m_ui.widgetsSeparator, widget->toggleViewAction() );
}

void
MainWindow::toolButtonClicked( QAction *action )
{
    if ( action == m_ui.actionSelection_mode )
        emit selectionToolSelected();
    else if ( action == m_ui.actionCut_mode )
        emit cutToolSelected();
    else
        vlmcCritical() << "Unknown tool. This should not happen !";
}

void
MainWindow::on_actionProject_Preferences_triggered()
{
  m_projectPreferences->show();
}

bool
MainWindow::saveSettings()
{
    // ??????
    clearTemporaryFiles();
    Settings* settings = Core::instance()->settings();
    // Save the current geometry
    settings->setValue( "private/MainWindowGeometry", saveGeometry() );
    // Save the current layout
    settings->setValue( "private/MainWindowState", saveState() );
    settings->setValue( "private/CleanQuit", true );
    return true;
}

void
MainWindow::closeEvent( QCloseEvent* e )
{
    if ( Core::instance()->project()->isClean() == false )
    {
        QMessageBox msgBox;
        msgBox.setText( QObject::tr( "The project has been modified." ) );
        msgBox.setInformativeText( QObject::tr( "Do you want to save it?" ) );
        msgBox.setStandardButtons( QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );
        msgBox.setDefaultButton( QMessageBox::Save );
        int     ret = msgBox.exec();
        switch ( ret )
        {
        case QMessageBox::Save:
            on_actionSave_triggered();
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
            e->ignore();
            return;
        }
    }
    saveSettings();
    e->accept();
}

void
MainWindow::updateRecentProjects()
{
    auto menu = new QMenu;
    for ( const auto& var : Core::instance()->recentProjects()->toVariant().toList() )
    {
        const auto& m = var.toMap();
        const auto& name = m["name"].toString();
        const auto& file = m["file"].toString();
        auto action = menu->addAction( QString( "%1 - %2" )
                                       .arg( name )
                                       .arg( file )
                                       );
        connect( action, &QAction::triggered, this, [this, file]()
        {
            Core::instance()->project()->load( file );
        } );
    }
    m_ui.actionRecent_Projects->setMenu( menu );
}

void
MainWindow::projectNameChanged( const QString& projectName )
{
    QString title = tr( "%1 VideoLAN Movie Creator [*]" ).arg( projectName );
    setWindowTitle( title );
}

void
MainWindow::cleanStateChanged( bool isClean )
{
    setWindowModified( isClean == false );
}

void
MainWindow::onProjectSaved()
{
    setWindowModified( false );
}

void
MainWindow::on_actionUndo_triggered()
{
    Core::instance()->workflow()->undoStack()->undo();
}

void
MainWindow::on_actionRedo_triggered()
{
    Core::instance()->workflow()->undoStack()->redo();
}

void
MainWindow::on_actionCrash_triggered()
{
    //WARNING: read this part at your own risk !!
    //I'm not responsible if you puke while reading this :D
    QString str;
    int test = 1 / str.length();
    Q_UNUSED( test );
}

bool
MainWindow::restoreSession()
{
    bool    cleanQuit = VLMC_GET_BOOL( "private/CleanQuit" );
    bool    ret = false;

    if ( cleanQuit == false )
    {
        QMessageBox::StandardButton res = QMessageBox::question( this, tr( "Crash recovery" ), tr( "VLMC didn't close nicely. Do you want to recover your project?" ),
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
        if ( res == QMessageBox::Yes )
        {
            auto path = Core::instance()->recentProjects()->mostRecentProjectFile();
            if ( path.isEmpty() == true )
            {
                QMessageBox::warning( this, tr( "Can't restore project" ), tr( "VLMC didn't manage to restore your project. We apology for the inconvenience" ) );
                ret = false;
            }
            else
            {
                Core::instance()->project()->load( path );
                ret = true;
            }
        }
    }
    Core::instance()->settings()->setValue( "private/CleanQuit", ret );
    return ret;
}

void
MainWindow::canUndoChanged( bool canUndo )
{
    m_ui.actionUndo->setEnabled( canUndo );
}

void
MainWindow::canRedoChanged( bool canRedo )
{
    m_ui.actionRedo->setEnabled( canRedo );
}

void
MainWindow::onOudatedBackupFile()
{
    if ( QMessageBox::question( nullptr, QObject::tr( "Backup file" ),
                                      QObject::tr( "An outdated backup file was found. "
                                     "Do you want to erase it?" ),
                                    QMessageBox::Ok | QMessageBox::No ) == QMessageBox::Ok )
    {
        Core::instance()->project()->removeBackupFile();
    }
}

void
MainWindow::onBackupFileLoaded()
{
    //FIXME: Adjust the behavior depending on how we react to a crash
}

#ifdef WITH_CRASHBUTTON
void
MainWindow::setupCrashTester()
{
    QAction* actionCrash = new QAction( this );
    actionCrash->setObjectName( QString::fromUtf8( "actionCrash" ) );
    m_ui.menuTools->addAction( actionCrash );
    actionCrash->setText( QApplication::translate( "MainWindow", "Crash", 0, QApplication::UnicodeUTF8 ) );
    connect( actionCrash, SIGNAL( triggered( bool ) ), this, SLOT( on_actionCrash_triggered() ) );
}
#endif

