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
#include "precompiled_sd.hxx"

#include <com/sun/star/presentation/XPresentation2.hpp>

#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/lang/ServiceNotRegisteredException.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/awt/XDevice.hpp>

#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/presentation/XPresentation2.hpp>

#include <osl/mutex.hxx>
#include <comphelper/serviceinfohelper.hxx>

#include <comphelper/sequence.hxx>

#include <rtl/uuid.h>
#include <rtl/memory.h>
#include <editeng/unofield.hxx>
#include <unomodel.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/bindings.hxx>
#include <vcl/svapp.hxx>
#include <editeng/UnoForbiddenCharsTable.hxx>
#include <svx/svdoutl.hxx>
#include <editeng/forbiddencharacterstable.hxx>
#include <svx/UnoNamespaceMap.hxx>
#include <svx/svdlayer.hxx>
#include <svx/svdsob.hxx>
#include <svx/unoapi.hxx>
#include <svx/unofill.hxx>
#include <svx/unopool.hxx>
#include <svx/svdorect.hxx>
#include <editeng/flditem.hxx>
#include <vos/mutex.hxx>
#include <toolkit/awt/vclxdevice.hxx>
#include <svx/svdpool.hxx>
#include <editeng/unolingu.hxx>
#include <svx/svdpagv.hxx>
#include <svtools/unoimap.hxx>
#include <svx/unoshape.hxx>
#include <editeng/unonrule.hxx>
#include <editeng/eeitem.hxx>

// #99870# Support creation of GraphicObjectResolver and EmbeddedObjectResolver
#include <svx/xmleohlp.hxx>
#include <svx/xmlgrhlp.hxx>
#include "DrawDocShell.hxx"
#include "ViewShellBase.hxx"
#include <UnoDocumentSettings.hxx>

#include <drawdoc.hxx>
#include <glob.hrc>
#include <sdresid.hxx>
#include <sdpage.hxx>

#include <strings.hrc>
#include "unohelp.hxx"
#include <unolayer.hxx>
#include <unoprnms.hxx>
#include <unopage.hxx>
#include <unocpres.hxx>
#include <unoobj.hxx>
#include <stlpool.hxx>
#include <unopback.hxx>
#include <unokywds.hxx>
#include "FrameView.hxx"
#include "ClientView.hxx"
#include "ViewShell.hxx"
#include "app.hrc"
#include <vcl/pdfextoutdevdata.hxx>
#include <com/sun/star/presentation/AnimationEffect.hpp>
#include <com/sun/star/presentation/AnimationSpeed.hpp>
#include <com/sun/star/presentation/ClickAction.hpp>
#include <tools/urlobj.hxx>
#include <svx/sdr/contact/viewobjectcontact.hxx>
#include <svx/sdr/contact/viewcontact.hxx>
#include <svx/sdr/contact/displayinfo.hxx>

#include <com/sun/star/office/XAnnotation.hpp>
#include <com/sun/star/office/XAnnotationAccess.hpp>
#include <com/sun/star/office/XAnnotationEnumeration.hpp>
#include <com/sun/star/geometry/RealPoint2D.hpp>
#include <com/sun/star/util/DateTime.hpp>

#include <drawinglayer/primitive2d/structuretagprimitive2d.hxx>
#include <svx/globaldrawitempool.hxx>

using ::rtl::OUString;
using namespace ::osl;
using namespace ::vos;
using namespace ::cppu;
using namespace ::com::sun::star;

extern uno::Reference< uno::XInterface > SdUnoCreatePool( SdDrawDocument* pDrawModel );

class SdUnoForbiddenCharsTable : public SvxUnoForbiddenCharsTable,
								 public SfxListener
{
public:
	SdUnoForbiddenCharsTable( SdrModel* pModel );
	~SdUnoForbiddenCharsTable();

	// SfxListener
	virtual void Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) throw ();
protected:
	virtual void onChange();

private:
	SdrModel*	mpModel;
};

SdUnoForbiddenCharsTable::SdUnoForbiddenCharsTable( SdrModel* pModel )
: SvxUnoForbiddenCharsTable( pModel->GetForbiddenCharsTable() ), mpModel( pModel )
{
	StartListening( *pModel );
}

void SdUnoForbiddenCharsTable::onChange()
{
	if( mpModel )
	{
		mpModel->ReformatAllTextObjects();
	}
}

SdUnoForbiddenCharsTable::~SdUnoForbiddenCharsTable()
{
	if( mpModel )
		EndListening( *mpModel );
}

void SdUnoForbiddenCharsTable::Notify( SfxBroadcaster&, const SfxHint& rHint ) throw()
{
    const SdrBaseHint* pSdrHint = dynamic_cast< const SdrBaseHint* >(&rHint);

	if( pSdrHint )
	{
		if( HINT_MODELCLEARED == pSdrHint->GetSdrHintKind() )
		{
			mpModel = NULL;
		}
	}
}

///////////////////////////////////////////////////////////////////////

const sal_Int32 WID_MODEL_LANGUAGE = 1;
const sal_Int32 WID_MODEL_TABSTOP  = 2;
const sal_Int32 WID_MODEL_VISAREA  = 3;
const sal_Int32 WID_MODEL_MAPUNIT  = 4;
const sal_Int32 WID_MODEL_FORBCHARS= 5;
const sal_Int32 WID_MODEL_CONTFOCUS = 6;
const sal_Int32 WID_MODEL_DSGNMODE	= 7;
const sal_Int32 WID_MODEL_BASICLIBS = 8;
const sal_Int32 WID_MODEL_RUNTIMEUID = 9;
const sal_Int32 WID_MODEL_BUILDID = 10;
const sal_Int32 WID_MODEL_HASVALIDSIGNATURES = 11;
const sal_Int32 WID_MODEL_DIALOGLIBS = 12;

const SvxItemPropertySet* ImplGetDrawModelPropertySet()
{
	// Achtung: Der erste Parameter MUSS sortiert vorliegen !!!
	const static SfxItemPropertyMapEntry aDrawModelPropertyMap_Impl[] =
	{
		{ MAP_CHAR_LEN("BuildId"),						WID_MODEL_BUILDID,	&::getCppuType(static_cast< const rtl::OUString * >(0)), 0, 0},
		{ MAP_CHAR_LEN(sUNO_Prop_CharLocale),		  	WID_MODEL_LANGUAGE,	&::getCppuType((const lang::Locale*)0),		0,	0},
		{ MAP_CHAR_LEN(sUNO_Prop_TabStop),				WID_MODEL_TABSTOP,	&::getCppuType((const sal_Int32*)0),		0,  0},
		{ MAP_CHAR_LEN(sUNO_Prop_VisibleArea),			WID_MODEL_VISAREA,	&::getCppuType((const awt::Rectangle*)0),	0,	0},
		{ MAP_CHAR_LEN(sUNO_Prop_MapUnit),				WID_MODEL_MAPUNIT,	&::getCppuType((const sal_Int16*)0),		beans::PropertyAttribute::READONLY,	0},
		{ MAP_CHAR_LEN(sUNO_Prop_ForbiddenCharacters),	WID_MODEL_FORBCHARS,&::getCppuType((const uno::Reference< i18n::XForbiddenCharacters > *)0), beans::PropertyAttribute::READONLY, 0 },
		{ MAP_CHAR_LEN(sUNO_Prop_AutomContFocus ),	WID_MODEL_CONTFOCUS,	&::getBooleanCppuType(),					0,	0},
		{ MAP_CHAR_LEN(sUNO_Prop_ApplyFrmDsgnMode),	WID_MODEL_DSGNMODE,		&::getBooleanCppuType(),					0,	0},
		{ MAP_CHAR_LEN("BasicLibraries"),				WID_MODEL_BASICLIBS,&::getCppuType((const uno::Reference< script::XLibraryContainer > *)0), beans::PropertyAttribute::READONLY, 0 },
        { MAP_CHAR_LEN("DialogLibraries"),              WID_MODEL_DIALOGLIBS,   &::getCppuType((const uno::Reference< script::XLibraryContainer > *)0), beans::PropertyAttribute::READONLY, 0 },
        { MAP_CHAR_LEN(sUNO_Prop_RuntimeUID),           WID_MODEL_RUNTIMEUID,   &::getCppuType(static_cast< const rtl::OUString * >(0)), beans::PropertyAttribute::READONLY, 0 },
        { MAP_CHAR_LEN(sUNO_Prop_HasValidSignatures),   WID_MODEL_HASVALIDSIGNATURES, &::getCppuType(static_cast< const sal_Bool * >(0)), beans::PropertyAttribute::READONLY, 0 },
		{ 0,0,0,0,0,0 }
	};
    static SvxItemPropertySet aDrawModelPropertySet_Impl( aDrawModelPropertyMap_Impl, GetGlobalDrawObjectItemPool() );
    return &aDrawModelPropertySet_Impl;
}

// this ctor is used from the DocShell
SdXImpressDocument::SdXImpressDocument (::sd::DrawDocShell* pShell, bool bClipBoard ) throw()
:	SfxBaseModel( pShell ),
	mpDocShell( pShell ),
    mpDoc( pShell ? pShell->GetDoc() : NULL ),
	mbDisposed(false),
	mbImpressDoc( pShell && pShell->GetDoc() && pShell->GetDoc()->GetDocumentType() == DOCUMENT_TYPE_IMPRESS ),
	mbClipBoard( bClipBoard ),
	mpPropSet( ImplGetDrawModelPropertySet() )
{
	if( mpDoc )
	{
		StartListening( *mpDoc );
	}
	else
	{
		DBG_ERROR("DocShell is invalid");
	}
}

SdXImpressDocument::SdXImpressDocument( SdDrawDocument* pDoc, bool bClipBoard ) throw()
:	SfxBaseModel( NULL ),
	mpDocShell( NULL ),
	mpDoc( pDoc ),
	mbDisposed(false),
	mbImpressDoc( pDoc && pDoc->GetDocumentType() == DOCUMENT_TYPE_IMPRESS ),
	mbClipBoard( bClipBoard ),
	mpPropSet( ImplGetDrawModelPropertySet() )
{
	if( mpDoc )
	{
		StartListening( *mpDoc );
	}
	else
	{
		DBG_ERROR("SdDrawDocument is invalid");
	}
}

/***********************************************************************
*                                                                      *
***********************************************************************/
SdXImpressDocument::~SdXImpressDocument() throw()
{
}

// uno helper


/******************************************************************************
* Erzeugt anhand der uebergebennen SdPage eine SdDrawPage. Wurde fuer diese   *
* SdPage bereits eine SdDrawPage erzeugt, wird keine neue SdDrawPage erzeug.  *
******************************************************************************/
/*
uno::Reference< drawing::XDrawPage >  SdXImpressDocument::CreateXDrawPage( SdPage* pPage ) throw()
{
	DBG_ASSERT(pPage,"SdXImpressDocument::CreateXDrawPage( NULL? )");

	uno::Reference< drawing::XDrawPage >  xDrawPage;

	if(pPage)
	{
		xDrawPage = SvxDrawPage::GetPageForSdrPage(pPage);

		if(!xDrawPage.is())
		{
			if(pPage->IsMasterPage())
			{
				xDrawPage = (presentation::XPresentationPage*)new SdMasterPage( this, pPage );
			}
			else
			{
				xDrawPage = (SvxDrawPage*)new SdDrawPage( this, pPage );
			}
		}
	}

	return xDrawPage;
}
*/

// XInterface
uno::Any SAL_CALL SdXImpressDocument::queryInterface( const uno::Type & rType ) throw(uno::RuntimeException)
{
	uno::Any aAny;

	QUERYINT(lang::XServiceInfo);
	else QUERYINT(beans::XPropertySet);
	else QUERYINT(lang::XMultiServiceFactory);
	else QUERYINT(drawing::XDrawPageDuplicator);
	else QUERYINT(drawing::XLayerSupplier);
	else QUERYINT(drawing::XMasterPagesSupplier);
	else QUERYINT(drawing::XDrawPagesSupplier);
	else QUERYINT(presentation::XHandoutMasterSupplier);
	else QUERYINT(document::XLinkTargetSupplier);
	else QUERYINT(style::XStyleFamiliesSupplier);
	else QUERYINT(com::sun::star::ucb::XAnyCompareFactory);
	else QUERYINT(view::XRenderable);
	else if( mbImpressDoc && rType == ITYPE(presentation::XPresentationSupplier) )
			aAny <<= uno::Reference< presentation::XPresentationSupplier >(this);
	else if( mbImpressDoc && rType == ITYPE(presentation::XCustomPresentationSupplier) )
			aAny <<= uno::Reference< presentation::XCustomPresentationSupplier >(this);
	else
		return SfxBaseModel::queryInterface( rType );

	return aAny;
}

void SAL_CALL SdXImpressDocument::acquire() throw ( )
{
	SfxBaseModel::acquire();
}

void SAL_CALL SdXImpressDocument::release() throw ( )
{
    if (osl_decrementInterlockedCount( &m_refCount ) == 0)
	{
        // restore reference count:
        osl_incrementInterlockedCount( &m_refCount );
        if(!mbDisposed)
		{
            try
			{
                dispose();
            }
			catch (uno::RuntimeException const& exc)
			{ // don't break throw ()
                OSL_ENSURE(
                    false, OUStringToOString(
                        exc.Message, RTL_TEXTENCODING_ASCII_US ).getStr() );
                static_cast<void>(exc);
            }
        }
        SfxBaseModel::release();
    }
}

