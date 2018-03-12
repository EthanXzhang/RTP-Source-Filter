#include "stdafx.h"
#include "CRTPReceiver.h"
void CRTPReceiver::OnPollThreadStep()
{
	BeginDataAccess();

	// check incoming packets
	if (GotoFirstSourceWithData())
	{
		do
		{
			RTPPacket *pack;
			RTPSourceData *srcdat;

			srcdat = GetCurrentSourceInfo();

			while ((pack = GetNextPacket()) != NULL)
			{
				//ProcessRTPPacket(*srcdat, *pack);
				// You can inspect the packet and the source's info here
				//std::cout << "Got packet " << std::endl;//<< rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() << std::endl;
				if (pack->GetPayloadType() == 96)
				{
					//std::cout<<"Got H264 packet：êo " << rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() <<std::endl;  
					if (pack->HasMarker())//如果是最后一包则进行组包  
					{
						//fu_ind = *(pack->GetPayloadData());
						memcpy(&fu_ind, pack->GetPayloadData(), 1);
						//std::cout << fu_ind << std::endl;
						//fu_head = *(pack->GetPayloadData() + 1);
						memcpy(&fu_head, pack->GetPayloadData()+1, 1);
						////std::cout << fu_head << std::endl;
						////fIndicator.TYPE = fu_ind;
						//fIndicator.NRI = fu_ind & 0x60;
						//fIndicator.F = fu_ind & 0x80;
						////std::cout << fIndicator.F << " " << fIndicator.NRI << " " << fIndicator.TYPE << std::endl;
						//fHeader.TYPE = fu_head & 0x1F;
						////std::cout << fHeader.TYPE << std::endl;
						nalu_header = (NALU_HEADER*)&head[0];
						nalu_header->TYPE = fu_head;
						nalu_header->NRI = fu_ind >>5;
						nalu_header->F = fu_ind >>2;
						//std::cout << fIndicator.F << " " << fIndicator.NRI << " " << fIndicator.TYPE << std::endl;
						if (fu_head!=0x41)
						{
							std::cout << fu_head << "header" << std::endl;
							std::cout << nalu_header->F << "F" << nalu_header->NRI << "NRI" << nalu_header->TYPE << "TYPE" << std::endl;
						}
						memcpy(staCache + m_current_size, pack->GetPayloadData()+2, pack->GetPayloadLength()-2);
						count++;
						m_current_size +=pack->GetPayloadLength()-2;//得到数据包总的长度  
						BYTE *p = (BYTE *)malloc(sizeof(BYTE)* m_current_size+1);//加入头信息
						memcpy(p, head, 1);
						memcpy(p+1, staCache, m_current_size);
						//FILE *pf;
						//fopen_s(&pf,"G://getdata.txt", "w+");
						//fwrite(p, 1, m_current_size+1, pf);
						//fclose(pf);
						//自写缓冲区
						//if (cache.size() > 20)
						//{
						//	Sleep(100);
						//}
						//cache.push(p);
						//cachesize.push(m_current_size);
						//新缓冲区
						if (buffer.getFreeSpaceSizeInBuffer() <= 0)
						{
							Sleep(50);
						}
						buffer.putOneImageIntoBuffer(p, m_current_size+1,2250);
						memset(staCache, 0, 614400);
						m_current_size = 0;
						count = 0;
					}
					else//放入缓冲区，在此必须确保有序  
					{
						memcpy(staCache + m_current_size, pack->GetPayloadData()+2, pack->GetPayloadLength()-2);
						m_current_size += pack->GetPayloadLength()-2;
						//std::cout << pack->GetPayloadLength()<<" "<<m_current_size << std::endl;
						count++;
					}
				}
				DeletePacket(pack);
			}
		} while (GotoNextSourceWithData());
	}

	EndDataAccess();
}
void CRTPReceiver::ProcessRTPPacket(const RTPSourceData &srcdat, const RTPPacket &rtppack)
{
	//// You can inspect the packet and the source's info here
	//std::cout << "Got packet " << std::endl;//<< rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() << std::endl;
	//if (rtppack.GetPayloadType() == 96)
	//{
	//	//std::cout<<"Got H264 packet：êo " << rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() <<std::endl;  
	//	if (rtppack.HasMarker())//如果是最后一包则进行组包  
	//	{
	//		memcpy(&staCache + m_current_size, rtppack.GetPayloadData(), rtppack.GetPayloadLength());
	//		m_current_size = m_current_size + rtppack.GetPayloadLength();//得到数据包总的长度  
	//		BYTE *p = (BYTE *)malloc(sizeof(BYTE)* m_current_size);
	//		memcpy(p, staCache, m_current_size);
	//		if (cache.size() > 20)
	//		{
	//			Sleep(100);
	//		}
	//		cache.push(p);
	//		cachesize.push(m_current_size);
	//		memset(&staCache, 0, 614400);
	//		m_current_size = 0;
	//	}
	//	else//放入缓冲区，在此必须确保有序  
	//	{
	//		memcpy(&staCache + m_current_size, rtppack.GetPayloadData(), rtppack.GetPayloadLength());
	//		m_current_size += rtppack.GetPayloadLength();

	//	}
	//}
}