/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef PAD_H
#define PAD_H

#include <zones.h>
#include <board_connected_item.h>
#include <board_item.h>
#include <convert_to_biu.h>
#include <geometry/shape_poly_set.h>
#include <geometry/shape_compound.h>
#include <pad_shapes.h>
#include <pcbnew.h>

class PCB_SHAPE;
class PARAM_CFG;
class SHAPE;
class SHAPE_SEGMENT;

enum CUST_PAD_SHAPE_IN_ZONE
{
    CUST_PAD_SHAPE_IN_ZONE_OUTLINE,
    CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL
};

class LINE_READER;
class EDA_3D_CANVAS;
class FOOTPRINT;
class FP_SHAPE;
class TRACK;

namespace KIGFX
{
    class VIEW;
}

class PAD : public BOARD_CONNECTED_ITEM
{
public:
    PAD( FOOTPRINT* parent );

    // Copy constructor & operator= are needed because the list of basic shapes
    // must be duplicated in copy.
    PAD( const PAD& aPad );
    PAD& operator=( const PAD &aOther );

    /*
     * Default layers used for pads, according to the pad type.
     * this is default values only, they can be changed for a given pad
     */
    static LSET PTHMask();     ///< layer set for a through hole pad
    static LSET SMDMask();          ///< layer set for a SMD pad on Front layer
    static LSET ConnSMDMask();      ///< layer set for a SMD pad on Front layer
                                    ///< used for edge board connectors
    static LSET UnplatedHoleMask(); ///< layer set for a mechanical unplated through hole pad
    static LSET ApertureMask();     ///< layer set for an aperture pad

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && PCB_PAD_T == aItem->Type();
    }

    bool IsType( const KICAD_T aScanTypes[] ) const override
    {
        if( BOARD_CONNECTED_ITEM::IsType( aScanTypes ) )
            return true;

        for( const KICAD_T* p = aScanTypes; *p != EOT; ++p )
        {
            if( m_drill.x > 0 && m_drill.y > 0 )
            {
                if( *p == PCB_LOCATE_HOLE_T )
                    return true;
                else if( *p == PCB_LOCATE_PTH_T && m_attribute != PAD_ATTRIB_NPTH )
                    return true;
                else if( *p == PCB_LOCATE_NPTH_T && m_attribute == PAD_ATTRIB_NPTH )
                    return true;
            }
        }

        return false;
    }

    FOOTPRINT* GetParent() const;

    /**
     * Imports the pad settings from aMasterPad.
     * The result is "this" has the same settinds (sizes, shapes ... )
     * as aMasterPad
     * @param aMasterPad = the template pad
     */
    void ImportSettingsFrom( const PAD& aMasterPad );

    /**
     * @return true if the pad has a footprint parent flipped
     * (on the back/bottom layer)
     */
    bool IsFlipped() const;

    /**
     * Set the pad name (sometimes called pad number, although
     * it can be an array reference like AA12).
     */
    void SetName( const wxString& aName ) { m_name = aName; }
    const wxString& GetName() const { return m_name; }

    /**
     * Set the pad function (pin name in schematic)
     */
    void SetPinFunction( const wxString& aName ) { m_pinFunction = aName; }
    const wxString& GetPinFunction() const { return m_pinFunction; }

    /**
     * Before we had custom pad shapes it was common to have multiple overlapping pads to
     * represent a more complex shape.
     * @param other
     * @return
     */
    bool SameLogicalPadAs( const PAD* other ) const
    {
        // hide tricks behind sensible API
        return GetParent() == other->GetParent() && m_name == other->m_name;
    }

    /**
     * Set the new shape of this pad.
     */
    void SetShape( PAD_SHAPE_T aShape )
    {
        m_padShape = aShape;
        m_shapesDirty = true;
    }

    /**
     * @return the shape of this pad.
     */
    PAD_SHAPE_T GetShape() const { return m_padShape; }

    void SetPosition( const wxPoint& aPos ) override
    {
        m_pos = aPos;
        m_shapesDirty = true;
    }

    wxPoint GetPosition() const override { return m_pos; }

    /**
     * Function GetAnchorPadShape
     * @return the shape of the anchor pad shape, for custom shaped pads.
     */
    PAD_SHAPE_T GetAnchorPadShape() const       { return m_anchorPadShape; }

    /**
     * @return the option for the custom pad shape to use as clearance area
     * in copper zones
     */
    CUST_PAD_SHAPE_IN_ZONE GetCustomShapeInZoneOpt() const
    {
        return m_customShapeClearanceArea;
    }

    /**
     * Set the option for the custom pad shape to use as clearance area
     * in copper zones
     * @param aOption is the clearance area shape CUST_PAD_SHAPE_IN_ZONE option
     */
    void SetCustomShapeInZoneOpt( CUST_PAD_SHAPE_IN_ZONE aOption )
    {
        m_customShapeClearanceArea = aOption;
    }

    /**
     * Function SetAnchorPadShape
     * Set the shape of the anchor pad for custm shped pads.
     * @param the shape of the anchor pad shape( currently, only
     * PAD_SHAPE_RECT or PAD_SHAPE_CIRCLE.
     */
    void SetAnchorPadShape( PAD_SHAPE_T aShape )
    {
        m_anchorPadShape = ( aShape ==  PAD_SHAPE_RECT ) ? PAD_SHAPE_RECT : PAD_SHAPE_CIRCLE;
        m_shapesDirty = true;
    }

    /**
     * @return true if the pad is on any copper layer, false otherwise.
     * pads can be only on tech layers to build special pads.
     * they are therefore not always on a copper layer
     */
    bool IsOnCopperLayer() const override
    {
        return ( GetLayerSet() & LSET::AllCuMask() ) != 0;
    }

    void SetY( int y )                          { m_pos.y = y; m_shapesDirty = true; }
    void SetX( int x )                          { m_pos.x = x; m_shapesDirty = true; }

    void SetPos0( const wxPoint& aPos )         { m_pos0 = aPos; }
    const wxPoint& GetPos0() const              { return m_pos0; }

    void SetY0( int y )                         { m_pos0.y = y; }
    void SetX0( int x )                         { m_pos0.x = x; }

    void SetSize( const wxSize& aSize )         { m_size = aSize; m_shapesDirty = true; }
    const wxSize& GetSize() const               { return m_size; }
    void SetSizeX( const int aX )               { m_size.x = aX; m_shapesDirty = true; }
    const int GetSizeX() const                  { return m_size.x; }
    void SetSizeY( const int aY )               { m_size.y = aY; m_shapesDirty = true; }
    const int GetSizeY() const                  { return m_size.y; }

    void SetDelta( const wxSize& aSize )        { m_deltaSize = aSize; m_shapesDirty = true; }
    const wxSize& GetDelta() const              { return m_deltaSize; }

    void SetDrillSize( const wxSize& aSize )    { m_drill = aSize; m_shapesDirty = true; }
    const wxSize& GetDrillSize() const          { return m_drill; }
    void SetDrillSizeX( const int aX )          { m_drill.x = aX; m_shapesDirty = true; }
    const int GetDrillSizeX() const             { return m_drill.x; }
    void SetDrillSizeY( const int aY )          { m_drill.y = aY; m_shapesDirty = true; }
    const int GetDrillSizeY() const             { return m_drill.y; }

    void SetOffset( const wxPoint& aOffset )    { m_offset = aOffset; m_shapesDirty = true; }
    const wxPoint& GetOffset() const            { return m_offset; }

    wxPoint GetCenter() const override          { return GetPosition(); }

    /**
     * Has meaning only for custom shape pads.
     * add a free shape to the shape list.
     * the shape can be
     *   a polygon (outline can have a thickness)
     *   a thick segment
     *   a filled circle (thickness == 0) or ring
     *   a filled rect (thickness == 0) or rectangular outline
     *   a arc
     *   a bezier curve
     */
    void AddPrimitivePoly( const SHAPE_POLY_SET& aPoly, int aThickness, bool aFilled );
    void AddPrimitivePoly( const std::vector<wxPoint>& aPoly, int aThickness, bool aFilled );
    void AddPrimitiveSegment( const wxPoint& aStart, const wxPoint& aEnd, int aThickness );
    void AddPrimitiveCircle( const wxPoint& aCenter, int aRadius, int aThickness, bool aFilled );
    void AddPrimitiveRect( const wxPoint& aStart, const wxPoint& aEnd, int aThickness,
                           bool aFilled );
    void AddPrimitiveArc( const wxPoint& aCenter, const wxPoint& aStart, int aArcAngle,
                          int aThickness );
    void AddPrimitiveCurve( const wxPoint& aStart, const wxPoint& aEnd, const wxPoint& aCtrl1,
                            const wxPoint& aCtrl2, int aThickness );


    bool GetBestAnchorPosition( VECTOR2I& aPos );

    /**
     * Merge all basic shapes to a SHAPE_POLY_SET
     * Note: The results are relative to the pad position, orientation 0.
     */
    void MergePrimitivesAsPolygon( SHAPE_POLY_SET* aMergedPolygon, PCB_LAYER_ID aLayer ) const;

    /**
     * clear the basic shapes list
     */
    void DeletePrimitivesList();

    /**
     * Accessor to the basic shape list for custom-shaped pads.
     */
    const std::vector<std::shared_ptr<PCB_SHAPE>>& GetPrimitives() const
    {
        return m_editPrimitives;
    }

    void Flip( const wxPoint& aCentre, bool aFlipLeftRight ) override;

    /**
     * Flip (mirror) the primitives left to right or top to bottom, around the anchor position
     * in custom pads
     */
    void FlipPrimitives( bool aFlipLeftRight );

    /**
     * Clear the current custom shape primitives list and import a new list.  Copies the input,
     * which is not altered.
     */
    void ReplacePrimitives( const std::vector<std::shared_ptr<PCB_SHAPE>>& aPrimitivesList );

    /**
     * Import a custom shape primites list (composed of basic shapes) and add items to the
     * current list.  Copies the input, which is not altered.
     */
    void AppendPrimitives( const std::vector<std::shared_ptr<PCB_SHAPE>>& aPrimitivesList );

    /**
     * Add item to the custom shape primitives list
     */
    void AddPrimitive( PCB_SHAPE* aPrimitive );

    /**
     * Function SetOrientation
     * sets the rotation angle of the pad.
     * @param aAngle in tenths of degrees.  If it is outside of 0 - 3600, then it will be
     * normalized before being saved.
     */
    void SetOrientation( double aAngle );

    /**
     * Set orientation in degrees.
     */
    void SetOrientationDegrees( double aOrientation ) { SetOrientation( aOrientation*10.0 ); }

    /**
     * Function GetOrientation
     * returns the rotation angle of the pad in a variety of units (the basic call returns
     * tenths of degrees).
     */
    double GetOrientation() const { return m_orient; }
    double GetOrientationDegrees() const   { return m_orient/10.0; }
    double GetOrientationRadians() const   { return m_orient*M_PI/1800; }

    void SetDrillShape( PAD_DRILL_SHAPE_T aShape ) { m_drillShape = aShape; m_shapesDirty = true; }
    PAD_DRILL_SHAPE_T GetDrillShape() const     { return m_drillShape; }

    bool IsDirty() const { return m_shapesDirty; }
    void SetDirty() { m_shapesDirty = true; }

    void SetLayerSet( LSET aLayers ) override   { m_layerMask = aLayers; }
    LSET GetLayerSet() const override           { return m_layerMask; }

    void SetAttribute( PAD_ATTR_T aAttribute );
    PAD_ATTR_T GetAttribute() const             { return m_attribute; }

    void SetProperty( PAD_PROP_T aProperty );
    PAD_PROP_T GetProperty() const              { return m_property; }

    // We don't currently have an attribute for APERTURE, and adding one will change the file
    // format, so for now just infer a copper-less pad to be an APERTURE pad.
    bool IsAperturePad() const                  { return ( m_layerMask & LSET::AllCuMask() ).none(); }

    void SetPadToDieLength( int aLength )       { m_lengthPadToDie = aLength; }
    int GetPadToDieLength() const               { return m_lengthPadToDie; }

    int GetLocalSolderMaskMargin() const        { return m_localSolderMaskMargin; }
    void SetLocalSolderMaskMargin( int aMargin ) { m_localSolderMaskMargin = aMargin; }

    int GetLocalClearance( wxString* aSource ) const override;
    int GetLocalClearance() const               { return m_localClearance; }
    void SetLocalClearance( int aClearance )    { m_localClearance = aClearance; }

    int GetLocalSolderPasteMargin() const       { return m_localSolderPasteMargin; }
    void SetLocalSolderPasteMargin( int aMargin ) { m_localSolderPasteMargin = aMargin; }

    double GetLocalSolderPasteMarginRatio() const { return m_localSolderPasteMarginRatio; }
    void SetLocalSolderPasteMarginRatio( double aRatio ) { m_localSolderPasteMarginRatio = aRatio; }

    /**
     * Function TransformShapeWithClearanceToPolygon
     * Convert the pad shape to a closed polygon. Circles and arcs are approximated by segments.
     * @param aCornerBuffer = a buffer to store the polygon
     * @param aClearanceValue = the clearance around the pad
     * @param aMaxError = maximum error from true when converting arcs
     * @param aErrorLoc = should the approximation error be placed outside or inside the polygon?
     * @param ignoreLineWidth = used for edge cuts where the line width is only for visualization
     */
    void TransformShapeWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer,
                                               PCB_LAYER_ID aLayer, int aClearanceValue,
                                               int aMaxError, ERROR_LOC aErrorLoc,
                                               bool ignoreLineWidth = false ) const override;

    /**
     * Function TransformHoleWithClearanceToPolygon
     * Build the Corner list of the polygonal drill shape in the board coordinate system.
     * @param aCornerBuffer = a buffer to fill.
     * @param aInflateValue = the clearance or margin value.
     * @param aError = maximum deviation of an arc from the polygon approximation
     * @param aErrorLoc = should the approximation error be placed outside or inside the polygon?
     * @return false if the pad has no hole, true otherwise
     */
    bool TransformHoleWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer, int aInflateValue,
                                              int aError, ERROR_LOC aErrorLoc ) const;

    // @copydoc BOARD_ITEM::GetEffectiveShape
    virtual std::shared_ptr<SHAPE> GetEffectiveShape( PCB_LAYER_ID aLayer = UNDEFINED_LAYER ) const override;

    const std::shared_ptr<SHAPE_POLY_SET>& GetEffectivePolygon( PCB_LAYER_ID = UNDEFINED_LAYER ) const;

    /**
     * Function GetEffectiveHoleShape
     * Returns a SHAPE object representing the pad's hole.
     */
    const SHAPE_SEGMENT* GetEffectiveHoleShape() const;

    /**
     * Function GetBoundingRadius
     * returns the radius of a minimum sized circle which fully encloses this pad.
     * The center is the pad position NOT THE SHAPE POS!
     */
    int GetBoundingRadius() const;

    /**
     * Function GetLocalClearanceOverrides
     * returns any local clearance overrides set in the "classic" (ie: pre-rule) system.
     * @param aSource [out] optionally reports the source as a user-readable string
     * @return int - the clearance in internal units.
     */
    int GetLocalClearanceOverrides( wxString* aSource ) const override;

   // Mask margins handling:

    /**
     * Function GetSolderMaskMargin
     * @return the margin for the solder mask layer
     * usually > 0 (mask shape bigger than pad)
     * For pads also on copper layers, the value (used to build a default shape) is
     * 1 - the local value
     * 2 - if 0, the parent footprint value
     * 3 - if 0, the global value
     * For pads NOT on copper layers, the value is the local value because there is no default
     * shape to build
     */
    int GetSolderMaskMargin() const;

    /**
     * Function GetSolderPasteMargin
     * @return the margin for the solder mask layer
     * usually < 0 (mask shape smaller than pad)
     * because the margin can be dependent on the pad size, the margin has a x and a y value
     *
     * For pads also on copper layers, the value (used to build a default shape) is
     * 1 - the local value
     * 2 - if 0, the parent footprint value
     * 3 - if 0, the global value
     *
     * For pads NOT on copper layers, the value is the local value because there is
     * no default shape to build
    */
    wxSize GetSolderPasteMargin() const;

    void SetZoneConnection( ZONE_CONNECTION aType ) { m_zoneConnection = aType; }
    ZONE_CONNECTION GetZoneConnection() const { return m_zoneConnection; }

    /**
     * Return the zone connection in effect (either locally overridden or overridden in the
     * parent footprint).
     * Optionally reports on the source of the property (pad, parent footprint or zone).
     */
    ZONE_CONNECTION GetEffectiveZoneConnection( wxString* aSource = nullptr ) const;

    /**
     * Set the width of the thermal spokes connecting the pad to a zone.  If != 0 this will
     * override similar settings in the parent footprint and zone.
     * @param aWidth
     */
    void SetThermalSpokeWidth( int aWidth ) { m_thermalWidth = aWidth; }
    int GetThermalSpokeWidth() const { return m_thermalWidth; }

    /**
     * Return the effective thermal spoke width having resolved any inheritance.
     */
    int GetEffectiveThermalSpokeWidth( wxString* aSource = nullptr ) const;

    void SetThermalGap( int aGap ) { m_thermalGap = aGap; }
    int GetThermalGap() const { return m_thermalGap; }

    /**
     * Return the effective thermal gap having resolved any inheritance.
     */
    int GetEffectiveThermalGap( wxString* aSource = nullptr ) const;

    /**
     * Function SetRoundRectCornerRadius
     * has meaning only for rounded rect pads
     * @return The radius of the rounded corners for this pad.
     */
    void SetRoundRectCornerRadius( double aRadius );
    int GetRoundRectCornerRadius() const;

    wxPoint ShapePos() const;

    /**
     * has meaning only for rounded rect pads
     * Set the ratio between the smaller X or Y size and the rounded corner radius.
     * Cannot be > 0.5; the normalized IPC-7351C value is 0.25
     */
    void SetRoundRectRadiusRatio( double aRadiusScale );
    double GetRoundRectRadiusRatio() const { return m_roundedCornerScale; }

    /**
     * has meaning only for chamfered rect pads
     * Set the ratio between the smaller X or Y size and chamfered corner size.
     * Cannot be < 0.5.
     */
    void SetChamferRectRatio( double aChamferScale );
    double GetChamferRectRatio() const { return m_chamferScale; }

    /**
     * has meaning only for chamfered rect pads
     * set the position of the chamfers for orientation 0.
     * @param aPositions a bit-set of RECT_CHAMFER_POSITIONS
     */
    void SetChamferPositions( int aPositions ) { m_chamferPositions = aPositions; }
    int GetChamferPositions() const { return m_chamferPositions; }

    /**
     * Function GetSubRatsnest
     * @return int - the netcode
     */
    int GetSubRatsnest() const                  { return m_subRatsnest; }
    void SetSubRatsnest( int aSubRatsnest )     { m_subRatsnest = aSubRatsnest; }

    /**
     * Sets the unconnected removal property.  If true, the copper is removed on zone fill
     * or when specifically requested when the pad is not connected on a layer.  This requires
     * that there be a through hole.
     */
    void SetRemoveUnconnected( bool aSet )      { m_removeUnconnectedLayer = aSet; }
    bool GetRemoveUnconnected() const           { return m_removeUnconnectedLayer; }

    /**
     * Sets whether we keep the top and bottom connections even if they are not connected
     */
    void SetKeepTopBottom( bool aSet )      { m_keepTopBottomLayer = aSet; }
    bool GetKeepTopBottom() const           { return m_keepTopBottomLayer; }

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    bool IsOnLayer( PCB_LAYER_ID aLayer ) const override
    {
        return m_layerMask[aLayer];
    }

    bool FlashLayer( int aLayer ) const;
    bool FlashLayer( LSET aLayers ) const;

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    wxString GetClass() const override
    {
        return wxT( "PAD" );
    }

    /**
     * Function GetBoundingBox
     * The bounding box is cached, so this will be efficient most of the time.
     */
    const EDA_RECT GetBoundingBox() const override;

    ///> Set absolute coordinates.
    void SetDrawCoord();

    //todo: Remove SetLocalCoord along with m_pos
    ///> Set relative coordinates.
    void SetLocalCoord();

    /**
     * Function Compare
     * compares two pads and return 0 if they are equal.
     * @return int - <0 if left less than right, 0 if equal, >0 if left greater than right.
     */
    static int Compare( const PAD* padref, const PAD* padcmp );

    void Move( const wxPoint& aMoveVector ) override
    {
        m_pos += aMoveVector;
        SetLocalCoord();
        m_shapesDirty = true;
    }

    void Rotate( const wxPoint& aRotCentre, double aAngle ) override;

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    /**
     * Function ShowPadShape
     * @return the GUI-appropriate name of the shape
     */
    wxString ShowPadShape() const;

    /**
     * Function ShowPadAttr
     * @return the GUI-appropriate description of the pad type (attribute) : Std, SMD ...
     */
    wxString ShowPadAttr() const;

    EDA_ITEM* Clone() const override;

    /**
     * same as Clone, but returns a PAD item.
     * Useful mainly for python scripts, because Clone returns an EDA_ITEM.
     */
    PAD* ClonePad() const
    {
        return (PAD*) Clone();
    }

    /**
     * A pad whose hole is the same size as the pad is a NPTH.  However, if the user
     * fails to mark this correctly then the pad will become invisible on the board.
     * This check allows us to special-case this error-condition.
     */
    bool PadShouldBeNPTH() const;

    /**
     * Rebuilds the effective shape cache (and bounding box and radius) for the pad and clears
     * the dirty bit.
     */
    void BuildEffectiveShapes( PCB_LAYER_ID aLayer ) const;

    virtual void ViewGetLayers( int aLayers[], int& aCount ) const override;

    double ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const override;

    virtual const BOX2I ViewBBox() const override;

    virtual void SwapData( BOARD_ITEM* aImage ) override;

