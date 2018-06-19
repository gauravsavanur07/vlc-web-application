import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2

Rectangle {
    id: transition

    x: ftop( begin )
    y: inTrack ? 0 : ( type === "Video" ? trackHeight : 0 ) - height / 2
    z: maxZ + 100
    height: inTrack ? trackHeight - 3 : trackHeight / 2
    width: ftop( end - begin )
    color: inTrack ? "#000000" : "#AAAA88"
    opacity: inTrack ? 0.5 : 1.0
    border.color: "#000000"
    border.width: 1

    property string uuid: "transitionUuid"
    property string identifier
    property string name
    property bool isCrossDissolve: identifier === "dissolve"
    property bool isFadeOut: false
    property bool isFadeIn: false
     // Whether it's in a track or between tracks.
    readonly property bool inTrack: isCrossDissolve || isFadeOut || isFadeIn
    property int begin
    property int end
    property int trackId
    property string type
    property var clips: [] // clips overlapping
    property var transitionInfo

    Drag.keys: ["Transition"]
    Drag.active: mouseArea.drag.active

    onUuidChanged: {
        transitionInfo["uuid"] = uuid;
        allTransitionsDict[uuid] = transition;
    }

    Component.onCompleted: {
        allTransitions.push( transition );
        if ( uuid )
            allTransitionsDict[uuid] = transition;
    }

    Component.onDestruction: {
        Drag.drop();
        for ( var i = 0; i < allTransitions.length; ++i ) {
            if ( allTransitions[i] === transition ) {
                allTransitions.splice( i, 1 );
                return;
            }
        }
    }

    onYChanged: {
        if ( inTrack === false ) {
            if ( ( y + height / 2 ) % trackHeight !== 0 )
                y -= ( y + height / 2 ) % trackHeight;
        }
        // Don't move outside its TrackContainer
        // For Top
        var yToMoveUp = track.mapToItem( container, 0, 0 ).y + y;
        if ( yToMoveUp < 0 )
            y += trackHeight;
        // For Bottom
        if ( yToMoveUp + height > container.height )
            y -= trackHeight;
    }

    Canvas {
        id: tCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext( "2d" );
            ctx.strokeStyle = Qt.rgba( 0.9, 0.8, 0.25, 1 );
            ctx.lineWidth = 1;
            ctx.beginPath();
            if ( isCrossDissolve === true || isFadeOut === true )
            {
                ctx.moveTo( 0, 0 );
                ctx.lineTo( width, height );
            }
            if ( isCrossDissolve === true || isFadeIn === true )
            {
                ctx.moveTo( 0, height );
                ctx.lineTo( width, 0 );
            }
            ctx.closePath();
            ctx.stroke();
        }
    }

    Text {
        id: nameLabel
        visible: !inTrack
        text: identifier
        width: parent.width
        elide: Text.ElideRight
        font.pixelSize: parent.height - 5
        anchors.centerIn: parent
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        visible: !isCrossDissolve
        drag.target: resizing || isCrossDissolve ? null : parent
        drag.minimumX: 0
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor

        property bool resizing: false

        onPositionChanged: {
            // If it's too short, don't resize.
            if ( width < 6 ) {
                return;
            }

            if ( mouseArea.pressed === true ) {
                // Handle resizing
                if ( resizing === true ) {
                    var oldBegin = begin;
                    var oldEnd = end;
                    if ( mouseX < width / 2 ) {
                        var newBegin = begin + ptof( mouseX );
                        if ( newBegin < 0 || newBegin >= end )
                            return;
                        begin = newBegin;
                    }
                    else {
                        var newEnd = begin + ptof( mouseX );
                        if ( newEnd <= begin )
                            return;
                        end = newEnd;
                    }
                }
            }
            else {
                if ( mouseX < 3 || ( transition.width - mouseX ) < 3 )
                    resizing = true;
                else
                    resizing = false;
            }
        }

        onReleased: {
            if ( transitionInfo["begin"] !== begin || transitionInfo["end"] !== end )
                workflow.moveTransition( uuid, begin, end );
            if ( transitionInfo["trackBId"] !== trackId )
                workflow.moveTransitionBetweenTracks( uuid, trackId - 1, trackId );
        }

        onClicked: {
            if ( mouse.button & Qt.RightButton ) {
                transitionContextMenu.popup();
            }
        }

        states: [
            State {
                name: "Move"
                when: !mouseArea.pressed && !mouseArea.resizing
                PropertyChanges { target: mouseArea; cursorShape: Qt.OpenHandCursor }
            },
            State {
                name: "Resizing"
                when: mouseArea.resizing
                PropertyChanges { target: mouseArea; cursorShape: Qt.SizeHorCursor }
            },
            State {
                name: "Dragging"
                when: mouseArea.pressed && !mouseArea.resizing
                PropertyChanges { target: mouseArea; cursorShape: Qt.ClosedHandCursor }
            }
        ]
    }

    Menu {
        id: transitionContextMenu
        title: "Edit"

        MenuItem {
            text: "Delete"

            onTriggered: {
                removeTransitionDialog.visible = true;
            }
        }

        MessageDialog {
            id: removeTransitionDialog
            title: "VLMC"
            text: qsTr( "Do you really want to remove the transition?" )
            icon: StandardIcon.Question
            standardButtons: StandardButton.Yes | StandardButton.No
            onYes: {
                workflow.removeTransition( uuid );
            }
        }
    }
}
