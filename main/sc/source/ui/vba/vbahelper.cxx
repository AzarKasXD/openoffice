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


#include <cppuhelper/bootstrap.hxx>
#include <com/sun/star/util/XURLTransformer.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XDesktop.hpp>
#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XMultiComponentFactory.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XIntrospection.hpp>

#include <comphelper/processfactory.hxx>

#include <sfx2/objsh.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/app.hxx>
#include <svl/stritem.hxx>

#include <docuno.hxx>

#include <basic/sbx.hxx>
#include <basic/sbstar.hxx>
#include <rtl/math.hxx>

#include <math.h>
#include "vbahelper.hxx"
#include "tabvwsh.hxx"
#include "transobj.hxx"
#include "scmod.hxx"
#include "vbashape.hxx"
#include "unonames.hxx"
#include "cellsuno.hxx"
using namespace ::com::sun::star;
using namespace ::ooo::vba;

#define POINTTO100THMILLIMETERFACTOR 35.27778
void unoToSbxValue( SbxVariable* pVar, const uno::Any& aValue );

uno::Any sbxToUnoValue( SbxVariable* pVar );


namespace ooo
{
namespace vba
{

const double Millimeter::factor =  35.27778;

uno::Reference< beans::XIntrospectionAccess >
getIntrospectionAccess( const uno::Any& aObject ) throw (uno::RuntimeException)
{
	static uno::Reference< beans::XIntrospection > xIntrospection;
	if( !xIntrospection.is() )
	{
		uno::Reference< lang::XMultiServiceFactory > xFactory( comphelper::getProcessServiceFactory(), uno::UNO_QUERY_THROW );
		xIntrospection.set( xFactory->createInstance( rtl::OUString::createFromAscii("com.sun.star.beans.Introspection") ), uno::UNO_QUERY_THROW );
	}
	return xIntrospection->inspect( aObject );
}

uno::Reference< script::XTypeConverter >
getTypeConverter( const uno::Reference< uno::XComponentContext >& xContext ) throw (uno::RuntimeException)
{
	static uno::Reference< script::XTypeConverter > xTypeConv( xContext->getServiceManager()->createInstanceWithContext( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.script.Converter") ), xContext ), uno::UNO_QUERY_THROW );
	return xTypeConv;
}
// helper method to determine if the view ( calc ) is in print-preview mode
bool isInPrintPreview( SfxViewFrame* pView )
{
	sal_uInt16 nViewNo = SID_VIEWSHELL1 - SID_VIEWSHELL0;
	if ( pView->GetObjectShell()->GetFactory().GetViewFactoryCount() >
nViewNo && !pView->GetObjectShell()->IsInPlaceActive() )
	{
		SfxViewFactory &rViewFactory =
			pView->GetObjectShell()->GetFactory().GetViewFactory(nViewNo);
		if (  pView->GetCurViewId() == rViewFactory.GetOrdinal() )
			return true;
	}
	return false;
}
const ::rtl::OUString REPLACE_CELLS_WARNING(  RTL_CONSTASCII_USTRINGPARAM( "ReplaceCellsWarning"));
const uno::Any&
aNULL()
{
 	static  uno::Any aNULLL = uno::makeAny( uno::Reference< uno::XInterface >() );
	return aNULLL;
}

class PasteCellsWarningReseter
{
private:
	bool bInitialWarningState;
	static uno::Reference< beans::XPropertySet > getGlobalSheetSettings() throw ( uno::RuntimeException )
	{
		static uno::Reference< beans::XPropertySet > xTmpProps( ::comphelper::getProcessServiceFactory(), uno::UNO_QUERY_THROW );
		static uno::Reference<uno::XComponentContext > xContext( xTmpProps->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "DefaultContext" ))), uno::UNO_QUERY_THROW );
		static uno::Reference<lang::XMultiComponentFactory > xServiceManager(
				xContext->getServiceManager(), uno::UNO_QUERY_THROW );
		static uno::Reference< beans::XPropertySet > xProps( xServiceManager->createInstanceWithContext( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.sheet.GlobalSheetSettings" ) ) ,xContext ), uno::UNO_QUERY_THROW );
		return xProps;
	}

	bool getReplaceCellsWarning() throw ( uno::RuntimeException )
	{
		sal_Bool res = sal_False;
		getGlobalSheetSettings()->getPropertyValue( REPLACE_CELLS_WARNING ) >>= res;
		return ( res == sal_True );
	}

	void setReplaceCellsWarning( bool bState ) throw ( uno::RuntimeException )
	{
		getGlobalSheetSettings()->setPropertyValue( REPLACE_CELLS_WARNING, uno::makeAny( bState ) );
	}
public:
	PasteCellsWarningReseter() throw ( uno::RuntimeException )
	{
		bInitialWarningState = getReplaceCellsWarning();
		if ( bInitialWarningState )
			setReplaceCellsWarning( false );
	}
	~PasteCellsWarningReseter()
	{
		if ( bInitialWarningState )
		{
			// don't allow dtor to throw
			try
			{
				setReplaceCellsWarning( true );
			}
			catch ( uno::Exception& /*e*/ ){}
		}
	}
};

void dispatchExecute(css::uno::Reference< css::frame::XModel>& xModel, sal_uInt16 nSlot, SfxCallMode nCall)
{
	ScTabViewShell* pViewShell = getBestViewShell( xModel );
	SfxViewFrame* pViewFrame = NULL;
	if ( pViewShell )
		pViewFrame = pViewShell->GetViewFrame();
	if ( pViewFrame )
	{
		SfxDispatcher* pDispatcher = pViewFrame->GetDispatcher();
		if( pDispatcher )
		{
			pDispatcher->Execute( nSlot , nCall );
		}
	}	
}
		
void
implnPaste()
{
	PasteCellsWarningReseter resetWarningBox;
	ScTabViewShell* pViewShell = getCurrentBestViewShell();
	if ( pViewShell )
	{
		pViewShell->PasteFromSystem();
		pViewShell->CellContentChanged();
	}
}


void
implnCopy()
{
	ScTabViewShell* pViewShell = getCurrentBestViewShell();
	if ( pViewShell )
		pViewShell->CopyToClip(NULL,false,false,true);
}

void 
implnCut()
{
	ScTabViewShell* pViewShell =  getCurrentBestViewShell();
	if ( pViewShell )
		pViewShell->CutToClip( NULL, sal_True );
}

void implnPasteSpecial(sal_uInt16 nFlags,sal_uInt16 nFunction,sal_Bool bSkipEmpty, sal_Bool bTranspose)
{
	PasteCellsWarningReseter resetWarningBox;
	sal_Bool bAsLink(sal_False), bOtherDoc(sal_False);
	InsCellCmd eMoveMode = INS_NONE;

	ScTabViewShell* pTabViewShell = ScTabViewShell::GetActiveViewShell();
	if ( !pTabViewShell ) 
		// none active, try next best
		pTabViewShell = getCurrentBestViewShell();
	if ( pTabViewShell )
	{
		ScViewData* pView = pTabViewShell->GetViewData();	
		Window* pWin = ( pView != NULL ) ? pView->GetActiveWin() : NULL;
		if ( pView && pWin )
		{
			if ( bAsLink && bOtherDoc )
				pTabViewShell->PasteFromSystem(0);//SOT_FORMATSTR_ID_LINK
			else 
			{
				ScTransferObj* pOwnClip = ScTransferObj::GetOwnClipboard( pWin );
				ScDocument* pDoc = NULL; 
				if ( pOwnClip )
					pDoc = pOwnClip->GetDocument();	
				pTabViewShell->PasteFromClip( nFlags, pDoc,
					nFunction, bSkipEmpty, bTranspose, bAsLink,
					eMoveMode, IDF_NONE, sal_True );
				pTabViewShell->CellContentChanged();
			}
		}
	}

}

 uno::Reference< frame::XModel > 
getCurrentDocument() throw (uno::RuntimeException)
{
	uno::Reference< frame::XModel > xModel;
	SbxObject* pBasic = dynamic_cast< SbxObject* > ( SFX_APP()->GetBasic() );
	SbxObject* basicChosen =  pBasic ;
	if ( basicChosen == NULL)
	{
		OSL_TRACE("getModelFromBasic() StarBASIC* is NULL" );
		return xModel;
	}    
    SbxObject* p = pBasic;
    SbxObject* pParent = p->GetParent();
    SbxObject* pParentParent = pParent ? pParent->GetParent() : NULL;

    if( pParentParent )
    {
        basicChosen = pParentParent;
    }
    else if( pParent )
    {
        basicChosen = pParent;
    }


    uno::Any aModel; 
    SbxVariable *pCompVar = basicChosen->Find(  UniString(RTL_CONSTASCII_USTRINGPARAM("ThisComponent")), SbxCLASS_OBJECT );

	if ( pCompVar )
	{
		aModel = sbxToUnoValue( pCompVar );
		if ( sal_False == ( aModel >>= xModel ) ||
			!xModel.is() )
		{
			// trying last gasp try the current component
			uno::Reference< beans::XPropertySet > xProps( ::comphelper::getProcessServiceFactory(), uno::UNO_QUERY_THROW );
			// test if vba service is present
			uno::Reference< uno::XComponentContext > xCtx( xProps->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "DefaultContext" ))), uno::UNO_QUERY_THROW );
			uno::Reference<lang::XMultiComponentFactory > xSMgr( xCtx->getServiceManager(), uno::UNO_QUERY_THROW );
			uno::Reference< frame::XDesktop > xDesktop (xSMgr->createInstanceWithContext(::rtl::OUString::createFromAscii("com.sun.star.frame.Desktop"), xCtx), uno::UNO_QUERY_THROW );
			xModel.set( xDesktop->getCurrentComponent(), uno::UNO_QUERY );
			if ( !xModel.is() )
			{
				throw uno::RuntimeException( 
					rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "Can't extract model from basic ( its obviously not set yet ) therefore don't know the currently selected document") ), uno::Reference< uno::XInterface >() );
			}
			return xModel;
		}
		else
		{
			OSL_TRACE("Have model ThisComponent points to url %s",
			::rtl::OUStringToOString( xModel->getURL(),
				RTL_TEXTENCODING_ASCII_US ).pData->buffer );
		}
	}
	else
	{
		OSL_TRACE("Failed to get ThisComponent");
		throw uno::RuntimeException( 
			rtl::OUString( 
				RTL_CONSTASCII_USTRINGPARAM(
					"Can't determine the currently selected document") ),
			uno::Reference< uno::XInterface >() );
	}
	return xModel;
}

