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



#include "ReportDrawPage.hxx"
#include "RptObject.hxx"
#include "RptModel.hxx"
#include "RptDef.hxx"
#include "corestrings.hrc"
#include <comphelper/mimeconfighelper.hxx>
#include <comphelper/classids.hxx>
#include <comphelper/embeddedobjectcontainer.hxx>
#include <comphelper/documentconstants.hxx>

#include <svx/svdmodel.hxx>
#include <com/sun/star/report/XFixedLine.hpp>
#include <com/sun/star/beans/NamedValue.hpp>

#include <tools/diagnose_ex.h>
#include <svx/unoshape.hxx>
#include <svx/svdlegacy.hxx>

namespace reportdesign
{
    using namespace ::com::sun::star;
    using namespace rptui;

OReportDrawPage::OReportDrawPage(SdrPage* _pPage
                                 ,const uno::Reference< report::XSection >& _xSection)
: SvxDrawPage(_pPage)
,m_xSection(_xSection)
{
}

SdrObject* OReportDrawPage::_CreateSdrObject( const uno::Reference< drawing::XShape > & xDescr ) throw ()
{
    uno::Reference< report::XReportComponent> xReportComponent(xDescr,uno::UNO_QUERY);
    if ( xReportComponent.is() )
        return OObjectBase::createObject(&GetSdrPage()->getSdrModelFromSdrPage(), xReportComponent);
	return SvxDrawPage::_CreateSdrObject( xDescr );
}

uno::Reference< drawing::XShape >  OReportDrawPage::_CreateShape( SdrObject *pObj ) const throw ()
{
    OObjectBase* pBaseObj = dynamic_cast<OObjectBase*>(pObj);
    if ( !pBaseObj )
	    return SvxDrawPage::_CreateShape( pObj );

    uno::Reference< report::XSection> xSection = m_xSection;
    uno::Reference< lang::XMultiServiceFactory> xFactory;
    if ( xSection.is() )
        xFactory.set(xSection->getReportDefinition(),uno::UNO_QUERY);
    uno::Reference< drawing::XShape > xRet;
	uno::Reference< drawing::XShape > xShape;
    if ( xFactory.is() )
    {
        bool bChangeOrientation = false;
        ::rtl::OUString sServiceName = pBaseObj->getServiceName();
        OSL_ENSURE(sServiceName.getLength(),"No Service Name given!");
        OUnoObject* pUnoObj = dynamic_cast< OUnoObject* >(pObj);

        if ( pUnoObj )
        {
            if ( pUnoObj->GetObjIdentifier() == OBJ_DLG_FIXEDTEXT )
            {
                uno::Reference<beans::XPropertySet> xControlModel(pUnoObj->GetUnoControlModel(),uno::UNO_QUERY);
                if ( xControlModel.is() )
                    xControlModel->setPropertyValue( PROPERTY_MULTILINE,uno::makeAny(sal_True));
            }
            else
                bChangeOrientation = pUnoObj->GetObjIdentifier() == OBJ_DLG_HFIXEDLINE;

            SvxShapeControl* pShape = new SvxShapeControl( pObj );
            xShape.set(*pShape,uno::UNO_QUERY);
            pShape->setSvxShapeKind(SdrObjectCreatorInventorToSvxShapeKind(pObj->GetObjIdentifier(), pObj->GetObjInventor()));
        }
		else if ( dynamic_cast< OCustomShape* >(pObj) )
		{
			SvxCustomShape* pShape = new SvxCustomShape( pObj );
            uno::Reference < drawing::XEnhancedCustomShapeDefaulter > xShape2 = pShape;
            xShape.set(xShape2,uno::UNO_QUERY);
			pShape->setSvxShapeKind(SdrObjectCreatorInventorToSvxShapeKind(pObj->GetObjIdentifier(), pObj->GetObjInventor()));
		}
        else 
        {
            SdrOle2Obj* pOle2Obj = dynamic_cast<SdrOle2Obj*>(pObj);
			
			if ( pOle2Obj )
			{
            if ( !pOle2Obj->GetObjRef().is() )
            {
                sal_Int64 nAspect = embed::Aspects::MSOLE_CONTENT;
                uno::Reference < embed::XEmbeddedObject > xObj;
                ::rtl::OUString sName;
					xObj = pObj->getSdrModelFromSdrObject().GetPersist()->getEmbeddedObjectContainer().CreateEmbeddedObject( 
                    ::comphelper::MimeConfigurationHelper::GetSequenceClassIDRepresentation(
                    ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("80243D39-6741-46C5-926E-069164FF87BB"))), sName );
                OSL_ENSURE(xObj.is(),"Embedded Object could not be created!");

                /**************************************************
                * Das leere OLE-Objekt bekommt ein neues IPObj
                **************************************************/
                pObj->SetEmptyPresObj(sal_False);
                pOle2Obj->SetOutlinerParaObject(NULL);
                pOle2Obj->SetObjRef(xObj);
                pOle2Obj->SetPersistName(sName);
                pOle2Obj->SetName(sName);
                pOle2Obj->SetAspect(nAspect);
					const Rectangle aRect(sdr::legacy::GetLogicRect(*pOle2Obj));

                Size aTmp = aRect.GetSize();
            	awt::Size aSz( aTmp.Width(), aTmp.Height() );
                xObj->setVisualAreaSize( nAspect, aSz );
            }
			SvxOle2Shape* pShape = new SvxOle2Shape( pObj );
            xShape.set(*pShape,uno::UNO_QUERY);
			pShape->setSvxShapeKind(SdrObjectCreatorInventorToSvxShapeKind(pObj->GetObjIdentifier(), pObj->GetObjInventor()));
			//xShape = new SvxOle2Shape( pOle2Obj );
        }
		}

		if ( !xShape.is() )
			xShape.set( SvxDrawPage::_CreateShape( pObj ) );

        try
        {
            OReportModel& rRptModel = static_cast< OReportModel& >(pObj->getSdrModelFromSdrObject());
            xRet.set( rRptModel.createShape(sServiceName,xShape,bChangeOrientation ? 0 : 1), uno::UNO_QUERY_THROW );
        }
        catch( const uno::Exception& )
        {
            DBG_UNHANDLED_EXCEPTION();
        }
    }

    return xRet;
}

}