// XUnoTunnel
const ::com::sun::star::uno::Sequence< sal_Int8 > & SdXImpressDocument::getUnoTunnelId() throw()
{
    static ::com::sun::star::uno::Sequence< sal_Int8 > * pSeq = 0;
    if( !pSeq )
    {
        ::osl::Guard< ::osl::Mutex > aGuard( ::osl::Mutex::getGlobalMutex() );
        if( !pSeq )
        {
            static ::com::sun::star::uno::Sequence< sal_Int8 > aSeq( 16 );
            rtl_createUuid( (sal_uInt8*)aSeq.getArray(), 0, sal_True );
            pSeq = &aSeq;
        }
    }
    return *pSeq;
}

SdXImpressDocument* SdXImpressDocument::getImplementation( const uno::Reference< uno::XInterface >& xInt )
{
    ::com::sun::star::uno::Reference< ::com::sun::star::lang::XUnoTunnel > xUT( xInt, ::com::sun::star::uno::UNO_QUERY );
    if( xUT.is() )
        return reinterpret_cast<SdXImpressDocument*>(sal::static_int_cast<sal_IntPtr>(xUT->getSomething( SdXImpressDocument::getUnoTunnelId() )));
    else
        return NULL;
}

sal_Int64 SAL_CALL SdXImpressDocument::getSomething( const ::com::sun::star::uno::Sequence< sal_Int8 >& rIdentifier ) throw(::com::sun::star::uno::RuntimeException)
{
    if( rIdentifier.getLength() == 16 )
    {
        if( (0 == rtl_compareMemory( SdXImpressDocument::getUnoTunnelId().getConstArray(), rIdentifier.getConstArray(), 16 )) )
            return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_IntPtr>(this));

        if( (0 == rtl_compareMemory( SdrModel::getUnoTunnelImplementationId().getConstArray(), rIdentifier.getConstArray(), 16 )) )
            return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_IntPtr>(mpDoc));
    }

    return SfxBaseModel::getSomething( rIdentifier );
}

// XTypeProvider
uno::Sequence< uno::Type > SAL_CALL SdXImpressDocument::getTypes(  ) throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( maTypeSequence.getLength() == 0 )
	{
		const uno::Sequence< uno::Type > aBaseTypes( SfxBaseModel::getTypes() );
		const sal_Int32 nBaseTypes = aBaseTypes.getLength();
		const uno::Type* pBaseTypes = aBaseTypes.getConstArray();

		const sal_Int32 nOwnTypes = mbImpressDoc ? 14 : 11;		// !DANGER! Keep this updated!

		maTypeSequence.realloc(  nBaseTypes + nOwnTypes );
		uno::Type* pTypes = maTypeSequence.getArray();

		*pTypes++ = ITYPE(beans::XPropertySet);
		*pTypes++ = ITYPE(lang::XServiceInfo);
		*pTypes++ = ITYPE(lang::XMultiServiceFactory);
		*pTypes++ = ITYPE(drawing::XDrawPageDuplicator);
		*pTypes++ = ITYPE(drawing::XLayerSupplier);
		*pTypes++ = ITYPE(drawing::XMasterPagesSupplier);
		*pTypes++ = ITYPE(drawing::XDrawPagesSupplier);
		*pTypes++ = ITYPE(document::XLinkTargetSupplier);
		*pTypes++ = ITYPE(style::XStyleFamiliesSupplier);
		*pTypes++ = ITYPE(com::sun::star::ucb::XAnyCompareFactory);
		*pTypes++ = ITYPE(view::XRenderable);
		if( mbImpressDoc )
		{
			*pTypes++ = ITYPE(presentation::XPresentationSupplier);
			*pTypes++ = ITYPE(presentation::XCustomPresentationSupplier);
			*pTypes++ = ITYPE(presentation::XHandoutMasterSupplier);
		}

		for( sal_Int32 nType = 0; nType < nBaseTypes; nType++ )
			*pTypes++ = *pBaseTypes++;
	}

	return maTypeSequence;
}

uno::Sequence< sal_Int8 > SAL_CALL SdXImpressDocument::getImplementationId(  ) throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	static uno::Sequence< sal_Int8 > aId;
	if( aId.getLength() == 0 )
	{
		aId.realloc( 16 );
		rtl_createUuid( (sal_uInt8 *)aId.getArray(), 0, sal_True );
	}
	return aId;
}

/***********************************************************************
*                                                                      *
***********************************************************************/
void SdXImpressDocument::Notify( SfxBroadcaster& rBC, const SfxHint& rHint )
{
	if( mpDoc )
	{
        const SdrBaseHint* pSdrHint = dynamic_cast< const SdrBaseHint* >(&rHint);

		if( pSdrHint )
		{
			if( hasEventListeners() )
			{
				document::EventObject aEvent;
				if( SvxUnoDrawMSFactory::createEvent( mpDoc, pSdrHint, aEvent ) )
					notifyEvent( aEvent );
			}

			if( pSdrHint->GetSdrHintKind() == HINT_MODELCLEARED )
			{
				if( mpDoc )
					EndListening( *mpDoc );
				mpDoc = NULL;
				mpDocShell = NULL;
			}
		}
		else
		{
			const SfxSimpleHint* pSfxHint = dynamic_cast< const SfxSimpleHint* >(&rHint );

			// ist unser SdDrawDocument gerade gestorben?
			if(pSfxHint && pSfxHint->GetId() == SFX_HINT_DYING)
			{
				// yup, also schnell ein neues erfragen
				if( mpDocShell )
				{
					SdDrawDocument *pNewDoc = mpDocShell->GetDoc();

					// ist ueberhaupt ein neues da?
					if( pNewDoc != mpDoc )
					{
						mpDoc = pNewDoc;
						if(mpDoc)
							StartListening( *mpDoc );
					}
				}
			}
		}
	}
	SfxBaseModel::Notify( rBC, rHint );
}

/******************************************************************************
*                                                                             *
******************************************************************************/
SdPage* SdXImpressDocument::InsertSdPage( sal_uInt32 nPage, sal_Bool bDuplicate ) throw()
{
	sal_uInt32 nPageCount = mpDoc->GetSdPageCount( PK_STANDARD );
	SdrLayerAdmin& rLayerAdmin = mpDoc->GetModelLayerAdmin();
	sal_uInt8 aBckgrnd = rLayerAdmin.GetLayerID(String(SdResId(STR_LAYER_BCKGRND)), false);
	sal_uInt8 aBckgrndObj = rLayerAdmin.GetLayerID(String(SdResId(STR_LAYER_BCKGRNDOBJ)), false);

	SdPage* pStandardPage = NULL;

	if( 0 == nPageCount )
	{
		// this is only used for clipboard where we only have one page
		pStandardPage = (SdPage*) mpDoc->AllocPage(sal_False);

		basegfx::B2DVector aDefSize(21000.0, 29700.0);   // A4-Hochformat
		pStandardPage->SetPageScale( aDefSize );
		mpDoc->InsertPage(pStandardPage, 0);
	}
	else
	{
		// Hier wird die Seite ermittelt, hinter der eingefuegt werden soll
		SdPage* pPreviousStandardPage = mpDoc->GetSdPage( Min( (nPageCount - 1), nPage ), PK_STANDARD );
		SetOfByte aVisibleLayers = pPreviousStandardPage->TRG_GetMasterPageVisibleLayers();
		sal_Bool bIsPageBack = aVisibleLayers.IsSet( aBckgrnd );
		sal_Bool bIsPageObj = aVisibleLayers.IsSet( aBckgrndObj );

		// AutoLayouts muessen fertig sein
		mpDoc->StopWorkStartupDelay();

		/**************************************************************
		* Es wird stets zuerst eine Standardseite und dann eine
		* Notizseite erzeugt. Es ist sichergestellt, dass auf eine
		* Standardseite stets die zugehoerige Notizseite folgt.
		**************************************************************/

		sal_uInt32 nStandardPageNum = pPreviousStandardPage->GetPageNumber() + 2;
		SdPage* pPreviousNotesPage = (SdPage*) mpDoc->GetPage( nStandardPageNum - 1 );
		sal_uInt32 nNotesPageNum = nStandardPageNum + 1;
		String aStandardPageName;
		String aNotesPageName;

		/**************************************************************
		* Standardseite
		**************************************************************/
		if( bDuplicate )
			pStandardPage = (SdPage*) pPreviousStandardPage->CloneSdrPage();
		else
			pStandardPage = (SdPage*) mpDoc->AllocPage(sal_False);

		pStandardPage->SetPageScale( pPreviousStandardPage->GetPageScale() );
		pStandardPage->SetPageBorder( pPreviousStandardPage->GetLeftPageBorder(),
									pPreviousStandardPage->GetTopPageBorder(),
									pPreviousStandardPage->GetRightPageBorder(),
									pPreviousStandardPage->GetBottomPageBorder() );
		pStandardPage->SetOrientation( pPreviousStandardPage->GetOrientation() );
		pStandardPage->SetName(aStandardPageName);

		// Seite hinter aktueller Seite einfuegen
		mpDoc->InsertPage(pStandardPage, nStandardPageNum);

		if( !bDuplicate )
		{
			// MasterPage der aktuellen Seite verwenden
			pStandardPage->TRG_SetMasterPage(pPreviousStandardPage->TRG_GetMasterPage());
			pStandardPage->SetLayoutName( pPreviousStandardPage->GetLayoutName() );
			pStandardPage->SetAutoLayout(AUTOLAYOUT_NONE, sal_True );
		}

		aBckgrnd = rLayerAdmin.GetLayerID(String(SdResId(STR_LAYER_BCKGRND)), sal_False);
		aBckgrndObj = rLayerAdmin.GetLayerID(String(SdResId(STR_LAYER_BCKGRNDOBJ)), sal_False);
		aVisibleLayers.Set(aBckgrnd, bIsPageBack);
		aVisibleLayers.Set(aBckgrndObj, bIsPageObj);
		pStandardPage->TRG_SetMasterPageVisibleLayers(aVisibleLayers);

		/**************************************************************
		* Notizseite
		**************************************************************/
		SdPage* pNotesPage = NULL;

		if( bDuplicate )
			pNotesPage = (SdPage*) pPreviousNotesPage->CloneSdrPage();
		else
			pNotesPage = (SdPage*) mpDoc->AllocPage(sal_False);

		pNotesPage->SetPageScale( pPreviousNotesPage->GetPageScale() );
		pNotesPage->SetPageBorder( pPreviousNotesPage->GetLeftPageBorder(),
								pPreviousNotesPage->GetTopPageBorder(),
								pPreviousNotesPage->GetRightPageBorder(),
								pPreviousNotesPage->GetBottomPageBorder() );
		pNotesPage->SetOrientation( pPreviousNotesPage->GetOrientation() );
		pNotesPage->SetName(aNotesPageName);
		pNotesPage->SetPageKind(PK_NOTES);

		// Seite hinter aktueller Seite einfuegen
		mpDoc->InsertPage(pNotesPage, nNotesPageNum);

		if( !bDuplicate )
		{
			// MasterPage der aktuellen Seite verwenden
			pNotesPage->TRG_SetMasterPage(pPreviousNotesPage->TRG_GetMasterPage());
			pNotesPage->SetLayoutName( pPreviousNotesPage->GetLayoutName() );
			pNotesPage->SetAutoLayout(AUTOLAYOUT_NOTES, sal_True );
		}
	}

	SetModified();

	return( pStandardPage );
}

void SdXImpressDocument::SetModified( sal_Bool bModified /* = sal_True */ ) throw()
{
	if( mpDoc )
		mpDoc->SetChanged( bModified );
}

// XModel
void SAL_CALL SdXImpressDocument	::lockControllers(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	mpDoc->setLock( sal_True );
}

void SAL_CALL SdXImpressDocument::unlockControllers(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	if( mpDoc->isLocked() )
	{
		mpDoc->setLock( sal_False );
	}
}

sal_Bool SAL_CALL SdXImpressDocument::hasControllersLocked(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	return mpDoc && mpDoc->isLocked();
}

#ifndef _UNOTOOLS_PROCESSFACTORY_HXX
#include <comphelper/processfactory.hxx>
#endif