ScDocShell* 
getDocShell( css::uno::Reference< css::frame::XModel>& xModel ) 
{
	uno::Reference< uno::XInterface > xIf( xModel, uno::UNO_QUERY_THROW );
	ScModelObj* pModel = dynamic_cast< ScModelObj* >( xIf.get() );
	ScDocShell* pDocShell = NULL;
	if ( pModel )
		pDocShell = (ScDocShell*)pModel->GetEmbeddedObject();
	return pDocShell;

}

ScTabViewShell* 
getBestViewShell(  css::uno::Reference< css::frame::XModel>& xModel )
{
	ScDocShell* pDocShell = getDocShell( xModel );
	if ( pDocShell )
		return pDocShell->GetBestViewShell();
	return NULL;
}

ScTabViewShell* 
getCurrentBestViewShell()
{ 
	uno::Reference< frame::XModel > xModel = getCurrentDocument();
	return getBestViewShell( xModel );
}

SfxViewFrame* 
getCurrentViewFrame()
{
	ScTabViewShell* pViewShell = getCurrentBestViewShell();	
	if ( pViewShell )
		return pViewShell->GetViewFrame();
	return NULL;
}

sal_Int32 
OORGBToXLRGB( sal_Int32 nCol )
{
	sal_Int32 nRed = nCol;
	nRed &= 0x00FF0000;
	nRed >>= 16;
	sal_Int32 nGreen = nCol;
	nGreen &= 0x0000FF00;
	nGreen >>= 8;
	sal_Int32 nBlue = nCol;
	nBlue &= 0x000000FF;
	sal_Int32 nRGB =  ( (nBlue << 16) | (nGreen << 8) | nRed );
	return nRGB;
}
sal_Int32 
XLRGBToOORGB( sal_Int32 nCol )
{
	sal_Int32 nBlue = nCol;
	nBlue &= 0x00FF0000;
	nBlue >>= 16;
	sal_Int32 nGreen = nCol;
	nGreen &= 0x0000FF00;
	nGreen >>= 8;
	sal_Int32 nRed = nCol;
	nRed &= 0x000000FF;
	sal_Int32 nRGB =  ( (nRed << 16) | (nGreen << 8) | nBlue );
	return nRGB;
}
uno::Any 
OORGBToXLRGB( const uno::Any& aCol )
{
	sal_Int32 nCol=0;
	aCol >>= nCol;
	nCol = OORGBToXLRGB( nCol );
	return uno::makeAny( nCol );
}
uno::Any 
XLRGBToOORGB(  const uno::Any& aCol )
{
	sal_Int32 nCol=0;
	aCol >>= nCol;
	nCol = XLRGBToOORGB( nCol );
	return uno::makeAny( nCol );
}

