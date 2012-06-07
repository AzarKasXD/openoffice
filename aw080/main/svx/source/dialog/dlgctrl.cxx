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
#include "precompiled_svx.hxx"

// include ---------------------------------------------------------------
#include <tools/shl.hxx>
#include <vcl/svapp.hxx>
#include <svx/xtable.hxx>
#include <svx/xpool.hxx>
#include <svx/dialogs.hrc>
#include <accessibility.hrc>
#include <svx/dlgctrl.hxx>
#include <svx/dialmgr.hxx>
#include <tools/poly.hxx>
#include <vcl/region.hxx>
#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <svtools/colorcfg.hxx>
#include <svxrectctaccessiblecontext.hxx>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdopath.hxx>
#include <svx/sdr/contact/objectcontactofobjlistpainter.hxx>
#include <svx/sdr/contact/displayinfo.hxx>
#include <linectrl.hrc>
#include <vcl/bmpacc.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/svdlegacy.hxx>

#define OUTPUT_DRAWMODE_COLOR		(DRAWMODE_DEFAULT)
#define OUTPUT_DRAWMODE_CONTRAST	(DRAWMODE_SETTINGSLINE | DRAWMODE_SETTINGSFILL | DRAWMODE_SETTINGSTEXT | DRAWMODE_SETTINGSGRADIENT)

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::accessibility;


/*************************************************************************
|*
|*	Control zur Darstellung und Auswahl der Eckpunkte (und Mittelpunkt)
|*	eines Objekts
|*
\************************************************************************/

Bitmap& SvxRectCtl::GetRectBitmap( void )
{
	if( !pBitmap )
		InitRectBitmap();

	return *pBitmap;
}

SvxRectCtl::SvxRectCtl( Window* pParent, const ResId& rResId, RECT_POINT eRpt,
						sal_uInt16 nBorder, sal_uInt16 nCircle, CTL_STYLE eStyle ) :

	Control( pParent, rResId ),

	pAccContext	( NULL ),
    nBorderWidth( nBorder ),
    nRadius     ( nCircle),
    eDefRP      ( eRpt ),
    eCS         ( eStyle ),
	pBitmap		( NULL ),
    m_nState    ( 0 ),
    mbCompleteDisable(sal_False)
{
	SetMapMode( MAP_100TH_MM );
    Resize_Impl();
}

// -----------------------------------------------------------------------

SvxRectCtl::~SvxRectCtl()
{
	delete pBitmap;

	if( pAccContext )
		pAccContext->release();
}

// -----------------------------------------------------------------------
void SvxRectCtl::Resize()
{
    Resize_Impl();
    Control::Resize();
}

// -----------------------------------------------------------------------

void SvxRectCtl::Resize_Impl()
{
    aSize = GetOutputSize();

	switch( eCS )
	{
		case CS_RECT:
		case CS_ANGLE:
		case CS_SHADOW:
			aPtLT = Point( 0 + nBorderWidth,  0 + nBorderWidth );
			aPtMT = Point( aSize.Width() / 2, 0 + nBorderWidth );
			aPtRT = Point( aSize.Width() - nBorderWidth, 0 + nBorderWidth );

			aPtLM = Point( 0 + nBorderWidth,  aSize.Height() / 2 );
			aPtMM = Point( aSize.Width() / 2, aSize.Height() / 2 );
			aPtRM = Point( aSize.Width() - nBorderWidth, aSize.Height() / 2 );

			aPtLB = Point( 0 + nBorderWidth,	aSize.Height() - nBorderWidth );
			aPtMB = Point( aSize.Width() / 2,	aSize.Height() - nBorderWidth );
			aPtRB = Point( aSize.Width() - nBorderWidth, aSize.Height() - nBorderWidth );
		break;

		case CS_LINE:
			aPtLT = Point( 0 + 3 * nBorderWidth,  0 + nBorderWidth );
			aPtMT = Point( aSize.Width() / 2, 0 + nBorderWidth );
			aPtRT = Point( aSize.Width() - 3 * nBorderWidth, 0 + nBorderWidth );

			aPtLM = Point( 0 + 3 * nBorderWidth,  aSize.Height() / 2 );
			aPtMM = Point( aSize.Width() / 2, aSize.Height() / 2 );
			aPtRM = Point( aSize.Width() - 3 * nBorderWidth, aSize.Height() / 2 );

			aPtLB = Point( 0 + 3 * nBorderWidth,	aSize.Height() - nBorderWidth );
			aPtMB = Point( aSize.Width() / 2,	aSize.Height() - nBorderWidth );
			aPtRB = Point( aSize.Width() - 3 * nBorderWidth, aSize.Height() - nBorderWidth );
		break;
	}
	Reset();
	InitSettings( sal_True, sal_True );
}
// -----------------------------------------------------------------------

void SvxRectCtl::InitRectBitmap( void )
{
	if( pBitmap )
		delete pBitmap;

	const StyleSettings&	rStyles = Application::GetSettings().GetStyleSettings();
	svtools::ColorConfig aColorConfig;

	pBitmap = new Bitmap( SVX_RES( RID_SVXCTRL_RECTBTNS ) );

	// set bitmap-colors
	Color	aColorAry1[7];
	Color	aColorAry2[7];
	aColorAry1[0] = Color( 0xC0, 0xC0, 0xC0 );	// light-gray
	aColorAry1[1] = Color( 0xFF, 0xFF, 0x00 );	// yellow
	aColorAry1[2] = Color( 0xFF, 0xFF, 0xFF );	// white
	aColorAry1[3] = Color( 0x80, 0x80, 0x80 );	// dark-gray
	aColorAry1[4] = Color( 0x00, 0x00, 0x00 );  // black
	aColorAry1[5] = Color( 0x00, 0xFF, 0x00 );	// green
	aColorAry1[6] = Color( 0x00, 0x00, 0xFF );	// blue
	aColorAry2[0] = rStyles.GetDialogColor();		// background
	aColorAry2[1] = rStyles.GetWindowColor();
	aColorAry2[2] = rStyles.GetLightColor();
	aColorAry2[3] = rStyles.GetShadowColor();
	aColorAry2[4] = rStyles.GetDarkShadowColor();
	aColorAry2[5] = Color( aColorConfig.GetColorValue( svtools::FONTCOLOR ).nColor );
	aColorAry2[6] = rStyles.GetDialogColor();

#ifdef DBG_UTIL
	static sal_Bool		bModify = sal_False;
	sal_Bool&			rModify = bModify;
	if( rModify )
	{
		static int		n = 0;
		static sal_uInt8	r = 0xFF;
		static sal_uInt8	g = 0x00;
		static sal_uInt8	b = 0xFF;
		int&			rn = n;
		sal_uInt8&			rr = r;
		sal_uInt8&			rg = g;
		sal_uInt8&			rb = b;
		aColorAry2[ rn ] = Color( rr, rg, rb );
	}
#endif

	pBitmap->Replace( aColorAry1, aColorAry2, 7, NULL );
}

// -----------------------------------------------------------------------

void SvxRectCtl::InitSettings( sal_Bool bForeground, sal_Bool bBackground )
{
	const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

	if( bForeground )
	{
		svtools::ColorConfig aColorConfig;
		Color aTextColor( aColorConfig.GetColorValue( svtools::FONTCOLOR ).nColor );

		if ( IsControlForeground() )
			aTextColor = GetControlForeground();
		SetTextColor( aTextColor );
	}

	if( bBackground )
	{
		if ( IsControlBackground() )
			SetBackground( GetControlBackground() );
		else
			SetBackground( rStyleSettings.GetWindowColor() );
	}

	delete pBitmap;
	pBitmap = NULL;		// forces new creating of bitmap

	Invalidate();
}

/*************************************************************************
|*
|*	Das angeklickte Rechteck (3 x 3) wird ermittelt und der Parent (Dialog)
|*	wird benachrichtigt, dass der Punkt geaendert wurde
|*
\************************************************************************/

void SvxRectCtl::MouseButtonDown( const MouseEvent& rMEvt )
{
	// #103516# CompletelyDisabled() added to have a disabled state for SvxRectCtl
	if(!IsCompletelyDisabled())
	{
		Point aPtLast = aPtNew;

		aPtNew = GetApproxLogPtFromPixPt( rMEvt.GetPosPixel() );

		if( aPtNew == aPtMM && ( eCS == CS_SHADOW || eCS == CS_ANGLE ) )
		{
			aPtNew = aPtLast;
		}
		else
		{
			Invalidate( Rectangle( aPtLast - Point( nRadius, nRadius ),
								   aPtLast + Point( nRadius, nRadius ) ) );
			Invalidate( Rectangle( aPtNew - Point( nRadius, nRadius ),
								   aPtNew + Point( nRadius, nRadius ) ) );
			eRP = GetRPFromPoint( aPtNew );

			SetActualRP( eRP );

			if( WINDOW_TABPAGE == GetParent()->GetType() )
				( (SvxTabPage*) GetParent() )->PointChanged( this, eRP );
		}
	}
}

// -----------------------------------------------------------------------