uno::Reference < container::XIndexAccess > SAL_CALL SdXImpressDocument::getViewData() throw( uno::RuntimeException )
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference < container::XIndexAccess > xRet( SfxBaseModel::getViewData() );

	if( !xRet.is() )
	{
		List* pFrameViewList = mpDoc->GetFrameViewList();

		if( pFrameViewList && pFrameViewList->Count() )
		{
			xRet = uno::Reference < container::XIndexAccess >::query(::comphelper::getProcessServiceFactory()->createInstance(OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.IndexedPropertyValues"))));


			uno::Reference < container::XIndexContainer > xCont( xRet, uno::UNO_QUERY );
			DBG_ASSERT( xCont.is(), "SdXImpressDocument::getViewData() failed for OLE object" );
			if( xCont.is() )
			{
				sal_uInt32 i;
				for( i = 0; i < pFrameViewList->Count(); i++ )
				{
					::sd::FrameView* pFrameView =
                          static_cast< ::sd::FrameView*>(
                              pFrameViewList->GetObject(i));

					if(pFrameView)
					{
						uno::Sequence< beans::PropertyValue > aSeq;
						pFrameView->WriteUserDataSequence( aSeq );
						xCont->insertByIndex( i, uno::makeAny( aSeq ) );
					}
				}
			}
		}
	}

	return xRet;
}

void SAL_CALL SdXImpressDocument::setViewData( const uno::Reference < container::XIndexAccess >& xData ) throw(::com::sun::star::uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	SfxBaseModel::setViewData( xData );
	if( mpDocShell && (mpDocShell->GetCreateMode() == SFX_CREATE_MODE_EMBEDDED) && xData.is() )
	{
		const sal_Int32 nCount = xData->getCount();

		List* pFrameViewList = mpDoc->GetFrameViewList();

		DBG_ASSERT( pFrameViewList, "No FrameViewList?" );
		if( pFrameViewList )
		{
			::sd::FrameView* pFrameView;

			sal_uInt32 i;
			for ( i = 0; i < pFrameViewList->Count(); i++)
			{
				// Ggf. FrameViews loeschen
				pFrameView = static_cast< ::sd::FrameView*>(
                    pFrameViewList->GetObject(i));

				if (pFrameView)
					delete pFrameView;
			}

			pFrameViewList->Clear();

			uno::Sequence< beans::PropertyValue > aSeq;
			sal_Int32 nIndex;
			for( nIndex = 0; nIndex < nCount; nIndex++ )
			{
				if( xData->getByIndex( nIndex ) >>= aSeq )
				{
					pFrameView = new ::sd::FrameView( *mpDoc );
					pFrameView->ReadUserDataSequence( aSeq );
					pFrameViewList->Insert( pFrameView );
				}
			}
		}
	}
}

// XDrawPageDuplicator
uno::Reference< drawing::XDrawPage > SAL_CALL SdXImpressDocument::duplicate( const uno::Reference< drawing::XDrawPage >& xPage )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	// pPage von xPage besorgen und dann die Id (nPos )ermitteln
	SvxDrawPage* pSvxPage = SvxDrawPage::getImplementation( xPage );
	if( pSvxPage )
	{
		SdPage* pPage = (SdPage*) pSvxPage->GetSdrPage();
		sal_uInt32 nPos = pPage->GetPageNumber();
		nPos = ( nPos - 1 ) / 2;
		pPage = InsertSdPage( nPos, sal_True );
		if( pPage )
		{
			uno::Reference< drawing::XDrawPage > xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
			return xDrawPage;
		}
	}

	uno::Reference< drawing::XDrawPage > xDrawPage;
	return xDrawPage;
}


// XDrawPagesSupplier
uno::Reference< drawing::XDrawPages > SAL_CALL SdXImpressDocument::getDrawPages()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< drawing::XDrawPages >  xDrawPages( mxDrawPagesAccess );

	if( !xDrawPages.is() )
	{
		initializeDocument();
		mxDrawPagesAccess = xDrawPages = (drawing::XDrawPages*)new SdDrawPagesAccess(*this);
	}

	return xDrawPages;
}

// XMasterPagesSupplier
uno::Reference< drawing::XDrawPages > SAL_CALL SdXImpressDocument::getMasterPages()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< drawing::XDrawPages >  xMasterPages( mxMasterPagesAccess );

	if( !xMasterPages.is() )
	{
		initializeDocument();
		mxMasterPagesAccess = xMasterPages = new SdMasterPagesAccess(*this);
	}

	return xMasterPages;
}

// XLayerManagerSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getLayerManager(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< container::XNameAccess >  xLayerManager( mxLayerManager );

	if( !xLayerManager.is() )
		mxLayerManager = xLayerManager = new SdLayerManager(*this);

	return xLayerManager;
}

// XCustomPresentationSupplier
uno::Reference< container::XNameContainer > SAL_CALL SdXImpressDocument::getCustomPresentations()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< container::XNameContainer >  xCustomPres( mxCustomPresentationAccess );

	if( !xCustomPres.is() )
		mxCustomPresentationAccess = xCustomPres = new SdXCustomPresentationAccess(*this);

	return xCustomPres;
}

extern uno::Reference< presentation::XPresentation > createPresentation( SdXImpressDocument& rModel );

// XPresentationSupplier
uno::Reference< presentation::XPresentation > SAL_CALL SdXImpressDocument::getPresentation()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	return uno::Reference< presentation::XPresentation >( mpDoc->getPresentation().get() );
}

// XHandoutMasterSupplier
uno::Reference< drawing::XDrawPage > SAL_CALL SdXImpressDocument::getHandoutMasterPage()
	throw (uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< drawing::XDrawPage > xPage;

	if( mpDoc )
	{
		initializeDocument();
		SdPage* pPage = mpDoc->GetMasterSdPage( 0, PK_HANDOUT );
		if( pPage )
			xPage = uno::Reference< drawing::XDrawPage >::query( pPage->getUnoPage() );
	}
	return xPage;
}

