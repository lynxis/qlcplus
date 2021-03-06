/*
  Q Light Controller
  vcsoloframe.h

  Copyright (c) Anders Thomsen

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef VCSOLOFRAME_H
#define VCSOLOFRAME_H

#include "vcwidget.h"
#include "vcframe.h"
#include "function.h"

class QDomDocument;
class QDomElement;
class QMouseEvent;
class QString;
class Doc;

#define KXMLQLCVCSoloFrame "SoloFrame"

class VCSoloFrame : public VCFrame
{
    Q_OBJECT
    Q_DISABLE_COPY(VCSoloFrame)

    /*************************************************************************
     * Initialization
     *************************************************************************/
public:
    VCSoloFrame(QWidget* parent, Doc* doc, bool canCollapse = false);
    virtual ~VCSoloFrame();

    /*************************************************************************
     * Clipboard
     *************************************************************************/
public:
    /** @reimp */
    VCWidget* createCopy(VCWidget* parent);

    /*************************************************************************
    * Solo behaviour
    *************************************************************************/
protected:
    bool thisIsNearestSoloFrameParent(QWidget* widget);

protected slots:
    virtual void slotModeChanged(Doc::Mode mode);
    void slotButtonFunctionStarting();


    /*********************************************************************
     * Web access
     *********************************************************************/
public:
    /** @reimpl */
    QString getCSS();

    /*************************************************************************
     * Load & Save
     *************************************************************************/
protected:
    virtual QString xmlTagName() const;

    /*************************************************************************
     * Event handlers
     *************************************************************************/
protected:
    virtual void paintEvent(QPaintEvent* e);

};

#endif