void PrintOutHelper( const uno::Any& From, const uno::Any& To, const uno::Any& Copies, const uno::Any& Preview, const uno::Any& /*ActivePrinter*/, const uno::Any& /*PrintToFile*/, const uno::Any& Collate, const uno::Any& PrToFileName, css::uno::Reference< frame::XModel >& xModel, sal_Bool bUseSelection  )
{
	sal_Int32 nTo = 0;
	sal_Int32 nFrom = 0;
	sal_Int16 nCopies = 1;
	sal_Bool bPreview = sal_False;
	sal_Bool bCollate = sal_False;
	sal_Bool bSelection = bUseSelection;
	From >>= nFrom;
	To >>= nTo;
	Copies >>= nCopies;
	Preview >>= bPreview;
	if ( nCopies > 1 ) // Collate only useful when more that 1 copy
		Collate >>= bCollate;

	rtl::OUString sRange(  RTL_CONSTASCII_USTRINGPARAM( "-" ) );
	rtl::OUString sFileName;
	
	if (( nFrom || nTo ) )
	{
		if ( nFrom )
			sRange = ( ::rtl::OUString::valueOf( nFrom ) + sRange );
		if ( nTo )
			sRange += ::rtl::OUString::valueOf( nTo );
	}

	if (  PrToFileName.getValue() )
	{
		PrToFileName >>= sFileName;
	}
	ScTabViewShell* pViewShell = getBestViewShell( xModel );
	SfxViewFrame* pViewFrame = NULL;
	if ( pViewShell )
		pViewFrame = pViewShell->GetViewFrame();
	if ( pViewFrame )
	{
		SfxAllItemSet aArgs( SFX_APP()->GetPool() );
				
		SfxBoolItem sfxCollate( SID_PRINT_COLLATE, bCollate );
		aArgs.Put( sfxCollate, sfxCollate.Which() );
		SfxInt16Item sfxCopies( SID_PRINT_COPIES, nCopies );
		aArgs.Put( sfxCopies, sfxCopies.Which() );
		if ( sFileName.getLength() )
		{
			SfxStringItem sfxFileName( SID_FILE_NAME, sFileName);
			aArgs.Put( sfxFileName, sfxFileName.Which() );
		
		}
		if (  sRange.getLength() )
		{
			SfxStringItem sfxRange( SID_PRINT_PAGES, sRange );
			aArgs.Put( sfxRange, sfxRange.Which() );
		}
		SfxBoolItem sfxSelection( SID_SELECTION, bSelection );
		aArgs.Put( sfxSelection, sfxSelection.Which() );
		SfxBoolItem sfxAsync( SID_ASYNCHRON, sal_False );
		aArgs.Put( sfxAsync, sfxAsync.Which() );
		SfxDispatcher* pDispatcher = pViewFrame->GetDispatcher();

		if ( pDispatcher )
		{
			if ( bPreview )
			{
				if ( !pViewFrame->GetFrame().IsInPlace() ) 	
				{
					SC_MOD()->InputEnterHandler();
					pViewFrame->GetDispatcher()->Execute( SID_VIEWSHELL1, SFX_CALLMODE_SYNCHRON );
					while ( isInPrintPreview( pViewFrame ) )
						Application::Yield();
				}
			}
			else
				pDispatcher->Execute( (sal_uInt16)SID_PRINTDOC, (SfxCallMode)SFX_CALLMODE_SYNCHRON, aArgs );
		}
			
	}
	
	// #FIXME #TODO
	// 1 ActivePrinter ( how/can we switch a printer via API? )
	// 2 PrintToFile ( ms behaviour if this option is specified but no 
	//   filename supplied 'PrToFileName' then the user will be prompted )
	// 3 Need to check behaviour of Selected sheets with range ( e.g. From & To
	//    values ) in oOO these options are mutually exclusive
	// 4 There is a pop up to do with transparent objects in the print source
	//   should be able to disable that via configuration for the duration
	//   of this method
}

 void PrintPreviewHelper( const css::uno::Any& /*EnableChanges*/, css::uno::Reference< css::frame::XModel >& xModel )
{
	dispatchExecute( xModel, SID_VIEWSHELL1 );
}