// XMultiServiceFactory ( SvxFmMSFactory )
uno::Reference< uno::XInterface > SAL_CALL SdXImpressDocument::createInstance( const OUString& aServiceSpecifier )
	throw(uno::Exception, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.DashTable") ) )
	{
		if( !mxDashTable.is() )
			mxDashTable = SvxUnoDashTable_createInstance( mpDoc );

		return mxDashTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.GradientTable") ) )
	{
		if( !mxGradientTable.is() )
			mxGradientTable = SvxUnoGradientTable_createInstance( mpDoc );

		return mxGradientTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.HatchTable") ) )
	{
		if( !mxHatchTable.is() )
			mxHatchTable = SvxUnoHatchTable_createInstance( mpDoc );

		return mxHatchTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.BitmapTable") ) )
	{
		if( !mxBitmapTable.is() )
			mxBitmapTable = SvxUnoBitmapTable_createInstance( mpDoc );

		return mxBitmapTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.TransparencyGradientTable") ) )
	{
		if( !mxTransGradientTable.is() )
			mxTransGradientTable = SvxUnoTransGradientTable_createInstance( mpDoc );

		return mxTransGradientTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.MarkerTable") ) )
	{
		if( !mxMarkerTable.is() )
			mxMarkerTable = SvxUnoMarkerTable_createInstance( mpDoc );

		return mxMarkerTable;
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.text.NumberingRules" ) ) )
	{
		return uno::Reference< uno::XInterface >( SvxCreateNumRule( mpDoc ), uno::UNO_QUERY );
	}
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.Background" ) ) )
	{
		return uno::Reference< uno::XInterface >(
            static_cast<uno::XWeak*>(new SdUnoPageBackground( mpDoc )));
	}

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.Defaults") ) )
	{
		if( !mxDrawingPool.is() )
			mxDrawingPool = SdUnoCreatePool( mpDoc );

		return mxDrawingPool;

	}

	if( aServiceSpecifier.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM(sUNO_Service_ImageMapRectangleObject) ) )
	{
		return SvUnoImageMapRectangleObject_createInstance( ImplGetSupportedMacroItems() );
	}

	if( aServiceSpecifier.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM(sUNO_Service_ImageMapCircleObject) ) )
	{
		return SvUnoImageMapCircleObject_createInstance( ImplGetSupportedMacroItems() );
	}

	if( aServiceSpecifier.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM(sUNO_Service_ImageMapPolygonObject) ) )
	{
		return SvUnoImageMapPolygonObject_createInstance( ImplGetSupportedMacroItems() );
	}

	if( ( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.document.Settings") ) ) ||
		( !mbImpressDoc && ( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.DocumentSettings") ) ) ) ||
		( mbImpressDoc && ( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.DocumentSettings") ) ) ) )
	{
		return sd::DocumentSettings_createInstance( this );
	}

	if( ( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.text.TextField.DateTime") ) ) ||
	    ( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.text.textfield.DateTime") ) ) )
	{
		return (::cppu::OWeakObject * )new SvxUnoTextField( ID_EXT_DATEFIELD );
	}

	if( (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.TextField.Header"))) ||
	    (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.textfield.Header"))) )
	{
		return (::cppu::OWeakObject * )new SvxUnoTextField( ID_HEADERFIELD );
	}

	if( (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.TextField.Footer"))) ||
	    (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.textfield.Footer"))) )
	{
		return (::cppu::OWeakObject * )new SvxUnoTextField( ID_FOOTERFIELD );
	}

	if( (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.TextField.DateTime"))) ||
	    (0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.presentation.textfield.DateTime"))) )
	{
		return (::cppu::OWeakObject * )new SvxUnoTextField( ID_DATETIMEFIELD );
	}

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.xml.NamespaceMap") ) )
	{
		static sal_uInt16 aWhichIds[] = { SDRATTR_XMLATTRIBUTES, EE_CHAR_XMLATTRIBS, EE_PARA_XMLATTRIBS, 0 };

		return svx::NamespaceMap_createInstance( aWhichIds, &mpDoc->GetItemPool() );
	}

	// #99870# Support creation of GraphicObjectResolver and EmbeddedObjectResolver
	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.document.ExportGraphicObjectResolver") ) )
	{
		return (::cppu::OWeakObject * )new SvXMLGraphicHelper( GRAPHICHELPER_MODE_WRITE );
	}

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.document.ImportGraphicObjectResolver") ) )
	{
		return (::cppu::OWeakObject * )new SvXMLGraphicHelper( GRAPHICHELPER_MODE_READ );
	}

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.document.ExportEmbeddedObjectResolver") ) )
	{
        ::comphelper::IEmbeddedHelper *pPersist = mpDoc ? mpDoc->GetPersist() : NULL;
		if( NULL == pPersist )
			throw lang::DisposedException();

		return (::cppu::OWeakObject * )new SvXMLEmbeddedObjectHelper( *pPersist, EMBEDDEDOBJECTHELPER_MODE_WRITE );
	}

	if( 0 == aServiceSpecifier.reverseCompareToAsciiL( RTL_CONSTASCII_STRINGPARAM("com.sun.star.document.ImportEmbeddedObjectResolver") ) )
	{
        ::comphelper::IEmbeddedHelper *pPersist = mpDoc ? mpDoc->GetPersist() : NULL;
		if( NULL == pPersist )
			throw lang::DisposedException();

		return (::cppu::OWeakObject * )new SvXMLEmbeddedObjectHelper( *pPersist, EMBEDDEDOBJECTHELPER_MODE_READ );
    }

	uno::Reference< uno::XInterface > xRet;

	const String aType( aServiceSpecifier );
	if( aType.EqualsAscii( "com.sun.star.presentation.", 0, 26 ) )
	{
		SvxShape* pShape = NULL;
        SvxShapeKind aSvxShapeKind(SvxShapeKind_Text);

		// create a shape wrapper
		if( aType.EqualsAscii( "TitleTextShape", 26, 14 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "OutlinerShape", 26, 13 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "SubtitleShape", 26, 13 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "GraphicObjectShape", 26, 18 ) )
		{
			aSvxShapeKind = SvxShapeKind_Graphic;
		}
		else if( aType.EqualsAscii( "PageShape", 26, 9 ) )
		{
			aSvxShapeKind = SvxShapeKind_Page;
		}
		else if( aType.EqualsAscii( "OLE2Shape", 26, 9 ) )
		{
			aSvxShapeKind = SvxShapeKind_OLE2;
		}
		else if( aType.EqualsAscii( "ChartShape", 26, 10 ) )
		{
			aSvxShapeKind = SvxShapeKind_OLE2;
		}
		else if( aType.EqualsAscii( "CalcShape", 26, 9 ) )
		{
			aSvxShapeKind = SvxShapeKind_OLE2;
		}
		else if( aType.EqualsAscii( "TableShape", 26, 10 ) )
		{
			aSvxShapeKind = SvxShapeKind_Table;
		}
		else if( aType.EqualsAscii( "OrgChartShape", 26, 13 ) )
		{
			aSvxShapeKind = SvxShapeKind_OLE2;
		}
		else if( aType.EqualsAscii( "NotesShape", 26, 13 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "HandoutShape", 26, 13 ) )
		{
			aSvxShapeKind = SvxShapeKind_Page;
		}
		else if( aType.EqualsAscii( "FooterShape", 26, 12 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "HeaderShape", 26, 12 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "SlideNumberShape", 26, 17 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "DateTimeShape", 26, 17 ) )
		{
			aSvxShapeKind = SvxShapeKind_Text;
		}
		else if( aType.EqualsAscii( "MediaShape", 26, 10 ) )
		{
			aSvxShapeKind = SvxShapeKind_Media;
		}
		else
		{
			throw lang::ServiceNotRegisteredException();
		}

		// create the API wrapper
		pShape = SvxDrawPage::CreateShapeBySvxShapeKind(aSvxShapeKind);

		// set shape type
		if( pShape && !mbClipBoard )
        {
			pShape->SetShapeType(aServiceSpecifier);
        }

		xRet = (uno::XWeak*)pShape;
	}
	else if( aServiceSpecifier.equalsAsciiL(RTL_CONSTASCII_STRINGPARAM("com.sun.star.drawing.TableShape") ) )
	{
		SvxShape* pShape = SvxDrawPage::CreateShapeBySvxShapeKind(SvxShapeKind_Table);
		
        if( pShape && !mbClipBoard )
        {
			pShape->SetShapeType(aServiceSpecifier);
        }

		xRet = (uno::XWeak*)pShape;
	}
	else
	{
		xRet = SvxFmMSFactory::createInstance( aServiceSpecifier );
	}

	uno::Reference< drawing::XShape > xShape( xRet, uno::UNO_QUERY );
	if( xShape.is() )
	{
		xRet.clear();
		new SdXShape( SvxShape::getImplementation( xShape ), (SdXImpressDocument*)this );
		xRet = xShape;
		xShape.clear();
	}

	return xRet;
}

uno::Sequence< OUString > SAL_CALL SdXImpressDocument::getAvailableServiceNames()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	const uno::Sequence< OUString > aSNS_ORG( SvxFmMSFactory::getAvailableServiceNames() );

	uno::Sequence< OUString > aSNS( mbImpressDoc ? (36) : (19) );

	sal_uInt16 i(0);

	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.DashTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.GradientTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.HatchTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.BitmapTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.TransparencyGradientTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.MarkerTable"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.text.NumberingRules"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.Background"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.Settings"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM(sUNO_Service_ImageMapRectangleObject));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM(sUNO_Service_ImageMapCircleObject));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM(sUNO_Service_ImageMapPolygonObject));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.xml.NamespaceMap"));

	// #99870# Support creation of GraphicObjectResolver and EmbeddedObjectResolver
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.ExportGraphicObjectResolver"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.ImportGraphicObjectResolver"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.ExportEmbeddedObjectResolver"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.ImportEmbeddedObjectResolver"));
	aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.TableShape"));

	if(mbImpressDoc)
	{
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.TitleTextShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.OutlinerShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.SubtitleShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.GraphicObjectShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.ChartShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.PageShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.OLE2Shape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.TableShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.OrgChartShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.NotesShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.HandoutShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.DocumentSettings"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.FooterShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.HeaderShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.SlideNumberShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.DateTimeShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.CalcShape"));
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.MediaShape"));
	}
	else
	{
		aSNS[i++] = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.DocumentSettings"));
	}

	DBG_ASSERT( i == aSNS.getLength(), "Sequence overrun!" );

	return comphelper::concatSequences( aSNS_ORG, aSNS );
}

// lang::XServiceInfo
OUString SAL_CALL SdXImpressDocument::getImplementationName()
	throw(uno::RuntimeException)
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM("SdXImpressDocument"));
}

sal_Bool SAL_CALL SdXImpressDocument::supportsService( const OUString& ServiceName )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

    if (
        (ServiceName.equalsAscii("com.sun.star.document.OfficeDocument"       )) ||
        (ServiceName.equalsAscii("com.sun.star.drawing.GenericDrawingDocument")) ||
        (ServiceName.equalsAscii("com.sun.star.drawing.DrawingDocumentFactory"))
       )
    {
        return sal_True;
    }

    return (
            ( mbImpressDoc && ServiceName.equalsAscii("com.sun.star.presentation.PresentationDocument")) ||
            (!mbImpressDoc && ServiceName.equalsAscii("com.sun.star.drawing.DrawingDocument"          ))
           );
}

uno::Sequence< OUString > SAL_CALL SdXImpressDocument::getSupportedServiceNames() throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	uno::Sequence< OUString > aSeq( 4 );
	OUString* pServices = aSeq.getArray();

	*pServices++ = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.OfficeDocument"));
	*pServices++ = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.GenericDrawingDocument"));
	*pServices++ = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.DrawingDocumentFactory"));

	if( mbImpressDoc )
		*pServices++ = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.presentation.PresentationDocument"));
    else
		*pServices++ = OUString( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.drawing.DrawingDocument"));

	return aSeq;
}

// XPropertySet
uno::Reference< beans::XPropertySetInfo > SAL_CALL SdXImpressDocument::getPropertySetInfo(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );
	return mpPropSet->getPropertySetInfo();
}

void SAL_CALL SdXImpressDocument::setPropertyValue( const OUString& aPropertyName, const uno::Any& aValue )
	throw(beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	const SfxItemPropertySimpleEntry* pEntry = mpPropSet->getPropertyMapEntry(aPropertyName);

	switch( pEntry ? pEntry->nWID : -1 )
	{
		case WID_MODEL_LANGUAGE:
		{
			lang::Locale aLocale;
			if(!(aValue >>= aLocale))
				throw lang::IllegalArgumentException();

			mpDoc->SetLanguage( SvxLocaleToLanguage(aLocale), EE_CHAR_LANGUAGE );
			break;
		}
		case WID_MODEL_TABSTOP:
		{
			sal_Int32 nValue = 0;
			if(!(aValue >>= nValue) || nValue < 0 )
				throw lang::IllegalArgumentException();

			mpDoc->SetDefaultTabulator((sal_uInt16)nValue);
			break;
		}
		case WID_MODEL_VISAREA:
			{
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
				if( !pEmbeddedObj )
					break;

				awt::Rectangle aVisArea;
				if( !(aValue >>= aVisArea) || (aVisArea.Width < 0) || (aVisArea.Height < 0) )
					throw lang::IllegalArgumentException();

				pEmbeddedObj->SetVisArea( Rectangle( aVisArea.X, aVisArea.Y, aVisArea.X + aVisArea.Width - 1, aVisArea.Y + aVisArea.Height - 1 ) );
			}
			break;
		case WID_MODEL_CONTFOCUS:
			{
				sal_Bool bFocus = sal_False;
				if( !(aValue >>= bFocus ) )
					throw lang::IllegalArgumentException();
				mpDoc->SetAutoControlFocus( bFocus );
			}
			break;
		case WID_MODEL_DSGNMODE:
			{
				sal_Bool bMode = sal_False;
				if( !(aValue >>= bMode ) )
					throw lang::IllegalArgumentException();
				mpDoc->SetOpenInDesignMode( bMode );
			}
			break;
		case WID_MODEL_BUILDID:
			aValue >>= maBuildId;
			return;
		case WID_MODEL_MAPUNIT:
		case WID_MODEL_BASICLIBS:
        case WID_MODEL_RUNTIMEUID: // is read-only
        case WID_MODEL_DIALOGLIBS:
			throw beans::PropertyVetoException();
		default:
			throw beans::UnknownPropertyException();
	}

	SetModified();
}

uno::Any SAL_CALL SdXImpressDocument::getPropertyValue( const OUString& PropertyName )
	throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	uno::Any aAny;
	if( NULL == mpDoc )
		throw lang::DisposedException();

	const SfxItemPropertySimpleEntry* pEntry = mpPropSet->getPropertyMapEntry(PropertyName);

	switch( pEntry ? pEntry->nWID : -1 )
	{
		case WID_MODEL_LANGUAGE:
		{
			LanguageType eLang = mpDoc->GetLanguage( EE_CHAR_LANGUAGE );
			lang::Locale aLocale;
			SvxLanguageToLocale( aLocale, eLang );
			aAny <<= aLocale;
			break;
		}
		case WID_MODEL_TABSTOP:
			aAny <<= (sal_Int32)mpDoc->GetDefaultTabulator();
			break;
		case WID_MODEL_VISAREA:
			{
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
				if( !pEmbeddedObj )
					break;

				const Rectangle& aRect = pEmbeddedObj->GetVisArea();
				awt::Rectangle aVisArea( aRect.nLeft, aRect.nTop, aRect.getWidth(), aRect.getHeight() );
				aAny <<= aVisArea;
			}
			break;
		case WID_MODEL_MAPUNIT:
			{
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
				if( !pEmbeddedObj )
					break;

				sal_Int16 nMeasureUnit = 0;
				SvxMapUnitToMeasureUnit( (const short)pEmbeddedObj->GetMapUnit(), nMeasureUnit );
				aAny <<= (sal_Int16)nMeasureUnit;
		}
		break;
		case WID_MODEL_FORBCHARS:
		{
			aAny <<= getForbiddenCharsTable();
		}
		break;
		case WID_MODEL_CONTFOCUS:
			aAny <<= (sal_Bool)mpDoc->GetAutoControlFocus();
			break;
		case WID_MODEL_DSGNMODE:
			aAny <<= mpDoc->GetOpenInDesignMode();
			break;
		case WID_MODEL_BASICLIBS:
			aAny <<= mpDocShell->GetBasicContainer();
			break;
        case WID_MODEL_DIALOGLIBS:
            aAny <<= mpDocShell->GetDialogContainer();
            break;
        case WID_MODEL_RUNTIMEUID:
            aAny <<= getRuntimeUID();
            break;
		case WID_MODEL_BUILDID:
			return uno::Any( maBuildId );
        case WID_MODEL_HASVALIDSIGNATURES:
            aAny <<= hasValidSignatures();
            break;
        default:
			throw beans::UnknownPropertyException();
	}

	return aAny;
}

void SAL_CALL SdXImpressDocument::addPropertyChangeListener( const OUString& , const uno::Reference< beans::XPropertyChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException) {}
void SAL_CALL SdXImpressDocument::removePropertyChangeListener( const OUString& , const uno::Reference< beans::XPropertyChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException) {}
void SAL_CALL SdXImpressDocument::addVetoableChangeListener( const OUString& , const uno::Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException) {}
void SAL_CALL SdXImpressDocument::removeVetoableChangeListener( const OUString& , const uno::Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException) {}

// XLinkTargetSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getLinks()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< container::XNameAccess > xLinks( mxLinks );
	if( !xLinks.is() )
		mxLinks = xLinks = new SdDocLinkTargets( *this );
	return xLinks;
}

// XStyleFamiliesSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getStyleFamilies(  )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	uno::Reference< container::XNameAccess > xStyles( dynamic_cast< container::XNameAccess* >( mpDoc->GetStyleSheetPool()) );
	return xStyles;
}

// XAnyCompareFactory
uno::Reference< com::sun::star::ucb::XAnyCompare > SAL_CALL SdXImpressDocument::createAnyCompareByName( const OUString& )
    throw (uno::RuntimeException)
{
	return SvxCreateNumRuleCompare();
}

// XRenderable
sal_Int32 SAL_CALL SdXImpressDocument::getRendererCount( const uno::Any& rSelection,
                                                         const uno::Sequence< beans::PropertyValue >&  )
    throw (lang::IllegalArgumentException, uno::RuntimeException)
{
	OGuard      aGuard( Application::GetSolarMutex() );
	sal_Int32   nRet = 0;

	if( NULL == mpDoc )
		throw lang::DisposedException();

    uno::Sequence< beans::PropertyValue > aRenderer;

    if( mpDocShell && mpDoc )
    {
        uno::Reference< frame::XModel > xModel;

        rSelection >>= xModel;

        if( xModel == mpDocShell->GetModel() )
            nRet = mpDoc->GetSdPageCount( PK_STANDARD );
        else
        {
            uno::Reference< drawing::XShapes > xShapes;

            rSelection >>= xShapes;

            if( xShapes.is() && xShapes->getCount() )
                nRet = 1;
        }
    }
    return nRet;
}

uno::Sequence< beans::PropertyValue > SAL_CALL SdXImpressDocument::getRenderer( sal_Int32 , const uno::Any& ,
                                                                                const uno::Sequence< beans::PropertyValue >& rxOptions )
    throw (lang::IllegalArgumentException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

	sal_Bool bExportNotesPages = sal_False;
	for( sal_Int32 nProperty = 0, nPropertyCount = rxOptions.getLength(); nProperty < nPropertyCount; ++nProperty )
    {
        if( rxOptions[ nProperty ].Name.equalsAscii( "ExportNotesPages" ) )
			rxOptions[ nProperty].Value >>= bExportNotesPages;
    }
    uno::Sequence< beans::PropertyValue > aRenderer;
    if( mpDocShell && mpDoc )
    {
		awt::Size aPageSize;
		if ( bExportNotesPages )
		{
			const basegfx::B2DVector& rNotesPageScale = mpDoc->GetSdPage( 0, PK_NOTES )->GetPageScale();
			aPageSize = awt::Size(basegfx::fround(rNotesPageScale.getX()), basegfx::fround(rNotesPageScale.getY()));
		}
		else
		{
			const Rectangle aVisArea( mpDocShell->GetVisArea( embed::Aspects::MSOLE_DOCPRINT ) );
			aPageSize = awt::Size( aVisArea.GetWidth(), aVisArea.GetHeight() );
		}
        aRenderer.realloc( 1 );

        aRenderer[ 0 ].Name = OUString( RTL_CONSTASCII_USTRINGPARAM( "PageSize" ) );
        aRenderer[ 0 ].Value <<= aPageSize;
    }
    return aRenderer;
}

class ImplRenderPaintProc : public ::sdr::contact::ViewObjectContactRedirector
{
	const SdrLayerAdmin&	rLayerAdmin;
	SdrPageView*			pSdrPageView;
	vcl::PDFExtOutDevData*	pPDFExtOutDevData;

	vcl::PDFWriter::StructElement ImplBegStructureTag( SdrObject& rObject );

public:
	sal_Bool IsVisible  ( const SdrObject* pObj ) const;
	sal_Bool IsPrintable( const SdrObject* pObj ) const;

	ImplRenderPaintProc( const SdrLayerAdmin& rLA, SdrPageView* pView, vcl::PDFExtOutDevData* pData );
	virtual ~ImplRenderPaintProc();

	// all default implementations just call the same methods at the original. To do something
	// different, overload the method and at least do what the method does.
	virtual drawinglayer::primitive2d::Primitive2DSequence createRedirectedPrimitive2DSequence(
		const sdr::contact::ViewObjectContact& rOriginal, 
		const sdr::contact::DisplayInfo& rDisplayInfo);
};

ImplRenderPaintProc::ImplRenderPaintProc( const SdrLayerAdmin& rLA, SdrPageView* pView, vcl::PDFExtOutDevData* pData )
:	ViewObjectContactRedirector(),
	rLayerAdmin			( rLA ),
	pSdrPageView		( pView ),
	pPDFExtOutDevData	( pData )
{
}

ImplRenderPaintProc::~ImplRenderPaintProc()
{
}

sal_Int32 ImplPDFGetBookmarkPage( const String& rBookmark, SdDrawDocument& rDoc )
{
	sal_Int32 nPage = -1;

    OSL_TRACE("GotoBookmark %s",
        ::rtl::OUStringToOString(rBookmark, RTL_TEXTENCODING_UTF8).getStr());

	String aBookmark( rBookmark );

	if( rBookmark.Len() && rBookmark.GetChar( 0 ) == sal_Unicode('#') )
		aBookmark = rBookmark.Copy( 1 );

	// is the bookmark a page ?
	bool bIsMasterPage;
    sal_uInt32 nPgNum = rDoc.GetPageByName( aBookmark, bIsMasterPage );
	SdrObject*  pObj = NULL;

	if ( nPgNum == SDRPAGE_NOTFOUND )
	{
		// is the bookmark a object ?
		pObj = rDoc.GetObj( aBookmark );
		if (pObj)
		{
			SdrPage* pOwningPage = pObj->getSdrPageFromSdrObject();

			if(pOwningPage)
			{
				nPgNum = pOwningPage->GetPageNumber();
			}
		}
	}
	if ( nPgNum != SDRPAGE_NOTFOUND )
		nPage = ( nPgNum - 1 ) / 2;
	return nPage;
}

void ImplPDFExportComments( uno::Reference< drawing::XDrawPage > xPage, vcl::PDFExtOutDevData& rPDFExtOutDevData )
{
	try
	{
		uno::Reference< office::XAnnotationAccess > xAnnotationAccess( xPage, uno::UNO_QUERY_THROW );
		uno::Reference< office::XAnnotationEnumeration > xAnnotationEnumeration( xAnnotationAccess->createAnnotationEnumeration() );

		LanguageType eLanguage = Application::GetSettings().GetLanguage();
		while( xAnnotationEnumeration->hasMoreElements() )
		{
			uno::Reference< office::XAnnotation > xAnnotation( xAnnotationEnumeration->nextElement() );

			geometry::RealPoint2D aRealPoint2D( xAnnotation->getPosition() );
			uno::Reference< text::XText > xText( xAnnotation->getTextRange() );
//			rtl::OUString sInitials( getInitials( sAuthor ) );
			util::DateTime aDateTime( xAnnotation->getDateTime() );

			Date aDate( aDateTime.Day, aDateTime.Month, aDateTime.Year );
			Time aTime;
			String aStr( SvxDateTimeField::GetFormatted( aDate, aTime, SVXDATEFORMAT_B, *(SD_MOD()->GetNumberFormatter()), eLanguage ) );

			vcl::PDFNote aNote;
			String sTitle( xAnnotation->getAuthor() );
            sTitle.AppendAscii( RTL_CONSTASCII_STRINGPARAM( ", " ) );
            sTitle += aStr;
            aNote.Title = sTitle;
			aNote.Contents = xText->getString();
			rPDFExtOutDevData.CreateNote( Rectangle( Point( static_cast< long >( aRealPoint2D.X * 100 ),
				static_cast< long >( aRealPoint2D.Y * 100 ) ), Size( 1000, 1000 ) ), aNote );
		}
	}
	catch( uno::Exception& )
	{
	}
}

void ImplPDFExportShapeInteraction( uno::Reference< drawing::XShape > xShape, SdDrawDocument& rDoc, vcl::PDFExtOutDevData& rPDFExtOutDevData )
{
	const rtl::OUString sGroup   ( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.drawing.GroupShape" ) );
	const rtl::OUString sOnClick ( RTL_CONSTASCII_USTRINGPARAM( "OnClick" ) );
	const rtl::OUString sBookmark( RTL_CONSTASCII_USTRINGPARAM( "Bookmark" ) );

	if ( xShape->getShapeType().equals( sGroup ) )
	{
		uno::Reference< container::XIndexAccess > xIndexAccess( xShape, uno::UNO_QUERY );
		if ( xIndexAccess.is() )
		{
			sal_Int32 i, nCount = xIndexAccess->getCount();
			for ( i = 0; i < nCount; i++ )
			{
				uno::Reference< drawing::XShape > xSubShape( xIndexAccess->getByIndex( i ), uno::UNO_QUERY );
				if ( xSubShape.is() )
					ImplPDFExportShapeInteraction( xSubShape, rDoc, rPDFExtOutDevData );
			}
		}
	}
	else
	{
		uno::Reference< beans::XPropertySet > xShapePropSet( xShape, uno::UNO_QUERY );
		if( xShapePropSet.is() )
		{
			const basegfx::B2DVector& rPageSize = rDoc.GetSdPage( 0, PK_STANDARD )->GetPageScale();
			const Rectangle aPageRect(0, 0, basegfx::fround(rPageSize.getX()), basegfx::fround(rPageSize.getY()));
			const awt::Point aShapePos( xShape->getPosition() );
			const awt::Size aShapeSize( xShape->getSize() );
			const Rectangle aLinkRect( Point( aShapePos.X, aShapePos.Y ), Size( aShapeSize.Width, aShapeSize.Height ) );

			presentation::ClickAction eCa;
			uno::Any aAny( xShapePropSet->getPropertyValue( sOnClick ) );
			if ( aAny >>= eCa )
			{
				switch ( eCa )
				{
					case presentation::ClickAction_LASTPAGE :
					{
						sal_Int32 nCount = rDoc.GetSdPageCount( PK_STANDARD );
						sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nCount - 1, vcl::PDFWriter::FitRectangle );
					    sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
						rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
					}
					break;
					case presentation::ClickAction_FIRSTPAGE :
					{
						sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, 0, vcl::PDFWriter::FitRectangle );
					    sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
						rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
					}
					break;
					case presentation::ClickAction_PREVPAGE :
					{
						sal_Int32 nDestPage = rPDFExtOutDevData.GetCurrentPageNumber();
						if ( nDestPage )
							nDestPage--;
						sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nDestPage, vcl::PDFWriter::FitRectangle );
					    sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
						rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
					}
					break;
					case presentation::ClickAction_NEXTPAGE :
					{
						sal_Int32 nDestPage = rPDFExtOutDevData.GetCurrentPageNumber() + 1;
						sal_Int32 nLastPage = rDoc.GetSdPageCount( PK_STANDARD ) - 1;
						if ( nDestPage > nLastPage )
							nDestPage = nLastPage;
						sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nDestPage, vcl::PDFWriter::FitRectangle );
					    sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
						rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
					}
					break;

					case presentation::ClickAction_PROGRAM :
					case presentation::ClickAction_BOOKMARK :
					case presentation::ClickAction_DOCUMENT :
					{
						rtl::OUString aBookmark;
						xShapePropSet->getPropertyValue( sBookmark ) >>= aBookmark;
						if( aBookmark.getLength() )
						{
							switch( eCa )
							{
								case presentation::ClickAction_DOCUMENT :
								case presentation::ClickAction_PROGRAM :
								{
									sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
									rPDFExtOutDevData.SetLinkURL( nLinkId, aBookmark );
								}
								break;
								case presentation::ClickAction_BOOKMARK :
								{
									sal_Int32 nPage = ImplPDFGetBookmarkPage( aBookmark, rDoc );
									if ( nPage != -1 )
									{
										sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nPage, vcl::PDFWriter::FitRectangle );
										sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
										rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
									}
								}
								break;
								default:
									break;
							}
						}
					}
					break;

					case presentation::ClickAction_STOPPRESENTATION :
					case presentation::ClickAction_SOUND :
					case presentation::ClickAction_INVISIBLE :
					case presentation::ClickAction_VERB :
					case presentation::ClickAction_VANISH :
					case presentation::ClickAction_MACRO :
					default :
					break;
				}
			}
		}
	}
}

vcl::PDFWriter::StructElement ImplRenderPaintProc::ImplBegStructureTag( SdrObject& rObject )
{
	vcl::PDFWriter::StructElement eElement(vcl::PDFWriter::NonStructElement);
	
	if ( pPDFExtOutDevData && pPDFExtOutDevData->GetIsExportTaggedPDF() )
	{
		sal_uInt32 nInventor   = rObject.GetObjInventor();
		sal_uInt16 nIdentifier = rObject.GetObjIdentifier();
		SdrTextObj* pSdrTextObj = dynamic_cast< SdrTextObj* >(&rObject);

		if ( nInventor == SdrInventor )
		{
			if ( nIdentifier == OBJ_GRUP )
				eElement = vcl::PDFWriter::Section;
			else if ( nIdentifier == OBJ_TITLETEXT )
				eElement = vcl::PDFWriter::Heading;
			else if ( nIdentifier == OBJ_OUTLINETEXT )
				eElement = vcl::PDFWriter::Division;
			else if ( !pSdrTextObj || !pSdrTextObj->HasText() )
				eElement = vcl::PDFWriter::Figure;
		}
	}

	return eElement;
}

drawinglayer::primitive2d::Primitive2DSequence ImplRenderPaintProc::createRedirectedPrimitive2DSequence(
	const sdr::contact::ViewObjectContact& rOriginal, 
	const sdr::contact::DisplayInfo& rDisplayInfo)
{
	SdrObject* pObject = rOriginal.GetViewContact().TryToGetSdrObject();

    if(pObject)
	{
		drawinglayer::primitive2d::Primitive2DSequence xRetval;

		if(pObject->getSdrPageFromSdrObject())
		{
			if(pObject->getSdrPageFromSdrObject()->checkVisibility(rOriginal, rDisplayInfo, false))
			{
				if(IsVisible(pObject) && IsPrintable(pObject))
				{
					const vcl::PDFWriter::StructElement eElement(ImplBegStructureTag( *pObject ));
					const bool bTagUsed(vcl::PDFWriter::NonStructElement != eElement);

					xRetval = ::sdr::contact::ViewObjectContactRedirector::createRedirectedPrimitive2DSequence(rOriginal, rDisplayInfo);

					if(xRetval.hasElements() && bTagUsed)
					{
						// embed Primitive2DSequence in a structure tag element for
						// exactly this purpose (StructureTagPrimitive2D)
						const drawinglayer::primitive2d::Primitive2DReference xReference(new drawinglayer::primitive2d::StructureTagPrimitive2D(eElement, xRetval));
						xRetval = drawinglayer::primitive2d::Primitive2DSequence(&xReference, 1);
					}
				}
			}
		}

		return xRetval;
	}
	else
	{
		// not an object, maybe a page
		return sdr::contact::ViewObjectContactRedirector::createRedirectedPrimitive2DSequence(rOriginal, rDisplayInfo);
	}
}

sal_Bool ImplRenderPaintProc::IsVisible( const SdrObject* pObj ) const
{
	sal_Bool bVisible = sal_True;
	SdrLayerID nLayerId = pObj->GetLayer();
	if( pSdrPageView )
	{
		const SdrLayer* pSdrLayer = rLayerAdmin.GetLayer( nLayerId );
		if ( pSdrLayer )
		{
			String aLayerName = pSdrLayer->GetName();
			bVisible = pSdrPageView->IsLayerVisible( aLayerName );
		}
	}
	return bVisible;
}
sal_Bool ImplRenderPaintProc::IsPrintable( const SdrObject* pObj ) const
{
	sal_Bool bPrintable = sal_True;
	SdrLayerID nLayerId = pObj->GetLayer();
	if( pSdrPageView )
	{
		const SdrLayer* pSdrLayer = rLayerAdmin.GetLayer( nLayerId );
		if ( pSdrLayer )
		{
			String aLayerName = pSdrLayer->GetName();
			bPrintable = pSdrPageView->IsLayerPrintable( aLayerName );
		}
	}
	return bPrintable;

}
void SAL_CALL SdXImpressDocument::render( sal_Int32 nRenderer, const uno::Any& rSelection,
                                          const uno::Sequence< beans::PropertyValue >& rxOptions )
    throw (lang::IllegalArgumentException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpDoc )
		throw lang::DisposedException();

    if( mpDocShell && mpDoc )
    {
        uno::Reference< awt::XDevice >  xRenderDevice;
        const sal_Int32					nPageNumber = nRenderer + 1;
		PageKind						ePageKind = PK_STANDARD;
		sal_Bool						bExportNotesPages = sal_False;

		for( sal_Int32 nProperty = 0, nPropertyCount = rxOptions.getLength(); nProperty < nPropertyCount; ++nProperty )
        {
			if( rxOptions[ nProperty ].Name == OUString( RTL_CONSTASCII_USTRINGPARAM( "RenderDevice" ) ) )
				rxOptions[ nProperty ].Value >>= xRenderDevice;
			else if ( rxOptions[ nProperty ].Name == OUString( RTL_CONSTASCII_USTRINGPARAM( "ExportNotesPages" ) ) )
			{
				rxOptions[ nProperty].Value >>= bExportNotesPages;
				if ( bExportNotesPages )
					ePageKind = PK_NOTES;
			}
        }

        if( xRenderDevice.is() && nPageNumber && ( nPageNumber <= (sal_Int32)mpDoc->GetSdPageCount( ePageKind ) ) )
        {
            VCLXDevice*     pDevice = VCLXDevice::GetImplementation( xRenderDevice );
            OutputDevice*   pOut = pDevice ? pDevice->GetOutputDevice() : NULL;

            if( pOut )
            {
				vcl::PDFExtOutDevData* pPDFExtOutDevData = dynamic_cast< vcl::PDFExtOutDevData* >(pOut->GetExtOutDevData() );
	            ::sd::ClientView* pView = new ::sd::ClientView( mpDocShell, pOut, NULL );
				const basegfx::B2DVector& rPageScale = mpDoc->GetSdPage( nPageNumber - 1, ePageKind )->GetPageScale();
				const Rectangle aVisArea(0, 0, basegfx::fround(rPageScale.getX()), basegfx::fround(rPageScale.getY()));
        		Region					aRegion( aVisArea );
                Point					aOrigin;

				::sd::ViewShell* pOldViewSh = mpDocShell->GetViewShell();
				::sd::View* pOldSdView = pOldViewSh ? pOldViewSh->GetView() : NULL;

				if  ( pOldSdView )
					pOldSdView->SdrEndTextEdit();

                pView->SetHlplVisible( sal_False );
                pView->SetGridVisible( sal_False );
	            pView->SetBordVisible( sal_False );
	            pView->SetPageVisible( sal_False );
	            pView->SetGlueVisible( sal_False );

                pOut->SetMapMode( MAP_100TH_MM );
	            pOut->IntersectClipRegion( aVisArea );



                uno::Reference< frame::XModel > xModel;
                rSelection >>= xModel;

                if( xModel == mpDocShell->GetModel() )
                {
                    pView->ShowSdrPage( *mpDoc->GetSdPage( nPageNumber - 1, ePageKind ));
					SdrPageView* pPV = pView->GetSdrPageView();

				    if( pOldSdView )
                    {
                        SdrPageView* pOldPV = pOldSdView->GetSdrPageView();
                        if( pPV && pOldPV )
                        {
                            pPV->SetVisibleLayers( pOldPV->GetVisibleLayers() );
                            pPV->SetPrintableLayers( pOldPV->GetPrintableLayers() );
                        }
                    }

					ImplRenderPaintProc	aImplRenderPaintProc( mpDoc->GetModelLayerAdmin(),
						pPV, pPDFExtOutDevData );

					// background color for outliner :o
					SdPage& rPage = (SdPage&)pPV->getSdrPageFromSdrPageView();
						SdrOutliner& rOutl = mpDoc->GetDrawOutliner( NULL );
                        bool bScreenDisplay(true);

                        if(bScreenDisplay && pOut && OUTDEV_PRINTER == pOut->GetOutDevType())
                        {
                            // #i75566# printing; suppress AutoColor BackgroundColor generation
                            // for visibility reasons by giving GetPageBackgroundColor()
                            // the needed hint
                            bScreenDisplay = false;
                        }

                        if(bScreenDisplay && pOut && pOut->GetPDFWriter())
                        {
                            // #i75566# PDF export; suppress AutoColor BackgroundColor generation (see above)
                            bScreenDisplay = false;
                        }

                        // #i75566# Name change GetBackgroundColor -> GetPageBackgroundColor and 
                        // hint value if screen display. Only then the AutoColor mechanisms shall be applied
					rOutl.SetBackgroundColor( rPage.GetPageBackgroundColor( pPV, bScreenDisplay ) );
					pView->SdrPaintView::CompleteRedraw( pOut, aRegion, &aImplRenderPaintProc );

					if ( pPDFExtOutDevData )
					{
						try
						{
							uno::Any aAny;
							uno::Reference< drawing::XDrawPage > xPage( uno::Reference< drawing::XDrawPage >::query( rPage.getUnoPage() ) );
							if ( xPage.is() )
							{
								if ( pPDFExtOutDevData->GetIsExportNotes() )
									ImplPDFExportComments( xPage, *pPDFExtOutDevData );
								uno::Reference< beans::XPropertySet > xPagePropSet( xPage, uno::UNO_QUERY );
								if( xPagePropSet.is() )
								{
									// exporting object interactions to pdf

									// if necessary, the master page interactions will be exported first
									sal_Bool bIsBackgroundObjectsVisible = sal_False;	// SJ: #i39428# IsBackgroundObjectsVisible not available for Draw
									const rtl::OUString sIsBackgroundObjectsVisible( RTL_CONSTASCII_USTRINGPARAM( "IsBackgroundObjectsVisible" ) );
									if ( mbImpressDoc && !pPDFExtOutDevData->GetIsExportNotesPages() && ( xPagePropSet->getPropertyValue( sIsBackgroundObjectsVisible ) >>= bIsBackgroundObjectsVisible ) && bIsBackgroundObjectsVisible )
									{
										uno::Reference< drawing::XMasterPageTarget > xMasterPageTarget( xPage, uno::UNO_QUERY );
										if ( xMasterPageTarget.is() )
										{
											uno::Reference< drawing::XDrawPage > xMasterPage = xMasterPageTarget->getMasterPage();
											if ( xMasterPage.is() )
											{
												uno::Reference< drawing::XShapes> xShapes( xMasterPage, uno::UNO_QUERY );
												sal_Int32 i, nCount = xShapes->getCount();
												for ( i = 0; i < nCount; i++ )
												{
													aAny = xShapes->getByIndex( i );
													uno::Reference< drawing::XShape > xShape;
													if ( aAny >>= xShape )
														ImplPDFExportShapeInteraction( xShape, *mpDoc, *pPDFExtOutDevData );
												}
											}
										}
									}

									// exporting slide page object interactions
									uno::Reference< drawing::XShapes> xShapes( xPage, uno::UNO_QUERY );
									sal_Int32 i, nCount = xShapes->getCount();
									for ( i = 0; i < nCount; i++ )
									{
										aAny = xShapes->getByIndex( i );
										uno::Reference< drawing::XShape > xShape;
										if ( aAny >>= xShape )
											ImplPDFExportShapeInteraction( xShape, *mpDoc, *pPDFExtOutDevData );
									}

									// exporting transition effects to pdf
									if ( mbImpressDoc && !pPDFExtOutDevData->GetIsExportNotesPages() && pPDFExtOutDevData->GetIsExportTransitionEffects() )
									{
										const rtl::OUString sEffect( RTL_CONSTASCII_USTRINGPARAM( "Effect" ) );
										const rtl::OUString sSpeed ( RTL_CONSTASCII_USTRINGPARAM( "Speed" ) );
										sal_Int32 nTime = 800;
										presentation::AnimationSpeed aAs;
										aAny = xPagePropSet->getPropertyValue( sSpeed );
										if ( aAny >>= aAs )
										{
											switch( aAs )
											{
												case presentation::AnimationSpeed_SLOW : nTime = 1500; break;
												case presentation::AnimationSpeed_FAST : nTime = 300; break;
												default:
												case presentation::AnimationSpeed_MEDIUM : nTime = 800;
											}
										}
										presentation::FadeEffect eFe;
										aAny = xPagePropSet->getPropertyValue( sEffect );
										vcl::PDFWriter::PageTransition eType = vcl::PDFWriter::Regular;
										if ( aAny >>= eFe )
										{
											switch( eFe )
											{
												case presentation::FadeEffect_HORIZONTAL_LINES :
												case presentation::FadeEffect_HORIZONTAL_CHECKERBOARD :
												case presentation::FadeEffect_HORIZONTAL_STRIPES : eType = vcl::PDFWriter::BlindsHorizontal; break;

												case presentation::FadeEffect_VERTICAL_LINES :
												case presentation::FadeEffect_VERTICAL_CHECKERBOARD :
												case presentation::FadeEffect_VERTICAL_STRIPES : eType = vcl::PDFWriter::BlindsVertical; break;

												case presentation::FadeEffect_UNCOVER_TO_RIGHT :
												case presentation::FadeEffect_UNCOVER_TO_UPPERRIGHT :
												case presentation::FadeEffect_ROLL_FROM_LEFT :
												case presentation::FadeEffect_FADE_FROM_UPPERLEFT :
												case presentation::FadeEffect_MOVE_FROM_UPPERLEFT :
												case presentation::FadeEffect_FADE_FROM_LEFT :
												case presentation::FadeEffect_MOVE_FROM_LEFT : eType = vcl::PDFWriter::WipeLeftToRight; break;

												case presentation::FadeEffect_UNCOVER_TO_BOTTOM :
												case presentation::FadeEffect_UNCOVER_TO_LOWERRIGHT :
												case presentation::FadeEffect_ROLL_FROM_TOP :
												case presentation::FadeEffect_FADE_FROM_UPPERRIGHT :
												case presentation::FadeEffect_MOVE_FROM_UPPERRIGHT :
												case presentation::FadeEffect_FADE_FROM_TOP :
												case presentation::FadeEffect_MOVE_FROM_TOP : eType = vcl::PDFWriter::WipeTopToBottom; break;

												case presentation::FadeEffect_UNCOVER_TO_LEFT :
												case presentation::FadeEffect_UNCOVER_TO_LOWERLEFT :
												case presentation::FadeEffect_ROLL_FROM_RIGHT :

												case presentation::FadeEffect_FADE_FROM_LOWERRIGHT :
												case presentation::FadeEffect_MOVE_FROM_LOWERRIGHT :
												case presentation::FadeEffect_FADE_FROM_RIGHT :
												case presentation::FadeEffect_MOVE_FROM_RIGHT : eType = vcl::PDFWriter::WipeRightToLeft; break;

												case presentation::FadeEffect_UNCOVER_TO_TOP :
												case presentation::FadeEffect_UNCOVER_TO_UPPERLEFT :
												case presentation::FadeEffect_ROLL_FROM_BOTTOM :
												case presentation::FadeEffect_FADE_FROM_LOWERLEFT :
												case presentation::FadeEffect_MOVE_FROM_LOWERLEFT :
												case presentation::FadeEffect_FADE_FROM_BOTTOM :
												case presentation::FadeEffect_MOVE_FROM_BOTTOM : eType = vcl::PDFWriter::WipeBottomToTop; break;

												case presentation::FadeEffect_OPEN_VERTICAL : eType = vcl::PDFWriter::SplitHorizontalInward; break;
												case presentation::FadeEffect_CLOSE_HORIZONTAL : eType = vcl::PDFWriter::SplitHorizontalOutward; break;

												case presentation::FadeEffect_OPEN_HORIZONTAL : eType = vcl::PDFWriter::SplitVerticalInward; break;
												case presentation::FadeEffect_CLOSE_VERTICAL : eType = vcl::PDFWriter::SplitVerticalOutward; break;

												case presentation::FadeEffect_FADE_TO_CENTER : eType = vcl::PDFWriter::BoxInward; break;
												case presentation::FadeEffect_FADE_FROM_CENTER : eType = vcl::PDFWriter::BoxOutward; break;

												case presentation::FadeEffect_NONE : eType = vcl::PDFWriter::Regular; break;

												case presentation::FadeEffect_RANDOM :
												case presentation::FadeEffect_DISSOLVE :
												default: eType = vcl::PDFWriter::Dissolve; break;
											}
										}
										pPDFExtOutDevData->SetPageTransition( eType, nTime, -1 );
									}
								}
							}

							const basegfx::B2DVector& rPageScale1 = mpDoc->GetSdPage( 0, PK_STANDARD )->GetPageScale();
							const Rectangle aPageRect(0, 0, basegfx::fround(rPageScale1.getX()), basegfx::fround(rPageScale1.getY()));

							// resolving links found in this page by the method ImpEditEngine::Paint
							std::vector< vcl::PDFExtOutDevBookmarkEntry >& rBookmarks = pPDFExtOutDevData->GetBookmarks();
							std::vector< vcl::PDFExtOutDevBookmarkEntry >::iterator aIBeg = rBookmarks.begin();
							std::vector< vcl::PDFExtOutDevBookmarkEntry >::iterator aIEnd = rBookmarks.end();
							while ( aIBeg != aIEnd )
							{
								sal_Int32 nPage = ImplPDFGetBookmarkPage( aIBeg->aBookmark, *mpDoc );
								if ( nPage != -1 )
								{
									if ( aIBeg->nLinkId != -1 )
										pPDFExtOutDevData->SetLinkDest( aIBeg->nLinkId, pPDFExtOutDevData->CreateDest( aPageRect, nPage, vcl::PDFWriter::FitRectangle ) );
									else
										pPDFExtOutDevData->DescribeRegisteredDest( aIBeg->nDestId, aPageRect, nPage, vcl::PDFWriter::FitRectangle );
								}
								else
									pPDFExtOutDevData->SetLinkURL( aIBeg->nLinkId, aIBeg->aBookmark );
								aIBeg++;
							}
							rBookmarks.clear();
							//---> i56629, i40318
							//get the page name, will be used as outline element in PDF bookmark pane
							String aPageName = mpDoc->GetSdPage( (sal_uInt16)nPageNumber - 1 , PK_STANDARD )->GetName();
							if( aPageName.Len() > 0 )
							{
								// insert the bookmark to this page into the NamedDestinations
								if( pPDFExtOutDevData->GetIsExportNamedDestinations() )
									pPDFExtOutDevData->CreateNamedDest( aPageName, aPageRect,  nPageNumber - 1 );
								//
								// add the name to the outline, (almost) same code as in sc/source/ui/unoobj/docuno.cxx
								// issue i40318.
								//
								if( pPDFExtOutDevData->GetIsExportBookmarks() )
								{
									// Destination Export
									const sal_Int32 nDestId =
										pPDFExtOutDevData->CreateDest( aPageRect , nPageNumber - 1 );
    
									// Create a new outline item:
									pPDFExtOutDevData->CreateOutlineItem( -1 , aPageName, nDestId );
								}                            
							}
							//<--- i56629, i40318
						}
						catch( uno::Exception& )
						{
						}

					}
            	}
            	else
            	{
		            uno::Reference< drawing::XShapes > xShapes;
		            rSelection >>= xShapes;

		            if( xShapes.is() && xShapes->getCount() )
		            {
					   ImplRenderPaintProc	aImplRenderPaintProc( mpDoc->GetModelLayerAdmin(),
										pOldSdView ? pOldSdView->GetSdrPageView() : NULL, pPDFExtOutDevData );

			            for( sal_uInt32 i = 0, nCount = xShapes->getCount(); i < nCount; i++ )
			            {
			                uno::Reference< drawing::XShape > xShape;
				            xShapes->getByIndex( i ) >>= xShape;

				            if( xShape.is() )
				            {
					            SvxShape* pShape = SvxShape::getImplementation( xShape );

					            if( pShape )
					            {
						            SdrObject* pObj = pShape->GetSdrObject();
						            if( pObj && pObj->getSdrPageFromSdrObject()
										&& aImplRenderPaintProc.IsVisible( pObj )
											&& aImplRenderPaintProc.IsPrintable( pObj ) )
						            {
						                if( !pView->GetSdrPageView() )
										{
                                            pView->ShowSdrPage( *pObj->getSdrPageFromSdrObject() );
										}

							            pView->MarkObj( *pObj );
							        }
					            }
				            }
			            }
                        pView->DrawMarkedObj(*pOut);
		            }
		        }

                delete pView;
            }
        }
    }
}

uno::Reference< i18n::XForbiddenCharacters > SdXImpressDocument::getForbiddenCharsTable()
{
	uno::Reference< i18n::XForbiddenCharacters > xForb(mxForbidenCharacters);

	if( !xForb.is() )
		mxForbidenCharacters = xForb = new SdUnoForbiddenCharsTable( mpDoc );

	return xForb;
}

void SdXImpressDocument::initializeDocument()
{
    if( !mbClipBoard )
    {
        switch( mpDoc->GetPageCount() )
        {
        case 1:
        {
            // nasty hack to detect clipboard document
            mbClipBoard = true;
            break;
        }
        case 0:
        {
	        mpDoc->CreateFirstPages();
		    mpDoc->StopWorkStartupDelay();
            break;
        }
	    }
    }
}

SdrModel* SdXImpressDocument::getSdrModel() const
{
	return GetDoc();
}

void SAL_CALL SdXImpressDocument::dispose() throw (::com::sun::star::uno::RuntimeException)
{
    if( !mbDisposed )
    {
        {
			OGuard aGuard( Application::GetSolarMutex() );

			if( mpDoc )
			{
				EndListening( *mpDoc );
				mpDoc = NULL;
			}

			// Call the base class dispose() before setting the mbDisposed flag
			// to true.  The reason for this is that if close() has not yet been
			// called this is done in SfxBaseModel::dispose().  At the end of
			// that dispose() is called again.  It is important to forward this
			// second dispose() to the base class, too.
			// As a consequence the following code has to be able to be run twice.
			SfxBaseModel::dispose();
			mbDisposed = true;

			uno::Reference< container::XNameAccess > xStyles(mxStyleFamilies);
			if( xStyles.is() )
			{
				uno::Reference< lang::XComponent > xComp( xStyles, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xStyles = 0;
			}

			uno::Reference< presentation::XPresentation > xPresentation( mxPresentation );
			if( xPresentation.is() )
			{
				uno::Reference< ::com::sun::star::presentation::XPresentation2 > xPres( mpDoc->getPresentation().get() );
				uno::Reference< lang::XComponent > xPresComp( xPres, uno::UNO_QUERY );
				if( xPresComp.is() )
					xPresComp->dispose();
			}

			uno::Reference< container::XNameAccess > xLinks( mxLinks );
			if( xLinks.is() )
			{
				uno::Reference< lang::XComponent > xComp( xLinks, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xLinks = 0;
			}

			uno::Reference< drawing::XDrawPages > xDrawPagesAccess( mxDrawPagesAccess );
			if( xDrawPagesAccess.is() )
			{
				uno::Reference< lang::XComponent > xComp( xDrawPagesAccess, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xDrawPagesAccess = 0;
			}

			uno::Reference< drawing::XDrawPages > xMasterPagesAccess( mxMasterPagesAccess );
			if( xDrawPagesAccess.is() )
			{
				uno::Reference< lang::XComponent > xComp( xMasterPagesAccess, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xDrawPagesAccess = 0;
			}

			uno::Reference< container::XNameAccess > xLayerManager( mxLayerManager );
			if( xLayerManager.is() )
			{
				uno::Reference< lang::XComponent > xComp( xLayerManager, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xLayerManager = 0;
			}

			uno::Reference< container::XNameContainer > xCustomPresentationAccess( mxCustomPresentationAccess );
			if( xCustomPresentationAccess.is() )
			{
				uno::Reference< lang::XComponent > xComp( xCustomPresentationAccess, uno::UNO_QUERY );
				if( xComp.is() )
					xComp->dispose();

				xCustomPresentationAccess = 0;
			}

			mxDashTable = 0;
			mxGradientTable = 0;
			mxHatchTable = 0;
			mxBitmapTable = 0;
			mxTransGradientTable = 0;
			mxMarkerTable = 0;
			mxDrawingPool = 0;
		}
	}
}

//=============================================================================
// class SdDrawPagesAccess
//=============================================================================

SdDrawPagesAccess::SdDrawPagesAccess( SdXImpressDocument& rMyModel )  throw()
:	mpModel( &rMyModel)
{
}

SdDrawPagesAccess::~SdDrawPagesAccess() throw()
{
}

// XIndexAccess
sal_Int32 SAL_CALL SdDrawPagesAccess::getCount()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	return mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
}

uno::Any SAL_CALL SdDrawPagesAccess::getByIndex( sal_Int32 Index )
	throw(lang::IndexOutOfBoundsException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	uno::Any aAny;

	if( (Index < 0) || (Index >= (sal_Int32)mpModel->mpDoc->GetSdPageCount( PK_STANDARD ) ) )
		throw lang::IndexOutOfBoundsException();

	SdPage* pPage = mpModel->mpDoc->GetSdPage( (sal_uInt32)Index, PK_STANDARD );
	if( pPage )
	{
		uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
		aAny <<= xDrawPage;
	}

	return aAny;
}

// XNameAccess
uno::Any SAL_CALL SdDrawPagesAccess::getByName( const OUString& aName ) throw(container::NoSuchElementException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	if( aName.getLength() != 0 )
	{
		const sal_uInt32 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
		sal_uInt32 nPage;
		for( nPage = 0; nPage < nCount; nPage++ )
		{
			SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
			if(NULL == pPage)
				continue;

			if( aName == SdDrawPage::getPageApiName( pPage ) )
			{
				uno::Any aAny;
				uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
				aAny <<= xDrawPage;
				return aAny;
			}
		}
	}

	throw container::NoSuchElementException();
}

uno::Sequence< OUString > SAL_CALL SdDrawPagesAccess::getElementNames() throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	const sal_uInt32 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
	uno::Sequence< OUString > aNames( nCount );
	OUString* pNames = aNames.getArray();

	sal_uInt32 nPage;
	for( nPage = 0; nPage < nCount; nPage++ )
	{
		SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
		*pNames++ = SdDrawPage::getPageApiName( pPage );
	}

	return aNames;
}

sal_Bool SAL_CALL SdDrawPagesAccess::hasByName( const OUString& aName ) throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	const sal_uInt32 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
	sal_uInt32 nPage;
	for( nPage = 0; nPage < nCount; nPage++ )
	{
		SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
		if(NULL == pPage)
			continue;

		if( aName == SdDrawPage::getPageApiName( pPage ) )
			return sal_True;
	}

	return sal_False;
}

// XElementAccess
uno::Type SAL_CALL SdDrawPagesAccess::getElementType()
	throw(uno::RuntimeException)
{
	return ITYPE( drawing::XDrawPage );
}

sal_Bool SAL_CALL SdDrawPagesAccess::hasElements()
	throw(uno::RuntimeException)
{
	return getCount() > 0;
}

// XDrawPages

/******************************************************************************
* Erzeugt eine neue Seite mit Model an der angegebennen Position und gibt die *
* dazugehoerige SdDrawPage zurueck.                                           *
******************************************************************************/
uno::Reference< drawing::XDrawPage > SAL_CALL SdDrawPagesAccess::insertNewByIndex( sal_Int32 nIndex )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	if( mpModel->mpDoc )
	{
		SdPage* pPage = mpModel->InsertSdPage( (sal_uInt32)nIndex );
		if( pPage )
		{
			uno::Reference< drawing::XDrawPage > xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
			return xDrawPage;
		}
	}
	uno::Reference< drawing::XDrawPage > xDrawPage;
	return xDrawPage;
}

/******************************************************************************
* Entfernt die angegebenne SdDrawPage aus dem Model und aus der internen      *
* Liste. Dies funktioniert nur, wenn mindestens eine *normale* Seite im Model *
* nach dem entfernen dieser Seite vorhanden ist.							  *
******************************************************************************/
void SAL_CALL SdDrawPagesAccess::remove( const uno::Reference< drawing::XDrawPage >& xPage )
		throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel || mpModel->mpDoc == NULL )
		throw lang::DisposedException();

	SdDrawDocument& rDoc = *mpModel->mpDoc;

	sal_uInt16 nPageCount = rDoc.GetSdPageCount( PK_STANDARD );
	if( nPageCount > 1 )
	{
		// pPage von xPage besorgen und dann die Id (nPos )ermitteln
		SdDrawPage* pSvxPage = SdDrawPage::getImplementation( xPage );
		if( pSvxPage )
		{
			SdPage* pPage = (SdPage*) pSvxPage->GetSdrPage();
			if(pPage && ( pPage->GetPageKind() == PK_STANDARD ) )
			{
				sal_uInt32 nPage = pPage->GetPageNumber();

				SdPage* pNotesPage = static_cast< SdPage* >( rDoc.GetPage( nPage+1 ) );

				bool bUndo = rDoc.IsUndoEnabled();
				if( bUndo )
				{
					// Add undo actions and delete the pages.  The order of adding
					// the undo actions is important.
					rDoc.BegUndo( SdResId( STR_UNDO_DELETEPAGES ) );
					rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pNotesPage));
					rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pPage));
				}

				rDoc.RemovePage( nPage ); // the page
				rDoc.RemovePage( nPage ); // the notes page

				if( bUndo )
				{
					rDoc.EndUndo();
				}
				else
				{
					delete pNotesPage;
					delete pPage;
				}
			}
		}
	}

	mpModel->SetModified();
}

