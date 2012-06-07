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



#ifndef _SVX_FMGLOB_HXX
#define _SVX_FMGLOB_HXX

#include <tools/solar.h>
#include <svx/svdobj.hxx>
#include <com/sun/star/form/FormComponentType.hpp>

// TTTT: Needed at all? SdrObjKind == OBJ_UNO for all these anyways
const sal_uInt32 FmFormInventor = sal_uInt32('F')*0x00000001+
							  sal_uInt32('M')*0x00000100+
							  sal_uInt32('0')*0x00010000+
							  sal_uInt32('1')*0x01000000;

const sal_uInt16 OBJ_FM_CONTROL			=	::com::sun::star::form::FormComponentType::CONTROL;
																	// fuer FormularKomponenten
const sal_uInt16 OBJ_FM_EDIT			=	::com::sun::star::form::FormComponentType::TEXTFIELD;
const sal_uInt16 OBJ_FM_BUTTON			=	::com::sun::star::form::FormComponentType::COMMANDBUTTON;
const sal_uInt16 OBJ_FM_FIXEDTEXT		=	::com::sun::star::form::FormComponentType::FIXEDTEXT;
const sal_uInt16 OBJ_FM_LISTBOX			=	::com::sun::star::form::FormComponentType::LISTBOX;
const sal_uInt16 OBJ_FM_CHECKBOX		=	::com::sun::star::form::FormComponentType::CHECKBOX;
const sal_uInt16 OBJ_FM_COMBOBOX		=	::com::sun::star::form::FormComponentType::COMBOBOX;
const sal_uInt16 OBJ_FM_RADIOBUTTON		=	::com::sun::star::form::FormComponentType::RADIOBUTTON;
const sal_uInt16 OBJ_FM_GROUPBOX		=	::com::sun::star::form::FormComponentType::GROUPBOX;
const sal_uInt16 OBJ_FM_GRID			=	::com::sun::star::form::FormComponentType::GRIDCONTROL;
const sal_uInt16 OBJ_FM_IMAGEBUTTON		=	::com::sun::star::form::FormComponentType::IMAGEBUTTON;
const sal_uInt16 OBJ_FM_FILECONTROL		=	::com::sun::star::form::FormComponentType::FILECONTROL;
const sal_uInt16 OBJ_FM_DATEFIELD		=	::com::sun::star::form::FormComponentType::DATEFIELD;
const sal_uInt16 OBJ_FM_TIMEFIELD		=	::com::sun::star::form::FormComponentType::TIMEFIELD;
const sal_uInt16 OBJ_FM_NUMERICFIELD	=	::com::sun::star::form::FormComponentType::NUMERICFIELD;
const sal_uInt16 OBJ_FM_CURRENCYFIELD	=	::com::sun::star::form::FormComponentType::CURRENCYFIELD;
const sal_uInt16 OBJ_FM_PATTERNFIELD	=	::com::sun::star::form::FormComponentType::PATTERNFIELD;
const sal_uInt16 OBJ_FM_HIDDEN			=	::com::sun::star::form::FormComponentType::HIDDENCONTROL;
const sal_uInt16 OBJ_FM_IMAGECONTROL	=	::com::sun::star::form::FormComponentType::IMAGECONTROL;
const sal_uInt16 OBJ_FM_FORMATTEDFIELD	=	::com::sun::star::form::FormComponentType::PATTERNFIELD + 1;
const sal_uInt16 OBJ_FM_SCROLLBAR       =   ::com::sun::star::form::FormComponentType::PATTERNFIELD + 2;
const sal_uInt16 OBJ_FM_SPINBUTTON      =   ::com::sun::star::form::FormComponentType::PATTERNFIELD + 3;
const sal_uInt16 OBJ_FM_NAVIGATIONBAR   =   ::com::sun::star::form::FormComponentType::PATTERNFIELD + 4;

#endif      // _FM_FMGLOB_HXX