void SvxRectCtl::KeyInput( const KeyEvent& rKeyEvt )
{
	// #103516# CompletelyDisabled() added to have a disabled state for SvxRectCtl
	if(!IsCompletelyDisabled())
	{
		RECT_POINT eNewRP = eRP;
		sal_Bool bUseMM = (eCS != CS_SHADOW) && (eCS != CS_ANGLE);

		switch( rKeyEvt.GetKeyCode().GetCode() )
		{
			case KEY_DOWN:
			{
				if( !(m_nState & CS_NOVERT) )
					switch( eNewRP )
					{
						case RP_LT: eNewRP = RP_LM; break;
						case RP_MT: eNewRP = bUseMM ? RP_MM : RP_MB; break;
						case RP_RT: eNewRP = RP_RM; break;
						case RP_LM: eNewRP = RP_LB; break;
						case RP_MM: eNewRP = RP_MB; break;
						case RP_RM: eNewRP = RP_RB; break;
                        default: ; //prevent warning
                    }
			}
			break;
			case KEY_UP:
			{
				if( !(m_nState & CS_NOVERT) )
					switch( eNewRP )
					{
						case RP_LM: eNewRP = RP_LT; break;
						case RP_MM: eNewRP = RP_MT; break;
						case RP_RM: eNewRP = RP_RT; break;
						case RP_LB: eNewRP = RP_LM; break;
						case RP_MB: eNewRP = bUseMM ? RP_MM : RP_MT; break;
						case RP_RB: eNewRP = RP_RM; break;
                        default: ; //prevent warning
                    }
			}
			break;
			case KEY_LEFT:
			{
				if( !(m_nState & CS_NOHORZ) )
					switch( eNewRP )
					{
						case RP_MT: eNewRP = RP_LT; break;
						case RP_RT: eNewRP = RP_MT; break;
						case RP_MM: eNewRP = RP_LM; break;
						case RP_RM: eNewRP = bUseMM ? RP_MM : RP_LM; break;
						case RP_MB: eNewRP = RP_LB; break;
						case RP_RB: eNewRP = RP_MB; break;
                        default: ; //prevent warning
                    }
			}
			break;
			case KEY_RIGHT:
			{
				if( !(m_nState & CS_NOHORZ) )
					switch( eNewRP )
					{
						case RP_LT: eNewRP = RP_MT; break;
						case RP_MT: eNewRP = RP_RT; break;
						case RP_LM: eNewRP = bUseMM ? RP_MM : RP_RM; break;
						case RP_MM: eNewRP = RP_RM; break;
						case RP_LB: eNewRP = RP_MB; break;
						case RP_MB: eNewRP = RP_RB; break;
                        default: ; //prevent warning
                    }
			}
			break;
			default:
				Control::KeyInput( rKeyEvt );
				return;
		}
		if( eNewRP != eRP )
		{
			SetActualRP( eNewRP );

			if( WINDOW_TABPAGE == GetParent()->GetType() )
				( (SvxTabPage*) GetParent() )->PointChanged( this, eRP );

			SetFocusRect();
		}
	}
}

// -----------------------------------------------------------------------

void SvxRectCtl::StateChanged( StateChangedType nType )
{
	if ( nType == STATE_CHANGE_CONTROLFOREGROUND )
		InitSettings( sal_True, sal_False );
	else if ( nType == STATE_CHANGE_CONTROLBACKGROUND )
		InitSettings( sal_False, sal_True );

	Window::StateChanged( nType );
}

// -----------------------------------------------------------------------

void SvxRectCtl::DataChanged( const DataChangedEvent& rDCEvt )
{
	if ( ( rDCEvt.GetType() == DATACHANGED_SETTINGS ) && ( rDCEvt.GetFlags() & SETTINGS_STYLE ) )
		InitSettings( sal_True, sal_True );
	else
		Window::DataChanged( rDCEvt );
}

/*************************************************************************
|*
|*	Zeichnet das Control (Rechteck mit 9 Kreisen)
|*
\************************************************************************/