// XServiceInfo
sal_Char pSdDrawPagesAccessService[sizeof("com.sun.star.drawing.DrawPages")] = "com.sun.star.drawing.DrawPages";

OUString SAL_CALL SdDrawPagesAccess::getImplementationName(  ) throw(uno::RuntimeException)
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM( "SdDrawPagesAccess" ) );
}

sal_Bool SAL_CALL SdDrawPagesAccess::supportsService( const OUString& ServiceName ) throw(uno::RuntimeException)
{
	return ServiceName.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( pSdDrawPagesAccessService ) );
}

uno::Sequence< OUString > SAL_CALL SdDrawPagesAccess::getSupportedServiceNames(  ) throw(uno::RuntimeException)
{
	OUString aService( RTL_CONSTASCII_USTRINGPARAM( pSdDrawPagesAccessService ) );
	uno::Sequence< OUString > aSeq( &aService, 1 );
	return aSeq;
}

// XComponent
void SAL_CALL SdDrawPagesAccess::dispose(  ) throw (uno::RuntimeException)
{
	mpModel = NULL;
}

void SAL_CALL SdDrawPagesAccess::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

void SAL_CALL SdDrawPagesAccess::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

//=============================================================================
// class SdMasterPagesAccess
//=============================================================================

SdMasterPagesAccess::SdMasterPagesAccess( SdXImpressDocument& rMyModel ) throw()
:	mpModel(&rMyModel)
{
}

