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



#include "XMLScanner.hxx"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <rtftok/RTFScanner.hxx>
#include <rtftok/RTFScannerHandler.hxx>
#include <com/sun/star/io/XStream.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/io/XSeekable.hpp>
#include <com/sun/star/io/XTruncate.hpp>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <ucbhelper/contentbroker.hxx>
#include <com/sun/star/ucb/XSimpleFileAccess.hpp>
#include <osl/process.h>
#include <rtl/string.hxx>
#include <hash_set>
#include <assert.h>
#include <string>
#include <cppuhelper/implbase2.hxx>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/embed/XStorage.hpp>
#include <com/sun/star/util/XCloseable.hpp>
#include <comphelper/storagehelper.hxx>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <comphelper/seqstream.hxx>

#include <ctype.h>
#include <iostream>

using namespace ::com::sun::star;
using namespace ::std;

namespace writerfilter { namespace rtftok {

const sal_Char XMLScanner::SERVICE_NAME[40] = "debugservices.rtftok.XMLScanner";
const sal_Char XMLScanner::IMPLEMENTATION_NAME[40] = "debugservices.rtftok.XMLScanner";

class XmlRtfScannerHandler : public writerfilter::rtftok::RTFScannerHandler
{
	std::vector<unsigned char> binBuffer;
	int objDataLevel;
	int numOfOLEs;
	unsigned char hb;
	int numOfOLEChars;
	uno::Reference<lang::XMultiServiceFactory> xServiceFactory;
	uno::Reference<com::sun::star::ucb::XSimpleFileAccess> xFileAccess;
    string charBuffer;
    string ucharBuffer;
    vector<string> vCloseTags;

    void xmlout(const string & str)
    {
        for (size_t n = 0; n < str.length(); ++n)
        {
            char c = str[n];

            switch (c)
            {
            case '<': cout << "&lt;"; break;
            case '>': cout << "&gt;"; break;
            case '&': cout << "&amp;"; break;
            default:
                cout << c;

                break;
            }
        }
    }

    void clearBuffers()
    {
        if (charBuffer.length() > 0)
        {
            cout << "<text>";
            xmlout(charBuffer);
            cout << "</text>" << endl;
        }

        charBuffer = "";

        if (ucharBuffer.length() > 0)
        {
            cout << "<utext>";
            xmlout(ucharBuffer);
            cout << ucharBuffer << "</utext>" << endl;
        }

        ucharBuffer = "";
    }

	void dest(char* token, char* value)
	{
        clearBuffers();
        cout << "<dest name=\"" << token << "\" value=\""<< value << "\">" << endl;
        vCloseTags.push_back("</dest>");
	}
	void ctrl(char*token, char* value)
	{
        clearBuffers();
        cout << "<" << token << ">" << value << "</" << token << ">"
             << endl;
	}
	void lbrace(void)
	{
        clearBuffers();
        cout << "<brace>" << endl;
        vCloseTags.push_back("</brace>");
	}
	void rbrace(void)
	{
        clearBuffers();
        cout << vCloseTags.back() << endl;
        vCloseTags.pop_back();
	}
	void addSpaces(int count)
	{
        clearBuffers();
        cout << "<spaces count=\"" << count << "\"/>" << endl;
	}
	void addBinData(unsigned char /*data*/)
	{
        clearBuffers();
        cout << "<bindata/>" << endl;
	}
	void addChar(char ch)
	{
        charBuffer += ch;
	}
	void addCharU(sal_Unicode ch)
	{
        ucharBuffer += sal_Char(ch < 128 ? ch : '.');
	}
	void addHexChar(char* hexch)
	{
        clearBuffers();
        cout << "<hexchar value=\"" << hexch << "\"/>" << endl;
	}

public:
	XmlRtfScannerHandler(uno::Reference<lang::XMultiServiceFactory> &xServiceFactory_, uno::Reference<com::sun::star::ucb::XSimpleFileAccess> &xFileAccess_) :
            objDataLevel(0), numOfOLEs(0), hb(' '), numOfOLEChars(0),
    xServiceFactory(xServiceFactory_),
    xFileAccess(xFileAccess_)
	{
	}

    virtual ~XmlRtfScannerHandler() {}

	void dump()
	{
	}
};

class RtfInputSourceImpl : public rtftok::RTFInputSource
{
private:
	uno::Reference< io::XInputStream > xInputStream;
	uno::Reference< io::XSeekable > xSeekable;
	uno::Reference< task::XStatusIndicator > xStatusIndicator;
	sal_Int64 bytesTotal;
	sal_Int64 bytesRead;
public:
	RtfInputSourceImpl(uno::Reference< io::XInputStream > &xInputStream_, uno::Reference< task::XStatusIndicator > &xStatusIndicator_) :
	  xInputStream(xInputStream_),
	  xStatusIndicator(xStatusIndicator_),
	  bytesRead(0)
	{
		xSeekable=uno::Reference< io::XSeekable >(xInputStream, uno::UNO_QUERY);
		if (xSeekable.is())
			bytesTotal=xSeekable->getLength();
		if (xStatusIndicator.is() && xSeekable.is())
		{
			xStatusIndicator->start(::rtl::OUString::createFromAscii("Converting"), 100);
		}
	}

    virtual ~RtfInputSourceImpl() {}

