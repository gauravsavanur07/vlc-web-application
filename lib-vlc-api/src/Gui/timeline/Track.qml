import QtQuick 2.0

Item {
    id: track
    width: parent.width
    height: trackHeight
    z: 10

    property int trackId
    property string type
    property ListModel clips
    property ListModel transitionModel

    Item {
        id: clipArea
        x: trackInfo.width
        height: parent.height
        width: track.width - initPosOfCursor

        Rectangle {
            color: "#666666"
            height: 1
            width: parent.width
            anchors.bottom: clipArea.bottom

            Component.onCompleted: {
                if ( track.type === "Video" && track.trackId == 0 )
                {
                    color = "#111111";
                }
            }
        }

        DropArea {
            id: dropArea
            anchors.fill: parent
            keys: ["Clip", "Transition", "vlmc/uuid", "vlmc/transition_id"]

            // Enum for drop mode
            readonly property var dropMode: {
                "New": 0,
                "Move": 1,
                "TransitionNew": 2,
                "TransitionMove": 3,
            }

            property string currentUuid
            property var aClipInfo: null
            property var vClipInfo: null
            property var mode

            property int lastPos: 0
            property int deltaPos: 0

            onDropped: {
                if ( mode === dropMode.New ) {
                    aClipInfo = findClipFromTrack( "Audio", trackId, "audioUuid" );
                    vClipInfo = findClipFromTrack( "Video", trackId, "videoUuid" );
                    var pos = 0;
                    if ( aClipInfo ) {
                        pos = aClipInfo["position"];
                        removeClipFromTrack( "Audio", trackId, "audioUuid" );
                    }
                    if ( vClipInfo ) {
                        pos = vClipInfo["position"];
                        removeClipFromTrack( "Video", trackId, "videoUuid" );
                    }
                    workflow.addClip( drop.getDataAsString("vlmc/uuid"), trackId, pos, false );
                    currentUuid = "";
                    aClipInfo = null;
                    vClipInfo = null;
                    clearSelectedClips();
                    adjustTracks( "Audio" );
                    adjustTracks( "Video" );
                }
                else if ( mode === dropMode.TransitionNew ) {
                    var transition = findTransitionItem( "transitionUuid" );
                    workflow.addTransitionBetweenTracks(
                        transition.identifier, transition.begin, transition.end,
                        transition.trackId - 1, transition.trackId, transition.type );
                    removeTransitionFromTrack( type, trackId, "transitionUuid" );
                    currentUuid = "";

                }
            }

            onExited: {
                if ( currentUuid === "transitionUuid" ) {
                    removeTransitionFromTrack( type, trackId, "transitionUuid" );
                }
                else if ( currentUuid !== "" ) {
                    removeClipFromTrack( "Audio", trackId, "audioUuid" );
                    removeClipFromTrack( "Video", trackId, "videoUuid" );
                }
            }

            onEntered: {
                if ( drag.keys.indexOf( "vlmc/uuid" ) >= 0 )
                    mode = dropMode.New;
                else if ( drag.keys.indexOf( "vlmc/transition_id" ) >= 0 )
                    mode = dropMode.TransitionNew
                else if ( drag.keys.indexOf( "Clip" ) >= 0 )
                    mode = dropMode.Move;
                else if ( drag.keys.indexOf( "Transition" ) >= 0 )
                    mode = dropMode.TransitionMove;

                if ( mode === dropMode.New ) {
                    clearSelectedClips();
                    if ( currentUuid === drag.getDataAsString( "vlmc/uuid" ) ) {
                        if ( aClipInfo )
                        {
                            aClipInfo["position"] = ptof( drag.x );
                            aClipInfo = addClip( "Audio", trackId, aClipInfo );
                        }
                        if ( vClipInfo )
                        {
                            vClipInfo["position"] = ptof( drag.x );
                            vClipInfo = addClip( "Video", trackId, vClipInfo );
                        }
                    }
                    else {
                        var newClipInfo = workflow.libraryClipInfo( drag.getDataAsString( "vlmc/uuid" ) );
                        currentUuid = "" + newClipInfo["uuid"];
                        newClipInfo["position"] = ptof( drag.x );
                        if ( newClipInfo["audio"] ) {
                            newClipInfo["uuid"] = "audioUuid";
                            aClipInfo = addClip( "Audio", trackId, newClipInfo );
                        }
                        if ( newClipInfo["video"] ) {
                            newClipInfo["uuid"] = "videoUuid";
                            vClipInfo = addClip( "Video", trackId, newClipInfo );
                        }
                    }
                    lastPos = ptof( drag.x );
                }
                else if ( mode === dropMode.Move ) {
                    lastPos = ptof( drag.source.x );
                    // HACK: Call onPositoinChanged forcely here.
                    // x will be rounded so it won't affect actual its position.
                    drag.source.x = drag.source.x + 0.000001;
                    drag.source.forcePosition(); // Restore the binding
                }
                else if ( mode === dropMode.TransitionNew ) {
                    if ( trackId > 0 ) {
                        var transition_id = drag.getDataAsString( "vlmc/transition_id" );
                        currentUuid = "transitionUuid";
                        addTransition( type, trackId, {
                                          "identifier": transition_id,
                                          "begin": ptof( drag.x ),
                                          "end": ptof( drag.x + 100 ),
                                          "uuid": currentUuid,
                                          "identifier": transition_id }
                                      );
                    }
                    lastPos = ptof( drag.x );
                }
                else if ( mode === dropMode.TransitionMove ) {
                    drag.source.trackId = trackId;
                    lastPos = ptof( drag.source.x );
                }
            }

            onPositionChanged: {
                // If resizing, ignore
                if ( drag.source.resizing === true )
                    return;

                if ( mode === dropMode.Move ) {
                    drag.source.scrollToThis();

                    // Move to the top
                    drag.source.z = ++maxZ;

                    // Prepare newTrackId for all the selected clips
                    var oldTrackId = drag.source.newTrackId;
                    drag.source.newTrackId = trackId;

                    deltaPos = ptof( drag.source.x ) - lastPos;

                    sortSelectedClips( trackId - oldTrackId, deltaPos );
                    var toMove = selectedClips.concat();

                    // Check if there is any impossible move
                    for ( var i = 0; i < toMove.length; ++i ) {
                        var target = findClipItem( toMove[i] );
                        if ( target !== drag.source ) {
                            var newTrackId = trackId - oldTrackId + target.trackId;
                            if ( newTrackId < 0 )
                            {
                                drag.source.newTrackId = oldTrackId;
                                drag.source.forcePosition();

                                // Direction depends on its type
                                drag.source.y +=
                                        drag.source.type === "Video"
                                        ? -( trackHeight * ( oldTrackId - trackId ) )
                                        : trackHeight * ( oldTrackId - trackId )
                                return;
                            }
                        }
                    }

                    for ( i = 0; i < toMove.length; ++i ) {
                        target = findClipItem( toMove[i] );
                        if ( target !== drag.source ) {
                             newTrackId = trackId - oldTrackId + target.trackId;
                            target.newTrackId = Math.max( 0, newTrackId );
                            if ( target.newTrackId !== target.trackId ) {
                                // Let's move to the new tracks
                                target.clipInfo["selected"] = true;
                                addClip( target.type, target.newTrackId, target.clipInfo );
                                removeClipFromTrack( target.type, target.trackId, target.uuid );
                            }
                        }
                    }
                }
                else if ( mode === dropMode.New ){
                    toMove = selectedClips.concat();
                    deltaPos = ptof( drag.x ) - lastPos;
                }
                else if ( mode === dropMode.TransitionNew )
                {
                    deltaPos = ptof( drag.x ) - lastPos;
                    var tItem = findTransitionItem( "transitionUuid" );
                    tItem.begin += deltaPos;
                    tItem.end += deltaPos;
                }
                else if ( mode === dropMode.TransitionMove ) {
                    deltaPos = ptof( drag.source.x ) - lastPos;
                    drag.source.begin += deltaPos;
                    drag.source.end += deltaPos;
                }

                if ( mode === dropMode.New || mode === dropMode.Move )
                {
                    while ( toMove.length > 0 ) {
                        target = findClipItem( toMove[0] );
                        var oldPos = target.position;
                        var newPos = findNewPosition( Math.max( oldPos + deltaPos, 0 ), target, drag.source, isMagneticMode );
                        deltaPos = newPos - oldPos;

                        // Let's find newX of the linked clip
                        for ( i = 0; i < target.linkedClips.length; ++i )
                        {
                            var linkedClipItem = findClipItem( target.linkedClips[i] );
                            if ( target === drag.source )
                                linkedClipItem.z = ++maxZ;

                            if ( linkedClipItem ) {
                                var newLinkedClipPos = findNewPosition( newPos, linkedClipItem, drag.source, isMagneticMode );

                                // If linked clip collides
                                if ( newLinkedClipPos !== newPos ) {
                                    // Recalculate target's newX
                                    // This time, don't use magnets
                                    if ( isMagneticMode === true )
                                    {
                                        newLinkedClipPos = findNewPosition( newPos, linkedClipItem, drag.source, false );
                                        newPos = findNewPosition( newPos, target, drag.source, false );

                                        // And if newX collides again, we don't move
                                        if ( newLinkedClipPos !== newPos )
                                            deltaPos = 0
                                        else
                                            linkedClipItem.position = target.position; // Link if possible
                                    }
                                    else
                                        deltaPos = 0;
                                }
                                else
                                    linkedClipItem.position = target.position; // Link if possible

                                var ind = toMove.indexOf( linkedClipItem.uuid );
                                if ( ind > 0 )
                                    toMove.splice( ind, 1 );
                            }
                        }

                        newPos = oldPos + deltaPos;
                        toMove.splice( 0, 1 );
                    }
                    // END of while ( toMove.length > 0 )

                    if ( deltaPos === 0 && mode === dropMode.Move ) {
                        drag.source.forcePosition(); // Use the original position
                        return;
                    }

                    for ( i = 0; i < selectedClips.length; ++i ) {
                        target = findClipItem( selectedClips[i] );
                        newPos = target.position + deltaPos;

                        var currentTrack = trackContainer( target.type )["tracks"].get( target.newTrackId );
                        if ( currentTrack )
                        {
                            var clips = currentTrack["clips"];
                            for ( var j = 0; j < clips.count; ++j )  {
                                var clip = clips.get( j );
                                if ( clip.uuid === target.uuid ||
                                     ( clip.uuid === drag.source.uuid && target.newTrackId !== drag.source.newTrackId )
                                   )
                                    continue;
                                var cPos = clip.uuid === drag.source.uuid ? ptof( drag.source.x ) : clip["position"];
                                var cEndPos = clip["position"] + clip["length"] - 1;
                                // If they overlap, create a cross-dissolve transition
                                if ( cEndPos >= newPos && newPos + target.length - 1 >= cPos )
                                {
                                    var toCreate = true;
                                    for ( var k = 0; k < allTransitions.length; ++k ) {
                                        var transitionItem = allTransitions[k];
                                        if ( transitionItem.trackId === clip.trackId &&
                                             transitionItem.type === clip.type &&
                                             transitionItem.isCrossDissolve === true &&
                                             transitionItem.clips.indexOf( clip.uuid ) !== -1 &&
                                             transitionItem.clips.indexOf( target.uuid ) !== -1
                                            ) {
                                            transitionItem.begin = Math.max( newPos, cPos );
                                            transitionItem.end = Math.min( newPos + target.length - 1, cEndPos );
                                            toCreate = false;
                                        }
                                    }
                                    if ( toCreate === true ) {
                                        addTransition( target.type, target.newTrackId,
                                                        { "begin": Math.max( newPos, cPos ),
                                                          "end": Math.min( newPos + target.length - 1, cEndPos ),
                                                          "identifier": "dissolve",
                                                          "uuid": "transitionUuid" } );
                                        transitionItem = allTransitions[allTransitions.length - 1];
                                        transitionItem.clips.push( clip.uuid );
                                        transitionItem.clips.push( target.uuid );
                                    }
                                }
                            }
                        }

                        // We only want to update the length when the right edge of the timeline
                        // is exposed.
                        if ( sView.flickableItem.contentX + page.width > sView.width &&
                                length < newPos + target.length ) {
                            length = newPos + target.length;
                        }

                        target.position = newPos;

                        // Scroll if needed
                        if ( drag.source === target || mode === dropMode.New )
                            target.scrollToThis();
                    }
                }

                if ( mode === dropMode.Move )
                    lastPos = drag.source.position;
                else if ( mode === dropMode.TransitionMove )
                    lastPos = drag.source.begin;
                else
                    lastPos = ptof( drag.x );
            }
        }

        Repeater {
            id: clipRepeater
            model: clips
            delegate: Clip {
                height: track.height - 3
                name: model.name
                trackId: model.trackId
                type: track.type
                uuid: model.uuid
                libraryUuid: model.libraryUuid
                position: model.position
                begin: model.begin
                end: model.end
                length: model.length
                clipInfo: model
            }
        }

        Repeater {
            id: transitionRepeater
            model: transitionModel
            delegate: TransitionItem {
                identifier: model.identifier
                uuid: model.uuid
                begin: model.begin
                end: model.end
                trackId: track.trackId
                type: track.type
                transitionInfo: model
            }
        }
    }

    Rectangle {
        id: trackInfo
        x: sView.flickableItem.contentX
        width: initPosOfCursor
        height: parent.height
        color: "#444444"

        Rectangle {
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
            color: "#111111"
        }

        Rectangle {
            width: 1
            height: parent.height
            anchors.left: parent.left
            color: "#111111"
        }

        Rectangle {
            width: 1
            height: parent.height
            anchors.right: parent.right
            color: "#111111"
        }

        Text {
            id: trackText
            anchors.verticalCenter: parent.verticalCenter
            x: 10
            text: type + " " + ( trackId + 1 )
            color: "white"
            font.pointSize: 10
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            x: trackText.y + trackText.contentWidth + 10
            spacing: 4

            PropertyButton {
                id: fxButton
                text: "Fx"
                selected: true

                onSelectedChanged: {
                    if ( selected === false ) {
                        workflow.showEffectStack( trackId );
                        selected = true;
                    }
                }
            }
        }
    }
}