void SvxRectCtl::Paint( const Rectangle& )
{
	Point	aPtDiff( PixelToLogic( Point( 1, 1 ) ) );

	const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();

	SetLineColor( rStyles.GetDialogColor() );
	SetFillColor( rStyles.GetDialogColor() );
	DrawRect( Rectangle( Point(0,0), GetOutputSize() ) );

	if( IsEnabled() )
		SetLineColor( rStyles.GetLabelTextColor() );
	else
		SetLineColor( rStyles.GetShadowColor() );

	SetFillColor();

	switch( eCS )
	{

		case CS_RECT:
		case CS_SHADOW:
			if( !IsEnabled() )
			{
				Color aOldCol = GetLineColor();
				SetLineColor( rStyles.GetLightColor() );
				DrawRect( Rectangle( aPtLT + aPtDiff, aPtRB + aPtDiff ) );
				SetLineColor( aOldCol );
			}
			DrawRect( Rectangle( aPtLT, aPtRB ) );
		break;

		case CS_LINE:
			if( !IsEnabled() )
			{
				Color aOldCol = GetLineColor();
				SetLineColor( rStyles.GetLightColor() );
				DrawLine( aPtLM - Point( 2 * nBorderWidth, 0) + aPtDiff,
						  aPtRM + Point( 2 * nBorderWidth, 0 ) + aPtDiff );
				SetLineColor( aOldCol );
			}
			DrawLine( aPtLM - Point( 2 * nBorderWidth, 0),
					  aPtRM + Point( 2 * nBorderWidth, 0 ) );
		break;

		case CS_ANGLE:
			if( !IsEnabled() )
			{
				Color aOldCol = GetLineColor();
				SetLineColor( rStyles.GetLightColor() );
				DrawLine( aPtLT + aPtDiff, aPtRB + aPtDiff );
				DrawLine( aPtLB + aPtDiff, aPtRT + aPtDiff );
				DrawLine( aPtLM + aPtDiff, aPtRM + aPtDiff );
				DrawLine( aPtMT + aPtDiff, aPtMB + aPtDiff );
				SetLineColor( aOldCol );
			}
			DrawLine( aPtLT, aPtRB );
			DrawLine( aPtLB, aPtRT );
			DrawLine( aPtLM, aPtRM );
			DrawLine( aPtMT, aPtMB );
		break;

		default:
			break;
	}
	SetFillColor( GetBackground().GetColor() );

	Size aBtnSize( 11, 11 );
	Size aDstBtnSize(  PixelToLogic( aBtnSize ) );
	Point aToCenter( aDstBtnSize.Width() >> 1, aDstBtnSize.Height() >> 1);
	Point aBtnPnt1( IsEnabled()?0:22,0 );
	Point aBtnPnt2( 11,0 );
	Point aBtnPnt3( 22,0 );

	sal_Bool bNoHorz = (m_nState & CS_NOHORZ) != 0;
	sal_Bool bNoVert = (m_nState & CS_NOVERT) != 0;

	Bitmap&			rBitmap = GetRectBitmap();

	// #103516# CompletelyDisabled() added to have a disabled state for SvxRectCtl
	if(IsCompletelyDisabled())
	{
		DrawBitmap( aPtLT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtMT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtRT - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtLM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		if( eCS == CS_RECT || eCS == CS_LINE )
			DrawBitmap( aPtMM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtRM - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtLB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtMB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
		DrawBitmap( aPtRB - aToCenter, aDstBtnSize, aBtnPnt3, aBtnSize, rBitmap );
	}
	else
	{
		DrawBitmap( aPtLT - aToCenter, aDstBtnSize, (bNoHorz | bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
		DrawBitmap( aPtMT - aToCenter, aDstBtnSize, bNoVert?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
		DrawBitmap( aPtRT - aToCenter, aDstBtnSize, (bNoHorz | bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

		DrawBitmap( aPtLM - aToCenter, aDstBtnSize, bNoHorz?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

		// Mittelpunkt bei Rechteck und Linie
		if( eCS == CS_RECT || eCS == CS_LINE )
			DrawBitmap( aPtMM - aToCenter, aDstBtnSize, aBtnPnt1, aBtnSize, rBitmap );

		DrawBitmap( aPtRM - aToCenter, aDstBtnSize, bNoHorz?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );

		DrawBitmap( aPtLB - aToCenter, aDstBtnSize, (bNoHorz | bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
		DrawBitmap( aPtMB - aToCenter, aDstBtnSize, bNoVert?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
		DrawBitmap( aPtRB - aToCenter, aDstBtnSize, (bNoHorz | bNoVert)?aBtnPnt3:aBtnPnt1, aBtnSize, rBitmap );
	}

	// draw active button, avoid center pos for angle
	// #103516# CompletelyDisabled() added to have a disabled state for SvxRectCtl
	if(!IsCompletelyDisabled())
	{
		if( IsEnabled() && (eCS != CS_ANGLE || aPtNew != aPtMM) )
		{
			Point		aCenterPt( aPtNew );
			aCenterPt -= aToCenter;

			DrawBitmap( aCenterPt, aDstBtnSize, aBtnPnt2, aBtnSize, rBitmap );
		}
	}
}

/*************************************************************************
|*
|*	Konvertiert RECT_POINT in Point
|*
\************************************************************************/

Point SvxRectCtl::GetPointFromRP( RECT_POINT _eRP) const
{
    switch( _eRP )
	{
		case RP_LT: return aPtLT;
		case RP_MT: return aPtMT;
		case RP_RT: return aPtRT;
		case RP_LM: return aPtLM;
		case RP_MM: return aPtMM;
		case RP_RM: return aPtRM;
		case RP_LB: return aPtLB;
		case RP_MB: return aPtMB;
		case RP_RB: return aPtRB;
	}
	return( aPtMM ); // default
}


void SvxRectCtl::SetFocusRect( const Rectangle* pRect )
{
	HideFocus();

	if( pRect )
		ShowFocus( *pRect );
	else
		ShowFocus( CalculateFocusRectangle() );
}

Point SvxRectCtl::SetActualRPWithoutInvalidate( RECT_POINT eNewRP )
{
	Point aPtLast = aPtNew;
	aPtNew = GetPointFromRP( eNewRP );

	if( (m_nState & CS_NOHORZ) != 0 )
		aPtNew.X() = aPtMM.X();

	if( (m_nState & CS_NOVERT) != 0 )
		aPtNew.Y() = aPtMM.Y();

	eNewRP = GetRPFromPoint( aPtNew );

	eDefRP = eNewRP;
	eRP = eNewRP;

	return aPtLast;
}

void SvxRectCtl::GetFocus()
{
	SetFocusRect();
}


void SvxRectCtl::LoseFocus()
{
	HideFocus();
}


Point SvxRectCtl::GetApproxLogPtFromPixPt( const Point& rPt ) const
{
	Point	aPt = PixelToLogic( rPt );
	long	x;
	long	y;

	if( ( m_nState & CS_NOHORZ ) == 0 )
	{
		if( aPt.X() < aSize.Width() / 3 )
			x = aPtLT.X();
		else if( aPt.X() < aSize.Width() * 2 / 3 )
			x = aPtMM.X();
		else
			x = aPtRB.X();
	}
	else
		x = aPtMM.X();

	if( ( m_nState & CS_NOVERT ) == 0 )
	{
		if( aPt.Y() < aSize.Height() / 3 )
			y = aPtLT.Y();
		else if( aPt.Y() < aSize.Height() * 2 / 3 )
			y = aPtMM.Y();
		else
			y = aPtRB.Y();
	}
	else
			y = aPtMM.Y();

	return Point( x, y );
}


/*************************************************************************
|*
|*	Konvertiert Point in RECT_POINT
|*
\************************************************************************/

RECT_POINT SvxRectCtl::GetRPFromPoint( Point aPt ) const
{
	if	   ( aPt == aPtLT) return RP_LT;
	else if( aPt == aPtMT) return RP_MT;
	else if( aPt == aPtRT) return RP_RT;
	else if( aPt == aPtLM) return RP_LM;
	else if( aPt == aPtRM) return RP_RM;
	else if( aPt == aPtLB) return RP_LB;
	else if( aPt == aPtMB) return RP_MB;
	else if( aPt == aPtRB) return RP_RB;

	else
		return RP_MM; // default
}

/*************************************************************************
|*
|*	Bewirkt den Ursprungszustand des Controls
|*
\************************************************************************/

void SvxRectCtl::Reset()
{
	aPtNew = GetPointFromRP( eDefRP );
	eRP = eDefRP;
	Invalidate();
}

/*************************************************************************
|*
|*	Gibt den aktuell ausgewaehlten RECT_POINT zur�ck
|*
\************************************************************************/

RECT_POINT SvxRectCtl::GetActualRP() const
{
	return( eRP );
}

/*************************************************************************
|*
|*	Gibt den aktuell ausgewaehlten RECT_POINT zur�ck
|*
\************************************************************************/

void SvxRectCtl::SetActualRP( RECT_POINT eNewRP )
{
	Point aPtLast( SetActualRPWithoutInvalidate( eNewRP ) );

	Invalidate( Rectangle( aPtLast - Point( nRadius, nRadius ), aPtLast + Point( nRadius, nRadius ) ) );
	Invalidate( Rectangle( aPtNew - Point( nRadius, nRadius ), aPtNew + Point( nRadius, nRadius ) ) );

	// notify accessibility object about change
	if( pAccContext )
		pAccContext->selectChild( eNewRP );
}

void SvxRectCtl::SetState( CTL_STATE nState )
{
	m_nState = nState;

	Point aPtLast( GetPointFromRP( eRP ) );
    Point _aPtNew( aPtLast );

	if( (m_nState & CS_NOHORZ) != 0 )
        _aPtNew.X() = aPtMM.X();

	if( (m_nState & CS_NOVERT) != 0 )
        _aPtNew.Y() = aPtMM.Y();

    eRP = GetRPFromPoint( _aPtNew );
	Invalidate();

	if( WINDOW_TABPAGE == GetParent()->GetType() )
		( (SvxTabPage*) GetParent() )->PointChanged( this, eRP );
}

sal_uInt8 SvxRectCtl::GetNumOfChilds( void ) const
{
	return ( eCS == CS_ANGLE )? 8 : 9;
}

Rectangle SvxRectCtl::CalculateFocusRectangle( void ) const
{
	Size		aDstBtnSize( PixelToLogic( Size( 15, 15 ) ) );
	return Rectangle( aPtNew - Point( aDstBtnSize.Width() >> 1, aDstBtnSize.Height() >> 1 ), aDstBtnSize );
}

Rectangle SvxRectCtl::CalculateFocusRectangle( RECT_POINT eRectPoint ) const
{
	Rectangle	aRet;
	RECT_POINT	eOldRectPoint = GetActualRP();

	if( eOldRectPoint == eRectPoint )
		aRet = CalculateFocusRectangle();
	else
	{
		SvxRectCtl*	pThis = const_cast< SvxRectCtl* >( this );

		pThis->SetActualRPWithoutInvalidate( eRectPoint );		// no invalidation because it's only temporary!
		aRet = CalculateFocusRectangle();

		pThis->SetActualRPWithoutInvalidate( eOldRectPoint );	// no invalidation because nothing has changed!
	}

	return aRet;
}

Reference< XAccessible > SvxRectCtl::CreateAccessible()
{
	Window*						pParent = GetAccessibleParentWindow();

	DBG_ASSERT( pParent, "-SvxRectCtl::CreateAccessible(): No Parent!" );

	Reference< XAccessible >	xAccParent  = pParent->GetAccessible();
	if( xAccParent.is() )
	{
		pAccContext = new SvxRectCtlAccessibleContext( xAccParent, *this );
		pAccContext->acquire();

        SetActualRP( GetActualRP() );

		return pAccContext;
	}
	else
		return Reference< XAccessible >();
}

RECT_POINT SvxRectCtl::GetApproxRPFromPixPt( const ::com::sun::star::awt::Point& r ) const
{
	return GetRPFromPoint( GetApproxLogPtFromPixPt( Point( r.X, r.Y ) ) );
}

// #103516# CompletelyDisabled() added to have a disabled state for SvxRectCtl
void SvxRectCtl::DoCompletelyDisable(sal_Bool bNew) 
{ 
	mbCompleteDisable = bNew; 
	Invalidate();
}

/*************************************************************************
|*
|* Konstruktor ohne Size-Parameter
|*
\************************************************************************/

SvxAngleCtl::SvxAngleCtl( Window* pParent, const ResId& rResId ) :

	SvxRectCtl( pParent, rResId ),

	aFont( Application::GetSettings().GetStyleSettings().GetAppFont() )
{
	aFontSize = Size( 250, 400 );
	Initialize();
}

/*************************************************************************
|*
|* Konstruktor mit Size-Parameter
|*
\************************************************************************/

SvxAngleCtl::SvxAngleCtl( Window* pParent, const ResId& rResId, Size _aSize ) :

	SvxRectCtl( pParent, rResId ),

	aFont( Application::GetSettings().GetStyleSettings().GetAppFont() )
{
    aFontSize = _aSize;
	Initialize();
}

/*************************************************************************
|*
|* Initialisierung
|*
\************************************************************************/

void SvxAngleCtl::Initialize()
{
	bPositive = sal_True;

	// aFont.SetName( "Helvetica" );
	aFont.SetSize( aFontSize );
	aFont.SetWeight( WEIGHT_NORMAL );
	aFont.SetTransparent( sal_False );

	SetFont( aFont );
}

/*************************************************************************
|*
|*	Zeichnet das (Mini-)Koordinatensystem
|*
\************************************************************************/

void SvxAngleCtl::Paint( const Rectangle& )
{
	SetLineColor( Color( COL_BLACK ) ); 	// PEN_DOT ???
	DrawLine( aPtLT - Point( 0, 0), aPtRB + Point( 0, 0 ) );
	DrawLine( aPtLB - Point( 0, 0), aPtRT + Point( 0, 0 ) );

	SetLineColor( Color( COL_BLACK ) );
	DrawLine( aPtLM - Point( 0, 0), aPtRM + Point( 0, 0 ) );
	DrawLine( aPtMT - Point( 0, 0), aPtMB + Point( 0, 0 ) );

	Point aDiff(aFontSize.Width() / 2, aFontSize.Height() / 2);

	DrawText( aPtLT - aDiff, UniString::CreateFromAscii(
		                     RTL_CONSTASCII_STRINGPARAM( "135" ) ) );
	DrawText( aPtLM - aDiff, UniString::CreateFromAscii(
		                     RTL_CONSTASCII_STRINGPARAM( "180" ) ) );

	if ( bPositive )
		DrawText( aPtLB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "225" ) ) );
	else
		DrawText( aPtLB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "-135" ) ) );

	aDiff.X() = aFontSize.Width();
	DrawText( aPtMT - aDiff, UniString::CreateFromAscii(
		                     RTL_CONSTASCII_STRINGPARAM( "90" ) ) );
	DrawText( aPtRT - aDiff, UniString::CreateFromAscii(
		                     RTL_CONSTASCII_STRINGPARAM( "45" ) ) );
	aDiff.X() = aDiff .X() * 3 / 2;

	if ( bPositive )
		DrawText( aPtMB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "270" ) ) );
	else
		DrawText( aPtMB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "-90" ) ) );

	DrawText( aPtRM - Point( 0, aDiff.Y() ), UniString::CreateFromAscii(
		                                     RTL_CONSTASCII_STRINGPARAM( "0" ) ) );
	aDiff.X() = aFontSize.Width() * 2;

	if ( bPositive )
		DrawText( aPtRB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "315" ) ) );
	else
		DrawText( aPtRB - aDiff, UniString::CreateFromAscii(
		                         RTL_CONSTASCII_STRINGPARAM( "-45" ) ) );
}

/*************************************************************************
|*
|*	Control zum Editieren von Bitmaps
|*
\************************************************************************/

SvxPixelCtl::SvxPixelCtl( Window* pParent, const ResId& rResId, sal_uInt16 nNumber ) :
						Control		( pParent, rResId ),
						nLines		( nNumber ),
						bPaintable	( sal_True )
{
	// SetMapMode( MAP_100TH_MM );
	aRectSize = GetOutputSize();

	SetPixelColor( Color( COL_BLACK ) );
	SetBackgroundColor( Color( COL_WHITE ) );
	SetLineColor( Application::GetSettings().GetStyleSettings().GetShadowColor() );

	nSquares = nLines * nLines;
	pPixel = new sal_uInt16[ nSquares ];
	rtl_zeroMemory(pPixel, nSquares * sizeof(sal_uInt16));
}

/*************************************************************************
|*
|*	Destruktor dealociert dyn. Array
|*
\************************************************************************/

SvxPixelCtl::~SvxPixelCtl( )
{
	delete []pPixel;
}

/*************************************************************************
|*
|*	Wechselt die Vordergrund- ,bzw. Hintergrundfarbe
|*
\************************************************************************/

void SvxPixelCtl::ChangePixel( sal_uInt16 nPixel )
{
	if( *( pPixel + nPixel) == 0 )
		*( pPixel + nPixel) = 1; // koennte erweitert werden auf mehrere Farben
	else
		*( pPixel + nPixel) = 0;
}

/*************************************************************************
|*
|*	Das angeklickte Rechteck wird ermittelt um die Farbe zu wechseln
|*
\************************************************************************/

void SvxPixelCtl::MouseButtonDown( const MouseEvent& rMEvt )
{
	Point aPt = PixelToLogic( rMEvt.GetPosPixel() );
	Point aPtTl, aPtBr;
	sal_uInt16	nX, nY;

	nX = (sal_uInt16) ( aPt.X() * nLines / aRectSize.Width() );
	nY = (sal_uInt16) ( aPt.Y() * nLines / aRectSize.Height() );

	ChangePixel( nX + nY * nLines );

	aPtTl.X() = aRectSize.Width() * nX / nLines + 1;
	aPtBr.X() = aRectSize.Width() * (nX + 1) / nLines - 1;
	aPtTl.Y() = aRectSize.Height() * nY / nLines + 1;
	aPtBr.Y() = aRectSize.Height() * (nY + 1) / nLines - 1;

	Invalidate( Rectangle( aPtTl, aPtBr ) );

	if( WINDOW_TABPAGE == GetParent()->GetType() )
		( (SvxTabPage*) GetParent() )->PointChanged( this, RP_MM ); // RectPoint ist dummy
}

/*************************************************************************
|*
|*	Zeichnet das Control (Rechteck mit 9 Kreisen)
|*
\************************************************************************/

void SvxPixelCtl::Paint( const Rectangle& )
{
	sal_uInt16	i, j, nTmp;
	Point	aPtTl, aPtBr;

	if( bPaintable )
	{
		// Linien Zeichnen
		Control::SetLineColor( aLineColor );
		for( i = 1; i < nLines; i++)
		{
			// horizontal
			nTmp = (sal_uInt16) ( aRectSize.Height() * i / nLines );
			DrawLine( Point( 0, nTmp ), Point( aRectSize.Width(), nTmp ) );
			// vertikal
			nTmp = (sal_uInt16) ( aRectSize.Width() * i / nLines );
			DrawLine( Point( nTmp, 0 ), Point( nTmp, aRectSize.Height() ) );
		}

		// Rechtecke (Quadrate) zeichnen
		Control::SetLineColor();
		sal_uInt16 nLastPixel = *pPixel ? 0 : 1;

		for( i = 0; i < nLines; i++)
		{
			aPtTl.Y() = aRectSize.Height() * i / nLines + 1;
			aPtBr.Y() = aRectSize.Height() * (i + 1) / nLines - 1;

			for( j = 0; j < nLines; j++)
			{
				aPtTl.X() = aRectSize.Width() * j / nLines + 1;
				aPtBr.X() = aRectSize.Width() * (j + 1) / nLines - 1;

				if ( *( pPixel + i * nLines + j ) != nLastPixel )
				{
					nLastPixel = *( pPixel + i * nLines + j );
					// Farbe wechseln: 0 -> Hintergrundfarbe
					SetFillColor( nLastPixel ? aPixelColor : aBackgroundColor );
				}
				DrawRect( Rectangle( aPtTl, aPtBr ) );
			}
		}
	} // bPaintable
	else
	{
		SetBackground( Wallpaper( Color( COL_LIGHTGRAY ) ) );
		Control::SetLineColor( Color( COL_LIGHTRED ) );
		DrawLine( Point( 0, 0 ), Point( aRectSize.Width(), aRectSize.Height() ) );
		DrawLine( Point( 0, aRectSize.Height() ), Point( aRectSize.Width(), 0 ) );
	}
}

/*************************************************************************
|*
|*
|*
\************************************************************************/

void SvxPixelCtl::SetXBitmap( const BitmapEx& rBitmapEx )
{
    BitmapColor aBack;
    BitmapColor aFront;

    if(isHistorical8x8(rBitmapEx, aBack, aFront))
    {
        Bitmap aBitmap(rBitmapEx.GetBitmap());
        BitmapReadAccess* pRead = aBitmap.AcquireReadAccess();

        aBackgroundColor = aBack;
        aPixelColor = aFront;

        for(sal_uInt16 i(0); i < nSquares; i++)
        {
            const BitmapColor aColor(pRead->GetColor(i/8, i%8));

            if(aColor == aBack)
            {
                *( pPixel + i ) = 0;
            }
            else
            {
                *( pPixel + i ) = 1;
            }
        }

        aBitmap.ReleaseAccess(pRead);
    }
}

/*************************************************************************
|*
|*	Gibt ein bestimmtes Pixel zurueck
|*
\************************************************************************/

sal_uInt16 SvxPixelCtl::GetBitmapPixel( const sal_uInt16 nPixel )
{
	return( *( pPixel + nPixel ) );
}

/*************************************************************************
|*
|*	Bewirkt den Ursprungszustand des Controls
|*
\************************************************************************/

void SvxPixelCtl::Reset()
{
	// clear pixel area
	rtl_zeroMemory(pPixel, nSquares * sizeof(sal_uInt16));
	Invalidate();
}

/*************************************************************************
|*
|*	Ctor: BitmapCtl fuer SvxPixelCtl
|*
\************************************************************************/

SvxBitmapCtl::SvxBitmapCtl( Window* /*pParent*/, const Size& rSize )
{
	aSize = rSize;
	// aVD.SetOutputSizePixel( aSize );
}

/*************************************************************************
|*
|*	Dtor
|*
\************************************************************************/

SvxBitmapCtl::~SvxBitmapCtl()
{
}

/*************************************************************************
|*
|*	BitmapCtl:	Gibt die Bitmap zurueck
|*
\************************************************************************/

BitmapEx SvxBitmapCtl::GetBitmapEx()
{
    const Bitmap aRetval(createHistorical8x8FromArray(pBmpArray, aPixelColor, aBackgroundColor));

	return BitmapEx(aRetval);
}

/*************************************************************************
|*
|*	Fuellt die Listbox mit Farben und Strings
|*
\************************************************************************/

void ColorLB::Fill( const XColorTable* pColorTab )
{
	long nCount = pColorTab->Count();
	XColorEntry* pEntry;
	SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pColorTab->GetColor( i );
		InsertEntry( pEntry->GetColor(), pEntry->GetName() );
	}
	SetUpdateMode( sal_True );
}