SdMasterPagesAccess::~SdMasterPagesAccess() throw()
{
}

// XComponent
void SAL_CALL SdMasterPagesAccess::dispose(  ) throw (uno::RuntimeException)
{
	mpModel = NULL;
}

void SAL_CALL SdMasterPagesAccess::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

void SAL_CALL SdMasterPagesAccess::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

// XIndexAccess
sal_Int32 SAL_CALL SdMasterPagesAccess::getCount()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel->mpDoc )
		throw lang::DisposedException();

	return mpModel->mpDoc->GetMasterSdPageCount(PK_STANDARD);
}

/******************************************************************************
* Liefert ein drawing::XDrawPage Interface fuer den Zugriff auf die Masterpage and der *
* angegebennen Position im Model.                                             *
******************************************************************************/
uno::Any SAL_CALL SdMasterPagesAccess::getByIndex( sal_Int32 Index )
	throw(lang::IndexOutOfBoundsException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	uno::Any aAny;

	if( (Index < 0) || (Index >= (sal_Int32)mpModel->mpDoc->GetMasterSdPageCount( PK_STANDARD ) ) )
		throw lang::IndexOutOfBoundsException();

	SdPage* pPage = mpModel->mpDoc->GetMasterSdPage( (sal_uInt32)Index, PK_STANDARD );
	if( pPage )
	{
		uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
		aAny <<= xDrawPage;
	}

	return aAny;
}

