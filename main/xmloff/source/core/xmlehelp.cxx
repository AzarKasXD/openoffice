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
#include "precompiled_xmloff.hxx"
#include <limits.h>
#include <tools/debug.hxx>
#include <tools/bigint.hxx>
#include <rtl/ustrbuf.hxx>
#include "xmlehelp.hxx"

#ifndef _XMLOFF_XMTOKEN_HXX
#include <xmloff/xmltoken.hxx>
#endif

using ::rtl::OUString;
using ::rtl::OUStringBuffer;

using namespace ::xmloff::token;

void SvXMLExportHelper::AddLength( sal_Int32 nValue, MapUnit eValueUnit,
		   			  	   		   OUStringBuffer& rOut,
								   MapUnit eOutUnit )
{
	// the sign is processed seperatly
	if( nValue < 0 )
	{
		nValue = -nValue;
		rOut.append( sal_Unicode('-') );
	}
	
	// The new length is (nVal * nMul)/(nDiv*nFac*10)
	sal_Int32 nMul = 1000;
	sal_Int32 nDiv = 1;
	sal_Int32 nFac = 100;
	enum XMLTokenEnum eUnit = XML_TOKEN_INVALID;
	switch( eValueUnit )
	{
	case MAP_TWIP:
		switch( eOutUnit )
		{
		case MAP_100TH_MM:
		case MAP_10TH_MM:
			DBG_ASSERT( MAP_INCH == eOutUnit,
						"output unit not supported for twip values" );
		case MAP_MM:
			// 0.01mm = 0.57twip (exactly)
			nMul = 25400;	// 25.4 * 1000
			nDiv = 1440;	// 72 * 20;
			nFac = 100;
			eUnit = XML_UNIT_MM;
			break;

		case MAP_CM:
			// 0.001cm = 0.57twip (exactly)
			nMul = 25400;	// 2.54 * 10000
			nDiv = 1440;	// 72 * 20;
			nFac = 1000;
			eUnit = XML_UNIT_CM;
			break;

		case MAP_POINT:
			// 0.01pt = 0.2twip (exactly)
			nMul = 1000;
			nDiv = 20;
			nFac = 100;
			eUnit = XML_UNIT_PT;
			break;

		case MAP_INCH:
		default:
			DBG_ASSERT( MAP_INCH == eOutUnit,
						"output unit not supported for twip values" );
			// 0.0001in = 0.144twip (exactly)
			nMul = 100000;
			nDiv = 1440;	// 72 * 20;
			nFac = 10000;
			eUnit = XML_UNIT_INCH;
			break;
		}
		break;

	case MAP_POINT:
		// 1pt = 1pt (exactly)
		DBG_ASSERT( MAP_POINT == eOutUnit,
					"output unit not supported for pt values" );
		nMul = 10;
		nDiv = 1;
		nFac = 1;
		eUnit = XML_UNIT_PT;
		break;
    case MAP_10TH_MM:
	case MAP_100TH_MM:
        {
            long nFac2 = (MAP_100TH_MM == eValueUnit) ? 100 : 10;
		    switch( eOutUnit )
		    {
		    case MAP_100TH_MM:
		    case MAP_10TH_MM:
			    DBG_ASSERT( MAP_INCH == eOutUnit,
						    "output unit not supported for 1/100mm values" );
		    case MAP_MM:
			    // 0.01mm = 1 mm/100 (exactly)
			    nMul = 10;	
			    nDiv = 1;	
			    nFac = nFac2;
			    eUnit = XML_UNIT_MM;
			    break;

		    case MAP_CM:
			    // 0.001mm = 1 mm/100 (exactly)
			    nMul = 10;
			    nDiv = 1;	// 72 * 20;
			    nFac = 10*nFac2;
			    eUnit = XML_UNIT_CM;
			    break;

		    case MAP_POINT:
			    // 0.01pt = 0.35 mm/100 (exactly)
			    nMul = 72000;
			    nDiv = 2540;
			    nFac = nFac2;
			    eUnit = XML_UNIT_PT;
			    break;

		    case MAP_INCH:
		    default:
			    DBG_ASSERT( MAP_INCH == eOutUnit,
						    "output unit not supported for 1/100mm values" );
			    // 0.0001in = 0.254 mm/100 (exactly)
			    nMul = 100000;
			    nDiv = 2540;
			    nFac = 100*nFac2;
			    eUnit = XML_UNIT_INCH;
			    break;
		    }
		    break;
        }
        default:
            DBG_ASSERT( 0, "input unit not handled" );
            break;
	}


	sal_Int32 nLongVal = 0;
	sal_Bool bOutLongVal = sal_True;
	if( nValue > SAL_MAX_INT32 / nMul )
	{
		// A big int is required for calculation
		BigInt nBigVal( nValue );
		nBigVal *= nMul;
		nBigVal /= nDiv;
		nBigVal += 5;
		nBigVal /= 10;

		if( nBigVal.IsLong() )
		{
			// To convert the value into a string a sal_Int32 is sufficient
			nLongVal = sal_Int32( nBigVal );
		}
		else
		{
			BigInt nBigFac( nFac );
			BigInt nBig10( 10 );
			rOut.append( (sal_Int32)(nBigVal / nBigFac) );
			if( !(nBigVal % nBigFac).IsZero() )
			{
				rOut.append( sal_Unicode('.') );
				while( nFac > 1 && !(nBigVal % nBigFac).IsZero() )
				{
					nFac /= 10;
					nBigFac = nFac;
					rOut.append( (sal_Int32)((nBigVal / nBigFac) % nBig10 ) );
				}
			}
			bOutLongVal = sal_False;
		}
	}
	else
	{
		nLongVal = nValue * nMul;
		nLongVal /= nDiv;
		nLongVal += 5;
		nLongVal /= 10;
	}

	if( bOutLongVal )
	{
		rOut.append( (sal_Int32)(nLongVal / nFac) );
		if( nFac > 1 && (nLongVal % nFac) != 0 )
		{
			rOut.append( sal_Unicode('.') );
			while( nFac > 1 && (nLongVal % nFac) != 0 )
			{
				nFac /= 10;
				rOut.append( (sal_Int32)((nLongVal / nFac) % 10) );
			}
		}
	}

	if( eUnit != XML_TOKEN_INVALID )
		rOut.append( GetXMLToken(eUnit) );
}

