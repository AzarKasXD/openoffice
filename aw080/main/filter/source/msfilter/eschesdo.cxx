/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_filter.hxx"
#include "eschesdo.hxx"
#include <svx/svdobj.hxx>
#include <svx/unoapi.hxx>
#include <svx/svdoashp.hxx>
#include <svx/unoshape.hxx>
#include <vcl/outdev.hxx>
#include <tools/poly.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/graph.hxx>
#include <tools/debug.hxx>
#include <svx/fmdpage.hxx>
#include <toolkit/unohlp.hxx>
#include <com/sun/star/style/VerticalAlignment.hpp>
#include <com/sun/star/awt/Gradient.hpp>
#include <com/sun/star/drawing/PointSequence.hpp>
#include <com/sun/star/drawing/PointSequenceSequence.hpp>
#include <com/sun/star/drawing/PolyPolygonBezierCoords.hpp>
#include <com/sun/star/drawing/FlagSequence.hpp>
#include <com/sun/star/drawing/TextAdjust.hpp>
#include <com/sun/star/drawing/LineDash.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/drawing/CircleKind.hpp>
#include <com/sun/star/drawing/FillStyle.hpp>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <comphelper/extract.hxx>
#include <svtools/fltcall.hxx>
#include <vcl/cvtgrf.hxx>
#include <svx/svdlegacy.hxx>
#include <vcl/svapp.hxx>
#include <svx/svdocirc.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>

using ::rtl::OUString;
using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::task;
using namespace ::com::sun::star::style;

#define EES_MAP_FRACTION 1440	// 1440 dpi

// ===================================================================

ImplEESdrWriter::ImplEESdrWriter( EscherEx& rEx )
		:
		mpEscherEx				( &rEx ),
		maMapModeSrc			( MAP_100TH_MM ),
		// PowerPoint: 576 dpi, WinWord: 1440 dpi, Excel: 1440 dpi
		maMapModeDest( MAP_INCH, Point(), Fraction( 1, EES_MAP_FRACTION ), Fraction( 1, EES_MAP_FRACTION ) ),
//		mXStatusIndicator		( rXStatInd ),
		maLogicToLogic			( ),
		mpPicStrm				( NULL ),
		mpHostAppData			( NULL ),
		mnPagesWritten			( 0 ),
		mnShapeMasterTitle		( 0 ),
		mnShapeMasterBody		( 0 ),
		mbStatusIndicator		( sal_False ),
		mbStatus				( sal_False )
{
}


// -------------------------------------------------------------------

void ImplEESdrWriter::implPrepareLogicToLogic()
{
	if(maLogicToLogic.isIdentity())
	{
		maLogicToLogic = Application::GetDefaultDevice()->GetViewTransformation(maMapModeSrc);
		maLogicToLogic.invert();
		maLogicToLogic = Application::GetDefaultDevice()->GetViewTransformation(maMapModeDest) * maLogicToLogic;
	}
}

// -------------------------------------------------------------------

basegfx::B2DPoint ImplEESdrWriter::ImplMapB2DPoint( const basegfx::B2DPoint& rB2DPoint )
{
	if(maMapModeSrc == maMapModeDest)
	{
		return rB2DPoint;
	}
	else
	{
		implPrepareLogicToLogic();

		return maLogicToLogic * rB2DPoint;
	}
}

// -------------------------------------------------------------------

basegfx::B2DVector ImplEESdrWriter::ImplMapB2DVector( const basegfx::B2DVector& rB2DVector )
{
	if(maMapModeSrc == maMapModeDest)
	{
		return rB2DVector;
	}
	else
{
		implPrepareLogicToLogic();

		return maLogicToLogic * rB2DVector;
	}
}

// -------------------------------------------------------------------

void ImplEESdrWriter::ImplFlipBoundingBox( ImplEESdrObject& rObj, EscherPropertyContainer& rPropOpt )
{
	sal_Int32 nAngle = rObj.GetAngle();

	if ( nAngle < 0 )
		nAngle = ( 36000 + nAngle ) % 36000;
	else
		nAngle = ( 36000 - ( nAngle % 36000 ) );

	double fVal = (double)nAngle * F_PI18000;
	double	fCos = cos( fVal );
	double	fSin = sin( fVal );

    basegfx::B2DPoint aPoint(rObj.getObjectPosition());
    basegfx::B2DVector aScale(rObj.getObjectScale());
	const basegfx::B2DVector aHalfScale(aScale * 0.5);
	const double nXDiff(fCos * aHalfScale.getX() + fSin * (-aHalfScale.getY()));
	const double nYDiff(-( fSin * aHalfScale.getX() - fCos * ( -aHalfScale.getY())));

    aPoint -= basegfx::B2DPoint(aHalfScale.getX() - nXDiff, aHalfScale.getY() + nYDiff);

	nAngle *= 655;
	nAngle += 0x8000;
	nAngle &=~0xffff;									// nAngle auf volle Gradzahl runden
	rPropOpt.AddOpt( ESCHER_Prop_Rotation, nAngle );

	rObj.SetAngle( nAngle );
	rObj.setObjectPositionAndScale(aPoint, aScale);
}

//	-----------------------------------------------------------------------

#define ADD_SHAPE( nType, nFlags )								\
{																\
	nShapeType = nType;											\
    nShapeID = mpEscherEx->GenerateShapeId();                   \
	rObj.SetShapeId( nShapeID );								\
	mpEscherEx->AddShape( (sal_uInt32)nType, (sal_uInt32)nFlags, nShapeID );	\
	rSolverContainer.AddShape( rObj.GetShapeRef(), nShapeID );	\
}

#define SHAPE_TEXT( bFill )											\
{																	\
	mpEscherEx->OpenContainer( ESCHER_SpContainer );				\
	ADD_SHAPE( ESCHER_ShpInst_TextBox, 0xa00 );						\
	if ( bFill )													\
		aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );	\
	if( rObj.ImplGetText() )										\
		aPropOpt.CreateTextProperties( rObj.mXPropSet,				\
			mpEscherEx->QueryTextID( rObj.GetShapeRef(),			\
				rObj.GetShapeId() ) );								\
}

