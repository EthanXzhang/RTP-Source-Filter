/**********************************************************
// Name			: imgBufferOprations.cpp
// Description	: Implement the CImageBufferClass class
// Wrote by		: Jianxin, Isoftstone corp.
// Version		: v0.2
**********************************************************/
#include "imgBufferOprations.h"
//-----------------------------------------------
// Constructor
//-----------------------------------------------
CImageBufferClass::CImageBufferClass()
{
	this->Initialization();
}

//-----------------------------------------------
// Initialization
//-----------------------------------------------
void CImageBufferClass::Initialization()
{
	this->isBufferBorderCrossed = false;
	this->img263BufferData_head = 0;		//beginning of image data in buffer
	this->freespace_head = 0;			//end of image data in buffer

#if _DEBUG
	for (int i = 0; i < IMG263_BUFFER_SIZE; i++)
	{
		this->img263Buffer[i] = 0;
	}
#else
	this->img263Buffer = new BYTE[IMG263_BUFFER_SIZE];
#endif

	activeInfoUnitCount = 0;
	hMutex = CreateMutex(NULL, FALSE, NULL);
	Sleep(20);

}

//-----------------------------------------------
// Destructor
//-----------------------------------------------
CImageBufferClass::~CImageBufferClass()
{
	//release infoUnit one by one
	while (!imageInfoUnitQueue.empty())
	{
		image263InBufferInfoUnit * tmpPointer = this->imageInfoUnitQueue.front();
		this->imageInfoUnitQueue.pop();
		delete tmpPointer;
		this->activeInfoUnitCount--;
	}

	//release the image buffer
#if _DEBUG

#else
	if (this->img263Buffer)
	{
		delete [] this->img263Buffer;
	}
#endif

}

//-----------------------------------------------
// Return the size of free space in the buffer
//-----------------------------------------------
UINT CImageBufferClass::getFreeSpaceSizeInBuffer()
{
	/* NOTE : -------------------------------------------------------
	//				How to tell the buffer is empty or full
	// If img263BufferData_head == freespace_head, means either
	// buffer is empty or full. If isBufferBorderCrossed == false, 
	// means empty, else means full.
	// --------------------------------------------------------------*/

	/* NOTE : -------------------------------------------------------
	//				How to calculate free space size in buffer
	// There's a relation as following :
	//		If isBufferBorderCrossed == false, 
	//		"freeBytesInImg263Buffer = freespace_head - img263BufferData_head"
	//		If isBufferBorderCrossed == true, 
	//		"freeBytesInImg263Buffer = img263BufferData_head - freespace_head"
	// --------------------------------------------------------------*/

	//check if the buffer is empty or full
	if (this->img263BufferData_head == this->freespace_head)
	{
		if (!isBufferBorderCrossed)
			return IMG263_BUFFER_SIZE;
		else
			return 0;
	}
	else	//general case
	{
		int size;
		if (!isBufferBorderCrossed)
			size = (IMG263_BUFFER_SIZE - (this->freespace_head - this->img263BufferData_head));
		else
			size = (this->img263BufferData_head - this->freespace_head);
		assert(size > 0);
		return size;
	}
}


//-----------------------------------------------
// Return next image in the buffer
//-----------------------------------------------
BOOL CImageBufferClass::getNextImageInBuffer(BYTE* dataRead, UINT* dataSizeRead, UINT32* timestamp)
{
	BOOL result;

	if (!dataSizeRead || !timestamp)
	{
		cout<< "position 100 false."<<endl;
		return false;
	}


	WaitForSingleObject(hMutex, INFINITE);

	//make sure there're elements in queue
	if (this->imageInfoUnitQueue.empty())
	{
		cout<< "position 101 false."<<endl;
		result = false;
		goto routineExit;
	}

	//get a imageInfoUnit
	image263InBufferInfoUnit *nextImageInfo = this->imageInfoUnitQueue.front();
	this->imageInfoUnitQueue.pop();

	//if the imageInfoUnit is null, there's something wrong
	if (!nextImageInfo)
	{
		assert(false);
		cout<< "position 102 false."<<endl;
		result = false;
		goto routineExit;
	}

	//read from buffer
	result = readImageFromImgBuffer(nextImageInfo,
		dataRead,		//put image data in this buffer
		dataSizeRead	//return the size of the image
		);
	if(result)
		*timestamp = nextImageInfo->timestamp;
	delete nextImageInfo;	//delete the infoUnit
	this->activeInfoUnitCount--;

routineExit:
	ReleaseMutex(hMutex);
	return result;

}