void SvXMLExportHelper::AddPercentage( sal_Int32 nValue, OUStringBuffer& rOut )
{
	rOut.append( nValue );
	rOut.append( sal_Unicode('%' ) );
}

double SvXMLExportHelper::GetConversionFactor(::rtl::OUStringBuffer& rUnit, 
	const MapUnit eCoreUnit, const MapUnit eDestUnit)
{
	double fRetval(1.0);
	rUnit.setLength(0L);

	if(eCoreUnit != eDestUnit)
	{
		enum XMLTokenEnum eUnit = XML_TOKEN_INVALID;

		switch(eCoreUnit)
		{
			case MAP_TWIP:
			{
				switch(eDestUnit)
				{
					case MAP_100TH_MM:
					case MAP_10TH_MM:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for twip values");
					}
					case MAP_MM:
					{
						// 0.01mm = 0.57twip (exactly)
						fRetval = ((25400.0 / 1440.0) / 1000.0);
						eUnit = XML_UNIT_MM;
						break;
					}
					case MAP_CM:
					{
						// 0.001cm = 0.57twip (exactly)
						fRetval = ((25400.0 / 1440.0) / 10000.0);
						eUnit = XML_UNIT_CM;
						break;
					}
					case MAP_POINT:
					{
						// 0.01pt = 0.2twip (exactly)
						fRetval = ((1000.0 / 20.0) / 1000.0);
						eUnit = XML_UNIT_PT;
						break;
					}
					case MAP_INCH:
					default:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for twip values");
						// 0.0001in = 0.144twip (exactly)
						fRetval = ((100000.0 / 1440.0) / 100000.0);
						eUnit = XML_UNIT_INCH;
						break;
					}
				}
				break;
			}
			case MAP_POINT:
			{
                switch(eDestUnit)
                {
                    case MAP_MM:
                        // 1mm = 72 / 25.4 pt (exactly)
                        fRetval = ( 25.4 / 72.0 );
                        eUnit = XML_UNIT_MM;
                        break;
                        
                    case MAP_CM:
                        // 1cm = 72 / 2.54 pt (exactly)
                        fRetval = ( 2.54 / 72.0 );
                        eUnit = XML_UNIT_CM;
                        break;

                    case MAP_TWIP:
                        // 1twip = 72 / 1440 pt (exactly)
                        fRetval = 20.0;     // 1440.0 / 72.0
                        eUnit = XML_UNIT_PC;
                        break;

                    case MAP_INCH:
                    default:
                        DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for pt values");
                        // 1in = 72 pt (exactly)
                        fRetval = ( 1.0 / 72.0 );
                        eUnit = XML_UNIT_INCH;
                        break;
                }
                break;
			}
            case MAP_10TH_MM:
			{
				switch(eDestUnit)
				{
					case MAP_100TH_MM:
					case MAP_10TH_MM:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for 1/100mm values");
					}
					case MAP_MM:
					{
						// 0.01mm = 1 mm/100 (exactly)
						fRetval = ((10.0 / 1.0) / 100.0);
						eUnit = XML_UNIT_MM;
						break;
					}
					case MAP_CM:
					{
						// 0.001mm = 1 mm/100 (exactly)
						fRetval = ((10.0 / 1.0) / 1000.0);
						eUnit = XML_UNIT_CM;
						break;
					}
					case MAP_POINT:
					{
						// 0.01pt = 0.35 mm/100 (exactly)
						fRetval = ((72000.0 / 2540.0) / 100.0);
						eUnit = XML_UNIT_PT;
						break;
					}
					case MAP_INCH:
					default:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for 1/100mm values");
						// 0.0001in = 0.254 mm/100 (exactly)
						fRetval = ((100000.0 / 2540.0) / 10000.0);
						eUnit = XML_UNIT_INCH;
						break;
					}
				}
				break;
            }
			case MAP_100TH_MM:
			{
				switch(eDestUnit)
				{
					case MAP_100TH_MM:
					case MAP_10TH_MM:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for 1/100mm values");
					}
					case MAP_MM:
					{
						// 0.01mm = 1 mm/100 (exactly)
						fRetval = ((10.0 / 1.0) / 1000.0);
						eUnit = XML_UNIT_MM;
						break;
					}
					case MAP_CM:
					{
						// 0.001mm = 1 mm/100 (exactly)
						fRetval = ((10.0 / 1.0) / 10000.0);
						eUnit = XML_UNIT_CM;
						break;
					}
					case MAP_POINT:
					{
						// 0.01pt = 0.35 mm/100 (exactly)
						fRetval = ((72000.0 / 2540.0) / 1000.0);
						eUnit = XML_UNIT_PT;
						break;
					}
					case MAP_INCH:
					default:
					{
						DBG_ASSERT(MAP_INCH == eDestUnit, "output unit not supported for 1/100mm values");
						// 0.0001in = 0.254 mm/100 (exactly)
						fRetval = ((100000.0 / 2540.0) / 100000.0);
						eUnit = XML_UNIT_INCH;
						break;
					}
				}
				break;
			}
			default:
				DBG_ERROR("xmloff::SvXMLExportHelper::GetConversionFactor(), illegal eCoreUnit value!");
				break;
		}

		if(eUnit != XML_TOKEN_INVALID)
			rUnit.append(GetXMLToken(eUnit));
	}

	return fRetval;
}