//Map from twips to export units, generally twips as well, only excel and word
//export is happening here, so native units are export units, leave as
//placeholder if required in future
void ImplEESdrWriter::MapRect(ImplEESdrObject& /* rObj */ )
{
}

sal_uInt32 ImplEESdrWriter::ImplWriteShape( ImplEESdrObject& rObj,
								EscherSolverContainer& rSolverContainer,
								ImplEESdrPageType ePageType )
{
	sal_uInt32 nShapeID = 0;
	sal_uInt16 nShapeType = 0;
	sal_Bool bDontWriteText = sal_False;		// if a metafile is written as shape replacement, then the text is already part of the metafile
	sal_Bool bAdditionalText = sal_False;
	sal_uInt32 nGrpShapeID = 0;

	do {
        const bool bHandOverPosAndSize(mpEscherEx->GetGroupLevel() > 1);
        mpHostAppData = mpEscherEx->StartShape( rObj.GetShapeRef(), bHandOverPosAndSize ? &rObj.getObjectPosition() : 0, bHandOverPosAndSize ? &rObj.getObjectScale() : 0 );
		if ( mpHostAppData && mpHostAppData->DontWriteShape() )
			break;

        // #i51348# get shape name
        String aShapeName;
        if( const SdrObject* pSdrObj = rObj.GetSdrObject() )
            if( pSdrObj->GetName().Len() > 0 )
                aShapeName = pSdrObj->GetName();

		Point aTextRefPoint;

		if( rObj.GetType().EqualsAscii( "drawing.Group" ))
		{
			Reference< XIndexAccess > xXIndexAccess( rObj.GetShapeRef(), UNO_QUERY );

			if( xXIndexAccess.is() && 0 != xXIndexAccess->getCount() )
			{
                nShapeID = mpEscherEx->EnterGroup( aShapeName, &rObj.getObjectPosition(), &rObj.getObjectScale() );
				nShapeType = ESCHER_ShpInst_Min;

				for( sal_uInt32 n = 0, nCnt = xXIndexAccess->getCount();
						n < nCnt; ++n )
				{
					ImplEESdrObject aObj( *this, *(Reference< XShape >*)
									xXIndexAccess->getByIndex( n ).getValue() );
					if( aObj.IsValid() )
						ImplWriteShape( aObj, rSolverContainer, ePageType );
				}
				mpEscherEx->LeaveGroup();
			}
			break;
		}
		rObj.SetAngle( rObj.ImplGetInt32PropertyValue( ::rtl::OUString::createFromAscii("RotateAngle") ));

		if( ( rObj.ImplGetPropertyValue( ::rtl::OUString::createFromAscii("IsFontwork") ) &&
			::cppu::any2bool( rObj.GetUsrAny() ) ) ||
			rObj.GetType().EqualsAscii( "drawing.Measure" ) || rObj.GetType().EqualsAscii( "drawing.Caption" ) )
		{
/*
			if( rObj.ImplGetPropertyValue( ::rtl::OUString::createFromAscii("BoundRect") ) )
			{
				::com::sun::star::awt::Rectangle aRect( *(::com::sun::star::awt::Rectangle*)rObj.GetUsrAny().getValue() );
				rObj.SetRange( ImplMapB2DPoint( Point( aRect.X, aRect.Y ) ),
								ImplMapB2DVector( Size( aRect.Width, aRect.Height ) ) );
			}
*/
			rObj.SetType( String( RTL_CONSTASCII_STRINGPARAM(
								"drawing.dontknow" ),
								RTL_TEXTENCODING_MS_1252 ));
		}

        // TTTT: adapted to transformation
        basegfx::B2DPoint maObjectPosition(0.0, 0.0);
        basegfx::B2DVector maObjectScale(1.0, 1.0);

		if(rObj.ImplGetPropertyValue(::rtl::OUString::createFromAscii("Transformation")))
        {
		    drawing::HomogenMatrix3 aMatrix;
            rObj.GetUsrAny() >>= aMatrix;
            const basegfx::tools::B2DHomMatrixBufferedDecompose aMat(basegfx::tools::UnoHomogenMatrix3ToB2DHomMatrix(aMatrix));
            maObjectPosition = aMat.getTranslate();
            maObjectScale = aMat.getScale();
        }

        if ( !mpPicStrm )
            mpPicStrm = mpEscherEx->QueryPictureStream();
        
        EscherPropertyContainer aPropOpt( mpEscherEx->GetGraphicProvider(), mpPicStrm, maObjectPosition, maObjectScale );

        // #i51348# shape name
        if( aShapeName.Len() > 0 )
            aPropOpt.AddOpt( ESCHER_Prop_wzName, aShapeName );

		if ( rObj.GetType().EqualsAscii( "drawing.Custom" ) )
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			sal_uInt32 nMirrorFlags;

			rtl::OUString sCustomShapeType;
			MSO_SPT eShapeType = aPropOpt.GetCustomShapeType( rObj.GetShapeRef(), nMirrorFlags, sCustomShapeType );
			if ( sCustomShapeType.equalsAscii( "col-502ad400" ) || sCustomShapeType.equalsAscii( "col-60da8460" ) )
			{
				ADD_SHAPE( ESCHER_ShpInst_PictureFrame, 0xa00 );
				if ( aPropOpt.CreateGraphicProperties( rObj.mXPropSet, String( RTL_CONSTASCII_USTRINGPARAM( "MetaFile" ) ), sal_False ) )
				{
					aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x800080 );
					aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x100000 );		// no fill
					aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x90000 );		// no linestyle
						SdrObject* pObj = GetSdrObjectFromXShape( rObj.GetShapeRef() );
					if ( pObj )
					{
						const basegfx::B2DRange aBound(pObj->getObjectRange(0));
						const basegfx::B2DPoint aPosition(ImplMapB2DPoint(aBound.getMinimum()));
						const basegfx::B2DVector aScale(ImplMapB2DVector(aBound.getRange()));
						
						rObj.setObjectPositionAndScale(aPosition, aScale);
						rObj.SetAngle( 0 );
						bDontWriteText = sal_True;
					}
				}
			}
			else
			{
				ADD_SHAPE(
                    sal::static_int_cast< sal_uInt16 >(eShapeType),
                    nMirrorFlags | 0xa00 );
				aPropOpt.CreateCustomShapeProperties( eShapeType, rObj.GetShapeRef() );
				aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );
				if ( rObj.ImplGetText() )
				{
					if ( !aPropOpt.IsFontWork() )
						aPropOpt.CreateTextProperties( rObj.mXPropSet, mpEscherEx->QueryTextID(
							rObj.GetShapeRef(),	rObj.GetShapeId() ), sal_True, sal_False );
				}
			}
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Rectangle" ))
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			sal_Int32 nRadius = (sal_Int32)rObj.ImplGetInt32PropertyValue(
											::rtl::OUString::createFromAscii("CornerRadius"));
			if( nRadius )
			{
				nRadius = ImplMapB2DVector(basegfx::B2DVector(nRadius, 0.0)).getLength();
				ADD_SHAPE( ESCHER_ShpInst_RoundRectangle, 0xa00 );	// Flags: Connector | HasSpt
				const sal_Int32 nLenght(basegfx::fround((std::min(rObj.getObjectScale().getX(), rObj.getObjectScale().getY())) * 0.5));
				if ( nRadius >= nLenght )
					nRadius = 0x2a30;							// 0x2a30 ist PPTs maximum radius
				else
					nRadius = ( 0x2a30 * nRadius ) / nLenght;
				aPropOpt.AddOpt( ESCHER_Prop_adjustValue, nRadius );
			}
			else
			{
				ADD_SHAPE( ESCHER_ShpInst_Rectangle, 0xa00 );			// Flags: Connector | HasSpt
			}
			aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );
			if( rObj.ImplGetText() )
				aPropOpt.CreateTextProperties( rObj.mXPropSet,
					mpEscherEx->QueryTextID( rObj.GetShapeRef(),
						rObj.GetShapeId() ), sal_False, sal_False );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Ellipse" ))
		{
            // changed to directly use the SdrObject, using the handmade creatin of the polygon data
            // seemed too dangerous for the moment. Can be changed back to own geometry creation, of 
            // course (e.g. by getting primitives using UNO API and filtering for hairlines)
            SdrCircObj* pCircObj = dynamic_cast< SdrCircObj* >(GetSdrObjectFromXShape(rObj.GetShapeRef()));

            if(!pCircObj)
                break;

            if(CircleType_Circle == pCircObj->GetSdrCircleObjType())
            {
				mpEscherEx->OpenContainer( ESCHER_SpContainer );
				ADD_SHAPE( ESCHER_ShpInst_Ellipse, 0xa00 );			// Flags: Connector | HasSpt
				aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );;
            }
			else
			{
                const basegfx::B2DPolyPolygon aOutline(pCircObj->TakeXorPoly());

                if(!aOutline.count())
                    break;

                Polygon aPolygon(aOutline.getB2DPolygon(0));
                
				if( rObj.GetAngle() )
				{
                    // already applied to the polygon, nothing to do there
					rObj.SetAngle( 0 );
				}

                mpEscherEx->OpenContainer( ESCHER_SpContainer );
				ADD_SHAPE( ESCHER_ShpInst_NotPrimitive, 0xa00 );		// Flags: Connector | HasSpt
				::com::sun::star::awt::Rectangle aNewRect;

                if(aOutline.isClosed())
                {
					aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYPOLYGON, sal_False, aNewRect, &aPolygon );
					aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True  );
                }
                else
                {
					aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYLINE, sal_False, aNewRect, &aPolygon );
					aPropOpt.CreateLineProperties( rObj.mXPropSet, sal_False );
                }
				
				const basegfx::B2DPoint aPosition(ImplMapB2DPoint(basegfx::B2DPoint(aNewRect.X, aNewRect.Y)));
				const basegfx::B2DVector aScale(ImplMapB2DVector(basegfx::B2DVector(aNewRect.Width, aNewRect.Height)));
				
				rObj.setObjectPositionAndScale(aPosition, aScale);
			}
			if ( rObj.ImplGetText() )
				aPropOpt.CreateTextProperties( rObj.mXPropSet,
					mpEscherEx->QueryTextID( rObj.GetShapeRef(),
						rObj.GetShapeId() ), sal_False, sal_False );

		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Control" ))
		{
			break;
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Connector" ))
		{
			sal_uInt16 nSpType, nSpFlags;
			::com::sun::star::awt::Rectangle aNewRect;
			if ( aPropOpt.CreateConnectorProperties( rObj.GetShapeRef(),
							rSolverContainer, aNewRect, nSpType, nSpFlags ) == sal_False )
				break;

			const basegfx::B2DPoint aPosition(ImplMapB2DPoint(basegfx::B2DPoint(aNewRect.X, aNewRect.Y)));
			const basegfx::B2DVector aScale(ImplMapB2DVector(basegfx::B2DVector(aNewRect.Width, aNewRect.Height)));
				
			rObj.setObjectPositionAndScale(aPosition, aScale);
            mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( nSpType, nSpFlags );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Measure" ))
		{
/*
			if ( ImplGetPropertyValue( L"MeasureKind" ) )
			{
				mpEscherEx->EnterGroup( &maRect );
				mpEscherEx->OpenContainer( ESCHER_SpContainer );
				ImplWriteAny( ANY_FLAGS_LINE, sal_False );
				sal_uInt32 nFlags = 0xa00;											// Flags: Connector | HasSpt
				if ( maRect.Top() > maRect.Bottom() )
					nFlags |= 0x80;												// Flags: VertMirror
				if ( maRect.Left() > maRect.Right() )
					nFlags |= 0x40;												// Flags: HorzMirror

				ADD_SHAPE( ESCHER_ShpInst_Line, nFlags );
				aPropOpt.AddOpt( ESCHER_Prop_shapePath, ESCHER_ShapeComplex );
				aPropOpt.CreateLineProperties( rObj.mXPropSet, sal_False );
				mpEscherEx->EndCount( ESCHER_OPT, 3 );
				maRect.Justify();
				mpEscherEx->AddClientAnchor( maRect );
				mpEscherEx->CloseContainer();			// ESCHER_SpContainer

				if ( ImplGetPropertyValue( L"MeasureTextHorizontalPosition" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextVerticalPosition" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureLineDistance" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureHelpLineOverhang" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureHelpLineDistance" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureHelpLine1Length" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureHelpLine2Length" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureBelowReferenceEdge" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextRotate90" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextUpsideDown" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureOverhang" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureUnit" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureScale" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureShowUnit" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureFormatString" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextAutoAngle" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextAutoAngleView" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextIsFixedAngle" ) )
				{
				}
				if ( ImplGetPropertyValue( L"MeasureTextFixedAngle" ) )
				{
				}
				mpEscherEx->LeaveGroup();
			}
*/
			break;
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Line" ))
		{
			::com::sun::star::awt::Rectangle aNewRect;
			aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_LINE, sal_False, aNewRect, NULL );
            MapRect(rObj);
			//i27942: Poly/Lines/Bezier do not support text.

			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			sal_uInt32 nFlags = 0xa00;			// Flags: Connector | HasSpt
            if( aNewRect.Height < 0 )
				nFlags |= 0x80;	  			// Flags: VertMirror
			if( aNewRect.Width < 0 )
				nFlags |= 0x40;				// Flags: HorzMirror

			ADD_SHAPE( ESCHER_ShpInst_Line, nFlags );
			aPropOpt.AddOpt( ESCHER_Prop_shapePath, ESCHER_ShapeComplex );
			aPropOpt.CreateLineProperties( rObj.mXPropSet, sal_False );
			rObj.SetAngle( 0 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.PolyPolygon" ))
		{
			if( rObj.ImplHasText() )
			{
				nGrpShapeID = ImplEnterAdditionalTextGroup(	rObj.GetShapeRef(), &rObj.getObjectPosition(), &rObj.getObjectScale() );
				bAdditionalText = sal_True;
			}
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_NotPrimitive, 0xa00 );		// Flags: Connector | HasSpt
			::com::sun::star::awt::Rectangle aNewRect;
			aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYPOLYGON, sal_False, aNewRect, NULL );
            MapRect(rObj);
			aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );
			rObj.SetAngle( 0 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.PolyLine" ))
		{
			//i27942: Poly/Lines/Bezier do not support text.

			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_NotPrimitive, 0xa00 );		// Flags: Connector | HasSpt
			::com::sun::star::awt::Rectangle aNewRect;
			aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYLINE, sal_False, aNewRect, NULL );
            MapRect(rObj);
			aPropOpt.CreateLineProperties( rObj.mXPropSet, sal_False );
			rObj.SetAngle( 0 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.OpenBezier" ) )
		{
			//i27942: Poly/Lines/Bezier do not support text.

			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_NotPrimitive, 0xa00 );		// Flags: Connector | HasSpt
			::com::sun::star::awt::Rectangle aNewRect;
			aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYLINE, sal_True, aNewRect, NULL );
            MapRect(rObj);
			aPropOpt.CreateLineProperties( rObj.mXPropSet, sal_False );
			rObj.SetAngle( 0 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.ClosedBezier" ) )
		{
			if ( rObj.ImplHasText() )
			{
				nGrpShapeID = ImplEnterAdditionalTextGroup(	rObj.GetShapeRef(), &rObj.getObjectPosition(), &rObj.getObjectScale() );
				bAdditionalText = sal_True;
			}
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_NotPrimitive, 0xa00 );		// Flags: Connector | HasSpt
			::com::sun::star::awt::Rectangle aNewRect;
			aPropOpt.CreatePolygonProperties( rObj.mXPropSet, ESCHER_CREATEPOLYGON_POLYPOLYGON, sal_True, aNewRect, NULL );
            MapRect(rObj);
			aPropOpt.CreateFillProperties( rObj.mXPropSet, sal_True );
			rObj.SetAngle( 0 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.GraphicObject" ))
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );

			// ein GraphicObject kann auch ein ClickMe Element sein
			if( rObj.IsEmptyPresObj() && ( ePageType == NORMAL ) )
			{
				ADD_SHAPE( ESCHER_ShpInst_Rectangle, 0x220 );				// Flags: HaveAnchor | HaveMaster
				sal_uInt32 nTxtBxId = mpEscherEx->QueryTextID( rObj.GetShapeRef(),
														rObj.GetShapeId() );
				aPropOpt.AddOpt( ESCHER_Prop_lTxid, nTxtBxId );
				aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x10001 );
				aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x10001 );
				aPropOpt.AddOpt( ESCHER_Prop_hspMaster, mnShapeMasterBody );
			}
			else
			{
				if( rObj.ImplGetText() )
				{
					/* SJ #i34951#: because M. documents are not allowing GraphicObjects containing text, we
					   have to create a simpe Rectangle with fill bitmap instead (while not allowing BitmapMode_Repeat).
					*/
					ADD_SHAPE( ESCHER_ShpInst_Rectangle, 0xa00 );			// Flags: Connector | HasSpt
					if ( aPropOpt.CreateGraphicProperties( rObj.mXPropSet, String( RTL_CONSTASCII_USTRINGPARAM( "GraphicURL" ) ), sal_True,  sal_True, sal_False ) )
					{
						aPropOpt.AddOpt( ESCHER_Prop_WrapText, ESCHER_WrapNone );
						aPropOpt.AddOpt( ESCHER_Prop_AnchorText, ESCHER_AnchorMiddle );
						aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x140014 );
						aPropOpt.AddOpt( ESCHER_Prop_fillBackColor, 0x8000000 );
						aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x80000 );
						if ( rObj.ImplGetText() )
							aPropOpt.CreateTextProperties( rObj.mXPropSet,
								mpEscherEx->QueryTextID( rObj.GetShapeRef(),
									rObj.GetShapeId() ), sal_False, sal_False );
					}
				}
				else
				{
					ADD_SHAPE( ESCHER_ShpInst_PictureFrame, 0xa00 );
					if ( aPropOpt.CreateGraphicProperties( rObj.mXPropSet, String( RTL_CONSTASCII_USTRINGPARAM( "GraphicURL" ) ), sal_False, sal_True ) )
						aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x800080 );
				}
			}
		}
		else if ( rObj.GetType().EqualsAscii(  "drawing.Text" ))
		{
			SHAPE_TEXT( sal_True );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Page" ))
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_Rectangle, 0xa00 );
			aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x40004 );
			aPropOpt.AddOpt( ESCHER_Prop_fFillOK, 0x100001 );
			aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x110011 );
			aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x90008 );
			aPropOpt.AddOpt( ESCHER_Prop_fshadowObscured, 0x10001 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.Frame" ))
		{
			break;
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.OLE2" ))
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			if( rObj.IsEmptyPresObj() && ( ePageType == NORMAL ) )
			{
				ADD_SHAPE( ESCHER_ShpInst_Rectangle, 0x220 );				// Flags: HaveAnchor | HaveMaster
				sal_uInt32 nTxtBxId = mpEscherEx->QueryTextID( rObj.GetShapeRef(),
														rObj.GetShapeId() );
				aPropOpt.AddOpt( ESCHER_Prop_lTxid, nTxtBxId );
				aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x10001 );
				aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x10001 );
				aPropOpt.AddOpt( ESCHER_Prop_hspMaster, mnShapeMasterBody );
			}
			else
			{
				//2do: could be made an option in HostAppData whether OLE object should be written or not
				sal_Bool bAppOLE = sal_True;
				ADD_SHAPE( ESCHER_ShpInst_PictureFrame,
					0xa00 | (bAppOLE ? SHAPEFLAG_OLESHAPE : 0) );
				if ( aPropOpt.CreateOLEGraphicProperties( rObj.GetShapeRef() ) )
				{
					if ( bAppOLE )
					{	// snooped from Xcl hex dump, nobody knows the trouble I have seen
						aPropOpt.AddOpt( ESCHER_Prop_FitTextToShape,	0x00080008 );
						aPropOpt.AddOpt( ESCHER_Prop_pictureId,		0x00000001 );
						aPropOpt.AddOpt( ESCHER_Prop_fillColor,		0x08000041 );
						aPropOpt.AddOpt( ESCHER_Prop_fillBackColor,	0x08000041 );
						aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest,	0x00110010 );
						aPropOpt.AddOpt( ESCHER_Prop_lineColor,		0x08000040 );
						aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash,0x00080008 );
//						aPropOpt.AddOpt( ESCHER_Prop_fshadowObscured,0x00020000 );
						aPropOpt.AddOpt( ESCHER_Prop_fPrint,			0x00080000 );
					}
					aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x800080 );
				}
			}
		}
		else if( '3' == rObj.GetType().GetChar(8 ) &&
				 'D' == rObj.GetType().GetChar( 9 ) )	// drawing.3D
		{
			// SceneObject, CubeObject, SphereObject, LatheObject, ExtrudeObject, PolygonObject
			if ( !rObj.ImplGetPropertyValue( ::rtl::OUString::createFromAscii("Bitmap") ) )
				break;

			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_PictureFrame, 0xa00 );

                if ( aPropOpt.CreateGraphicProperties( rObj.mXPropSet, String( RTL_CONSTASCII_USTRINGPARAM( "Bitmap" ) ), sal_False ) )
				aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x800080 );
		}
		else if ( rObj.GetType().EqualsAscii( "drawing.dontknow" ))
		{
			rObj.SetAngle( 0 );
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			ADD_SHAPE( ESCHER_ShpInst_PictureFrame, 0xa00 );
			if ( aPropOpt.CreateGraphicProperties( rObj.mXPropSet, String( RTL_CONSTASCII_USTRINGPARAM( "MetaFile" ) ), sal_False ) )
				aPropOpt.AddOpt( ESCHER_Prop_LockAgainstGrouping, 0x800080 );
		}
		else
		{
			break;
		}
		aPropOpt.CreateShadowProperties( rObj.mXPropSet );

		if( USHRT_MAX != mpEscherEx->GetHellLayerId() &&
			rObj.ImplGetPropertyValue( ::rtl::OUString::createFromAscii("LayerID") ) &&
			(*((sal_uInt16*)rObj.GetUsrAny().getValue()) ) == mpEscherEx->GetHellLayerId() )
		{
			aPropOpt.AddOpt( ESCHER_Prop_fPrint, 0x200020 );
		}

		if( rObj.GetAngle() )
			ImplFlipBoundingBox( rObj, aPropOpt );

		aPropOpt.CreateShapeProperties( rObj.GetShapeRef() );
		mpEscherEx->Commit( aPropOpt, rObj.getObjectPosition(), rObj.getObjectScale() );
		if( mpEscherEx->GetGroupLevel() > 1 )
            mpEscherEx->AddChildAnchor( rObj.getObjectPosition(), rObj.getObjectScale() );

		if ( mpHostAppData )
		{	//! with AdditionalText the App has to control whether these are written or not
			mpHostAppData->WriteClientAnchor( *mpEscherEx, rObj.getObjectPosition(), rObj.getObjectScale() );
			mpHostAppData->WriteClientData( *mpEscherEx );
			if ( !bDontWriteText )
				mpHostAppData->WriteClientTextbox( *mpEscherEx );
		}
		mpEscherEx->CloseContainer();		// ESCHER_SpContainer

		if( bAdditionalText )
		{
			mpEscherEx->EndShape( nShapeType, nShapeID );
			ImplWriteAdditionalText( rObj, aTextRefPoint );
		}

	} while ( 0 );

	if ( bAdditionalText )
		mpEscherEx->EndShape( ESCHER_ShpInst_Min, nGrpShapeID );
	else
		mpEscherEx->EndShape( nShapeType, nShapeID );
	return nShapeID;
}

