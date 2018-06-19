import QtQuick 2.0

Rectangle {
    id: effect
    color: "#444444"
    border.color: "#222222"
    border.width: 1

    height: childrenRect.height

    property string identifier
    property string name
    property string description

    Column {
        Text {
            text: name
            font.pointSize: 14
            color: "#EEEEEE"
            elide: Text.ElideRight
        }

        Text {
            text: description
            color: "#EEEEEE"
            elide: Text.ElideRight
        }
    }
    MouseArea {
        anchors.fill: parent
        onPressed: {
            view.startDrag( identifier );
        }
    }
}