rtl::OUString getAnyAsString( const uno::Any& pvargItem ) throw ( uno::RuntimeException )
{
	uno::Type aType = pvargItem.getValueType();
	uno::TypeClass eTypeClass = aType.getTypeClass();
	rtl::OUString sString;
	switch ( eTypeClass )
	{
		case uno::TypeClass_BOOLEAN:
		{
			sal_Bool bBool = sal_False;
			pvargItem >>= bBool;
			sString = rtl::OUString::valueOf( bBool );
			break;
		}
		case uno::TypeClass_STRING:
			pvargItem >>= sString;
			break;
		case uno::TypeClass_FLOAT:
			{
				float aFloat = 0;
				pvargItem >>= aFloat;
				sString = rtl::OUString::valueOf( aFloat );
				break;
			}
		case uno::TypeClass_DOUBLE:
			{
				double aDouble = 0;
				pvargItem >>= aDouble;
				sString = rtl::OUString::valueOf( aDouble );
				break;
			}
		case uno::TypeClass_SHORT:
		case uno::TypeClass_LONG:
		case uno::TypeClass_BYTE:
			{
				sal_Int32 aNum = 0;
				pvargItem >>= aNum;
				sString = rtl::OUString::valueOf( aNum );
				break;
			}

		case uno::TypeClass_HYPER:
			{
				sal_Int64 aHyper = 0;
				pvargItem >>= aHyper;
				sString = rtl::OUString::valueOf( aHyper );
				break;
			}
		default:
       			throw uno::RuntimeException( rtl::OUString::createFromAscii( "Invalid type, can't convert" ), uno::Reference< uno::XInterface >() );
	}
	return sString;
}


