/***************************************************************************
 valuemap.qml
  --------------------------------------
  Date                 : 2017
  Copyright            : (C) 2017 by Matthias Kuhn
  Email                : matthias@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 2.7
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.0
import "../components"

/**
 * Value Map for QGIS Attribute Form
 * Requires various global properties set to function, see featureform Loader section
 * Do not use directly from Application QML
 */
Item {
  signal editorValueChanged(var newValue, bool isNull)
  property bool isReadOnly: readOnly

  id: fieldItem
  enabled: !readOnly
  height: customStyle.fields.height
  anchors {
    left: parent.left
    right: parent.right
  }

  InputComboBox {
    // Reversed to model's key-value map. It is used to find index according current value
    property var reverseConfig: ({})
    property var currentEditorValue: value

    comboStyle: customStyle.fields
    textRole: 'display'
    height: parent.height
    readOnly: isReadOnly
    iconSize: fieldItem.height * 0.50
    model: ListModel {
      id: listModel
    }

    Component.onCompleted: {
      var currentMap;
      var currentKey;

      if( config['map'] )
      {
        if( config['map'].length )
        {
          //it's a list (>=QGIS3.0)
          for(var i=0; i<config['map'].length; i++)
          {
            currentMap = config['map'][i]
            currentKey = Object.keys(currentMap)[0]
            listModel.append( { display: currentKey } )
            reverseConfig[currentMap[currentKey]] = currentKey;
          }
        }
        else
        {
          //it's a map (<=QGIS2.18)
          currentMap = config['map'].length ? config['map'][currentIndex] : config['map']
          currentKey = Object.keys(currentMap)[0]
          for(var key in config['map']) {
            listModel.append( { display: key } )
            reverseConfig[config['map'][key]] = key;
          }
        }
      }
      currentIndex = find(reverseConfig[value])
    }

    onCurrentTextChanged: {
      var currentMap = config['map'].length ? config['map'][currentIndex] : config['map']
      if (currentMap)
        editorValueChanged(currentMap[currentText], false)
    }

    // Workaround to get a signal when the value has changed
    onCurrentEditorValueChanged: {
      currentIndex = find(reverseConfig[value])
    }

  }
}