#if defined(DEBUG)
    virtual void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif


private:
    void addPadPrimitivesToPolygon( SHAPE_POLY_SET* aMergedPolygon, PCB_LAYER_ID aLayer,
                                    int aError, ERROR_LOC aErrorLoc ) const;

private:
    wxString      m_name;               // Pad name (pin number in schematic)
    wxString      m_pinFunction;        // Pin function in schematic

    wxPoint       m_pos;                // Pad Position on board

    PAD_SHAPE_T   m_padShape;           // Shape: PAD_SHAPE_CIRCLE, PAD_SHAPE_RECT,
                                        //   PAD_SHAPE_OVAL, PAD_SHAPE_TRAPEZOID,
                                        //   PAD_SHAPE_ROUNDRECT, PAD_SHAPE_POLYGON
    /*
     * Editing definitions of primitives for custom pad shapes.  In local coordinates relative
     * to m_Pos (NOT shapePos) at orient 0.
     */
    std::vector<std::shared_ptr<PCB_SHAPE>> m_editPrimitives;

    // Must be set to true to force rebuild shapes to draw (after geometry change for instance)
    mutable bool                              m_shapesDirty;
    mutable int                               m_effectiveBoundingRadius;
    mutable EDA_RECT                          m_effectiveBoundingBox;
    mutable std::shared_ptr<SHAPE_COMPOUND>   m_effectiveShape;
    mutable std::shared_ptr<SHAPE_SEGMENT>    m_effectiveHoleShape;
    mutable std::shared_ptr<SHAPE_POLY_SET>   m_effectivePolygon;

    /*
     * How to build the custom shape in zone, to create the clearance area:
     * CUST_PAD_SHAPE_IN_ZONE_OUTLINE = use pad shape
     * CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL = use the convex hull of the pad shape
     */
    CUST_PAD_SHAPE_IN_ZONE  m_customShapeClearanceArea;

    int               m_subRatsnest;        // Variable used to handle subnet (block) number in
                                            //   ratsnest computations

    wxSize            m_drill;              // Drill diameter (x == y) or slot dimensions (x != y)
    wxSize            m_size;               // X and Y size (relative to orient 0)

    PAD_DRILL_SHAPE_T m_drillShape;         // PAD_DRILL_SHAPE_CIRCLE, PAD_DRILL_SHAPE_OBLONG

    double            m_roundedCornerScale; // Scaling factor of min(width, hieght) to corner
                                            //   radius, default 0.25
    double            m_chamferScale;       // Scaling factor of min(width, height) to chamfer
                                            //   size, default 0.25
    int               m_chamferPositions;   // The positions of the chamfers (at orient 0)

    PAD_SHAPE_T       m_anchorPadShape;     // For custom shaped pads: shape of pad anchor,
                                            //   PAD_SHAPE_RECT, PAD_SHAPE_CIRCLE

    /*
     * Most of the time the hole is the center of the shape (m_Offset = 0). But some designers
     * use oblong/rect pads with a hole moved to one of the oblong/rect pad shape ends.
     * In all cases the hole is at the pad position.  This offset is from the hole to the center
     * of the pad shape (ie: the copper area around the hole).
     * ShapePos() returns the board shape position according to the offset and the pad rotation.
     */
    wxPoint     m_offset;

    LSET        m_layerMask;        // Bitwise layer: 1 = copper layer, 15 = cmp,
                                    // 2..14 = internal layers, 16..31 = technical layers

    wxSize      m_deltaSize;        // Delta for PAD_SHAPE_TRAPEZOID; half the delta squeezes
                                    //   one end and half expands the other.  It is only valid
                                    //   to have a single axis be non-0.

    wxPoint     m_pos0;             // Initial Pad position (i.e. pad position relative to the
                                    //   footprint anchor, orientation 0)

    PAD_ATTR_T  m_attribute;        // PAD_ATTRIB_NORMAL, PAD_ATTRIB_SMD, PAD_ATTRIB_CONN,
                                    //   PAD_ATTRIB_NPTH
    PAD_PROP_T  m_property;         // Property in fab files (BGA, FIDUCIAL, TESTPOINT, etc.)

    double      m_orient;           // in 1/10 degrees

    int         m_lengthPadToDie;   // Length net from pad to die, inside the package

    bool        m_removeUnconnectedLayer;  // If true, the pad copper is removed for layers that are not connected
    bool        m_keepTopBottomLayer;      // When removing unconnected pads, keep the top and bottom pads

    /*
     * Pad clearances, margins, etc. exist in a hiearchy.  If a given level is specified then
     * the remaining levels are NOT consulted.
     *
     * LEVEL 1: (highest priority) local overrides (pad, footprint, etc.)
     * LEVEL 2: Rules
     * LEVEL 3: Accumulated local settings, netclass settings, & board design settings
     *
     * These are the LEVEL 1 settings for a pad.
     */
    int         m_localClearance;
    int         m_localSolderMaskMargin;        // Local solder mask margin
    int         m_localSolderPasteMargin;       // Local solder paste margin absolute value
    double      m_localSolderPasteMarginRatio;  // Local solder mask margin ratio of pad size
                                                // The final margin is the sum of these 2 values

    ZONE_CONNECTION m_zoneConnection;           // No connection, thermal relief, etc.
    int         m_thermalWidth;                 // Thermal spoke width.
    int         m_thermalGap;
};

#endif  // PAD_H
