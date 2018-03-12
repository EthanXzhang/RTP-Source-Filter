// RTPFilter.cpp : 定义 DLL 应用程序的导出函数。
//
#include "stdafx.h"
#include <initguid.h>
#include <olectl.h>
#include "RTPFilteruid.h"
#include "RTPFilter.h"
#if (1100 > _MSC_VER)
#include <olectlid.h>
#endif
#include "CRTPReceiver.h"
//
// setup data
//
//注册时候的信息
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
	&MEDIATYPE_NULL,            // Major type
	&MEDIASUBTYPE_NULL          // Minor type
};
//注册时候的信息
const AMOVIESETUP_PIN psudPins[] =
{
	{
		L"Output",           // String pin name
		FALSE,              // Is it rendered
		TRUE,              // Is it an output
		FALSE,              // Allowed none
		FALSE,              // Allowed many
		&CLSID_NULL,        // Connects to filter
		L"Input",          // Connects to pin
		1,                  // Number of types
		&sudPinTypes }
};

//注册时候的信息
const AMOVIESETUP_FILTER sudFilter =
{
	&CLSID_RTPSourceFilter,       // Filter CLSID
	L"RTP Source Filter",    // Filter name
	MERIT_DO_NOT_USE,        // Its merit
	1,                       // Number of pins
	psudPins                 // Pin details
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance
//注意g_Templates名称是固定的
CFactoryTemplate g_Templates[1] =
{
	{
		L"RTP Source Filter",
		&CLSID_RTPSourceFilter,
		RTPSource::CreateInstance,
		NULL,
		&sudFilter
	}
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


// ----------------------------------------------------------------------------
//            Filter implementation
// ----------------------------------------------------------------------------
RTPSource::RTPSource(LPUNKNOWN lpunk, HRESULT *phr) :
CSource(NAME("RTP Source Filter"), lpunk, CLSID_RTPSourceFilter)
{
	ASSERT(phr);
	CAutoLock cAutoLock(&m_cStateLock);
	m_paStreams = (CSourceStream **) new RTPSourceStream*[1];
	if (m_paStreams == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;

		return;
	}

	m_paStreams[0] = new RTPSourceStream(phr, this, L"H264 Stream");
	if (m_paStreams[0] == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}
}
	

CUnknown * WINAPI RTPSource::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
	ASSERT(phr);

	CUnknown *punk = new RTPSource(lpunk, phr);
	if (punk == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
	}
	return punk;
}
RTPSourceStream::RTPSourceStream(HRESULT *phr,
	RTPSource *pParent,
	LPCWSTR pPinName) :
	CSourceStream(NAME("Bouncing Ball"), phr, pParent, pPinName)
{
	ASSERT(phr);
	CAutoLock cAutoLock(&m_cSharedState);
	WSAStartup(MAKEWORD(2, 2), &dat);
	for (int i = 0; i < 3; i++)
	{
		startcode[i] = 0x00;
	}
	startcode[3] = 0x01;
	m_nCount = 0;
}
RTPSourceStream::~RTPSourceStream()
{
}


HRESULT RTPSourceStream::GetMediaType(int iPosition, CMediaType *pmt)
{
	CheckPointer(pmt, E_POINTER);
	if (iPosition < 0)
	{
		return E_INVALIDARG;
	}
	if (iPosition >= 1)
	{
		return VFW_S_NO_MORE_ITEMS;
	}
	VIDEOINFO *pvi = (VIDEOINFO *)pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
	if (NULL == pvi)
		return(E_OUTOFMEMORY);
	ZeroMemory(pvi, sizeof(VIDEOINFO));
	pvi->dwBitRate = 444334080;
	pvi->AvgTimePerFrame = 166666;
	pvi->bmiHeader.biSize = 40;
	pvi->bmiHeader.biWidth = 640;
	pvi->bmiHeader.biHeight = 480;
	pvi->bmiHeader.biBitCount = 24;
	pvi->bmiHeader.biCompression = 0x34363248;
	pvi->bmiHeader.biSizeImage = 925696;
	SetRectEmpty(&pvi->rcSource);
	SetRectEmpty(&pvi->rcTarget);

	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);
	pmt->SetTemporalCompression(TRUE);
	pmt->SetSubtype(&MEDIASUBTYPE_H264);
	pmt->SetSampleSize(614400);
	return NOERROR;
}
HRESULT RTPSourceStream::SetMediaType(const CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// Pass the call up to my base class

	HRESULT hr = CSourceStream::SetMediaType(pMediaType);
	return hr;
}
HRESULT RTPSourceStream::DecideBufferSize(IMemAllocator *pAlloc,
	ALLOCATOR_PROPERTIES *pProperties)
{
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(pProperties, E_POINTER);

	CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = NOERROR;

	VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();
	pProperties->cBuffers = 1;
	pProperties->cbBuffer = 925696;

	ASSERT(pProperties->cbBuffer);

	// Ask the allocator to reserve us some sample memory, NOTE the function
	// can succeed (that is return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted

	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProperties, &Actual);
	if (FAILED(hr))
	{
		return hr;
	}

	// Is this allocator unsuitable

	if (Actual.cbBuffer < pProperties->cbBuffer)
	{
		return E_FAIL;
	}

	// Make sure that we have only 1 buffer (we erase the ball in the
	// old buffer to save having to zero a 200k+ buffer every time
	// we draw a frame)

	ASSERT(Actual.cBuffers == 1);
	return NOERROR;

} // DecideBufferSize
HRESULT RTPSourceStream::CheckMediaType(const CMediaType *pMediaType)
{
	CheckPointer(pMediaType, E_POINTER);

	if ((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
		!(pMediaType->IsFixedSize()))                   // in fixed size samples
	{
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID *SubType = pMediaType->Subtype();
	if (SubType == NULL)
		return E_INVALIDARG;

	if ((*SubType != MEDIASUBTYPE_H264))
	{
		return E_INVALIDARG;
	}
	return S_OK;  // This format is acceptable.
}
//*****************************DoRenderSample——发送RTP*****************************
//DoRenderSample——发送RTP
//*********************************主要的实现*********************************
HRESULT RTPSourceStream::FillBuffer(IMediaSample *pms)
{
	while (sess.buffer.isImgQueueEmpty())
	{
		Sleep(50);
	}
	//BYTE *p = sess.cache.front();
	BYTE *pb;
	UINT size;
	UINT32 timestamp;
	pms->GetPointer(&pb);
	ZeroMemory(pb, pms->GetSize());
	memcpy(pb, startcode, 4);
	pms->GetPointer(&pb);
	sess.buffer.getNextImageInBuffer(pb+4, &size, &timestamp);
	//while (sess.cachesize.empty())
	//{
	//	Sleep(100);
	//}
	//memcpy(pb+4, p, sess.cachesize.front());
	//sess.cachesize.pop();
	//sess.cache.pop();
	//free(p);
	//设置时间戳  
	pms->SetTime(NULL, NULL);
	//准备下一帧数据
	pms->SetSyncPoint(TRUE);
	return NOERROR;
}
// Basic COM - used here to reveal our own interfaces
//暴露接口，使外部程序可以QueryInterface，关键！
STDMETHODIMP RTPSourceStream::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
	CheckPointer(ppv, E_POINTER);
	return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
} // NonDelegatingQueryInterface
HRESULT RTPSourceStream::Active(void)
{
	//开始接收RTP包
	sess.staCache = (BYTE*)malloc(sizeof(BYTE) * 614400);
	memset(sess.staCache, 0, 614400);
	if (!sess.IsActive())
	{
		StartReceive();
	}
	return CSourceStream::Active();
};
void RTPSourceStream::StartReceive()
{
	/*CRTPReceiver sess;*/
	std::string ipstr="172.20.111.200";
	int status;

	// Now, we'll create a RTP session, set the destination  
	// and poll for incoming data.  
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise  
	//            RTCP Sender Report info will be calculated wrong  
	// In this case, we'll be just use 9000 samples per second.  
	sessparams.SetOwnTimestampUnit(1.0 / 9000.0);
	portbase = 9000, destport = 8000;
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams, &transparams);
	checkerror(status);
	destip = inet_addr(ipstr.c_str());
	if (destip == INADDR_NONE)
	{
		std::cerr << "Bad IP address specified" << std::endl;
		return ;
	}
	// The inet_addr function returns a value in network byte order, but
	// we need the IP address in host byte order, so we use a call to
	// ntohl
	destip = ntohl(destip);
	std::cout << portbase << " " << destport << " " << destip << std::endl;
	//RTPIPv4Address addr(destip, destport);
	//status = sess.AddDestination(addr);
	checkerror(status);
}



//******************************JRTP******************************
//JRTP发包部分
//实现
//******************************JRTP******************************
void RTPSourceStream::checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

/******************************Public Routine******************************\
* exported entry points for registration and
* unregistration (in this case they only call
* through to default implmentations).
*
*
*
* History:
*
\**************************************************************************/

//DllEntryPoint

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD dwReason,
	LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

//BOOL APIENTRY DllMain( HMODULE hModule,
//                       DWORD  ul_reason_for_call,
//                       LPVOID lpReserved
//					 )
//{
//	switch (ul_reason_for_call)
//	{
//	case DLL_PROCESS_ATTACH:
//	case DLL_THREAD_ATTACH:
//	case DLL_THREAD_DETACH:
//	case DLL_PROCESS_DETACH:
//		break;
//	}
//	return TRUE;
//
//}

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2(FALSE);
}

//extern "C" _declspec(dllexport)BOOL DLLRegisterServer;