void ImplEESdrWriter::ImplWriteAdditionalText( ImplEESdrObject& rObj,
												const Point& rTextRefPoint )
{
	sal_uInt32 nShapeID = 0;
	sal_uInt16 nShapeType = 0;
	do
	{
        const bool bHandOver(mpEscherEx->GetGroupLevel() > 1);
        mpHostAppData = mpEscherEx->StartShape( rObj.GetShapeRef(), bHandOver ? &rObj.getObjectPosition() : 0, bHandOver ? &rObj.getObjectScale() : 0 );
		if ( mpHostAppData && mpHostAppData->DontWriteShape() )
			break;

        // TTTT: adapted to transformation
        basegfx::B2DPoint maObjectPosition(0.0, 0.0);
        basegfx::B2DVector maObjectScale(1.0, 1.0);

		if(rObj.ImplGetPropertyValue(::rtl::OUString::createFromAscii("Transformation")))
        {
		    drawing::HomogenMatrix3 aMatrix;
            rObj.GetUsrAny() >>= aMatrix;
            const basegfx::tools::B2DHomMatrixBufferedDecompose aMat(basegfx::tools::UnoHomogenMatrix3ToB2DHomMatrix(aMatrix));
            maObjectPosition = aMat.getTranslate();
            maObjectScale = aMat.getScale();
        }
		
        if ( !mpPicStrm )
            mpPicStrm = mpEscherEx->QueryPictureStream();
        
        EscherPropertyContainer aPropOpt( mpEscherEx->GetGraphicProvider(), mpPicStrm, maObjectPosition, maObjectScale );

        rObj.SetAngle( rObj.ImplGetInt32PropertyValue( ::rtl::OUString::createFromAscii("RotateAngle")));
		sal_Int32 nAngle = rObj.GetAngle();
		if( rObj.GetType().EqualsAscii( "drawing.Line" ))
		{
//2do: this does not work right
			const double fDist(hypot(rObj.getObjectScale().getX(), rObj.getObjectScale().getY()));
            rObj.setObjectPositionAndScale(basegfx::B2DPoint(rTextRefPoint.X(), rTextRefPoint.Y()), basegfx::B2DVector(fDist, -1));

			mpEscherEx->OpenContainer( ESCHER_SpContainer );
			mpEscherEx->AddShape( ESCHER_ShpInst_TextBox, 0xa00 );
			if ( rObj.ImplGetText() )
				aPropOpt.CreateTextProperties( rObj.mXPropSet,
					mpEscherEx->QueryTextID( rObj.GetShapeRef(),
						rObj.GetShapeId() ) );

			aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x90000 );
			aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x100000 );
			aPropOpt.AddOpt( ESCHER_Prop_FitTextToShape, 0x60006 );		// Size Shape To Fit Text
			if ( nAngle < 0 )
				nAngle = ( 36000 + nAngle ) % 36000;
			if ( nAngle )
				ImplFlipBoundingBox( rObj, aPropOpt );
		}
		else
		{
			mpEscherEx->OpenContainer( ESCHER_SpContainer );
            nShapeID = mpEscherEx->GenerateShapeId();
			mpEscherEx->AddShape( nShapeType = ESCHER_ShpInst_TextBox, 0xa00, nShapeID );
			if ( rObj.ImplGetText() )
				aPropOpt.CreateTextProperties( rObj.mXPropSet,
					mpEscherEx->QueryTextID( rObj.GetShapeRef(),
						rObj.GetShapeId() ) );
			aPropOpt.AddOpt( ESCHER_Prop_fNoLineDrawDash, 0x90000 );
			aPropOpt.AddOpt( ESCHER_Prop_fNoFillHitTest, 0x100000 );

			if( nAngle < 0 )
				nAngle = ( 36000 + nAngle ) % 36000;
			else
				nAngle = ( 36000 - ( nAngle % 36000 ) );

			nAngle *= 655;
			nAngle += 0x8000;
			nAngle &=~0xffff;	// nAngle auf volle Gradzahl runden
			aPropOpt.AddOpt( ESCHER_Prop_Rotation, nAngle );
			mpEscherEx->SetGroupSnapPositionAndScale( mpEscherEx->GetGroupLevel(), rObj.getObjectPosition(), rObj.getObjectScale() );
			mpEscherEx->SetGroupLogicPositionAndScale( mpEscherEx->GetGroupLevel(), rObj.getObjectPosition(), rObj.getObjectScale() );
		}
		rObj.SetAngle( nAngle );
		aPropOpt.CreateShapeProperties( rObj.GetShapeRef() );
		mpEscherEx->Commit( aPropOpt, rObj.getObjectPosition(), rObj.getObjectScale() );

		// write the childanchor
        mpEscherEx->AddChildAnchor( rObj.getObjectPosition(), rObj.getObjectScale() );

