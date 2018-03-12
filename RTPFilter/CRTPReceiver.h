#pragma once
#include "stdafx.h"
#include <queue> 
#include "imgBufferOprations.h"
typedef struct {
	//byte 0
	unsigned char TYPE : 5;
	unsigned char NRI : 2;
	unsigned char F : 1;
} NALU_HEADER; /**//* 1 BYTES */
typedef struct {
	//byte 0
	unsigned char TYPE : 5;
	unsigned char NRI : 2;
	unsigned char F : 1;


} FU_INDICATOR; /**//* 1 BYTES */

typedef struct {
	//byte 0
	unsigned char TYPE : 5;
	unsigned char R : 1;
	unsigned char E : 1;
	unsigned char S : 1;
} FU_HEADER; /**//* 1 BYTES */
class CRTPReceiver : public RTPSession
{
protected:
	void OnPollThreadStep();
	void ProcessRTPPacket(const RTPSourceData &srcdat, const RTPPacket &rtppack);

public:
	//std::queue<BYTE*> cache;
	//std::queue<size_t> cachesize;
	CImageBufferClass buffer;
	BYTE *staCache;
private:
	size_t m_current_size=0;
	int count=0;
	NALU_HEADER *nalu_header;
	BYTE fu_ind, fu_head;
	FU_HEADER fHeader;
	FU_INDICATOR fIndicator;
	BYTE head[1];
};
