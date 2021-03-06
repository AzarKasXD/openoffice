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
#include "precompiled_stoc.hxx"

#include "methoddescription.hxx"

#include "com/sun/star/container/NoSuchElementException.hpp"
#include "com/sun/star/container/XHierarchicalNameAccess.hpp"
#include "com/sun/star/reflection/XParameter.hpp"
#include "com/sun/star/reflection/XTypeDescription.hpp"
#include "com/sun/star/uno/Reference.hxx"
#include "com/sun/star/uno/RuntimeException.hpp"
#include "com/sun/star/uno/Sequence.hxx"
#include "cppuhelper/implbase1.hxx"
#include "cppuhelper/weak.hxx"
#include "osl/mutex.hxx"
#include "registry/reader.hxx"
#include "registry/types.h"
#include "rtl/ustring.hxx"
#include "sal/types.h"

namespace css = com::sun::star;

using stoc::registry_tdprovider::MethodDescription;

namespace {

class Parameter: public cppu::WeakImplHelper1< css::reflection::XParameter > {
public:
    Parameter(
        css::uno::Reference< css::container::XHierarchicalNameAccess > const &
            manager,
        rtl::OUString const & name, rtl::OUString const & typeName,
        RTParamMode mode, sal_Int32 position):
        m_manager(manager), m_name(name),
        m_typeName(typeName.replace('/', '.')), m_mode(mode),
        m_position(position) {}

    virtual ~Parameter() {}

    virtual rtl::OUString SAL_CALL getName() throw (css::uno::RuntimeException)
    { return m_name; }

    virtual css::uno::Reference< css::reflection::XTypeDescription > SAL_CALL
    getType() throw (css::uno::RuntimeException);

    virtual sal_Bool SAL_CALL isIn() throw (css::uno::RuntimeException)
    { return (m_mode & RT_PARAM_IN) != 0; }

    virtual sal_Bool SAL_CALL isOut() throw (css::uno::RuntimeException)
    { return (m_mode & RT_PARAM_OUT) != 0; }

    virtual sal_Int32 SAL_CALL getPosition() throw (css::uno::RuntimeException)
    { return m_position; }

    virtual sal_Bool SAL_CALL isRestParameter()
        throw (css::uno::RuntimeException)
    { return (m_mode & RT_PARAM_REST) != 0; }

private:
    Parameter(Parameter &); // not implemented
    void operator =(Parameter); // not implemented

    css::uno::Reference< css::container::XHierarchicalNameAccess > m_manager;
    rtl::OUString m_name;
    rtl::OUString m_typeName;
    RTParamMode m_mode;
    sal_Int32 m_position;
};

css::uno::Reference< css::reflection::XTypeDescription > Parameter::getType()
    throw (css::uno::RuntimeException)
{
    try {
        return css::uno::Reference< css::reflection::XTypeDescription >(
            m_manager->getByHierarchicalName(m_typeName),
            css::uno::UNO_QUERY_THROW);
    } catch (css::container::NoSuchElementException & e) {
        throw new css::uno::RuntimeException(
            (rtl::OUString(
                RTL_CONSTASCII_USTRINGPARAM(
                    "com.sun.star.container.NoSuchElementException: "))
             + e.Message),
            static_cast< cppu::OWeakObject * >(this));
    }
}

}

MethodDescription::MethodDescription(
    css::uno::Reference< css::container::XHierarchicalNameAccess > const &
        manager,
    rtl::OUString const & name,
    com::sun::star::uno::Sequence< sal_Int8 > const & bytes,
    sal_uInt16 index):
    FunctionDescription(manager, bytes, index), m_name(name),
    m_parametersInit(false)
{}

MethodDescription::~MethodDescription() {}

css::uno::Sequence< css::uno::Reference< css::reflection::XParameter > >
MethodDescription::getParameters() const {
    osl::MutexGuard guard(m_mutex);
    if (!m_parametersInit) {
        typereg::Reader reader(getReader());
        sal_uInt16 n = reader.getMethodParameterCount(m_index);
        m_parameters.realloc(n);
        for (sal_uInt16 i = 0; i < n; ++i) {
            m_parameters[i] = new Parameter(
                m_manager, reader.getMethodParameterName(m_index, i),
                reader.getMethodParameterTypeName(m_index, i),
                reader.getMethodParameterFlags(m_index, i), i);
        }
        m_parametersInit = true;
    }
    return m_parameters;
}