#if defined EES_WRITE_EPP
		// ClientAnchor
		mpEscherEx->AddClientAnchor( rObj.getObjectPosition(), rObj.getObjectScale() );
		// ClientTextbox
		mpEscherEx->OpenContainer( ESCHER_ClientTextbox );
		mpEscherEx->AddAtom( 4, EPP_TextHeaderAtom );
		*mpStrm << (sal_uInt32)EPP_TEXTTYPE_Other;								// Text in a Shape
		ImplWriteTextStyleAtom();
		mpEscherEx->CloseContainer();	// ESCHER_ClientTextBox
#else // !EES_WRITE_EPP
		if ( mpHostAppData )
		{	//! the App has to control whether these are written or not
			mpHostAppData->WriteClientAnchor( *mpEscherEx, rObj.getObjectPosition(), rObj.getObjectScale() );
			mpHostAppData->WriteClientData( *mpEscherEx );
			mpHostAppData->WriteClientTextbox( *mpEscherEx );
		}
#endif // EES_WRITE_EPP
		mpEscherEx->CloseContainer();	// ESCHER_SpContainer
	} while ( 0 );
	mpEscherEx->LeaveGroup();
	mpEscherEx->EndShape( nShapeType, nShapeID );
}


// -------------------------------------------------------------------