/************************************************************************/

void ColorLB::Append( XColorEntry* pEntry, Bitmap* )
{
	InsertEntry( pEntry->GetColor(), pEntry->GetName() );
}

/************************************************************************/

void ColorLB::Modify( XColorEntry* pEntry, sal_uInt16 nPos, Bitmap*  )
{
	RemoveEntry( nPos );
	InsertEntry( pEntry->GetColor(), pEntry->GetName(), nPos );
}

/*************************************************************************
|*
|*	Fuellt die Listbox mit Farben und Strings
|*
\************************************************************************/

void FillAttrLB::Fill( const XColorTable* pColorTab )
{
	long nCount = pColorTab->Count();
	XColorEntry* pEntry;
	SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pColorTab->GetColor( i );
		InsertEntry( pEntry->GetColor(), pEntry->GetName() );
	}
	SetUpdateMode( sal_True );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

HatchingLB::HatchingLB( Window* pParent, ResId Id, sal_Bool bUserDraw /*= sal_True*/ )
: ListBox( pParent, Id ),
  mpList ( NULL ),
  mbUserDraw( bUserDraw )
{
	EnableUserDraw( mbUserDraw );
}

HatchingLB::HatchingLB( Window* pParent, WinBits aWB, sal_Bool bUserDraw /*= sal_True*/ )
: ListBox( pParent, aWB ),
  mpList ( NULL ),
  mbUserDraw( bUserDraw )
{
	EnableUserDraw( mbUserDraw );
}