//-----------------------------------------------
// put an image into the buffer
//-----------------------------------------------
BOOL CImageBufferClass::putOneImageIntoBuffer(BYTE* dataTobeWrote, 
											  UINT dataSizeTobeWrote, 
											  UINT32 timestamp)
{
	BOOL result;

	if (dataSizeTobeWrote > IMG263_BUFFER_SIZE)
	{
		assert(false);
		cout<< "position 01 false."<<endl;
		return false;
	}

	//create a infoUnit
	WaitForSingleObject(hMutex, INFINITE);
	image263InBufferInfoUnit *currentImageInfoUnit = new image263InBufferInfoUnit;
	this->activeInfoUnitCount++;
	//assign values
	currentImageInfoUnit->data_head = this->freespace_head;
	currentImageInfoUnit->data_size = dataSizeTobeWrote;
	currentImageInfoUnit->timestamp = timestamp;

	if (this->getFreeSpaceSizeInBuffer() < dataSizeTobeWrote)
	{
		UINT maxDeleteCount = 50;
		while(maxDeleteCount != 0)
		{
			image263InBufferInfoUnit * imageToDeleteInfo;
			if (this->imageInfoUnitQueue.empty())
			{
				assert(false);
				cout<< "position 02 false."<<endl;
				result = false;
				goto routineExit;
			}
			imageToDeleteInfo = this->imageInfoUnitQueue.front();
			this->imageInfoUnitQueue.pop();

			deleteOldestImageInBuffer(imageToDeleteInfo);
			delete imageToDeleteInfo;	//delete the infoUnit
			this->activeInfoUnitCount--;

			if (this->getFreeSpaceSizeInBuffer() >= dataSizeTobeWrote)
			{
				break;
			}
			maxDeleteCount--;
		}
	}

	if (this->getFreeSpaceSizeInBuffer() < dataSizeTobeWrote)	
	{	//still not enough, then return false
		assert(false);
		delete currentImageInfoUnit;
		this->activeInfoUnitCount--;
		cout<< "position 03 false."<<endl;
		result = false;
		goto routineExit;
	}

	//see if need to cross buffer's border
	currentImageInfoUnit->isBlockCrossBorder = doesNeedCrossBorder(dataSizeTobeWrote);

	result = writeImageIntoImgBuffer(	currentImageInfoUnit, 
		dataTobeWrote);
	if (result)
	{
		this->imageInfoUnitQueue.push(currentImageInfoUnit);
	}
	else
	{
		assert(false);
		delete currentImageInfoUnit;
		this->activeInfoUnitCount--;

	}

	//there's a limit on max count of image frames in the buffer
	//so if it is exceeded, then delete the oldest frames
	while (this->imageInfoUnitQueue.size() > MAXCOUNT_FRAMES_IN_BUFFER)
	{
		image263InBufferInfoUnit * imageToDeleteInfo = this->imageInfoUnitQueue.front();
		this->imageInfoUnitQueue.pop();
		deleteOldestImageInBuffer(imageToDeleteInfo);
		delete imageToDeleteInfo;	//delete the infoUnit
		this->activeInfoUnitCount--;
	}

routineExit:
	ReleaseMutex(hMutex);
	return result;
}