sal_uInt32 ImplEESdrWriter::ImplEnterAdditionalTextGroup( 
    const Reference< XShape >& rShape,
	const basegfx::B2DPoint* pObjectPosition,
    const basegfx::B2DVector* pObjectScale)
{
	mpHostAppData = mpEscherEx->EnterAdditionalTextGroup();
	sal_uInt32 nGrpId = mpEscherEx->EnterGroup( pObjectPosition, pObjectScale );
    mpHostAppData = mpEscherEx->StartShape( rShape, pObjectPosition, pObjectScale );
	return nGrpId;
}


// -------------------------------------------------------------------

sal_Bool ImplEESdrWriter::ImplInitPageValues()
{
	mnIndices = 0;
	mnOutlinerCount = 0;				// die gliederungsobjekte muessen dem layout entsprechen,
	mnEffectCount = 0;
	mbIsTitlePossible = sal_True;			// bei mehr als einem title geht powerpoint in die knie

	return sal_True;
}


// -------------------------------------------------------------------

void ImplEESdrWriter::ImplWritePage(
			EscherSolverContainer& rSolverContainer,
			ImplEESdrPageType ePageType, sal_Bool /* bBackGround */ )
{
	ImplInitPageValues();

	sal_uInt32 nLastPer = 0, nShapes = mXShapes->getCount();
	for( sal_uInt32 n = 0; n < nShapes; ++n )
	{
		sal_uInt32 nPer = ( 5 * n ) / nShapes;
		if( nPer != nLastPer )
		{
			nLastPer = nPer;
			sal_uInt32 nValue = mnPagesWritten * 5 + nPer;
			if( nValue > mnStatMaxValue )
				nValue = mnStatMaxValue;
			if( mbStatusIndicator )
				mXStatusIndicator->setValue( nValue );
		}

		ImplEESdrObject aObj( *this, *(Reference< XShape >*)
									mXShapes->getByIndex( n ).getValue() );
		if( aObj.IsValid() )
		{
			ImplWriteShape( aObj, rSolverContainer, ePageType );
		}
	}
	mnPagesWritten++;
}

