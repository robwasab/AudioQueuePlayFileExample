//
//  main.cpp
//  PlayingAudioExample
//
//  Created by Robby Tong on 9/3/17.
//  Copyright © 2017 Robby Tong. All rights reserved.

/* This code is an example from:
 * https://developer.apple.com/library/content/documentation/MusicAudio/Conceptual/AudioQueueProgrammingGuide/AQPlayback/PlayingAudio.html#//apple_ref/doc/uid/TP40005343-CH3-SW1
 */

#include <stdio.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioFile.h>

// Set the number of buffers to use
static const int kNumberBuffers = 3;

struct AQPlayerState
{
	
	/* Description:
	 * From CoreAudioTypes.h, representing the audio data format of the file being played.
	 * Gets used by the audio queue specified in the mQueue field.
	 */
	AudioStreamBasicDescription mDataFormat;
	
	/* Description:
	 * The playback audio queue created by your application.
	 */
	AudioQueueRef mQueue;
	
	/* Description:
	 * An array holding pointers to the audio queue buffers managed
	 * by the audio queue.
	 */
	AudioQueueBufferRef mBuffers[kNumberBuffers];
	
	/* Description:
	 * An audio file object that represents the audio file your program plays.
	 */
	AudioFileID mAudioFile;
	
	/* Description:
	 * The size, in bytes, for each audio queue buffer. This value is calculated
	 * in the DeriveBufferSize function, after the audio queue is created and before
	 * it is started.
	 */
	UInt32 bufferByteSize;
	
	/* Description:
	 * The packet index for the next packet ot play from the audio file.
	 */
	SInt64 mCurrentPacket;
	
	/* Description:
	 * The number of packets to read on each invocation of the audio queue's playback callback.
	 * Like the bufferByteSize field, this value is calculated in these examples in the DeriveBufferSize
	 * function, after the audio queue is created and before it is started.
	 */
	UInt32 mNumPacketsToRead;
	
	/* Description:
	 * For VBR audio data, the array of packet descriptions for the file being played. For CBR data,
	 * the value of this field is NULL.
	 */
	AudioStreamPacketDescription  *mPacketDescs;
	
	/* Description:
	 * A Boolean value indicating whether or not the audio queue is running.
	 */
	bool mIsRunning;
};

// Audio Queue callback
static
void HandleOutputBuffer(void * aqData, AudioQueueRef aq, AudioQueueBufferRef buf)
{
	struct AQPlayerState * data = (AQPlayerState *) aqData;
	
	//printf("Current packet: %lld\n", data->mCurrentPacket);
	
	if (!data->mIsRunning)
	{
		return;
	}
	
	UInt32 ioNumBytesReadFromFile;	// on input, the size of the outBuffer parameter
									// on output, the number of bytes actually read
	
	UInt32 ioNumPackets;	// on input, the number of packets to read
							// on output, the number of packets actually read
	
	ioNumBytesReadFromFile = data->bufferByteSize;
	ioNumPackets = data->mNumPacketsToRead;
	
	//printf("attempting to read %d bytes\n", ioNumBytesReadFromFile);
	//printf("attempting to read %d packets\n", ioNumPackets);
	
	AudioFileReadPacketData(data->mAudioFile,
							false,
							&ioNumBytesReadFromFile,
							data->mPacketDescs,
							data->mCurrentPacket,
							&ioNumPackets,
							buf->mAudioData);
	
	//printf("read %d bytes\n", ioNumBytesReadFromFile);
	//printf("read %d packets\n", ioNumPackets);
	
	printf("read %d / %d bytes\n", ioNumBytesReadFromFile, data->bufferByteSize);
	
	if (ioNumPackets > 0)
	{
		buf->mAudioDataByteSize = ioNumBytesReadFromFile;
		AudioQueueEnqueueBuffer(
								aq,
								buf,
								data->mPacketDescs ? ioNumPackets : 0,
								data->mPacketDescs);
		
		data->mCurrentPacket += ioNumPackets;
	}
	else
	{
		AudioQueueStop(aq, false);
		data->mIsRunning = false;
	}
}

static
void DeriveBufferSize(const AudioStreamBasicDescription * inDesc,
					  UInt32   maxPacketSize,
					  Float64  seconds,
					  UInt32 * outBufferSize,
					  UInt32 * outNumPacketsToRead)
{
	static const int maxBufferSize = 0x50000;  // 320 kBytes
	static const int minBufferSize = 0x4000;   // 16 kBytes
	
	
	// For audio data formats that devine a fixed number of frames per packet
	if (inDesc->mFramesPerPacket != 0)
	{
		// Frames per second / frames per packet * seconds
		Float64 numPacketsForTime = inDesc->mSampleRate / inDesc->mFramesPerPacket * seconds;
		
		*outBufferSize = numPacketsForTime * maxPacketSize;
	}
	// For audio data formats that do not define a fixed number of frames per packet,
	// derives a reasonable audio queue buffer size based on the maximum packet size
	// and the upper bound you've set.
	else
	{
		// maxPacket size is the max buffer size when CBR? CBR indicated by mFramesPerPacket = 0?
		*outBufferSize = maxBufferSize > maxPacketSize ? maxBufferSize : maxPacketSize;
	}
	
	// If the derived buffer size is above the upper bound you've set, adjust the bound,
	// taking into account the estimated maximum packet size;
	// I think this if statement checks the outBufferSize calculated for audio data formats
	// that do not define a fixed number of frames per packet.
	if (*outBufferSize > maxBufferSize && *outBufferSize > maxPacketSize)
	{
		*outBufferSize = maxBufferSize;
	}
	else
	{
		if (*outBufferSize < minBufferSize)
		{
			*outBufferSize = minBufferSize;
		}
	}
	
	*outNumPacketsToRead = *outBufferSize / maxPacketSize;
	
	printf("maxPacketSize: %d\n", maxPacketSize);
	printf("outBufferSize: %d\n", *outBufferSize);
	printf("numPacketsToRead: %d\n", *outNumPacketsToRead);

}