void HatchingLB::Fill( const XHatchList* pList )
{
	mpList = (XHatchList*)pList;
	XHatchEntry* pEntry;
	long nCount = pList->Count();

	SetUpdateMode( sal_False );

	if( mbUserDraw )
	{
		for( long i = 0; i < nCount; i++ )
            InsertEntry( pList->GetHatch( i )->GetName() );
	}
	else
	{
		for( long i = 0; i < nCount; i++ )
		{
            pEntry = pList->GetHatch( i );
			Bitmap* pBitmap = pList->GetBitmap( i );
			if( pBitmap )
				InsertEntry( pEntry->GetName(), *pBitmap );
			else
				InsertEntry( pEntry->GetName() );
		}
	}

	SetUpdateMode( sal_True );
}

void HatchingLB::UserDraw( const UserDrawEvent& rUDEvt )
{
	if( mpList != NULL )
	{
		// Draw gradient with borderrectangle
		const Rectangle& rDrawRect = rUDEvt.GetRect();
		Rectangle aRect( rDrawRect.nLeft+1, rDrawRect.nTop+1, rDrawRect.nLeft+33, rDrawRect.nBottom-1 );

        sal_Int32 nId = rUDEvt.GetItemId();
		if( nId >= 0 && nId <= mpList->Count() )
		{
			OutputDevice* pDevice = rUDEvt.GetDevice();

			sal_uIntPtr nOldDrawMode = pDevice->GetDrawMode();
		    pDevice->SetDrawMode( GetSettings().GetStyleSettings().GetHighContrastMode() ? OUTPUT_DRAWMODE_CONTRAST : OUTPUT_DRAWMODE_COLOR );

            XHatch& rXHatch = mpList->GetHatch( rUDEvt.GetItemId() )->GetHatch();
			MapMode aMode( MAP_100TH_MM );
			Hatch aHatch( (HatchStyle) rXHatch.GetHatchStyle(),
						  rXHatch.GetColor(),
						  rUDEvt.GetDevice()->LogicToPixel( Point( rXHatch.GetDistance(), 0 ), aMode ).X(),
						  (sal_uInt16)rXHatch.GetAngle() );
			const Polygon aPolygon( aRect );
			const PolyPolygon aPolypoly( aPolygon );
			pDevice->DrawHatch( aPolypoly, aHatch );

			pDevice->SetLineColor( COL_BLACK );
			pDevice->SetFillColor();
			pDevice->DrawRect( aRect );

			pDevice->SetDrawMode( nOldDrawMode );

			// Draw name
            pDevice->DrawText( Point( aRect.nRight+7, aRect.nTop-1 ), mpList->GetHatch( rUDEvt.GetItemId() )->GetName() );
		}
	}
}

/************************************************************************/

void HatchingLB::Append( XHatchEntry* pEntry, Bitmap* pBmp )
{
	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp );
	else
		InsertEntry( pEntry->GetName() );
}

/************************************************************************/

void HatchingLB::Modify( XHatchEntry* pEntry, sal_uInt16 nPos, Bitmap* pBmp )
{
	RemoveEntry( nPos );

	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp, nPos );
	else
		InsertEntry( pEntry->GetName(), nPos );
}

/************************************************************************/

void HatchingLB::SelectEntryByList( const XHatchList* pList, const String& rStr,
									const XHatch& rHatch, sal_uInt16 nDist )
{
	long nCount = pList->Count();
	XHatchEntry* pEntry;
	sal_Bool bFound = sal_False;
	String aStr;

	long i;
	for( i = 0; i < nCount && !bFound; i++ )
	{
        pEntry = pList->GetHatch( i );

		aStr = pEntry->GetName();

		if( rStr == aStr && rHatch == pEntry->GetHatch() )
			bFound = sal_True;
	}
	if( bFound )
		SelectEntryPos( (sal_uInt16) ( i - 1 + nDist ) );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

void FillAttrLB::Fill( const XHatchList* pList )
{
	long nCount = pList->Count();
	XHatchEntry* pEntry;
	ListBox::SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pList->GetHatch( i );
		Bitmap* pBitmap = pList->GetBitmap( i );
		if( pBitmap )
			ListBox::InsertEntry( pEntry->GetName(), *pBitmap );
		else
			InsertEntry( pEntry->GetName() );
	}
	ListBox::SetUpdateMode( sal_True );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

GradientLB::GradientLB( Window* pParent, ResId Id, sal_Bool bUserDraw /*= sal_True*/ )
: ListBox( pParent, Id ),
  mpList(NULL),
  mbUserDraw( bUserDraw )
{
	EnableUserDraw( mbUserDraw);
}

GradientLB::GradientLB( Window* pParent, WinBits aWB, sal_Bool bUserDraw /*= sal_True*/ )
: ListBox( pParent, aWB ),
  mpList(NULL),
  mbUserDraw( bUserDraw )
{
	EnableUserDraw( mbUserDraw );
}

void GradientLB::Fill( const XGradientList* pList )
{
	mpList = (XGradientList*)pList;
	XGradientEntry* pEntry;
	long nCount = pList->Count();

	SetUpdateMode( sal_False );

	if( mbUserDraw )
	{
		for( long i = 0; i < nCount; i++ )
            InsertEntry( pList->GetGradient( i )->GetName() );
	}
	else
	{
		for( long i = 0; i < nCount; i++ )
		{
            pEntry = pList->GetGradient( i );
			Bitmap* pBitmap = pList->GetBitmap( i );
			if( pBitmap )
				InsertEntry( pEntry->GetName(), *pBitmap );
			else
				InsertEntry( pEntry->GetName() );
		}
	}

	SetUpdateMode( sal_True );
}

void GradientLB::UserDraw( const UserDrawEvent& rUDEvt )
{
	if( mpList != NULL )
	{
		// Draw gradient with borderrectangle
		const Rectangle& rDrawRect = rUDEvt.GetRect();
		Rectangle aRect( rDrawRect.nLeft+1, rDrawRect.nTop+1, rDrawRect.nLeft+33, rDrawRect.nBottom-1 );

        sal_Int32 nId = rUDEvt.GetItemId();
		if( nId >= 0 && nId <= mpList->Count() )
		{
			OutputDevice* pDevice = rUDEvt.GetDevice();

            XGradient& rXGrad = mpList->GetGradient( rUDEvt.GetItemId() )->GetGradient();
			Gradient aGradient( (GradientStyle) rXGrad.GetGradientStyle(), rXGrad.GetStartColor(), rXGrad.GetEndColor() );
			aGradient.SetAngle( (sal_uInt16)rXGrad.GetAngle() );
			aGradient.SetBorder( rXGrad.GetBorder() );
			aGradient.SetOfsX( rXGrad.GetXOffset() );
			aGradient.SetOfsY( rXGrad.GetYOffset() );
			aGradient.SetStartIntensity( rXGrad.GetStartIntens() );
			aGradient.SetEndIntensity( rXGrad.GetEndIntens() );
			aGradient.SetSteps( 255 );
            
            // #i76307# always paint the preview in LTR, because this is what the document does
            Window* pWin = dynamic_cast<Window*>(pDevice);
            if( pWin && pWin->IsRTLEnabled() && Application::GetSettings().GetLayoutRTL())
            {
                long nWidth = pDevice->GetOutputSize().Width();

                pWin->EnableRTL( sal_False );
                
                Rectangle aMirrorRect( Point( nWidth - aRect.Left() - aRect.GetWidth(), aRect.Top() ),
                                       aRect.GetSize() );

                pDevice->DrawGradient( aMirrorRect, aGradient );

                pWin->EnableRTL( sal_True );
            }
            else
                pDevice->DrawGradient( aRect, aGradient );

			pDevice->SetLineColor( COL_BLACK );
			pDevice->SetFillColor();
			pDevice->DrawRect( aRect );

			// Draw name
            pDevice->DrawText( Point( aRect.nRight+7, aRect.nTop-1 ), mpList->GetGradient( rUDEvt.GetItemId() )->GetName() );
		}
	}
}

/************************************************************************/

void GradientLB::Append( XGradientEntry* pEntry, Bitmap* pBmp )
{
	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp );
	else
		InsertEntry( pEntry->GetName() );
}

/************************************************************************/

void GradientLB::Modify( XGradientEntry* pEntry, sal_uInt16 nPos, Bitmap* pBmp )
{
	RemoveEntry( nPos );

	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp, nPos );
	else
		InsertEntry( pEntry->GetName(), nPos );
}

/************************************************************************/