// ===================================================================

ImplEscherExSdr::ImplEscherExSdr( EscherEx& rEx )
		:
		ImplEESdrWriter( rEx ),
		mpSdrPage( NULL ),
		mpSolverContainer( NULL )
{
}


// -------------------------------------------------------------------

ImplEscherExSdr::~ImplEscherExSdr()
{
	DBG_ASSERT( !mpSolverContainer, "ImplEscherExSdr::~ImplEscherExSdr: unwritten SolverContainer" );
	delete mpSolverContainer;
}


// -------------------------------------------------------------------

bool ImplEscherExSdr::ImplInitPage( const SdrPage& rPage )
{
	do
	{
		SvxDrawPage* pSvxDrawPage;
		if ( mpSdrPage != &rPage || !mXDrawPage.is() )
		{
			// eventually write SolverContainer of current page, deletes the Solver
			ImplFlushSolverContainer();

			mpSdrPage = NULL;
			// why not declare a const parameter if the object will not be modified?
//			mXDrawPage = pSvxDrawPage = new SvxDrawPage( (SdrPage*) &rPage );
			mXDrawPage = pSvxDrawPage = new SvxFmDrawPage( (SdrPage*) &rPage );
			mXShapes = Reference< XShapes >::query( mXDrawPage );
			if ( !mXShapes.is() )
				break;
			if ( !ImplInitPageValues() )	// ImplEESdrWriter
				break;
			mpSdrPage = &rPage;

			mpSolverContainer = new EscherSolverContainer;
		}
		else
			pSvxDrawPage = SvxDrawPage::getImplementation(mXDrawPage);

        return pSvxDrawPage != 0;
	} while ( 0 );

    return false;
}