// XElementAccess
uno::Type SAL_CALL SdMasterPagesAccess::getElementType()
	throw(uno::RuntimeException)
{
	return ITYPE(drawing::XDrawPage);
}

sal_Bool SAL_CALL SdMasterPagesAccess::hasElements()
	throw(uno::RuntimeException)
{
	return getCount() > 0;
}

// XDrawPages
uno::Reference< drawing::XDrawPage > SAL_CALL SdMasterPagesAccess::insertNewByIndex( sal_Int32 nInsertPos )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	uno::Reference< drawing::XDrawPage > xDrawPage;

	SdDrawDocument* mpDoc = mpModel->mpDoc;
	if( mpDoc )
	{
		// calculate internal index and check for range errors
		const sal_Int32 nMPageCount = mpDoc->GetMasterPageCount();
		nInsertPos = nInsertPos * 2 + 1;
		if( nInsertPos < 0 || nInsertPos > nMPageCount )
			nInsertPos = nMPageCount;

		// now generate a unique name for the new masterpage
		const String aStdPrefix( SdResId(STR_LAYOUT_DEFAULT_NAME) );
		String aPrefix( aStdPrefix );

		sal_Bool bUnique = sal_True;
		sal_Int32 i = 0;
		do
		{
			bUnique = sal_True;
			for( sal_Int32 nMaster = 1; nMaster < nMPageCount; nMaster++ )
			{
				SdPage* pPage = (SdPage*)mpDoc->GetMasterPage((sal_uInt16)nMaster);
				if( pPage && pPage->GetName() == aPrefix )
				{
					bUnique = sal_False;
					break;
				}
			}

			if( !bUnique )
			{
				i++;
				aPrefix = aStdPrefix;
				aPrefix += sal_Unicode( ' ' );
				aPrefix += String::CreateFromInt32( i );
			}

		} while( !bUnique );

		String aLayoutName( aPrefix );
		aLayoutName.AppendAscii( RTL_CONSTASCII_STRINGPARAM( SD_LT_SEPARATOR ));
		aLayoutName += String(SdResId(STR_LAYOUT_OUTLINE));

		// create styles
		((SdStyleSheetPool*)mpDoc->GetStyleSheetPool())->CreateLayoutStyleSheets( aPrefix );

		// get the first page for initial size and border settings
		SdPage* pPage = mpModel->mpDoc->GetSdPage( 0, PK_STANDARD );
		SdPage* pRefNotesPage = mpModel->mpDoc->GetSdPage( 0, PK_NOTES);

		// create and instert new draw masterpage
		SdPage* pMPage = (SdPage*)mpModel->mpDoc->AllocPage(sal_True);
		pMPage->SetPageScale( pPage->GetPageScale() );
		pMPage->SetPageBorder( pPage->GetLeftPageBorder(),
						   pPage->GetTopPageBorder(),
						   pPage->GetRightPageBorder(),
						   pPage->GetBottomPageBorder() );
		pMPage->SetLayoutName( aLayoutName );
		mpDoc->InsertMasterPage(pMPage,  nInsertPos);

		{ 
			// ensure default MasterPage fill
            pMPage->EnsureMasterPageDefaultBackground();
		}

		xDrawPage = uno::Reference< drawing::XDrawPage >::query( pMPage->getUnoPage() );

		// create and instert new notes masterpage
		SdPage* pMNotesPage = (SdPage*)mpModel->mpDoc->AllocPage(sal_True);
		pMNotesPage->SetPageScale( pRefNotesPage->GetPageScale() );
		pMNotesPage->SetPageKind(PK_NOTES);
		pMNotesPage->SetPageBorder( pRefNotesPage->GetLeftPageBorder(),
								pRefNotesPage->GetTopPageBorder(),
								pRefNotesPage->GetRightPageBorder(),
								pRefNotesPage->GetBottomPageBorder() );
		pMNotesPage->SetLayoutName( aLayoutName );
		mpDoc->InsertMasterPage(pMNotesPage,  nInsertPos + 1);
//		pMNotesPage->InsertMasterPage( pMPage->GetPageNumber() );
		pMNotesPage->SetAutoLayout(AUTOLAYOUT_NOTES, sal_True, sal_True);
		mpModel->SetModified();
	}

	return( xDrawPage );
}