void GradientLB::SelectEntryByList( const XGradientList* pList, const String& rStr,
								const XGradient& rGradient, sal_uInt16 nDist )
{
	long nCount = pList->Count();
	XGradientEntry* pEntry;
	sal_Bool bFound = sal_False;
	String aStr;

	long i;
	for( i = 0; i < nCount && !bFound; i++ )
	{
        pEntry = pList->GetGradient( i );

		aStr = pEntry->GetName();

		if( rStr == aStr && rGradient == pEntry->GetGradient() )
			bFound = sal_True;
	}
	if( bFound )
		SelectEntryPos( (sal_uInt16) ( i - 1 + nDist ) );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

void FillAttrLB::Fill( const XGradientList* pList )
{
	long nCount = pList->Count();
	XGradientEntry* pEntry;
	ListBox::SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pList->GetGradient( i );
		Bitmap* pBitmap = pList->GetBitmap( i );
		if( pBitmap )
			ListBox::InsertEntry( pEntry->GetName(), *pBitmap );
		else
			InsertEntry( pEntry->GetName() );
	}
	ListBox::SetUpdateMode( sal_True );
}

/*************************************************************************
|*
|*	Konstruktor von BitmapLB
|*
\************************************************************************/

BitmapLB::BitmapLB(Window* pParent, ResId Id, bool bUserDraw /*= false*/ )
:   ListBox(pParent, Id),
    maVD(),
    maBitmapEx(),
    mpList(NULL),
    mbUserDraw(bUserDraw)
{
	maVD.SetOutputSizePixel(Size(32, 16));
	EnableUserDraw(mbUserDraw);
}

/************************************************************************/

void BitmapLB::SetVirtualDevice()
{
	if(maBitmapEx.GetSizePixel().Width() > 8 || maBitmapEx.GetSizePixel().Height() > 8)
	{
		maVD.DrawBitmapEx(Point(0, 0), Size(32, 16), maBitmapEx);
	}
	else
	{
		maVD.DrawBitmapEx(Point(0,  0), maBitmapEx);
		maVD.DrawBitmapEx(Point(8,  0), maBitmapEx);
		maVD.DrawBitmapEx(Point(16, 0), maBitmapEx);
		maVD.DrawBitmapEx(Point(24, 0), maBitmapEx);
		maVD.DrawBitmapEx(Point(0,  8), maBitmapEx);
		maVD.DrawBitmapEx(Point(8,  8), maBitmapEx);
		maVD.DrawBitmapEx(Point(16, 8), maBitmapEx);
		maVD.DrawBitmapEx(Point(24, 8), maBitmapEx);
	}
}

/************************************************************************/

void BitmapLB::Fill(const XBitmapList* pList)
{
	mpList = (XBitmapList*)pList;
	XBitmapEntry* pEntry;
	const long nCount(pList->Count());

	SetUpdateMode(false);

	if(mbUserDraw)
	{
		for(long i(0); i < nCount; i++)
        {
            InsertEntry(pList->GetBitmap(i)->GetName());
        }
	}
	else
	{
		for(long i(0); i < nCount; i++)
		{
            pEntry = pList->GetBitmap(i);
			maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
			SetVirtualDevice();
			InsertEntry(pEntry->GetName(), maVD.GetBitmap(Point(0, 2), Size(32, 12)));
		}
	}

	SetUpdateMode(true);
}

void BitmapLB::UserDraw(const UserDrawEvent& rUDEvt)
{
	if(mpList)
	{
		// Draw bitmap
		const Rectangle& rDrawRect = rUDEvt.GetRect();
		const Rectangle aRect(rDrawRect.nLeft + 1, rDrawRect.nTop + 1, rDrawRect.nLeft + 33, rDrawRect.nBottom - 1);
        const sal_Int32 nId(rUDEvt.GetItemId());
		
        if(nId >= 0 && nId <= mpList->Count())
		{
			const Rectangle aClipRect(rDrawRect.nLeft + 1, rDrawRect.nTop + 1, rDrawRect.nRight - 1, rDrawRect.nBottom - 1);
			OutputDevice* pDevice = rUDEvt.GetDevice();
			pDevice->SetClipRegion(Region(aClipRect));
            maBitmapEx = mpList->GetBitmap(nId)->GetGraphicObject().GetGraphic().GetBitmapEx();
			long nPosBaseX = aRect.nLeft;
			long nPosBaseY = aRect.nTop;

			if(maBitmapEx.GetSizePixel().Width() > 8 || maBitmapEx.GetSizePixel().Height() > 8)
			{
				pDevice->DrawBitmapEx(Point(nPosBaseX, nPosBaseY), Size(32, 16), maBitmapEx);
			}
			else
			{
				pDevice->DrawBitmapEx(Point(nPosBaseX+ 0, nPosBaseY+0 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+ 8, nPosBaseY+0 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+16, nPosBaseY+0 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+24, nPosBaseY+0 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+ 0, nPosBaseY+8 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+ 8, nPosBaseY+8 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+16, nPosBaseY+8 ), maBitmapEx);
				pDevice->DrawBitmapEx(Point(nPosBaseX+24, nPosBaseY+8 ), maBitmapEx);
			}

			pDevice->SetClipRegion();

			// Draw name
            pDevice->DrawText(Point(aRect.nRight + 7, aRect.nTop - 1), mpList->GetBitmap(nId)->GetName());
		}
	}
}

/************************************************************************/

void BitmapLB::Append(XBitmapEntry* pEntry, BitmapEx* pBmpEx)
{
	if(pBmpEx)
	{
		maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
		SetVirtualDevice();
		InsertEntry(pEntry->GetName(), maVD.GetBitmap(Point(0, 2), Size(32, 12)));
	}
	else
    {
		InsertEntry(pEntry->GetName());
    }
}

/************************************************************************/

void BitmapLB::Modify(XBitmapEntry* pEntry, sal_uInt16 nPos, BitmapEx* pBmpEx)
{
	RemoveEntry(nPos);

	if(pBmpEx)
	{
		maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
		SetVirtualDevice();
		InsertEntry(pEntry->GetName(), maVD.GetBitmap(Point(0, 2), Size(32, 12)), nPos);
	}
	else
    {
		InsertEntry(pEntry->GetName());
    }
}

/************************************************************************/

void BitmapLB::SelectEntryByList(const XBitmapList* pList, const String& rStr)
{
	const long nCount(pList->Count());
	XBitmapEntry* pEntry;
	bool bFound(false);
    long i(0);

	for(i = 0; i < nCount && !bFound; i++)
	{
        pEntry = pList->GetBitmap(i);
		const String aStr(pEntry->GetName());

		if(rStr == aStr)
		{
			bFound = true;
		}
	}

    if(bFound)
    {
		SelectEntryPos((sal_uInt16)(i - 1));
    }
}

/*************************************************************************
|*
|*	Konstruktor von FillAttrLB
|*
\************************************************************************/

FillAttrLB::FillAttrLB( Window* pParent, ResId Id ) 
:   ColorListBox(pParent, Id),
    maVD(),
    maBitmapEx()
{
	maVD.SetOutputSizePixel(Size(32, 16));
}

/************************************************************************/

FillAttrLB::FillAttrLB(Window* pParent, WinBits aWB) 
:   ColorListBox(pParent, aWB)
{
	maVD.SetOutputSizePixel(Size(32, 16));
}

/************************************************************************/

void FillAttrLB::SetVirtualDevice()
{
    maVD.Erase();

    if(maBitmapEx.GetSizePixel().Width() > 8 || maBitmapEx.GetSizePixel().Height() > 8)
	{
		maVD.DrawBitmapEx(Point(0, 0), Size(32, 16), maBitmapEx);
	}
	else
	{
		maVD.DrawBitmapEx(Point(0,  0), maBitmapEx);
		maVD.DrawBitmapEx(Point(8,  0), maBitmapEx);
		maVD.DrawBitmapEx(Point(16, 0), maBitmapEx);
		maVD.DrawBitmapEx(Point(24, 0), maBitmapEx);
		maVD.DrawBitmapEx(Point(0,  8), maBitmapEx);
		maVD.DrawBitmapEx(Point(8,  8), maBitmapEx);
		maVD.DrawBitmapEx(Point(16, 8), maBitmapEx);
		maVD.DrawBitmapEx(Point(24, 8), maBitmapEx);
	}
}

/************************************************************************/

void FillAttrLB::Fill(const XBitmapList* pList)
{
	const long nCount(pList->Count());
	XBitmapEntry* pEntry;
	
    ListBox::SetUpdateMode(false);

	for(long i(0); i < nCount; i++)
	{
        pEntry = pList->GetBitmap( i );
		maBitmapEx = pEntry->GetGraphicObject().GetGraphic().GetBitmapEx();
		SetVirtualDevice();
		ListBox::InsertEntry(pEntry->GetName(), maVD.GetBitmap(Point(0, 2), Size(32, 12)));
	}

    ListBox::SetUpdateMode(true);
}

/************************************************************************/

