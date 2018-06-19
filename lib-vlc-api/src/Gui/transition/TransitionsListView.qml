import QtQuick 2.0
import QtQuick.Controls 1.4

Rectangle {
    anchors.fill: parent
    color: "#999999"

    ListModel {
        id: transitionsList
    }

    Component.onCompleted: {
        var transitionsInfo = view.transitions();
        for ( var i = 0; i < transitionsInfo.length; ++i ) {
            transitionsList.append( transitionsInfo[i] );
        }
    }

    ScrollView {
        id: sView
        height: parent.height
        width: parent.width

        ListView {
            width: sView.viewport.width
            model: transitionsList
            delegate: Effect {
                width: sView.viewport.width
                identifier: model.identifier
                name: model.name
                description: model.description
            }
        }
    }
}