//$$$$$$$$$$$$$$$$$$$$$$$$$$$ private methods $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

//--------------------------------------------------------
// Write image data to buffer
//--------------------------------------------------------
// Normally, this routine would always return true,
// if a false returned, this means something unknown happened.
//--------------------------------------------------------
BOOL CImageBufferClass::writeImageIntoImgBuffer(image263InBufferInfoUnit *imageToWrite, 
												BYTE* dataTobeWrote)
{

	if (!imageToWrite || !dataTobeWrote)
	{
		cout<< "position 04 false."<<endl;
		return false;
	}

	assert(this->getFreeSpaceSizeInBuffer() >= imageToWrite->data_size);

	//begin copy
	if (!imageToWrite->isBlockCrossBorder)
	{
		assert(imageToWrite->data_size <= IMG263_BUFFER_SIZE);

		memcpy(	&img263Buffer[imageToWrite->data_head], 
			dataTobeWrote,
			imageToWrite->data_size);

		this->freespace_head += imageToWrite->data_size;

		assert(this->freespace_head <= IMG263_BUFFER_SIZE);

		if (this->freespace_head == IMG263_BUFFER_SIZE)
		{
			this->freespace_head = 0;
			this->isBufferBorderCrossed = true;
		}

	}
	else
	{
		memcpy(	&img263Buffer[imageToWrite->data_head], 
			dataTobeWrote,
			IMG263_BUFFER_SIZE - imageToWrite->data_head);
		memcpy(	&img263Buffer[0], 
			dataTobeWrote + IMG263_BUFFER_SIZE - imageToWrite->data_head,
			imageToWrite->data_size - (IMG263_BUFFER_SIZE - imageToWrite->data_head));

		assert(imageToWrite->data_size >= (IMG263_BUFFER_SIZE - imageToWrite->data_head));

		this->freespace_head = imageToWrite->data_size - (IMG263_BUFFER_SIZE - imageToWrite->data_head);
		this->isBufferBorderCrossed = true;

	}

	return true;
}

//---------------------------------------------------------
// Read image data from buffer. 
//---------------------------------------------------------
// Return true means succeed, 
// false means buffer is empty or something abnormal happened
//---------------------------------------------------------
BOOL CImageBufferClass::readImageFromImgBuffer(image263InBufferInfoUnit *imageToReadInfo,
											   BYTE* dataRead, 
											   UINT* dataSizeRead
											   )
{
	/*NOTE: ------------------------------------------------------------------
	//			How to read out one image from image263 buffer? 
	// If "isBlockCrossBorder == false", then it's straightforward, just 
	// read from the position of data_head sequentially till data_size achieved.
	// If "isBlockCrossBorder == true", it's a little bit tricky,
	// the process broke into two parts, first, read from the position of data_head
	// to the end of buffer, i.e. till "IMG263_BUFFER_SIZE - 1".
	// Next, read from the beginning of the buffer to the position of 
	//------------------------------------------------------------------------*/
	if (!imageToReadInfo || !dataRead)
	{
		cout<< "position 05 false."<<endl;
		return false;
	}

	if (!imageToReadInfo->isBlockCrossBorder)
	{

		memcpy(	dataRead, 
			&img263Buffer[this->img263BufferData_head],
			imageToReadInfo->data_size);
		this->img263BufferData_head += imageToReadInfo->data_size;

		assert(this->img263BufferData_head < IMG263_BUFFER_SIZE);

		//if (this->img263BufferData_head == IMG263_BUFFER_SIZE)
		//{
		//	this->freespace_head = 0;
		//}
	}
	else
	{
		memcpy(	dataRead, 
			&img263Buffer[this->img263BufferData_head],
			IMG263_BUFFER_SIZE - this->img263BufferData_head);

		memcpy(	dataRead + (IMG263_BUFFER_SIZE - this->img263BufferData_head), 
			&img263Buffer[0],
			imageToReadInfo->data_size- (IMG263_BUFFER_SIZE - this->img263BufferData_head));

		assert(imageToReadInfo->data_size >= (IMG263_BUFFER_SIZE - this->img263BufferData_head));

		this->img263BufferData_head = imageToReadInfo->data_size 
			- (IMG263_BUFFER_SIZE - this->img263BufferData_head);

		this->isBufferBorderCrossed = false;
	}
	*dataSizeRead = imageToReadInfo->data_size;

	assert(checkBufferIfNormal());

	return true;

}