	int read(void *buf, int maxlen)
	{
		uno::Sequence< sal_Int8 > buffer;
		int len=xInputStream->readSomeBytes(buffer,maxlen);
		if (len>0)
		{
			sal_Int8 *_buffer=buffer.getArray();
			memcpy(buf, _buffer, len);
			bytesRead+=len;
			if (xStatusIndicator.is())
			{
				if (xSeekable.is())
				{
					xStatusIndicator->setValue((int)(bytesRead*100/bytesTotal));
				}
				else
				{
					char buf1[100];
					sprintf(buf1, "Converted %" SAL_PRIdINT64 " KB", bytesRead/1024);
					xStatusIndicator->start(::rtl::OUString::createFromAscii(buf1), 0);
				}
			}
			return len;
		}
		else
		{
			if (xStatusIndicator.is())
			{
				xStatusIndicator->end();
			}
			return 0;
		}
	}
};

XMLScanner::XMLScanner(const uno::Reference< uno::XComponentContext > &xContext_) :
xContext( xContext_ )
{
}

sal_Int32 SAL_CALL XMLScanner::run( const uno::Sequence< rtl::OUString >& aArguments ) throw (uno::RuntimeException)
{
  	uno::Sequence<uno::Any> aUcbInitSequence(2);
	aUcbInitSequence[0] <<= rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Local"));
	aUcbInitSequence[1] <<= rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Office"));
	uno::Reference<lang::XMultiServiceFactory> xServiceFactory(xContext->getServiceManager(), uno::UNO_QUERY_THROW);
	uno::Reference<lang::XMultiComponentFactory> xFactory(xContext->getServiceManager(), uno::UNO_QUERY_THROW );
    if (::ucbhelper::ContentBroker::initialize(xServiceFactory, aUcbInitSequence))
	{
			rtl::OUString arg=aArguments[0];

			uno::Reference<com::sun::star::ucb::XSimpleFileAccess> xFileAccess(
			xFactory->createInstanceWithContext(
				::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.ucb.SimpleFileAccess")),
				xContext), uno::UNO_QUERY_THROW );

			rtl_uString *dir=NULL;
			osl_getProcessWorkingDir(&dir);
			rtl::OUString absFileUrl;
			osl_getAbsoluteFileURL(dir, arg.pData, &absFileUrl.pData);
			rtl_uString_release(dir);

			uno::Reference <lang::XSingleServiceFactory> xStorageFactory(
				xServiceFactory->createInstance (rtl::OUString::createFromAscii("com.sun.star.embed.StorageFactory")), uno::UNO_QUERY_THROW);

#if 0
			rtl::OUString outFileUrl;
			{
			rtl_uString *dir1=NULL;
			osl_getProcessWorkingDir(&dir1);
			osl_getAbsoluteFileURL(dir1, aArguments[1].pData, &outFileUrl.pData);
			rtl_uString_release(dir1);
			}

			uno::Sequence< uno::Any > aArgs( 2 );
			aArgs[0] <<= outFileUrl;
			aArgs[1] <<= embed::ElementModes::READWRITE | embed::ElementModes::TRUNCATE;
			uno::Reference<embed::XStorage> xStorage(xStorageFactory->createInstanceWithArguments(aArgs), uno::UNO_QUERY_THROW);
			uno::Reference<beans::XPropertySet> xPropSet(xStorage, uno::UNO_QUERY_THROW);
			xPropSet->setPropertyValue(rtl::OUString::createFromAscii("MediaType"), uno::makeAny(rtl::OUString::createFromAscii("application/vnd.oasis.opendocument.text")));
#endif
			uno::Reference<io::XInputStream> xInputStream = xFileAccess->openFileRead(absFileUrl);
			uno::Reference< task::XStatusIndicator > xStatusIndicator;

			RtfInputSourceImpl rtfInputSource(xInputStream, xStatusIndicator);
			XmlRtfScannerHandler eventHandler(xServiceFactory, xFileAccess);
			writerfilter::rtftok::RTFScanner *rtfScanner=writerfilter::rtftok::RTFScanner::createRTFScanner(rtfInputSource, eventHandler);

            cout << "<out>" << endl;
			rtfScanner->yylex();
            cout << "</out>" << endl;
			delete rtfScanner;

        ::ucbhelper::ContentBroker::deinitialize();
	}
	else
	{
		fprintf(stderr, "can't initialize UCB");
	}
	return 0;
}

::rtl::OUString XMLScanner_getImplementationName ()
{
	return rtl::OUString::createFromAscii ( XMLScanner::IMPLEMENTATION_NAME );
}

sal_Bool SAL_CALL XMLScanner_supportsService( const ::rtl::OUString& ServiceName )
{
	return ServiceName.equals( rtl::OUString::createFromAscii( XMLScanner::SERVICE_NAME ) );
}
uno::Sequence< rtl::OUString > SAL_CALL XMLScanner_getSupportedServiceNames(  ) throw (uno::RuntimeException)
{
	uno::Sequence < rtl::OUString > aRet(1);
	rtl::OUString* pArray = aRet.getArray();
	pArray[0] =  rtl::OUString::createFromAscii ( XMLScanner::SERVICE_NAME );
	return aRet;
}

uno::Reference< uno::XInterface > SAL_CALL XMLScanner_createInstance( const uno::Reference< uno::XComponentContext > & xContext) throw( uno::Exception )
{
	return (cppu::OWeakObject*) new XMLScanner( xContext );
}

} } /* end namespace writerfilter::rtftok */
