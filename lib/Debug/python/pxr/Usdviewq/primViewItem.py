#
# Copyright 2016 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
from __future__ import print_function

from .qt import QtCore, QtGui, QtWidgets
from pxr import Sdf, Usd, UsdGeom
from pxr.UsdUtils.constantsGroup import ConstantsGroup
from ._usdviewq import Utils

from .common import UIPrimTypeColors, UIFonts

HALF_DARKER = 150

# Pulled out as a wrapper to facilitate cprofile tracking
def _GetPrimInfo(prim, time):
    return Utils.GetPrimInfo(prim, time)


class PrimViewColumnIndex(ConstantsGroup):
    NAME, TYPE, VIS, GUIDES, DRAWMODE = range(5)

# This class extends QTreeWidgetItem to also contain all the stage
# prim data associated with it and populate itself with that data.

class PrimViewItem(QtWidgets.QTreeWidgetItem):
    def __init__(self, prim, appController, primHasChildren):
        # Do *not* pass a parent.  The client must build the hierarchy.
        # This can dramatically improve performance when building a
        # large hierarchy.
        super(PrimViewItem, self).__init__()

        self.prim = prim
        self._appController = appController
        self._needsPull = True
        self._needsPush = True
        self._needsChildrenPopulated = primHasChildren

        # Initialize these now so loadVis(), _visData() and onClick() can
        # use them without worrying if _pull() has been called.
        self.imageable = False
        self.active = False
        self.vis = False

        # True if this item is an ancestor of a selected item.
        self.ancestorOfSelected = False

        # If we know we'll have children show a norgie, otherwise don't.
        if primHasChildren:
            self.setChildIndicatorPolicy(
                    QtWidgets.QTreeWidgetItem.ShowIndicator)
        else:
            self.setChildIndicatorPolicy(
                    QtWidgets.QTreeWidgetItem.DontShowIndicator)

        # If this item includes a persistent drawMode widget, it is stored here.
        self.drawModeWidget = None
        
    def push(self):
        """Pushes prim data to the UI."""
        # Push to UI.
        if self._needsPush:
            self._needsPush = False
            self._pull()

    def _pull(self):
        """Extracts and stores prim data."""
        if self._needsPull:
            # Only do this once.
            self._needsPull = False

            # Visibility is recursive so the parent must pull before us.
            parent = self.parent()
            if isinstance(parent, PrimViewItem):
                parent._pull()

            # Get our prim info.
            # To avoid Python overhead, request data in batch from C++.
            info = _GetPrimInfo(
                self.prim, self._appController._dataModel.currentFrame)
            self._extractInfo(info)

            self.emitDataChanged()

    @staticmethod
    def _HasAuthoredDrawMode(prim):
        modelAPI = UsdGeom.ModelAPI(prim)
        drawModeAttr = modelAPI.GetModelDrawModeAttr()
        return drawModeAttr and drawModeAttr.HasAuthoredValue()

    def _isComputedDrawModeInherited(self, parentDrawModeIsInherited=None):
        """Returns true if the computed draw mode for this item is inherited 
           from an authored "model:drawMode" value on an ancestor prim.
        """
        if PrimViewItem._HasAuthoredDrawMode(self.prim):
            return False

        parent = self.prim.GetParent()
        while parent and parent.GetPath() != Sdf.Path.absoluteRootPath:
            if PrimViewItem._HasAuthoredDrawMode(parent):
                return True

            # Stop the upward traversal if we know whether the parent's draw 
            # mode is inherited.
            if parentDrawModeIsInherited is not None:
                return parentDrawModeIsInherited
            
            parent = parent.GetParent()
        return False

    def _extractInfo(self, info):
        ( self.hasArcs,
          self.active,
          self.imageable,
          self.defined,
          self.abstract,
          self.isInPrototype,
          self.isInstance,
          self.supportsGuides,
          self.supportsDrawMode,
          isVisibilityInherited,
          self.visVaries,
          self.name,
          self.typeName ) = info

        parent = self.parent()
        parentIsPrimViewItem = isinstance(parent, PrimViewItem)
        self.computedVis =  parent.computedVis if parentIsPrimViewItem \
                            else UsdGeom.Tokens.inherited
        if self.imageable and self.active:
            if isVisibilityInherited:
                self.vis = UsdGeom.Tokens.inherited
            else:
                self.vis = self.computedVis = UsdGeom.Tokens.invisible

        # If this is the invisible root item, initialize fallback values for 
        # the drawMode related parameters.
        if not parentIsPrimViewItem:
            self.computedDrawMode = ''
            self.isDrawModeInherited = False
            return

        # We don't need to compute drawMode related parameters for primViewItems
        # that don't support draw mode.
        if not self.supportsDrawMode:
            return
        
        self.computedDrawMode = UsdGeom.ModelAPI(self.prim).ComputeModelDrawMode(
            parent.computedDrawMode) if parentIsPrimViewItem else ''


        parentDrawModeIsInherited = parent.isDrawModeInherited if \
                parentIsPrimViewItem else None
        
        self.isDrawModeInherited = self._isComputedDrawModeInherited(
                    parentDrawModeIsInherited)
        

    def addChildren(self, children):
        """Adds children to the end of this item.  This is the only
           method clients should call to manage an item's children."""
        self._needsChildrenPopulated = False
        super(PrimViewItem, self).addChildren(children)

    def data(self, column, role):
        # All Qt queries that affect the display of this item (label, font,
        # color, etc) will come through this method.  We set that data
        # lazily because it's expensive to do and (for the most part) Qt
        # only needs the data for items that are visible.
        self.push()

        # We report data directly rather than use set...() and letting
        # super().data() return it because Qt can be pathological during
        # calls to set...() when the item hierarchy is attached to a view,
        # making thousands of calls to data().
        result = None
        if column == PrimViewColumnIndex.NAME:
            result = self._nameData(role)
        elif column == PrimViewColumnIndex.TYPE:
            result = self._typeData(role)
        elif column == PrimViewColumnIndex.VIS:
            result = self._visData(role)
        elif column == PrimViewColumnIndex.GUIDES and self.supportsGuides:
            # XXX Temp fix to prevent API calls from throwing an exception since
            # this method is being called on shutdown, after the stage has
            # closed, on expired prims
            if self.prim:
                result = self._guideData(role)
        elif column == PrimViewColumnIndex.DRAWMODE and self.supportsDrawMode:
            result = self._drawModeData(role)
        if not result:
            result = super(PrimViewItem, self).data(column, role)
        return result

    def _GetForegroundColor(self):
        self.push()
        
        if self.isInstance:
            color = UIPrimTypeColors.INSTANCE
        elif self.hasArcs:
            color = UIPrimTypeColors.HAS_ARCS
        elif self.isInPrototype:
            color = UIPrimTypeColors.PROTOTYPE
        else:
            color = UIPrimTypeColors.NORMAL
        return color.color() if self.active else color.color().darker(HALF_DARKER)

    def _nameData(self, role):
        if role == QtCore.Qt.DisplayRole:
            return self.name
        elif role == QtCore.Qt.FontRole:
            # Abstract prims are also considered defined; since we want
            # to distinguish abstract defined prims from non-abstract
            # defined prims, we check for abstract first.
            if self.abstract:
                return UIFonts.ABSTRACT_PRIM
            elif not self.defined:
                return UIFonts.OVER_PRIM
            else:
                return UIFonts.DEFINED_PRIM
        elif role == QtCore.Qt.ForegroundRole:
            return self._GetForegroundColor()
        elif role == QtCore.Qt.ToolTipRole:
            toolTip = 'Prim'
            if len(self.typeName) > 0:
                toolTip = self.typeName + ' ' + toolTip
            if self.isInPrototype:
                toolTip = 'Prototype ' + toolTip
            if not self.defined:
                toolTip = 'Undefined ' + toolTip
            elif self.abstract:
                toolTip = 'Abstract ' + toolTip
            else:
                toolTip = 'Defined ' + toolTip
            if not self.active:
                toolTip = 'Inactive ' + toolTip
            elif self.isInstance:
                toolTip = 'Instanced ' + toolTip
            if self.hasArcs:
                toolTip = toolTip + "<br>Has composition arcs"
            return toolTip
        else:
            return None

    def _drawModeData(self, role):
        if role == QtCore.Qt.DisplayRole:
            return self.computedDrawMode
        elif role == QtCore.Qt.FontRole:
            return UIFonts.BOLD_ITALIC if self.isDrawModeInherited else \
                   UIFonts.DEFINED_PRIM
        elif role == QtCore.Qt.ForegroundRole:
            color = self._GetForegroundColor()
            return color.darker(110) if self.isDrawModeInherited else \
                   color

    def _typeData(self, role):
        if role == QtCore.Qt.DisplayRole:
            return self.typeName
        else:
            return self._nameData(role)
            
    def _isVisInherited(self):
        return self.imageable and self.active and \
               self.vis != UsdGeom.Tokens.invisible and \
               self.computedVis == UsdGeom.Tokens.invisible

    def _visData(self, role):
        if role == QtCore.Qt.DisplayRole:
            if self.imageable and self.active:
                return "I" if self.vis == UsdGeom.Tokens.invisible else "V"
            else:
                return ""
        elif role == QtCore.Qt.TextAlignmentRole:
            return QtCore.Qt.AlignCenter
        elif role == QtCore.Qt.FontRole:
            return UIFonts.BOLD_ITALIC if self._isVisInherited() \
                   else UIFonts.BOLD
        elif role == QtCore.Qt.ForegroundRole:
            fgColor = self._GetForegroundColor()
            return fgColor.darker() if self._isVisInherited() \
                   else fgColor
        elif role == QtCore.Qt.ToolTipRole:
            if self.imageable and self.active:
                if self.vis == UsdGeom.Tokens.invisible:
                    return "Invisible Prim"
                else:
                    return "Visible Prim"
        else:
            return None

    def _guideData(self, role):
        if role == QtCore.Qt.DisplayRole:
            if (UsdGeom.VisibilityAPI(self.prim).GetGuideVisibilityAttr().Get()
                == UsdGeom.Tokens.visible):
                return "V"
            else:
                return "I"
        elif role == QtCore.Qt.TextAlignmentRole:
            return QtCore.Qt.AlignCenter
        elif role == QtCore.Qt.FontRole:
            if self._isVisInherited() or self.vis == UsdGeom.Tokens.invisible:
                return UIFonts.BOLD_ITALIC
            else:
                return UIFonts.BOLD
        elif role == QtCore.Qt.ForegroundRole:
            fgColor = self._GetForegroundColor()
            if self._isVisInherited() or self.vis == UsdGeom.Tokens.invisible:
                return fgColor.darker()
            else:
                return fgColor
        elif role == QtCore.Qt.ToolTipRole:
            if (UsdGeom.VisibilityAPI(self.prim).GetGuideVisibilityAttr().Get()
                == UsdGeom.Tokens.visible):
                return "Visible Guides"
            else:
                return "Invisible Guides"
        else:
            return None

    def needsChildrenPopulated(self):
        return self._needsChildrenPopulated

    def canChangeVis(self):
        if not self.imageable:
            print("WARNING: The prim <" + str(self.prim.GetPath()) + \
                    "> is not imageable. Cannot change visibility.")
            return False
        elif self.isInPrototype:
            print("WARNING: The prim <" + str(self.prim.GetPath()) + \
                   "> is in a prototype. Cannot change visibility.")
            return False
        return True

    def loadVis(self, inheritedVis, visHasBeenAuthored):
        if not (self.imageable and self.active):
            return inheritedVis

        time = self._appController._dataModel.currentFrame
        # If visibility-properties have changed on the stage, then
        # we must re-evaluate our variability before deciding whether
        # we can avoid re-reading our visibility
        visAttr = UsdGeom.Imageable(self.prim).GetVisibilityAttr()
        if visHasBeenAuthored:
            self.visVaries = visAttr.ValueMightBeTimeVarying()

            if not self.visVaries:
                self.vis = visAttr.Get(time)

        if self.visVaries:
            self.vis =  visAttr.Get(time)

        self.computedVis = UsdGeom.Tokens.invisible \
            if self.vis == UsdGeom.Tokens.invisible \
            else inheritedVis

        self.emitDataChanged()
        return self.computedVis

    @staticmethod
    def propagateDrawMode(item, primView, parentDrawMode='', 
                          parentDrawModeIsInherited=None):
        # If this item does not support draw mode, none of its descendants 
        # can support it. Hence, stop recursion here.
        # 
        # Call push() here to ensure that supportsDrawMode has been populated
        # for the item.
        item.push()
        if not item.supportsDrawMode:
            return

        from .primTreeWidget import DrawModeWidget
        drawModeWidget = item.drawModeWidget
        if drawModeWidget:
            drawModeWidget.RefreshDrawMode()
        else:
            modelAPI = UsdGeom.ModelAPI(item.prim)
            item.computedDrawMode = modelAPI.ComputeModelDrawMode(parentDrawMode)
            item.isDrawModeInherited = item._isComputedDrawModeInherited(
                    parentDrawModeIsInherited=parentDrawModeIsInherited)
            item.emitDataChanged()

        # Traverse down to children to update their drawMode.
        for child in [item.child(i) for i in range(item.childCount())]:
            PrimViewItem.propagateDrawMode(child, primView, 
                    parentDrawMode=item.computedDrawMode,
                    parentDrawModeIsInherited=item.isDrawModeInherited)

    @staticmethod
    def propagateVis(item, authoredVisHasChanged=True):
        parent = item.parent()
        inheritedVis = parent._resetAncestorsRecursive(authoredVisHasChanged) \
            if isinstance(parent, PrimViewItem) \
            else UsdGeom.Tokens.inherited
        # This may be called on the "InvisibleRootItem" that is an ordinary
        # QTreeWidgetItem, in which case we need to process its children
        # individually
        if isinstance(item, PrimViewItem):
            item._pushVisRecursive(inheritedVis, authoredVisHasChanged)
        else:
            for child in [item.child(i) for i in range(item.childCount())]:
                child._pushVisRecursive(inheritedVis, authoredVisHasChanged)

    def _resetAncestorsRecursive(self, authoredVisHasChanged):
        parent = self.parent()
        inheritedVis = parent._resetAncestorsRecursive(authoredVisHasChanged) \
            if isinstance(parent, PrimViewItem) \
            else UsdGeom.Tokens.inherited

        return self.loadVis(inheritedVis, authoredVisHasChanged)

    def _pushVisRecursive(self, inheritedVis, authoredVisHasChanged):
        myComputedVis = self.loadVis(inheritedVis, authoredVisHasChanged)

        for child in [self.child(i) for i in range(self.childCount())]:
            child._pushVisRecursive(myComputedVis, authoredVisHasChanged)

    def setLoaded(self, loaded):
        if self.prim.IsPrototype():
            print("WARNING: The prim <" + str(self.prim.GetPath()) + \
                   "> is a prototype prim. Cannot change load state.")
            return

        if self.prim.IsActive():
            if loaded:
                self.prim.Load()
            else:
                self.prim.Unload()

    def setVisible(self, visible):
        if self.canChangeVis():
            UsdGeom.Imageable(self.prim).GetVisibilityAttr().Set(UsdGeom.Tokens.inherited
                             if visible else UsdGeom.Tokens.invisible)
            self.visChanged()

    def makeVisible(self):
        if self.canChangeVis():
            # It is in general not kosher to use an Sdf.ChangeBlock around
            # operations at the Usd API level.  We have carefully arranged
            # (with insider knowledge) to have only "safe" mutations
            # happening inside the ChangeBlock.  We do this because
            # UsdImaging updates itself independently for each
            # Usd.Notice.ObjectsChanged it receives.  We hope to eliminate
            # the performance need for this by addressing bug #121992
            from pxr import Sdf
            with Sdf.ChangeBlock():
                UsdGeom.Imageable(self.prim).MakeVisible()
            self.visChanged()

    def visChanged(self):
        # called when user authors a new visibility value
        # we must re-determine if visibility is varying over time
        self.loadVis(self.parent().computedVis, True)

    def toggleVis(self):
        """Return True if the the prim's visibility state was toggled. """
        if self.imageable and self.active:
            self.setVisible(self.vis == UsdGeom.Tokens.invisible)
            PrimViewItem.propagateVis(self)
            return True
        return False

    def toggleGuides(self):
        """Return True if the the prim's guide visibility state was toggled."""
        if not self.supportsGuides:
            return False
        attr = (
            UsdGeom.VisibilityAPI.Apply(self.prim).CreateGuideVisibilityAttr())
        return attr.Set(UsdGeom.Tokens.invisible
            if attr.Get() == UsdGeom.Tokens.visible else UsdGeom.Tokens.visible)
