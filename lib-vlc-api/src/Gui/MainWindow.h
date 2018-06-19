/*****************************************************************************
 * MainWindow.h: VLMC MainWindow
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

class QUndoView;
class QProgressBar;

#include <QApplication>
#include <QSlider>
#include <QToolButton>
#include <QButtonGroup>
#include <QDockWidget>

#include "vlmc.h"
#include "config.h"
#include "ui/MainWindow.h"

class   EffectsListView;
class   TransitionsListView;
class   ClipLibraryView;
class   MediaLibraryView;
class   PreviewWidget;
class   Project;
class   ProjectWizard;
class   SettingsDialog;
class   Timeline;
class   WorkflowRenderer;

namespace Backend
{
class IBackend;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY( MainWindow )

public:
    explicit MainWindow( Backend::IBackend* backend,
                         QWidget *parent = 0 );
    ~MainWindow();

    void registerWidgetInWindowMenu( QDockWidget* widget );
    void        showWizard();

public slots:
    void        zoomIn();
    void        zoomOut();
    void        setScale( quint32 scale );

protected:
    virtual void    changeEvent( QEvent *e );
    virtual void    closeEvent( QCloseEvent* e );

private:
    void        initializeDockWidgets();
    void        checkFolders();
    void        createStatusBar();
    void        createGlobalPreferences();
    void        createProjectPreferences();
    void        clearTemporaryFiles();
    void        initVlmcPreferences();
    void        loadGlobalProxySettings();
    void        initToolbar();
    bool        saveSettings();
    void        setupLibrary();
    void        setupClipPreview();
    void        setupProjectPreview();
    void        setupEffectsList();
    void        setupTransitionsList();
    void        setupUndoRedoWidget();
    void        retranslateUi();
#ifdef WITH_CRASHBUTTON
    void        setupCrashTester();
#endif
    /**
     *  \brief  Will check if vlmc closed nicely or crashed.
     *          If so, the emergency backup will be opened.
     *  \return true if a project was restored. If so, the wizard should
     *          not be opened.
     */
    bool        restoreSession();

    /**
     *  \brief  Will check if there is any video on the Project Timeline.
     *  \return true, if ( Core::instance()->workflow()->getLengthFrame() > 0 )
     *          else false
     */
    bool        checkVideoLength();

    /**
     *  \brief  Gets video parameters from RendererSettings Dialog
     *          exportType when set to true, renders video to user defined location
     *          and when set to false, renders video to temporary folder.
     *  \return same as renderVideo(),
     *          true if video renders well or not cancelled by the user.
     */
    bool        renderVideoSettings( bool exportType );

    QDockWidget* dockWidget( QWidget* widget, Qt::DockWidgetArea startArea );

    Ui::MainWindow          m_ui;
    QSlider*                m_zoomSlider;
    Timeline*               m_timeline;
    Backend::IBackend*      m_backend;
    PreviewWidget*          m_clipPreview;
    PreviewWidget*          m_projectPreview;
    SettingsDialog*         m_globalPreferences;
    SettingsDialog*         m_DefaultProjectPreferences;
    SettingsDialog*         m_projectPreferences;
    ProjectWizard*          m_wizard;
    MediaLibraryView*       m_mediaLibrary;
    ClipLibraryView*        m_clipLibrary;
    EffectsListView*        m_effectsList;
    TransitionsListView*    m_transitionsList;
    QUndoView*              m_undoView;
    QDockWidget*            m_dockedUndoView;
    QDockWidget*            m_dockedEffectsList;
    QDockWidget*            m_dockedTransitionsList;
    QDockWidget*            m_dockedClipLibrary;
    QDockWidget*            m_dockedMediaLibrary;
    QDockWidget*            m_dockedClipPreview;
    QDockWidget*            m_dockedProjectPreview;
    QProgressBar*           m_progressBar;

private slots:
    void                    on_actionFullscreen_triggered( bool checked );
    void                    on_actionQuit_triggered();
    void                    on_actionAbout_triggered();
    void                    on_actionPreferences_triggered();
    void                    on_actionRender_triggered();
    void                    on_actionShare_On_Internet_triggered();
    void                    on_actionNew_Project_triggered();
    void                    on_actionLoad_Project_triggered();
    void                    on_actionSave_triggered();
    void                    on_actionSave_As_triggered();
    void                    on_actionHelp_triggered();
    void                    on_actionProject_Preferences_triggered();
    void                    on_actionUndo_triggered();
    void                    on_actionRedo_triggered();
    void                    on_actionCrash_triggered();
    void                    toolButtonClicked( QAction *action );
    void                    updateRecentProjects();
    void                    projectNameChanged(const QString& projectName);
    void                    cleanStateChanged( bool isClean );
    void                    canUndoChanged( bool canUndo );
    void                    canRedoChanged( bool canRedo );
    void                    onOudatedBackupFile();
    void                    onBackupFileLoaded();
    void                    onProjectSaved();

signals:
    void                    selectionToolSelected();
    void                    cutToolSelected();
    void                    scaleChanged( quint32 scaleLevel );
};

#endif // MAINWINDOW_H