static
void PrintResultCodes(OSStatus code)
{
	switch (code)
	{
		case kAudioFileUnspecifiedError:
			printf("File unspecified\n");
			break;
		case kAudioFileUnsupportedFileTypeError:
			printf("Unsupported file type\n");
			break;
		case kAudioFileUnsupportedDataFormatError:
			printf("Unsupported data format\n");
			break;
		case kAudioFileUnsupportedPropertyError:
			printf("Unsupported property\n");
			break;
		case kAudioFileBadPropertySizeError:
			printf("Bad property size\n");
			break;
		case kAudioFileNotOptimizedError:
			printf("File not optimized\n");
			break;
		case kAudioFileInvalidChunkError:
			printf("Invalid chunk\n");
			break;
		case kAudioFileDoesNotAllow64BitDataSizeError:
			printf("File does not allow 64 bit data size\n");
			break;
		case kAudioFileInvalidPacketOffsetError:
			printf("Invalid packet offset\n");
			break;
		case kAudioFileInvalidFileError:
			printf("Invalid file\n");
			break;
		case kAudioFileOperationNotSupportedError:
			printf("Operation not supported\n");
			break;
		case kAudioFileNotOpenError:
			printf("File not open\n");
			break;
		case kAudioFileEndOfFileError:
			printf("End of file\n");
			break;
		case kAudioFilePositionError:
			printf("Position\n");
			break;
		case kAudio_FileNotFoundError:
			printf("File not found\n");
			break;
		default:
			return;
	}
	assert(false);
}

static
void PrintCFString(CFStringRef cf_string)
{
	const char * string = CFStringGetCStringPtr(cf_string, kCFStringEncodingMacRoman);
	
	printf("%s\n", string);
}

static
void AQPlayerState_InitAudioFile(struct AQPlayerState * aq, const char filePath[])
{
	printf("filename: %s\n", filePath);
	
	CFURLRef audioFileURL = CFURLCreateFromFileSystemRepresentation(
								NULL,
								(const UInt8 *) filePath,
								strlen(filePath),
								false);
	
	PrintCFString(CFURLGetString(audioFileURL));
	
	OSStatus result =
	AudioFileOpenURL(audioFileURL, kAudioFileReadPermission, 0, &aq->mAudioFile);

	printf("mAudioFile: %p\n", aq->mAudioFile);
	
	CFRelease(audioFileURL);
	
	PrintResultCodes(result);
}

static
void PrintBasicDescription(AudioStreamBasicDescription * mDataFormat)
{
	printf("Bits per channel  : %d\n", mDataFormat->mBitsPerChannel);
	printf("Sample rate       : %lf.3\n", mDataFormat->mSampleRate);
	printf("Channels per frame: %d\n", mDataFormat->mChannelsPerFrame);
	printf("Frames per packet : %d\n", mDataFormat->mFramesPerPacket);
	printf("Bytes per packet  : %d\n", mDataFormat->mBytesPerPacket);
}

static
void AQPlayerState_InitBasicDescription(struct AQPlayerState * aq)
{
	UInt32 ioDataSize = sizeof(AudioStreamBasicDescription);
	
	AudioFileGetProperty(aq->mAudioFile, kAudioFilePropertyDataFormat, &ioDataSize, &aq->mDataFormat);
	
	PrintBasicDescription(&aq->mDataFormat);
}

static
void CheckError(OSStatus error, const char *operation)
{
	if (error == noErr) return;
	
	char str[20];
	// see if it appears to be a 4-char-code
	*(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
	if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
		str[0] = str[5] = '\'';
		str[6] = '\0';
	} else
		// no, format it as an integer
		sprintf(str, "%d", (int)error);
	
	fprintf(stderr, "Error: %s (%s)\n", operation, str);
	
	exit(1);
}

static
void AQPlayerState_InitOutputQueue(struct AQPlayerState * aq)
{
	printf("aq->mQueue: %p -> ", aq->mQueue);
	OSStatus code =
	AudioQueueNewOutput(&aq->mDataFormat, HandleOutputBuffer, aq, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &aq->mQueue);
	
	printf("%p\n", aq->mQueue);
	
	CheckError(code, "AudioQueueNewOutput ");
}

