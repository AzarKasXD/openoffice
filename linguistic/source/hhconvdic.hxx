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



#ifndef _LINGUISTIC_HHConvDic_HXX_
#define _LINGUISTIC_HHConvDic_HXX_

#include <com/sun/star/linguistic2/XConversionDictionary.hpp>
#include <com/sun/star/util/XFlushable.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <cppuhelper/implbase3.hxx>
#include <cppuhelper/interfacecontainer.h>
#include <tools/string.hxx>

#include "linguistic/misc.hxx"
#include "defs.hxx"
#include "convdic.hxx"

///////////////////////////////////////////////////////////////////////////

class HHConvDic :
    public ConvDic
{
	// disallow copy-constructor and assignment-operator for now
    HHConvDic(const HHConvDic &);
    HHConvDic & operator = (const HHConvDic &);

public:
    HHConvDic( const String &rName, const String &rMainURL );
    virtual ~HHConvDic();

    // XConversionDictionary
    virtual void SAL_CALL addEntry( const ::rtl::OUString& aLeftText, const ::rtl::OUString& aRightText ) throw (::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::container::ElementExistException, ::com::sun::star::uno::RuntimeException);

    // XServiceInfo
    virtual ::rtl::OUString SAL_CALL getImplementationName(  ) throw (::com::sun::star::uno::RuntimeException);
    virtual sal_Bool SAL_CALL supportsService( const ::rtl::OUString& ServiceName ) throw (::com::sun::star::uno::RuntimeException);
    virtual ::com::sun::star::uno::Sequence< ::rtl::OUString > SAL_CALL getSupportedServiceNames(  ) throw (::com::sun::star::uno::RuntimeException);


    static inline ::rtl::OUString
        getImplementationName_Static() throw();
    static com::sun::star::uno::Sequence< ::rtl::OUString >
        getSupportedServiceNames_Static() throw();
};

inline ::rtl::OUString HHConvDic::getImplementationName_Static() throw()
{
    return A2OU( "com.sun.star.lingu2.HHConvDic" );
}

///////////////////////////////////////////////////////////////////////////

#endif

