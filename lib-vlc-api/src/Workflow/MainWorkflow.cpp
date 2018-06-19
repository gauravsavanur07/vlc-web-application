/*****************************************************************************
 * MainWorkflow.cpp : Will query all of the track workflows to render the final
 *                    image
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Yikei Lu    <luyikei.qmltu@gmail.com>
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

#include "vlmc.h"
#include "Commands/Commands.h"
#include "Commands/AbstractUndoStack.h"
#include "Backend/MLT/MLTOutput.h"
#include "Backend/MLT/MLTMultiTrack.h"
#include "Backend/MLT/MLTTrack.h"
#include "Renderer/AbstractRenderer.h"
#include "EffectsEngine/EffectHelper.h"
#ifdef HAVE_GUI
#include "Gui/effectsengine/EffectStack.h"
#include "Gui/WorkflowFileRendererDialog.h"
#endif
#include "Project/Project.h"
#include "Media/Clip.h"
#include "Media/Media.h"
#include "Library/Library.h"
#include "MainWorkflow.h"
#include "Project/Project.h"
#include "SequenceWorkflow.h"
#include "Settings/Settings.h"
#include "Tools/VlmcDebug.h"
#include "Tools/RendererEventWatcher.h"
#include "Tools/OutputEventWatcher.h"
#include "Transition/Transition.h"
#include "Workflow/Types.h"

#include <QJsonArray>
#include <QMutex>

MainWorkflow::MainWorkflow( Settings* projectSettings, int trackCount ) :
        m_trackCount( trackCount ),
        m_settings( new Settings ),
        m_renderer( new AbstractRenderer ),
        m_undoStack( new Commands::AbstractUndoStack ),
        m_sequenceWorkflow( new SequenceWorkflow( trackCount ) )
{
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipAdded, this, &MainWorkflow::clipAdded );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipRemoved, this, &MainWorkflow::clipRemoved );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipLinked, this, &MainWorkflow::clipLinked );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipUnlinked, this, &MainWorkflow::clipUnlinked );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipMoved, this, &MainWorkflow::clipMoved );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::clipResized, this, &MainWorkflow::clipResized );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::transitionAdded, this, &MainWorkflow::transitionAdded );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::transitionMoved, this, &MainWorkflow::transitionMoved );
    connect( m_sequenceWorkflow.get(), &SequenceWorkflow::transitionRemoved, this, &MainWorkflow::transitionRemoved );
    m_renderer->setInput( m_sequenceWorkflow->input() );

    connect( m_renderer->eventWatcher().data(), &RendererEventWatcher::lengthChanged, this, &MainWorkflow::lengthChanged );
    connect( m_renderer->eventWatcher().data(), &RendererEventWatcher::endReached, this, &MainWorkflow::mainWorkflowEndReached );
    connect( m_renderer->eventWatcher().data(), &RendererEventWatcher::positionChanged, this, [this]( qint64 pos )
    {
        emit frameChanged( pos, m_sequenceWorkflow->input()->playableLength(), Vlmc::Renderer );
    }, Qt::DirectConnection );

    m_settings->createVar( SettingValue::List, "tracks", QVariantList(), "", "", SettingValue::Nothing );
    connect( m_settings, &Settings::postLoad, this, &MainWorkflow::postLoad, Qt::DirectConnection );
    connect( m_settings, &Settings::preSave, this, &MainWorkflow::preSave, Qt::DirectConnection );
    projectSettings->addSettings( QStringLiteral( "Workspace" ), *m_settings );

    connect( m_undoStack.get(), &Commands::AbstractUndoStack::cleanChanged, this, &MainWorkflow::cleanChanged );
}

MainWorkflow::~MainWorkflow()
{
    m_renderer->stop();
    delete m_renderer;
    delete m_settings;
}

void
MainWorkflow::unmuteTrack( unsigned int trackId, Workflow::TrackType trackType )
{
    // TODO
}

void
MainWorkflow::muteClip( const QUuid& uuid, unsigned int trackId )
{
    // TODO
}

void
MainWorkflow::unmuteClip( const QUuid& uuid, unsigned int trackId )
{
    // TODO
}

void
MainWorkflow::trigger( Commands::Generic* command )
{
    m_undoStack->push( command );
}

void
MainWorkflow::clear()
{
    m_sequenceWorkflow->clear();
    emit cleared();
}

void
MainWorkflow::setClean()
{
    m_undoStack->setClean();
}

void
MainWorkflow::setPosition( qint64 newFrame )
{
    m_renderer->setPosition( newFrame );
}

void
MainWorkflow::setFps( double fps )
{
    Backend::instance()->profile().setFrameRate( fps * 100, 100 );
    emit fpsChanged( fps );
}

void
MainWorkflow::showEffectStack()
{
#ifdef HAVE_GUI
    auto w = new EffectStack( m_sequenceWorkflow->input() );
    w->show();
#endif
}

void
MainWorkflow::showEffectStack( quint32 trackId )
{
#ifdef HAVE_GUI
    auto w = new EffectStack( m_sequenceWorkflow->trackInput( trackId ) );
    w->show();
#endif
}

void
MainWorkflow::showEffectStack( const QString& uuid )
{
#ifdef HAVE_GUI
    auto w = new EffectStack( m_sequenceWorkflow->clip( uuid )->clip->input() );
    connect( w, &EffectStack::finished, Core::instance()->workflow(), [uuid]{ emit Core::instance()->workflow()->effectsUpdated( uuid ); } );
    w->show();
#endif
}

AbstractRenderer*
MainWorkflow::renderer()
{
    return m_renderer;
}

Commands::AbstractUndoStack*
MainWorkflow::undoStack()
{
    return m_undoStack.get();
}

int
MainWorkflow::getTrackCount() const
{
    return m_trackCount;
}

quint32
MainWorkflow::trackCount() const
{
    return m_trackCount;
}

void
MainWorkflow::addClip( const QString& uuid, quint32 trackId, qint32 pos )
{
    vlmcDebug() << "Adding clip:" << uuid;
    auto command = new Commands::Clip::Add( m_sequenceWorkflow, uuid, trackId, pos );
    trigger( command );
}

QJsonObject
MainWorkflow::clipInfo( const QString& uuid )
{
    auto c = m_sequenceWorkflow->clip( uuid );
    if ( c != nullptr )
    {
        auto clip = c->clip;
        auto h = clip->toVariant().toHash();
        h["uuid"] = uuid;
        h["length"] = clip->length();
        h["name"] = clip->media()->title();
        h["audio"] = c->isAudio;

        QStringList linkedClipList;
        for ( const auto& linkedClipUuid : c->linkedClips )
            linkedClipList << linkedClipUuid.toString();
        h["linkedClips"] = QJsonArray::fromStringList( linkedClipList );

        h["position"] = m_sequenceWorkflow->position( uuid );
        h["trackId"] = m_sequenceWorkflow->trackId( uuid );
        h["filters"] = EffectHelper::toVariant( clip->input() );
        return QJsonObject::fromVariantHash( h );
    }
    return QJsonObject();
}

QJsonObject
MainWorkflow::libraryClipInfo( const QString& uuid )
{
    auto c = Core::instance()->library()->clip( uuid );
    if ( c == nullptr )
        return {};
    auto h = c->toVariant().toHash();
    h["length"] = (qint64)( c->input()->length() );
    h["name"] = c->media()->title();
    h["audio"] = c->media()->hasAudioTracks();
    h["video"] = c->media()->hasVideoTracks();
    h["begin"] = c->begin();
    h["end"] = c->end();
    h["uuid"] = c->uuid().toString();
    return QJsonObject::fromVariantHash( h );
}

QJsonObject
MainWorkflow::transitionInfo( const QString& uuid )
{
    auto t = m_sequenceWorkflow->transition( uuid );
    if ( !t )
        return {};
    return QJsonObject::fromVariantHash( t->toVariant().toHash() );
}

void
MainWorkflow::moveClip( const QString& uuid, quint32 trackId, qint64 startFrame )
{
    trigger( new Commands::Clip::Move( m_sequenceWorkflow, uuid, trackId, startFrame ) );
}

void
MainWorkflow::resizeClip( const QString& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos )
{
    trigger( new Commands::Clip::Resize( m_sequenceWorkflow, uuid, newBegin, newEnd, newPos ) );
}

void
MainWorkflow::removeClip( const QString& uuid )
{
    trigger( new Commands::Clip::Remove( m_sequenceWorkflow, uuid ) );
}

void
MainWorkflow::splitClip( const QUuid& uuid, qint64 newClipPos, qint64 newClipBegin )
{
    trigger( new Commands::Clip::Split( m_sequenceWorkflow, uuid, newClipPos, newClipBegin ) );
}

void
MainWorkflow::linkClips( const QString& uuidA, const QString& uuidB )
{
    trigger( new Commands::Clip::Link( m_sequenceWorkflow, uuidA, uuidB ) );
}

void
MainWorkflow::unlinkClips( const QString& uuidA, const QString& uuidB )
{
    trigger( new Commands::Clip::Unlink( m_sequenceWorkflow, uuidA, uuidB ) );
}

QString
MainWorkflow::addEffect( const QString &clipUuid, const QString &effectId )
{
    std::shared_ptr<EffectHelper> newEffect;

    try
    {
        newEffect.reset( new EffectHelper( effectId ) );
    }
    catch( Backend::InvalidServiceException& e )
    {
        return QStringLiteral( "" );
    }

    auto clip = m_sequenceWorkflow->clip( clipUuid );
    if ( clip && clip->clip->input() )
    {
        trigger( new Commands::Effect::Add( newEffect, clip->clip->input() ) );
        emit effectsUpdated( clipUuid );
        return newEffect->uuid().toString();
    }

    return QStringLiteral( "" );
}

QString
MainWorkflow::addTransition( const QString& identifier, qint64 begin, qint64 end, quint32 trackId, const QString& type )
{
    auto trackType = type == QStringLiteral( "Video" ) ? Workflow::VideoTrack : Workflow::AudioTrack;
    auto command = new Commands::Transition::Add( m_sequenceWorkflow, identifier, begin, end, trackId, trackType );
    trigger( command );
    return command->uuid().toString();
}

QString
MainWorkflow::addTransitionBetweenTracks( const QString& identifier, qint64 begin, qint64 end, quint32 trackAId, quint32 trackBId,
                                          const QString& type )
{
    auto trackType = type == QStringLiteral( "Video" ) ? Workflow::VideoTrack : Workflow::AudioTrack;
    auto command = new Commands::Transition::Add( m_sequenceWorkflow, identifier, begin, end, trackAId, trackBId, trackType );
    trigger( command );
    return command->uuid().toString();
}

void
MainWorkflow::moveTransition( const QUuid& uuid, qint64 begin, qint64 end )
{
    trigger( new Commands::Transition::Move( m_sequenceWorkflow, uuid, begin, end ) );
}

void
MainWorkflow::moveTransitionBetweenTracks( const QUuid& uuid, quint32 trackAId, quint32 trackBId )
{
    trigger( new Commands::Transition::MoveBetweenTracks( m_sequenceWorkflow, uuid, trackAId, trackBId ) );
}

void
MainWorkflow::removeTransition( const QUuid& uuid )
{
    trigger( new Commands::Transition::Remove( m_sequenceWorkflow, uuid ) );
}

bool
MainWorkflow::startRenderToFile( const QString &outputFileName, quint32 width, quint32 height,
                                 double fps, const QString &ar, quint32 vbitrate, quint32 abitrate,
                                 quint32 nbChannels, quint32 sampleRate )
{
    m_renderer->stop();

    if ( canRender() == false )
        return false;

    Backend::MLT::MLTFFmpegOutput output;
    auto input = m_sequenceWorkflow->input();
    OutputEventWatcher            cEventWatcher;
    output.setCallback( &cEventWatcher );
    output.setTarget( qPrintable( outputFileName ) );
    output.setWidth( width );
    output.setHeight( height );
    output.setFrameRate( fps * 100, 100 );
    auto temp = ar.split( "/" );
    output.setAspectRatio( temp[0].toInt(), temp[1].toInt() );
    output.setVideoBitrate( vbitrate );
    output.setAudioBitrate( abitrate );
    output.setChannels( nbChannels );
    output.setAudioSampleRate( sampleRate );
    output.connect( *input );

#ifdef HAVE_GUI
    WorkflowFileRendererDialog  dialog( width, height );
    dialog.setModal( true );
    dialog.setOutputFileName( outputFileName );
    connect( this, &MainWorkflow::frameChanged, &dialog, &WorkflowFileRendererDialog::frameChanged );
    connect( &dialog, &WorkflowFileRendererDialog::stop, this, [&output]{ output.stop(); } );
    connect( m_renderer->eventWatcher().data(), &RendererEventWatcher::positionChanged, &dialog,
             [this, input, &dialog, width, height]( qint64 pos )
    {
        // Update the preview per five seconds
        if ( pos % qRound( input->fps() * 5 ) == 0 )
        {
            dialog.updatePreview( input->image( width, height ) );
        }
    });
    connect( &cEventWatcher, &OutputEventWatcher::stopped, &dialog, &WorkflowFileRendererDialog::accept );
#endif

    input->setPosition( 0 );
    output.start();

#ifdef HAVE_GUI
    if ( dialog.exec() == QDialog::Rejected )
        return false;
#else
    while ( output.isStopped() == false )
        SleepS( 1 );
#endif
    return true;
}

bool
MainWorkflow::canRender()
{
    return m_sequenceWorkflow->input()->playableLength() > 0;
}

void
MainWorkflow::preSave()
{
    m_settings->value( "tracks" )->set( m_sequenceWorkflow->toVariant() );
}

void
MainWorkflow::postLoad()
{
    m_sequenceWorkflow->loadFromVariant( m_settings->value( "tracks" )->get() );
}