void FillAttrLB::SelectEntryByList( const XBitmapList* pList, const String& rStr)
{
	const long nCount(pList->Count());
	XBitmapEntry* pEntry;
	bool bFound(false);
	long i(0);

    for(i = 0; i < nCount && !bFound; i++)
	{
        pEntry = pList->GetBitmap(i);
		const String aStr(pEntry->GetName());

		if(rStr == aStr)
		{
			bFound = true;
		}
	}

    if(bFound)
    {
		SelectEntryPos((sal_uInt16)(i - 1));
    }
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

void FillTypeLB::Fill()
{
	SetUpdateMode( sal_False );
	InsertEntry( String( SVX_RES( RID_SVXSTR_INVISIBLE ) ) );
	InsertEntry( String( SVX_RES( RID_SVXSTR_COLOR ) ) );
	InsertEntry( String( SVX_RES( RID_SVXSTR_GRADIENT ) ) );
	InsertEntry( String( SVX_RES( RID_SVXSTR_HATCH ) ) );
	InsertEntry( String( SVX_RES( RID_SVXSTR_BITMAP ) ) );
	SetUpdateMode( sal_True );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

void LineLB::Fill( const XDashList* pList )
{
	long nCount = pList->Count();
	XDashEntry* pEntry;
	SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pList->GetDash( i );
		Bitmap* pBitmap = const_cast<XDashList*>(pList)->CreateBitmapForUI( i );
		if( pBitmap )
		{
			InsertEntry( pEntry->GetName(), *pBitmap );
			delete pBitmap;
		}
		else
			InsertEntry( pEntry->GetName() );
	}
	SetUpdateMode( sal_True );
}

void LineLB::FillStyles()
{
	ResMgr& rMgr = DIALOG_MGR();

	// Linienstile
	Clear();
	InsertEntry( String( ResId( RID_SVXSTR_INVISIBLE, rMgr ) ) );

	const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();
	Bitmap aBitmap ( SVX_RES ( RID_SVXCTRL_LINECTRL ) );
	Color aSourceColors[2];
	Color aDestColors[2];

	aSourceColors[0] = Color( COL_WHITE );
	aSourceColors[1] = Color( COL_BLACK );

	aDestColors[0] = rStyles.GetFieldColor();
	aDestColors[1] = rStyles.GetFieldTextColor();

	aBitmap.Replace ( aSourceColors, aDestColors, 2 );
	Image aSolidLine ( aBitmap );
	InsertEntry( String( ResId( RID_SVXSTR_SOLID, rMgr ) ), aSolidLine );
}

/************************************************************************/

void LineLB::Append( XDashEntry* pEntry, Bitmap* pBmp )
{
	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp );
	else
		InsertEntry( pEntry->GetName() );
}

/************************************************************************/

void LineLB::Modify( XDashEntry* pEntry, sal_uInt16 nPos, Bitmap* pBmp )
{
	RemoveEntry( nPos );

	if( pBmp )
		InsertEntry( pEntry->GetName(), *pBmp, nPos );
	else
		InsertEntry( pEntry->GetName(), nPos );
}

/************************************************************************/

void LineLB::SelectEntryByList( const XDashList* pList, const String& rStr,
								const XDash& rDash, sal_uInt16 nDist )
{
	long nCount = pList->Count();
	XDashEntry* pEntry;
	sal_Bool bFound = sal_False;
	String aStr;
	XDash aDash;

	long i;
	for( i = 0; i < nCount && !bFound; i++ )
	{
        pEntry = pList->GetDash( i );

		aStr = pEntry->GetName();
		aDash = pEntry->GetDash();

		if( rStr == aStr && rDash == aDash )
			bFound = sal_True;
	}
	if( bFound )
		SelectEntryPos( (sal_uInt16) ( i - 1 + nDist ) );
}

/*************************************************************************
|*
|*	Fuellt die Listbox (vorlaeufig) mit Strings
|*
\************************************************************************/

void LineEndLB::Fill( const XLineEndList* pList, sal_Bool bStart )
{
	long nCount = pList->Count();
	XLineEndEntry* pEntry;
	VirtualDevice aVD;
	SetUpdateMode( sal_False );

	for( long i = 0; i < nCount; i++ )
	{
        pEntry = pList->GetLineEnd( i );
		Bitmap* pBitmap = const_cast<XLineEndList*>(pList)->CreateBitmapForUI( i );
		if( pBitmap )
		{
			Size aBmpSize( pBitmap->GetSizePixel() );
			aVD.SetOutputSizePixel( aBmpSize, sal_False );
			aVD.DrawBitmap( Point(), *pBitmap );
			InsertEntry( pEntry->GetName(),
				aVD.GetBitmap( bStart ? Point() : Point( aBmpSize.Width() / 2, 0 ),
					Size( aBmpSize.Width() / 2, aBmpSize.Height() ) ) );

			delete pBitmap;
		}
		else
			InsertEntry( pEntry->GetName() );
	}
	SetUpdateMode( sal_True );
}

/************************************************************************/

void LineEndLB::Append( XLineEndEntry* pEntry, Bitmap* pBmp,
						sal_Bool bStart )
{
	if( pBmp )
	{
		VirtualDevice aVD;
		Size aBmpSize( pBmp->GetSizePixel() );

		aVD.SetOutputSizePixel( aBmpSize, sal_False );
		aVD.DrawBitmap( Point(), *pBmp );
		InsertEntry( pEntry->GetName(),
			aVD.GetBitmap( bStart ? Point() : Point( aBmpSize.Width() / 2, 0 ),
				Size( aBmpSize.Width() / 2, aBmpSize.Height() ) ) );
	}
	else
		InsertEntry( pEntry->GetName() );
}

/************************************************************************/

void LineEndLB::Modify( XLineEndEntry* pEntry, sal_uInt16 nPos, Bitmap* pBmp,
						sal_Bool bStart )
{
	RemoveEntry( nPos );

	if( pBmp )
	{
		VirtualDevice aVD;
		Size aBmpSize( pBmp->GetSizePixel() );

		aVD.SetOutputSizePixel( aBmpSize, sal_False );
		aVD.DrawBitmap( Point(), *pBmp );
		InsertEntry( pEntry->GetName(),
			aVD.GetBitmap( bStart ? Point() : Point( aBmpSize.Width() / 2, 0 ),
				Size( aBmpSize.Width() / 2, aBmpSize.Height() ) ), nPos );
	}
	else
		InsertEntry( pEntry->GetName(), nPos );
}

//////////////////////////////////////////////////////////////////////////////

void SvxPreviewBase::InitSettings(bool bForeground, bool bBackground)
{
	const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

	if(bForeground)
	{
		svtools::ColorConfig aColorConfig;
		Color aTextColor(aColorConfig.GetColorValue(svtools::FONTCOLOR).nColor);

		if(IsControlForeground())
        {
			aTextColor = GetControlForeground();
        }

		getBufferDevice().SetTextColor(aTextColor);
	}

	if(bBackground)
	{
		if(IsControlBackground())
        {
			getBufferDevice().SetBackground(GetControlBackground());
        }
		else
        {
			getBufferDevice().SetBackground(rStyleSettings.GetWindowColor());
        }
	}

    // do not paint background self, it gets painted buffered
	SetControlBackground();
    SetBackground();

    Invalidate();
}

SvxPreviewBase::SvxPreviewBase( Window* pParent, const ResId& rResId )
:	Control( pParent, rResId ),
	mpModel( new SdrModel() ),
    mpBufferDevice( new VirtualDevice(*this) )
{
	//	Draw the control's border as a flat thin black line.
    SetBorderStyle(WINDOW_BORDER_MONO);
    SetDrawMode( GetSettings().GetStyleSettings().GetHighContrastMode() ? OUTPUT_DRAWMODE_CONTRAST : OUTPUT_DRAWMODE_COLOR );
    SetMapMode(MAP_100TH_MM);

    // init model
	mpModel->GetItemPool().FreezeIdRanges();
}

SvxPreviewBase::~SvxPreviewBase()
{
	delete mpModel;
    delete mpBufferDevice;
}

void SvxPreviewBase::LocalPrePaint()
{
    // init BufferDevice
    if(mpBufferDevice->GetOutputSizePixel() != GetOutputSizePixel())
    {
        mpBufferDevice->SetDrawMode(GetDrawMode());
        mpBufferDevice->SetSettings(GetSettings());
        mpBufferDevice->SetAntialiasing(GetAntialiasing());
        mpBufferDevice->SetOutputSizePixel(GetOutputSizePixel());
	    mpBufferDevice->SetMapMode(GetMapMode());
    }

    mpBufferDevice->Erase();
}

void SvxPreviewBase::LocalPostPaint()
{
    // copy to front (in pixel mode)
	const bool bWasEnabledSrc(mpBufferDevice->IsMapModeEnabled());
	const bool bWasEnabledDst(IsMapModeEnabled());
	const Point aEmptyPoint;
	
    mpBufferDevice->EnableMapMode(false);
    EnableMapMode(false);
	
    DrawOutDev(
        aEmptyPoint, GetOutputSizePixel(), 
        aEmptyPoint, GetOutputSizePixel(), 
        *mpBufferDevice);
	
    mpBufferDevice->EnableMapMode(bWasEnabledSrc);
    EnableMapMode(bWasEnabledDst);
}

void SvxPreviewBase::StateChanged(StateChangedType nType)
{
	Control::StateChanged(nType);

    if(STATE_CHANGE_CONTROLFOREGROUND == nType)
    {
		InitSettings(true, false);
    }
	else if(STATE_CHANGE_CONTROLBACKGROUND == nType)
    {
		InitSettings(false, true);
    }
}