MapUnit SvXMLExportHelper::GetUnitFromString(const ::rtl::OUString& rString, MapUnit eDefaultUnit)
{
    sal_Int32 nPos = 0;
	sal_Int32 nLen = rString.getLength();
	MapUnit eRetUnit = eDefaultUnit;

	// skip white space
	while( nPos < nLen && sal_Unicode(' ') == rString[nPos] )
		nPos++;

	// skip negative
	if( nPos < nLen && sal_Unicode('-') == rString[nPos] )
		nPos++;

	// skip number
	while( nPos < nLen && sal_Unicode('0') <= rString[nPos] && sal_Unicode('9') >= rString[nPos] )
		nPos++;

	if( nPos < nLen && sal_Unicode('.') == rString[nPos] )
	{
		nPos++;
		while( nPos < nLen && sal_Unicode('0') <= rString[nPos] && sal_Unicode('9') >= rString[nPos] )
			nPos++;
	}

	// skip white space
	while( nPos < nLen && sal_Unicode(' ') == rString[nPos] )
		nPos++;

	if( nPos < nLen )
	{
		switch(rString[nPos])
		{
			case sal_Unicode('%') :
			{
				eRetUnit = MAP_RELATIVE;
				break;
			}
			case sal_Unicode('c'):
			case sal_Unicode('C'):
			{
				if(nPos+1 < nLen && (rString[nPos+1] == sal_Unicode('m') 
					|| rString[nPos+1] == sal_Unicode('M')))
					eRetUnit = MAP_CM;
				break;
			}
			case sal_Unicode('e'):
			case sal_Unicode('E'):
			{
				// CSS1_EMS or CSS1_EMX later
				break;
			}
			case sal_Unicode('i'):
			case sal_Unicode('I'):
			{
				if(nPos+1 < nLen && (rString[nPos+1] == sal_Unicode('n') 
					|| rString[nPos+1] == sal_Unicode('n')))
					eRetUnit = MAP_INCH;
				break;
			}
			case sal_Unicode('m'):
			case sal_Unicode('M'):
			{
				if(nPos+1 < nLen && (rString[nPos+1] == sal_Unicode('m') 
					|| rString[nPos+1] == sal_Unicode('M')))
					eRetUnit = MAP_MM;
				break;
			}
			case sal_Unicode('p'):
			case sal_Unicode('P'):
			{
				if(nPos+1 < nLen && (rString[nPos+1] == sal_Unicode('t') 
					|| rString[nPos+1] == sal_Unicode('T')))
					eRetUnit = MAP_POINT;
				if(nPos+1 < nLen && (rString[nPos+1] == sal_Unicode('c') 
					|| rString[nPos+1] == sal_Unicode('C')))
					eRetUnit = MAP_TWIP;
				break;
			}
		}
	}

	return eRetUnit;
}