rtl::OUString 
ContainerUtilities::getUniqueName( const uno::Sequence< ::rtl::OUString >&  _slist, const rtl::OUString& _sElementName, const ::rtl::OUString& _sSuffixSeparator)
{
	return getUniqueName(_slist, _sElementName, _sSuffixSeparator, sal_Int32(2));
}

rtl::OUString 
ContainerUtilities::getUniqueName( const uno::Sequence< rtl::OUString >& _slist, const rtl::OUString _sElementName, const rtl::OUString& _sSuffixSeparator, sal_Int32 _nStartSuffix)
{
	sal_Int32 a = _nStartSuffix;
	rtl::OUString scompname = _sElementName;
	bool bElementexists = true;
	sal_Int32 nLen = _slist.getLength();
	if ( nLen == 0 )
		return _sElementName;

	while (bElementexists == true) 
	{
		for (sal_Int32 i = 0; i < nLen; i++)
		{
			if (FieldInList(_slist, scompname) == -1)
			{
				return scompname;
			}
		}
		scompname = _sElementName + _sSuffixSeparator + rtl::OUString::valueOf( a++ );
	}
	return rtl::OUString();
}

sal_Int32 
ContainerUtilities::FieldInList( const uno::Sequence< rtl::OUString >& SearchList, const rtl::OUString& SearchString )
{
	sal_Int32 FieldLen = SearchList.getLength();
	sal_Int32 retvalue = -1;
	for (sal_Int32 i = 0; i < FieldLen; i++) 
	{
		// I wonder why comparing lexicographically is done
		// when its a match is whats interesting?
		//if (SearchList[i].compareTo(SearchString) == 0) 
		if ( SearchList[i].equals( SearchString ) ) 
		{
			retvalue = i;
			break;
		}
	}
	return retvalue;

}
bool NeedEsc(sal_Unicode cCode)
{
	String sEsc(RTL_CONSTASCII_USTRINGPARAM(".^$+\\|{}()"));
	return (STRING_NOTFOUND != sEsc.Search(cCode));
}