void SvxPreviewBase::DataChanged(const DataChangedEvent& rDCEvt)
{
    SetDrawMode(GetSettings().GetStyleSettings().GetHighContrastMode() ? OUTPUT_DRAWMODE_CONTRAST : OUTPUT_DRAWMODE_COLOR);

    if((DATACHANGED_SETTINGS == rDCEvt.GetType()) && (rDCEvt.GetFlags() & SETTINGS_STYLE))
    {
		InitSettings(true, true);
    }
	else
    {
		Control::DataChanged(rDCEvt);
    }
}

/*************************************************************************
|*
|*    SvxXLinePreview::SvxXLinePreview()
|*
*************************************************************************/

SvxXLinePreview::SvxXLinePreview( Window* pParent, const ResId& rResId )
:	SvxPreviewBase( pParent, rResId ),
	mpLineObjA( 0L ),
	mpLineObjB( 0L ),
	mpLineObjC( 0L ),
	mpGraphic( 0L ),
	mbWithSymbol( sal_False )
{
	const Size aOutputSize(GetOutputSize());
	InitSettings( sal_True, sal_True );

	const sal_Int32 nDistance(500L);
	const sal_Int32 nAvailableLength(aOutputSize.Width() - (4 * nDistance));

	// create DrawObectA
	const sal_Int32 aYPosA(aOutputSize.Height() / 2);
	const basegfx::B2DPoint aPointA1( nDistance,  aYPosA);
	const basegfx::B2DPoint aPointA2( aPointA1.getX() + ((nAvailableLength * 14) / 20), aYPosA );
	basegfx::B2DPolygon aPolygonA;
	aPolygonA.append(aPointA1);
	aPolygonA.append(aPointA2);
	mpLineObjA = new SdrPathObj(getModel(), basegfx::B2DPolyPolygon(aPolygonA));

	// create DrawObectB
	const sal_Int32 aYPosB1((aOutputSize.Height() * 3) / 4);
	const sal_Int32 aYPosB2((aOutputSize.Height() * 1) / 4);
	const basegfx::B2DPoint aPointB1( aPointA2.getX() + nDistance,  aYPosB1);
	const basegfx::B2DPoint aPointB2( aPointB1.getX() + ((nAvailableLength * 2) / 20), aYPosB2 );
	const basegfx::B2DPoint aPointB3( aPointB2.getX() + ((nAvailableLength * 2) / 20), aYPosB1 );
	basegfx::B2DPolygon aPolygonB;
	aPolygonB.append(aPointB1);
	aPolygonB.append(aPointB2);
	aPolygonB.append(aPointB3);
	mpLineObjB = new SdrPathObj(getModel(), basegfx::B2DPolyPolygon(aPolygonB));
	
	// create DrawObectC
	const basegfx::B2DPoint aPointC1( aPointB3.getX() + nDistance,  aYPosB1);
	const basegfx::B2DPoint aPointC2( aPointC1.getX() + ((nAvailableLength * 1) / 20), aYPosB2 );
	const basegfx::B2DPoint aPointC3( aPointC2.getX() + ((nAvailableLength * 1) / 20), aYPosB1 );
	basegfx::B2DPolygon aPolygonC;
	aPolygonC.append(aPointC1);
	aPolygonC.append(aPointC2);
	aPolygonC.append(aPointC3);
	mpLineObjC = new SdrPathObj(getModel(), basegfx::B2DPolyPolygon(aPolygonC));
}

SvxXLinePreview::~SvxXLinePreview()
{
    deleteSdrObjectSafeAndClearPointer( mpLineObjA );
    deleteSdrObjectSafeAndClearPointer( mpLineObjB );
    deleteSdrObjectSafeAndClearPointer( mpLineObjC );
}

// -----------------------------------------------------------------------

void SvxXLinePreview::SetSymbol(Graphic* p,const Size& s)
{
	mpGraphic = p;
	maSymbolSize = s;
}

// -----------------------------------------------------------------------

void SvxXLinePreview::ResizeSymbol(const Size& s)
{
	if ( s != maSymbolSize )
	{
		maSymbolSize = s;
		Invalidate();
	}
}

// -----------------------------------------------------------------------

void SvxXLinePreview::SetLineAttributes(const SfxItemSet& rItemSet)
{
	// Set ItemSet at objects
	mpLineObjA->SetMergedItemSet(rItemSet);

	// At line joints, do not use arrows
	SfxItemSet aTempSet(rItemSet);
	aTempSet.ClearItem(XATTR_LINESTART);
	aTempSet.ClearItem(XATTR_LINEEND);

	mpLineObjB->SetMergedItemSet(aTempSet);
	mpLineObjC->SetMergedItemSet(aTempSet);
}

// -----------------------------------------------------------------------

void SvxXLinePreview::Paint( const Rectangle& )
{
	LocalPrePaint();

	// paint objects to buffer device
	SdrObjectVector aObjectVector;
	aObjectVector.push_back(mpLineObjA);
	aObjectVector.push_back(mpLineObjB);
	aObjectVector.push_back(mpLineObjC);

	sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
	sdr::contact::DisplayInfo aDisplayInfo;

	// do processing
	aPainter.ProcessDisplay(aDisplayInfo);

	if ( mbWithSymbol && mpGraphic )
	{
    	const Size aOutputSize(GetOutputSize());
		Point aPos = Point( aOutputSize.Width() / 3, aOutputSize.Height() / 2 );
		aPos.X() -= maSymbolSize.Width() / 2;
		aPos.Y() -= maSymbolSize.Height() / 2;
		mpGraphic->Draw( &getBufferDevice(), aPos, maSymbolSize );
	}

	LocalPostPaint();
}

/*************************************************************************
|*
|*    SvxXRectPreview::SvxXRectPreview()
|*
*************************************************************************/

SvxXRectPreview::SvxXRectPreview( Window* pParent, const ResId& rResId )
:	SvxPreviewBase( pParent, rResId ),
	mpRectangleObject(0)
{
	InitSettings(true, true);

	// create RectangleObject
	mpRectangleObject = new SdrRectObj(
		getModel(), 
		basegfx::tools::createScaleB2DHomMatrix(
			GetOutputSize().getWidth(), GetOutputSize().getHeight()));
	//mpRectangleObject->SetModel(&getModel());
}

SvxXRectPreview::~SvxXRectPreview()
{
    deleteSdrObjectSafeAndClearPointer(mpRectangleObject);
}

void SvxXRectPreview::SetAttributes(const SfxItemSet& rItemSet)
{
	mpRectangleObject->SetMergedItemSet(rItemSet, true);
	mpRectangleObject->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXRectPreview::Paint( const Rectangle& )
{
	LocalPrePaint();

    SdrObjectVector aObjectVector;

    aObjectVector.push_back(mpRectangleObject);

    sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
	sdr::contact::DisplayInfo aDisplayInfo;

	aPainter.ProcessDisplay(aDisplayInfo);

    LocalPostPaint();
}

/*************************************************************************
|*
|*    SvxXShadowPreview::SvxXShadowPreview()
|*
*************************************************************************/

SvxXShadowPreview::SvxXShadowPreview( Window* pParent, const ResId& rResId )
:	SvxPreviewBase( pParent, rResId ),
	mpRectangleObject(0),
    mpRectangleShadow(0)
{
	InitSettings(true, true);

    // prepare size
	Size aSize = GetOutputSize();
	aSize.Width() = aSize.Width() / 3;
	aSize.Height() = aSize.Height() / 3;

    // create RectangleObject
	mpRectangleObject = new SdrRectObj(
		getModel(), 
		basegfx::tools::createScaleTranslateB2DHomMatrix(
			aSize.getWidth(), aSize.getHeight(), 
			aSize.getWidth(), aSize.getHeight()));
	//mpRectangleObject->SetModel(&getModel());

    // create ShadowObject
	mpRectangleShadow = new SdrRectObj(
		getModel(), 
		mpRectangleObject->getSdrObjectTransformation());
	//mpRectangleShadow->SetModel(&getModel());
}

SvxXShadowPreview::~SvxXShadowPreview()
{
    deleteSdrObjectSafeAndClearPointer(mpRectangleObject);
    deleteSdrObjectSafeAndClearPointer(mpRectangleShadow);
}

void SvxXShadowPreview::SetRectangleAttributes(const SfxItemSet& rItemSet)
{
	mpRectangleObject->SetMergedItemSet(rItemSet, true);
	mpRectangleObject->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXShadowPreview::SetShadowAttributes(const SfxItemSet& rItemSet)
{
	mpRectangleShadow->SetMergedItemSet(rItemSet, true);
	mpRectangleShadow->SetMergedItem(XLineStyleItem(XLINE_NONE));
}

void SvxXShadowPreview::SetShadowPosition(const Point& rPos) 
{ 
	sdr::legacy::transformSdrObject(*mpRectangleShadow, basegfx::tools::createTranslateB2DHomMatrix(rPos.X(), rPos.Y()));
}

void SvxXShadowPreview::Paint( const Rectangle& )
{
	LocalPrePaint();

    SdrObjectVector aObjectVector;

    aObjectVector.push_back(mpRectangleShadow);
	aObjectVector.push_back(mpRectangleObject);

    sdr::contact::ObjectContactOfObjListPainter aPainter(getBufferDevice(), aObjectVector, 0);
	sdr::contact::DisplayInfo aDisplayInfo;

	aPainter.ProcessDisplay(aDisplayInfo);

    LocalPostPaint();
}

// -----------------------------------------------------------------------
// eof
