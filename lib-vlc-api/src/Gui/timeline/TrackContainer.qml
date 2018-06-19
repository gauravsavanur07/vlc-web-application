import QtQuick 2.0

Rectangle {
    id: container
    width: parent.width
    height: tracks.count * trackHeight
    color: "#222222"

    property ListModel tracks
    property bool isUpward
    property string type

    ListView {
        anchors.fill: parent
        verticalLayoutDirection: isUpward ? ListView.BottomToTop  : ListView.TopToBottom
        interactive: false
        focus: true
        model: tracks
        delegate: Track {
            trackId: index
            type: container.type
            clips: model["clips"]
            transitionModel: model["transitions"]
        }
    }
}