rtl::OUString VBAToRegexp(const rtl::OUString &rIn, bool bForLike )
{
	rtl::OUStringBuffer sResult;
	const sal_Unicode *start = rIn.getStr();
	const sal_Unicode *end = start + rIn.getLength();

	int seenright = 0;
	if ( bForLike )
		sResult.append(static_cast<sal_Unicode>('^'));

	while (start < end) 
	{
		switch (*start)
		{
			case '?':
				sResult.append(static_cast<sal_Unicode>('.'));
				start++;
				break;
			case '*':
				sResult.append(rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(".*")));
				start++;
				break;
			case '#':
				sResult.append(rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("[0-9]")));
				start++;
				break;
			case '~':
				sResult.append(static_cast<sal_Unicode>('\\'));
				sResult.append(*(++start));
				start++;
				break;
				// dump the ~ and escape the next characture
			case ']':
				sResult.append(static_cast<sal_Unicode>('\\'));
				sResult.append(*start++);
				break;
			case '[':
				sResult.append(*start++);
				seenright = 0;
				while (start < end && !seenright)
				{
					switch (*start)
					{
						case '[':
						case '?':
						case '*':
						sResult.append(static_cast<sal_Unicode>('\\'));
						sResult.append(*start);
							break;
						case ']':
						sResult.append(*start);
							seenright = 1;
							break;
						case '!':
							sResult.append(static_cast<sal_Unicode>('^'));
							break;
						default:
						if (NeedEsc(*start))
							sResult.append(static_cast<sal_Unicode>('\\'));
						sResult.append(*start);
							break;
					}
					start++;
				}
				break;
			default:
				if (NeedEsc(*start))
					sResult.append(static_cast<sal_Unicode>('\\'));
				sResult.append(*start++);
		}
	}

	if ( bForLike )
		sResult.append(static_cast<sal_Unicode>('$'));

	return sResult.makeStringAndClear( );
}