/******************************************************************************
* Entfernt die angegebenne SdMasterPage aus dem Model und aus der internen    *
* Liste. Dies funktioniert nur, wenn keine *normale* Seite im Model diese     *
* Seite als Masterpage benutzt.                                               *
******************************************************************************/
void SAL_CALL SdMasterPagesAccess::remove( const uno::Reference< drawing::XDrawPage >& xPage )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel || mpModel->mpDoc == NULL )
		throw lang::DisposedException();

	SdDrawDocument& rDoc = *mpModel->mpDoc;

	SdMasterPage* pSdPage = SdMasterPage::getImplementation( xPage );
	if(pSdPage == NULL)
		return;

	SdPage* pPage = dynamic_cast< SdPage* > (pSdPage->GetSdrPage());

	DBG_ASSERT( pPage && pPage->IsMasterPage(), "SdMasterPage is not masterpage?");

	if( !pPage || !pPage->IsMasterPage() || (mpModel->mpDoc->GetMasterPageUserCount(pPage) > 0))
		return; //Todo: this should be excepted

	// only standard pages can be removed directly
	if( pPage->GetPageKind() == PK_STANDARD )
	{
		sal_uInt32 nPage = pPage->GetPageNumber();

		SdPage* pNotesPage = static_cast< SdPage* >( rDoc.GetMasterPage( nPage+1 ) );

		bool bUndo = rDoc.IsUndoEnabled();
		if( bUndo )
		{
			// Add undo actions and delete the pages.  The order of adding
			// the undo actions is important.
			rDoc.BegUndo( SdResId( STR_UNDO_DELETEPAGES ) );
			rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pNotesPage));
			rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pPage));
		}

		rDoc.RemoveMasterPage( nPage );
		rDoc.RemoveMasterPage( nPage );

		if( bUndo )
		{
			rDoc.EndUndo();
		}
		else
		{
			delete pNotesPage;
			delete pPage;
		}
	}
}

// XServiceInfo
sal_Char pSdMasterPagesAccessService[sizeof("com.sun.star.drawing.MasterPages")] = "com.sun.star.drawing.MasterPages";

OUString SAL_CALL SdMasterPagesAccess::getImplementationName(  ) throw(uno::RuntimeException)
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM( "SdMasterPagesAccess" ) );
}

sal_Bool SAL_CALL SdMasterPagesAccess::supportsService( const OUString& ServiceName ) throw(uno::RuntimeException)
{
	return ServiceName.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( pSdMasterPagesAccessService ) );
}

uno::Sequence< OUString > SAL_CALL SdMasterPagesAccess::getSupportedServiceNames(  ) throw(uno::RuntimeException)
{
	OUString aService( RTL_CONSTASCII_USTRINGPARAM( pSdMasterPagesAccessService ) );
	uno::Sequence< OUString > aSeq( &aService, 1 );
	return aSeq;
}

//=============================================================================
// class SdDocLinkTargets
//=============================================================================

SdDocLinkTargets::SdDocLinkTargets( SdXImpressDocument& rMyModel ) throw()
: mpModel( &rMyModel )
{
}

SdDocLinkTargets::~SdDocLinkTargets() throw()
{
}

// XComponent
void SAL_CALL SdDocLinkTargets::dispose(  ) throw (uno::RuntimeException)
{
	mpModel = NULL;
}

void SAL_CALL SdDocLinkTargets::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

void SAL_CALL SdDocLinkTargets::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException)
{
	DBG_ERROR( "not implemented!" );
}

// XNameAccess
uno::Any SAL_CALL SdDocLinkTargets::getByName( const OUString& aName )
	throw(container::NoSuchElementException, lang::WrappedTargetException, uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	SdPage* pPage = FindPage( aName );

	if( pPage == NULL )
		throw container::NoSuchElementException();

	uno::Any aAny;

	uno::Reference< beans::XPropertySet > xProps( pPage->getUnoPage(), uno::UNO_QUERY );
	if( xProps.is() )
		aAny <<= xProps;

	return aAny;
}

uno::Sequence< OUString > SAL_CALL SdDocLinkTargets::getElementNames()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	SdDrawDocument* mpDoc = mpModel->GetDoc();
	if( mpDoc == NULL )
	{
		uno::Sequence< OUString > aSeq;
		return aSeq;
	}

	if( mpDoc->GetDocumentType() == DOCUMENT_TYPE_DRAW )
	{
		const sal_uInt32 nMaxPages = mpDoc->GetSdPageCount( PK_STANDARD );
		const sal_uInt32 nMaxMasterPages = mpDoc->GetMasterSdPageCount( PK_STANDARD );

		uno::Sequence< OUString > aSeq( nMaxPages + nMaxMasterPages );
		OUString* pStr = aSeq.getArray();

		sal_uInt32 nPage;
		// standard pages
		for( nPage = 0; nPage < nMaxPages; nPage++ )
			*pStr++ = mpDoc->GetSdPage( nPage, PK_STANDARD )->GetName();

		// master pages
		for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
			*pStr++ = mpDoc->GetMasterSdPage( nPage, PK_STANDARD )->GetName();
		return aSeq;
	}
	else
	{
		const sal_uInt32 nMaxPages = mpDoc->GetPageCount();
		const sal_uInt32 nMaxMasterPages = mpDoc->GetMasterPageCount();

		uno::Sequence< OUString > aSeq( nMaxPages + nMaxMasterPages );
		OUString* pStr = aSeq.getArray();

		sal_uInt32 nPage;
		// standard pages
		for( nPage = 0; nPage < nMaxPages; nPage++ )
			*pStr++ = ((SdPage*)mpDoc->GetPage( nPage ))->GetName();

		// master pages
		for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
			*pStr++ = ((SdPage*)mpDoc->GetMasterPage( nPage ))->GetName();
		return aSeq;
	}
}

sal_Bool SAL_CALL SdDocLinkTargets::hasByName( const OUString& aName )
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	return FindPage( aName ) != NULL;
}

// container::XElementAccess
uno::Type SAL_CALL SdDocLinkTargets::getElementType()
	throw(uno::RuntimeException)
{
	return ITYPE(beans::XPropertySet);
}

sal_Bool SAL_CALL SdDocLinkTargets::hasElements()
	throw(uno::RuntimeException)
{
	OGuard aGuard( Application::GetSolarMutex() );

	if( NULL == mpModel )
		throw lang::DisposedException();

	return mpModel->GetDoc() != NULL;
}

SdPage* SdDocLinkTargets::FindPage( const OUString& rName ) const throw()
{
	SdDrawDocument* mpDoc = mpModel->GetDoc();
	if( mpDoc == NULL )
		return NULL;

	const sal_uInt16 nMaxPages = mpDoc->GetPageCount();
	const sal_uInt16 nMaxMasterPages = mpDoc->GetMasterPageCount();

	sal_uInt16 nPage;
	SdPage* pPage;

	const String aName( rName );

	const bool bDraw = mpDoc->GetDocumentType() == DOCUMENT_TYPE_DRAW;

	// standard pages
	for( nPage = 0; nPage < nMaxPages; nPage++ )
	{
		pPage = (SdPage*)mpDoc->GetPage( nPage );
		if( (pPage->GetName() == aName) && (!bDraw || (pPage->GetPageKind() == PK_STANDARD)) )
			return pPage;
	}

	// master pages
	for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
	{
		pPage = (SdPage*)mpDoc->GetMasterPage( nPage );
		if( (pPage->GetName() == aName) && (!bDraw || (pPage->GetPageKind() == PK_STANDARD)) )
			return pPage;
	}

	return NULL;
}

// XServiceInfo
OUString SAL_CALL SdDocLinkTargets::getImplementationName()
	throw(uno::RuntimeException)
{
	return OUString( RTL_CONSTASCII_USTRINGPARAM("SdDocLinkTargets") );
}

sal_Bool SAL_CALL SdDocLinkTargets::supportsService( const OUString& ServiceName )
	throw(uno::RuntimeException)
{
	return comphelper::ServiceInfoHelper::supportsService( ServiceName, getSupportedServiceNames() );
}

uno::Sequence< OUString > SAL_CALL SdDocLinkTargets::getSupportedServiceNames()
	throw(uno::RuntimeException)
{
	const OUString aSN( RTL_CONSTASCII_USTRINGPARAM("com.sun.star.document.LinkTargets") );
	uno::Sequence< OUString > aSeq( &aSN, 1 );
	return aSeq;
}

rtl::Reference< SdXImpressDocument > SdXImpressDocument::GetModel( SdDrawDocument* pDocument )
{
	rtl::Reference< SdXImpressDocument > xRet;
	if( pDocument )
	{
		::sd::DrawDocShell* pDocShell = pDocument->GetDocSh();
		if( pDocShell )
		{
			uno::Reference<frame::XModel> xModel(pDocShell->GetModel());

			xRet.set( dynamic_cast< SdXImpressDocument* >( xModel.get() ) );
		}
	}

	return xRet;
}

void NotifyDocumentEvent( SdDrawDocument* pDocument, const rtl::OUString& rEventName )
{
	rtl::Reference< SdXImpressDocument > xModel( SdXImpressDocument::GetModel( pDocument ) );

	if( xModel.is() )
	{
		uno::Reference< uno::XInterface > xSource( static_cast<uno::XWeak*>( xModel.get() ) );
		::com::sun::star::document::EventObject aEvent( xSource, rEventName );
		xModel->notifyEvent(aEvent );
	}
}

void NotifyDocumentEvent( SdDrawDocument* pDocument, const rtl::OUString& rEventName, const uno::Reference< uno::XInterface >& xSource )
{
	rtl::Reference< SdXImpressDocument > xModel( SdXImpressDocument::GetModel( pDocument ) );

	if( xModel.is() )
	{
		::com::sun::star::document::EventObject aEvent( xSource, rEventName );
		xModel->notifyEvent(aEvent );
	}
}