// -------------------------------------------------------------------

bool ImplEscherExSdr::ImplInitUnoShapes( const Reference< XShapes >& rxShapes )
{
    // eventually write SolverContainer of current page, deletes the Solver
    ImplFlushSolverContainer();

    if( !rxShapes.is() )
        return false;

    mpSdrPage = 0;
    mXDrawPage.clear();
    mXShapes = rxShapes;

    if( !ImplInitPageValues() )    // ImplEESdrWriter
        return false;

    mpSolverContainer = new EscherSolverContainer;
    return true;
}

// -------------------------------------------------------------------

void ImplEscherExSdr::ImplExitPage()
{
    // close all groups before the solver container is written
    while( mpEscherEx->GetGroupLevel() )
        mpEscherEx->LeaveGroup();

	ImplFlushSolverContainer();
	mpSdrPage = NULL;	// reset page for next init
}


// -------------------------------------------------------------------

void ImplEscherExSdr::ImplFlushSolverContainer()
{
	if ( mpSolverContainer )
	{
		mpSolverContainer->WriteSolver( mpEscherEx->GetStream() );
		delete mpSolverContainer;
		mpSolverContainer = NULL;
	}
}


// -------------------------------------------------------------------

void ImplEscherExSdr::ImplWriteCurrentPage()
{
	DBG_ASSERT( mpSolverContainer, "ImplEscherExSdr::ImplWriteCurrentPage: no SolverContainer" );
	ImplWritePage( *mpSolverContainer, NORMAL );
	ImplExitPage();
}


// -------------------------------------------------------------------

sal_uInt32 ImplEscherExSdr::ImplWriteTheShape( ImplEESdrObject& rObj )
{
	DBG_ASSERT( mpSolverContainer, "ImplEscherExSdr::ImplWriteShape: no SolverContainer" );
	return ImplWriteShape( rObj, *mpSolverContainer, NORMAL );
}


// ===================================================================

void EscherEx::AddSdrPage( const SdrPage& rPage )
{
	if ( mpImplEscherExSdr->ImplInitPage( rPage ) )
		mpImplEscherExSdr->ImplWriteCurrentPage();
}

// -------------------------------------------------------------------

void EscherEx::AddUnoShapes( const Reference< XShapes >& rxShapes )
{
    if ( mpImplEscherExSdr->ImplInitUnoShapes( rxShapes ) )
        mpImplEscherExSdr->ImplWriteCurrentPage();
}

// -------------------------------------------------------------------

sal_uInt32 EscherEx::AddSdrObject( const SdrObject& rObj )
{
	ImplEESdrObject aObj( *mpImplEscherExSdr, rObj );
	if( aObj.IsValid() )
		return mpImplEscherExSdr->ImplWriteTheShape( aObj );
	return 0;
}


// -------------------------------------------------------------------

void EscherEx::EndSdrObjectPage()
{
	mpImplEscherExSdr->ImplExitPage();
}

// -------------------------------------------------------------------

EscherExHostAppData* EscherEx::StartShape( 
    const Reference< XShape >& /* rShape */, 
    const basegfx::B2DPoint* /*pObjectPosition*/,
    const basegfx::B2DVector* /*pObjectScale*/ )
{
	return NULL;
}