double getPixelTo100thMillimeterConversionFactor( css::uno::Reference< css::awt::XDevice >& xDevice, sal_Bool bVertical)
{
	double fConvertFactor = 1.0;
	if( bVertical )
	{
		fConvertFactor = xDevice->getInfo().PixelPerMeterY/100000;
	}
	else
	{
		fConvertFactor = xDevice->getInfo().PixelPerMeterX/100000;	
	}
	return fConvertFactor;
}

double PointsToPixels( css::uno::Reference< css::awt::XDevice >& xDevice, double fPoints, sal_Bool bVertical)
{
	double fConvertFactor = getPixelTo100thMillimeterConversionFactor( xDevice, bVertical );
	return fPoints * POINTTO100THMILLIMETERFACTOR * fConvertFactor;
}
double PixelsToPoints( css::uno::Reference< css::awt::XDevice >& xDevice, double fPixels, sal_Bool bVertical)
{
	double fConvertFactor = getPixelTo100thMillimeterConversionFactor( xDevice, bVertical );
	return (fPixels/fConvertFactor)/POINTTO100THMILLIMETERFACTOR;
}

ConcreteXShapeGeometryAttributes::ConcreteXShapeGeometryAttributes( const css::uno::Reference< css::uno::XComponentContext >& xContext, const css::uno::Reference< css::drawing::XShape >& xShape )
{
	m_xShape = new ScVbaShape( xContext, xShape );
}

#define VBA_LEFT "PositionX"
#define VBA_TOP "PositionY"
UserFormGeometryHelper::UserFormGeometryHelper( const uno::Reference< uno::XComponentContext >& /*xContext*/, const uno::Reference< awt::XControl >& xControl )
{
    mxModel.set( xControl->getModel(), uno::UNO_QUERY_THROW );
}
    double UserFormGeometryHelper::getLeft()
    {
	sal_Int32 nLeft = 0;
	mxModel->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( VBA_LEFT ) ) ) >>= nLeft;	
	return Millimeter::getInPoints( nLeft );
    }
    void UserFormGeometryHelper::setLeft( double nLeft )
    {
        mxModel->setPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( VBA_LEFT ) ), uno::makeAny( Millimeter::getInHundredthsOfOneMillimeter( nLeft ) ) );
    }
    double UserFormGeometryHelper::getTop()
    {
	sal_Int32 nTop = 0;
	mxModel->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	VBA_TOP ) ) ) >>= nTop;	
	return Millimeter::getInPoints( nTop );
    }
    void UserFormGeometryHelper::setTop( double nTop )
    {
	mxModel->setPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	VBA_TOP ) ), uno::makeAny( Millimeter::getInHundredthsOfOneMillimeter( nTop ) ) );
    }
    double UserFormGeometryHelper::getHeight()
    {
	sal_Int32 nHeight = 0;
	mxModel->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	SC_UNONAME_CELLHGT ) ) ) >>= nHeight;	
	return Millimeter::getInPoints( nHeight );
    }
    void UserFormGeometryHelper::setHeight( double nHeight )
    {
	mxModel->setPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	SC_UNONAME_CELLHGT ) ), uno::makeAny( Millimeter::getInHundredthsOfOneMillimeter( nHeight ) ) );
    }
    double UserFormGeometryHelper::getWidth()
    {
	sal_Int32 nWidth = 0;
	mxModel->getPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	SC_UNONAME_CELLWID ) ) ) >>= nWidth;	
	return Millimeter::getInPoints( nWidth );
    }
    void UserFormGeometryHelper::setWidth( double nWidth)
    {
	mxModel->setPropertyValue( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( 	SC_UNONAME_CELLWID ) ), uno::makeAny(  Millimeter::getInHundredthsOfOneMillimeter( nWidth ) ) );
    }

SfxItemSet*
ScVbaCellRangeAccess::GetDataSet( ScCellRangeObj* pRangeObj )
{
	SfxItemSet* pDataSet = pRangeObj ? pRangeObj->GetCurrentDataSet( true ) : NULL ;
	return pDataSet;
	
}

} // vba
} // ooo
