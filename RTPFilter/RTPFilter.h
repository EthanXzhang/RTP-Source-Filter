//#include <windows.h>
//#include "rtpsession.h"
//#include "rtpudpv4transmitter.h"
//#include "rtpipv4address.h"
//#include "rtpsessionparams.h"
//#include "rtperrors.h"
//#include "rtplibraryversion.h"
//#pragma warning(disable:4996)
//#pragma comment (lib, "jrtplib.lib")
//#pragma comment (lib, "jthread.lib")
//#pragma comment (lib, "ws2_32.lib")
//using namespace jrtplib;
#include "stdafx.h"
#include "CRTPReceiver.h"
class RTPSourceStream;
class RTPSource :public CSource
{
public:

	// The only allowed way to create Bouncing balls!
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
private:
	// It is only allowed to to create these objects with CreateInstance
	RTPSource(LPUNKNOWN lpunk, HRESULT *phr);
};
class RTPSourceStream :public CSourceStream
{
private:
	//jrtp实例
	uint16_t portbase, destport;
	uint32_t destip;
	int status, i, num;
	unsigned int timestamp_increse = 0, ts_current = 0;
public:
	RTPSourceStream(HRESULT *phr, RTPSource *pParent, LPCWSTR pPinName);
	~RTPSourceStream();
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

	// check if you can support mtIn
	virtual HRESULT GetMediaType(int iPosition, CMediaType *pmt);
	virtual HRESULT SetMediaType(const CMediaType *pMediaType);
	virtual HRESULT CheckMediaType(const CMediaType *pMediaType); // PURE
	//必须重写的核心函数
	virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
	// Delegating methods
	virtual HRESULT FillBuffer(IMediaSample *pms);
	HRESULT Active(void);
	void RTPSourceStream::StartReceive();
	//jrtp发送部分
	void checkerror(int rtperr);
private:
	CCritSec m_cSharedState;            // Lock on m_rtSampleTime and m_Ball
	CRTPReceiver sess;
	std::string ipstr;
	WSADATA dat;
	BYTE startcode[4] ;
	int m_nCount;
};