static
void AQPlayerState_InitSizes(struct AQPlayerState * aq)
{
	UInt32 outBufferSize;
	UInt32 outNumPacketsToRead;
	UInt32 maxPacketSize;
	UInt32 propertySize = sizeof(maxPacketSize);
	
	AudioFileGetProperty(aq->mAudioFile, kAudioFilePropertyPacketSizeUpperBound, &propertySize, &maxPacketSize);

	DeriveBufferSize(&aq->mDataFormat, maxPacketSize, 0.5, &outBufferSize, &outNumPacketsToRead);
	
	aq->bufferByteSize = outBufferSize;
	aq->mNumPacketsToRead = outNumPacketsToRead;
	
	printf("bufferByteSize: %d\n", outBufferSize);
	printf("mNumPacketsToRead: %d\n", outNumPacketsToRead);
}

static
void AQPlayerState_AllocatePacketDescriptionsArray(struct AQPlayerState * aq)
{
	bool isFormatVBR =
	aq->mDataFormat.mBytesPerPacket == 0 || aq->mDataFormat.mFramesPerPacket == 0;
	
	if (isFormatVBR)
	{
		aq->mPacketDescs = (AudioStreamPacketDescription *) malloc(aq->mNumPacketsToRead * sizeof(AudioStreamPacketDescription));
	}
	else
	{
		aq->mPacketDescs = NULL;
	}
}

static
void AQPlayerState_MagicCookie(struct AQPlayerState * aq)
{
	UInt32 cookieSize;
	
	bool couldNotGetProperty = AudioFileGetPropertyInfo(aq->mAudioFile, kAudioFilePropertyMagicCookieData, &cookieSize, NULL);

	bool couldGetProperty = !couldNotGetProperty;
	
	if (couldGetProperty)
	{
		printf("Setting aq->mQueue's magic cookie property\n");
		
		char * magicCookie = (char *) malloc(cookieSize);
		
		AudioFileGetProperty(aq->mAudioFile, kAudioFilePropertyMagicCookieData, &cookieSize, magicCookie);
		
		AudioQueueSetProperty(aq->mQueue, kAudioQueueProperty_MagicCookie, magicCookie, cookieSize);
		
		free(magicCookie);
	}
	else
	{
		printf("No magic cookie\n");
	}
}

static
void AQPlayerState_AllocateBuffersAndPrime(struct AQPlayerState * aq)
{
	int k;
	aq->mCurrentPacket = 0;
	
	for (k = 0; k < kNumberBuffers; k++)
	{
		AudioQueueAllocateBuffer(aq->mQueue, aq->bufferByteSize, &aq->mBuffers[k]);
	
		HandleOutputBuffer(aq, aq->mQueue, aq->mBuffers[k]);
	}
}

static
void AQPlayerState_SetGain(struct AQPlayerState * aq)
{
	Float32 gain = 1.0;
	
	AudioQueueSetParameter(aq->mQueue, kAudioQueueParam_Volume, gain);
}

static
void AQPlayerState_CleanUp(struct AQPlayerState * aq)
{
	AudioQueueDispose(aq->mQueue, true);
	AudioFileClose(aq->mAudioFile);
	free(aq->mPacketDescs);
}

static
void AQPlayerState_Initialize(struct AQPlayerState * aq, const char audioFileName[])
{
	aq->mIsRunning = true;
	
	// Init audio file with system path
	AQPlayerState_InitAudioFile(aq, audioFileName);
	
	// Init basic description property
	AQPlayerState_InitBasicDescription(aq);
	
	// Init audio queue
	AQPlayerState_InitOutputQueue(aq);
	
	// Init buffer & packet size numbers
	AQPlayerState_InitSizes(aq);
	
	// Allocate audio queue packet descriptor array
	AQPlayerState_AllocatePacketDescriptionsArray(aq);
	
	// Set the magic cookie property of the audio queue
	AQPlayerState_MagicCookie(aq);
	
	// Allocate audio queue buffers and prime them
	AQPlayerState_AllocateBuffersAndPrime(aq);
	
	// Set the gain
	AQPlayerState_SetGain(aq);
}

int main(int argc, const char * argv[])
{
	struct AQPlayerState aq;
	
	memset(&aq, 0, sizeof(AQPlayerState));
	
	// Absolute path to music file
	char audioFileName[] = "/Users/robbytong/Documents/Xcode/PlayingAudioExample/PlayingAudioExample/over_everything.aac";
	
	AQPlayerState_Initialize(&aq, audioFileName);
	
	// Start the audio queue
	printf("Starting audio queue: %p\n", aq.mQueue);
	
	CheckError(AudioQueueStart(aq.mQueue, NULL), "AudioQueueStart");

	do
	{
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
	} while(aq.mIsRunning);
	
	// After the audio queue has stopped, runs the run loop a bit longer to ensure
	// that the audio queue buffer currently playing has time to finish.
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
	
	
	// Clean up
	AQPlayerState_CleanUp(&aq);
	
	return 0;
}