//-----------------------------------------------
// Delete oldest image in the buffer
//-----------------------------------------------
// If the buffer is full before writing a new image,
// we should delete the oldest images in the buffer.
// run this routine once, results in the oldest image
// be deleted.
//-----------------------------------------------
void CImageBufferClass::deleteOldestImageInBuffer(image263InBufferInfoUnit * imageToDeleteInfo)
{

	if (!imageToDeleteInfo->isBlockCrossBorder)
	{
		this->img263BufferData_head += imageToDeleteInfo->data_size;

		assert(this->img263BufferData_head < IMG263_BUFFER_SIZE);

	}
	else
	{
		assert(imageToDeleteInfo->data_size >= (IMG263_BUFFER_SIZE - this->img263BufferData_head));
		this->img263BufferData_head = imageToDeleteInfo->data_size 
			- (IMG263_BUFFER_SIZE - this->img263BufferData_head);
		this->isBufferBorderCrossed = false;
	}

}

BOOL CImageBufferClass::checkBufferIfNormal()
{
	/* NOTE : -------------------------------------------------------
	//				How to tell the buffer in a chaos state?
	// Maybe for some unknown reasons, the buffer would be chaotic.
	// So we should check if the buffer in a good condition occasionally.
	// If isBufferBorderCrossed == false, and
	// img263BufferData_head > freespace_head, 
	// or if isBufferBorderCrossed == true, and
	// img263BufferData_head < freespace_head, these two cases mean
	//  something is wrong. In this case, we need to reset the buffer,
	//  i.e. to initialize this class.
	// --------------------------------------------------------------*/

	if (!isBufferBorderCrossed)
	{
		if (this->img263BufferData_head > this->freespace_head)
			return false;
		else
			return true;
	}
	else
	{
		if (this->img263BufferData_head < this->freespace_head)
			return false;
		else
			return true;
	}
}

//-----------------------------------------------
// See if next image to be write into buffer 
// need store it cross the border
//-----------------------------------------------
BOOL CImageBufferClass::doesNeedCrossBorder(UINT dataSizeTobeWrote)
{
	UINT distant01;

	if (!this->isBufferBorderCrossed)
	{
		distant01 = IMG263_BUFFER_SIZE - this->freespace_head;
		return (dataSizeTobeWrote >= distant01)? true : false;
	}
	else
	{
		return false;
	}

}




//-----------------------------------------------
// 
//-----------------------------------------------
BOOL CImageBufferClass::isImgBufferEmpty()
{
	if (this->img263BufferData_head == this->freespace_head)
	{
		if (!this->isBufferBorderCrossed)
			return true;
		else
			return false;
	}
	else
		return false;
}

//-----------------------------------------------
// 
//-----------------------------------------------
UINT CImageBufferClass::getFrameCountInBuffer()
{
	queue <int>::size_type sizeInQueue;
	sizeInQueue = this->imageInfoUnitQueue.size();
	return (UINT)sizeInQueue;
}

//-----------------------------------------------
// 
//-----------------------------------------------
INT CImageBufferClass::getActiveInfoUnitCount()
{
	return this->activeInfoUnitCount;
}

//-----------------------------------------------
// 
//-----------------------------------------------
BOOL CImageBufferClass::isImgQueueEmpty()
{
	return this->imageInfoUnitQueue.empty();
}