// -------------------------------------------------------------------

void EscherEx::EndShape( sal_uInt16 /* nShapeType */, sal_uInt32 /* nShapeID */ )
{
}

// -------------------------------------------------------------------

sal_uInt32 EscherEx::QueryTextID( const Reference< XShape >&, sal_uInt32 )
{
	return 0;
}

// -------------------------------------------------------------------
// add an dummy rectangle shape into the escher stream
sal_uInt32 EscherEx::AddDummyShape()
{
	OpenContainer( ESCHER_SpContainer );
    sal_uInt32 nShapeID = GenerateShapeId();
	AddShape( ESCHER_ShpInst_Rectangle, 0xa00, nShapeID );
//??	aSolverContainer.AddShape( mXShape, nShapeID );
	CloseContainer();

	return nShapeID;
}

// -------------------------------------------------------------------

// static
const SdrObject* EscherEx::GetSdrObject( const Reference< XShape >& rShape )
{
	const SdrObject* pRet = 0;
	const SvxShape* pSvxShape = SvxShape::getImplementation( rShape );
	DBG_ASSERT( pSvxShape, "EscherEx::GetSdrObject: no SvxShape" );
	if( pSvxShape )
	{
		pRet = pSvxShape->GetSdrObject();
		DBG_ASSERT( pRet, "EscherEx::GetSdrObject: no SdrObj" );
	}
	return pRet;
}


// -------------------------------------------------------------------

ImplEESdrObject::ImplEESdrObject( ImplEscherExSdr& rEx,
									const SdrObject& rObj ) :
	mnShapeId( 0 ),
	mnTextSize( 0 ),
	mnAngle( 0 ),
	mbValid( sal_False ),
	mbPresObj( sal_False ),
	mbEmptyPresObj( sal_False )
{
	SdrPage* pPage = rObj.getSdrPageFromSdrObject();
	DBG_ASSERT( pPage, "ImplEESdrObject::ImplEESdrObject: no SdrPage" );
    if( pPage && rEx.ImplInitPage( *pPage ) )
    {
        // why not declare a const parameter if the object will
        // not be modified?
        mXShape = uno::Reference< drawing::XShape >::query( ((SdrObject*)&rObj)->getUnoShape() );;
        Init( rEx );
    }
}

ImplEESdrObject::ImplEESdrObject( ImplEESdrWriter& rEx,
									const Reference< XShape >& rShape ) :
	mXShape( rShape ),
	mnShapeId( 0 ),
	mnTextSize( 0 ),
	mnAngle( 0 ),
	mbValid( sal_False ),
	mbPresObj( sal_False ),
	mbEmptyPresObj( sal_False )
{
	Init( rEx );
}


ImplEESdrObject::~ImplEESdrObject()
{
}

void ImplEESdrObject::Init( ImplEESdrWriter& rEx )
{
	mXPropSet = Reference< XPropertySet >::query( mXShape );
	if( mXPropSet.is() )
	{
		static const sal_Char aPrefix[] = "com.sun.star.";
		static const xub_StrLen nPrefix = sizeof(aPrefix)-1;
		const basegfx::B2DPoint aPosition(rEx.ImplMapB2DPoint(basegfx::B2DPoint(mXShape->getPosition().X, mXShape->getPosition().Y)));
		const basegfx::B2DVector aScale(rEx.ImplMapB2DVector(basegfx::B2DVector(mXShape->getSize().Width, mXShape->getSize().Height)));

		setObjectPositionAndScale(aPosition, aScale);
		mType = String( mXShape->getShapeType() );
		mType.Erase( 0, nPrefix );	// strip "com.sun.star."
		xub_StrLen nPos = mType.SearchAscii( "Shape" );
		mType.Erase( nPos, 5 );

		static const OUString sPresStr(rtl::OUString::createFromAscii("IsPresentationObject"));
		static const OUString sEmptyPresStr(rtl::OUString::createFromAscii("IsEmptyPresentationObject"));

		if( ImplGetPropertyValue( sPresStr ) )
			mbPresObj = ::cppu::any2bool( mAny );

		if( mbPresObj && ImplGetPropertyValue( sEmptyPresStr ) )
			mbEmptyPresObj = ::cppu::any2bool( mAny );

		mbValid = sal_True;
	}
}

//sal_Bool ImplEESdrObject::ImplGetPropertyValue( const OUString& rString )
sal_Bool ImplEESdrObject::ImplGetPropertyValue( const sal_Unicode* rString )
{
	sal_Bool bRetValue = sal_False;
	if( mbValid )
	{
		try
		{
			mAny = mXPropSet->getPropertyValue( rString );
			if( mAny.hasValue() )
				bRetValue = sal_True;
		}
		catch( ::com::sun::star::uno::Exception& )
		{
			bRetValue = sal_False;
		}
	}
	return bRetValue;
}

#ifdef USED
sal_Bool ImplEESdrObject::ImplGetPropertyValue( const Reference< XPropertySet >& rXPropSet,
											const OUString& rString )
{
	sal_Bool bRetValue = sal_False;
	if( mbValid )
	{
		try
		{
			mAny = rXPropSet->getPropertyValue( rString );
			if( 0 != mAny.get() )
				bRetValue = sal_True;
		}
		catch( ::com::sun::star::uno::Exception& )
		{
			bRetValue = sal_False;
		}
	}
	return bRetValue;
}
#endif

const SdrObject* ImplEESdrObject::GetSdrObject() const
{
	return EscherEx::GetSdrObject( mXShape );
}

//  laedt und konvertiert text aus shape, ergebnis ist mnTextSize gespeichert
sal_uInt32 ImplEESdrObject::ImplGetText()
{
	Reference< XText > xXText( mXShape, UNO_QUERY );
	mnTextSize = 0;
	if( xXText.is() )
		mnTextSize = xXText->getString().getLength();
	return mnTextSize;
}

sal_Bool ImplEESdrObject::ImplHasText() const
{
	Reference< XText > xXText( mXShape, UNO_QUERY );
	return xXText.is() && xXText->getString().getLength();
}

